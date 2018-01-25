/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    spproto.h

Abstract:

    This header contains definitions for the system profiler protocol. It is
    used by both the profiling application and the target system.

Author:

    Chris Stevens 11-Jul-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define a sentinel value for data within the profiler sampling array.
//

#define PROFILER_DATA_SENTINEL 0x57A67000
#define PROFILER_DATA_SENTINEL_MASK 0xFFFFF000

//
// Define a macro to test whether or not a value is the sentinel.
//

#define IS_PROFILER_DATA_SENTINEL(_Value) \
    ((_Value & PROFILER_DATA_SENTINEL_MASK) == PROFILER_DATA_SENTINEL)

//
// Define a macro to get the data size, in bytes, from a profiler data
// sentinel.
//

#define GET_PROFILER_DATA_SIZE(_Value) \
    (_Value & ~PROFILER_DATA_SENTINEL_MASK)

//
// Define the various types of profiling data available for collection.
//

#define PROFILER_TYPE_FLAG_STACK_SAMPLING    0x00000001
#define PROFILER_TYPE_FLAG_MEMORY_STATISTICS 0x00000002
#define PROFILER_TYPE_FLAG_THREAD_STATISTICS 0x00000004

//
// Define the minimum length of the profiler notification data buffer.
//

#define PROFILER_NOTIFICATION_SIZE 1

//
// Defines a value that marks the head of a proflier pool memory structure.
//

#define PROFILER_POOL_MAGIC 0x6C6F6F50 // 'looP'

//
// ------------------------------------------------------ Data Type Definitions
//

//
// The positive values here line up with the SCHEDULER_REASON enum.
//

typedef enum _PROFILER_THREAD_EVENT {
    ProfilerThreadEventInvalid       = 0,
    ProfilerThreadEventPreemption    = 1,
    ProfilerThreadEventBlocking      = 2,
    ProfilerThreadEventYielding      = 3,
    ProfilerThreadEventSuspending    = 4,
    ProfilerThreadEventExiting       = 5,
    ProfilerThreadEventSchedulerMax,
    ProfilerThreadEventAlternateMin  = 0x80,
    ProfilerThreadEventNewThread     = 0x80,
    ProfilerThreadEventNewProcess    = 0x81,
    ProfilerThreadEventTimeCounter   = 0x82,
    ProfilerThreadEventMax
} PROFILER_THREAD_EVENT, *PPROFILER_THREAD_EVENT;

/*++

Enumeration Descriptoin:

    This enumeration describes the various profiler data types.

Values:

    ProfilerDataTypeStack - Indicates that the profiler data is from stack
        sampling.

    ProfilerDataTypeMemory - Indicates that the profiler data is from memory
        statistics.

    ProfilerDataTypeThread - Indicates that the profiler data is from the
        thread profiler.

    ProfilerDataTypeMax - Indicates an invalid profiler data type and the total
        number of profiler types.

--*/

typedef enum _PROFILER_DATA_TYPE {
    ProfilerDataTypeInvalid,
    ProfilerDataTypeStack,
    ProfilerDataTypeMemory,
    ProfilerDataTypeThread,
    ProfilerDataTypeMax
} PROFILER_DATA_TYPE, *PPROFILER_DATA_TYPE;

/*++

Structure Description:

    This structure defines the header of a profiler notification payload. It is
    sent by the profiling producer to the consumer on periodic clock intervals.

Members:

    Processor - Stores the number of the processor that is sending this
        notification to the consumer.

    DataSize - Stores the size of the rest of the profiler notification, which
        follows immediately after this field.

--*/

#pragma pack(push, 1)

typedef struct _PROFILER_NOTIFICATION_HEADER {
    PROFILER_DATA_TYPE Type;
    ULONG Processor;
    ULONG DataSize;
    // Data follows here.
} PACKED PROFILER_NOTIFICATION_HEADER, *PPROFILER_NOTIFICATION_HEADER;

/*++

Structure Description:

    This structure defines the contents of a profiler notification.

Members:

    Header - Stores a profiler notification header.

    Data - Stores an array of bytes that store the profiler data being sent to
        the consumer.

--*/

typedef struct _PROFILER_NOTIFICATION {
    PROFILER_NOTIFICATION_HEADER Header;
    BYTE Data[PROFILER_NOTIFICATION_SIZE];
} PACKED PROFILER_NOTIFICATION, *PPROFILER_NOTIFICATION;

#pragma pack(pop)

/*++

Enumeration Descriptoin:

    This enumeration describes the various memory types used by the profiler.

Values:

    ProfilerMemoryTypeNonPagedPool - Indicates that the profiler memory is of
        non-paged pool type.

    ProfilerMemoryTypePagedPool - Indicates that the profiler memory is of
        paged pool type.

    ProfilerMEmoryTypeMax - Indicates the maximum number of profiler memory
        types.

--*/

typedef enum _PROFILER_MEMORY_TYPE {
    ProfilerMemoryTypeNonPagedPool,
    ProfilerMemoryTypePagedPool,
    ProfilerMemoryTypeMax
} PROFILER_MEMORY_TYPE, *PPROFILER_MEMORY_TYPE;

/*++

Structure Description:

    This structure defines a pool of memory for the profiler.

Members:

    Magic - Stores a magic number, POOL_MAGIC. This is used by the initialize
        routine to determine if the emergency resources are utilized or just
        uninitialized.

    TagCount - Stores the number of unique tags that have been used for
        allocations.

    ProfilerMemoryType - Stores the profiler memory type that the pool should
        use when requesting additional memory.

    TotalPoolSize - Stores the total size of the memory pool, in bytes.

    FreeListSize - Stores the amount of free memory in the pool, in bytes.

    TotalAllocationCalls - Stores the number of calls to allocate memory since
        the pool's initialization.

    FailedAllocations - Stores the number of calls to allocate memory that
        have been failed.

    TotalFreeCalls - Stores the number of calls to free memory since the pool's
        initialization.

--*/

#pragma pack(push, 1)

typedef struct _PROFILER_MEMORY_POOL {
    ULONG Magic;
    ULONG TagCount;
    PROFILER_MEMORY_TYPE ProfilerMemoryType;
    ULONGLONG TotalPoolSize;
    ULONGLONG FreeListSize;
    ULONGLONG TotalAllocationCalls;
    ULONGLONG FailedAllocations;
    ULONGLONG TotalFreeCalls;
} PACKED PROFILER_MEMORY_POOL, *PPROFILER_MEMORY_POOL;

/*++

Structure Description:

    This structure defines profiler statistics for one allocation tag.

Members:

    Tag - Stores the allocation tag associated with this statistic.

    LargestAllocation - Stores the largest single allocation ever made under
        this tag, in bytes.

    ActiveSize - Stores the total number of bytes currently allocated under
        this tag.

    LargestActiveSize - Stores the largest number of bytes the active size has
        ever been.

    LifetimeAllocationSize - Stores the total number of bytes that have been
        allocated under this tag (not necessarily all at once).

    ActiveAllocationCount - Stores the current number of allocations under this
        allocation tag.

    LargestActiveAllocationCount - Stores the largest number the active
        allocation count has ever been for this tag.

--*/

typedef struct _PROFILER_MEMORY_POOL_TAG_STATISTIC {
    ULONG Tag;
    ULONG LargestAllocation;
    ULONGLONG ActiveSize;
    ULONGLONG LargestActiveSize;
    ULONGLONG LifetimeAllocationSize;
    ULONG ActiveAllocationCount;
    ULONG LargestActiveAllocationCount;
} PACKED PROFILER_MEMORY_POOL_TAG_STATISTIC,
         *PPROFILER_MEMORY_POOL_TAG_STATISTIC;

/*++

Structure Description:

    This structure defines a context swap event in the profiler.

Members:

    EventType - Stores the type of event that occurred. This identifies it as a
        context swap event (by using positive numbers), and also provides the
        scheduling out reason.

    ScheduleOutReason - Stores the reason for the scheduling event.

    TimeCount - Stores the current time counter value.

    BlockingQueue - Stores a pointer to the queue that the old thread is
        blocking on in the case of a blocking event.

    ThreadId - Stores the ID of the thread being scheduled out.

    ProcessId - Stores the ID of the process that owns the thread.

--*/

typedef struct _PROFILER_CONTEXT_SWAP {
    CHAR EventType;
    ULONGLONG TimeCount;
    ULONGLONG BlockingQueue;
    ULONG ThreadId;
    ULONG ProcessId;
} PACKED PROFILER_CONTEXT_SWAP, *PPROFILER_CONTEXT_SWAP;

/*++

Structure Description:

    This structure defines a time counter calibration event in the thread
    profiler.

Members:

    EventType - Stores the event type, which will always be
        ProfilerThreadEventTimeCounter.

    TimeCounter - Stores a value of the time counter.

    SystemTimeSeconds - Stores the seconds of the system time that matches the
        time counter value.

    SystemTimeNanoseconds - Stores the seconds of the system time that matches
        the time counter value.

    TimeCounterFrequency - Stores the frequency of the time counter.

--*/

typedef struct _PROFILER_THREAD_TIME_COUNTER {
    UCHAR EventType;
    ULONGLONG TimeCounter;
    ULONGLONG TimeCounterFrequency;
    ULONGLONG SystemTimeSeconds;
    ULONG SystemTimeNanoseconds;
} PACKED PROFILER_THREAD_TIME_COUNTER, *PPROFILER_THREAD_TIME_COUNTER;

/*++

Structure Description:

    This structure defines a process creation event.

Members:

    EventType - Stores the event type, which will always be
        ProfilerThreadEventNewProcess.

    StructureSize - Stores the size of the structure including the null
        terminated name.

    ProcessId - Stores the identifier of the process.

    TimeCounter - Stores the time counter value when this process was created,
        or 0 if the process was created before profiling was enabled.

    Name - Stores the null terminated name of the process.

--*/

typedef struct _PROFILER_THREAD_NEW_PROCESS {
    UCHAR EventType;
    ULONG StructureSize;
    ULONG ProcessId;
    ULONGLONG TimeCounter;
    CHAR Name[ANYSIZE_ARRAY];
} PACKED PROFILER_THREAD_NEW_PROCESS, *PPROFILER_THREAD_NEW_PROCESS;

/*++

Structure Description:

    This structure defines a thread creation event.

Members:

    EventType - Stores the event type, which will always be
        ProfilerThreadEventNewThread.

    StructureSize - Stores the size of the structure including the null
        terminated name.

    ThreadId - Stores the identifier of the thread.

    TimeCounter - Stores the time counter value when this thread was created,
        or 0 if the thread was created before profiling was enabled.

    Name - Stores the null terminated name of the process.

--*/

typedef struct _PROFILER_THREAD_NEW_THREAD {
    UCHAR EventType;
    ULONG StructureSize;
    ULONG ProcessId;
    ULONG ThreadId;
    ULONGLONG TimeCounter;
    CHAR Name[ANYSIZE_ARRAY];
} PACKED PROFILER_THREAD_NEW_THREAD, *PPROFILER_THREAD_NEW_THREAD;

#pragma pack(pop)

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

