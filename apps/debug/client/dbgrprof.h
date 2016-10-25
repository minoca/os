/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dbgrprof.h

Abstract:

    This header contains definitions for the debugger profiler support.

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
// ------------------------------------------------------ Data Type Definitions
//

/*++

Enumeration Description:

    This enumeration describes the various profiler display requests that can
    be made to a profiler UI console.

Values:

    ProfilerDislayInvalid - Indicates an invalid choice.

    ProfilerDisplayOneTime - Indicates that a one-time snap shot of the data
        should be displayed.

    ProfilerDisplayThreshold - Indicates that a one-time snap shot of the data
        should be displayed, but only display data up to the threshold value.

    ProfilerDisplayStart - Indicates that continuous collection and display of
        profiler data should begin.

    ProfilerDisplayStop - Indicates that continuous collection and display of
        profiler data shoul stop.

    ProfilerDisplayClear - Indicates that the profiler data display should be
        cleared of all data collected up to the point of this request.

    ProfilerDisplayStartDelta - Indicates that the profiler display should only
        show the deltas in data from the current time forward.

    ProfilerDisplayStopDelta - Indicates that the profiler display should stop
        only displaying the deltas in data.

--*/

typedef enum _PROFILER_DISPLAY_REQUEST {
    ProfilerDisplayInvalid,
    ProfilerDisplayOneTime,
    ProfilerDisplayOneTimeThreshold,
    ProfilerDisplayStart,
    ProfilerDisplayStop,
    ProfilerDisplayClear,
    ProfilerDisplayStartDelta,
    ProfilerDisplayStopDelta
} PROFILER_DISPLAY_REQUEST, *PPROFILER_DISPLAY_REQUEST;

/*++

Structure Description:

    This structure defines call stack data used for profiler tracing.

Members:

    SiblingEntry - Stores the element's entry into its list of siblings.

    Children - Stores a list of the element's child stack entries.

    Parent - Stores a pointer to the parent stack data entry.

    Address - Stores the current address for this stack entry.

    AddressSymbol - Stores a string representation of the stack entry's address.

    Count - Stores the number of times that the address has been encountered.

--*/

typedef struct _STACK_DATA_ENTRY STACK_DATA_ENTRY, *PSTACK_DATA_ENTRY;
struct _STACK_DATA_ENTRY {
    LIST_ENTRY SiblingEntry;
    LIST_ENTRY Children;
    PSTACK_DATA_ENTRY Parent;
    ULONGLONG Address;
    PSTR AddressSymbol;
    ULONG Count;
    HANDLE UiHandle;
};

/*++

Structure Description:

    This structure defines memory pool data use for profiler tracing.

Members:

    ListEntry - Stores pointers to the next and previous memory pool entries.

    MemoryPool - Stores information on this memory pool.

    TagStatistics - Stores an array of pool tag information.

--*/

typedef struct _MEMORY_POOL_ENTRY {
    LIST_ENTRY ListEntry;
    PROFILER_MEMORY_POOL MemoryPool;
    PROFILER_MEMORY_POOL_TAG_STATISTIC *TagStatistics;
} MEMORY_POOL_ENTRY, *PMEMORY_POOL_ENTRY;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

