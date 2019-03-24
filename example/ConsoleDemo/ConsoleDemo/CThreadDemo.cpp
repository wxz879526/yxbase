#include "pch.h"
#include "CThreadDemo.h"
#include <iostream>
#include "base/threading/thread.h"
#include "base/timer/timer.h"


CThreadDemo::CThreadDemo()
{
}


CThreadDemo::~CThreadDemo()
{
}

namespace thread
{
	enum ID
	{
		UI = 0,
		FILE,
		NET,
		DB,
		ID_COUNT
	};
}

const char* thread_names[thread::ID_COUNT] = {
	"UI Thread",
	"File Thread",
	"Net Thread",
	"DB Thread"
};

base::Thread* threads[thread::ID_COUNT] = {
	nullptr
};

base::RepeatingTimer timers[thread::ID_COUNT];

void ui_thread_func(const std::string &text);
void file_thread_func(const std::string &text);
void net_thread_func(const std::string &text);
void db_thread_func(const std::string &text);

void ui_thread_timer(const std::string &text)
{
	std::cout << "UI Thread Timer: " << text << std::endl;
}

void ui_thread_func(const std::string &text)
{
	std::cout << "UI Thread Recv Text: " << text << std::endl;

	threads[thread::FILE]->task_runner()->PostDelayedTask(
		FROM_HERE, base::Bind(&file_thread_func, "I am file thread"),
		base::TimeDelta::FromSeconds(3));

	/*timers[thread::UI].Start(FROM_HERE,
		base::TimeDelta::FromMilliseconds(3000),
		base::Bind(&ui_thread_timer, text));*/
}

void file_thread_timer(const std::string &text)
{
	std::cout << "File Thread Timer: " << text << std::endl;
}

void file_thread_func(const std::string &text)
{
	std::cout << "File Thread Recv Text: " << text << std::endl;

	threads[thread::NET]->task_runner()->PostDelayedTask(
		FROM_HERE, base::Bind(&net_thread_func, "I am net thread"),
		base::TimeDelta::FromSeconds(3));

	/*timers[thread::FILE].Start(FROM_HERE,
		base::TimeDelta::FromMilliseconds(3000),
		base::Bind(&file_thread_timer, text));*/
}

void net_thread_timer(const std::string &text)
{
	std::cout << "Net Thread Timer: " << text << std::endl;
}

void net_thread_func(const std::string &text)
{
	std::cout << "NET Thread Recv Text: " << text << std::endl;

	threads[thread::DB]->task_runner()->PostDelayedTask(
		FROM_HERE, base::Bind(&db_thread_func, "I am db thread"),
		base::TimeDelta::FromSeconds(3));

	/*timers[thread::NET].Start(FROM_HERE,
		base::TimeDelta::FromMilliseconds(3000),
		base::Bind(&net_thread_timer, text));*/
}

void db_thread_timer(const std::string &text)
{
	std::cout << "DB Thread Timer: " << text << std::endl;
}

void db_thread_func(const std::string &text)
{
	std::cout << "DB Thread Recv Text: " << text << std::endl;

	threads[thread::UI]->task_runner()->PostDelayedTask(FROM_HERE,
		base::Bind(&ui_thread_func, "I am ui thread"),
		base::TimeDelta::FromSeconds(5));

	/*timers[thread::DB].Start(FROM_HERE,
		base::TimeDelta::FromMilliseconds(3000),
		base::Bind(&db_thread_timer, text));*/
}

void CThreadDemo::DoWork()
{
	for (auto i = 0; i < thread::ID_COUNT; ++i)
	{
		threads[i] = new base::Thread(thread_names[i]);
		base::Thread::Options options;
		if (i == thread::ID::UI)
		{
			options.message_loop_type = base::MessageLoop::TYPE_UI;
		}
		else
		{
			options.message_loop_type = base::MessageLoop::TYPE_IO;
		}

		threads[i]->StartWithOptions(options);
	}

	bool start = false;
	while (1)
	{
		Sleep(1000);

		if (!start)
		{
			threads[thread::UI]->task_runner()->PostTask(
				FROM_HERE,
				base::Bind(&ui_thread_func, "I am ui thread"));

			start = true;
		}

	}
}
