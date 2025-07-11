#include "threadpool.h"

#include <iostream>

const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_IDLE_TIME = 60;    // s

// 线程池构造
ThreadPool::ThreadPool()
	: initThreadSize_(4)
	, taskSize_(0)
	, idleThreadSize_(0)
	, curThreadSize_(0)
	, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
	, threadSizeThreadHold_(THREAD_MAX_THRESHHOLD)
	, poolMode_(PoolMode::MODE_FIXED)
	, isPoolRunning(false)
{}

// 线程池析构
ThreadPool::~ThreadPool()
{
	isPoolRunning = false;

	// 等待线程池里面所有的线程返回  有两种状态：一种阻塞， 一种正在执行任务（因为分离线程）
	std::unique_lock<std::mutex> lock(taskQueMtx_);

	notEmpty_.notify_all();
	exitCond_.wait(lock, [&]()->bool { return threads_.size() == 0; });
}

// 设置线程池的工作模式
void ThreadPool::setMode(PoolMode mode)
{
	if (checkRunningState())
		return;
	poolMode_ = mode;
}

// 设置task任务队列上线阈值
void ThreadPool::setTaskQueMaxThreshHold(int threshhold)
{
	if (checkRunningState())
		return;
	taskQueMaxThreshHold_ = threshhold;
}

// 设置线程池Cached模式下线程阈值
void ThreadPool::setThreadSizeThreshHold(int threshhold)
{
	if (checkRunningState())
		return;
	if (poolMode_ == PoolMode::MODE_CACHED)
	{
		threadSizeThreadHold_ = threshhold;
	}
}

// 给线程池提交任务  用户调用该接口，传入任务对象，生产任务
Result ThreadPool::submitTask(std::shared_ptr<Task> sp) 
{
	// 获取锁
	std::unique_lock<std::mutex> lock(taskQueMtx_);

	// 线程通信 等待任务队列有空余
	// 用户提交任务，最长不能阻塞超过1s，否则判断提交任务失败，返回   wait   wait_for   wait_until
	if (!notFull_.wait_for(lock, std::chrono::seconds(1),
		[&]()->bool { return taskQue_.size() < taskQueMaxThreshHold_; })) 
	{
		// notFull_ 等待1s，条件依然没有满足
		std::cerr << "task queue is full, submit task fail." << std::endl;
		//return task->getResult();  
		return Result(sp, false);
	}

	// 如果有空余，把任务放入队列中
	taskQue_.emplace(sp);
	taskSize_++;

	// 新放了任务，肯定不空了，在notEmpty_上进行通知
	notEmpty_.notify_all();

	// cached模式，任务处理比较紧急 场景：小而快的任务 需要根据任务数量和空闲线程的数量，判断是否需要创建新的线程出来？
	if (poolMode_ == PoolMode::MODE_CACHED
		&& taskSize_ > idleThreadSize_
		&& curThreadSize_ <= threadSizeThreadHold_)
	{
		std::cout << ">>>> create new thread" << std::endl;
		// 创建新线程
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		// 启动线程
		threads_[threadId]->start();  
		// 修改线程个数相关的变量
		curThreadSize_++;
		idleThreadSize_++;
	}

	// 返回任务的Result对象
	return Result(sp);
}

// 开启线程池
void ThreadPool::start(int initThreadSize) 
{
	// 设置线程池启动状态
	isPoolRunning = true;

	// 记录初始线程个数
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize;

	// 创建线程对象
	for (int i = 0; i < initThreadSize_; i++) 
	{
		// 创建thread线程对象的时候，把线程函数给到thread线程对象
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		//threads_.emplace_back(std::move(ptr));
	}

	// 启动所有线程  std::vector<Thread*> threads_;
	for (int i = 0; i < initThreadSize_; i++) 
	{
		threads_[i]->start();  // 需要去执行一个线程函数
		idleThreadSize_++;  // 记录初始空闲线程的数量
	}
}

// 定义线程函数  线程池的所有线程从任务队列中消费任务
void ThreadPool::threadFunc(int threadid)   // 线程函数执行完，相应的线程也就结束了
{
	auto lastTime = std::chrono::high_resolution_clock().now();

	// 等所有任务都执行完成，线程池才可以回收所有线程资源
	for(;;)
	{
		std::shared_ptr<Task> task;
		{
			// 先获取锁
			std::unique_lock<std::mutex> lock(taskQueMtx_);

			std::cout << "tid: " << std::this_thread::get_id()
				<< "尝试获取任务..." << std::endl;

			// cached模式下，有可能已经创建了很多的线程，但是空闲时间超过60s，应该把多余的线程(超过最初设置的线程数量)结束回收掉
			// 当前时间 - 上一次线程执行的时间 》 60s
		
			// 每一秒钟返回一次   怎么区分：超时返回？还是有任务待执行返回
			// 锁 + 双重判断
			while (taskQue_.size() == 0) 
			{
				if (!isPoolRunning)
				{
					threads_.erase(threadid);
					exitCond_.notify_all();
					std::cout << "threadid:" << std::this_thread::get_id() << "exit!";
					return; // 结束线程函数就是结束当前线程
				}
				if (poolMode_ == PoolMode::MODE_CACHED) 
				{
					// 条件变量超时返回
					if (std::cv_status::timeout ==
						notEmpty_.wait_for(lock, std::chrono::seconds(1)))
					{
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
						if (dur.count() >= THREAD_MAX_IDLE_TIME
							&& curThreadSize_ > initThreadSize_)
						{
							// 开始回收当前线程
							// 记录线程数量的相关变量的值修改
							// 把线程对象从线程列表容器中删除  没有办法  threadFunc 对应 哪一个thread对象
							// threadid -> thread对象 -> 删除
							threads_.erase(threadid);  // 不要用 std::this_thread::get_id()
							curThreadSize_--;
							idleThreadSize_--;

							std::cout << "threadid:" << std::this_thread::get_id() << "exit!"
								<< std::endl;
							return; 
						}
					}
				}
				else
				{
					//等待notEmpty_条件
					notEmpty_.wait(lock);
				}

				////	线程池结束，回收资源
				//if (!isPoolRunning) 
				//{
				//	threads_.erase(threadid);  
				//	exitCond_.notify_all();
				//	std::cout << "threadid:" << std::this_thread::get_id() << "exit!"
				//		<< std::endl;
				//	return;  // 结束线程函数就是结束当前线程
				//}
			}

			idleThreadSize_--;

			std::cout << "tid: " << std::this_thread::get_id() 
				<< "获取任务成功..." << std::endl;

			// 从任务队列中取一个任务出来
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;

			// 如果依然有剩余任务，继续通知其他线程执行任务
			if (taskSize_ > 0)
			{
				notEmpty_.notify_all();
			}

			// 执行完一个任务，进行通知，通知可以继续提交生产任务
			notFull_.notify_all();

		}  // 把锁释放掉

		// 当前线程负责执行这个任务
		if (task != nullptr) 
		{
			// task->run();  // 执行任务；把任务的返回值通过setVal方法给到Result
			task->exec();
		}
		idleThreadSize_++;
		lastTime = std::chrono::high_resolution_clock().now(); // 更新线程执行完任务的时间
	}

}

bool ThreadPool::checkRunningState() const 
{
	return isPoolRunning;
}


///////////////////////////////// 线程方法实现
int Thread::generateId_ = 0;

// 线程构造
Thread::Thread(ThreadFunc func) 
	: func_(func) 
	, threadId_(generateId_++)
{}

// 线程析构
Thread::~Thread() {}

// 启动线程
void Thread::start() 
{
	// 创建一个线程来执行一个线程函数
	std::thread t(func_, threadId_);  // C++11来说  线程对象t 和线程函数func_
	t.detach(); // 设置分离线程
	//t.join();
}

int Thread::getId() const
{
	return threadId_;
}

///////////////////////// task 方法实现
Task::Task()
	:result_(nullptr)
{}

void Task::exec()
{
	if (result_ != nullptr)
	{
		result_->setVal(run());  // 这里发生多态调用
	}
}

void Task::setResult(Result* res) 
{
	result_ = res;
}

//////////////////////// Result 方法的实现
Result::Result(std::shared_ptr<Task> task, bool isValid)
	: isValid_(isValid)
	, task_(task)
{
	task_->setResult(this);
}

Any Result::get() // 用户调用
{
	if (!isValid_)
	{
		return "";
	}

	sem_.wait();  // task任务如果没有执行完，这里会阻塞用户的线程
	return std::move(any_);
}

void Result::setVal(Any any)   // 
{
	// 存储task的返回值
	this->any_ = std::move(any);
	sem_.post();  // 已经获取了任务的返回值，增加信号量的资源
}