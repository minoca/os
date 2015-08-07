/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    intrupt.c

Abstract:

    This module implements driver-facing APIs for managing interrupts.

Author:

    Evan Green 21-Dec-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include "iop.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
KSTATUS
IoConnectInterrupt (
    PDEVICE Device,
    ULONGLONG LineNumber,
    ULONGLONG Vector,
    PINTERRUPT_SERVICE_ROUTINE ServiceRoutine,
    PVOID Context,
    PHANDLE Interrupt
    )

/*++

Routine Description:

    This routine connects a device's interrupt.

Arguments:

    Device - Supplies a pointer to the device making the connection.

    LineNumber - Supplies the global system interrupt number of the line to
        connect to. This value can be found from the interrupt line allocation.
        It is required that the device arbitrate for the line being connected
        to. Supply INVALID_INTERRUPT_LINE if only the vector needs to be
        connected (e.g. when enabling an MSI).

    Vector - Supplies the vector to connect the interrupt on. It is required
        that the device arbitrate for the vector being connected to.

    ServiceRoutine - Supplies a pointer to the routine to call when the
        interrupt fires.

    Context - Supplies a pointer's worth of context that will be passed in to
        the interrupt service routine when it is called.

    Interrupt - Supplies a handle to the interrupt returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_READY if the device has no resources or is not started.

    STATUS_RESOURCE_IN_USE if the device attempts to connect to an interrupt
    it does not own.

    Other errors on failure.

--*/

{

    PRESOURCE_ALLOCATION Allocation;
    PRESOURCE_ALLOCATION_LIST AllocationList;
    BOOL Connected;
    BOOL Enabled;
    INTERRUPT_MODE InterruptMode;
    INTERRUPT_ACTIVE_LEVEL InterruptPolarity;
    PRESOURCE_ALLOCATION LineAllocation;
    ULONGLONG LineCharacteristics;
    ULONG LineState;
    PKINTERRUPT NewInterrupt;
    KSTATUS Status;
    PRESOURCE_ALLOCATION VectorAllocation;

    Connected = FALSE;
    Enabled = FALSE;
    NewInterrupt = NULL;
    LineAllocation = NULL;
    VectorAllocation = NULL;
    if (Device == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Ensure that the device has resources.
    //

    AllocationList = Device->ProcessorLocalResources;
    if (AllocationList == NULL) {
        Status = STATUS_NOT_READY;
        goto ConnectInterruptEnd;
    }

    //
    // Ensure that the device owns the line number it's trying to connect to.
    //

    Allocation = IoGetNextResourceAllocation(AllocationList, NULL);
    while (Allocation != NULL) {
        if ((VectorAllocation == NULL) &&
            (Allocation->Type == ResourceTypeInterruptVector)) {

            if ((Allocation->Allocation <= Vector) &&
                (Allocation->Allocation + Allocation->Length > Vector)) {

                VectorAllocation = Allocation;
            }

        } else if ((LineNumber != INVALID_INTERRUPT_LINE) &&
                   (LineAllocation == NULL) &&
                   (Allocation->Type == ResourceTypeInterruptLine)) {

            if ((Allocation->Allocation <= LineNumber) &&
                (Allocation->Allocation + Allocation->Length > LineNumber)) {

                LineAllocation = Allocation;
            }
        }

        //
        // If both are found stop looking.
        //

        if ((VectorAllocation != NULL) &&
            ((LineNumber == INVALID_INTERRUPT_LINE) ||
             (LineAllocation != NULL))) {

            break;
        }

        Allocation = IoGetNextResourceAllocation(AllocationList, Allocation);
    }

    //
    // If the device is trying to pull a fast one and doesn't own the
    // resources its connecting to, fail.
    //

    if ((VectorAllocation == NULL) ||
        ((LineNumber != INVALID_INTERRUPT_LINE) &&
         (LineAllocation == NULL))) {

        Status = STATUS_RESOURCE_IN_USE;
        goto ConnectInterruptEnd;
    }

    //
    // If the vector and line allocations are not connected then something is
    // wrong. The line might not be targeting the correct vector.
    //

    if ((LineNumber != INVALID_INTERRUPT_LINE) &&
        (VectorAllocation->OwningAllocation != LineAllocation)) {

        Status = STATUS_INVALID_PARAMETER;
        goto ConnectInterruptEnd;
    }

    //
    // Attempt to create an interrupt.
    //

    NewInterrupt = HlCreateInterrupt(Vector,
                                     VECTOR_TO_RUN_LEVEL(Vector),
                                     ServiceRoutine,
                                     Context);

    if (NewInterrupt == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ConnectInterruptEnd;
    }

    //
    // Attempt to wire up the ISR.
    //

    *Interrupt = NewInterrupt;
    Status = HlConnectInterrupt(NewInterrupt);
    if (!KSUCCESS(Status)) {
        goto ConnectInterruptEnd;
    }

    Connected = TRUE;

    //
    // If a valid line number was supplied, then enable the interrupt line.
    //

    if (LineNumber != INVALID_INTERRUPT_LINE) {
        LineState = 0;
        LineCharacteristics = LineAllocation->Characteristics;

        //
        // Set up the interrupt mode and polarity.
        //

        InterruptPolarity = InterruptActiveLevelUnknown;
        if ((LineCharacteristics & INTERRUPT_LINE_ACTIVE_HIGH) != 0) {
            if ((LineCharacteristics & INTERRUPT_LINE_ACTIVE_LOW) != 0) {
                InterruptPolarity = InterruptActiveBoth;

            } else {
                InterruptPolarity = InterruptActiveHigh;
            }

        } else if ((LineCharacteristics & INTERRUPT_LINE_ACTIVE_LOW) != 0) {
            InterruptPolarity = InterruptActiveLow;
        }

        InterruptMode = InterruptModeLevel;
        if ((LineCharacteristics & INTERRUPT_LINE_EDGE_TRIGGERED) != 0) {
            InterruptMode = InterruptModeEdge;
        }

        //
        // Set any other flags.
        //

        if ((LineCharacteristics & INTERRUPT_LINE_WAKE) != 0) {
            LineState |= INTERRUPT_LINE_STATE_FLAG_WAKE;
        }

        if ((LineCharacteristics & INTERRUPT_LINE_DEBOUNCE) != 0) {
            LineState |= INTERRUPT_LINE_STATE_FLAG_DEBOUNCE;
        }

        //
        // Now attempt to enable the interrupt line.
        //

        Status = HlEnableInterruptLine(LineNumber,
                                       InterruptMode,
                                       InterruptPolarity,
                                       LineState,
                                       NewInterrupt);

        if (!KSUCCESS(Status)) {
            goto ConnectInterruptEnd;
        }

        Enabled = TRUE;
    }

    Status = STATUS_SUCCESS;

ConnectInterruptEnd:
    if (!KSUCCESS(Status)) {
        if (NewInterrupt != NULL) {
            if (Enabled != FALSE) {
                HlDisableInterruptLine(NewInterrupt);
            }

            if (Connected != FALSE) {
                HlDisconnectInterrupt(NewInterrupt);
            }

            HlDestroyInterrupt(NewInterrupt);
        }

        *Interrupt = INVALID_HANDLE;
    }

    return Status;
}

KERNEL_API
VOID
IoDisconnectInterrupt (
    HANDLE InterruptHandle
    )

/*++

Routine Description:

    This routine disconnects a device's interrupt. The device must not
    generate interrupts when this routine is called, as the interrupt line
    may remain open to service other devices connected to the line.

Arguments:

    InterruptHandle - Supplies the handle to the interrupt, returned when the
        interrupt was connected.

Return Value:

    None.

--*/

{

    PKINTERRUPT Interrupt;

    Interrupt = (PKINTERRUPT)InterruptHandle;

    //
    // Disable the interrupt line, then disconnect the vector.
    //

    HlDisableInterruptLine(Interrupt);
    HlDisconnectInterrupt(Interrupt);

    //
    // Destroy the interrupt.
    //

    HlDestroyInterrupt(Interrupt);
    return;
}

KERNEL_API
RUNLEVEL
IoRaiseToInterruptRunLevel (
    HANDLE InterruptHandle
    )

/*++

Routine Description:

    This routine raises the current run level to that of the given connected
    interrupt. Callers should use KeLowerRunLevel to return from the run level
    raised to here.

Arguments:

    InterruptHandle - Supplies the handle to the interrupt, returned when the
        interrupt was connected.

Return Value:

    Returns the run level of the current processor immediately before it was
    raised by this function.

--*/

{

    PKINTERRUPT Interrupt;

    ASSERT((InterruptHandle != INVALID_HANDLE) && (InterruptHandle != NULL));

    Interrupt = (PKINTERRUPT)InterruptHandle;
    return KeRaiseRunLevel(Interrupt->RunLevel);
}

//
// --------------------------------------------------------- Internal Functions
//

