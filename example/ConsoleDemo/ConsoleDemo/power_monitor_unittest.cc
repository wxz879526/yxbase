#include "pch.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "base/test/power_monitor_test_base.h"
#include "base/power_monitor/power_monitor.h"
#include "base/message_loop/message_loop.h"

namespace base {
	class PowerMonitorTest : public testing::Test {
	protected:
		PowerMonitorTest() {
			power_monitor_source_ = new PowerMonitorTestSource();
			power_monitor_.reset(new PowerMonitor(std::unique_ptr<PowerMonitorSource>(power_monitor_source_)));
		}

		~PowerMonitorTest() override {};

		PowerMonitorTestSource* source() { return power_monitor_source_; }
		PowerMonitor* monitor() { return power_monitor_.get(); }

	private:
		base::MessageLoop message_loop_;
		PowerMonitorTestSource* power_monitor_source_;
		std::unique_ptr<PowerMonitor> power_monitor_;

		DISALLOW_COPY_AND_ASSIGN(PowerMonitorTest);
	};

	TEST_F(PowerMonitorTest, PowerNotifications) {
		const int kObservers = 5;

		PowerMonitorTestObserver observers[kObservers];
		for (int i = 0; i < kObservers; ++i)
		{
			monitor()->AddObserver(&observers[i]);
		}

		source()->GenerateResumeEvent();
		int x = observers[0].resumes();
		EXPECT_EQ(observers[0].resumes(), 0);

		source()->GenerateSuspendEvent();
	}
}