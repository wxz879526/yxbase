#include "pch.h"
#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/debug/leak_annotations.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_task_runner.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace base {
	class MessageLoopTaskRunnerTest : public testing::Test {
	public:
		MessageLoopTaskRunnerTest()
		: current_loop_(new MessageLoop())
		, task_thread_("task_thread")
		, thread_sync_(WaitableEvent::ResetPolicy::MANUAL,
			WaitableEvent::InitialState::NOT_SIGNALED)
		{

		}

		void DeleteCurrentMessageLoop() { current_loop_.reset(); }

	protected:
		void SetUp() override {
			task_thread_.Start();

			task_thread_.task_runner()->PostTask(FROM_HERE,
				base::BindOnce(&MessageLoopTaskRunnerTest::BlockTaskThreadHelper,
					Unretained(this)));
		}

		void TearDown() override {
			thread_sync_.Signal();
			task_thread_.Stop();
			DeleteCurrentMessageLoop();
		}

		class LoopRecorder : public RefCountedThreadSafe<LoopRecorder> {
		public:
			LoopRecorder(MessageLoop** run_on,
				MessageLoop** deleted_on,
				int* destruct_order)
				: run_on_(run_on)
				, deleted_on_(deleted_on_)
			    , destruct_order_(destruct_order){

			}

			void RecordRun() { *run_on_ = MessageLoop::current(); }

		private:
			friend class RefCountedThreadSafe<LoopRecorder>;
			~LoopRecorder() {
				*deleted_on_ = MessageLoop::current();
				*destruct_order_ = g_order.GetNext();
			}
			MessageLoop** run_on_;
			MessageLoop** deleted_on_;
			int* destruct_order_;
		};
		
		static void RecordLoop(scoped_refptr<LoopRecorder> recorder) {
			recorder->RecordRun();
		}

		static void RecordLoopAndQuit(scoped_refptr<LoopRecorder> recorder) {
			recorder->RecordRun();
			MessageLoop::current()->QuitWhenIdle();
		}

		void UnblockTaskThread() { thread_sync_.Signal(); }
		void BlockTaskThreadHelper() { thread_sync_.Wait(); }

		static StaticAtomicSequenceNumber g_order;
		std::unique_ptr<MessageLoop> current_loop_;
		Thread task_thread_;

	private:
		base::WaitableEvent thread_sync_;
	};

	StaticAtomicSequenceNumber MessageLoopTaskRunnerTest::g_order;

	TEST_F(MessageLoopTaskRunnerTest, PostTaskAndReply_Basic) {

	}
}