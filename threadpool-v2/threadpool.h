#pragma once
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unordered_map>
#include <thread>
#include <future>
#include <iostream>
const int TASK_MAX_THRESHHOLD = 2; //INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_IDLE_TIME = 60;    // s

enum class PoolMode
{
	MODE_FIXED,  //�̶��������߳�
	MODE_CACHED,  //�߳������ɶ�̬����
};

// �߳�����
class Thread
{
public:

	// �̺߳�����������
	using ThreadFunc = std::function<void(int)>;

	// �̹߳���
	Thread(ThreadFunc func)
		: func_(func)
		, threadId_(generateId_++)
	{}

	// �߳�����
	~Thread() = default;

	// �����߳�
	void start() 
	{
		// ����һ���߳���ִ��һ���̺߳���
		std::thread t(func_, threadId_);  // C++11��˵  �̶߳���t ���̺߳���func_
		t.detach(); // ���÷����߳�
	}

	// ��ȡ�߳�id
	int getId() const
	{
		return threadId_;
	}
private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_;  // �����߳�id
};

int Thread::generateId_ = 0;

// �̳߳�����
class ThreadPool
{
public:
	// �̳߳ع���
	
	ThreadPool()
		: initThreadSize_(4)
		, taskSize_(0)
		, idleThreadSize_(0)
		, curThreadSize_(0)
		, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
		, threadSizeThreadHold_(THREAD_MAX_THRESHHOLD)
		, poolMode_(PoolMode::MODE_FIXED)
		, isPoolRunning(false)
	{}

	// �̳߳�����
	~ThreadPool() 
	{
		isPoolRunning = false;

		// �ȴ��̳߳��������е��̷߳���  ������״̬��һ�������� һ������ִ��������Ϊ�����̣߳�
		std::unique_lock<std::mutex> lock(taskQueMtx_);

		notEmpty_.notify_all();
		exitCond_.wait(lock, [&]()->bool { return threads_.size() == 0; });
	}

	// �����̳߳صĹ���ģʽ
	void setMode(PoolMode mode)
	{
		if (checkRunningState())
			return;
		poolMode_ = mode;
	}

	// ����task�������������ֵ
	void setTaskQueMaxThreshHold(int threshhold)
	{
		if (checkRunningState())
			return;
		taskQueMaxThreshHold_ = threshhold;
	}

	// �����̳߳�Cachedģʽ���߳���ֵ
	void setThreadSizeThreshHold(int threshhold)
	{
		if (checkRunningState())
			return;
		if (poolMode_ == PoolMode::MODE_CACHED)
		{
			threadSizeThreadHold_ = threshhold;
		}
	}

	// ���̳߳��ύ����
	// ʹ�ÿɱ��ģ���̣���submitTask���Խ������������������������Ĳ���
	// pool.submitTask(sum, 10, 20);
	// ����ֵfuture<>
	template<typename Func, typename... Args>
	auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
	{
		// ������񣬷����������
		using RType = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<RType()>>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
		std::future<RType> result = task->get_future();

		// ��ȡ��
		std::unique_lock<std::mutex> lock(taskQueMtx_);
		// �û��ύ�����������������1s�������ж��ύ����ʧ�ܣ�����   wait   wait_for   wait_until
		if (!notFull_.wait_for(lock, std::chrono::seconds(1),
			[&]()->bool { return taskQue_.size() < taskQueMaxThreshHold_; }))
		{
			// notFull_ �ȴ�1s��������Ȼû������
			std::cerr << "task queue is full, submit task fail." << std::endl;
			auto task = std::make_shared<std::packaged_task<RType()>>(
				[]()->RType { return RType(); });
			(*task)();
			return task->get_future();
		}

		// ����п��࣬��������������
		// taskQue_.emplace(sp);
		// using Task = std::function<void()>;
		taskQue_.emplace([task]() {(*task)();});
		taskSize_++;

		// �·������񣬿϶������ˣ���notEmpty_�Ͻ���֪ͨ
		notEmpty_.notify_all();

		// cachedģʽ��������ȽϽ��� ������С��������� ��Ҫ�������������Ϳ����̵߳��������ж��Ƿ���Ҫ�����µ��̳߳�����
		if (poolMode_ == PoolMode::MODE_CACHED
			&& taskSize_ > idleThreadSize_
			&& curThreadSize_ <= threadSizeThreadHold_)
		{
			std::cout << ">>>> create new thread" << std::endl;
			// �������߳�
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			int threadId = ptr->getId();
			threads_.emplace(threadId, std::move(ptr));
			// �����߳�
			threads_[threadId]->start();
			// �޸��̸߳�����صı���
			curThreadSize_++;
			idleThreadSize_++;
		}

		// ���������Result����
		return result;
	}

	// �����̳߳�
	void start(int initThreadSize = std::thread::hardware_concurrency()) {
		// �����̳߳�����״̬
		isPoolRunning = true;

		// ��¼��ʼ�̸߳���
		initThreadSize_ = initThreadSize;
		curThreadSize_ = initThreadSize;

		// �����̶߳���
		for (int i = 0; i < initThreadSize_; i++)
		{
			// ����thread�̶߳����ʱ�򣬰��̺߳�������thread�̶߳���
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			int threadId = ptr->getId();
			threads_.emplace(threadId, std::move(ptr));
			//threads_.emplace_back(std::move(ptr));
		}

		// ���������߳�  std::vector<Thread*> threads_;
		for (int i = 0; i < initThreadSize_; i++)
		{
			threads_[i]->start();  // ��Ҫȥִ��һ���̺߳���
			idleThreadSize_++;  // ��¼��ʼ�����̵߳�����
		}
	}

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
private:
	// �����̺߳���
	void threadFunc(int threadid)
	{
		auto lastTime = std::chrono::high_resolution_clock().now();

		// ����������ִ����ɣ��̳߳زſ��Ի��������߳���Դ
		for (;;)
		{
			Task task;
			{
				// �Ȼ�ȡ��
				std::unique_lock<std::mutex> lock(taskQueMtx_);

				std::cout << "tid: " << std::this_thread::get_id()
					<< "���Ի�ȡ����..." << std::endl;

				// cachedģʽ�£��п����Ѿ������˺ܶ���̣߳����ǿ���ʱ�䳬��60s��Ӧ�ðѶ�����߳�(����������õ��߳�����)�������յ�
				// ��ǰʱ�� - ��һ���߳�ִ�е�ʱ�� �� 60s

				// ÿһ���ӷ���һ��   ��ô���֣���ʱ���أ������������ִ�з���
				// �� + ˫���ж�
				while (taskQue_.size() == 0)
				{
					if (!isPoolRunning)
					{
						threads_.erase(threadid);
						exitCond_.notify_all();
						std::cout << "threadid:" << std::this_thread::get_id() << "exit!";
						return; // �����̺߳������ǽ�����ǰ�߳�
					}
					if (poolMode_ == PoolMode::MODE_CACHED)
					{
						// ����������ʱ����
						if (std::cv_status::timeout ==
							notEmpty_.wait_for(lock, std::chrono::seconds(1)))
						{
							auto now = std::chrono::high_resolution_clock().now();
							auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
							if (dur.count() >= THREAD_MAX_IDLE_TIME
								&& curThreadSize_ > initThreadSize_)
							{
								// ��ʼ���յ�ǰ�߳�
								// ��¼�߳���������ر�����ֵ�޸�
								// ���̶߳�����߳��б�������ɾ��  û�а취  threadFunc ��Ӧ ��һ��thread����
								// threadid -> thread���� -> ɾ��
								threads_.erase(threadid);  // ��Ҫ�� std::this_thread::get_id()
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
						//�ȴ�notEmpty_����
						notEmpty_.wait(lock);
					}
				}

				idleThreadSize_--;

				std::cout << "tid: " << std::this_thread::get_id()
					<< "��ȡ����ɹ�..." << std::endl;

				// �����������ȡһ���������
				task = taskQue_.front();
				taskQue_.pop();
				taskSize_--;

				// �����Ȼ��ʣ�����񣬼���֪ͨ�����߳�ִ������
				if (taskSize_ > 0)
				{
					notEmpty_.notify_all();
				}

				// ִ����һ�����񣬽���֪ͨ��֪ͨ���Լ����ύ��������
				notFull_.notify_all();

			}  // �����ͷŵ�

			// ��ǰ�̸߳���ִ���������
			if (task != nullptr)
			{
				task();  // ִ��function<void()>
			}
			idleThreadSize_++;
			lastTime = std::chrono::high_resolution_clock().now(); // �����߳�ִ���������ʱ��
		}
	}

	// ���pool������״̬
	bool checkRunningState() const
	{
		return isPoolRunning;
	}

private:
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;  // �߳��б�
	int initThreadSize_;  // ��ʼ���߳�����
	std::atomic_int curThreadSize_;   // ��¼��ǰ�̳߳��������̵߳�������
	std::atomic_int idleThreadSize_;   // ��¼�����̵߳�����
	int threadSizeThreadHold_;  // �߳�����������ֵ

	using Task = std::function<void()>;
	std::queue<Task> taskQue_; // �������
	std::atomic_int taskSize_; // ���������
	int taskQueMaxThreshHold_;  // �����������������ֵ

	std::mutex taskQueMtx_; // ��֤������е��̰߳�ȫ
	std::condition_variable notFull_; // ��ʾ������в���
	std::condition_variable notEmpty_; // ��ʾ������в���
	std::condition_variable exitCond_; // �ȴ��߳���Դȫ������

	PoolMode poolMode_;  // �̳߳�ģʽ
	std::atomic_bool isPoolRunning; // ��ʾ��ǰ�̳߳�����״̬
};

#endif
