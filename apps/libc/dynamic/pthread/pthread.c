/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pthread.c

Abstract:

    This module implements support for POSIX threads.

Author:

    Evan Green 29-Apr-2015

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "pthreadp.h"
#include <assert.h>
#include <limits.h>
#include <sys/mman.h>
#include <unistd.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores a thread-local destructor entry.

Members:

    ListEntry - Stores pointers to the next and previous elements in the
        destructor entry.

    DestructorRoutine - Stores a pointer to the routine to call.

    Argument - Stores a pointer to the argument to pass to the routine.

    SharedObjectHandle - Stores the shared object handle this destructor is
        associated with.

--*/

typedef struct _THREAD_DESTRUCTOR {
    LIST_ENTRY ListEntry;
    PTHREAD_ENTRY_ROUTINE DestructorRoutine;
    PVOID Argument;
    PVOID SharedObjectHandle;
} THREAD_DESTRUCTOR, *PTHREAD_DESTRUCTOR;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
ClpThreadStart (
    PVOID Parameter
    );

VOID
ClpInitializeThreading (
    VOID
    );

void
ClpThreadSignalHandler (
    int Signal
    );

int
ClpAllocateThread (
    const pthread_attr_t *Attribute,
    void *(*StartRoutine)(void *),
    void *Argument,
    PPTHREAD *Thread
    );

void
ClpDestroyThread (
    PPTHREAD Thread
    );

VOID
ClpCallThreadDestructors (
    VOID
    );

PPTHREAD
ClpGetThreadFromId (
    pthread_t ThreadId
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the global thread list.
//

LIST_ENTRY ClThreadList;
pthread_mutex_t ClThreadListMutex = PTHREAD_MUTEX_INITIALIZER;

//
// Store a thread-local pointer to the current thread.
//

__THREAD PPTHREAD ClCurrentThread;

//
// Store a thread-local list of destructor functions registered by the
// compiler.
//

__THREAD LIST_ENTRY ClThreadDestructors;

//
// ------------------------------------------------------------------ Functions
//

PTHREAD_API
pthread_t
pthread_self (
    void
    )

/*++

Routine Description:

    This routine returns the thread ID for the current thread.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PPTHREAD Thread;

    Thread = ClCurrentThread;

    //
    // If there is no current thread, then this must be the main thread.
    // Lazily allocate a new thread structure for it.
    //

    if (Thread == NULL) {
        ClpInitializeThreading();
        Thread = ClCurrentThread;
    }

    return (pthread_t)Thread;
}

PTHREAD_API
int
pthread_create (
    pthread_t *CreatedThread,
    const pthread_attr_t *Attribute,
    void *(*StartRoutine)(void *),
    void *Argument
    )

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

{

    SIGNAL_SET InternalSignals;
    KSTATUS KernelStatus;
    pthread_attr_t LocalAttributes;
    PPTHREAD NewThread;
    int Status;

    //
    // Initialize the default attributes if none were supplied.
    //

    if (Attribute == NULL) {
        pthread_attr_init(&LocalAttributes);
        Attribute = &LocalAttributes;
    }

    Status = ClpAllocateThread(Attribute, StartRoutine, Argument, &NewThread);
    if (Status != 0) {
        return Status;
    }

    //
    // Force the main thread to get with the program in case this is the first
    // thread created.
    //

    pthread_self();
    pthread_mutex_lock(&(NewThread->StartMutex));

    //
    // Block all possible signals in the new thread while it sets itself up,
    // including the internal signals.
    //

    FILL_SIGNAL_SET(InternalSignals);
    NewThread->SignalMask = OsSetSignalBehavior(SignalMaskBlocked,
                                                SignalMaskOperationOverwrite,
                                                &InternalSignals);

    KernelStatus = OsCreateThread(NULL,
                                  0,
                                  ClpThreadStart,
                                  NewThread,
                                  NewThread->Attribute.StackBase,
                                  NewThread->Attribute.StackSize,
                                  NewThread->OsData,
                                  &(NewThread->ThreadId));

    OsSetSignalBehavior(SignalMaskBlocked,
                        SignalMaskOperationOverwrite,
                        &(NewThread->SignalMask));

    if (!KSUCCESS(KernelStatus)) {
        Status = ClConvertKstatusToErrorNumber(KernelStatus);
        ClpDestroyThreadKeyData(NewThread);
        ClpDestroyThread(NewThread);
        return Status;
    }

    if ((NewThread->Attribute.Flags & PTHREAD_FLAG_DETACHED) != 0) {
        NewThread->State = PthreadStateDetached;

    } else {
        NewThread->State = PthreadStateNotJoined;
    }

    //
    // Add the thread to the global list.
    //

    pthread_mutex_lock(&ClThreadListMutex);
    INSERT_BEFORE(&(NewThread->ListEntry), &ClThreadList);
    pthread_mutex_unlock(&ClThreadListMutex);

    //
    // Let the thread run.
    //

    pthread_mutex_unlock(&(NewThread->StartMutex));
    *CreatedThread = (pthread_t)NewThread;
    return 0;
}

PTHREAD_API
int
pthread_detach (
    pthread_t ThreadId
    )

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

{

    ULONG OldState;
    PPTHREAD Thread;

    Thread = ClpGetThreadFromId(ThreadId);
    if (Thread == NULL) {
        return ESRCH;
    }

    //
    // Try to detach the thread.
    //

    OldState = RtlAtomicCompareExchange32(&(Thread->State),
                                          PthreadStateDetached,
                                          PthreadStateNotJoined);

    //
    // If the compare exchange was won, then the thread was successfully
    // detached.
    //

    if (OldState == PthreadStateNotJoined) {
        return 0;
    }

    //
    // If the thread has already exited, call join to clean up the remaining
    // thread resources.
    //

    if (OldState == PthreadStateExited) {
        return pthread_join(ThreadId, NULL);
    }

    //
    // The thread is either all funky or has already been joined in which case
    // the user is on drugs.
    //

    assert((OldState == PthreadStateJoined) ||
           (OldState == PthreadStateDetached));

    return EINVAL;
}

PTHREAD_API
int
pthread_join (
    pthread_t ThreadId,
    void **ReturnValue
    )

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

{

    ULONG NewState;
    ULONG OldState;
    THREAD_ID OsThreadId;
    PPTHREAD Thread;

    pthread_testcancel();

    //
    // Don't be ridiculous.
    //

    if (ThreadId == pthread_self()) {
        return EDEADLK;
    }

    Thread = ClpGetThreadFromId(ThreadId);
    if (Thread == NULL) {
        return ESRCH;
    }

    //
    // Try to change the state from not joined or exited to joined. This may
    // race with other calls to join (weird), detach, and the thread exiting.
    //

    OldState = PthreadStateNotJoined;
    while ((OldState == PthreadStateNotJoined) ||
           (OldState == PthreadStateExited)) {

        NewState = RtlAtomicCompareExchange32(&(Thread->State),
                                              PthreadStateJoined,
                                              OldState);

        if (OldState == NewState) {
            break;
        }

        OldState = NewState;
    }

    if ((OldState == PthreadStateDetached) ||
        (OldState == PthreadStateJoined)) {

        return EINVAL;
    }

    //
    // Wait for the thread to exit.
    //

    OsThreadId = Thread->ThreadId;
    while (Thread->ThreadId != 0) {
        OsUserLock(&(Thread->ThreadId),
                   UserLockWait,
                   (PULONG)&OsThreadId,
                   SYS_WAIT_TIME_INDEFINITE);
    }

    //
    // Get the return value if requested.
    //

    if (ReturnValue != NULL) {
        *ReturnValue = Thread->ReturnValue;
    }

    //
    // Remove and clean up the thread structure.
    //

    pthread_mutex_lock(&ClThreadListMutex);
    LIST_REMOVE(&(Thread->ListEntry));
    Thread->ListEntry.Next = NULL;
    pthread_mutex_unlock(&ClThreadListMutex);
    if (Thread->KeyData != NULL) {
        ClpDestroyThreadKeyData(Thread);
    }

    ClpDestroyThread(Thread);
    return 0;
}

PTHREAD_API
__NO_RETURN
void
pthread_exit (
    void *ReturnValue
    )

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

{

    __pthread_cleanup_t *CleanupItem;
    PVOID DestroyRegion;
    UINTN DestroyRegionSize;
    ULONG OldState;
    SIGNAL_SET SignalMask;
    PPTHREAD Thread;

    //
    // Call C++ thread_local destructors.
    //

    Thread = (PPTHREAD)pthread_self();
    ClpCallThreadDestructors();
    Thread->ReturnValue = ReturnValue;

    //
    // Call the cleanup handlers as well.
    //

    while (Thread->CleanupStack != NULL) {
        CleanupItem = Thread->CleanupStack;
        Thread->CleanupStack = CleanupItem->__cleanup_prev;
        CleanupItem->__cleanup_routine(CleanupItem->__cleanup_arg);
    }

    //
    // Clean up all thread local keys.
    //

    ClpDestroyThreadKeyData(Thread);
    DestroyRegion = NULL;
    DestroyRegionSize = 0;

    //
    // Mask out all signals, as this thread will not be handling anything else,
    // and then exit. This may touch errno so it must be done before the thread
    // is torn down.
    //

    FILL_SIGNAL_SET(SignalMask);
    OsSetSignalBehavior(SignalMaskBlocked,
                        SignalMaskOperationOverwrite,
                        &SignalMask);

    //
    // Indicate that the thread has exited.
    //

    OldState = RtlAtomicCompareExchange32(&(Thread->State),
                                          PthreadStateExited,
                                          PthreadStateNotJoined);

    if (OldState == PthreadStateDetached) {

        //
        // No one will be joining this thread, it must clean itself up. The
        // kernel will help with the last deallocation since it is the stack.
        //

        pthread_mutex_lock(&ClThreadListMutex);
        LIST_REMOVE(&(Thread->ListEntry));
        Thread->ListEntry.Next = NULL;
        pthread_mutex_unlock(&ClThreadListMutex);
        DestroyRegion = Thread->ThreadAllocation;
        DestroyRegionSize = Thread->ThreadAllocationSize;
        Thread->ThreadAllocationSize = 0;
        ClpDestroyThread(Thread);
    }

    OsExitThread(DestroyRegion, DestroyRegionSize);
    abort();
}

PTHREAD_API
int
pthread_equal (
    pthread_t FirstThread,
    pthread_t SecondThread
    )

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

{

    return FirstThread == SecondThread;
}

PTHREAD_API
int
pthread_kill (
    pthread_t ThreadId,
    int Signal
    )

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

{

    THREAD_ID KernelThreadId;
    KSTATUS Status;
    PPTHREAD Thread;

    Thread = ClpGetThreadFromId(ThreadId);
    if (Thread == NULL) {
        return ESRCH;
    }

    KernelThreadId = Thread->ThreadId;
    if (KernelThreadId == 0) {
        return ESRCH;
    }

    Status = OsSendSignal(SignalTargetThread,
                          KernelThreadId,
                          Signal,
                          SIGNAL_CODE_THREAD_KILL,
                          0);

    if (!KSUCCESS(Status)) {
        return ClConvertKstatusToErrorNumber(Status);
    }

    return 0;
}

PTHREAD_API
int
pthread_sigqueue (
    pthread_t ThreadId,
    int Signal,
    const union sigval Value
    )

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

{

    KSTATUS Status;
    PPTHREAD Thread;

    ASSERT(sizeof(void *) >= sizeof(int));

    Thread = ClpGetThreadFromId(ThreadId);
    if (Thread == NULL) {
        return ESRCH;
    }

    Status = OsSendSignal(SignalTargetThread,
                          Thread->ThreadId,
                          Signal,
                          SIGNAL_CODE_QUEUE,
                          (UINTN)(Value.sival_ptr));

    if (!KSUCCESS(Status)) {
        return ClConvertKstatusToErrorNumber(Status);
    }

    return 0;
}

PTHREAD_API
int
pthread_cancel (
    pthread_t ThreadId
    )

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

{

    PPTHREAD Thread;

    Thread = ClpGetThreadFromId(ThreadId);
    if (Thread == NULL) {
        return ESRCH;
    }

    Thread->CancelRequested = TRUE;
    pthread_kill(ThreadId, SIGNAL_PTHREAD);
    return 0;
}

PTHREAD_API
int
pthread_setcancelstate (
    int State,
    int *OldState
    )

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

{

    ULONG PreviousState;
    PPTHREAD Thread;

    if ((State != PTHREAD_CANCEL_ENABLE) && (State != PTHREAD_CANCEL_DISABLE)) {
        return EINVAL;
    }

    Thread = (PPTHREAD)pthread_self();
    PreviousState = RtlAtomicExchange32(&(Thread->CancelState), State);
    if (OldState != NULL) {
        *OldState = PreviousState;
    }

    if (State == PTHREAD_CANCEL_ENABLE) {
        pthread_testcancel();
    }

    return 0;
}

PTHREAD_API
int
pthread_setcanceltype (
    int Type,
    int *OldType
    )

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

{

    ULONG PreviousType;
    PPTHREAD Thread;

    if ((Type != PTHREAD_CANCEL_DEFERRED) &&
        (Type != PTHREAD_CANCEL_ASYNCHRONOUS)) {

        return EINVAL;
    }

    Thread = (PPTHREAD)pthread_self();
    PreviousType = RtlAtomicExchange32(&(Thread->CancelType), Type);
    if (OldType != NULL) {
        *OldType = PreviousType;
    }

    if (Type == PTHREAD_CANCEL_ASYNCHRONOUS) {
        pthread_testcancel();
    }

    return 0;
}

PTHREAD_API
void
pthread_testcancel (
    void
    )

/*++

Routine Description:

    This routine creates a cancellation point in the calling thread. If
    cancellation is currently disabled, this does nothing.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PPTHREAD Thread;

    Thread = (PPTHREAD)pthread_self();
    if (Thread->CancelState != PTHREAD_CANCEL_ENABLE) {
        return;
    }

    if (Thread->CancelRequested != FALSE) {
        pthread_exit(PTHREAD_CANCELED);
    }

    return;
}

PTHREAD_API
pid_t
pthread_gettid_np (
    pthread_t ThreadId
    )

/*++

Routine Description:

    This routine returns the kernel thread ID for the given POSIX thread ID.

Arguments:

    ThreadId - Supplies the POSIX thread ID.

Return Value:

    Returns the kernel thread ID.

--*/

{

    PPTHREAD Thread;

    Thread = (PPTHREAD)ThreadId;
    return Thread->ThreadId;
}

PTHREAD_API
pid_t
pthread_getthreadid_np (
    void
    )

/*++

Routine Description:

    This routine returns the kernel thread ID for the current thread.

Arguments:

    None.

Return Value:

    Returns the kernel thread ID.

--*/

{

    return pthread_gettid_np(pthread_self());
}

PTHREAD_API
int
pthread_getattr_np (
    pthread_t ThreadId,
    pthread_attr_t *Attribute
    )

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

{

    PPROCESS_ENVIRONMENT Environment;
    int Error;
    struct rlimit Limit;
    int OldError;
    PPTHREAD Thread;

    Thread = (PPTHREAD)ThreadId;
    if (Thread->State == PthreadStateDetached) {
        Thread->Attribute.Flags |= PTHREAD_FLAG_DETACHED;
    }

    //
    // For the main thread, try to get the stack size.
    //

    if (Thread->Attribute.StackSize == 0) {
        OldError = errno;
        if (getrlimit(RLIMIT_STACK, &Limit) < 0) {
            Error = errno;
            errno = OldError;
            return Error;
        }

        Thread->Attribute.StackSize = Limit.rlim_cur;
        if (Thread->Attribute.StackBase == NULL) {
            Environment = OsGetCurrentEnvironment();
            Thread->Attribute.StackBase = Environment->StartData->StackBase;
        }
    }

    memcpy(Attribute, &(Thread->Attribute), sizeof(PTHREAD_ATTRIBUTE));
    return 0;
}

PTHREAD_API
void
__pthread_cleanup_push (
    __pthread_cleanup_t *CleanupItem,
    __pthread_cleanup_func_t CleanupRoutine,
    void *Argument
    )

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

{

    PPTHREAD Thread;

    Thread = (PPTHREAD)pthread_self();
    CleanupItem->__cleanup_routine = CleanupRoutine;
    CleanupItem->__cleanup_arg = Argument;
    CleanupItem->__cleanup_prev = Thread->CleanupStack;
    Thread->CleanupStack = CleanupItem;
    return;
}

PTHREAD_API
void
__pthread_cleanup_pop (
    __pthread_cleanup_t *CleanupItem,
    int Execute
    )

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

{

    PPTHREAD Thread;

    Thread = (PPTHREAD)pthread_self();
    Thread->CleanupStack = CleanupItem->__cleanup_prev;
    if (Execute != 0) {
        CleanupItem->__cleanup_routine(CleanupItem->__cleanup_arg);
    }

    return;
}

LIBC_API
int
__cxa_thread_atexit_impl (
    PTHREAD_ENTRY_ROUTINE DestructorRoutine,
    void *Argument,
    void *DynamicObjectHandle
    )

/*++

Routine Description:

    This routine registers a new thread-local destructor, that is called when
    the thread is destroyed.

Arguments:

    DestructorRoutine - Supplies a pointer to the routine to call when the
        thread is destroyed.

    Argument - Supplies an argument to pass to the destructor routine.

    DynamicObjectHandle - Supplies a pointer to the dynamic object this
        destructor is associated with.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PTHREAD_DESTRUCTOR Destructor;

    Destructor = malloc(sizeof(THREAD_DESTRUCTOR));
    if (Destructor == NULL) {
        return ENOMEM;
    }

    Destructor->DestructorRoutine = DestructorRoutine;
    Destructor->Argument = Argument;
    Destructor->SharedObjectHandle = DynamicObjectHandle;
    if (ClThreadDestructors.Next == NULL) {
        INITIALIZE_LIST_HEAD(&ClThreadDestructors);
    }

    INSERT_AFTER(&(Destructor->ListEntry), &ClThreadDestructors);
    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
ClpThreadStart (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine implements the initial routine for a POSIX thread.

Arguments:

    Parameter - Supplies a pointer to the parameter, which in this case is a
        pointer to the PTHREAD structure for this thread.

Return Value:

    None. Actually, this routine never returns.

--*/

{

    PVOID Result;
    PPTHREAD Thread;

    Thread = Parameter;

    //
    // Acquire the start mutex to synchronize with the thread that created this
    // thread.
    //

    ClCurrentThread = Thread;
    pthread_mutex_lock(&(Thread->StartMutex));
    OsSetSignalBehavior(SignalMaskBlocked,
                        SignalMaskOperationOverwrite,
                        &(Thread->SignalMask));

    pthread_testcancel();
    Result = Thread->ThreadRoutine(Thread->ThreadParameter);
    pthread_exit(Result);
    return;
}

VOID
ClpInitializeThreading (
    VOID
    )

/*++

Routine Description:

    This routine is called to initialize threading support, mostly performing
    some initialization tasks on the main thread that were deferred for
    better performance on non-threaded applications. This code must be called
    on the main thread.

Arguments:

    None.

Return Value:

    None.

--*/

{

    struct sigaction Action;
    UINTN AllocationSize;
    PPTHREAD Thread;

    AllocationSize = sizeof(PTHREAD) +
                     (PTHREAD_KEYS_MAX * sizeof(PTHREAD_KEY_DATA));

    Thread = OsHeapAllocate(AllocationSize, PTHREAD_ALLOCATION_TAG);
    if (Thread == NULL) {
        return;
    }

    memset(Thread, 0, AllocationSize);
    pthread_mutex_init(&(Thread->StartMutex), NULL);
    Thread->KeyData = (PPTHREAD_KEY_DATA)(Thread + 1);

    //
    // Set the thread ID pointer of the main thread in the kernel so that
    // other threads can join the main thread if desired.
    //

    OsSetThreadIdPointer(&(Thread->ThreadId));
    Thread->State = PthreadStateNotJoined;
    ClCurrentThread = Thread;

    //
    // Add the thread to the global list.
    //

    pthread_mutex_lock(&ClThreadListMutex);

    ASSERT(ClThreadList.Next == NULL);

    INITIALIZE_LIST_HEAD(&ClThreadList);
    INSERT_AFTER(&(Thread->ListEntry), &ClThreadList);
    pthread_mutex_unlock(&ClThreadListMutex);

    //
    // Also register the thread signal handler and the set ID signal handler.
    //

    sigemptyset(&(Action.sa_mask));
    Action.sa_flags = 0;
    Action.sa_handler = ClpThreadSignalHandler;
    ClpSetSignalAction(SIGNAL_PTHREAD, &Action, NULL);
    Action.sa_handler = ClpSetIdSignalHandler;
    ClpSetSignalAction(SIGNAL_SETID, &Action, NULL);

    //
    // Make sure the PLT entry for the exit thread routine is wired up.
    // Detached threads carefully tear themselves down, and cannot handle a
    // PLT lookup by the time they call exit thread.
    //
    // This does not actually exit the current thread, it simply returns.
    //

    OsExitThread(NULL, -1);
    return;
}

void
ClpThreadSignalHandler (
    int Signal
    )

/*++

Routine Description:

    This routine implements the thread signal handler, which manages
    cancellation requests.

Arguments:

    Signal - Supplies the signal number that occurred.

Return Value:

    None.

--*/

{

    PPTHREAD Thread;

    Thread = (PPTHREAD)pthread_self();

    ASSERT(Thread->CancelRequested != FALSE);

    if (Thread->CancelType == PTHREAD_CANCEL_ASYNCHRONOUS) {
        pthread_testcancel();
    }

    return;
}

int
ClpAllocateThread (
    const pthread_attr_t *Attribute,
    void *(*StartRoutine)(void *),
    void *Argument,
    PPTHREAD *Thread
    )

/*++

Routine Description:

    This routine allocates and initializes a new thread structure.

Arguments:

    Attribute - Supplies a required pointer to the attributes of the thread.

    StartRoutine - Supplies a pointer to the routine to call on the new thread.

    Argument - Supplies the argument to pass to the start routine.

    Thread - Supplies a pointer where a pointer to the thread will be returned
        on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PVOID Allocation;
    PPTHREAD_ATTRIBUTE AttributeInternal;
    UINTN GuardSize;
    KSTATUS KernelStatus;
    ULONG MapFlags;
    UINTN MapSize;
    PPTHREAD NewThread;
    UINTN PageSize;
    PVOID Stack;
    UINTN StackSize;
    int Status;

    Allocation = NULL;

    //
    // If the caller wants something non-standard for a stack, then allocate
    // the stack.
    //

    GuardSize = 0;
    NewThread = NULL;
    PageSize = sysconf(_SC_PAGE_SIZE);
    AttributeInternal = (PPTHREAD_ATTRIBUTE)Attribute;
    Stack = AttributeInternal->StackBase;
    StackSize = 0;
    MapSize = sizeof(PTHREAD) + (PTHREAD_KEYS_MAX * sizeof(PTHREAD_KEY_DATA));

    //
    // Also allocate a stack if none was supplied but a weird guard size was
    // requested.
    //

    if ((Stack == NULL) && (AttributeInternal->GuardSize != PageSize)) {
        StackSize = ALIGN_RANGE_UP(AttributeInternal->StackSize, 16);
        MapSize += StackSize;
        MapSize = ALIGN_RANGE_UP(MapSize, PageSize);
        GuardSize = ALIGN_RANGE_UP(AttributeInternal->GuardSize, PageSize);
        MapSize += GuardSize;
    }

    MapFlags = MAP_PRIVATE | MAP_ANONYMOUS;
    Allocation = mmap(Stack, MapSize, PROT_READ | PROT_WRITE, MapFlags, -1, 0);
    if (Allocation == MAP_FAILED) {
        Status = errno;
        goto AllocateThreadEnd;
    }

    if ((Stack == NULL) && (AttributeInternal->GuardSize != PageSize)) {
        if (GuardSize != 0) {
            if (mprotect(Stack, GuardSize, PROT_NONE) < 0) {
                Status = errno;
                goto AllocateThreadEnd;
            }
        }
    }

    NewThread = Allocation + GuardSize + StackSize;
    memcpy(&(NewThread->Attribute),
           AttributeInternal,
           sizeof(PTHREAD_ATTRIBUTE));

    NewThread->Attribute.StackBase = Stack;
    if (StackSize != 0) {
        NewThread->Attribute.StackSize = StackSize;
    }

    NewThread->ThreadRoutine = StartRoutine;
    NewThread->ThreadParameter = Argument;
    NewThread->ThreadAllocation = Allocation;
    NewThread->ThreadAllocationSize = MapSize;
    NewThread->KeyData = (PPTHREAD_KEY_DATA)(NewThread + 1);
    pthread_mutex_init(&(NewThread->StartMutex), NULL);
    KernelStatus = OsCreateThreadData(&(NewThread->OsData));
    if (!KSUCCESS(KernelStatus)) {
        Status = ClConvertKstatusToErrorNumber(KernelStatus);
        goto AllocateThreadEnd;
    }

    Status = 0;

AllocateThreadEnd:
    if (Status != 0) {
        if (Allocation != NULL) {
            munmap(Allocation, MapSize);
        }

        NewThread = NULL;
    }

    *Thread = NewThread;
    return Status;
}

void
ClpDestroyThread (
    PPTHREAD Thread
    )

/*++

Routine Description:

    This routine frees a thread structure. The thread better not be alive there.

Arguments:

    Thread - Supplies a pointer to the thread structure to destroy.

Return Value:

    None.

--*/

{

    assert(Thread->KeyData == NULL);

    pthread_mutex_destroy(&(Thread->StartMutex));
    if (Thread->OsData != NULL) {
        OsDestroyThreadData(Thread->OsData);
        Thread->OsData = NULL;
    }

    if (Thread->ThreadAllocationSize != 0) {
        munmap(Thread->ThreadAllocation, Thread->ThreadAllocationSize);
    }

    return;
}

VOID
ClpCallThreadDestructors (
    VOID
    )

/*++

Routine Description:

    This routine calls all the thread destructor routines in the reverse order
    as they were registered.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PTHREAD_DESTRUCTOR Destructor;

    if (ClThreadDestructors.Next == NULL) {
        return;
    }

    CurrentEntry = ClThreadDestructors.Next;
    while (CurrentEntry != &ClThreadDestructors) {
        Destructor = LIST_VALUE(CurrentEntry, THREAD_DESTRUCTOR, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        Destructor->DestructorRoutine(Destructor->Argument);
        free(Destructor);
    }

    return;
}

PPTHREAD
ClpGetThreadFromId (
    pthread_t ThreadId
    )

/*++

Routine Description:

    This routine looks up the given thread based on its thread ID.

Arguments:

    ThreadId - Supplies the thread ID to look up.

Return Value:

    Returns a pointer to the internal thread structure on success.

    NULL on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PPTHREAD CurrentThread;
    PPTHREAD FoundThread;

    if (ClThreadList.Next == NULL) {
        pthread_self();
    }

    FoundThread = NULL;
    pthread_mutex_lock(&ClThreadListMutex);
    CurrentEntry = ClThreadList.Next;
    while (CurrentEntry != &ClThreadList) {
        CurrentThread = LIST_VALUE(CurrentEntry, PTHREAD, ListEntry);
        if (CurrentThread == (PPTHREAD)ThreadId) {
            FoundThread = CurrentThread;
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    pthread_mutex_unlock(&ClThreadListMutex);
    return FoundThread;
}

