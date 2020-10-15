#pragma once

#include <functional>
#include "Task.h"

class FuncTask :public Task
{
public:
	FuncTask(std::function<int(void)> f) : _pf(f) {}
	FuncTask(void) : _pf(nullptr) {}

	virtual ~FuncTask() {}

	template <typename F, typename... Args>
	void asynBind(F(*f)(Args...), Args... args)
	{
		_pf = std::bind(f, args...);
	}

	virtual int doWork()
	{
		if (_pf == nullptr)
			return 86;
		return _pf();
	}

private:
	typedef std::function<int(void)> pvFunc;
	pvFunc _pf;
};
