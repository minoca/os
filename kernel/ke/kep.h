/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    kep.h

Abstract:

    This header contains private definitions for the Kernel Executive.

Author:

    Evan Green 6-Aug-2012

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

    This structure stores an IPI request packet.

Members:

    ListEntry - Stores pointers to the previous and next list entries.

    IpiRoutine - Stores a pointer to the routine to execute on each processor.

    Context - Stores a pointer to the parameter passed to the IPI routine.

    ProcessorsRemaining - Stores a pointer where the number of processors that
        have yet to complete the IPI is stored.

--*/

typedef struct _IPI_REQUEST {
    LIST_ENTRY ListEntry;
    PIPI_ROUTINE IpiRoutine;
    PVOID Context;
    volatile ULONG *ProcessorsRemaining;
} IPI_REQUEST, *PIPI_REQUEST;

//
// -------------------------------------------------------------------- Globals
//

//
// Structures that store the processor blocks and total number of processors.
//

extern PPROCESSOR_BLOCK *KeProcessorBlocks;
extern volatile ULONG KeActiveProcessorCount;

//
// Store the version information jammed into a packed format.
//

extern ULONG KeEncodedVersion;
extern ULONG KeVersionSerial;
extern ULONG KeBuildTime;
extern PSTR KeBuildString;
extern PSTR KeProductName;

extern SYSTEM_FIRMWARE_TYPE KeSystemFirmwareType;
extern PKERNEL_COMMAND_LINE KeCommandLine;

//
// Store the current periodic clock rate, in time counter ticks.
//

extern ULONGLONG KeClockRate;

//
// Set this to true to disable dynamic tick. This reverts back to a periodic
// timer tick that's always running.
//

extern BOOL KeDisableDynamicTick;

//
// -------------------------------------------------------- Function Prototypes
//

VOID
KepArchPrepareForContextSwap (
    PPROCESSOR_BLOCK ProcessorBlock,
    PKTHREAD CurrentThread,
    PKTHREAD NewThread
    );

/*++

Routine Description:

    This routine performs any architecture specific work before context swapping
    between threads. This must be called at dispatch level.

Arguments:

    ProcessorBlock - Supplies a pointer to the processor block of the current
        processor.

    CurrentThread - Supplies a pointer to the current (old) thread.

    NewThread - Supplies a pointer to the thread that's about to be switched to.

Return Value:

    None.

--*/

VOID
KepContextSwap (
    PVOID *SavedStackLocation,
    PVOID NewStack,
    ULONGLONG NewThreadPointer,
    BOOL FirstTime
    );

/*++

Routine Description:

    This routine switches context to the given thread.

Arguments:

    SavedStackLocation - Supplies a pointer where the old stack pointer will
        be saved.

    NewStack - Supplies the new stack address.

    NewThreadPointer - Supplies the new thread pointer data.

    FirstTime - Supplies a boolean indicating whether the thread has never been
        run before.

Return Value:

    None.

--*/

KSTATUS
KepInitializeSystemWorkQueue (
    VOID
    );

/*++

Routine Description:

    This routine initializes the system work queue. This must happen after the
    Object Manager initializes.

Arguments:

    None.

Return Value:

    Status code.

--*/

VOID
KepExecutePendingDpcs (
    VOID
    );

/*++

Routine Description:

    This routine executes any pending DPCs on the current processor. This
    routine should only be executed internally by the scheduler. It must be
    called at dispatch level. Interrupts must be disabled upon entry, but will
    be enabled on exit.

Arguments:

    None.

Return Value:

    None.

--*/

PKTIMER_DATA
KepCreateTimerData (
    VOID
    );

/*++

Routine Description:

    This routine is called upon system initialization to create a timer
    management context for a new processor.

Arguments:

    None.

Return Value:

    Returns a pointer to the timer data on success.

    NULL on resource allocation failure.

--*/

VOID
KepDestroyTimerData (
    PKTIMER_DATA Data
    );

/*++

Routine Description:

    This routine tears down a processor's timer management context.

Arguments:

    Data - Supplies a pointer to the data to destroy.

Return Value:

    None.

--*/

VOID
KepDispatchTimers (
    ULONGLONG CurrentTime
    );

/*++

Routine Description:

    This routine is called at regular intervals to check for and expire timers
    whose time has come. This routine must only be called internally, and must
    be called at dispatch level.

Arguments:

    Queue - Supplies a pointer to the timer queue for the current processor.

    CurrentTime - Supplies the current time counter value. Any timers with this
        due time or earlier will be expired.

Return Value:

    None.

--*/

ULONGLONG
KepGetNextTimerDeadline (
    PPROCESSOR_BLOCK Processor,
    PBOOL Hard
    );

/*++

Routine Description:

    This routine returns the next waking deadline of timers on the given
    processor. This routine must be called at or above dispatch level.

Arguments:

    Processor - Supplies a pointer to the processor.

    Hard - Supplies a pointer where a boolean will be returned indicating if
        this is a hard deadline or a soft deadline.

Return Value:

    Returns the next waking timer deadline.

    -1 if there are no waking timers.

--*/

VOID
KepGetTimeOffset (
    PSYSTEM_TIME TimeOffset
    );

/*++

Routine Description:

    This routine reads the time offset from the shared user data page.

Arguments:

    TimeOffset - Supplies a pointer that receives the time offset from the
        shared user data page.

Return Value:

    None.

--*/

KSTATUS
KepSetTimeOffset (
    PSYSTEM_TIME NewTimeOffset,
    PDPC Dpc
    );

/*++

Routine Description:

    This routine sets the time offset in the shared user data page. For
    synchronization purposes, the time offset can only be updated by the clock
    owner at the clock run level. If the caller requires this routine to
    succeed, then a DPC can be supplied, otherwise the DPC will be allocated if
    necessary and said allocation could fail.

Arguments:

    NewTimeOffset - Supplies a pointer to the new time offset. This cannot be
        a pointer to paged pool as it may be used at dispatch level by the DPC.

    Dpc - Supplies a pointer to an optional DPC to use when tracking down the
        clock owner.

Return Value:

    Status code.

--*/

VOID
KepInitializeClock (
    PPROCESSOR_BLOCK Processor
    );

/*++

Routine Description:

    This routine initializes system clock information.

Arguments:

    Processor - Supplies a pointer to the processor block being initialized.

Return Value:

    None.

--*/

VOID
KepUpdateClockDeadline (
    VOID
    );

/*++

Routine Description:

    This routine is called when the next clock deadline is potentially changed.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
KepClockIdle (
    PPROCESSOR_BLOCK Processor
    );

/*++

Routine Description:

    This routine is called when the processor goes idle. It potentially
    requests a clock transition to disable the clock.

Arguments:

    Processor - Supplies a pointer to the current processor block.

Return Value:

    None.

--*/

VOID
KepSetClockToPeriodic (
    PPROCESSOR_BLOCK Processor
    );

/*++

Routine Description:

    This routine sets the clock to be periodic on the given processor. This
    routine must be called at or above dispatch level.

Arguments:

    Processor - Supplies a pointer to the processor block for the processor
        whose clock should be switched to periodic.

Return Value:

    None.

--*/

VOID
KepAddTimePointEntropy (
    VOID
    );

/*++

Routine Description:

    This routine adds entropy in the form of a timestamp to the pseudo random
    interface, if one exists.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
KepInitializeScheduler (
    PPROCESSOR_BLOCK ProcessorBlock
    );

/*++

Routine Description:

    This routine initializes the scheduler for a processor.

Arguments:

    ProcessorBlock - Supplies a pointer to the processor block.

Return Value:

    None.

--*/

KSTATUS
KepWriteCrashDump (
    ULONG CrashCode,
    ULONGLONG Parameter1,
    ULONGLONG Parameter2,
    ULONGLONG Parameter3,
    ULONGLONG Parameter4
    );

/*++

Routine Description:

    This routine writes crash dump data to disk.

Arguments:

    CrashCode - Supplies the reason for the system crash.

    Parameter1 - Supplies an optional parameter regarding the crash.

    Parameter2 - Supplies an optional parameter regarding the crash.

    Parameter3 - Supplies an optional parameter regarding the crash.

    Parameter4 - Supplies an optional parameter regarding the crash.

Return Value:

    Status code.

--*/

KSTATUS
KepSetBannerThread (
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

/*++

Routine Description:

    This routine enables or disables the banner thread.

Arguments:

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    Status code.

--*/
