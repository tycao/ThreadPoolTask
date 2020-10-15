#pragma once

#include <time.h>
#include <atomic>

//任务的基类
class Task
{
public:
	//构造、析构函数
	Task() :_id(_nRequestID++), _isCancelRequired(false), _createTime(clock()) {}
	~Task() {};

	// 任务类虚接口，继承这个类的必须要实现这个接口
	virtual int doWork(void) = 0;

	// 任务已取消回调
	virtual int onCanceled(void)
	{
		return  1;
	}
	// 任务已完成
	virtual int onCompleted(int)
	{
		return 1;
	}
	// 任务是否超时
	virtual bool isTimeout(const clock_t& now)
	{
		return ((now - _createTime) > 5000);
	}
	// 获取任务ID
	size_t getID(void)
	{
		return _id;
	}
	//获取任务取消状态
	bool isCancelRequired(void)
	{
		return _isCancelRequired;
	}
	//设置任务取消状态
	void setCancelRequired(void)
	{
		_isCancelRequired = true;
	}



protected:
	size_t _id;	//任务的唯一标识
	clock_t _createTime;	//任务创建时间，非Unix时间戳

private:
	static std::atomic<size_t> _nRequestID;
	std::atomic<bool> _isCancelRequired;	//任务取消状态
};

//selectany可以让我们在.h文件中初始化一个全局变量而不是只能放在.cpp中。
//这样的代码来初始化这个全局变量。既是该.h被多次include，链接器也会为我们剔除多重定义的错误。
__declspec(selectany) std::atomic<size_t> Task::_nRequestID = 100000;