// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/condition_variable.h"

#include "base/synchronization/lock.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"

#include <windows.h>
#include <stack>

#include "base/compiler_specific.h"
#include "base/macros.h"

namespace {
// We can't use the linker supported delay-load for kernel32 so all this
// cruft here is to manually late-bind the needed functions.
typedef void (WINAPI *InitializeConditionVariableFn)(PCONDITION_VARIABLE);
typedef BOOL (WINAPI *SleepConditionVariableCSFn)(PCONDITION_VARIABLE,
                                                  PCRITICAL_SECTION, DWORD);
typedef void (WINAPI *WakeConditionVariableFn)(PCONDITION_VARIABLE);
typedef void (WINAPI *WakeAllConditionVariableFn)(PCONDITION_VARIABLE);

InitializeConditionVariableFn initialize_condition_variable_fn;
SleepConditionVariableCSFn sleep_condition_variable_fn;
WakeConditionVariableFn wake_condition_variable_fn;
WakeAllConditionVariableFn wake_all_condition_variable_fn;

bool BindVistaCondVarFunctions() {
  HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
  initialize_condition_variable_fn =
      reinterpret_cast<InitializeConditionVariableFn>(
          GetProcAddress(kernel32, "InitializeConditionVariable"));
  if (!initialize_condition_variable_fn)
    return false;
  sleep_condition_variable_fn =
      reinterpret_cast<SleepConditionVariableCSFn>(
          GetProcAddress(kernel32, "SleepConditionVariableCS"));
  if (!sleep_condition_variable_fn)
    return false;
  wake_condition_variable_fn =
      reinterpret_cast<WakeConditionVariableFn>(
          GetProcAddress(kernel32, "WakeConditionVariable"));
  if (!wake_condition_variable_fn)
    return false;
  wake_all_condition_variable_fn =
      reinterpret_cast<WakeAllConditionVariableFn>(
          GetProcAddress(kernel32, "WakeAllConditionVariable"));
  if (!wake_all_condition_variable_fn)
    return false;
  return true;
}

}  // namespace.

namespace base {

// Abstract base class of the pimpl idiom.
class ConditionVarImpl {
 public:
  virtual ~ConditionVarImpl() {};
  virtual void Wait() = 0;
  virtual void TimedWait(const TimeDelta& max_time) = 0;
  virtual void Broadcast() = 0;
  virtual void Signal() = 0;
};

///////////////////////////////////////////////////////////////////////////////
// Windows Vista and Win7 implementation.
///////////////////////////////////////////////////////////////////////////////

class WinVistaCondVar : public ConditionVarImpl {
 public:
  WinVistaCondVar(Lock* user_lock);
  ~WinVistaCondVar() override {}
  // Overridden from ConditionVarImpl.
  void Wait() override;
  void TimedWait(const TimeDelta& max_time) override;
  void Broadcast() override;
  void Signal() override;

 private:
  base::Lock& user_lock_;
  CONDITION_VARIABLE cv_;
};

WinVistaCondVar::WinVistaCondVar(Lock* user_lock)
    : user_lock_(*user_lock) {
  initialize_condition_variable_fn(&cv_);
  DCHECK(user_lock);
}

void WinVistaCondVar::Wait() {
  TimedWait(TimeDelta::FromMilliseconds(INFINITE));
}

void WinVistaCondVar::TimedWait(const TimeDelta& max_time) {
  base::ThreadRestrictions::AssertWaitAllowed();
  DWORD timeout = static_cast<DWORD>(max_time.InMilliseconds());
  CRITICAL_SECTION* cs = user_lock_.lock_.native_handle();

#if DCHECK_IS_ON()
  user_lock_.CheckHeldAndUnmark();
#endif

  if (FALSE == sleep_condition_variable_fn(&cv_, cs, timeout)) {
    DCHECK(GetLastError() != WAIT_TIMEOUT);
  }

#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
  user_lock_.CheckUnheldAndMark();
#endif
}

void WinVistaCondVar::Broadcast() {
  wake_all_condition_variable_fn(&cv_);
}

void WinVistaCondVar::Signal() {
  wake_condition_variable_fn(&cv_);
}

///////////////////////////////////////////////////////////////////////////////
// Windows XP implementation.
///////////////////////////////////////////////////////////////////////////////

class WinXPCondVar : public ConditionVarImpl {
 public:
  WinXPCondVar(Lock* user_lock);
  ~WinXPCondVar() override;
  // Overridden from ConditionVarImpl.
  void Wait() override;
  void TimedWait(const TimeDelta& max_time) override;
  void Broadcast() override;
  void Signal() override;

  // Define Event class that is used to form circularly linked lists.
  // The list container is an element with NULL as its handle_ value.
  // The actual list elements have a non-zero handle_ value.
  // All calls to methods MUST be done under protection of a lock so that links
  // can be validated.  Without the lock, some links might asynchronously
  // change, and the assertions would fail (as would list change operations).
  class Event {
   public:
    // Default constructor with no arguments creates a list container.
    Event();
    ~Event();

    // InitListElement transitions an instance from a container, to an element.
    void InitListElement();

    // Methods for use on lists.
    bool IsEmpty() const;
    void PushBack(Event* other);
    Event* PopFront();
    Event* PopBack();

    // Methods for use on list elements.
    // Accessor method.
    HANDLE handle() const;
    // Pull an element from a list (if it's in one).
    Event* Extract();

    // Method for use on a list element or on a list.
    bool IsSingleton() const;

   private:
    // Provide pre/post conditions to validate correct manipulations.
    bool ValidateAsDistinct(Event* other) const;
    bool ValidateAsItem() const;
    bool ValidateAsList() const;
    bool ValidateLinks() const;

    HANDLE handle_;
    Event* next_;
    Event* prev_;
    DISALLOW_COPY_AND_ASSIGN(Event);
  };

  // Note that RUNNING is an unlikely number to have in RAM by accident.
  // This helps with defensive destructor coding in the face of user error.
  enum RunState { SHUTDOWN = 0, RUNNING = 64213 };

  // Internal implementation methods supporting Wait().
  Event* GetEventForWaiting();
  void RecycleEvent(Event* used_event);

  RunState run_state_;

  // Private critical section for access to member data.
  base::Lock internal_lock_;

  // Lock that is acquired before calling Wait().
  base::Lock& user_lock_;

  // Events that threads are blocked on.
  Event waiting_list_;

  // Free list for old events.
  Event recycling_list_;
  int recycling_list_size_;

  // The number of allocated, but not yet deleted events.
  int allocation_counter_;
};

WinXPCondVar::WinXPCondVar(Lock* user_lock)
    : run_state_(RUNNING),
      user_lock_(*user_lock),
      recycling_list_size_(0),
      allocation_counter_(0) {
  DCHECK(user_lock);
}

WinXPCondVar::~WinXPCondVar() {
  AutoLock auto_lock(internal_lock_);
  run_state_ = SHUTDOWN;  // Prevent any more waiting.

  DCHECK_EQ(recycling_list_size_, allocation_counter_);
  if (recycling_list_size_ != allocation_counter_) {  // Rare shutdown problem.
    // There are threads of execution still in this->TimedWait() and yet the
    // caller has instigated the destruction of this instance :-/.
    // A common reason for such "overly hasty" destruction is that the caller
    // was not willing to wait for all the threads to terminate.  Such hasty
    // actions are a violation of our usage contract, but we'll give the
    // waiting thread(s) one last chance to exit gracefully (prior to our
    // destruction).
    // Note: waiting_list_ *might* be empty, but recycling is still pending.
    AutoUnlock auto_unlock(internal_lock_);
    Broadcast();  // Make sure all waiting threads have been signaled.
    Sleep(10);  // Give threads a chance to grab internal_lock_.
    // All contained threads should be blocked on user_lock_ by now :-).
  }  // Reacquire internal_lock_.

  DCHECK_EQ(recycling_list_size_, allocation_counter_);
}

void WinXPCondVar::Wait() {
  // Default to "wait forever" timing, which means have to get a Signal()
  // or Broadcast() to come out of this wait state.
  TimedWait(TimeDelta::FromMilliseconds(INFINITE));
}

void WinXPCondVar::TimedWait(const TimeDelta& max_time) {
  base::ThreadRestrictions::AssertWaitAllowed();
  Event* waiting_event;
  HANDLE handle;
  {
    AutoLock auto_lock(internal_lock_);
    if (RUNNING != run_state_) return;  // Destruction in progress.
    waiting_event = GetEventForWaiting();
    handle = waiting_event->handle();
    DCHECK(handle);
  }  // Release internal_lock.

  {
    AutoUnlock unlock(user_lock_);  // Release caller's lock
    WaitForSingleObject(handle, static_cast<DWORD>(max_time.InMilliseconds()));
    // Minimize spurious signal creation window by recycling asap.
    AutoLock auto_lock(internal_lock_);
    RecycleEvent(waiting_event);
    // Release internal_lock_
  }  // Reacquire callers lock to depth at entry.
}

// Broadcast() is guaranteed to signal all threads that were waiting (i.e., had
// a cv_event internally allocated for them) before Broadcast() was called.
void WinXPCondVar::Broadcast() {
  std::stack<HANDLE> handles;  // See FAQ-question-10.
  {
    AutoLock auto_lock(internal_lock_);
    if (waiting_list_.IsEmpty())
      return;
    while (!waiting_list_.IsEmpty())
      // This is not a leak from waiting_list_.  See FAQ-question 12.
      handles.push(waiting_list_.PopBack()->handle());
  }  // Release internal_lock_.
  while (!handles.empty()) {
    SetEvent(handles.top());
    handles.pop();
  }
}

// Signal() will select one of the waiting threads, and signal it (signal its
// cv_event).  For better performance we signal the thread that went to sleep
// most recently (LIFO).  If we want fairness, then we wake the thread that has
// been sleeping the longest (FIFO).
void WinXPCondVar::Signal() {
  HANDLE handle;
  {
    AutoLock auto_lock(internal_lock_);
    if (waiting_list_.IsEmpty())
      return;  // No one to signal.
    // Only performance option should be used.
    // This is not a leak from waiting_list.  See FAQ-question 12.
     handle = waiting_list_.PopBack()->handle();  // LIFO.
  }  // Release internal_lock_.
  SetEvent(handle);
}

// GetEventForWaiting() provides a unique cv_event for any caller that needs to
// wait.  This means that (worst case) we may over time create as many cv_event
// objects as there are threads simultaneously using this instance's Wait()
// functionality.
WinXPCondVar::Event* WinXPCondVar::GetEventForWaiting() {
  // We hold internal_lock, courtesy of Wait().
  Event* cv_event;
  if (0 == recycling_list_size_) {
    DCHECK(recycling_list_.IsEmpty());
    cv_event = new Event();
    cv_event->InitListElement();
    allocation_counter_++;
    DCHECK(cv_event->handle());
  } else {
    cv_event = recycling_list_.PopFront();
    recycling_list_size_--;
  }
  waiting_list_.PushBack(cv_event);
  return cv_event;
}

// RecycleEvent() takes a cv_event that was previously used for Wait()ing, and
// recycles it for use in future Wait() calls for this or other threads.
// Note that there is a tiny chance that the cv_event is still signaled when we
// obtain it, and that can cause spurious signals (if/when we re-use the
// cv_event), but such is quite rare (see FAQ-question-5).
void WinXPCondVar::RecycleEvent(Event* used_event) {
  // We hold internal_lock, courtesy of Wait().
  // If the cv_event timed out, then it is necessary to remove it from
  // waiting_list_.  If it was selected by Broadcast() or Signal(), then it is
  // already gone.
  used_event->Extract();  // Possibly redundant
  recycling_list_.PushBack(used_event);
  recycling_list_size_++;
}
//------------------------------------------------------------------------------
// The next section provides the implementation for the private Event class.
//------------------------------------------------------------------------------

// Event provides a doubly-linked-list of events for use exclusively by the
// ConditionVariable class.

// This custom container was crafted because no simple combination of STL
// classes appeared to support the functionality required.  The specific
// unusual requirement for a linked-list-class is support for the Extract()
// method, which can remove an element from a list, potentially for insertion
// into a second list.  Most critically, the Extract() method is idempotent,
// turning the indicated element into an extracted singleton whether it was
// contained in a list or not.  This functionality allows one (or more) of
// threads to do the extraction.  The iterator that identifies this extractable
// element (in this case, a pointer to the list element) can be used after
// arbitrary manipulation of the (possibly) enclosing list container.  In
// general, STL containers do not provide iterators that can be used across
// modifications (insertions/extractions) of the enclosing containers, and
// certainly don't provide iterators that can be used if the identified
// element is *deleted* (removed) from the container.

// It is possible to use multiple redundant containers, such as an STL list,
// and an STL map, to achieve similar container semantics.  This container has
// only O(1) methods, while the corresponding (multiple) STL container approach
// would have more complex O(log(N)) methods (yeah... N isn't that large).
// Multiple containers also makes correctness more difficult to assert, as
// data is redundantly stored and maintained, which is generally evil.

WinXPCondVar::Event::Event() : handle_(0) {
  next_ = prev_ = this;  // Self referencing circular.
}

WinXPCondVar::Event::~Event() {
  if (0 == handle_) {
    // This is the list holder
    while (!IsEmpty()) {
      Event* cv_event = PopFront();
      DCHECK(cv_event->ValidateAsItem());
      delete cv_event;
    }
  }
  DCHECK(IsSingleton());
  if (0 != handle_) {
    int ret_val = CloseHandle(handle_);
    DCHECK(ret_val);
  }
}

// Change a container instance permanently into an element of a list.
void WinXPCondVar::Event::InitListElement() {
  DCHECK(!handle_);
  handle_ = CreateEvent(NULL, false, false, NULL);
  DCHECK(handle_);
}

// Methods for use on lists.
bool WinXPCondVar::Event::IsEmpty() const {
  DCHECK(ValidateAsList());
  return IsSingleton();
}

void WinXPCondVar::Event::PushBack(Event* other) {
  DCHECK(ValidateAsList());
  DCHECK(other->ValidateAsItem());
  DCHECK(other->IsSingleton());
  // Prepare other for insertion.
  other->prev_ = prev_;
  other->next_ = this;
  // Cut into list.
  prev_->next_ = other;
  prev_ = other;
  DCHECK(ValidateAsDistinct(other));
}

WinXPCondVar::Event* WinXPCondVar::Event::PopFront() {
  DCHECK(ValidateAsList());
  DCHECK(!IsSingleton());
  return next_->Extract();
}

WinXPCondVar::Event* WinXPCondVar::Event::PopBack() {
  DCHECK(ValidateAsList());
  DCHECK(!IsSingleton());
  return prev_->Extract();
}

// Methods for use on list elements.
// Accessor method.
HANDLE WinXPCondVar::Event::handle() const {
  DCHECK(ValidateAsItem());
  return handle_;
}

// Pull an element from a list (if it's in one).
WinXPCondVar::Event* WinXPCondVar::Event::Extract() {
  DCHECK(ValidateAsItem());
  if (!IsSingleton()) {
    // Stitch neighbors together.
    next_->prev_ = prev_;
    prev_->next_ = next_;
    // Make extractee into a singleton.
    prev_ = next_ = this;
  }
  DCHECK(IsSingleton());
  return this;
}

// Method for use on a list element or on a list.
bool WinXPCondVar::Event::IsSingleton() const {
  DCHECK(ValidateLinks());
  return next_ == this;
}

// Provide pre/post conditions to validate correct manipulations.
bool WinXPCondVar::Event::ValidateAsDistinct(Event* other) const {
  return ValidateLinks() && other->ValidateLinks() && (this != other);
}

bool WinXPCondVar::Event::ValidateAsItem() const {
  return (0 != handle_) && ValidateLinks();
}

bool WinXPCondVar::Event::ValidateAsList() const {
  return (0 == handle_) && ValidateLinks();
}

bool WinXPCondVar::Event::ValidateLinks() const {
  // Make sure both of our neighbors have links that point back to us.
  // We don't do the O(n) check and traverse the whole loop, and instead only
  // do a local check to (and returning from) our immediate neighbors.
  return (next_->prev_ == this) && (prev_->next_ == this);
}

ConditionVariable::ConditionVariable(Lock* user_lock)
/// youxiao patch
#if 0
    : srwlock_(user_lock->lock_.native_handle())
#if DCHECK_IS_ON()
    , user_lock_(user_lock)
#endif
#endif
///
{
  /// youxiao patch
  #if 0
  DCHECK(user_lock);
  InitializeConditionVariable(&cv_);
  #else
  static bool use_vista_native_cv = BindVistaCondVarFunctions();
  if (use_vista_native_cv)
    impl_= new WinVistaCondVar(user_lock);
  else
    impl_ = new WinXPCondVar(user_lock);
  #endif
  ///
}

/// youxiao patch
#if 0
ConditionVariable::~ConditionVariable() = default;
#else
ConditionVariable::~ConditionVariable() {
  delete impl_;
  impl_ = NULL;
}
#endif
///

void ConditionVariable::Wait() {
  /// youxiao patch
  #if 0
  TimedWait(TimeDelta::FromMilliseconds(INFINITE));
  #else
  impl_->Wait();
  #endif
  ///
}

void ConditionVariable::TimedWait(const TimeDelta& max_time) {
  /// youxiao patch
  #if 0
  base::ThreadRestrictions::AssertWaitAllowed();
  DWORD timeout = static_cast<DWORD>(max_time.InMilliseconds());

#if DCHECK_IS_ON()
  user_lock_->CheckHeldAndUnmark();
#endif

  if (!SleepConditionVariableSRW(&cv_, srwlock_, timeout, 0)) {
    // On failure, we only expect the CV to timeout. Any other error value means
    // that we've unexpectedly woken up.
    // Note that WAIT_TIMEOUT != ERROR_TIMEOUT. WAIT_TIMEOUT is used with the
    // WaitFor* family of functions as a direct return value. ERROR_TIMEOUT is
    // used with GetLastError().
    DCHECK_EQ(static_cast<DWORD>(ERROR_TIMEOUT), GetLastError());
  }

#if DCHECK_IS_ON()
  user_lock_->CheckUnheldAndMark();
#endif
  #else
  impl_->TimedWait(max_time);
  #endif
  ///
}

void ConditionVariable::Broadcast() {
  /// youxiao patch
  #if 0
  WakeAllConditionVariable(&cv_);
  #else
  impl_->Broadcast();
  #endif
  ///
}

void ConditionVariable::Signal() {
  /// youxiao patch
  #if 0
  WakeConditionVariable(&cv_);
  #else
  impl_->Signal();
  #endif
  ///
}

}  // namespace base
