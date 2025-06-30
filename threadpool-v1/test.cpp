#include <iostream>
#include <chrono>
#include <thread>

#include "threadpool.h"

/*
��Щ������ϣ���ܹ���ȡ�߳�ִ������ķ���ֵ
������
1 + ������ + 30000 �ĺ�
thread1 1 + ������ + 10000
thread2 10001 + ������ + 20000��
......
main thread: ��ÿһ���̷߳����������䣬���ȴ��������귵�ؽ�������պϲ����
*/

using ULong = unsigned long long;
class MyTask : public Task
{
public:
	MyTask(int begin, int end)
		: begin_(begin)
		, end_(end)
	{}

	// ����һ�� ��ô���run�����ķ���ֵ�����Ա�ʾ���������
	// C++ 17 Any����
	Any run()   // run �������վ����̳߳ط�����߳���ȥִ��
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
	// ThreadPool���������Ժ��������̳߳���ص���Դȫ������
	{
		ThreadPool pool;
		// �û������̳߳ع���ģʽ
		pool.setMode(PoolMode::MODE_CACHED);
		pool.start(4);

		// ������Result����
		Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));


		ULong sum1 = res1.get().cast_<ULong>();  // get ����һ��Any���ͣ�ת�ɾ�������
		ULong sum2 = res2.get().cast_<ULong>();
		ULong sum3 = res3.get().cast_<ULong>();

		// Master - Slave �߳�ģʽ
		// Master�߳������ֽ�����Ȼ�������Slave�̷߳�������
		// �ȴ�����Slave�߳�ִ�������񣬷��ؽ��
		// Master�̺߳ϲ����������������
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