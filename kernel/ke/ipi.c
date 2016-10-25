/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ipi.c

Abstract:

    This module implements support for generic Inter-Processor Interrupt
    handling.

Author:

    Evan Green 27-Sep-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "kep.h"

//
// ---------------------------------------------------------------- Definitions
//

#define IPI_ALLOCATION_TAG 0x6970494B // 'ipIK'

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INTERRUPT_STATUS
KeIpiServiceRoutine (
    PVOID Context
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

PPROCESSOR_BLOCK
KeGetProcessorBlock (
    ULONG ProcessorNumber
    )

/*++

Routine Description:

    This routine returns the processor block for the given processor number.

Arguments:

    ProcessorNumber - Supplies the number of the processor.

Return Value:

    Returns the processor block for the given processor.

    NULL if the input was not a valid processor number.

--*/

{

    if (ProcessorNumber >= KeActiveProcessorCount) {
        return NULL;
    }

    return KeProcessorBlocks[ProcessorNumber];
}

KSTATUS
KeSendIpi (
    PIPI_ROUTINE IpiRoutine,
    PVOID IpiContext,
    PPROCESSOR_SET Processors
    )

/*++

Routine Description:

    This routine runs the given routine at IPI level on the specified set of
    processors. This routine runs synchronously: the routine will have completed
    running on all processors by the time this routine returns. This routine
    must be called at or below dispatch level.

Arguments:

    IpiRoutine - Supplies a pointer to the routine to run at IPI level.

    IpiContext - Supplies the value to pass to the IPI routine as a parameter.

    Processors - Supplies the set of processors to run the IPI on.

Return Value:

    Status code.

--*/

{

    ULONG CurrentProcessor;
    PIPI_REQUEST IpiRequests;
    RUNLEVEL OldRunLevel;
    PPROCESSOR_BLOCK Processor;
    ULONG ProcessorCount;
    ULONG ProcessorIndex;
    volatile ULONG ProcessorsRemaining;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() <= RunLevelDispatch);

    CurrentProcessor = KeGetCurrentProcessorNumber();
    IpiRequests = NULL;
    if (Processors == NULL) {
        Status = STATUS_INVALID_PARAMETER;
        goto SendIpiEnd;
    }

    //
    // Determine the number of processors targeted by this IPI.
    //

    switch (Processors->Target) {
    case ProcessorTargetNone:
        Status = STATUS_SUCCESS;
        goto SendIpiEnd;

    case ProcessorTargetAll:
        ProcessorCount = KeActiveProcessorCount;
        break;

    case ProcessorTargetAllExcludingSelf:
        ProcessorCount = KeActiveProcessorCount - 1;
        break;

    case ProcessorTargetSelf:
        ProcessorCount = 1;
        break;

    case ProcessorTargetSingleProcessor:
        ProcessorCount = 1;
        break;

    case ProcessorTargetAny:
    default:

        ASSERT(FALSE);

        Status = STATUS_INVALID_PARAMETER;
        goto SendIpiEnd;
    }

    //
    // Allocate an IPI request packet for each processor being targeted.
    //

    IpiRequests = MmAllocateNonPagedPool(ProcessorCount * sizeof(IPI_REQUEST),
                                         IPI_ALLOCATION_TAG);

    if (IpiRequests == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto SendIpiEnd;
    }

    //
    // Fill out each IPI request packet.
    //

    for (ProcessorIndex = 0;
         ProcessorIndex < ProcessorCount;
         ProcessorIndex += 1) {

        IpiRequests[ProcessorIndex].IpiRoutine = IpiRoutine;
        IpiRequests[ProcessorIndex].Context = IpiContext;
        IpiRequests[ProcessorIndex].ProcessorsRemaining = &ProcessorsRemaining;
    }

    //
    // Put each IPI request packet on the given processor blocks.
    //

    OldRunLevel = KeRaiseRunLevel(RunLevelIpi);
    ProcessorsRemaining = ProcessorCount;
    switch (Processors->Target) {
    case ProcessorTargetAll:
    case ProcessorTargetAllExcludingSelf:

        //
        // Insert the IPI onto each processor's list.
        //

        for (ProcessorIndex = 0;
             ProcessorIndex < ProcessorCount;
             ProcessorIndex += 1) {

            Processor = KeProcessorBlocks[ProcessorIndex];

            //
            // If the IPI is not targeted at this processor, skip it.
            //

            if ((ProcessorIndex == CurrentProcessor) &&
                (Processors->Target == ProcessorTargetAllExcludingSelf)) {

                continue;
            }

            //
            // Insert the IPI onto the end of the processor's IPI request
            // queue.
            //

            KeAcquireSpinLock(&(Processor->IpiListLock));
            INSERT_BEFORE(&(IpiRequests[ProcessorIndex].ListEntry),
                          &(Processor->IpiListHead));

            KeReleaseSpinLock(&(Processor->IpiListLock));
        }

        break;

    case ProcessorTargetSelf:

        //
        // Insert the IPI request onto this processor's queue, then flush it.
        //

        Processor = KeProcessorBlocks[CurrentProcessor];
        KeAcquireSpinLock(&(Processor->IpiListLock));
        INSERT_BEFORE(&(IpiRequests[0].ListEntry), &(Processor->IpiListHead));
        KeReleaseSpinLock(&(Processor->IpiListLock));
        break;

    case ProcessorTargetSingleProcessor:
        if (Processors->U.Number > KeActiveProcessorCount) {
            Status = STATUS_INVALID_PARAMETER;
            goto SendIpiEnd;
        }

        Processor = KeProcessorBlocks[Processors->U.Number];
        KeAcquireSpinLock(&(Processor->IpiListLock));
        INSERT_BEFORE(&(IpiRequests[0].ListEntry), &(Processor->IpiListHead));
        KeReleaseSpinLock(&(Processor->IpiListLock));
        break;

    case ProcessorTargetAny:
    default:

        ASSERT(FALSE);

        Status = STATUS_INVALID_PARAMETER;
        KeLowerRunLevel(OldRunLevel);
        goto SendIpiEnd;
    }

    //
    // Send the IPI interrupt, unless there is only one processor in the system,
    // in which case just call the routine.
    //

    if ((ProcessorCount == 1) && (KeActiveProcessorCount == 1)) {
        KeIpiServiceRoutine(NULL);
        Status = STATUS_SUCCESS;

    } else {
        Status = HlSendIpi(IpiTypePacket, Processors);
    }

    KeLowerRunLevel(OldRunLevel);
    if (!KSUCCESS(Status)) {
        goto SendIpiEnd;
    }

    //
    // Wait for all processors to complete the IPI.
    //

    while (ProcessorsRemaining != 0) {
        ArProcessorYield();
    }

    Status = STATUS_SUCCESS;

SendIpiEnd:
    if (IpiRequests != NULL) {
        MmFreeNonPagedPool(IpiRequests);
    }

    return Status;
}

INTERRUPT_STATUS
KeIpiServiceRoutine (
    PVOID Context
    )

/*++

Routine Description:

    This routine checks for any pending IPIs on the current processor and
    executes them, in order. The processor must be executing at IPI level.

Arguments:

    Context - Supplies an unused context pointer.

Return Value:

    Returns claimed always. On return, the IPI queue will be empty.

--*/

{

    PIPI_REQUEST CurrentRequest;
    RUNLEVEL OldRunLevel;
    PPROCESSOR_BLOCK Processor;

    OldRunLevel = KeRaiseRunLevel(RunLevelIpi);
    Processor = KeGetCurrentProcessorBlock();
    KeAcquireSpinLock(&(Processor->IpiListLock));
    while (!LIST_EMPTY(&(Processor->IpiListHead))) {

        //
        // Get and remove the first item on the list.
        //

        CurrentRequest = LIST_VALUE(Processor->IpiListHead.Next,
                                    IPI_REQUEST,
                                    ListEntry);

        LIST_REMOVE(&(CurrentRequest->ListEntry));

        //
        // Execute the IPI.
        //

        CurrentRequest->IpiRoutine(CurrentRequest->Context);

        //
        // Signal this IPI is complete.
        //

        RtlAtomicAdd32(CurrentRequest->ProcessorsRemaining, -1);
    }

    KeReleaseSpinLock(&(Processor->IpiListLock));
    KeLowerRunLevel(OldRunLevel);
    return InterruptStatusClaimed;
}

//
// --------------------------------------------------------- Internal Functions
//

