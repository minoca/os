/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pthreadp.h

Abstract:

    This header contains internal definitions for the POSIX thread library.

Author:

    Evan Green 27-Apr-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "../libcp.h"
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the mask of flags reserved for the mutex type.
//

#define PTHREAD_MUTEX_TYPE_MASK 0x0000000F

//
// This bit is set if the mutex is shared between processes.
//

#define PTHREAD_MUTEX_SHARED 0x00000010

//
// Define the default stack size for a thread.
//

#define PTHREAD_DEFAULT_STACK_SIZE (2 * _1MB)

//
// Define thread flags.
//

//
// This flag is set if the thread has the detached attribute.
//

#define PTHREAD_FLAG_DETACHED 0x00000001

#define PTHREAD_ALLOCATION_TAG 0x72687450

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _PTHREAD_THREAD_STATE {
    PthreadStateInvalid,
    PthreadStateNotJoined,
    PthreadStateExited,
    PthreadStateJoined,
    PthreadStateDetached
} PTHREAD_THREAD_STATE, *PPTHREAD_THREAD_STATE;

typedef
void *
(*PPTHREAD_ENTRY_ROUTINE) (
    void *Parameter
    );

/*++

Routine Description:

    This routine is the entry point prototype for a POSIX thread.

Arguments:

    Parameter - Supplies a pointer supplied by the creator of the thread.

Return Value:

    Returns a void pointer that can be accessed by calling the join funcion.

--*/

/*++

Structure Description:

    This structure stores the internal structure of a mutex.

Members:

    State - Stores the state of the mutex.

    Owner - Stores the owner of the mutex, used when the recursive
        implementation is set.

--*/

typedef struct _PTHREAD_MUTEX {
    ULONG State;
    UINTN Owner;
} PTHREAD_MUTEX, *PPTHREAD_MUTEX;

/*++

Structure Description:

    This structure stores the internal structure of a mutex attribute.

Members:

    Flags - Stores the flags for the mutex.

--*/

typedef struct _PTHREAD_MUTEX_ATTRIBUTE {
    ULONG Flags;
} PTHREAD_MUTEX_ATTRIBUTE, *PPTHREAD_MUTEX_ATTRIBUTE;

/*++

Structure Description:

    This structure stores the internal structure of a condition variable.

Members:

    State - Stores the state of the condition variable.

--*/

typedef struct _PTHREAD_CONDITION {
    ULONG State;
} PTHREAD_CONDITION, *PPTHREAD_CONDITION;

/*++

Structure Description:

    This structure stores the internal structure of a condition variable
    attribute.

Members:

    Flags - Stores the flags for the condition variable.

--*/

typedef struct _PTHREAD_CONDITION_ATTRIBUTE {
    ULONG Flags;
} PTHREAD_CONDITION_ATTRIBUTE, *PPTHREAD_CONDITION_ATTRIBUTE;

/*++

Structure Description:

    This structure stores the internal structure of a read/write lock.

Members:

    Lock - Stores the OS library RW lock.

--*/

typedef struct _PTHREAD_RWLOCK {
    OS_RWLOCK Lock;
} PTHREAD_RWLOCK, *PPTHREAD_RWLOCK;

/*++

Structure Description:

    This structure stores the internal structure of a read/write lock
    attribute.

Members:

    Flags - Stores the flags for the read/write lock.

--*/

typedef struct _PTHREAD_RWLOCK_ATTRIBUTE {
    ULONG Flags;
} PTHREAD_RWLOCK_ATTRIBUTE, *PPTHREAD_RWLOCK_ATTRIBUTE;

/*++

Structure Description:

    This structure stores the internal structure of a POSIX semaphore.

Members:

    State - Stores the current state of the semaphore.

--*/

typedef struct _PTHREAD_SEMAPHORE {
    ULONG State;
} PTHREAD_SEMAPHORE, *PPTHREAD_SEMAPHORE;

/*++

Structure Description:

    This structure stores the internal structure of a thread attribute.

Members:

    Flags - Stores the flags for the thread.

    StackBase - Stores a pointer to the stack base.

    StackSize - Stores the size of the stack.

    GuardSize - Stores the size of the stack guard.

    SchedulingPolicy - Stores the thread scheduling policy.

    SchedulingPriority - Stores the thread scheduling priority.

--*/

typedef struct _PTHREAD_ATTRIBUTE {
    ULONG Flags;
    PVOID StackBase;
    UINTN StackSize;
    UINTN GuardSize;
    LONG SchedulingPolicy;
    LONG SchedulingPriority;
} PTHREAD_ATTRIBUTE, *PPTHREAD_ATTRIBUTE;

/*++

Structure Description:

    This structure stores the internal structure of a thread-specific key
    value.

Members:

    Sequence - Stores the sequence number associated with this value.

    Value - Stores the key value as set by the user.

--*/

typedef struct _PTHREAD_KEY_DATA {
    UINTN Sequence;
    PVOID Value;
} PTHREAD_KEY_DATA, *PPTHREAD_KEY_DATA;

/*++

Structure Description:

    This structure stores the internal structure of a POSIX thread barrier.

Members:

    State - Stores the current state of the barrier.

    ThreadCount - Stores the thread count that must be reached before waits on
        this barrier are satisified. This is set when the barrier is
        initialized.

    WaitingThreadCount - Stores the number of threads that are currently
        waiting on the barrier. This is reset once a wait is satisfied.

    Mutex - Stores the mutex that synchronizes access to the waiting thread
        count and the state.

--*/

typedef struct _PTHREAD_BARRIER {
    ULONG State;
    ULONG ThreadCount;
    ULONG WaitingThreadCount;
    pthread_mutex_t Mutex;
} PTHREAD_BARRIER, *PPTHREAD_BARRIER;

/*++

Structure Description:

    This structure stores the internal structure of a barrier attribute.

Members:

    Flags - Stores the flags for the barrier.

--*/

typedef struct _PTHREAD_BARRIER_ATTRIBUTE {
    ULONG Flags;
} PTHREAD_BARRIER_ATTRIBUTE, *PPTHREAD_BARRIER_ATTRIBUTE;

/*++

Structure Description:

    This structure stores the internal structure of a thread.

Members:

    ListEntry - Stores pointers to the next and previous threads in the global
        list.

    Attribute - Stores the thread attributes.

    ThreadRoutine - Stores the thread routine to call.

    ThreadParameter - Stores the thread parameter.

    ReturnValue - Stores the return value from the thread.

    ThreadAllocation - Stores the pointer to the allocation for this structure
        and perhaps the stack.

    ThreadAllocationSize - Stores the size of the allocation for this structure
        and perhaps the stack.

    StartMutex - Stores a mutex used to hold up the new thread until it is
        fully initialized.

    ThreadId - Stores the kernel thread identifier.

    State - Stores the state of the thread. This is of type
        PTHREAD_THREAD_STATE, but is defined as a 32-bit value so atomic
        operations can be done on it.

    CleanupStack - Stores a pointer to the top of the stack of cleanup routines
        to call.

    CancelState - Stores the current cancellation state. This is either
        PTHREAD_CANCEL_ENABLE or PTHREAD_CANCEL_DISABLE.

    CancelType - Stores the cancellation type. This is either
        PTHREAD_CANCEL_ASYNCHRONOUS or PTHREAD_CANCEL_DEFERRED.

    CancelRequested - Stores a boolean indicating whether a cancellation has
        been requested or not.

    KeyData - Stores a pointer to the key data for this thread

    OsData - Stores a pointer to the thread control data allocated by the OS
        library.

    SignalMask - Stores the original signal mask to restore once the thread
        is initialized. The thread starts with all signals blocked.

--*/

typedef struct _PTHREAD {
    LIST_ENTRY ListEntry;
    PTHREAD_ATTRIBUTE Attribute;
    PPTHREAD_ENTRY_ROUTINE ThreadRoutine;
    PVOID ThreadParameter;
    PVOID ReturnValue;
    PVOID ThreadAllocation;
    UINTN ThreadAllocationSize;
    pthread_mutex_t StartMutex;
    THREAD_ID ThreadId;
    ULONG State;
    __pthread_cleanup_t *CleanupStack;
    ULONG CancelState;
    ULONG CancelType;
    BOOL CancelRequested;
    PPTHREAD_KEY_DATA KeyData;
    PVOID OsData;
    SIGNAL_SET SignalMask;
} PTHREAD, *PPTHREAD;

//
// -------------------------------------------------------------------- Globals
//

//
// Store the global thread list.
//

extern LIST_ENTRY ClThreadList;
extern pthread_mutex_t ClThreadListMutex;

//
// -------------------------------------------------------- Function Prototypes
//

void
ClpSetIdSignalHandler (
    int Signal
    );

/*++

Routine Description:

    This routine is a signal handler called to fix up the user identity on a
    thread.

Arguments:

    Signal - Supplies the signal that caused this handler to be invoked.

Return Value:

    None.

--*/

VOID
ClpDestroyThreadKeyData (
    PPTHREAD Thread
    );

/*++

Routine Description:

    This routine destroys the thread key data for the given thread and calls
    all destructor routines.

Arguments:

    Thread - Supplies a pointer to the thread that is exiting.

Return Value:

    None.

--*/

ULONG
ClpConvertAbsoluteTimespecToRelativeMilliseconds (
    const struct timespec *AbsoluteTime,
    int Clock
    );

/*++

Routine Description:

    This routine converts an absolute timespec structure into a number of
    milliseconds from now.

Arguments:

    AbsoluteTime - Supplies a pointer to the absolute timespec to convert.

    Clock - Supplies the clock to query from.

Return Value:

    Returns the number of milliseconds from now the timespec expires in.

    0 if the absolute time is in the past.

--*/

