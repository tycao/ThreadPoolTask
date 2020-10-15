#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <iostream>
#include <thread>

#include "Task.h"
#include "TaskQueue.h"

class ThreadPool
{
public:
    // 线程池配置参数
    typedef struct tagThreadPoolConfig {
        int nMaxThreadsNum; // 最大线程数量
        int nMinThreadsNum; // 最小线程数量
        double dbTaskAddThreadRate; // 增 最大线程任务比 (任务数量与线程数量，什么比例的时候才加)
        double dbTaskSubThreadRate; // 减 最小线程任务比 (任务数量与线程数量，什么比例的时候才减)
    } ThreadPoolConfig;

public:
    //构造函数
    ThreadPool(void) :_taskQueue(new TaskQueue<Task>()), _atcCurTotalThrNum(0), _atcWorking(true) {}

    //析构函数
    ~ThreadPool(void)
    {
        release();
    }

    //初始化资源
    int init(const ThreadPoolConfig& threadPoolConfig) {
        // 错误的设置
        if (threadPoolConfig.dbTaskAddThreadRate < threadPoolConfig.dbTaskSubThreadRate)
            return 87;


        _threadPoolConfig.nMaxThreadsNum = threadPoolConfig.nMaxThreadsNum;
        _threadPoolConfig.nMinThreadsNum = threadPoolConfig.nMinThreadsNum;
        _threadPoolConfig.dbTaskAddThreadRate = threadPoolConfig.dbTaskAddThreadRate;
        _threadPoolConfig.dbTaskSubThreadRate = threadPoolConfig.dbTaskSubThreadRate;


        int ret = 0;
        // 创建线程池
        if (_threadPoolConfig.nMinThreadsNum > 0)
            ret = addProThreads(_threadPoolConfig.nMinThreadsNum);
        return ret;
    }

    // 添加任务
    int addTask(std::shared_ptr<Task> taskptr, bool priority = false)
    {
        const double& rate = getThreadTaskRate();
        int ret = 0;
        if (priority)
        {
            if (rate > 1000)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            _taskQueue->put_front(taskptr);
        }
        else
        {
            // 检测任务数量
            if (rate > 100) {
                taskptr->onCanceled();
                return 298;
            }
            // 将任务推入队列
            _taskQueue->put_back(taskptr);

        }
        // 检查是否要扩展线程
        if (_atcCurTotalThrNum < _threadPoolConfig.nMaxThreadsNum
            && rate > _threadPoolConfig.dbTaskAddThreadRate)
            ret = addProThreads(1);
        return ret;
    }

    // 删除任务（从就绪队列删除，如果就绪队列没有，则看执行队列有没有，有的话置下取消状态位）
    int deleteTask(size_t nID)
    {
        return _taskQueue->deleteTask(nID);
    }

    // 删除所有任务
    int deleteAllTasks(void)
    {
        return _taskQueue->deleteAllTasks();
    }

    std::shared_ptr<Task> isTaskProcessed(size_t nId)
    {
        return _taskQueue->isTaskProcessed(nId);
    }

    // 释放资源（释放线程池、释放任务队列）
    bool release(void)
    {
        // 1、停止线程池。
        // 2、清楚就绪队列。
        // 3、等待执行队列为0
        releaseThreadPool();
        _taskQueue->release();

        int i = 0;
        while (_atcCurTotalThrNum != 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            // 异常等待
            if (i++ == 10)
                exit(23);
        }

        _atcCurTotalThrNum = 0;
        return true;
    }

    // 获取当前线程任务比
    double getThreadTaskRate(void)
    {
        if (_atcCurTotalThrNum != 0)
            return _taskQueue->size() * 1.0 / _atcCurTotalThrNum;
        return 0;
    }
    // 当前线程是否需要结束
    bool shouldEnd(void)
    {

        bool bFlag = false;
        double dbThreadTaskRate = getThreadTaskRate();

        // 检查线程与任务比率
        if (!_atcWorking || _atcCurTotalThrNum > _threadPoolConfig.nMinThreadsNum
            && dbThreadTaskRate < _threadPoolConfig.dbTaskSubThreadRate)
            bFlag = true;

        return bFlag;
    }

    // 释放线程池
    bool releaseThreadPool(void)
    {
        _threadPoolConfig.nMinThreadsNum = 0;
        _threadPoolConfig.dbTaskSubThreadRate = 0;
        _atcWorking = false;
        return true;
    }


    // 添加指定数量的处理线程
    int addProThreads(int nThreadsNum)
    {
        try {
            for (; nThreadsNum > 0; --nThreadsNum)
                std::thread(&ThreadPool::taskProcessThread, this).detach();
        }
        catch (...) {
            return 155;
        }

        return 0;
    }

    // 任务处理线程函数
    void taskProcessThread(void)
    {
        int nTaskProcRet = 0;
        // 线程增加
        _atcCurTotalThrNum.fetch_add(1);
        std::chrono::milliseconds mills_sleep(500);


        std::shared_ptr<Task> pTask;
        while (_atcWorking)
        {
            // 从任务队列中获取任务
            pTask = _taskQueue->get();  //get会将任务添加到运行任务的map中去
            if (pTask == nullptr)
            {
                if (shouldEnd())
                    break;

                // 进入睡眠池
                _taskQueue->wait(mills_sleep);
                continue;
            }

            // 检测任务取消状态
            if (pTask->isCancelRequired())
                pTask->onCanceled();
            else
                // 处理任务
                pTask->onCompleted(pTask->doWork());


            // 从运行任务队列中移除任务
            _taskQueue->onTaskFinished(pTask->getID());


            // 判断线程是否需要结束
            if (shouldEnd())
                break;

        }

        // 线程个数减一
        _atcCurTotalThrNum.fetch_sub(1);
    }

private:
    std::shared_ptr<TaskQueue<Task> > _taskQueue;	//任务队列
    ThreadPoolConfig _threadPoolConfig; //线程池配置
    std::atomic<bool> _atcWorking;  //线程池是否被要求结束
    std::atomic<int> _atcCurTotalThrNum;    //当前线程个数
};