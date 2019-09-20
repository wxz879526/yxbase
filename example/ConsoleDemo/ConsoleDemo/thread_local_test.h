#pragma once

#include "base/logging.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread_local.h"
#include "base/synchronization/waitable_event.h"

class ThreadLocalTesterBase : public base::DelegateSimpleThreadPool::Delegate {
public:
	typedef base::ThreadLocalPointer<char> TLPType;

	ThreadLocalTesterBase(TLPType *tlp, base::WaitableEvent *done)
		: tlp_(tlp)
		, done_(done)
	{

	}

	~ThreadLocalTesterBase() override
	{

	}

protected:
	TLPType *tlp_;
	base::WaitableEvent *done_;
};

class SetThreadLocal : public ThreadLocalTesterBase {
public:
	SetThreadLocal(TLPType *tlp, base::WaitableEvent *done)
		: ThreadLocalTesterBase(tlp, done)
		, val_(nullptr)
	{

	}

	~SetThreadLocal() override {}

	void set_value(char *val) { val_ = val; }

	virtual void Run() override {
		DCHECK(!done_->IsSignaled());
		tlp_->Set(val_);
		done_->Signal();
	}

private:
	char* val_;
};

class GetThreadLocal : public ThreadLocalTesterBase {
public:
	GetThreadLocal(TLPType *tlp, base::WaitableEvent* done)
		: ThreadLocalTesterBase(tlp, done)
		, ptr_(nullptr)
	{

	}

	~GetThreadLocal() override {}

	void set_ptr(char** ptr) { ptr_ = ptr; }

	virtual void Run() override {
		DCHECK(!done_->IsSignaled());

		*ptr_ = tlp_->Get();
		done_->Signal();
	}

private:
	char** ptr_;
};

void ThreadLocalTest()
{
	base::ThreadLocalBoolean tlb;
	auto bFalse = tlb.Get();

	tlb.Set(false);
	bFalse = tlb.Get();

	tlb.Set(true);
	auto bTrue = tlb.Get();
}