// ConsoleDemo.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>

#include "base/at_exit.h"
#include "base/strings/string_number_conversions.h"
#include "base/message_loop/message_loop.h"
#include "base/observer_list_threadsafe.h"
#include "base/run_loop.h"
#include "base/location.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/callback.h"
#include "base/logging.h"

#include "CSystemInfoDemo.h"
#include "CThreadDemo.h"

#include "testing/gtest/include/gtest/gtest.h"

#include "base/task_scheduler/task_scheduler.h"
#include "base/task_scheduler/post_task.h"

#include "base/threading/thread.h"
#include "URLRequestContextGetter.h"

int TaskDemo()
{
	int duration_seconds = 0;
	if (!base::StringToInt("10", &duration_seconds))
	{
		std::cout << "Error in StringToInt" << std::endl;
		return -1;
	}

	base::OnceClosure task = base::BindOnce([]() {
		std::cout << "task started" << std::endl;
		std::cout << "task finished" << std::endl;
	});
	base::TimeDelta duration = base::TimeDelta::FromSeconds(duration_seconds);
	base::MessageLoop main_loop;
	base::RunLoop run_loop;
	main_loop.task_runner()->PostDelayedTask(FROM_HERE, std::move(task), duration);
	main_loop.task_runner()->PostDelayedTask(FROM_HERE, run_loop.QuitClosure(), duration);
	run_loop.Run();

	return  0;
}

void ThreadName()
{
	{
		std::string kThreadName("foo");
		base::MessageLoop loop;
		base::PlatformThread::SetName(kThreadName);
		std::cout << loop.GetThreadName() << std::endl;
	}

	{
		std::string kThreadName("bar");
		base::Thread thread(kThreadName);
		thread.StartAndWaitForTesting();
		std::cout << thread.message_loop()->GetThreadName() << std::endl;
	}
}

class Foo {
public:
	virtual void Observe(int x) = 0;
	virtual ~Foo() {};
	virtual int GetValue() const { return 0; }
};

class Adder : public Foo {
public:
	explicit Adder(int scaler) : total(0), scaler_(scaler) {}
	~Adder() override {}

	virtual void Observe(int x) override
	{
		total += x * scaler_;
	}

	virtual int GetValue() const override
	{
		return total;
	}

	int total;

private:
	int scaler_;
};

class Disrupter : public Foo {
public:
	Disrupter(base::ObserverList<Foo>*list, Foo* doomed, bool remove_self)
		: list_(list)
		, doomed_(doomed)
		, remove_self_(remove_self) {}

	Disrupter(base::ObserverList<Foo>*list, Foo* doomed)
		: Disrupter(list, doomed, false) {}

	Disrupter(base::ObserverList<Foo>*list, bool remove_self)
		: Disrupter(list, nullptr, remove_self) {}

	~Disrupter() override {}

	void Observe(int x) override
	{
		if (remove_self_)
			list_->RemoveObserver(this);
		if (doomed_)
			list_->RemoveObserver(doomed_);
	}

	void SetDoomed(Foo *doomed)
	{
		doomed_ = doomed;
	}

private:
	base::ObserverList<Foo>* list_;
	Foo* doomed_;
	bool remove_self_;
};

template<typename ObserverListType>
class AddInObserve : public Foo {
public:
	explicit AddInObserve(ObserverListType* observer_list)
		: observer_list(observer_list), to_add_() {}

	void SetToAdd(Foo* to_add)
	{
		to_add_ = to_add;
	}

	void Observe(int x) override
	{
		if (to_add_) {
			observer_list->AddObserver(to_add_);
			to_add_ = nullptr;
		}
	}

	ObserverListType *observer_list;
	Foo* to_add_;
};

static const int kThreadRunTime = 2000;

class AddRemoveThread : public base::PlatformThread::Delegate, public Foo 
{
public:
	AddRemoveThread(base::ObserverListThreadSafe<Foo>* list, bool bNotify)
		: list_(list)
		, loop_(nullptr)
		, in_list_(false)
		, start_(base::Time::Now())
		, count_observes(0)
		, count_addtask_(0)
		, do_notifies_(bNotify)
		, weak_factory_(this)
	{}

	~AddRemoveThread() override {}

	void ThreadMain() override
	{
		loop_ = new base::MessageLoop();
		loop_->task_runner()->PostTask(FROM_HERE, 
			base::BindOnce(&AddRemoveThread::AddTask, weak_factory_.GetWeakPtr()));
		base::RunLoop().Run();
		delete loop_;
		loop_ = reinterpret_cast<base::MessageLoop*>(0xdeadbeef);
		delete this;
	}

	void AddTask()
	{
		count_addtask_++;

		if ((base::Time::Now() - start_).InMilliseconds() > kThreadRunTime)
		{
			VLOG(1) << "Done";
			return;
		}

		if (!in_list_)
		{
			list_->AddObserver(this);
			in_list_ = true;
		}

		if (do_notifies_)
		{
			list_->Notify(FROM_HERE, &Foo::Observe, 10);
		}

		loop_->task_runner()->PostTask(FROM_HERE,
			base::BindOnce(&AddRemoveThread::AddTask, weak_factory_.GetWeakPtr()));
	}

	void Quit()
	{
		loop_->task_runner()->PostTask(FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
	}

	void Observe(int x) override
	{
		count_observes++;
		DCHECK(in_list_);
		EXPECT_EQ(loop_, base::MessageLoop::current());

		list_->RemoveObserver(this);
		in_list_ = false;
	}

private:
	base::ObserverListThreadSafe<Foo>* list_;
	base::MessageLoop* loop_;
	bool in_list_;

	base::Time start_;

	int count_observes;
	int count_addtask_;
	bool do_notifies_;

	base::WeakPtrFactory<AddRemoveThread> weak_factory_;
};

class CDemoThreadObj : public base::PlatformThread::Delegate
{
public:
	void ThreadMain() override
	{
		DWORD dw = 0;
		while (dw < 30)
		{
			std::cout << dw << std::endl;
			++dw;
			base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(1));
		}
	}
};

bool FetchUrl(const std::string &url, URLRequestContextGetter *getter, std::string *response)
{
	return SyncUrlFetcher(GURL(url), getter, response).Fetch();
}

void ThreadPoolDemo()
{
	// 创建一个名为MyApp的线程池
	base::TaskScheduler::CreateAndStartWithDefaultParams("MyApp");

	for (int i = 0; i < 100; ++i)
	{
		auto task = base::BindOnce([]() {
			std::cout << "taskscheduler run task at " << base::PlatformThread::CurrentId() << std::endl;
		});

		base::PostTask(FROM_HERE, std::move(task));
	}

	if (base::TaskScheduler::GetInstance())
		base::TaskScheduler::GetInstance()->Shutdown();
}

int main(int argc, char* argv[])
{
	base::CommandLine::Init(argc, argv);
	base::AtExitManager exit_manager;

	logging::LoggingSettings settings;
	settings.log_file = L"example.log";
	bool success = logging::InitLogging(settings);

	if (success)
	{
		logging::SetLogItems(true, true, true, false);
	}

	LOG(INFO) << "info.log";
	LOG(ERROR) << "error.log";
	LOG(WARNING) << "warning.log";

	//ThreadPoolDemo();

	//TaskDemo();

	ThreadName();

	/*base::Thread io_thread("io thread");
	base::Thread::Options options(base::MessageLoop::TYPE_IO, 0);
	CHECK(io_thread.StartWithOptions(options));

	URLRequestContextGetter *context_getter = new URLRequestContextGetter(io_thread.task_runner());

	std::string response;
	FetchUrl("http://www.baidu.com", context_getter, &response);
	std::cout << "Url response: " << response << std::endl;

	testing::InitGoogleTest(&argc, argv);
	RUN_ALL_TESTS();

	auto pDelegate = new CDemoThreadObj();
	DCHECK(pDelegate);
	base::PlatformThreadHandle h;
	base::PlatformThread::Create(0, pDelegate, &h);
	base::PlatformThread::Join(h);

	CSystemInfoDemo sysDemo;
	sysDemo.DoWork();

	CThreadDemo threadDemo;
	threadDemo.DoWork();*/

	std::cout << "main exit" << std::endl;
	system("Pause");
	return 0;
}
