/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pthread.h

Abstract:

    This header contains definitions for the POSIX thread library.

Author:

    Evan Green 27-Apr-2015

--*/

#ifndef _PTHREAD_H
#define _PTHREAD_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>
#include <sched.h>
#include <stdarg.h>
#include <stddef.h>

//
// --------------------------------------------------------------------- Macros
//

//
// These macros push and pop a cleanup item. They must be used in matching
// pairs. They introduce braces, so the use of setjmp, longjmp, return, break,
// or continue in between matching calls to push and pop will cause undefined
// results. Interestingly enough, this is POSIX compliant behavior.
//

#define pthread_cleanup_push(_CleanupRoutine, _Argument)    \
    do {                                                    \
        __pthread_cleanup_t __CleanupItem;                  \
        __pthread_cleanup_push(&__CleanupItem, (_CleanupRoutine), (_Argument));

//
// The execute parameter is an integer that is zero if the cleanup item should
// just be popped, or non-zero if it should be popped and executed.
//

#define pthread_cleanup_pop(_Execute)                       \
        __pthread_cleanup_pop(&__CleanupItem, (_Execute));  \
    } while (0);

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Currently the pthread library is built into the C library, so its API
// decorator should be whatever the C library is.
//

#define PTHREAD_API LIBC_API

//
// Define mutex types.
//

//
// This type of mutex does not detect deadlock.
//

#define PTHREAD_MUTEX_NORMAL 0

//
// This type of mutex provides error checking. A thread attempting to relock
// this mutex without first unlocking fails. A thread attempting to unlock a
// mutex that another thread has locked will also fail.
//

#define PTHREAD_MUTEX_ERRORCHECK 1

//
// This type of mutex allows a thread to succeed a call to acquire the mutex
// after already acquiring the mutex. A count will be maintained of acquire
// counts for the owning thread, and the mutex will only be released to other
// threads when the acquire count drops to zero.
//

#define PTHREAD_MUTEX_RECURSIVE 2

//
// This type is the default type for an initialized mutex.
//

#define PTHREAD_MUTEX_DEFAULT PTHREAD_MUTEX_NORMAL

//
// This value indicates an object such as a mutex is private to the process.
//

#define PTHREAD_PROCESS_PRIVATE 0

//
// This value indicates an object such as a mutex is shared across all
// processes.
//

#define PTHREAD_PROCESS_SHARED 1

//
// This (default) value indicates that use of the thread ID as a parameter to
// the thread join or detach routines is permitted.
//

#define PTHREAD_CREATE_JOINABLE 0

//
// This value indicates that the use of the thread ID in the join or detach
// routines is prohibited, as the thread may exit and the ID may be reused at
// any time.
//

#define PTHREAD_CREATE_DETACHED 1

//
// This value indicates that created threads contend with all other threads
// in the system for CPU resources.
//

#define PTHREAD_SCOPE_SYSTEM 0

//
// This value indicates that created threads only contend with other threads
// in their parent process for CPU resources. This is not currently supported.
//

#define PTHREAD_SCOPE_PROCESS 1

//
// This value indicates that thread cancellation is enabled: thread can be
// canceled.
//

#define PTHREAD_CANCEL_ENABLE 0

//
// This value indicates that thread cancellation is currently disabled.
//

#define PTHREAD_CANCEL_DISABLE 1

//
// This value indicates that thread cancellation requests will be deferred
// until the next cancellation point.
//

#define PTHREAD_CANCEL_DEFERRED 0

//
// This value indicates that thread cancellation requests will be processed
// immediately.
//

#define PTHREAD_CANCEL_ASYNCHRONOUS 1

//
// This thread return value indicates that the thread was canceled, rather than
// returning naturally.
//

#define PTHREAD_CANCELED ((void *)-1)

//
// Define the constant initializer for a mutex, which can be assigned as the
// initial value for a global variable mutex.
//

#define PTHREAD_MUTEX_INITIALIZER {{0}}

//
// Define the constant initializer for a recursive mutex.
//

#define PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP \
    {{0, 0, 0, 0x40}}

//
// Define the constant initializer for an error-checking mutex.
//

#define PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP \
    {{0, 0, 0, 0x80}}

//
// Define the constant initializer for a condition variable.
//

#define PTHREAD_COND_INITIALIZER {{0}}

//
// Define the constant initializer for a read/write lock.
//

#define PTHREAD_RWLOCK_INITIALIZER {{0}}

//
// Define the constant initializer for a once object.
//

#define PTHREAD_ONCE_INIT 0

//
// Define the value returned to one arbitrary thread after a pthread barrier
// wait is satisfied. This value must be distinct from all error numbers and
// cannot be 0.
//

#define PTHREAD_BARRIER_SERIAL_THREAD -1

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
void
(*__pthread_cleanup_func_t) (
    void *Parameter
    );

/*++

Routine Description:

    This routine prototype represents a function that is called when a thread
    is exiting.

Arguments:

    Parameter - Supplies a pointer's worth of data.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure stores the context for a thread cleanup routine.

Members:

    __cleanup_prev - Stores a pointer to the previous item on the stack.

    __cleanup_routine - Stores a pointer to the routine to call.

    __cleanup_arg - Stores an argument to pass to the cleanup routine.

--*/

typedef struct __pthread_cleanup_t {
    struct __pthread_cleanup_t *__cleanup_prev;
    __pthread_cleanup_func_t __cleanup_routine;
    void *__cleanup_arg;
} __pthread_cleanup_t;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

PTHREAD_API
int
pthread_mutex_init (
    pthread_mutex_t *Mutex,
    const pthread_mutexattr_t *Attribute
    );

/*++

Routine Description:

    This routine initializes a mutex.

Arguments:

    Mutex - Supplies a pointer to the mutex to initialize.

    Attribute - Supplies an optional pointer to the initialized attributes to
        set in the mutex.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_mutex_destroy (
    pthread_mutex_t *Mutex
    );

/*++

Routine Description:

    This routine destroys a mutex.

Arguments:

    Mutex - Supplies a pointer to the mutex to destroy.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_mutex_lock (
    pthread_mutex_t *Mutex
    );

/*++

Routine Description:

    This routine acquires a mutex.

Arguments:

    Mutex - Supplies a pointer to the mutex to acquire.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_mutex_unlock (
    pthread_mutex_t *Mutex
    );

/*++

Routine Description:

    This routine releases a mutex.

Arguments:

    Mutex - Supplies a pointer to the mutex to release.

Return Value:

    0 on success.

    EPERM if this thread is not the thread that originally acquire the mutex.

--*/

PTHREAD_API
int
pthread_mutex_trylock (
    pthread_mutex_t *Mutex
    );

/*++

Routine Description:

    This routine attempts to acquire the given mutex once.

Arguments:

    Mutex - Supplies a pointer to the mutex to attempt to acquire.

Return Value:

    0 on success.

    EBUSY if the mutex is already held by another thread and this is an error
    checking mutex.

--*/

PTHREAD_API
int
pthread_mutex_timedlock (
    pthread_mutex_t *Mutex,
    const struct timespec *AbsoluteTimeout
    );

/*++

Routine Description:

    This routine attempts to acquire a mutex, giving up after a specified
    deadline.

Arguments:

    Mutex - Supplies a pointer to the mutex to acquire.

    AbsoluteTimeout - Supplies the absolute timeout after which the attempt
        shall fail and time out.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_mutexattr_init (
    pthread_mutexattr_t *Attribute
    );

/*++

Routine Description:

    This routine initializes a mutex attribute object.

Arguments:

    Attribute - Supplies a pointer to the attribute to initialize.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_mutexattr_destroy (
    pthread_mutexattr_t *Attribute
    );

/*++

Routine Description:

    This routine destroys a mutex attribute object.

Arguments:

    Attribute - Supplies a pointer to the attribute to destroy.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_mutexattr_gettype (
    const pthread_mutexattr_t *Attribute,
    int *Type
    );

/*++

Routine Description:

    This routine returns the mutex type given an attribute that was previously
    set.

Arguments:

    Attribute - Supplies a pointer to the attribute to get the type from.

    Type - Supplies a pointer where the mutex type will be returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_mutexattr_settype (
    pthread_mutexattr_t *Attribute,
    int Type
    );

/*++

Routine Description:

    This routine sets a mutex type in the given mutex attributes object.

Arguments:

    Attribute - Supplies a pointer to the attribute to set the type in.

    Type - Supplies the mutex type to set. See PTHREAD_MUTEX_* definitions.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_mutexattr_getpshared (
    const pthread_mutexattr_t *Attribute,
    int *Shared
    );

/*++

Routine Description:

    This routine returns the mutex sharing type given an attribute that was
    previously set.

Arguments:

    Attribute - Supplies a pointer to the attribute to get the sharing
    information from.

    Shared - Supplies a pointer where the sharing type will be returned on
        success. See PTHREAD_PROCESS_* definitions.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_mutexattr_setpshared (
    pthread_mutexattr_t *Attribute,
    int Shared
    );

/*++

Routine Description:

    This routine sets a mutex sharing type in the given mutex attributes object.

Arguments:

    Attribute - Supplies a pointer to the attribute to set the type in.

    Shared - Supplies the mutex type to set. See PTHREAD_PROCESS_* definitions.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_cond_init (
    pthread_cond_t *Condition,
    pthread_condattr_t *Attribute
    );

/*++

Routine Description:

    This routine initializes a condition variable structure.

Arguments:

    Condition - Supplies a pointer to the condition variable structure.

    Attribute - Supplies an optional pointer to the condition variable
        attributes.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_cond_destroy (
    pthread_cond_t *Condition
    );

/*++

Routine Description:

    This routine destroys a condition variable structure.

Arguments:

    Condition - Supplies a pointer to the condition variable structure.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_cond_broadcast (
    pthread_cond_t *Condition
    );

/*++

Routine Description:

    This routine wakes up all threads waiting on the given condition variable.
    This is useful when there are multiple different predicates behind the
    same condition variable.

Arguments:

    Condition - Supplies a pointer to the condition variable to wake.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_cond_signal (
    pthread_cond_t *Condition
    );

/*++

Routine Description:

    This routine wakes up at least one thread waiting on the given condition
    variable. This is preferred over the broadcast function if all waiting
    threads are checking the same mutex, as it prevents the thundering herd
    associated with broadcast (all woken threads race to acquire the same
    mutex). Multiple threads may exit a condition wait, so it is critical to
    check the predicate on return.

Arguments:

    Condition - Supplies a pointer to the condition variable to wake.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_cond_wait (
    pthread_cond_t *Condition,
    pthread_mutex_t *Mutex
    );

/*++

Routine Description:

    This routine unlocks the given mutex, blocks until the given condition
    variable is signaled, and then reacquires the mutex.

Arguments:

    Condition - Supplies a pointer to the condition variable to wait on.

    Mutex - Supplies a pointer to the mutex to operate on.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_cond_timedwait (
    pthread_cond_t *Condition,
    pthread_mutex_t *Mutex,
    const struct timespec *AbsoluteTimeout
    );

/*++

Routine Description:

    This routine unlocks the given mutex, blocks until the given condition
    variable is signaled, and then reacquires the mutex. This wait can time
    out after the specified deadline.

Arguments:

    Condition - Supplies a pointer to the condition variable to wait on.

    Mutex - Supplies a pointer to the mutex to operate on.

    AbsoluteTimeout - Supplies a pointer to an absolute time after which the
        wait should return anyway.

Return Value:

    0 on success.

    ETIMEDOUT if the operation timed out. The predicate may have become true
    naturally anyway, so the caller should always check their predicates.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_condattr_init (
    pthread_condattr_t *Attribute
    );

/*++

Routine Description:

    This routine initializes a condition variable attribute structure.

Arguments:

    Attribute - Supplies a pointer to the condition variable attribute
        structure.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_condattr_destroy (
    pthread_condattr_t *Attribute
    );

/*++

Routine Description:

    This routine destroys a condition variable attribute structure.

Arguments:

    Attribute - Supplies a pointer to the condition variable attribute
        structure.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_condattr_getpshared (
    const pthread_condattr_t *Attribute,
    int *Shared
    );

/*++

Routine Description:

    This routine determines the shared attribute in a condition variable.

Arguments:

    Attribute - Supplies a pointer to the condition variable attribute
        structure.

    Shared - Supplies a pointer where the shared attribute will be returned,
        indicating whether the condition variable is visible across processes.
        See PTHREAD_PROCESS_* definitions.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_condattr_setpshared (
    pthread_condattr_t *Attribute,
    int Shared
    );

/*++

Routine Description:

    This routine sets the shared attribute in a condition variable.

Arguments:

    Attribute - Supplies a pointer to the condition variable attribute
        structure.

    Shared - Supplies the value indicating whether this condition variable
        should be visible across processes. See PTHREAD_PROCESS_* definitions.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_condattr_getclock (
    const pthread_condattr_t *Attribute,
    int *Clock
    );

/*++

Routine Description:

    This routine determines which clock the condition variable uses for timed
    waits.

Arguments:

    Attribute - Supplies a pointer to the condition variable attribute
        structure.

    Clock - Supplies a pointer where the clock source for timed waits on the
        condition variable with this attribute will be returned. See CLOCK_*
        definitions.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_condattr_setclock (
    pthread_condattr_t *Attribute,
    int Clock
    );

/*++

Routine Description:

    This routine sets the clock used for condition variable timed waits.

Arguments:

    Attribute - Supplies a pointer to the condition variable attribute
        structure.

    Clock - Supplies the clock to use when performing timed waits.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_rwlock_init (
    pthread_rwlock_t *Lock,
    pthread_rwlockattr_t *Attribute
    );

/*++

Routine Description:

    This routine initializes a read/write lock.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

    Attribute - Supplies an optional pointer to an initialized attribute
        structure governing the internal behavior of the lock.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_rwlock_destroy (
    pthread_rwlock_t *Lock
    );

/*++

Routine Description:

    This routine destroys a read/write lock.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_rwlock_rdlock (
    pthread_rwlock_t *Lock
    );

/*++

Routine Description:

    This routine acquires the read/write lock for read access. Multiple readers
    can acquire the lock simultaneously, but any writes that try to acquire
    the lock while it's held for read will block. Readers that try to
    acquire the lock while it's held for write will also block.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_rwlock_timedrdlock (
    pthread_rwlock_t *Lock,
    const struct timespec *AbsoluteTimeout
    );

/*++

Routine Description:

    This routine acquires the read/write lock for read access just like the
    read lock function, except that this function will return after the
    specified deadline if the lock could not be acquired.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

    AbsoluteTimeout - Supplies a pointer to the absolute deadline after which
        this function should give up and return failure.

Return Value:

    0 on success.

    ETIMEDOUT if the operation timed out. This thread will not own the lock.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_rwlock_tryrdlock (
    pthread_rwlock_t *Lock
    );

/*++

Routine Description:

    This routine performs a single attempt at acquiring the lock for read
    access.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_rwlock_wrlock (
    pthread_rwlock_t *Lock
    );

/*++

Routine Description:

    This routine acquires the read/write lock for write access. The lock can
    only be acquired for write access if there are no readers and no other
    writers.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_rwlock_timedwrlock (
    pthread_rwlock_t *Lock,
    const struct timespec *AbsoluteTimeout
    );

/*++

Routine Description:

    This routine acquires the read/write lock for write access just like the
    write lock function, except that this function will return after the
    specified deadline if the lock could not be acquired.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

    AbsoluteTimeout - Supplies a pointer to the absolute deadline after which
        this function should give up and return failure.

Return Value:

    0 on success.

    ETIMEDOUT if the operation timed out. This thread will not own the lock.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_rwlock_trywrlock (
    pthread_rwlock_t *Lock
    );

/*++

Routine Description:

    This routine performs a single attempt at acquiring the lock for write
    access.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_rwlock_unlock (
    pthread_rwlock_t *Lock
    );

/*++

Routine Description:

    This routine unlocks a read/write lock that's been acquired by this thread
    for either read or write.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_rwlockattr_init (
    pthread_rwlockattr_t *Attribute
    );

/*++

Routine Description:

    This routine initializes a read/write lock attribute structure.

Arguments:

    Attribute - Supplies a pointer to the read/write lock attribute.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_rwlockattr_destroy (
    pthread_rwlockattr_t *Attribute
    );

/*++

Routine Description:

    This routine destroys a read/write lock attribute structure.

Arguments:

    Attribute - Supplies a pointer to the read/write lock attribute.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_rwlockattr_getpshared (
    const pthread_rwlockattr_t *Attribute,
    int *Shared
    );

/*++

Routine Description:

    This routine reads the shared attribute from a read/write lock attributes
    structure.

Arguments:

    Attribute - Supplies a pointer to the read/write lock attribute.

    Shared - Supplies a pointer where the shared attribute of the lock will be
        returned on success. See PTHREAD_PROCESS_* definitions.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_rwlockattr_setpshared (
    pthread_rwlockattr_t *Attribute,
    int Shared
    );

/*++

Routine Description:

    This routine sets the shared attribute in a read/write lock attributes
    structure.

Arguments:

    Attribute - Supplies a pointer to the read/write lock attribute.

    Shared - Supplies the new shared value to set. See PTHREAD_PROCESS_*
        definitions.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
pthread_t
pthread_self (
    void
    );

/*++

Routine Description:

    This routine returns the thread ID for the current thread.

Arguments:

    None.

Return Value:

    None.

--*/

PTHREAD_API
int
pthread_create (
    pthread_t *CreatedThread,
    const pthread_attr_t *Attribute,
    void *(*StartRoutine)(void *),
    void *Argument
    );

/*++

Routine Description:

    This routine creates and starts a new thread. The signal mask of the new
    thread is inherited from the current thread. The set of pending signals in
    the new thread will be initially empty.

Arguments:

    CreatedThread - Supplies a pointer where the identifier of the new thread
        will be returned on success.

    Attribute - Supplies an optional pointer to the attributes of the thread.

    StartRoutine - Supplies a pointer to the routine to call on the new thread.

    Argument - Supplies the argument to pass to the start routine.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_detach (
    pthread_t ThreadId
    );

/*++

Routine Description:

    This routine marks the given thread as detached, which means that when it
    exits, its resources are automatically released without needing another
    thread to call join on it. It is illegal to call join on a detached thread,
    as the thread ID may be already released and reused.

Arguments:

    ThreadId - Supplies the thread ID to detach.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_join (
    pthread_t ThreadId,
    void **ReturnValue
    );

/*++

Routine Description:

    This routine waits for the given thread to exit and collects its return
    value. Detached threads cannot be joined.

Arguments:

    ThreadId - Supplies the identifier of the thread to join with.

    ReturnValue - Supplies a pointer where the thread return value will be
        returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
__NO_RETURN
void
pthread_exit (
    void *ReturnValue
    );

/*++

Routine Description:

    This routine exits the current thread. If this is a detached thread, then
    all thread resources are destroyed immediately. If this is a joinable
    thread, then some state is kept around until another thread calls join to
    collect the return value.

Arguments:

    ReturnValue - Supplies an optional return value to give to anyone that
        calls join.

Return Value:

    None. This routine does not return.

--*/

PTHREAD_API
int
pthread_equal (
    pthread_t FirstThread,
    pthread_t SecondThread
    );

/*++

Routine Description:

    This routine compares two thread identifiers.

Arguments:

    FirstThread - Supplies the first thread identifier to compare.

    SecondThread - Supplies the second thread identifier to compare.

Return Value:

    Returns non-zero if the two thread IDs are equal.

    0 if the thread IDs are not equal.

--*/

PTHREAD_API
int
pthread_kill (
    pthread_t ThreadId,
    int Signal
    );

/*++

Routine Description:

    This routine sends a signal to the given thread.

Arguments:

    ThreadId - Supplies the identifier of the thread to send the signal to.

    Signal - Supplies the signal number to send. Supply 0 to test if a signal
        can be sent, but not actually send any signal.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_sigqueue (
    pthread_t ThreadId,
    int Signal,
    const union sigval Value
    );

/*++

Routine Description:

    This routine queues a signal with data the given thread.

Arguments:

    ThreadId - Supplies the identifier of the thread to send the signal to.

    Signal - Supplies the signal number to send. Supply 0 to test if a signal
        can be sent, but not actually send any signal.

    Value - Supplies the signal value to send.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_cancel (
    pthread_t ThreadId
    );

/*++

Routine Description:

    This routine attempts to cancel (terminate) the thread with the given
    thread ID. This may not terminate the thread immediately if it has disabled
    or deferred cancellation.

Arguments:

    ThreadId - Supplies the identifier of the thread to cancel.

Return Value:

    0 on success.

    ESRCH if a thread with the given ID could not be found.

--*/

PTHREAD_API
int
pthread_setcancelstate (
    int State,
    int *OldState
    );

/*++

Routine Description:

    This routine atomically sets the thread cancellation state for the current
    thread and returns the old state. By default, new threads are created with
    cancellation enabled.

Arguments:

    State - Supplies the new state to set. Valid values are
        PTHREAD_CANCEL_ENABLE and PTHREAD_CANCEL_DISABLE.

    OldState - Supplies the previously set cancellation state.

Return Value:

    0 on success.

    EINVAL if an invalid new state was supplied.

--*/

PTHREAD_API
int
pthread_setcanceltype (
    int Type,
    int *OldType
    );

/*++

Routine Description:

    This routine atomically sets the thread cancellation type for the current
    thread and returns the old type. By default, new threads are created with
    deferred cancellation.

Arguments:

    Type - Supplies the new type to set. Valid values are
        PTHREAD_CANCEL_DEFERRED and PTHREAD_CANCEL_ASYNCHRONOUS.

    OldType - Supplies the previously set cancellation type.

Return Value:

    0 on success.

    EINVAL if an invalid new type was supplied.

--*/

PTHREAD_API
void
pthread_testcancel (
    void
    );

/*++

Routine Description:

    This routine creates a cancellation point in the calling thread. If
    cancellation is currently disabled, this does nothing.

Arguments:

    None.

Return Value:

    None.

--*/

PTHREAD_API
int
pthread_once (
    pthread_once_t *Once,
    void (*Routine)(void)
    );

/*++

Routine Description:

    This routine can be called by any thread in the process. The first call
    to this routine will execute the given method. All others calls will do
    nothing. On return from this routine, the routine will have completed
    executing. If the routine is a cancellation point and is canceled, then
    the effect will be as if the routine was never called.

Arguments:

    Once - Supplies a pointer to the initialized once object. Initialize it
        with the value PTHREAD_ONCE_INIT.

    Routine - Supplies a pointer to the routine to be called exactly once.

Return Value:

    0 on success.

    EINVAL if the given once object or routine is invalid.

--*/

PTHREAD_API
pid_t
pthread_gettid_np (
    pthread_t ThreadId
    );

/*++

Routine Description:

    This routine returns the kernel thread ID for the given POSIX thread ID.

Arguments:

    ThreadId - Supplies the POSIX thread ID.

Return Value:

    Returns the kernel thread ID.

--*/

PTHREAD_API
pid_t
pthread_getthreadid_np (
    void
    );

/*++

Routine Description:

    This routine returns the kernel thread ID for the current thread.

Arguments:

    None.

Return Value:

    Returns the kernel thread ID.

--*/

PTHREAD_API
int
pthread_getattr_np (
    pthread_t ThreadId,
    pthread_attr_t *Attribute
    );

/*++

Routine Description:

    This routine returns the current attributes for a given thread.

Arguments:

    ThreadId - Supplies the thread to get attributes for.

    Attribute - Supplies a pointer where the attributes will be returned. The
        detach state, stack size, stack base, and guard size may be different
        from when the thread was created to reflect their actual values.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
void
__pthread_cleanup_push (
    __pthread_cleanup_t *CleanupItem,
    __pthread_cleanup_func_t CleanupRoutine,
    void *Argument
    );

/*++

Routine Description:

    This routine pushes a new element onto the cleanup stack for the current
    thread.

Arguments:

    CleanupItem - Supplies a pointer to the cleanup item context. This routine
        uses this buffer, so it cannot be freed until the cleanup item is
        popped.

    CleanupRoutine - Supplies a pointer to the routine to call if the thread
        exits.

    Argument - Supplies a pointer to pass to the cleanup routine.

Return Value:

    None.

--*/

PTHREAD_API
void
__pthread_cleanup_pop (
    __pthread_cleanup_t *CleanupItem,
    int Execute
    );

/*++

Routine Description:

    This routine potentially pops an element from the cleanup stack.

Arguments:

    CleanupItem - Supplies a pointer to the cleanup item to pop.

    Execute - Supplies an integer that is non-zero if the cleanup routine
        should be run, or zero if it should just be popped.

Return Value:

    None.

--*/

PTHREAD_API
int
pthread_attr_init (
    pthread_attr_t *Attribute
    );

/*++

Routine Description:

    This routine initializes a thread attribute structure.

Arguments:

    Attribute - Supplies a pointer to the attribute to initialize.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_attr_destroy (
    pthread_attr_t *Attribute
    );

/*++

Routine Description:

    This routine destroys a thread attribute structure.

Arguments:

    Attribute - Supplies a pointer to the attribute to destroy.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_attr_getdetachstate (
    const pthread_attr_t *Attribute,
    int *State
    );

/*++

Routine Description:

    This routine returns the thread detach state for the given attribute.

Arguments:

    Attribute - Supplies a pointer to the attribute.

    State - Supplies a pointer where the state will be returned on success. See
        PTHREAD_CREATE_* definitions.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_attr_setdetachstate (
    pthread_attr_t *Attribute,
    int State
    );

/*++

Routine Description:

    This routine sets the thread detach state for the given attribute.

Arguments:

    Attribute - Supplies a pointer to the attribute.

    State - Supplies the new detach state to set. See PTHREAD_CREATE_*
        definitions.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_attr_getschedpolicy (
    const pthread_attr_t *Attribute,
    int *Policy
    );

/*++

Routine Description:

    This routine returns the thread scheduling policy for the given attribute.

Arguments:

    Attribute - Supplies a pointer to the attribute.

    Policy - Supplies a pointer where the policy will be returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_attr_setschedpolicy (
    const pthread_attr_t *Attribute,
    int Policy
    );

/*++

Routine Description:

    This routine sets the thread scheduling policy for the given attribute.

Arguments:

    Attribute - Supplies a pointer to the attribute.

    Policy - Supplies the new policy to set.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_attr_getschedparam (
    const pthread_attr_t *Attribute,
    int *Parameter
    );

/*++

Routine Description:

    This routine returns the thread scheduling parameter for the given
    attribute.

Arguments:

    Attribute - Supplies a pointer to the attribute.

    Parameter - Supplies a pointer where the scheduling parameter will be
        returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_attr_setschedparam (
    pthread_attr_t *Attribute,
    int Parameter
    );

/*++

Routine Description:

    This routine sets the thread scheduling parameter for the given attribute.

Arguments:

    Attribute - Supplies a pointer to the attribute.

    Parameter - Supplies the new parameter to set.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_attr_getscope (
    const pthread_attr_t *Attribute,
    int *Scope
    );

/*++

Routine Description:

    This routine returns the thread scheduling scope for the given attribute.

Arguments:

    Attribute - Supplies a pointer to the attribute.

    Scope - Supplies a pointer where the thread scope will be returned on
        success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_attr_setscope (
    pthread_attr_t *Attribute,
    int Scope
    );

/*++

Routine Description:

    This routine sets the thread scheduling scope for the given attribute.

Arguments:

    Attribute - Supplies a pointer to the attribute.

    Scope - Supplies the new scope to set.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_attr_getstacksize (
    const pthread_attr_t *Attribute,
    size_t *StackSize
    );

/*++

Routine Description:

    This routine returns the thread stack size for the given attribute.

Arguments:

    Attribute - Supplies a pointer to the attribute.

    StackSize - Supplies a pointer where the stack size will be returned on
        success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_attr_setstacksize (
    pthread_attr_t *Attribute,
    size_t StackSize
    );

/*++

Routine Description:

    This routine sets the thread stack size for the given attribute.

Arguments:

    Attribute - Supplies a pointer to the attribute.

    StackSize - Supplies the desired stack size. This should not be less than
        PTHREAD_STACK_MIN and should be a multiple of the page size.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_attr_getstack (
    const pthread_attr_t *Attribute,
    void **StackBase,
    size_t *StackSize
    );

/*++

Routine Description:

    This routine returns the thread stack information for the given attribute.

Arguments:

    Attribute - Supplies a pointer to the attribute.

    StackBase - Supplies a pointer where the stack base will be returned on
        success.

    StackSize - Supplies a pointer where the stack size will be returned on
        success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_attr_setstack (
    pthread_attr_t *Attribute,
    void *StackBase,
    size_t StackSize
    );

/*++

Routine Description:

    This routine sets the thread stack information for the given attribute.

Arguments:

    Attribute - Supplies a pointer to the attribute.

    StackBase - Supplies the stack base pointer.

    StackSize - Supplies the desired stack size. This should not be less than
        PTHREAD_STACK_MIN.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_attr_getguardsize (
    const pthread_attr_t *Attribute,
    size_t *GuardSize
    );

/*++

Routine Description:

    This routine returns the thread stack guard region size for the given
    attribute.

Arguments:

    Attribute - Supplies a pointer to the attribute.

    GuardSize - Supplies a pointer where the stack guard region size will be
        returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_attr_setguardsize (
    pthread_attr_t *Attribute,
    size_t GuardSize
    );

/*++

Routine Description:

    This routine sets the thread stack guard region size for the given
    attribute.

Arguments:

    Attribute - Supplies a pointer to the attribute.

    GuardSize - Supplies the desired stack guard region size.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_key_create (
    pthread_key_t *Key,
    void (*KeyDestructorRoutine)(void *)
    );

/*++

Routine Description:

    This routine attempts to create and reserve a new thread key.

Arguments:

    Key - Supplies a pointer where the key information will be returned.

    KeyDestructorRoutine - Supplies an optional pointer to a routine to call
        when the key is destroyed on a particular thread. This routine will
        be called with a pointer to the thread-specific value for the key.

Return Value:

    0 on success.

    EAGAIN if the system lacked the resources to create a new key slot, or
    there are too many keys.

    ENOMEM if insufficient memory exists to create the key.

--*/

PTHREAD_API
int
pthread_key_delete (
    pthread_key_t Key
    );

/*++

Routine Description:

    This routine releases a thread key. It is the responsibility of the
    application to release any thread-specific data associated with the old key.
    No destructors are called from this function.

Arguments:

    Key - Supplies a pointer to the key to delete.

Return Value:

    0 on success.

    EINVAL if the key is invalid.

--*/

PTHREAD_API
void *
pthread_getspecific (
    pthread_key_t Key
    );

/*++

Routine Description:

    This routine returns the thread-specific value for the given key.

Arguments:

    Key - Supplies a pointer to the key whose value should be returned.

Return Value:

    Returns the last value set for the current thread and key combination, or
    NULL if no value has been set or the key is not valid.

--*/

PTHREAD_API
int
pthread_setspecific (
    pthread_key_t Key,
    const void *Value
    );

/*++

Routine Description:

    This routine sets the thread-specific value for the given key and current
    thread.

Arguments:

    Key - Supplies the key whose value should be set.

    Value - Supplies the value to set.

Return Value:

    0 on success.

    EINVAL if the key passed was invalid.

--*/

__HIDDEN
int
pthread_atfork (
    void (*PrepareRoutine)(void),
    void (*ParentRoutine)(void),
    void (*ChildRoutine)(void)
    );

/*++

Routine Description:

    This routine is called to register an at-fork handler, whose callbacks are
    called immediately before and after any fork operation.

Arguments:

    PrepareRoutine - Supplies an optional pointer to a routine to be called
        immediately before a fork operation.

    ParentRoutine - Supplies an optional pointer to a routine to be called
        after a fork in the parent process.

    ChildRoutine - Supplies an optional pointer to a routine to be called
        after a fork in the child process.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
__register_atfork (
    void (*PrepareRoutine)(void),
    void (*ParentRoutine)(void),
    void (*ChildRoutine)(void),
    void *DynamicObjectHandle
    );

/*++

Routine Description:

    This routine is called to register an at-fork handler, remembering the
    dynamic object it was registered from.

Arguments:

    PrepareRoutine - Supplies an optional pointer to a routine to be called
        immediately before a fork operation.

    ParentRoutine - Supplies an optional pointer to a routine to be called
        after a fork in the parent process.

    ChildRoutine - Supplies an optional pointer to ao routine to be called
        after a fork in the child process.

    DynamicObjectHandle - Supplies an identifier unique to the dynamic object
        registering the handlers. This can be used to unregister the handles if
        the dynamic object is unloaded.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_barrier_init (
    pthread_barrier_t *Barrier,
    const pthread_barrierattr_t *Attribute,
    unsigned Count
    );

/*++

Routine Description:

    This routine initializes the given POSIX thread barrier with the given
    attributes and thread count.

Arguments:

    Barrier - Supplies a pointer to the POSIX thread barrier to be initialized.

    Attribute - Supplies an optional pointer to the attribute to use for
        initializing the barrier.

    Count - Supplies the number of threads that must wait on the barrier for it
        to be satisfied.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_barrier_destroy (
    pthread_barrier_t *Barrier
    );

/*++

Routine Description:

    This routine destroys the given POSIX thread barrier.

Arguments:

    Barrier - Supplies a pointer to the POSIX thread barrier to destroy.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_barrier_wait (
    pthread_barrier_t *Barrier
    );

/*++

Routine Description:

    This routine blocks untils the required number of threads have waited on
    the barrier. Upon success, an arbitrary thread will receive
    PTHREAD_BARRIER_SERIAL_THREAD as a return value; the rest will receive 0.
    This routine does not get interrupted by signals and will continue to block
    after a signal is handled.

Arguments:

    Barrier - Supplies a pointer to the POSIX thread barrier.

Return Value:

    0 or PTHREAD_BARRIER_SERIAL_THREAD on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_barrierattr_init (
    pthread_barrierattr_t *Attribute
    );

/*++

Routine Description:

    This routine initializes a barrier attribute structure.

Arguments:

    Attribute - Supplies a pointer to the barrier attribute structure to
        initialize.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_barrierattr_destroy (
    pthread_barrierattr_t *Attribute
    );

/*++

Routine Description:

    This routine destroys the given barrier attribute structure.

Arguments:

    Attribute - Supplies a pointer to the barrier attribute to destroy.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_barrierattr_getpshared (
    const pthread_barrierattr_t *Attribute,
    int *Shared
    );

/*++

Routine Description:

    This routine determines the shared state in a barrier attribute.

Arguments:

    Attribute - Supplies a pointer to the barrier attribute structure.

    Shared - Supplies a pointer where the shared attribute will be returned,
        indicating whether the condition variable is visible across processes.
        See PTHREAD_PROCESS_* definitions.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PTHREAD_API
int
pthread_barrierattr_setpshared (
    const pthread_barrierattr_t *Attribute,
    int Shared
    );

/*++

Routine Description:

    This routine sets the shared state in a barrier attribute.

Arguments:

    Attribute - Supplies a pointer to the barrier attribute structure.

    Shared - Supplies the value indicating whether this barrier should be
        visible across processes. See PTHREAD_PROCESS_* definitions.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

#ifdef __cplusplus

}

#endif
#endif

