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

void ThreadLocalTest2()
{
	base::DelegateSimpleThreadPool tp1("ThreadLocalTest1", 1);
	base::DelegateSimpleThreadPool tp2("ThreadLocalTest2", 1);
	tp1.Start();
	tp2.Start();

	base::ThreadLocalPointer<char> tlp;

	static char* const kBogusPointer = reinterpret_cast<char*>(0x1234);

	char *tls_val;
	base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
		base::WaitableEvent::InitialState::NOT_SIGNALED);

	GetThreadLocal getter(&tlp, &done);
	getter.set_ptr(&tls_val);

	tls_val = kBogusPointer;
	done.Reset();
	tp1.AddWork(&getter);
	done.Wait();
	auto bTrue = (tls_val == nullptr);

	tls_val = kBogusPointer;
	done.Reset();
	tp2.AddWork(&getter);
	done.Wait();
	bTrue = (tls_val == nullptr);

	SetThreadLocal setter(&tlp, &done);
	setter.set_value(kBogusPointer);
	done.Reset();
	tp1.AddWork(&setter);
	done.Wait();

	tls_val = nullptr;
	done.Reset();
	tp1.AddWork(&getter);
	done.Wait();
	bool b1 = (kBogusPointer == tls_val);

	setter.set_value(kBogusPointer + 1);
	done.Reset();
	tp2.AddWork(&setter);
	done.Wait();

	tls_val = nullptr;
	done.Reset();
	tp2.AddWork(&getter);
	done.Wait();
	bool b2 = (kBogusPointer + 1 == tls_val);

	tls_val = nullptr;
	done.Reset();
	tp1.AddWork(&getter);
	done.Wait();
	bool b3 = (kBogusPointer == tls_val);

	tp1.JoinAll();
	tp2.JoinAll();
}