/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dbgprofp.h

Abstract:

    This header contains internal definitions for the debugger's profiling
    support.

Author:

    Evan Green 14-Sep-2013

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

Structure Description:

    This structure defines a profiler data list entry. It stores the data sent
    by the profiler.

Members:

    ListEntry - Stores pointers to the next and previous data packets in the
        list of profiler data.

    Processor - Stores the processor number this data came in from.

    Flags - Stores a set of flags describing the properties of this data entry.

    Offset - Stores the consumer's current offset into the data for
        convenience.

    DataSize - Stores the size of the data array, in bytes.

    Data - Stores an array of profiler data.

--*/

typedef struct _PROFILER_DATA_ENTRY {
    LIST_ENTRY ListEntry;
    ULONG Processor;
    ULONG Flags;
    ULONG Offset;
    ULONG DataSize;
    BYTE *Data;
} PROFILER_DATA_ENTRY, *PPROFILER_DATA_ENTRY;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

//
// Common functions
//

VOID
DbgrpDestroyProfilerDataList (
    PLIST_ENTRY ListHead
    );

/*++

Routine Description:

    This routine destroys a profiler data list. It does not destroy the head
    of the list.

Arguments:

    ListHead - Supplies a pointer to the head of the list that is to be
        destroyed.

Return Value:

    None.

--*/

//
// Thread profiling functions
//

INT
DbgrpInitializeThreadProfiling (
    PDEBUGGER_CONTEXT Context
    );

/*++

Routine Description:

    This routine initializes support for thread profiling.

Arguments:

    Context - Supplies a pointer to the debugger context.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

VOID
DbgrpDestroyThreadProfiling (
    PDEBUGGER_CONTEXT Context
    );

/*++

Routine Description:

    This routine destroys any structures used for thread profiling.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

VOID
DbgrpProcessThreadProfilingData (
    PDEBUGGER_CONTEXT Context,
    PPROFILER_DATA_ENTRY ProfilerData
    );

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

INT
DbgrpDispatchThreadProfilerCommand (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

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

