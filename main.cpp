#pragma once

#include <time.h>
#include <iostream>
#include <memory>
#include <string>
#include <queue>
#include <algorithm>
#include "ThreadPool.h"
#include "FuncTask.h"

using namespace std;

int vFunction(void)
{
	std::cout << __FUNCTION__ << std::endl;
	return 0;
}
std::mutex mtx_;
std::queue<std::function<void()>> _queue;
std::condition_variable cond;
std::atomic_int id = 0;

int counter(int a, int b)
{
	std::function<void()> output = [a,b] () { std::cout << a << ":" << b << std::endl; };
	std::lock_guard<std::mutex> lock(mtx_);
	_queue.push(output);
	cond.notify_one();
	return 0;
}


int main()
{
	std::thread([]() {
		while (true)
		{
			std::function<void()> task;
			{
				std::unique_lock<std::mutex> lock(mtx_);
				cond.wait(lock, []() {return !_queue.empty();  });
				task = _queue.front();
				_queue.pop();
			}
			task();
		}
	}).detach();

	ThreadPool::ThreadPoolConfig threadPoolConfig;
	threadPoolConfig.nMaxThreadsNum = 100;
	threadPoolConfig.nMinThreadsNum = 5;
	threadPoolConfig.dbTaskAddThreadRate = 3;
	threadPoolConfig.dbTaskSubThreadRate = 0.5;

	clock_t start = clock();
	{
		std::shared_ptr<ThreadPool> threadPool(new ThreadPool);
		threadPool->init(threadPoolConfig);

		int i = 1;
		while (true)
		{
			/*std::shared_ptr<FuncTask> request(new FuncTask(vFunction));
			threadPool->addTask(request);*/

			std::shared_ptr<FuncTask> request(new FuncTask);
			request->asynBind(counter, i++, 1);
			threadPool->addTask(request);
			if (request->getID() == 110000) {
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}

		threadPool->release();
	}

	clock_t finish = clock();
	std::cout << "duration:" << finish - start << "ms" << std::endl;

	std::cout << "main:thread" << std::endl;
	return 0;
}