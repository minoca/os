/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    profthrd.c

Abstract:

    This module implements generic support for thread profiling in the debuger.

Author:

    Evan Green 14-Sep-2013

Environment:

    Debug

--*/

//
// ------------------------------------------------------------------- Includes
//

#define KERNEL_API

#include "dbgrtl.h"
#include <minoca/debug/spproto.h>
#include <minoca/lib/im.h>
#include <minoca/debug/dbgext.h>
#include "symbols.h"
#include "dbgapi.h"
#include "dbgsym.h"
#include "dbgrprof.h"
#include "dbgprofp.h"
#include "console.h"
#include "dbgrcomm.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

#define THREAD_PROFILER_USAGE                                                  \
    "Usage: profiler thread <command> [options...]\n"                          \
    "This command works with context swap and thread lifetime information \n"  \
    "sent from the target. Valid commands are:\n"                              \
    "  clear - Delete all historical data stored in the debugger.\n"           \
    "  contextswaps [threadID...] - Write the thread context swap events \n"   \
    "          out to the debugger command console. A list of thread IDs \n"   \
    "          can be optionally specified to only print events related to \n" \
    "          those threads. If not specified, data for all threads will \n"  \
    "          be printed.\n"                                                  \
    "  list  - Write a summary of all processes and threads contained in \n"   \
    "          the data.\n"                                                    \
    "  blockingqueues [threadID...] - Dump a list of blocking wait queues \n"  \
    "          threads are waiting on, sorted in descending order by the \n"   \
    "          number of times that queue has been blocked on. The list \n"    \
    "          can be optionally restricted to queues waited on by the \n"     \
    "          given list of thread IDs.\n"                                    \
    "  help  - Display this help.\n\n"

#define INITIAL_POINTER_ARRAY_CAPACITY 16

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a context swap event.

Members:

    Processor - Stores the processor number this context swap event is
        associated with.

    Event - Stores a pointer to the actual event.

--*/

typedef struct _CONTEXT_SWAP_EVENT {
    ULONG Processor;
    PROFILER_CONTEXT_SWAP Event;
} CONTEXT_SWAP_EVENT, *PCONTEXT_SWAP_EVENT;

/*++

Structure Description:

    This structure defines information about a blocking queue.

Members:

    Queue - Stores the pointer to the blocking queue.

    TotalWaitDuration - Stores the total amount of time all threads have waited
        on the queue, in time counter ticks.

    TotalWaitCount - Stores the number of waits that have occurred on this
        queue.

    ThreadList - Stores the array of threads that have waited on the queue.

--*/

typedef struct _PROFILER_BLOCKING_QUEUE {
    ULONGLONG Queue;
    ULONGLONG TotalWaitDuration;
    ULONGLONG TotalWaitCount;
    PPOINTER_ARRAY ThreadList;
} PROFILER_BLOCKING_QUEUE, *PPROFILER_BLOCKING_QUEUE;

/*++

Structure Description:

    This structure defines information about a thread blocking on an object.

Members:

    ProcessId - Stores the ID of the process that owns the thread.

    ThreadId - Stores the ID of the thread that waited.

    TotalWaitDuration - Stores the total amount of time all the thread has
        waited on the object, in time counter ticks.

    TotalWaitCount - Stores the number of waits that have occurred on this
        object.

--*/

typedef struct _PROFILER_BLOCKING_THREAD {
    ULONG ProcessId;
    ULONG ThreadId;
    ULONGLONG TotalWaitDuration;
    ULONGLONG TotalWaitCount;
} PROFILER_BLOCKING_THREAD, *PPROFILER_BLOCKING_THREAD;

typedef
VOID
(*PPOINTER_ARRAY_ITERATE_ROUTINE) (
    PVOID Element
    );

/*++

Routine Description:

    This routine is called once for each element in a pointer array.

Arguments:

    Element - Supplies a pointer to the element.

Return Value:

    None.

--*/

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
DbgrpDisplayContextSwaps (
    PDEBUGGER_CONTEXT Context,
    PULONG ThreadList,
    ULONG ThreadListSize
    );

VOID
DbgrpListProcessesAndThreads (
    PDEBUGGER_CONTEXT Context
    );

VOID
DbgrpDisplayBlockingQueues (
    PDEBUGGER_CONTEXT Context,
    PULONG ThreadList,
    ULONG ThreadListSize
    );

VOID
DbgrpFullyProcessThreadProfilingData (
    PDEBUGGER_CONTEXT Context
    );

VOID
DbgrpClearThreadProfilingData (
    PDEBUGGER_CONTEXT Context
    );

PULONG
DbgrpCreateThreadIdArray (
    PSTR *Arguments,
    ULONG ArgumentCount
    );

PSTR
DbgpGetProcessName (
    PDEBUGGER_CONTEXT Context,
    ULONG ProcessId,
    PSTR NumericBuffer,
    ULONG NumericBufferSize
    );

PSTR
DbgrpGetThreadName (
    PDEBUGGER_CONTEXT Context,
    ULONG ThreadId,
    PSTR NumericBuffer,
    ULONG NumericBufferSize
    );

int
DbgrpCompareContextSwapsByTimeAscending (
    const void *LeftPointer,
    const void *RightPointer
    );

BOOL
DbgrpReadFromProfilingBuffers (
    PLIST_ENTRY ListHead,
    PVOID Buffer,
    ULONG Size,
    BOOL Consume
    );

PPOINTER_ARRAY
DbgrpCreatePointerArray (
    ULONGLONG InitialCapacity
    );

VOID
DbgrpDestroyPointerArray (
    PPOINTER_ARRAY Array,
    PPOINTER_ARRAY_ITERATE_ROUTINE DestroyRoutine
    );

BOOL
DbgrpPointerArrayAddElement (
    PPOINTER_ARRAY Array,
    PVOID Element
    );

VOID
DbgrpCalculateDuration (
    ULONGLONG Duration,
    ULONGLONG Frequency,
    PULONGLONG TimeDuration,
    PSTR *Units,
    PBOOL TimesTen
    );

VOID
DbgrpDestroyBlockingQueue (
    PVOID Queue
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
DbgrpInitializeThreadProfiling (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine initializes support for thread profiling.

Arguments:

    Context - Supplies a pointer to the debugger context.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    Context->ThreadProfiling.StatisticsListLock = CreateDebuggerLock();
    if (Context->ThreadProfiling.StatisticsListLock == NULL) {
        return ENOMEM;
    }

    Context->ThreadProfiling.StatisticsLock = CreateDebuggerLock();
    if (Context->ThreadProfiling.StatisticsLock == NULL) {
        return ENOMEM;
    }

    Context->ThreadProfiling.ContextSwaps = DbgrpCreatePointerArray(0);
    if (Context->ThreadProfiling.ContextSwaps == NULL) {
        return ENOMEM;
    }

    Context->ThreadProfiling.Processes = DbgrpCreatePointerArray(0);
    if (Context->ThreadProfiling.Processes == NULL) {
        return ENOMEM;
    }

    Context->ThreadProfiling.Threads = DbgrpCreatePointerArray(0);
    if (Context->ThreadProfiling.Threads == NULL) {
        return ENOMEM;
    }

    INITIALIZE_LIST_HEAD(&(Context->ThreadProfiling.StatisticsListHead));
    Context->ThreadProfiling.ProcessNameWidth = 5;
    Context->ThreadProfiling.ThreadNameWidth = 5;
    return 0;
}

VOID
DbgrpDestroyThreadProfiling (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys any structures used for thread profiling.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    if (Context->ThreadProfiling.StatisticsListLock != NULL) {
        DbgrpClearThreadProfilingData(Context);
        if (Context->ThreadProfiling.Processes != NULL) {
            DbgrpDestroyPointerArray(Context->ThreadProfiling.Processes, free);
            Context->ThreadProfiling.Processes = NULL;
        }

        if (Context->ThreadProfiling.Threads != NULL) {
            DbgrpDestroyPointerArray(Context->ThreadProfiling.Threads, free);
            Context->ThreadProfiling.Threads = NULL;
        }

        DestroyDebuggerLock(Context->ThreadProfiling.StatisticsLock);
        DestroyDebuggerLock(Context->ThreadProfiling.StatisticsListLock);
        DbgrpDestroyPointerArray(Context->ThreadProfiling.ContextSwaps, free);
        Context->ThreadProfiling.ContextSwaps = NULL;
    }

    return;
}

VOID
DbgrpProcessThreadProfilingData (
    PDEBUGGER_CONTEXT Context,
    PPROFILER_DATA_ENTRY ProfilerData
    )

/*++

Routine Description:

    This routine processes a profiler notification that the debuggee sends to
    the debugger. The routine should collect the profiler data and return as
    quickly as possible.

Arguments:

    Context - Supplies a pointer to the application context.

    ProfilerData - Supplies a pointer to the newly allocated data. This routine
        will take ownership of that allocation.

Return Value:

    None.

--*/

{

    AcquireDebuggerLock(Context->ThreadProfiling.StatisticsListLock);
    INSERT_BEFORE(&(ProfilerData->ListEntry),
                  &(Context->ThreadProfiling.StatisticsListHead));

    ReleaseDebuggerLock(Context->ThreadProfiling.StatisticsListLock);
    return;
}

INT
DbgrpDispatchThreadProfilerCommand (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine handles a thread profiler command.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments.

    ArgumentCount - Supplies the number of arguments in the Arguments array.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PULONG ThreadList;
    ULONG ThreadListSize;

    assert(strcasecmp(Arguments[0], "thread") == 0);

    ThreadList = NULL;
    if (ArgumentCount < 2) {
        DbgOut(THREAD_PROFILER_USAGE);
        return EINVAL;
    }

    if (strcasecmp(Arguments[1], "clear") == 0) {
        DbgrpClearThreadProfilingData(Context);

    } else if (strcasecmp(Arguments[1], "contextswaps") == 0) {
        DbgrpFullyProcessThreadProfilingData(Context);
        ThreadListSize = 0;
        if (ArgumentCount > 2) {
            ThreadListSize = ArgumentCount - 2;
            ThreadList = DbgrpCreateThreadIdArray(Arguments + 2,
                                                      ThreadListSize);
        }

        DbgrpDisplayContextSwaps(Context, ThreadList, ThreadListSize);

    } else if (strcasecmp(Arguments[1], "list") == 0) {
        DbgrpFullyProcessThreadProfilingData(Context);
        DbgrpListProcessesAndThreads(Context);

    } else if (strcasecmp(Arguments[1], "blockingqueues") == 0) {
        DbgrpFullyProcessThreadProfilingData(Context);
        ThreadListSize = 0;
        if (ArgumentCount > 2) {
            ThreadListSize = ArgumentCount - 2;
            ThreadList = DbgrpCreateThreadIdArray(Arguments + 2,
                                                      ThreadListSize);
        }

        DbgrpDisplayBlockingQueues(Context, ThreadList, ThreadListSize);

    } else if (strcasecmp(Arguments[1], "help") == 0) {
        DbgOut(THREAD_PROFILER_USAGE);
    }

    if (ThreadList != NULL) {
        free(ThreadList);
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
DbgrpDisplayContextSwaps (
    PDEBUGGER_CONTEXT Context,
    PULONG ThreadList,
    ULONG ThreadListSize
    )

/*++

Routine Description:

    This routine prints the current context swap data.

Arguments:

    Context - Supplies a pointer to the application context.

    ThreadList - Supplies an optional pointer to an array of threads to limit
        the output to.

    ThreadListSize - Supplies the number of elements in the thread list.

Return Value:

    None.

--*/

{

    ULONG AllocationSize;
    PCONTEXT_SWAP_EVENT *Array;
    ULONGLONG Count;
    ULONGLONG Duration;
    PSTR DurationUnits;
    PCONTEXT_SWAP_EVENT Event;
    ULONGLONG Frequency;
    ULONGLONG Index;
    PULONGLONG PreviousCounts;
    PSTR ProcessName;
    CHAR ProcessNumberString[10];
    PSTR Reason;
    ULONG SearchIndex;
    PSTR ThreadName;
    CHAR ThreadNumberString[10];
    PDEBUGGER_THREAD_PROFILING_DATA ThreadProfiling;
    BOOL TimesTen;

    ThreadProfiling = &(Context->ThreadProfiling);
    if ((ThreadProfiling->ContextSwaps == NULL) ||
        (ThreadProfiling->ContextSwaps->Size == 0)) {

        DbgOut("No context swap data.\n");
        return;
    }

    //
    // Allocate space to remember the previous time counter values for each
    // processor.
    //

    AllocationSize = ThreadProfiling->ProcessorCount * sizeof(ULONGLONG);
    PreviousCounts = malloc(AllocationSize);
    if (PreviousCounts == NULL) {
        return;
    }

    memset(PreviousCounts, 0, AllocationSize);

    //
    // Sort the context swap data by counter.
    //

    Array = (PCONTEXT_SWAP_EVENT *)(ThreadProfiling->ContextSwaps->Elements);
    Count = Context->ThreadProfiling.ContextSwaps->Size;
    qsort(Array,
          Count,
          sizeof(PVOID),
          DbgrpCompareContextSwapsByTimeAscending);

    Frequency = ThreadProfiling->ReferenceTime.TimeCounterFrequency;
    for (Index = 1; Index < Count; Index += 1) {
        Event = Array[Index];

        //
        // Figure out the duration of this event.
        //

        assert(Event->Processor < ThreadProfiling->ProcessorCount);

        if (PreviousCounts[Event->Processor] == 0) {
            Duration = 0;

        } else {
            Duration = Event->Event.TimeCount -
                       PreviousCounts[Event->Processor];
        }

        PreviousCounts[Event->Processor] = Event->Event.TimeCount;

        //
        // If there's a filter list, try to find this thread in it.
        //

        if (ThreadListSize != 0) {
            for (SearchIndex = 0;
                 SearchIndex < ThreadListSize;
                 SearchIndex += 1) {

                if (ThreadList[SearchIndex] == Event->Event.ThreadId) {
                    break;
                }
            }

            if (SearchIndex == ThreadListSize) {
                continue;
            }
        }

        DbgrpCalculateDuration(Duration,
                               Frequency,
                               &Duration,
                               &DurationUnits,
                               &TimesTen);

        switch (Event->Event.EventType) {
        case ProfilerThreadEventPreemption:
            Reason = "preempted";
            break;

        case ProfilerThreadEventBlocking:
            Reason = "blocked";
            break;

        case ProfilerThreadEventYielding:
            Reason = "yielded";
            break;

        case ProfilerThreadEventSuspending:
            Reason = "suspended";
            break;

        case ProfilerThreadEventExiting:
            Reason = "exited";
            break;

        default:
            Reason = "unknown";
            break;
        }

        ProcessName = DbgpGetProcessName(Context,
                                         Event->Event.ProcessId,
                                         ProcessNumberString,
                                         sizeof(ProcessNumberString));

        ThreadName = DbgrpGetThreadName(Context,
                                        Event->Event.ThreadId,
                                        ThreadNumberString,
                                        sizeof(ThreadNumberString));

        if (TimesTen != FALSE) {
            DbgOut("%3d %*s %*s %3I64d.%d%-2s %9s",
                   Event->Processor,
                   Context->ThreadProfiling.ProcessNameWidth,
                   ProcessName,
                   Context->ThreadProfiling.ThreadNameWidth,
                   ThreadName,
                   Duration / 10UL,
                   (ULONG)(Duration % 10),
                   DurationUnits,
                   Reason);

        } else {
            DbgOut("%3d %*s %*s %5I64d%-2s %9s",
                   Event->Processor,
                   Context->ThreadProfiling.ProcessNameWidth,
                   ProcessName,
                   Context->ThreadProfiling.ThreadNameWidth,
                   ThreadName,
                   Duration,
                   DurationUnits,
                   Reason);
        }

        if (Event->Event.BlockingQueue != 0) {
            DbgOut(" %I64x\n", Event->Event.BlockingQueue);

        } else {
            DbgOut("\n");
        }
    }

    free(PreviousCounts);
    return;
}

VOID
DbgrpListProcessesAndThreads (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine lists all the processes and threads in the thread profiling
    data.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    PPROFILER_THREAD_NEW_PROCESS Process;
    PPOINTER_ARRAY Processes;
    ULONGLONG ProcessIndex;
    PPROFILER_THREAD_NEW_THREAD Thread;
    ULONGLONG ThreadIndex;
    PPOINTER_ARRAY Threads;

    Processes = Context->ThreadProfiling.Processes;
    Threads = Context->ThreadProfiling.Threads;
    if (((Processes == NULL) || (Processes->Size == 0)) &&
        ((Threads == NULL) || (Threads->Size == 0))) {

        DbgOut("No data received.\n");
        return;
    }

    DbgOut("Process Legend: StartTime ProcessId Name\n");
    DbgOut("Thread Legend: StartTime ProcessId ThreadId Name\n");
    if (Processes != NULL) {
        for (ProcessIndex = 0;
             ProcessIndex < Processes->Size;
             ProcessIndex += 1) {

            Process = Processes->Elements[ProcessIndex];
            DbgOut("Process %16I64x %d %s\n",
                   Process->TimeCounter,
                   Process->ProcessId,
                   Process->Name);

            if (Threads != NULL) {
                for (ThreadIndex = 0;
                     ThreadIndex < Threads->Size;
                     ThreadIndex += 1) {

                    Thread = Threads->Elements[ThreadIndex];
                    if (Thread->ProcessId != Process->ProcessId) {
                        continue;
                    }

                    DbgOut("    Thread  %16I64x %d %d %s\n",
                           Thread->TimeCounter,
                           Thread->ProcessId,
                           Thread->ThreadId,
                           Thread->Name);
                }
            }
        }
    }

    return;
}

VOID
DbgrpDisplayBlockingQueues (
    PDEBUGGER_CONTEXT Context,
    PULONG ThreadList,
    ULONG ThreadListSize
    )

/*++

Routine Description:

    This routine prints a summary of the wait queues generally blocked on.

Arguments:

    Context - Supplies a pointer to the application context.

    ThreadList - Supplies an optional pointer to an array of threads to limit
        the output to.

    ThreadListSize - Supplies the number of elements in the thread list.

Return Value:

    None.

--*/

{

    ULONG AllocationSize;
    PCONTEXT_SWAP_EVENT *Array;
    ULONGLONG Count;
    ULONGLONG Duration;
    PSTR DurationUnits;
    PCONTEXT_SWAP_EVENT Event;
    ULONGLONG Frequency;
    ULONGLONG Index;
    PCONTEXT_SWAP_EVENT NextEvent;
    PULONGLONG PreviousCounts;
    PCONTEXT_SWAP_EVENT PreviousNextEvent;
    PSTR ProcessName;
    CHAR ProcessNumberString[10];
    PPROFILER_BLOCKING_QUEUE Queue;
    PPOINTER_ARRAY Queues;
    BOOL Result;
    ULONG SearchIndex;
    PPROFILER_BLOCKING_THREAD Thread;
    ULONGLONG ThreadIndex;
    PSTR ThreadName;
    CHAR ThreadNumberString[10];
    PDEBUGGER_THREAD_PROFILING_DATA ThreadProfiling;
    BOOL TimesTen;

    ThreadProfiling = &(Context->ThreadProfiling);
    if ((ThreadProfiling->ContextSwaps == NULL) ||
        (ThreadProfiling->ContextSwaps->Size == 0)) {

        DbgOut("No context swap data.\n");
        return;
    }

    Queues = NULL;

    //
    // Allocate space to remember the previous time counter values for each
    // processor.
    //

    AllocationSize = ThreadProfiling->ProcessorCount * sizeof(ULONGLONG);
    PreviousCounts = malloc(AllocationSize);
    if (PreviousCounts == NULL) {
        goto DisplayBlockingQueuesEnd;
    }

    Queues = DbgrpCreatePointerArray(0);
    if (Queues == NULL) {
        goto DisplayBlockingQueuesEnd;
    }

    memset(PreviousCounts, 0, AllocationSize);

    //
    // Sort the context swap data by counter.
    //

    Array = (PCONTEXT_SWAP_EVENT *)(ThreadProfiling->ContextSwaps->Elements);
    Count = ThreadProfiling->ContextSwaps->Size;
    qsort(Array,
          Count,
          sizeof(PVOID),
          DbgrpCompareContextSwapsByTimeAscending);

    Frequency = ThreadProfiling->ReferenceTime.TimeCounterFrequency;
    for (Index = 1; Index < Count; Index += 1) {
        Event = Array[Index];
        PreviousCounts[Event->Processor] = Event->Event.TimeCount;

        //
        // If there's a filter list, try to find this thread in it.
        //

        if (ThreadListSize != 0) {
            for (SearchIndex = 0;
                 SearchIndex < ThreadListSize;
                 SearchIndex += 1) {

                if (ThreadList[SearchIndex] == Event->Event.ThreadId) {
                    break;
                }
            }

            if (SearchIndex == ThreadListSize) {
                continue;
            }
        }

        //
        // Skip it if it's not a blocking event.
        //

        if (Event->Event.BlockingQueue == 0) {
            continue;
        }

        //
        // Find the blocking queue structure, or create one if it's new.
        //

        for (SearchIndex = 0; SearchIndex < Queues->Size; SearchIndex += 1) {
            Queue = Queues->Elements[SearchIndex];
            if (Queue->Queue == Event->Event.BlockingQueue) {
                break;
            }
        }

        if (SearchIndex == Queues->Size) {
            Queue = malloc(sizeof(PROFILER_BLOCKING_QUEUE));
            if (Queue == NULL) {
                goto DisplayBlockingQueuesEnd;
            }

            memset(Queue, 0, sizeof(PROFILER_BLOCKING_QUEUE));
            Queue->Queue = Event->Event.BlockingQueue;
            Result = DbgrpPointerArrayAddElement(Queues, Queue);
            if (Result == FALSE) {
                free(Queue);
                goto DisplayBlockingQueuesEnd;
            }

            Queue->ThreadList = DbgrpCreatePointerArray(0);
            if (Queue->ThreadList == NULL) {
                goto DisplayBlockingQueuesEnd;
            }
        }

        //
        // Find the blocking thread, or create one.
        //

        for (SearchIndex = 0;
             SearchIndex < Queue->ThreadList->Size;
             SearchIndex += 1) {

            Thread = Queue->ThreadList->Elements[SearchIndex];
            if (Thread->ThreadId == Event->Event.ThreadId) {
                break;
            }
        }

        if (SearchIndex == Queue->ThreadList->Size) {
            Thread = malloc(sizeof(PROFILER_BLOCKING_THREAD));
            if (Thread == NULL) {
                goto DisplayBlockingQueuesEnd;
            }

            memset(Thread, 0, sizeof(PROFILER_BLOCKING_THREAD));
            Thread->ThreadId = Event->Event.ThreadId;
            Thread->ProcessId = Event->Event.ProcessId;
            Result = DbgrpPointerArrayAddElement(Queue->ThreadList, Thread);
            if (Result == FALSE) {
                goto DisplayBlockingQueuesEnd;
            }
        }

        //
        // Attempt to find the next time the thread was run to figure out how
        // long it blocked for.
        //

        NextEvent = NULL;
        for (SearchIndex = Index + 1; SearchIndex < Count; SearchIndex += 1) {
            NextEvent = Array[SearchIndex];
            if (NextEvent->Event.ThreadId == Event->Event.ThreadId) {
                break;
            }
        }

        //
        // The next event timestamps when that thread was swapped out. Find the
        // previous event on that same processor to figure out when it was
        // swapped in. That then represents the total wait time.
        //

        if (SearchIndex != Count) {
            SearchIndex -= 1;
            while (SearchIndex >= Index + 1) {
                PreviousNextEvent = Array[SearchIndex];
                if (PreviousNextEvent->Processor == NextEvent->Processor) {
                    break;
                }

                SearchIndex -= 1;
            }

            //
            // If all that worked, add the wait duration information to the
            // thread and queue.
            //

            if (SearchIndex >= Index + 1) {
                if (PreviousNextEvent->Event.TimeCount <
                    Event->Event.TimeCount) {

                    DbgOut("TimeCounter appeared to move backwards from "
                           "%I64x to %I64x.\n",
                           Event->Event.TimeCount,
                           PreviousNextEvent->Event.TimeCount);

                } else {
                    Duration = PreviousNextEvent->Event.TimeCount -
                               Event->Event.TimeCount;

                    Thread->TotalWaitCount += 1;
                    Thread->TotalWaitDuration += Duration;
                    Queue->TotalWaitDuration += Duration;
                    Queue->TotalWaitCount += 1;
                }
            }
        }
    }

    //
    // Loop through all the constructed queues printing them out.
    //

    DbgOut("Queue Legend: Queue BlockCount AverageBlockingDuration\n");
    DbgOut("Thread Legend: Process Thread BlockCount "
           "AverageBlockingDuration\n");

    for (Index = 0; Index < Queues->Size; Index += 1) {
        Queue = Queues->Elements[Index];
        Duration = 0;
        if (Queue->TotalWaitCount != 0) {
            Duration = Queue->TotalWaitDuration / Queue->TotalWaitCount;
        }

        DbgrpCalculateDuration(Duration,
                               Frequency,
                               &Duration,
                               &DurationUnits,
                               &TimesTen);

        if (TimesTen != FALSE) {
            DbgOut("%08I64x %6I64d %I64d.%d%-2s\n",
                   Queue->Queue,
                   Queue->TotalWaitCount,
                   Duration / 10UL,
                   (ULONG)(Duration % 10),
                   DurationUnits);

        } else {
            DbgOut("%08I64x %6I64d %I64d%-2s\n",
                   Queue->Queue,
                   Queue->TotalWaitCount,
                   Duration,
                   DurationUnits);
        }

        //
        // Print out all threads that got stuck on this object.
        //

        for (ThreadIndex = 0;
             ThreadIndex < Queue->ThreadList->Size;
             ThreadIndex += 1) {

            Thread = Queue->ThreadList->Elements[ThreadIndex];
            ProcessName = DbgpGetProcessName(Context,
                                             Thread->ProcessId,
                                             ProcessNumberString,
                                             sizeof(ProcessNumberString));

            ThreadName = DbgrpGetThreadName(Context,
                                            Thread->ThreadId,
                                            ThreadNumberString,
                                            sizeof(ThreadNumberString));

            Duration = 0;
            if (Thread->TotalWaitCount != 0) {
                Duration = Thread->TotalWaitDuration / Thread->TotalWaitCount;
                DbgrpCalculateDuration(Duration,
                                       Frequency,
                                       &Duration,
                                       &DurationUnits,
                                       &TimesTen);

                if (TimesTen != FALSE) {
                    DbgOut("    %*s %*s %6I64d %I64d.%d%-2s\n",
                           Context->ThreadProfiling.ProcessNameWidth,
                           ProcessName,
                           Context->ThreadProfiling.ThreadNameWidth,
                           ThreadName,
                           Thread->TotalWaitCount,
                           Duration / 10UL,
                           (ULONG)(Duration % 10),
                           DurationUnits);

                } else {
                    DbgOut("    %*s %*s %6I64d %I64d%-2s\n",
                           Context->ThreadProfiling.ProcessNameWidth,
                           ProcessName,
                           Context->ThreadProfiling.ThreadNameWidth,
                           ThreadName,
                           Thread->TotalWaitCount,
                           Duration,
                           DurationUnits);
                }

            }
        }

        DbgOut("\n");
    }

DisplayBlockingQueuesEnd:
    if (PreviousCounts != NULL) {
        free(PreviousCounts);
    }

    if (Queues != NULL) {
        DbgrpDestroyPointerArray(Queues, DbgrpDestroyBlockingQueue);
    }

    return;
}

VOID
DbgrpFullyProcessThreadProfilingData (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine processes unhandled thread profiling data, sorting its
    events into the proper pointer arrays.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    PCONTEXT_SWAP_EVENT ContextSwap;
    PLIST_ENTRY CurrentEntry;
    UCHAR EventType;
    ULONG Length;
    LIST_ENTRY LocalList;
    PPROFILER_THREAD_NEW_PROCESS NewProcess;
    PPROFILER_THREAD_NEW_THREAD NewThread;
    PROFILER_THREAD_NEW_PROCESS Process;
    PSTR ProcessName;
    PPROFILER_DATA_ENTRY ProfilerData;
    ULONG RemainingSize;
    BOOL Result;
    PROFILER_THREAD_NEW_THREAD Thread;
    PSTR ThreadName;

    //
    // Pull everything off of the unprocessed list as quickly as possible so
    // as not to block inoming profiling data notifications.
    //

    AcquireDebuggerLock(Context->ThreadProfiling.StatisticsListLock);
    if (LIST_EMPTY(&(Context->ThreadProfiling.StatisticsListHead)) != FALSE) {
        ReleaseDebuggerLock(Context->ThreadProfiling.StatisticsListLock);
        return;
    }

    LocalList.Next = Context->ThreadProfiling.StatisticsListHead.Next;
    LocalList.Previous = Context->ThreadProfiling.StatisticsListHead.Previous;
    LocalList.Next->Previous = &LocalList;
    LocalList.Previous->Next = &LocalList;
    INITIALIZE_LIST_HEAD(&(Context->ThreadProfiling.StatisticsListHead));
    ReleaseDebuggerLock(Context->ThreadProfiling.StatisticsListLock);

    //
    // Loop through the entries to find and take note of the maximum processor
    // number.
    //

    AcquireDebuggerLock(Context->ThreadProfiling.StatisticsLock);
    CurrentEntry = LocalList.Next;
    while (CurrentEntry != &LocalList) {
        ProfilerData = LIST_VALUE(CurrentEntry,
                                  PROFILER_DATA_ENTRY,
                                  ListEntry);

        if (ProfilerData->Processor + 1 >
            Context->ThreadProfiling.ProcessorCount) {

            Context->ThreadProfiling.ProcessorCount =
                                                   ProfilerData->Processor + 1;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    //
    // Loop through all the data in the entry, switching based on the
    // first byte which signifies the event type.
    //

    while (TRUE) {
        Result = DbgrpReadFromProfilingBuffers(&LocalList,
                                               &EventType,
                                               1,
                                               FALSE);

        if (Result == FALSE) {
            break;
        }

        assert(LIST_EMPTY(&LocalList) == FALSE);

        ProfilerData = LIST_VALUE(LocalList.Next,
                                  PROFILER_DATA_ENTRY,
                                  ListEntry);

        if ((EventType >= ProfilerThreadEventAlternateMin) &&
            (EventType < ProfilerThreadEventMax)) {

            switch (EventType) {
            case ProfilerThreadEventNewProcess:
                Result = DbgrpReadFromProfilingBuffers(
                                           &LocalList,
                                           &Process,
                                           sizeof(PROFILER_THREAD_NEW_PROCESS),
                                           TRUE);

                if (Result == FALSE) {
                    break;
                }

                if (Process.StructureSize > 0x1000) {
                    DbgOut("Got a process with giant size %x. Skipping.\n",
                           Process.StructureSize);

                    break;
                }

                NewProcess = malloc(Process.StructureSize);
                if (NewProcess == NULL) {
                    Result = FALSE;
                    break;
                }

                RtlCopyMemory(NewProcess,
                              &Process,
                              sizeof(PROFILER_THREAD_NEW_PROCESS));

                RemainingSize = Process.StructureSize -
                                sizeof(PROFILER_THREAD_NEW_PROCESS);

                Result = DbgrpReadFromProfilingBuffers(&LocalList,
                                                       NewProcess + 1,
                                                       RemainingSize,
                                                       TRUE);

                if (Result == FALSE) {
                    free(NewProcess);
                    break;
                }

                ProcessName = (PSTR)(NewProcess->Name);
                ProcessName[RemainingSize] = '\0';
                Result = DbgrpPointerArrayAddElement(
                                            Context->ThreadProfiling.Processes,
                                            NewProcess);

                if (Result == FALSE) {
                    free(NewProcess);
                    break;
                }

                Length = RtlStringLength(NewProcess->Name);
                if (Length > Context->ThreadProfiling.ProcessNameWidth) {
                    Context->ThreadProfiling.ProcessNameWidth = Length;
                }

                break;

            case ProfilerThreadEventNewThread:
                Result = DbgrpReadFromProfilingBuffers(
                                           &LocalList,
                                           &Thread,
                                           sizeof(PROFILER_THREAD_NEW_THREAD),
                                           TRUE);

                if (Result == FALSE) {
                    break;
                }

                if (Thread.StructureSize > 0x1000) {
                    DbgOut("Got a thread with giant size %x. Skipping.\n",
                           Thread.StructureSize);

                    break;
                }

                NewThread = malloc(Thread.StructureSize);
                if (NewThread == NULL) {
                    Result = FALSE;
                    break;
                }

                RtlCopyMemory(NewThread,
                              &Thread,
                              sizeof(PROFILER_THREAD_NEW_THREAD));

                RemainingSize = Thread.StructureSize -
                                sizeof(PROFILER_THREAD_NEW_THREAD);

                Result = DbgrpReadFromProfilingBuffers(&LocalList,
                                                       NewThread + 1,
                                                       RemainingSize,
                                                       TRUE);

                if (Result == FALSE) {
                    free(NewThread);
                    break;
                }

                ThreadName = (PSTR)(NewThread->Name);
                ThreadName[RemainingSize] = '\0';
                Result = DbgrpPointerArrayAddElement(
                                              Context->ThreadProfiling.Threads,
                                              NewThread);

                if (Result == FALSE) {
                    free(NewThread);
                    break;
                }

                Length = RtlStringLength(NewThread->Name);
                if (Length > Context->ThreadProfiling.ThreadNameWidth) {
                    Context->ThreadProfiling.ThreadNameWidth = Length;
                }

                break;

            case ProfilerThreadEventTimeCounter:
                Result = DbgrpReadFromProfilingBuffers(
                                     &LocalList,
                                     &(Context->ThreadProfiling.ReferenceTime),
                                     sizeof(PROFILER_THREAD_TIME_COUNTER),
                                     TRUE);

                if (Result == FALSE) {
                    break;
                }

                break;

            default:
                DbgOut("Unrecognized thread profiling event %d received.\n",
                       EventType);

                DbgrpReadFromProfilingBuffers(&LocalList,
                                              &EventType,
                                              1,
                                              TRUE);

                break;
            }

        } else {
            if (EventType >= ProfilerThreadEventSchedulerMax) {
                DbgOut("Got unknown context swap event, type %d.\n",
                       EventType);

                DbgrpReadFromProfilingBuffers(&LocalList,
                                              &EventType,
                                              1,
                                              TRUE);
            }

            //
            // It's a context switch event. Add it to that array if it's
            // large enough.
            //

            ContextSwap = malloc(sizeof(CONTEXT_SWAP_EVENT));
            if (ContextSwap != NULL) {
                ContextSwap->Processor = ProfilerData->Processor;
                Result = DbgrpReadFromProfilingBuffers(
                                                 &LocalList,
                                                 &(ContextSwap->Event),
                                                 sizeof(PROFILER_CONTEXT_SWAP),
                                                 TRUE);

                if (Result == FALSE) {
                    free(ContextSwap);
                    break;
                }

                Result = DbgrpPointerArrayAddElement(
                                         Context->ThreadProfiling.ContextSwaps,
                                         ContextSwap);

                if (Result == FALSE) {
                    free(ContextSwap);
                }
            }
        }

        if (Result == FALSE) {
            break;
        }
    }

    //
    // If there are any buffers left on the local list, put them back on the
    // main list.
    //

    if (LIST_EMPTY(&LocalList) == FALSE) {
        AcquireDebuggerLock(Context->ThreadProfiling.StatisticsListLock);
        while (LIST_EMPTY(&LocalList) == FALSE) {
            CurrentEntry = LocalList.Previous;
            LIST_REMOVE(CurrentEntry);
            INSERT_AFTER(CurrentEntry,
                         &(Context->ThreadProfiling.StatisticsListHead));
        }

        ReleaseDebuggerLock(Context->ThreadProfiling.StatisticsListLock);
    }

    ReleaseDebuggerLock(Context->ThreadProfiling.StatisticsLock);
    return;
}

VOID
DbgrpClearThreadProfilingData (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine erases all thread profiling data.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    //
    // Destroy processed entries.
    //

    AcquireDebuggerLock(Context->ThreadProfiling.StatisticsLock);
    if (Context->ThreadProfiling.ContextSwaps != NULL) {
        DbgrpDestroyPointerArray(Context->ThreadProfiling.ContextSwaps, free);
        Context->ThreadProfiling.ContextSwaps = DbgrpCreatePointerArray(0);
    }

    ReleaseDebuggerLock(Context->ThreadProfiling.StatisticsLock);

    //
    // Destroy unprocessed entries.
    //

    AcquireDebuggerLock(Context->ThreadProfiling.StatisticsListLock);
    DbgrpDestroyProfilerDataList(
                               &(Context->ThreadProfiling.StatisticsListHead));

    ReleaseDebuggerLock(Context->ThreadProfiling.StatisticsListLock);
    return;
}

PULONG
DbgrpCreateThreadIdArray (
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine implements a helper function that converts an array of strings
    into an array of thread IDs.

Arguments:

    Arguments - Supplies an array of strings containing the arguments.

    ArgumentCount - Supplies the number of arguments in the Arguments array.

Return Value:

    Returns an array of thread IDs on success.

    NULL on failure.

--*/

{

    PSTR AfterScan;
    ULONG Index;
    LONG Integer;
    PULONG IntegerArray;

    IntegerArray = malloc(ArgumentCount * sizeof(ULONG));
    if (IntegerArray == NULL) {
        return NULL;
    }

    for (Index = 0; Index < ArgumentCount; Index += 1) {
        Integer = strtoul(Arguments[Index], &AfterScan, 0);
        if (AfterScan == Arguments[Index]) {
            DbgOut("Error: Invalid thread ID '%s'.\n", Arguments[Index]);
            free(IntegerArray);
            return NULL;
        }

        IntegerArray[Index] = Integer;
    }

    return IntegerArray;
}

PSTR
DbgpGetProcessName (
    PDEBUGGER_CONTEXT Context,
    ULONG ProcessId,
    PSTR NumericBuffer,
    ULONG NumericBufferSize
    )

/*++

Routine Description:

    This routine gets the name of a given process. If the name cannot be found,
    the number will be converted to a string.

Arguments:

    Context - Supplies a pointer to the application context.

    ProcessId - Supplies the process ID to convert to a name.

    NumericBuffer - Supplies a pointer to a buffer to use to create the string
        if a name could not be found.

    NumericBufferSize - Supplies the size of the numeric buffer in bytes.

Return Value:

    Returns the name of the process, returning either a found name or the
    numeric buffer. Either way, the caller does not need to free this buffer.

--*/

{

    ULONGLONG Index;
    PPROFILER_THREAD_NEW_PROCESS Process;
    PPOINTER_ARRAY Processes;

    Processes = Context->ThreadProfiling.Processes;
    if (Processes != NULL) {
        for (Index = 0; Index < Processes->Size; Index += 1) {
            Process = Processes->Elements[Index];
            if (Process->ProcessId == ProcessId) {
                if (RtlStringLength(Process->Name) != 0) {
                    return Process->Name;
                }
            }
        }
    }

    snprintf(NumericBuffer, NumericBufferSize, "%d", ProcessId);
    return NumericBuffer;
}

PSTR
DbgrpGetThreadName (
    PDEBUGGER_CONTEXT Context,
    ULONG ThreadId,
    PSTR NumericBuffer,
    ULONG NumericBufferSize
    )

/*++

Routine Description:

    This routine gets the name of a given thread. If the name cannot be found,
    the number will be converted to a string.

Arguments:

    Context - Supplies a pointer to the application context.

    ThreadId - Supplies the thread ID to convert to a name.

    NumericBuffer - Supplies a pointer to a buffer to use to create the string
        if a name could not be found.

    NumericBufferSize - Supplies the size of the numeric buffer in bytes.

Return Value:

    Returns the name of the thread, returning either a found name or the
    numeric buffer. Either way, the caller does not need to free this buffer.

--*/

{

    ULONGLONG Index;
    PPROFILER_THREAD_NEW_THREAD Thread;
    PPOINTER_ARRAY Threads;

    Threads = Context->ThreadProfiling.Threads;
    if (Threads != NULL) {
        for (Index = 0; Index < Threads->Size; Index += 1) {
            Thread = Threads->Elements[Index];
            if (Thread->ThreadId == ThreadId) {
                if (RtlStringLength(Thread->Name) != 0) {
                    return Thread->Name;
                }
            }
        }
    }

    snprintf(NumericBuffer, NumericBufferSize, "%d", ThreadId);
    return NumericBuffer;
}

int
DbgrpCompareContextSwapsByTimeAscending (
    const void *LeftPointer,
    const void *RightPointer
    )

/*++

Routine Description:

    This routine compares the timestamps of two pointers to pointers to
    context swap events.

Arguments:

    LeftPointer - Supplies a pointer to the left element of pointer array.

    RightPointer - Supplies a pointer to the right element of the pointer array.

Return Value:

    -1 if the left timestamp is less than the right.

    0 if the left timestamp is equal to the right.

    1 if the left timestamp is greater than the right.

--*/

{

    PCONTEXT_SWAP_EVENT Left;
    PCONTEXT_SWAP_EVENT Right;

    Left = *((PCONTEXT_SWAP_EVENT *)LeftPointer);
    Right = *((PCONTEXT_SWAP_EVENT *)RightPointer);
    if (Left->Event.TimeCount < Right->Event.TimeCount) {
        return -1;
    }

    if (Left->Event.TimeCount > Right->Event.TimeCount) {
        return 1;
    }

    //
    // For events whose time counts are the same, sort by processor.
    //

    if (Left->Processor < Right->Processor) {
        return -1;
    }

    if (Left->Processor > Right->Processor) {
        return 1;
    }

    return 0;
}

BOOL
DbgrpReadFromProfilingBuffers (
    PLIST_ENTRY ListHead,
    PVOID Buffer,
    ULONG Size,
    BOOL Consume
    )

/*++

Routine Description:

    This routine reads from the profiling data buffers, freeing and consuming
    data as it goes.

Arguments:

    ListHead - Supplies a pointer to the head of the list of entries.

    Buffer - Supplies a pointer where the read data will be returned.

    Size - Supplies the number of bytes to consume.

    Consume - Supplies a boolean indicating if the bytes should be consumed
        out of the profiling buffers (TRUE) or just peeked at (FALSE).

Return Value:

    TRUE if the full amount could be read.

    FALSE if the full amount was not available in the buffers. The buffers will
    not be advanced if this is the case.

--*/

{

    ULONG BytesRead;
    PLIST_ENTRY CurrentEntry;
    PPROFILER_DATA_ENTRY Entry;
    ULONG SizeThisRound;

    //
    // Loop once performing the read.
    //

    BytesRead = 0;
    CurrentEntry = ListHead->Next;
    while (CurrentEntry != ListHead) {
        Entry = LIST_VALUE(CurrentEntry, PROFILER_DATA_ENTRY, ListEntry);

        assert(Entry->Offset <= Entry->DataSize);

        SizeThisRound = Entry->DataSize - Entry->Offset;
        if (SizeThisRound > Size - BytesRead) {
            SizeThisRound = Size - BytesRead;
        }

        RtlCopyMemory(Buffer + BytesRead,
                      Entry->Data + Entry->Offset,
                      SizeThisRound);

        BytesRead += SizeThisRound;
        if (BytesRead == Size) {
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    if (BytesRead != Size) {
        return FALSE;
    }

    if (Consume == FALSE) {
        return TRUE;
    }

    //
    // Loop again consuming the buffer.
    //

    BytesRead = 0;
    CurrentEntry = ListHead->Next;
    while (CurrentEntry != ListHead) {
        Entry = LIST_VALUE(CurrentEntry, PROFILER_DATA_ENTRY, ListEntry);

        assert(Entry->Offset <= Entry->DataSize);

        SizeThisRound = Entry->DataSize - Entry->Offset;
        if (SizeThisRound > Size - BytesRead) {
            SizeThisRound = Size - BytesRead;
        }

        BytesRead += SizeThisRound;
        Entry->Offset += SizeThisRound;
        if (BytesRead == Size) {
            break;
        }

        assert(Entry->Offset == Entry->DataSize);

        CurrentEntry = CurrentEntry->Next;
        LIST_REMOVE(&(Entry->ListEntry));
        free(Entry->Data);
        free(Entry);
    }

    return TRUE;
}

PPOINTER_ARRAY
DbgrpCreatePointerArray (
    ULONGLONG InitialCapacity
    )

/*++

Routine Description:

    This routine creates a resizeable pointer array.

Arguments:

    InitialCapacity - Supplies the initial number of elements to support. Supply
        0 to use a default value.

Return Value:

    Returns a pointer to the array on success.

    NULL on allocation failure.

--*/

{

    PPOINTER_ARRAY Array;

    Array = malloc(sizeof(POINTER_ARRAY));
    if (Array == NULL) {
        return NULL;
    }

    memset(Array, 0, sizeof(POINTER_ARRAY));
    if (InitialCapacity != 0) {
        Array->Elements = malloc(InitialCapacity * sizeof(PVOID));
        if (Array->Elements == NULL) {
            free(Array);
            return NULL;
        }

        memset(Array->Elements, 0, InitialCapacity * sizeof(PVOID));
        Array->Capacity = InitialCapacity;
    }

    return Array;
}

VOID
DbgrpDestroyPointerArray (
    PPOINTER_ARRAY Array,
    PPOINTER_ARRAY_ITERATE_ROUTINE DestroyRoutine
    )

/*++

Routine Description:

    This routine destroys a resizeable pointer array.

Arguments:

    Array - Supplies a pointer to the array to destroy.

    DestroyRoutine - Supplies an optional pointer to a function to call on
        each element to destroy it.

Return Value:

    None.

--*/

{

    ULONGLONG Index;

    if (Array == NULL) {
        return;
    }

    if (Array->Elements != NULL) {
        if (DestroyRoutine != NULL) {
            for (Index = 0; Index < Array->Size; Index += 1) {
                DestroyRoutine(Array->Elements[Index]);
            }
        }

        free(Array->Elements);
    }

    free(Array);
    return;
}

BOOL
DbgrpPointerArrayAddElement (
    PPOINTER_ARRAY Array,
    PVOID Element
    )

/*++

Routine Description:

    This routine adds an element to the end of a pointer array.

Arguments:

    Array - Supplies a pointer to the array.

    Element - Supplies the element to add.

Return Value:

    TRUE if the element was successfully added.

    FALSE on reallocation failure.

--*/

{

    PVOID *NewBuffer;
    ULONGLONG NewCapacity;

    if (Array->Size == Array->Capacity) {
        NewCapacity = Array->Capacity * 2;
        if (NewCapacity < INITIAL_POINTER_ARRAY_CAPACITY) {
            NewCapacity = INITIAL_POINTER_ARRAY_CAPACITY;
        }

        NewBuffer = realloc(Array->Elements, NewCapacity * sizeof(PVOID));
        if (NewBuffer == NULL) {
            return FALSE;
        }

        memset(NewBuffer + Array->Size,
               0,
               (NewCapacity - Array->Size) * sizeof(PVOID));

        Array->Elements = NewBuffer;
        Array->Capacity = NewCapacity;
    }

    assert(Array->Elements != NULL);

    Array->Elements[Array->Size] = Element;
    Array->Size += 1;
    return TRUE;
}

VOID
DbgrpCalculateDuration (
    ULONGLONG Duration,
    ULONGLONG Frequency,
    PULONGLONG TimeDuration,
    PSTR *Units,
    PBOOL TimesTen
    )

/*++

Routine Description:

    This routine computes the proper units of time for the given counter ticks.

Arguments:

    Duration - Supplies the duration in ticks.

    Frequency - Supplies the frequency of the timer in ticks per second.

    TimeDuration - Supplies a pointer where the duration in units of time will
        be returned.

    Units - Supplies a pointer where a constant string will be returned
        representing the units. The caller does not need to free this memory.

    TimesTen - Supplies a pointer where a boolean will be returned indicating
        whether the returned duration is multiplied by ten so the tenths unit
        can be displayed.

Return Value:

    None.

--*/

{

    *TimesTen = FALSE;
    *Units = "";
    if (Frequency != 0) {
        if ((Duration / Frequency) >= 10) {
            Duration = Duration / Frequency;
            *Units = "s";

        } else {
            Duration = (Duration * 1000000000ULL) / Frequency;
            *Units = "ns";
            if (Duration > 1000) {
                Duration = Duration / 100;
                *Units = "us";
                *TimesTen = TRUE;
                if (Duration > 10000) {
                    Duration = Duration / 1000;
                    *Units = "ms";
                    if (Duration > 10000) {
                        Duration = Duration / 1000;
                        *Units = "s";
                    }
                }
            }
        }
    }

    if ((*TimesTen != FALSE) && (Duration > 100)) {
        Duration /= 10UL;
        *TimesTen = FALSE;
    }

    *TimeDuration = Duration;
    return;
}

VOID
DbgrpDestroyBlockingQueue (
    PVOID Queue
    )

/*++

Routine Description:

    This routine destroys a profiler blocking queue.

Arguments:

    Queue - Supplies a pointer to the queue to destroy.

Return Value:

    None.

--*/

{

    PPROFILER_BLOCKING_QUEUE BlockingQueue;

    BlockingQueue = Queue;
    if (BlockingQueue->ThreadList != NULL) {
        DbgrpDestroyPointerArray(BlockingQueue->ThreadList, free);
    }

    free(BlockingQueue);
    return;
}

