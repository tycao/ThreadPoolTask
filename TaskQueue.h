#pragma once

#include <deque>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <memory>
#include <thread>

//任务队列
template<typename T>
class TaskQueue
{
public:
	//向队列的末尾插入任务,task是任务类
	void put_back(std::shared_ptr<T>& task)
	{
		std::unique_lock<std::mutex> lock(_mutexQueue);
		_queue.push_back(task);
		_conditPut.notify_one();
	}

	//向队列的头部插入任务
	void put_front(std::shared_ptr<T>& task)
	{
		std::unique_lock<std::mutex> lock(_mutexQueue);
		_queue.push_front(task);
		_conditPut.notify_one();
	}

	//获取队首（并将任务加到运行任务列表中），返回task是任务类
	std::shared_ptr<T> get(void) {
		std::unique_lock<std::mutex> lock(_mutexQueue);
		if (_queue.empty())
			return nullptr;
		//lock_guard取代了mutex的lock()和unlock();
		std::lock_guard<std::mutex> lock_doing_task(_mutexDoingTask);

		std::shared_ptr<T>& task = _queue.front();

		_mapDoingTask.insert(std::make_pair(task->getID(), task));

		_queue.pop_front();
		return task;
	}

	//获取双向链表queue的大小
	size_t size(void)
	{
		std::unique_lock<std::mutex> lock(_mutexQueue);
		return _queue.size();
	}

	//释放队列
	void release(void)
	{
		deleteAllTasks();
		_conditPut.notify_all();
	}

	//删除任务（从就绪队列删除，如果就绪队列没有，则看执行队列有没有，有的话置下取消状态位）
	int deleteTask(size_t nID)
	{
		std::unique_lock<std::mutex> lock(_mutexQueue, std::defer_lock);
		lock.lock();

		auto it = _queue.begin();
		for (; it != _queue.end(); ++it)
		{
			if ((*it)->getID() == nID)
			{
				_queue.erase(it);
				lock.unlock();
				return 0;
			}
		}
		//下面的逻辑可能会造成死锁，这里要及时释放
		lock.unlock();

		// 试图取消正在执行的任务
		{
			std::lock_guard<std::mutex> lock_doing_task(_mutexDoingTask);

			auto it_map = _mapDoingTask.find(nID);
			if (it_map != _mapDoingTask.end())
				it_map->second->setCancelRequired();
		}

		//任务执行完后再返回
		while (_mapDoingTask.count(nID))
			std::this_thread::sleep_for(std::chrono::milliseconds(20));

		return 0;
	}

	//删除所有任务
	int deleteAllTasks(void)
	{
		std::unique_lock<std::mutex> lock(_mutexQueue, std::defer_lock);
		lock.lock();

		if (!_queue.empty())
			_queue.clear();//清空

		{
			std::lock_guard<std::mutex> lock_doing_task(_mutexDoingTask);
			if (!_mapDoingTask.empty())
			{
				auto it_map = _mapDoingTask.begin();
				for (; it_map != _mapDoingTask.end(); ++it_map)
					it_map->second->setCancelRequired();
			}
		}

		lock.unlock();

		//任务执行完后再返回
		while (!_mapDoingTask.empty())
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		return 0;
	}

	//任务完成回调（从运行列表中删除指定任务）
	int onTaskFinished(size_t nID)
	{
		std::lock_guard<std::mutex> lock_doing_task(_mutexDoingTask);
		auto it_map = _mapDoingTask.find(nID);
		if (it_map != _mapDoingTask.end())
			_mapDoingTask.erase(it_map);
		return 0;
	}

	//判断任务是否执行完毕
	std::shared_ptr<T> isTaskProcessed(size_t nId)
	{
		std::lock_guard<std::mutex> lock_queue(_mutexQueue);
		auto it = _queue.begin();
		for (; it != _queue.end(); ++it) {

			if ((*it)->getID() == nId)
				return *it;

		}
		std::lock_guard<std::mutex> lock_doing_task(_mutexDoingTask);

		auto it_map = _mapDoingTask.find(nId);
		if (it_map != _mapDoingTask.end())
			return it_map->second;
		return nullptr;
	}

	//等待有任务到达（带超时：超时自动唤醒）
	bool wait(std::chrono::milliseconds millsec)
	{
		std::unique_lock<std::mutex> lock(_mutexConditPut);
		_conditPut.wait_for(lock, millsec);
		return true;
	}

private:
	//就绪的任务
	std::mutex _mutexQueue;
	std::deque<std::shared_ptr<T>> _queue;

	//条件变量
	std::mutex _mutexConditPut;
	std::condition_variable _conditPut;

	//运行的任务
	std::mutex _mutexDoingTask;
	std::unordered_map<size_t, std::shared_ptr<T> > _mapDoingTask;

};