// ConsoleDemo.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <windows.h>

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

#include "base/memory/ref_counted.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"

#include "base/win/current_module.h"
#include "base/cancelable_callback.h"

#include "thread_local_test.h"

int TaskDemo()
{
	base::MessageLoop loop;

	base::CancelableClosure cancelable(base::Bind([]() {
		std::cout << "CancelableClosure run!!!" << std::endl;
	}));

	base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, cancelable.callback());
	base::RunLoop().RunUntilIdle();

	base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, cancelable.callback());
	cancelable.Cancel();
	base::RunLoop().RunUntilIdle();

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
	//main_loop.task_runner()->PostDelayedTask(FROM_HERE, run_loop.QuitClosure(), duration);
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

void MessageLoopAndTaskRunnerCompare()
{
	base::MessageLoop loop;
	if (&loop == base::MessageLoop::current())
	{
		std::cout << "The same messageloop" << std::endl;
	}
	else
	{
		std::cout << "The different messageloop" << std::endl;
	}

	if (loop.task_runner() == base::ThreadTaskRunnerHandle::Get())
	{
		std::cout << "The same task runner" << std::endl;
	}
	else
	{
		std::cout << "The different task runner" << std::endl;
	}
}

void MessageLoopTypeTest()
{
	base::MessageLoop loop(base::MessageLoop::TYPE_UI);
	std::cout << std::boolalpha << "the messageloop type is TYPE_UI: " << loop.IsType(base::MessageLoop::TYPE_UI) << std::endl;
	std::cout << std::boolalpha << "the messageloop type is TYPE_DEFAULT: " << loop.IsType(base::MessageLoop::TYPE_DEFAULT) << std::endl;
}

class Foo1 : public base::RefCounted<Foo1>
{
public:
	Foo1() : test_count_(0) {}

	void Test1ConstRef(const std::string &a)
	{
		++test_count_;
		result_.append(a);
	}

	int test_count() const { return test_count_; }
	const std::string& result() { return result_; }

private:
	friend class base::RefCounted<Foo1>;

	~Foo1(){}

	int test_count_;
	std::string result_;
};

static void SlowFunc(base::TimeDelta pause, int *quit_counter)
{
	base::PlatformThread::Sleep(pause);
	if (--(*quit_counter) == 0)
		base::MessageLoop::current()->QuitWhenIdle();
}

static void RecordRunTimeFunc(base::Time *run_time, int *quit_counter)
{
	*run_time = base::Time::Now();

	SlowFunc(base::TimeDelta::FromMicroseconds(10), quit_counter);
}

void SumbPumpFunc()
{
	base::MessageLoop::current()->SetNestableTasksAllowed(true);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	base::MessageLoop::current()->QuitWhenIdle();
}

void PostDelayedTask_SharedTimer_SubPump()
{
	base::MessageLoop loop(base::MessageLoop::TYPE_UI);

	int num_tasks = 1;
	base::Time run_time;

	loop.task_runner()->PostTask(FROM_HERE, base::BindOnce(&SumbPumpFunc));

	auto callBack = base::BindOnce([]() {
		std::cout << "Hello World" << std::endl;
	});

	loop.task_runner()->PostTask(FROM_HERE, std::move(callBack));

	loop.task_runner()->PostDelayedTask(FROM_HERE,
		base::BindOnce(&RecordRunTimeFunc, &run_time, &num_tasks),
		base::TimeDelta::FromSeconds(1000));

	loop.task_runner()->PostDelayedTask(FROM_HERE,
		base::BindOnce(&PostQuitMessage, 0),
		base::TimeDelta::FromSeconds(3));

	base::Time start_time = base::Time::Now();

	base::RunLoop().Run();

	std::cout << "num_tasks: " << num_tasks << std::endl;

	base::TimeDelta total_time = base::Time::Now() - start_time;
	std::cout << "Elapse time delta: " << total_time << " milliseconds" << std::endl;

	base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
	base::RunLoop().RunUntilIdle();

	std::cout << std::boolalpha << "run_time is null: " << run_time.is_null() << std::endl;
}

const wchar_t kMessageBoxTitle[] = L"MessageBox Unit Test";

enum TaskType
{
	MESSAGEBOX,
	ENDDIALOG,
	RECURSIVE,
	TIMEDMESSAGELOOP,
	QUITMESSAGELOOP,
	ORDERED,
	PUMPS,
	SLEEP,
	RUNS
};

struct TaskItem 
{
	TaskItem(TaskType t, int c, bool s)
		: type(t)
		, cookie(c)
		, start(s)
	{

	}

	TaskType type;
	int cookie;
	bool start;

	bool operator==(const TaskItem &other)
	{
		return type == other.type && cookie == other.cookie && start == other.start;
	}
};

std::ostream& operator<<(std::ostream& os, TaskType type)
{
	switch (type)
	{
	case MESSAGEBOX: os << "MESSAGEBOX"; break;
	case ENDDIALOG: os << "ENDDIALOG"; break;
	case RECURSIVE: os << "RECURSIVE"; break;
	case TIMEDMESSAGELOOP: os << "TIMEDMESSAGEBOX"; break;
	case QUITMESSAGELOOP: os << "QUITMESSAGEBOX"; break;
	case ORDERED: os << "ORDERED"; break;
	case PUMPS: os << "PUMPS"; break;
	case SLEEP: os << "SLEEP"; break;
	case RUNS: os << "RUNS"; break;
	default:
		NOTREACHED();
		os << "Unknown TaskType";
		break;
	}

	return os;
}

std::ostream& operator<<(std::ostream& os, const TaskItem& item)
{
	if (item.start)
		os << item.type << " " << item.cookie << " starts";
	else
		os << item.type << " " << item.cookie << " ends";

	return os;
}

class TaskList
{
public:
	void RecordStart(TaskType type, int cookie)
	{
		TaskItem item(type, cookie, true);
		DVLOG(1) << item;
		task_list_.push_back(item);
	}

	void RecordEnd(TaskType type, int cookie)
	{
		TaskItem item(type, cookie, false);
		DVLOG(1) << item;
		task_list_.push_back(item);
	}

	size_t Size()
	{
		return task_list_.size();
	}

	TaskItem Get(int n)
	{
		return task_list_[n];
	}

private:
	std::vector<TaskItem> task_list_;
};

void MessageBoxFunc(TaskList* order, int cookie, bool is_reentrant)
{
	order->RecordStart(MESSAGEBOX, cookie);
	if (is_reentrant)
		base::MessageLoop::current()->SetNestableTasksAllowed(true);
	MessageBox(NULL, L"Please wait...", kMessageBoxTitle, MB_OK);
	order->RecordEnd(MESSAGEBOX, cookie);
}

void EndDialogFunc(TaskList* order, int cookie)
{
	order->RecordStart(ENDDIALOG, cookie);
	HWND window = GetActiveWindow();
	if (window != nullptr)
	{
		EndDialog(window, IDCONTINUE);
		order->RecordEnd(ENDDIALOG, cookie);
	}
}

void RecusiveFunc(TaskList* order, int cookie, int depth, bool is_reentrant)
{
	order->RecordStart(RECURSIVE, cookie);
	if (depth > 0)
	{
		if (is_reentrant)
			base::MessageLoop::current()->SetNestableTasksAllowed(true);
		base::ThreadTaskRunnerHandle::Get()->PostTask(
			FROM_HERE,
			base::BindOnce(&RecusiveFunc, order, cookie, depth - 1, is_reentrant));
	}
	order->RecordEnd(RECURSIVE, cookie);
}

void QuitFunc(TaskList* order, int cookie)
{
	order->RecordStart(QUITMESSAGELOOP, cookie);
	base::MessageLoop::current()->QuitWhenIdle();
	order->RecordEnd(QUITMESSAGELOOP, cookie);
}

void RecursiveFuncWin(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
	HANDLE event,
	bool expected_window,
	TaskList *order,
	bool is_reentrant)
{
	//task_runner->PostTask(FROM_HERE,
		//base::BindOnce(&RecusiveFunc, order, 1, 2, is_reentrant));
	task_runner->PostTask(FROM_HERE,
		base::BindOnce(&MessageBoxFunc, order, 2, is_reentrant));
	task_runner->PostTask(FROM_HERE, base::BindOnce([]() {
		std::cout << "Task Pending." << std::endl;
		std::cout << "Task Pending.." << std::endl;
		std::cout << "Task Pending..." << std::endl;
	}));
	//task_runner->PostTask(FROM_HERE,
		//base::BindOnce(&RecusiveFunc, order, 3, 2, is_reentrant));
	//task_runner->PostTask(FROM_HERE,
		//base::BindOnce(&EndDialogFunc, order, 4));
	//task_runner->PostTask(FROM_HERE, base::BindOnce(&QuitFunc, order, 5));

	SetEvent(event);

	/*for (; expected_window; )
	{
		HWND window = FindWindow(L"#32770", kMessageBoxTitle);
		if (window)
		{
			for (;;)
			{
				HWND button = FindWindowEx(window, NULL, L"Button", NULL);
				if (button)
				{
					SendMessage(button, WM_LBUTTONDOWN, 0, 0);
					SendMessage(button, WM_LBUTTONUP, 0, 0);
					break;
				}
			}
			break;
		}
	}*/
}

void RunTest_RecursiveDenial2(base::MessageLoop::Type message_loop_type)
{
	base::MessageLoop loop(message_loop_type);
	base::MessageLoop::current()->SetNestableTasksAllowed(true);

	base::Thread worker("RecursiveDenial2_worker");
	base::Thread::Options options;
	options.message_loop_type = message_loop_type;
	worker.StartWithOptions(options);
	TaskList order;
	base::win::ScopedHandle event(CreateEvent(NULL, FALSE, FALSE, NULL));
	worker.task_runner()->PostTask(FROM_HERE,
		base::BindOnce(&RecursiveFuncWin, base::ThreadTaskRunnerHandle::Get(),
			event.Get(), true, &order, true));
	WaitForSingleObject(event.Get(), INFINITE);
	base::RunLoop().Run();

	std::cout << "order.Size() " << order.Size() << std::endl; //17
	std::cout << "order.Get(0) " << order.Get(0) << std::endl;
	std::cout << "order.Get(0) " << order.Get(1) << std::endl;
}

void EmptyFunction(){}

void PostMultipleTasks()
{
	base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
		base::BindOnce(&EmptyFunction));

	base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
		base::BindOnce(&EmptyFunction));
}

static const int kSignalMsg = WM_USER + 2;

void PostWindowsMessage(HWND message_hwnd)
{
	PostMessage(message_hwnd, kSignalMsg, 0, 2);
}

void EndTest(bool *did_run, HWND hwnd)
{
	*did_run = true;
	PostMessage(hwnd, WM_CLOSE, 0, 0);
}

int kMyMessageFilterCode = 0x5002;

LRESULT CALLBACK TestWndProcThunk(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_CLOSE)
		DestroyWindow(hwnd);

	if (uMsg != kSignalMsg)
		return DefWindowProc(hwnd, uMsg, wParam, lParam);

	bool did_run = false;
	switch (lParam)
	{
	case 1:
		base::ThreadTaskRunnerHandle::Get()->PostTask(
			FROM_HERE,
			base::BindOnce(&PostMultipleTasks));
		base::ThreadTaskRunnerHandle::Get()->PostTask(
			FROM_HERE,
			base::BindOnce(&PostWindowsMessage, hwnd));
		break;
	case 2:
		base::MessageLoop::current()->SetNestableTasksAllowed(true);
		base::ThreadTaskRunnerHandle::Get()->PostTask(
			FROM_HERE,
			base::BindOnce([]() {
			std::cout << "xxxxxxxxxxxxx" << std::endl; }));
		base::ThreadTaskRunnerHandle::Get()->PostTask(
			FROM_HERE,
			base::BindOnce(&EndTest, &did_run, hwnd));

		MSG msg;
		while (GetMessage(&msg, 0, 0, 0))
		{
			if (!CallMsgFilter(&msg, kMyMessageFilterCode))
				DispatchMessage(&msg);

			if (msg.message == WM_CLOSE)
				break;
		}
		base::MessageLoop::current()->QuitWhenIdle();
		break;
	default:
		break;
	}

	return 0;
}

void AlwaysHaveUserMessageWhenNesting()
{
	base::MessageLoop loop(base::MessageLoop::TYPE_UI);
	HINSTANCE instance = CURRENT_MODULE();
	WNDCLASSEX wc = { 0 };
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = TestWndProcThunk;
	wc.hInstance = instance;
	wc.lpszClassName = L"MessageLoopTest_HWND";
	ATOM atom = RegisterClassEx(&wc);

	HWND message_hwnd = CreateWindow(MAKEINTATOM(atom),
		0, 0, 0, 0, 0, 0, HWND_MESSAGE, 0, instance, 0);
	PostMessage(message_hwnd, kSignalMsg, 0, 1);
	base::RunLoop().Run();
	UnregisterClass(MAKEINTATOM(atom), instance);
}

int main(int argc, char* argv[])
{
	
	ThreadLocalTest2();

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

	CThreadDemo threadDemo;
	threadDemo.DoWork();

	//ThreadPoolDemo();

	//TaskDemo();

	//ThreadName();

	//MessageLoopAndTaskRunnerCompare();

	//MessageLoopTypeTest();

	//PostDelayedTask_SharedTimer_SubPump();

	RunTest_RecursiveDenial2(base::MessageLoop::TYPE_UI);

	//RunTest_RecursiveDenial2(MessageLoop::TYPE_UI);
	//RunTest_RecursiveDenial2(MessageLoop::TYPE_IO);

	AlwaysHaveUserMessageWhenNesting();

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
