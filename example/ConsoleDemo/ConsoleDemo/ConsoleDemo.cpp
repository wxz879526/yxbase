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

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
