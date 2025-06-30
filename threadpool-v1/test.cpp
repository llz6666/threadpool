#include <iostream>
#include <chrono>
#include <thread>

#include "threadpool.h"

/*
有些场景，希望能够获取线程执行任务的返回值
举例：
1 + 。。。 + 30000 的和
thread1 1 + 。。。 + 10000
thread2 10001 + 。。。 + 20000；
......
main thread: 给每一个线程分配计算的区间，并等待他们算完返回结果，最终合并结果
*/

using ULong = unsigned long long;
class MyTask : public Task
{
public:
	MyTask(int begin, int end)
		: begin_(begin)
		, end_(end)
	{}

	// 问题一： 怎么设计run函数的返回值，可以表示任意的类型
	// C++ 17 Any类型
	Any run()   // run 方法最终就在线程池分配的线程中去执行
	{
		std::cout << "begin threadFunc tid: " << std::this_thread::get_id() 
			<< std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(3));
		ULong sum = 0;
		for (ULong i = begin_; i <= end_; i++) {
			sum += i;
		}
		std::cout << "end threadFunc tid: " << std::this_thread::get_id() 
			<< std::endl;

		return sum;
	}

private:
	int begin_;
	int end_;
};

int main() 
{
	{
		ThreadPool pool;
		pool.setMode(PoolMode::MODE_CACHED);
		pool.start(2);

		Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		pool.submitTask(std::make_shared<MyTask>(1, 100000000));

		ULong sum1 = res1.get().cast_<ULong>();
		std::cout << sum1 << std::endl;
	}

	std::cout << "main over !" << std::endl;
	getchar();

#if 0
	// ThreadPool对象析构以后，怎样把线程池相关的资源全部回收
	{
		ThreadPool pool;
		// 用户设置线程池工作模式
		pool.setMode(PoolMode::MODE_CACHED);
		pool.start(4);

		// 如何设计Result机制
		Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));


		ULong sum1 = res1.get().cast_<ULong>();  // get 返回一个Any类型，转成具体类型
		ULong sum2 = res2.get().cast_<ULong>();
		ULong sum3 = res3.get().cast_<ULong>();

		// Master - Slave 线程模式
		// Master线程用来分解任务，然后给各个Slave线程分配任务
		// 等待各个Slave线程执行完任务，返回结果
		// Master线程合并各个任务结果，输出
		std::cout << (sum1 + sum2 + sum3) << std::endl;
	}
	
	/*ULong sum = 0;
	for (ULong i = 1; i <= 300000000; i++) {
		sum += i;
	}
	std::cout << sum << std::endl;*/

	/*pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());*/

	
	getchar();
	//std::this_thread::sleep_for(std::chrono::seconds(5));

#endif
}