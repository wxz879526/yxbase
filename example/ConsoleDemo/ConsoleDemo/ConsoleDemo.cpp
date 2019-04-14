// ConsoleDemo.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>

#include "base/at_exit.h"
#include "base/strings/string_number_conversions.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/location.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/callback.h"
#include "base/logging.h"

#include "CSystemInfoDemo.h"
#include "CThreadDemo.h"

#include "testing/gtest/include/gtest/gtest.h"

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

private:
	base::ObserverList<Foo>* list_;
	Foo* doomed_;
	bool remove_self_;
};


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

	testing::InitGoogleTest(&argc, argv);
	RUN_ALL_TESTS();

	CSystemInfoDemo sysDemo;
	sysDemo.DoWork();

	CThreadDemo threadDemo;
	threadDemo.DoWork();

	std::cout << "main exit" << std::endl;
	system("Pause");
	return 0;
}
