#include<iostream>
#include<functional>
#include<thread>
#include<future>
#include <chrono>
#include "threadpool.h"
using namespace std;


/*
1、提交任务更方便
   pool.submitTask(sum, 10, 20);
   pool.submitTask(sum2, 1, 2, 3);
   submitTask: 可变参模板变成

2、C++11 线程库  thread 无法获取返回值   packaged_task(function函数对象) 可以获取  async  直接获取返回值
   使用future代替Result
*/


int sum1(int a, int b)
{
	this_thread::sleep_for(chrono::seconds(2));
	return a + b;
}
int sum2(int a, int b, int c) {
	return a + b + c;
}

int main() 
{
	ThreadPool pool;
	//pool.setMode(PoolMode::MODE_CACHED);
	pool.start(2);

	future<int> res = pool.submitTask(sum1, 1, 2);
	future<int> res1 = pool.submitTask(sum1, 1, 2);
	future<int> res2 = pool.submitTask(sum1, 1, 2);
	future<int> res3 = pool.submitTask(sum1, 1, 2);
	future<int> res4 = pool.submitTask(sum1, 1, 2);
	future<int> res5 = pool.submitTask(sum1, 1, 2);


	cout << res.get() << endl;
	cout << res1.get() << endl;
	cout << res2.get() << endl;
	cout << res3.get() << endl;
	cout << res4.get() << endl;
	cout << res5.get() << endl;
	//packaged_task<int(int, int)> task(sum1);
	//// future <=> Result
	//future<int> res = task.get_future();
	//task(10, 20);
	////thread t(std::move(task), 10, 20);
	////t.detach();

	//cout << res.get() << endl;

}