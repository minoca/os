/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dbgrprof.c

Abstract:

    This module implements support for monitoring the debuggee's profiling.

Author:

    Chris Stevens 11-Jul-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "dbgrtl.h"
#include <minoca/debug/spproto.h>
#include <minoca/lib/im.h>
#include <minoca/debug/dbgext.h>
#include "symbols.h"
#include "dbgapi.h"
#include "dbgsym.h"
#include "dbgrprof.h"
#include "dbgprofp.h"
#include "dbgrcomm.h"
#include "console.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Defines the indent length (in characters) of each level of the console
// output of profiler stack data.
//

#define PROFILER_STACK_INDENT_LENGTH 2

//
// Define flags for profiler data entries.
//

#define PROFILER_DATA_FLAGS_MEMORY_SENTINEL 0x1

#define PROFILER_USAGE                                                         \
    "Usage: profiler <type> [options...]\n"                                    \
    "Valid Types:\n"                                                           \
    "  stack  - Samples the execution call stack at a regular interval.\n"     \
    "  memory - Displays kernel memory pool data.\n"                           \
    "  thread - Displays kernel thread information.\n"                         \
    "  help   - Display this help.\n"                                          \
    "Try 'profiler <type> help' for help with a specific profiling type.\n"    \
    "Note that profiling must be activated on the target for data to be \n"    \
    "received.\n\n"

#define STACK_PROFILER_USAGE                                                   \
    "Usage: profiler stack <command> [options...]\n"                           \
    "This command works with periodic stack trace data sent from the target.\n"\
    "Valid commands are:\n"                                                    \
    "  start - Begin displaying stack profiling data in the UI. Note that \n"  \
    "          stack-based profiling must be activated in the target.\n"       \
    "  stop  - Stop displaying stack profiling data in the UI. If profiling \n"\
    "          is still activated in the target then data collection will \n"  \
    "          continue to occur.\n"                                           \
    "  clear - Delete all historical data stored in the debugger.\n"           \
    "  dump  - Write the stack profiling data out to the debugger command \n"  \
    "          console.\n"                                                     \
    "  threshold <percentage> - Set the threshold as a percentage of total \n" \
    "          hits that a stack entry must achieve to be printed out in \n"   \
    "          the dump. This is useful for limiting results to only those \n" \
    "          that dominate the sampling.\n"                                  \
    "  help  - Display this help.\n\n"

#define MEMORY_PROFILER_USAGE                                                  \
    "Usage: profiler memory <command> [options...]\n"                          \
    "This command works with memory statistics sent periodically from the \n"  \
    "target. Valid commands are:\n"                                            \
    "  start - Begin displaying memory profiling data in the UI. Note that \n" \
    "          memory profiling must be activated in the target as well.\n"    \
    "  delta - Begin displaying memory profiling data in the UI as a\n"        \
    "          difference from the current snap of memory information. \n"     \
    "          Values that are not different from the current snap will \n"    \
    "          not be displayed.\n"                                            \
    "  stop  - Stop displaying memory profiling data in the UI. Data may \n"   \
    "          still be collected if activated in the target.\n"               \
    "  clear - Delete all historical data stored in the debugger.\n"           \
    "  dump  - Write the memory profiling data out to the debugger command \n" \
    "          console.\n"                                                     \
    "  threshold <activecount> - Set the minimum threshold of active\n"        \
    "          allocations that must be reached for an allocation to be\n"     \
    "          displayed. This is useful for weeding out unimportant data.\n"  \

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PSTACK_DATA_ENTRY
DbgrpCreateStackEntry (
    PDEBUGGER_CONTEXT Context,
    PSTACK_DATA_ENTRY Parent,
    ULONGLONG Address
    );

VOID
DbgrpInsertStackData (
    PSTACK_DATA_ENTRY Parent,
    PSTACK_DATA_ENTRY Child
    );

VOID
DbgrpPrintProfilerStackData (
    PSTACK_DATA_ENTRY Root,
    ULONG Threshold
    );

PMEMORY_POOL_ENTRY
DbgrpGetMemoryPoolEntry (
    PLIST_ENTRY PoolListHead,
    PROFILER_MEMORY_TYPE ProfilerMemoryType
    );

PPROFILER_MEMORY_POOL_TAG_STATISTIC
DbgrpGetTagStatistics (
    PMEMORY_POOL_ENTRY MemoryPoolEntry,
    ULONG Tag
    );

INT
DbgrpDispatchStackProfilerCommand (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

INT
DbgrpDispatchMemoryProfilerCommand (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a global containing the debugger context, assumed to be singular. This
// is pretty awful and goes against the idea of the debugger context, but
// many of these functions are called back from UI window procedures and timer
// callbacks.
//

PDEBUGGER_CONTEXT DbgrProfilerGlobalContext;

//
// ------------------------------------------------------------------ Functions
//

INT
DbgrProfilerInitialize (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine initializes the debugger for profiler data consumption.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    INT Result;

    DbgrProfilerGlobalContext = Context;
    Context->ProfilingData.StackListLock = CreateDebuggerLock();
    if (Context->ProfilingData.StackListLock == NULL) {
        return ENOMEM;
    }

    Context->ProfilingData.MemoryListLock = CreateDebuggerLock();
    if (Context->ProfilingData.MemoryListLock == NULL) {
        return ENOMEM;
    }

    Result = DbgrpInitializeThreadProfiling(Context);
    if (Result != 0) {
        return Result;
    }

    INITIALIZE_LIST_HEAD(&(Context->ProfilingData.StackListHead));
    INITIALIZE_LIST_HEAD(&(Context->ProfilingData.MemoryListHead));
    Context->ProfilingData.MemoryCollectionActive = FALSE;
    return 0;
}

VOID
DbgrProfilerDestroy (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys any structures used to consume profiler data.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    if (Context->ProfilingData.StackListLock != NULL) {
        AcquireDebuggerLock(Context->ProfilingData.StackListLock);
        DbgrpDestroyProfilerDataList(&(Context->ProfilingData.StackListHead));
        ReleaseDebuggerLock(Context->ProfilingData.StackListLock);
        DestroyDebuggerLock(Context->ProfilingData.StackListLock);
    }

    if (Context->ProfilingData.MemoryListLock != NULL) {
        AcquireDebuggerLock(Context->ProfilingData.MemoryListLock);
        DbgrpDestroyProfilerDataList(&(Context->ProfilingData.MemoryListHead));
        ReleaseDebuggerLock(Context->ProfilingData.MemoryListLock);
        DestroyDebuggerLock(Context->ProfilingData.MemoryListLock);
    }

    DbgrpDestroyThreadProfiling(Context);
    DbgrDestroyProfilerStackData(Context->ProfilingData.CommandLineStackRoot);
    DbgrDestroyProfilerMemoryData(
                               Context->ProfilingData.CommandLinePoolListHead);

    DbgrDestroyProfilerMemoryData(
                               Context->ProfilingData.CommandLineBaseListHead);

    return;
}

VOID
DbgrProcessProfilerNotification (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine processes a profiler notification that the debuggee sends to
    the debugger. The routine should collect the profiler data and return as
    quickly as possible.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    PPROFILER_DATA_ENTRY ProfilerData;
    PPROFILER_NOTIFICATION ProfilerNotification;
    BOOL Result;

    ProfilerData = NULL;

    //
    // Get the profiler notification data out of the current event.
    //

    ProfilerNotification = Context->CurrentEvent.ProfilerNotification;

    //
    // If the end packet was received, denoted by the max profiler type, then
    // close out this round of data collection.
    //

    if (ProfilerNotification->Header.Type >= ProfilerDataTypeMax) {
        AcquireDebuggerLock(Context->ProfilingData.MemoryListLock);

        //
        // Put the sentinel on the last entry if memory profiling is active.
        //

        if (Context->ProfilingData.MemoryCollectionActive != FALSE) {

            ASSERT(LIST_EMPTY(&(Context->ProfilingData.MemoryListHead)) ==
                                                                        FALSE);

            ProfilerData = LIST_VALUE(
                                Context->ProfilingData.MemoryListHead.Previous,
                                PROFILER_DATA_ENTRY,
                                ListEntry);

            ProfilerData->Flags |= PROFILER_DATA_FLAGS_MEMORY_SENTINEL;
            Context->ProfilingData.MemoryCollectionActive = FALSE;
        }

        ReleaseDebuggerLock(Context->ProfilingData.MemoryListLock);
        Result = TRUE;
        goto ProcessProfilerNotificationEnd;
    }

    //
    // This is a valid profiler data type. Create a profiler data list element
    // and copy this notification's data into the element.
    //

    ProfilerData = malloc(sizeof(PROFILER_DATA_ENTRY));
    if (ProfilerData == NULL) {
        Result = FALSE;
        goto ProcessProfilerNotificationEnd;
    }

    ProfilerData->Processor = ProfilerNotification->Header.Processor;
    ProfilerData->DataSize = ProfilerNotification->Header.DataSize;
    ProfilerData->Offset = 0;
    ProfilerData->Data = malloc(ProfilerData->DataSize);
    if (ProfilerData->Data == NULL) {
        Result = FALSE;
        goto ProcessProfilerNotificationEnd;
    }

    ProfilerData->Flags = 0;
    RtlCopyMemory(ProfilerData->Data,
                  ProfilerNotification->Data,
                  ProfilerData->DataSize);

    //
    // Insert the profile data into the correct list.
    //

    switch (ProfilerNotification->Header.Type) {
    case ProfilerDataTypeStack:

        //
        // Insert the element into the list of stack samples.
        //

        AcquireDebuggerLock(Context->ProfilingData.StackListLock);
        INSERT_BEFORE(&(ProfilerData->ListEntry),
                      &(Context->ProfilingData.StackListHead));

        ReleaseDebuggerLock(Context->ProfilingData.StackListLock);
        Result = TRUE;
        break;

    case ProfilerDataTypeMemory:

        //
        // Insert the element into the list of memory samples.
        //

        AcquireDebuggerLock(Context->ProfilingData.MemoryListLock);
        Context->ProfilingData.MemoryCollectionActive = TRUE;
        INSERT_BEFORE(&(ProfilerData->ListEntry),
                      &(Context->ProfilingData.MemoryListHead));

        ReleaseDebuggerLock(Context->ProfilingData.MemoryListLock);
        Result = TRUE;
        break;

    case ProfilerDataTypeThread:
        DbgrpProcessThreadProfilingData(Context, ProfilerData);
        Result = TRUE;
        break;

    default:
        DbgOut("Error: Unknown profiler notification type %d.\n",
               ProfilerNotification->Header.Type);

        Result = FALSE;
        break;
    }

ProcessProfilerNotificationEnd:
    if (Result == FALSE) {
        if (ProfilerData != NULL) {
            free(ProfilerData);
        }
    }

    return;
}

INT
DbgrDispatchProfilerCommand (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine handles a profiler command.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments.

    ArgumentCount - Supplies the number of arguments in the Arguments array.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    INT Result;

    //
    // Currently the profiler supports only one debug context.
    //

    assert(Context == DbgrProfilerGlobalContext);

    if (ArgumentCount < 1) {
        DbgOut(PROFILER_USAGE);
        return EINVAL;
    }

    if (strcasecmp(Arguments[0], "stack") == 0) {
        Result = DbgrpDispatchStackProfilerCommand(Context,
                                                   Arguments,
                                                   ArgumentCount);

    } else if (strcasecmp(Arguments[0], "memory") == 0) {
        Result = DbgrpDispatchMemoryProfilerCommand(Context,
                                                    Arguments,
                                                    ArgumentCount);

    } else if (strcasecmp(Arguments[0], "thread") == 0) {
        Result = DbgrpDispatchThreadProfilerCommand(Context,
                                                    Arguments,
                                                    ArgumentCount);

    } else if (strcasecmp(Arguments[0], "help") == 0) {
        DbgOut(PROFILER_USAGE);
        Result = 0;

    } else {
        DbgOut("Error: Invalid profiler type '%s'.\n\n", Arguments[0]);
        DbgOut(PROFILER_USAGE);
        Result = EINVAL;
    }

    return Result;
}

VOID
DbgrDisplayCommandLineProfilerData (
    PROFILER_DATA_TYPE DataType,
    PROFILER_DISPLAY_REQUEST DisplayRequest,
    ULONG Threshold
    )

/*++

Routine Description:

    This routine displays the profiler data collected by the core debugging
    infrastructure to standard out.

Arguments:

    DataType - Supplies the type of profiler data that is to be displayed.

    DisplayRequest - Supplies a value requesting a display action, which can
        either be to display data once, continually, or to stop continually
        displaying data.

    Threshold - Supplies the minimum percentage a stack entry hit must be in
        order to be displayed.

Return Value:

    None.

--*/

{

    PDEBUGGER_CONTEXT Context;
    BOOL DeltaMode;
    PLIST_ENTRY PoolListHead;
    PDEBUGGER_PROFILING_DATA ProfilingData;
    BOOL Result;

    Context = DbgrProfilerGlobalContext;
    ProfilingData = &(Context->ProfilingData);
    switch (DisplayRequest) {
    case ProfilerDisplayOneTime:
    case ProfilerDisplayOneTimeThreshold:

        //
        // Display the profiler stack data once if there is any.
        //

        switch (DataType) {
        case ProfilerDataTypeStack:
            Result = DbgrGetProfilerStackData(
                                       &(ProfilingData->CommandLineStackRoot));

            if (Result == FALSE) {
                DbgOut("Error: There is no valid stack data to display.\n");
                return;
            }

            DbgrPrintProfilerStackData(ProfilingData->CommandLineStackRoot,
                                       Threshold);

            break;

        //
        // Display the profiler memory data if there is any.
        //

        case ProfilerDataTypeMemory:
            Result = DbgrGetProfilerMemoryData(&PoolListHead);
            if ((Result == FALSE) &&
                (ProfilingData->CommandLinePoolListHead == NULL)) {

                DbgOut("Error: There is no valid memory data to display.\n");
                return;
            }

            //
            // Always save the latest valid list in case there is no new data
            // for the next call.
            //

            if (Result != FALSE) {
                if (ProfilingData->CommandLinePoolListHead !=
                    ProfilingData->CommandLineBaseListHead) {

                    DbgrDestroyProfilerMemoryData(
                                       ProfilingData->CommandLinePoolListHead);
                }

                ProfilingData->CommandLinePoolListHead = PoolListHead;
            }

            //
            // Try to subtract the base line statistics and determine if delta
            // mode is enabled.
            //

            PoolListHead = DbgrSubtractMemoryStatistics(
                                ProfilingData->CommandLinePoolListHead,
                                ProfilingData->CommandLineBaseListHead);

            if (PoolListHead != ProfilingData->CommandLinePoolListHead) {
                DeltaMode = TRUE;

            } else {
                DeltaMode = FALSE;
            }

            //
            // Print the statistics to the console and destroy the temporary
            // list if a delta was displayed.
            //

            DbgrPrintProfilerMemoryData(PoolListHead, DeltaMode, Threshold);
            if (DeltaMode != FALSE) {
                DbgrDestroyProfilerMemoryData(PoolListHead);
            }

            break;

        default:
            DbgOut("Error: invalid profiler type %d.\n", DataType);
            break;
        }

        break;

    case ProfilerDisplayClear:
        switch (DataType) {
        case ProfilerDataTypeStack:
            DbgrDestroyProfilerStackData(ProfilingData->CommandLineStackRoot);
            Context->ProfilingData.CommandLineStackRoot = NULL;
            break;

        default:
            DbgOut("Error: invalid profiler type %d for the 'clear' command.\n",
                   DataType);

            break;
        }

        break;

    case ProfilerDisplayStartDelta:
        switch (DataType) {

        //
        // Establish a base memory record to start delta mode.
        //

        case ProfilerDataTypeMemory:

            //
            // Use the most recent memory pool statistics if available.
            //

            DbgrDestroyProfilerMemoryData(
                                       ProfilingData->CommandLineBaseListHead);

            if (ProfilingData->CommandLinePoolListHead != NULL) {
                ProfilingData->CommandLineBaseListHead =
                                        ProfilingData->CommandLinePoolListHead;

                Result = TRUE;

            //
            // If they are not available, then query for new statistics.
            //

            } else {
                Result = DbgrGetProfilerMemoryData(&PoolListHead);
                if (Result == FALSE) {
                    ProfilingData->CommandLineBaseListHead = NULL;
                    DbgOut("There is no memory data available to establish a "
                           "baseline for delta mode.\n");

                } else {
                    ProfilingData->CommandLineBaseListHead = PoolListHead;
                    ProfilingData->CommandLinePoolListHead = PoolListHead;
                }
            }

            if (Result != FALSE) {
                DbgOut("Memory profiler delta mode enabled.\n");
            }

            break;

        default:
            DbgOut("Error: invalid profiler type %d for the 'delta' command.\n",
                   DataType);

            break;
        }

        break;

    case ProfilerDisplayStopDelta:
        switch (DataType) {

        //
        // Remove the record of a memory base to stop delta mode.
        //

        case ProfilerDataTypeMemory:
            if (ProfilingData->CommandLineBaseListHead !=
                ProfilingData->CommandLinePoolListHead) {

                DbgrDestroyProfilerMemoryData(
                                       ProfilingData->CommandLineBaseListHead);
            }

            ProfilingData->CommandLineBaseListHead = NULL;
            DbgOut("Memory profiler delta mode disabled.\n");
            break;

        default:
            DbgOut("Error: invalid profiler type %d for the 'delta' command.\n",
                   DataType);

            break;
        }

        break;

    default:
        DbgOut("Error: Invalid profiler display request %d.\n", DisplayRequest);
        break;
    }

    return;
}

BOOL
DbgrGetProfilerStackData (
    PSTACK_DATA_ENTRY *StackTreeRoot
    )

/*++

Routine Description:

    This routine processes and returns any pending profiling stack data. It
    will add it to the provided stack tree root. The caller is responsible for
    destroying the tree.

Arguments:

    StackTreeRoot - Supplies a pointer to a pointer to the root of the stack
        data tree. Upon return from the routine it will be updated with all the
        newly parsed data. If the root is null, a new root will be allocated.

Return Value:

    Returns TRUE when data is successfully returned, or FALSE on failure.

--*/

{

    ULONGLONG Address;
    PSTACK_DATA_ENTRY AllocatedRoot;
    PDEBUGGER_CONTEXT Context;
    PSTACK_DATA_ENTRY CurrentEntry;
    ULONG CurrentStackLength;
    PLIST_ENTRY DataEntry;
    ULONG Index;
    ULONGLONG Offset;
    PSTACK_DATA_ENTRY Parent;
    ULONG PointerSize;
    PPROFILER_DATA_ENTRY ProfilerData;
    BOOL Result;
    PSTACK_DATA_ENTRY Root;
    ULONG RoutineCount;
    PSTACK_DATA_ENTRY StackData;
    PLIST_ENTRY StackEntry;
    ULONG StackLength;
    LIST_ENTRY StackListHead;

    assert(StackTreeRoot != NULL);

    Context = DbgrProfilerGlobalContext;
    AllocatedRoot = NULL;
    Result = FALSE;
    INITIALIZE_LIST_HEAD(&StackListHead);

    //
    // If the tree root is NULL, create a new one for the caller.
    //

    if (*StackTreeRoot == NULL) {
        AllocatedRoot = DbgrpCreateStackEntry(DbgrProfilerGlobalContext,
                                              NULL,
                                              0);

        if (AllocatedRoot == NULL) {
            Result = FALSE;
            goto GetProfilerStackDataEnd;
        }

        *StackTreeRoot = AllocatedRoot;
    }

    Root = *StackTreeRoot;

    //
    // Acquire the profiler lock and copy the head of the stack data list, fix
    // up the pointers, and empty the global list. If the list is currently
    // empty, just exit.
    //

    AcquireDebuggerLock(Context->ProfilingData.StackListLock);
    if (LIST_EMPTY(&(Context->ProfilingData.StackListHead)) != FALSE) {
        Result = TRUE;
        ReleaseDebuggerLock(Context->ProfilingData.StackListLock);
        goto GetProfilerStackDataEnd;
    }

    RtlCopyMemory(&StackListHead,
                  &(Context->ProfilingData.StackListHead),
                  sizeof(LIST_ENTRY));

    StackListHead.Next->Previous = &StackListHead;
    StackListHead.Previous->Next = &StackListHead;
    INITIALIZE_LIST_HEAD(&(Context->ProfilingData.StackListHead));
    ReleaseDebuggerLock(Context->ProfilingData.StackListLock);

    //
    // Loop through each profiler stack data packet in the list, adding its
    // stack entries to the tree of stack data.
    //

    PointerSize = DbgGetTargetPointerSize(Context);
    DataEntry = StackListHead.Next;
    while (DataEntry != &StackListHead) {
        ProfilerData = LIST_VALUE(DataEntry, PROFILER_DATA_ENTRY, ListEntry);
        RoutineCount = ProfilerData->DataSize / PointerSize;
        if ((ProfilerData->DataSize % PointerSize) != 0) {
            DbgOut("Bad profiler data size %d.\n", ProfilerData->DataSize);
            Result = FALSE;
            goto GetProfilerStackDataEnd;
        }

        //
        // Run through the data array backwards to parse each stack from the
        // root routine.
        //

        Parent = Root;
        CurrentStackLength = 0;
        for (Index = RoutineCount; Index > 0; Index -= 1) {
            Address = 0;
            Offset = (Index - 1) * PointerSize;
            RtlCopyMemory(&Address, &(ProfilerData->Data[Offset]), PointerSize);

            //
            // Every sentinel encountered means that a call stack was
            // completely processed.
            //

            if (IS_PROFILER_DATA_SENTINEL(Address) != FALSE) {

                //
                // Validate that this was a complete stack. The stack size
                // stored in the sentinel marker includes the size of the
                // sentinel.
                //

                StackLength = GET_PROFILER_DATA_SIZE(Address) / PointerSize;
                if (CurrentStackLength != (StackLength - 1)) {
                    DbgOut("Error: Profiler collected incomplete call "
                           "stack.\n");

                    Result = FALSE;
                    goto GetProfilerStackDataEnd;
                }

                CurrentStackLength = 0;
                Root->Count += 1;
                Parent = Root;
                continue;
            }

            //
            // Look up the call site in the parent's list of children.
            //

            CurrentEntry = NULL;
            StackEntry = Parent->Children.Next;
            while (StackEntry != &(Parent->Children)) {
                StackData = LIST_VALUE(StackEntry,
                                       STACK_DATA_ENTRY,
                                       SiblingEntry);

                if (StackData->Address == Address) {
                    CurrentEntry = StackData;
                    break;
                }

                StackEntry = StackEntry->Next;
            }

            //
            // If there was no match, create a new entry. If this fails,
            // just exit returning failure.
            //

            if (CurrentEntry == NULL) {
                CurrentEntry = DbgrpCreateStackEntry(DbgrProfilerGlobalContext,
                                                     Parent,
                                                     Address);

                if (CurrentEntry == NULL) {
                    DbgOut("Error: Failed to create stack entry.\n");
                    Result = FALSE;
                    goto GetProfilerStackDataEnd;
                }
            }

            //
            // Account for this match on the current entry, remove it, and
            // then insert it back into the stack in order.
            //

            CurrentEntry->Count += 1;
            LIST_REMOVE(&(CurrentEntry->SiblingEntry));
            DbgrpInsertStackData(Parent, CurrentEntry);

            //
            // Move down the stack.
            //

            Parent = CurrentEntry;
            CurrentStackLength += 1;
        }

        //
        // Move on to the next entry.
        //

        DataEntry = DataEntry->Next;
    }

    Result = TRUE;

GetProfilerStackDataEnd:
    DbgrpDestroyProfilerDataList(&StackListHead);

    //
    // If the routine failed and allocated the root, destroy the tree.
    //

    if (Result == FALSE) {
        if (AllocatedRoot != NULL) {
            DbgrDestroyProfilerStackData(AllocatedRoot);
            if (*StackTreeRoot == AllocatedRoot) {
                *StackTreeRoot = NULL;
            }
        }
    }

    return Result;
}

VOID
DbgrDestroyProfilerStackData (
    PSTACK_DATA_ENTRY Root
    )

/*++

Routine Description:

    This routine destroys a profiler stack data tree.

Arguments:

    Root - Supplies a pointer to the root element of the tree.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PSTACK_DATA_ENTRY StackData;

    if (Root == NULL) {
        return;
    }

    //
    // Recursively destroy all the children.
    //

    while (LIST_EMPTY(&(Root->Children)) == FALSE) {
        CurrentEntry = Root->Children.Next;
        StackData = LIST_VALUE(CurrentEntry, STACK_DATA_ENTRY, SiblingEntry);
        DbgrDestroyProfilerStackData(StackData);
    }

    //
    // Now destroy the current root.
    //

    if (Root->SiblingEntry.Next != NULL) {
        LIST_REMOVE(&(Root->SiblingEntry));
    }

    if (Root->AddressSymbol != NULL) {
        free(Root->AddressSymbol);
    }

    free(Root);
    return;
}

VOID
DbgrPrintProfilerStackData (
    PSTACK_DATA_ENTRY Root,
    ULONG Threshold
    )

/*++

Routine Description:

    This routine prints profiler stack data to standard out.

Arguments:

    Root - Supplies a pointer to the root of the profiler stack data tree.

    Threshold - Supplies the minimum percentage a stack entry hit must be in
        order to be displayed.

Return Value:

    None.

--*/

{

    DbgrpPrintProfilerStackData(Root, Threshold);
    return;
}

VOID
DbgrProfilerStackEntrySelected (
    PSTACK_DATA_ENTRY Root
    )

/*++

Routine Description:

    This routine is called when a profiler stack data entry is selected by the
    user.

Arguments:

    Root - Supplies a pointer to the root of the profiler stack data tree.

Return Value:

    None.

--*/

{

    //
    // If an entry is found, highlight the code line associated with the
    // selected item. This operation will remove the highlight from the
    // previously selected item.
    //

    if ((Root != NULL) && (Root->Address != 0)) {
        DbgrShowSourceAtAddress(DbgrProfilerGlobalContext, Root->Address);
    }

    return;
}

BOOL
DbgrGetProfilerMemoryData (
    PLIST_ENTRY *MemoryPoolListHead
    )

/*++

Routine Description:

    This routine processes and returns any pending profiling memory data.

Arguments:

    MemoryPoolListHead - Supplies a pointer to head of the memory pool list
        that is to be populated with the most up to date pool data.

Return Value:

    Returns TRUE when data is successfully returned, or FALSE on failure.

--*/

{

    ULONG BytesRemaining;
    PDEBUGGER_CONTEXT Context;
    BYTE *Data;
    ULONG DataSize;
    LIST_ENTRY LocalListHead;
    PLIST_ENTRY MemoryListEntry;
    LIST_ENTRY MemoryListHead;
    PMEMORY_POOL_ENTRY MemoryPoolEntry;
    PLIST_ENTRY NewPoolListHead;
    ULONG Offset;
    PPROFILER_DATA_ENTRY ProfilerData;
    BOOL Result;
    ULONG SentinelCount;
    ULONG TagCount;
    ULONG TagSize;

    Context = DbgrProfilerGlobalContext;
    Data = NULL;
    NewPoolListHead = NULL;
    INITIALIZE_LIST_HEAD(&LocalListHead);
    INITIALIZE_LIST_HEAD(&MemoryListHead);

    //
    // Allocate a new pool list head to return to the caller if successful.
    //

    NewPoolListHead = malloc(sizeof(LIST_ENTRY));
    if (NewPoolListHead == NULL) {
        Result = FALSE;
        goto GetProfilerMemoryDataEnd;
    }

    INITIALIZE_LIST_HEAD(NewPoolListHead);

    //
    // Acquire the profiler memory lock and remove all complete memory data
    // packets.
    //

    AcquireDebuggerLock(Context->ProfilingData.MemoryListLock);

    //
    // Do nothing if the list is empty.
    //

    if (LIST_EMPTY(&(Context->ProfilingData.MemoryListHead)) != FALSE) {
        Result = FALSE;
        ReleaseDebuggerLock(Context->ProfilingData.MemoryListLock);
        goto GetProfilerMemoryDataEnd;
    }

    //
    // First remove all the data packets.
    //

    RtlCopyMemory(&LocalListHead,
                  &(Context->ProfilingData.MemoryListHead),
                  sizeof(LIST_ENTRY));

    LocalListHead.Next->Previous = &LocalListHead;
    LocalListHead.Previous->Next = &LocalListHead;
    INITIALIZE_LIST_HEAD(&(Context->ProfilingData.MemoryListHead));

    //
    // Now run backwards through the local list, copying packets back to the
    // global list until the first sentinel is encountered.
    //

    while (LIST_EMPTY(&LocalListHead) == FALSE) {
        MemoryListEntry = LocalListHead.Previous;
        ProfilerData = LIST_VALUE(MemoryListEntry,
                                  PROFILER_DATA_ENTRY,
                                  ListEntry);

        if ((ProfilerData->Flags & PROFILER_DATA_FLAGS_MEMORY_SENTINEL) != 0) {
            break;
        }

        LIST_REMOVE(MemoryListEntry);
        INSERT_AFTER(MemoryListEntry, &(Context->ProfilingData.MemoryListHead));
    }

    ReleaseDebuggerLock(Context->ProfilingData.MemoryListLock);
    Result = TRUE;

    //
    // If this list is empty, just leave.
    //

    if (LIST_EMPTY(&LocalListHead) != FALSE) {
        Result = FALSE;
        goto GetProfilerMemoryDataEnd;
    }

    //
    // Only the most recent memory data is interesting, so out of the list of
    // completed memory snapshots, find the start of the last one.
    //

    SentinelCount = 0;
    INITIALIZE_LIST_HEAD(&MemoryListHead);
    while (LIST_EMPTY(&LocalListHead) == FALSE) {
        MemoryListEntry = LocalListHead.Previous;
        ProfilerData = LIST_VALUE(MemoryListEntry,
                                  PROFILER_DATA_ENTRY,
                                  ListEntry);

        if ((ProfilerData->Flags & PROFILER_DATA_FLAGS_MEMORY_SENTINEL) != 0) {
            SentinelCount += 1;
            if (SentinelCount > 1) {
                break;
            }
        }

        LIST_REMOVE(MemoryListEntry);
        INSERT_AFTER(MemoryListEntry, &MemoryListHead);
    }

    //
    // Release the outdated information.
    //

    DbgrpDestroyProfilerDataList(&LocalListHead);

    //
    // Now package the data into what the debugger UI consoles expect. Start
    // by pulling all the data into one buffer, it may have been awkwardly
    // split across packets.
    //

    DataSize = 0;
    MemoryListEntry = MemoryListHead.Next;
    while (MemoryListEntry != &MemoryListHead) {
        ProfilerData = LIST_VALUE(MemoryListEntry,
                                  PROFILER_DATA_ENTRY,
                                  ListEntry);

        DataSize += ProfilerData->DataSize;
        MemoryListEntry = MemoryListEntry->Next;
    }

    Data = malloc(DataSize);
    if (Data == NULL) {
        DbgOut("Error: failed to allocate %d bytes for the memory profiler.\n",
               DataSize);

        Result = FALSE;
        goto GetProfilerMemoryDataEnd;
    }

    Offset = 0;
    MemoryListEntry = MemoryListHead.Next;
    while (MemoryListEntry != &MemoryListHead) {
        ProfilerData = LIST_VALUE(MemoryListEntry,
                                  PROFILER_DATA_ENTRY,
                                  ListEntry);

        RtlCopyMemory(&(Data[Offset]),
                      ProfilerData->Data,
                      ProfilerData->DataSize);

        Offset += ProfilerData->DataSize;
        MemoryListEntry = MemoryListEntry->Next;
    }

    //
    // With all the data copied, destroy the list.
    //

    DbgrpDestroyProfilerDataList(&MemoryListHead);

    //
    // Now read through the data buffer, translating the byte segments into the
    // appropriate structures.
    //

    Offset = 0;
    BytesRemaining = DataSize;
    while (BytesRemaining != 0) {
        if (BytesRemaining < sizeof(PROFILER_MEMORY_POOL)) {
            DbgOut("Error: invalid memory pool data.\n");
            Result = FALSE;
            goto GetProfilerMemoryDataEnd;
        }

        //
        // Allocate a pool entry in anticipation of a valid data buffer.
        //

        MemoryPoolEntry = malloc(sizeof(MEMORY_POOL_ENTRY));
        if (MemoryPoolEntry == NULL) {
            DbgOut("Error: failed to allocate %d bytes for a memory pool "
                   "entry.\n",
                   sizeof(MEMORY_POOL_ENTRY));

            Result = FALSE;
            goto GetProfilerMemoryDataEnd;
        }

        //
        // Copy the memory into the pool entry.
        //

        RtlCopyMemory(&(MemoryPoolEntry->MemoryPool),
                      &(Data[Offset]),
                      sizeof(PROFILER_MEMORY_POOL));

        Offset += sizeof(PROFILER_MEMORY_POOL);
        BytesRemaining -= sizeof(PROFILER_MEMORY_POOL);

        //
        // If this is not a pool header, then exit.
        //

        if (MemoryPoolEntry->MemoryPool.Magic != PROFILER_POOL_MAGIC) {
            DbgOut("Error: found 0x%08x when expected pool magic 0x%08x.\n",
                   MemoryPoolEntry->MemoryPool.Magic,
                   PROFILER_POOL_MAGIC);

            Result = FALSE;
            free(MemoryPoolEntry);
            goto GetProfilerMemoryDataEnd;
        }

        //
        // Determine the number of tag statistics in this pool and whether or
        // not the data buffer is big enough to hold the expected tag data. If
        // not, then exit.
        //

        TagCount = MemoryPoolEntry->MemoryPool.TagCount;
        TagSize = TagCount * sizeof(PROFILER_MEMORY_POOL_TAG_STATISTIC);
        if (BytesRemaining < TagSize) {
            DbgOut("Error: unexpected end of memory data buffer. %d bytes "
                   "remaining when expected %d bytes.\n",
                   BytesRemaining,
                   TagSize);

            Result = FALSE;
            free(MemoryPoolEntry);
            goto GetProfilerMemoryDataEnd;
        }

        //
        // Allocate an array for the tag statistics.
        //

        MemoryPoolEntry->TagStatistics = malloc(TagSize);
        if (MemoryPoolEntry->TagStatistics == NULL) {
            DbgOut("Error: failed to allocate %d bytes for a memory pool "
                   "tag statistics.\n",
                   TagSize);

            Result = FALSE;
            free(MemoryPoolEntry);
            goto GetProfilerMemoryDataEnd;
        }

        //
        // Copy the tag statistics.
        //

        RtlCopyMemory(MemoryPoolEntry->TagStatistics, &(Data[Offset]), TagSize);
        Offset += TagSize;
        BytesRemaining -= TagSize;

        //
        // Insert this complete pool data into the supplied list head.
        //

        INSERT_BEFORE(&(MemoryPoolEntry->ListEntry), NewPoolListHead);
    }

    *MemoryPoolListHead = NewPoolListHead;
    Result = TRUE;

GetProfilerMemoryDataEnd:
    if (Result == FALSE) {
        DbgrpDestroyProfilerDataList(&MemoryListHead);
        DbgrpDestroyProfilerDataList(&LocalListHead);
        if (Data != NULL) {
            free(Data);
        }

        if (NewPoolListHead != NULL) {
            DbgrDestroyProfilerMemoryData(NewPoolListHead);
        }
    }

    return Result;
}

VOID
DbgrDestroyProfilerMemoryData (
    PLIST_ENTRY PoolListHead
    )

/*++

Routine Description:

    This routine destroys a profiler memory data list.

Arguments:

    PoolListHead - Supplies a pointer to the head of the memory pool list
        that is to be destroyed.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PMEMORY_POOL_ENTRY MemoryPoolEntry;

    if (PoolListHead == NULL) {
        return;
    }

    //
    // Destroy each element in the list.
    //

    while (LIST_EMPTY(PoolListHead) == FALSE) {
        CurrentEntry = PoolListHead->Next;
        MemoryPoolEntry = LIST_VALUE(CurrentEntry,
                                     MEMORY_POOL_ENTRY,
                                     ListEntry);

        LIST_REMOVE(CurrentEntry);
        if (MemoryPoolEntry->TagStatistics != NULL) {
            free(MemoryPoolEntry->TagStatistics);
        }

        free(MemoryPoolEntry);
    }

    free(PoolListHead);
    return;
}

VOID
DbgrPrintProfilerMemoryData (
    PLIST_ENTRY MemoryPoolListHead,
    BOOL DeltaMode,
    ULONG ActiveCountThreshold
    )

/*++

Routine Description:

    This routine prints the statistics from the given memory pool list to the
    debugger console.

Arguments:

    MemoryPoolListHead - Supplies a pointer to the head of the memory pool
        list.

    DeltaMode - Supplies a boolean indicating whether or not the memory pool
        list represents a delta from a previous point in time.

    ActiveCountThreshold - Supplies the active count threshold. No statistics
        will be displayed for tags with an active count less than this
        threshold.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    LONG DeltaAllocationCount;
    LONG DeltaThreshold;
    ULONG FreePercentage;
    ULONG Index;
    PPROFILER_MEMORY_POOL Pool;
    PMEMORY_POOL_ENTRY PoolEntry;
    PPROFILER_MEMORY_POOL_TAG_STATISTIC Statistic;

    CurrentEntry = MemoryPoolListHead->Next;
    while (CurrentEntry != MemoryPoolListHead) {
        PoolEntry = LIST_VALUE(CurrentEntry, MEMORY_POOL_ENTRY, ListEntry);
        Pool = &(PoolEntry->MemoryPool);

        //
        // Print the pool statistics.
        //

        if (Pool->TotalPoolSize != 0) {
            FreePercentage = Pool->FreeListSize * 100 / Pool->TotalPoolSize;
            DbgOut("Pool Type %d, Size %I64xh, %d%% free, "
                   "%I64d allocation calls, %I64d free calls, %I64d failed.\n",
                   Pool->ProfilerMemoryType,
                   Pool->TotalPoolSize,
                   FreePercentage,
                   Pool->TotalAllocationCalls,
                   Pool->TotalFreeCalls,
                   Pool->FailedAllocations);

        } else {

            ASSERT(Pool->FreeListSize == 0);
            ASSERT(DeltaMode != FALSE);

            DbgOut("Pool Type %d, Size -, -%% free, "
                   "%I64d allocation calls, %I64d free calls, %I64d failed.\n",
                   Pool->ProfilerMemoryType,
                   Pool->TotalAllocationCalls,
                   Pool->TotalFreeCalls,
                   Pool->FailedAllocations);
        }

        DbgOut("------------------------------------------------------------"
               "----------------------------\n"
               "       Largest                                       Active "
               "Max Active\n"
               "Tag      Alloc      Active Bytes  Max Active Bytes    Count "
               "     Count    Lifetime Alloc\n"
               "------------------------------------------------------------"
               "----------------------------\n");

        //
        // Loop through the tags in the pool, printing statistics for each.
        //

        for (Index = 0; Index < Pool->TagCount; Index += 1) {
            Statistic = &(PoolEntry->TagStatistics[Index]);
            if (DeltaMode == FALSE) {

                //
                // Skip statistics that are below the active count threshold.
                //

                if (Statistic->ActiveAllocationCount < ActiveCountThreshold) {
                    continue;
                }

                DbgOut("%c%c%c%c %8xh %16I64xh %16I64xh %8d   %8d %16I64xh\n",
                       (UCHAR)(Statistic->Tag),
                       (UCHAR)(Statistic->Tag >> 8),
                       (UCHAR)(Statistic->Tag >> 16),
                       (UCHAR)(Statistic->Tag >> 24),
                       Statistic->LargestAllocation,
                       Statistic->ActiveSize,
                       Statistic->LargestActiveSize,
                       Statistic->ActiveAllocationCount,
                       Statistic->LargestActiveAllocationCount,
                       Statistic->LifetimeAllocationSize);

            } else {

                //
                // Honor the threshold, the absolute value of both values must
                // be obtained in delta mode.
                //

                DeltaAllocationCount = (LONG)Statistic->ActiveAllocationCount;
                DeltaThreshold = (LONG)ActiveCountThreshold;
                if (DeltaAllocationCount < 0) {
                    DeltaAllocationCount = -DeltaAllocationCount;
                }

                if (DeltaThreshold < 0) {
                    DeltaThreshold = -DeltaThreshold;
                }

                if (DeltaAllocationCount < DeltaThreshold) {
                    continue;
                }

                //
                // Only print on tags in delta mode if there is data present.
                //

                if ((Statistic->ActiveSize == 0) &&
                    (Statistic->ActiveAllocationCount == 0) &&
                    (Statistic->LifetimeAllocationSize == 0) &&
                    (Statistic->LargestAllocation == 0) &&
                    (Statistic->LargestActiveAllocationCount == 0) &&
                    (Statistic->LargestActiveSize == 0)) {

                    continue;
                }

                DbgOut("%c%c%c%c ",
                       (UCHAR)(Statistic->Tag),
                       (UCHAR)(Statistic->Tag >> 8),
                       (UCHAR)(Statistic->Tag >> 16),
                       (UCHAR)(Statistic->Tag >> 24));

                if (Statistic->LargestAllocation != 0) {
                    DbgOut("%8xh ", Statistic->LargestAllocation);

                } else {
                    DbgOut("        - ");
                }

                if (Statistic->ActiveSize != 0) {
                    DbgOut(" %16I64d ", Statistic->ActiveSize);

                } else {
                    DbgOut("                - ");
                }

                if (Statistic->LargestActiveSize != 0) {
                    DbgOut("%16I64xh ", Statistic->LargestActiveSize);

                } else {
                    DbgOut("                - ");
                }

                if (Statistic->ActiveAllocationCount != 0) {
                    DbgOut("%8d   ", Statistic->ActiveAllocationCount);

                } else {
                    DbgOut("       -   ");
                }

                if (Statistic->LargestActiveAllocationCount != 0) {
                    DbgOut("%8d ", Statistic->LargestActiveAllocationCount);

                } else {
                    DbgOut("       - ");
                }

                if (Statistic->LifetimeAllocationSize != 0) {
                    DbgOut("%16I64xh\n", Statistic->LifetimeAllocationSize);

                } else {
                    DbgOut("                -\n");
                }
            }
        }

        DbgOut("\n");
        CurrentEntry = CurrentEntry->Next;
    }

    return;
}

PLIST_ENTRY
DbgrSubtractMemoryStatistics (
    PLIST_ENTRY CurrentListHead,
    PLIST_ENTRY BaseListHead
    )

/*++

Routine Description:

    This routine subtracts the given current memory list from the base memory
    list, returning a list that contains the deltas for memory pool statistics.
    If this routine ever fails, it just returns the current list.

Arguments:

    CurrentListHead - Supplies a pointer to the head of the current list of
        memory pool data.

    BaseListHead - Supplies a pointer to the head of the base line memory list
        from which the deltas are created.

Return Value:

    Returns a new memory pool list if possible. If the routine fails or there
    is no base line, then the current memory list is returned.

--*/

{

    PPROFILER_MEMORY_POOL BaseMemoryPool;
    PMEMORY_POOL_ENTRY BaseMemoryPoolEntry;
    PPROFILER_MEMORY_POOL_TAG_STATISTIC BaseStatistic;
    PLIST_ENTRY CurrentEntry;
    BOOL DestroyNewList;
    ULONG Index;
    PPROFILER_MEMORY_POOL MemoryPool;
    PMEMORY_POOL_ENTRY MemoryPoolEntry;
    PLIST_ENTRY NewListHead;
    PPROFILER_MEMORY_POOL NewMemoryPool;
    PMEMORY_POOL_ENTRY NewMemoryPoolEntry;
    PROFILER_MEMORY_TYPE ProfilerMemoryType;
    PLIST_ENTRY ResultListHead;
    PPROFILER_MEMORY_POOL_TAG_STATISTIC Statistic;
    ULONG TagCount;
    ULONG TagSize;

    assert(CurrentListHead != NULL);

    DestroyNewList = FALSE;
    NewListHead = NULL;

    //
    // Always return the current list head unless the subtraction succeeds.
    //

    ResultListHead = CurrentListHead;

    //
    // Do nothing if the base line statistics are NULL.
    //

    if (BaseListHead == NULL) {
        goto SubtractMemoryStatisticsEnd;
    }

    //
    // Otherwise, create a new list to return the subtracted list.
    //

    NewListHead = malloc(sizeof(LIST_ENTRY));
    if (NewListHead == NULL) {
        goto SubtractMemoryStatisticsEnd;
    }

    INITIALIZE_LIST_HEAD(NewListHead);

    //
    // Loop through the current memory pool list, subtracting the value from
    // the baseline.
    //

    CurrentEntry = CurrentListHead->Next;
    while (CurrentEntry != CurrentListHead) {

        //
        // Get the memory pool entry in the current list.
        //

        MemoryPoolEntry = LIST_VALUE(CurrentEntry,
                                     MEMORY_POOL_ENTRY,
                                     ListEntry);

        CurrentEntry = CurrentEntry->Next;

        //
        // Create a new pool entry and a new memory pool.
        //

        NewMemoryPoolEntry = malloc(sizeof(MEMORY_POOL_ENTRY));
        if (NewMemoryPoolEntry == NULL) {
            DestroyNewList = TRUE;
            goto SubtractMemoryStatisticsEnd;
        }

        //
        // Allocate the array for tag statistics.
        //

        MemoryPool = &(MemoryPoolEntry->MemoryPool);
        TagCount = MemoryPool->TagCount;
        TagSize = TagCount * sizeof(PROFILER_MEMORY_POOL_TAG_STATISTIC);
        NewMemoryPoolEntry->TagStatistics = malloc(TagSize);
        if (NewMemoryPoolEntry->TagStatistics == NULL) {
            free(NewMemoryPoolEntry);
            DestroyNewList = TRUE;
            goto SubtractMemoryStatisticsEnd;
        }

        //
        // Copy the the current pool contents.
        //

        NewMemoryPool = &(NewMemoryPoolEntry->MemoryPool);
        RtlCopyMemory(NewMemoryPool, MemoryPool, sizeof(PROFILER_MEMORY_POOL));

        //
        // And the tag statistics.
        //

        RtlCopyMemory(NewMemoryPoolEntry->TagStatistics,
                      MemoryPoolEntry->TagStatistics,
                      TagSize);

        //
        // Add the new pool entry to the new list.
        //

        INSERT_BEFORE(&(NewMemoryPoolEntry->ListEntry), NewListHead);

        //
        // Find the corresponding memory pool entry in the base list. If it
        // does not exist, then this pool is brand new, don't do any
        // subtraction.
        //

        ProfilerMemoryType = MemoryPool->ProfilerMemoryType;
        BaseMemoryPoolEntry = DbgrpGetMemoryPoolEntry(BaseListHead,
                                                      ProfilerMemoryType);

        if (BaseMemoryPoolEntry == NULL) {
            continue;
        }

        //
        // Now subtract the base statistics from the new copy of the current
        // statistics.
        //

        BaseMemoryPool = &(BaseMemoryPoolEntry->MemoryPool);
        if ((NewMemoryPool->TotalPoolSize == BaseMemoryPool->TotalPoolSize) &&
            (NewMemoryPool->FreeListSize == BaseMemoryPool->FreeListSize)) {

            NewMemoryPool->TotalPoolSize = 0;
            NewMemoryPool->FreeListSize = 0;
        }

        NewMemoryPool->FailedAllocations -= BaseMemoryPool->FailedAllocations;
        NewMemoryPool->TotalFreeCalls -= BaseMemoryPool->TotalFreeCalls;
        NewMemoryPool->TotalAllocationCalls -=
                                          BaseMemoryPool->TotalAllocationCalls;

        //
        // Loop through the tag statistics and subtract the base statsitics.
        //

        for (Index = 0; Index < TagCount; Index += 1) {
            Statistic = &(NewMemoryPoolEntry->TagStatistics[Index]);

            //
            // Find the corresponding memory pool entry in the base list.
            //

            BaseStatistic = DbgrpGetTagStatistics(BaseMemoryPoolEntry,
                                                  Statistic->Tag);

            if (BaseStatistic == NULL) {
                continue;
            }

            //
            // Subtract the base statistics from the current statistics.
            //

            if (Statistic->LargestAllocation ==
                BaseStatistic->LargestAllocation) {

                Statistic->LargestAllocation = 0;
            }

            if (Statistic->LargestActiveSize ==
                BaseStatistic->LargestActiveSize) {

                Statistic->LargestActiveSize = 0;
            }

            if (Statistic->LifetimeAllocationSize ==
                BaseStatistic->LifetimeAllocationSize) {

                Statistic->LifetimeAllocationSize = 0;
            }

            if (Statistic->LargestActiveAllocationCount ==
                BaseStatistic->LargestActiveAllocationCount) {

                Statistic->LargestActiveAllocationCount = 0;
            }

            Statistic->ActiveSize -= BaseStatistic->ActiveSize;
            Statistic->ActiveAllocationCount -=
                                          BaseStatistic->ActiveAllocationCount;
        }
    }

    ResultListHead = NewListHead;

SubtractMemoryStatisticsEnd:
    if (DestroyNewList != FALSE) {
        DbgrDestroyProfilerMemoryData(NewListHead);
    }

    return ResultListHead;
}

VOID
DbgrpDestroyProfilerDataList (
    PLIST_ENTRY ListHead
    )

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

{

    PLIST_ENTRY DataEntry;
    PPROFILER_DATA_ENTRY ProfilerData;

    assert(ListHead != NULL);

    while (LIST_EMPTY(ListHead) == FALSE) {
        DataEntry = ListHead->Next;
        ProfilerData = LIST_VALUE(DataEntry, PROFILER_DATA_ENTRY, ListEntry);
        LIST_REMOVE(DataEntry);
        free(ProfilerData);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

PSTACK_DATA_ENTRY
DbgrpCreateStackEntry (
    PDEBUGGER_CONTEXT Context,
    PSTACK_DATA_ENTRY Parent,
    ULONGLONG Address
    )

/*++

Routine Description:

    This routine creates a stack entry and inserts it into the parent's list of
    children.

Arguments:

    Context - Supplies a pointer to the application context.

    Parent - Supplies a pointer to the stack entry's parent.

    Address - Supplies the call site address for this stack entry.

    ReturnAddress - Supplies the return address for this stack entry.

Return Value:

    Returns a pointer to a stack entry on success, or NULL on failure.

--*/

{

    PSTR AddressSymbol;
    BOOL Result;
    PSTACK_DATA_ENTRY StackData;

    AddressSymbol = NULL;
    Result = FALSE;

    //
    // Allocate a new stack data entry and begin filling it in.
    //

    StackData = malloc(sizeof(STACK_DATA_ENTRY));
    if (StackData == NULL) {
        DbgOut("Error: Failed to allocate %d bytes.\n",
               sizeof(STACK_DATA_ENTRY));

        goto CreateStackDataEntryEnd;
    }

    RtlZeroMemory(StackData, sizeof(STACK_DATA_ENTRY));
    INITIALIZE_LIST_HEAD(&(StackData->Children));
    INITIALIZE_LIST_HEAD(&(StackData->SiblingEntry));
    StackData->Address = Address;
    StackData->Parent = Parent;

    assert(StackData->Count == 0);
    assert(StackData->UiHandle == NULL);

    //
    // If the parent is NULL, then this is the root. Just exit.
    //

    if (Parent == NULL) {

        assert(StackData->AddressSymbol == NULL);

        Result = TRUE;
        goto CreateStackDataEntryEnd;
    }

    //
    // Get the name for the stack data entry.
    //

    AddressSymbol = DbgGetAddressSymbol(Context, Address, NULL);
    if (AddressSymbol == NULL) {
        DbgOut("Error: failed to get symbol for address 0x%I64x.\n", Address);
        goto CreateStackDataEntryEnd;
    }

    StackData->AddressSymbol = AddressSymbol;

    //
    // Insert this new stack entry into the parent's list of children in order.
    //

    if (Parent != NULL) {
        DbgrpInsertStackData(Parent, StackData);
    }

    Result = TRUE;

CreateStackDataEntryEnd:
    if (Result == FALSE) {
        if (StackData != NULL) {
            free(StackData);
            StackData = NULL;
        }

        if (AddressSymbol != NULL) {
            free(AddressSymbol);
        }
    }

    return StackData;
}

VOID
DbgrpInsertStackData (
    PSTACK_DATA_ENTRY Parent,
    PSTACK_DATA_ENTRY Child
    )

/*++

Routine Description:

    This routine inserts the child into the parent's list of children in the
    correct order.

Arguments:

    Parent - Supplies a pointer to the parent stack entry.

    Child - Supplies a pointer to the stack entry that is to be inserted.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PSTACK_DATA_ENTRY StackData;

    //
    // The list of children is already sorted, so just search for the correct
    // location to insert it. Going backwards makes this easier.
    //

    CurrentEntry = Parent->Children.Previous;
    while (CurrentEntry != &(Parent->Children)) {
        StackData = LIST_VALUE(CurrentEntry, STACK_DATA_ENTRY, SiblingEntry);
        if (Child->Count < StackData->Count) {
            INSERT_AFTER(&(Child->SiblingEntry), &(StackData->SiblingEntry));
            return;
        }

        if ((Child->Count == StackData->Count) &&
            (Child->Address > StackData->Address)) {

            INSERT_AFTER(&(Child->SiblingEntry), &(StackData->SiblingEntry));
            return;
        }

        CurrentEntry = CurrentEntry->Previous;
    }

    //
    // It was not inserted, just place it at the beginning.
    //

    INSERT_AFTER(&(Child->SiblingEntry), &(Parent->Children));
    return;
}

VOID
DbgrpPrintProfilerStackData (
    PSTACK_DATA_ENTRY Root,
    ULONG Threshold
    )

/*++

Routine Description:

    This routine prints information for the given profiler stack data entry and
    all its children to standard out.

Arguments:

    Root - Supplies a pointer to the root of the stack data tree.

    Threshold - Supplies the minimum percentage a stack entry hit must be in
        order to be displayed.

Return Value:

    None.

--*/

{

    BOOL AddVerticalBar;
    PSTACK_DATA_ENTRY ChildData;
    PSTR FunctionString;
    PSTR IndentString;
    ULONG IndentStringLength;
    PSTR NewIndentString;
    PSTACK_DATA_ENTRY NextEntry;
    ULONG NextPercent;
    ULONG Percent;
    PSTACK_DATA_ENTRY StackData;
    ULONG TotalCount;

    TotalCount = Root->Count;

    //
    // Return if the total count is zero. There is nothing to do.
    //

    if (TotalCount == 0) {
        return;
    }

    //
    // The nodes need to be displayed in depth first order where a parent is
    // displayed before any of its children. So the iteration order is to
    // process the current node, then move to either its first child, its next
    // sibling, or an anscestor's sibling. Of course, only move to nodes if
    // they meet the threshold requirements.
    //

    IndentString = malloc(sizeof(UCHAR));
    IndentStringLength = sizeof(UCHAR);
    *IndentString = '\0';
    StackData = Root;
    do {
        Percent = (StackData->Count * 100) / TotalCount;

        assert(Percent >= Threshold);

        //
        // Display the indent string.
        //

        DbgOut(IndentString);

        //
        // If this element has children, print the appropriate start symbol
        // depending on whether or not the element has children. Don't worry
        // about whether or not the children meet the threshold; either way
        // this serves as an indication that there are children.
        //

        if (LIST_EMPTY(&(StackData->Children)) == FALSE) {
            DbgOut(" +");

        } else {
            DbgOut(" -");
        }

        //
        // Print the stack entry's information.
        //

        if (StackData->AddressSymbol == NULL) {
            FunctionString = "Root";

        } else {
            FunctionString = StackData->AddressSymbol;
        }

        DbgOut("%s: %d%%, %d\n", FunctionString, Percent, StackData->Count);

        //
        // Move to the first child if it meets the threshold. The children are
        // always sorted by hit count, so only the first child needs to be
        // checked.
        //

        if (LIST_EMPTY(&(StackData->Children)) == FALSE) {
            ChildData = LIST_VALUE(StackData->Children.Next,
                                   STACK_DATA_ENTRY,
                                   SiblingEntry);

            Percent = (ChildData->Count * 100) / TotalCount;
            if (Percent >= Threshold) {

                //
                // Determine the length of the child's indent string.
                //

                IndentStringLength += PROFILER_STACK_INDENT_LENGTH;
                NewIndentString = realloc(IndentString, IndentStringLength);
                if (NewIndentString == NULL) {
                    DbgOut("Allocation failure\n");
                    free(IndentString);
                    return;
                }

                IndentString = NewIndentString;
                AddVerticalBar = FALSE;

                //
                // If the current node's next sibling will meet the threshold,
                // then a vertical bar needs to be added to the indent.
                //

                if ((StackData->Parent != NULL) &&
                    (StackData->SiblingEntry.Next !=
                     &(StackData->Parent->Children))) {

                    NextEntry = LIST_VALUE(StackData->SiblingEntry.Next,
                                           STACK_DATA_ENTRY,
                                           SiblingEntry);

                    NextPercent = (NextEntry->Count * 100) / TotalCount;
                    if (NextPercent >= Threshold) {
                        AddVerticalBar = TRUE;
                    }
                }

                IndentString[IndentStringLength - 3] = ' ';
                if (AddVerticalBar == FALSE) {
                    IndentString[IndentStringLength - 2] = ' ';

                } else {
                    IndentString[IndentStringLength - 2] = '|';
                }

                IndentString[IndentStringLength - 1] = '\0';
                StackData = ChildData;
                continue;
            }
        }

        //
        // The first child didn't exists or didn't meet the threshold. Search
        // until a sibling or ancestor sibling needs to be processed. As the
        // children are in order, only one sibling needs to be checked before
        // moving back up the tree.
        //

        while (StackData != Root) {

            assert(StackData->Parent != NULL);

            if (StackData->SiblingEntry.Next !=
                &(StackData->Parent->Children)) {

                StackData = LIST_VALUE(StackData->SiblingEntry.Next,
                                       STACK_DATA_ENTRY,
                                       SiblingEntry);

                Percent = (StackData->Count * 100) / TotalCount;
                if (Percent >= Threshold) {
                    break;
                }
            }

            //
            // The sibling didn't work out. So move up the tree and look at
            // the parent's sibling. Update the indent string.
            //

            StackData = StackData->Parent;
            IndentStringLength -= PROFILER_STACK_INDENT_LENGTH;
            IndentString[IndentStringLength - 1] = '\0';
        }

    } while (StackData != Root);

    free(IndentString);
    return;
}

PMEMORY_POOL_ENTRY
DbgrpGetMemoryPoolEntry (
    PLIST_ENTRY PoolListHead,
    PROFILER_MEMORY_TYPE ProfilerMemoryType
    )

/*++

Routine Description:

    This routine searches the given memory pool list and returns the entry for
    the given pool type.

Arguments:

    PoolListHead - Supplies a pointer to the head of the memory pool list to be
        searched.

    ProfilerMemoryType - Supplies the profiler memory type of the memory pool
        being sought.

Return Value:

    Returns a memory pool entry on success, or NULL on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PPROFILER_MEMORY_POOL MemoryPool;
    PMEMORY_POOL_ENTRY MemoryPoolEntry;

    CurrentEntry = PoolListHead->Next;
    while (CurrentEntry != PoolListHead) {
        MemoryPoolEntry = LIST_VALUE(CurrentEntry,
                                     MEMORY_POOL_ENTRY,
                                     ListEntry);

        MemoryPool = &(MemoryPoolEntry->MemoryPool);
        if (MemoryPool->ProfilerMemoryType == ProfilerMemoryType) {
            return MemoryPoolEntry;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    return NULL;
}

PPROFILER_MEMORY_POOL_TAG_STATISTIC
DbgrpGetTagStatistics (
    PMEMORY_POOL_ENTRY MemoryPoolEntry,
    ULONG Tag
    )

/*++

Routine Description:

    This routine searches the tag statistics in the given memory pool for those
    belonging to the given tag.

Arguments:

    MemoryPoolEntry - Supplies a pointer to the memory pool entry to be
        searched.

    Tag - Supplies the tag identifying the statistics to be returned.

Return Value:

    Returns a pointer to memory pool tag statistics on success, or NULL on
    failure.

--*/

{

    ULONG Index;
    PPROFILER_MEMORY_POOL_TAG_STATISTIC Statistic;
    ULONG TagCount;

    TagCount = MemoryPoolEntry->MemoryPool.TagCount;
    for (Index = 0; Index < TagCount; Index += 1) {
        Statistic = &(MemoryPoolEntry->TagStatistics[Index]);
        if (Statistic->Tag == Tag) {
            return Statistic;
        }
    }

    return NULL;
}

INT
DbgrpDispatchStackProfilerCommand (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine handles a stack profiler command.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments.

    ArgumentCount - Supplies the number of arguments in the Arguments array.

Return Value:

    Returns TRUE on success, or FALSE on failure.

--*/

{

    PSTR AdvancedString;
    PROFILER_DISPLAY_REQUEST DisplayRequest;
    LONG Threshold;

    assert(strcasecmp(Arguments[0], "stack") == 0);

    if (ArgumentCount < 2) {
        DbgOut(STACK_PROFILER_USAGE);
        return EINVAL;
    }

    Threshold = 0;
    DisplayRequest = ProfilerDisplayOneTime;
    if (strcasecmp(Arguments[1], "start") == 0) {
        DisplayRequest = ProfilerDisplayStart;

    } else if (strcasecmp(Arguments[1], "stop") == 0) {
        DisplayRequest = ProfilerDisplayStop;

    } else if (strcasecmp(Arguments[1], "clear") == 0) {
        DisplayRequest = ProfilerDisplayClear;

    } else if (strcasecmp(Arguments[1], "dump") == 0) {
        DisplayRequest = ProfilerDisplayOneTime;

    } else if (strcasecmp(Arguments[1], "threshold") == 0) {
        DisplayRequest = ProfilerDisplayOneTimeThreshold;
        if (ArgumentCount < 3) {
            DbgOut("Error: Percentage argument expected.\n");
            return EINVAL;
        }

        Threshold = strtol(Arguments[2], &AdvancedString, 0);
        if (Arguments[2] == AdvancedString) {
            DbgOut("Error: Invalid argument %s. Unable to convert to a valid "
                   "threshold value.\n",
                   Arguments[2]);

            return EINVAL;
        }

        //
        // The threshold for the stack profiler should only be from 0 to 100.
        //

        if ((Threshold < 0) || (Threshold > 100)) {
            DbgOut("Error: Invalid threshold percentage specified. Valid "
                   "values are between 0 and 100.\n");

            return EINVAL;
        }

    } else if (strcasecmp(Arguments[1], "help") == 0) {
        DbgOut(STACK_PROFILER_USAGE);
        return 0;

    } else {
        DbgOut("Error: Unknown stack profiler command '%s'.\n\n", Arguments[1]);
        DbgOut(STACK_PROFILER_USAGE);
        return EINVAL;
    }

    UiDisplayProfilerData(ProfilerDataTypeStack,
                          DisplayRequest,
                          (ULONG)Threshold);

    return 0;
}

INT
DbgrpDispatchMemoryProfilerCommand (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine handles a memory profiler command.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments.

    ArgumentCount - Supplies the number of arguments in the Arguments array.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PSTR AdvancedString;
    PROFILER_DISPLAY_REQUEST DisplayRequest;
    LONG Threshold;

    assert(strcasecmp(Arguments[0], "memory") == 0);

    if (ArgumentCount < 2) {
        DbgOut(MEMORY_PROFILER_USAGE);
        return EINVAL;
    }

    Threshold = 0;
    DisplayRequest = ProfilerDisplayOneTime;
    if (strcasecmp(Arguments[1], "start") == 0) {
        DisplayRequest = ProfilerDisplayStart;

    } else if (strcasecmp(Arguments[1], "delta") == 0) {
        DisplayRequest = ProfilerDisplayStartDelta;

    } else if (strcasecmp(Arguments[1], "stop") == 0) {
        DisplayRequest = ProfilerDisplayStop;

    } else if (strcasecmp(Arguments[1], "clear") == 0) {
        DisplayRequest = ProfilerDisplayStopDelta;

    } else if (strcasecmp(Arguments[1], "dump") == 0) {
        DisplayRequest = ProfilerDisplayOneTime;

    } else if (strcasecmp(Arguments[1], "threshold") == 0) {
        DisplayRequest = ProfilerDisplayOneTimeThreshold;
        if (ArgumentCount < 3) {
            DbgOut("Error: Active count threshold argument expected.\n");
            return EINVAL;
        }

        Threshold = strtol(Arguments[2], &AdvancedString, 0);
        if (Arguments[2] == AdvancedString) {
            DbgOut("Error: Invalid argument %s. Unable to convert to a valid "
                   "threshold value.\n",
                   Arguments[2]);

            return EINVAL;
        }

    } else if (strcasecmp(Arguments[1], "help") == 0) {
        DbgOut(MEMORY_PROFILER_USAGE);
        return 0;

    } else {
        DbgOut("Error: Unknown memory profiler command '%s'.\n\n",
               Arguments[1]);

        DbgOut(MEMORY_PROFILER_USAGE);
        return EINVAL;
    }

    UiDisplayProfilerData(ProfilerDataTypeMemory,
                          DisplayRequest,
                          (ULONG)Threshold);

    return 0;
}

