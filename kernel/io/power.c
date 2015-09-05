/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    power.c

Abstract:

    This module implements generic support for device runtime power managment
    within the kernel.

Author:

    Evan Green 2-Sep-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include "pmp.h"
#include "iop.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the default delay in seconds before a device that has no power
// references is sent an idle request.
//

#define PM_INITIAL_IDLE_DELAY_SECONDS 1

//
// Define the number of data points of device idle history to keep, expressed
// as a bit shift.
//

#define PM_DEVICE_HISTORY_SIZE_SHIFT 5

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
PmpInitializeDevice (
    PDEVICE Device
    );

VOID
PmpDeviceIdleTimerDpc (
    PDPC Dpc
    );

VOID
PmpDeviceIdleWorker (
    PVOID Parameter
    );

VOID
PmpDeviceIncrementActiveChildren (
    PDEVICE Device
    );

VOID
PmpDeviceDecrementActiveChildren (
    PDEVICE Device
    );

KSTATUS
PmpDeviceAddReference (
    PDEVICE Device,
    DEVICE_POWER_REQUEST Request
    );

KSTATUS
PmpDeviceQueuePowerTransition (
    PDEVICE Device,
    DEVICE_POWER_REQUEST Request
    );

KSTATUS
PmpDeviceResume (
    PDEVICE Device
    );

VOID
PmpDeviceIdle (
    PDEVICE Device
    );

VOID
PmpDeviceSuspend (
    PDEVICE Device
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Set this boolean to TRUE to print power transitions.
//

BOOL PmDebugPowerTransitions;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
KSTATUS
PmInitialize (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine initializes power management infrastructure for a given
    device.

Arguments:

    Device - Supplies a pointer to the device to prepare to do power management
        calls on.

Return Value:

    Status code.

--*/

{

    if (Device->Power != NULL) {
        return STATUS_SUCCESS;
    }

    return PmpInitializeDevice(Device);
}

KERNEL_API
KSTATUS
PmDeviceAddReference (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine adds a power management reference on the given device,
    and waits for the device to transition to the active state.

Arguments:

    Device - Supplies a pointer to the device to add a power reference to.

Return Value:

    Status code. On failure, the caller will not have a reference on the
    device, and should not assume that the device or its parent lineage is
    active.

--*/

{

    KSTATUS Status;

    Status = PmpDeviceAddReference(Device, DevicePowerRequestResume);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    KeWaitForEvent(Device->Power->ActiveEvent, FALSE, WAIT_TIME_INDEFINITE);
    return Status;
}

KERNEL_API
KSTATUS
PmDeviceAddReferenceAsynchronous (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine adds a power management reference on the given device,
    preventing the device from idling until the reference is released.

Arguments:

    Device - Supplies a pointer to the device to add a power reference to.

Return Value:

    Status code indicating if the request was successfully queued. On failure,
    the caller will not have the reference on the device.

--*/

{

    UINTN PreviousCount;
    PDEVICE_POWER State;
    KSTATUS Status;

    State = Device->Power;
    PreviousCount = RtlAtomicAdd(&(State->ReferenceCount), 1);

    ASSERT(PreviousCount < 0x10000000);

    Status = STATUS_SUCCESS;
    if (PreviousCount == 0) {
        Status = PmpDeviceQueuePowerTransition(Device,
                                               DevicePowerRequestResume);

        if (!KSUCCESS(Status)) {
            RtlAtomicAdd(&(State->ReferenceCount), -1);
        }
    }

    return Status;
}

KERNEL_API
KSTATUS
PmDeviceReleaseReference (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine releases a power management reference on a device.

Arguments:

    Device - Supplies a pointer to the device to subtract a power reference
        from.

Return Value:

    Status code indicating if the idle timer was successfully queued. The
    reference itself is always dropped, even on failure.

--*/

{

    UINTN PreviousCount;
    PDEVICE_POWER State;
    KSTATUS Status;
    ULONG TimerQueued;

    State = Device->Power;
    Status = STATUS_SUCCESS;
    PreviousCount = RtlAtomicAdd(&(State->ReferenceCount), -1);

    ASSERT((PreviousCount != 0) && (PreviousCount < 0x10000000));

    if (PreviousCount > 1) {
        return Status;
    }

    //
    // Bump up the idle deadline even if the timer is already queued. The
    // timer will see this and requeue itself.
    //

    State->IdleTimeout = HlQueryTimeCounter() + State->IdleDelay;

    //
    // Try to win the race to queue the timer.
    //

    TimerQueued = RtlAtomicCompareExchange32(&(State->TimerQueued), 1, 0);
    if (TimerQueued == 0) {
        Status = KeQueueTimer(State->IdleTimer,
                              TimerQueueSoftWake,
                              State->IdleTimeout,
                              0,
                              0,
                              State->IdleTimerDpc);

        if (!KSUCCESS(Status)) {
            RtlDebugPrint("PM: Cannot queue idle timer: device %x: %x\n",
                          Device,
                          Status);
        }
    }

    return Status;
}

KERNEL_API
KSTATUS
PmDeviceSetState (
    PDEVICE Device,
    DEVICE_POWER_STATE PowerState
    )

/*++

Routine Description:

    This routine sets a new power state for the device. This can be used to
    clear an error. It should not be called from a power IRP.

Arguments:

    Device - Supplies a pointer to the device to set the power state for.

    PowerState - Supplies the new power management state to set. The only
        valid states to set are active and suspended.

Return Value:

    Status code.

--*/

{

    PDEVICE_POWER State;
    KSTATUS Status;

    State = Device->Power;
    Status = STATUS_SUCCESS;
    switch (PowerState) {
    case DevicePowerStateActive:
        if (State->State == DevicePowerStateActive) {
            Status = STATUS_SUCCESS;

        } else {

            //
            // Add a reference on the device to bring it up, then release that
            // reference to send it down towards sleepytown.
            //

            Status = PmpDeviceAddReference(Device,
                                           DevicePowerRequestMarkActive);

            if (KSUCCESS(Status)) {
                Status = PmDeviceReleaseReference(Device);
            }
        }

        break;

    case DevicePowerStateSuspended:
        KeAcquireSharedExclusiveLockExclusive(Device->Lock);
        if (State->State == DevicePowerStateRemoved) {
            Status = STATUS_DEVICE_NOT_CONNECTED;

        } else {
            if ((State->State == DevicePowerStateActive) ||
                ((State->State == DevicePowerStateTransitioning) &&
                 (State->PreviousState == DevicePowerStateActive))) {

                PmpDeviceDecrementActiveChildren(Device->ParentDevice);
            }

            State->State = DevicePowerStateSuspended;
            State->Request = DevicePowerRequestNone;
        }

        KeReleaseSharedExclusiveLockExclusive(Device->Lock);
        break;

    case DevicePowerStateIdle:
    default:

        ASSERT(FALSE);

        Status = STATUS_INVALID_PARAMETER;
        break;
    }

    return Status;
}

VOID
PmpRemoveDevice (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine is called when a device is removed from the system. It cleans
    up the power management state. It is assumed that the device lock is
    already held exclusive.

Arguments:

    Device - Supplies a pointer to the device to remove.

Return Value:

    None.

--*/

{

    DEVICE_POWER_STATE OldState;
    PDEVICE_POWER State;

    State = Device->Power;
    if (State == NULL) {
        return;
    }

    OldState = State->State;
    State->State = DevicePowerStateRemoved;
    State->Request = DevicePowerRequestNone;
    RtlAtomicExchange32(&(State->TimerQueued), 1);
    KeCancelTimer(State->IdleTimer);
    KeCancelDpc(State->IdleTimerDpc);
    KeCancelWorkItem(State->IdleTimerWorkItem);
    if ((OldState == DevicePowerStateActive) ||
        ((State->State == DevicePowerStateTransitioning) &&
         (State->PreviousState == DevicePowerStateActive))) {

        PmpDeviceDecrementActiveChildren(Device->ParentDevice);
    }

    return;
}

VOID
PmpDestroyDevice (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine tears down the power management structures associated with a
    device.

Arguments:

    Device - Supplies a pointer to the device to tear down.

Return Value:

    None.

--*/

{

    PDEVICE_POWER State;

    if (Device->Power != NULL) {
        State = Device->Power;

        //
        // Work through the timer, DPC, work item flow, starting at the source
        // and squeezing the tube along the way.
        //

        if (State->IdleTimer != NULL) {
            KeDestroyTimer(State->IdleTimer);
        }

        if (State->IdleTimerDpc != NULL) {
            KeDestroyDpc(State->IdleTimerDpc);
        }

        if (State->IdleTimerWorkItem != NULL) {
            KeCancelWorkItem(State->IdleTimerWorkItem);
            KeFlushWorkItem(State->IdleTimerWorkItem);
            KeDestroyWorkItem(State->IdleTimerWorkItem);
        }

        if (State->ActiveEvent != NULL) {
            KeDestroyEvent(State->ActiveEvent);
        }

        if (State->Irp != NULL) {
            IoDestroyIrp(State->Irp);
        }

        if (State->History != NULL) {
            PmpDestroyIdleHistory(State->History);
        }

        Device->Power = NULL;
        MmFreePagedPool(State);
    }

    return;
}

VOID
PmpDevicePowerTransition (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine is called by the worker thread to perform a device power
    transition.

Arguments:

    Device - Supplies a pointer to the device to work on.

Return Value:

    None.

--*/

{

    PDEVICE_POWER State;

    State = Device->Power;
    if (State->State == DevicePowerStateTransitioning) {
        switch (State->Request) {
        case DevicePowerRequestIdle:
            PmpDeviceIdle(Device);
            break;

        case DevicePowerRequestSuspend:
            PmpDeviceSuspend(Device);
            break;

        case DevicePowerRequestResume:
        case DevicePowerRequestMarkActive:
            PmpDeviceResume(Device);
            break;

        default:

            ASSERT(FALSE);

            break;
        }
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
PmpInitializeDevice (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine initializes the power management portion of a device structure.

Arguments:

    Device - Supplies a pointer to the device to initialize.

Return Value:

    Status code.

--*/

{

    PDEVICE_POWER State;
    KSTATUS Status;

    State = MmAllocatePagedPool(sizeof(DEVICE_POWER), PM_ALLOCATION_TAG);
    if (State == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(State, sizeof(DEVICE_POWER));
    Device->Power = State;
    State->State = DevicePowerStateSuspended;
    State->Request = DevicePowerRequestNone;
    State->IdleDelay = HlQueryTimeCounterFrequency() *
                       PM_INITIAL_IDLE_DELAY_SECONDS;

    State->ActiveEvent = KeCreateEvent(NULL);
    State->IdleTimer = KeCreateTimer(PM_ALLOCATION_TAG);

    //
    // This work item should go on the same work queue as the pnp thread to
    // avoid an extra context switch.
    //

    State->IdleTimerWorkItem = KeCreateWorkItem(NULL,
                                                WorkPriorityNormal,
                                                PmpDeviceIdleWorker,
                                                Device,
                                                PM_ALLOCATION_TAG);

    State->IdleTimerDpc = KeCreateDpc(PmpDeviceIdleTimerDpc,
                                      State->IdleTimerWorkItem);

    State->Irp = IoCreateIrp(Device, IrpMajorStateChange, 0);
    State->History = PmpCreateIdleHistory(0, PM_DEVICE_HISTORY_SIZE_SHIFT);
    if ((State->ActiveEvent == NULL) || (State->IdleTimer == NULL) ||
        (State->IdleTimerDpc == NULL) || (State->IdleTimerWorkItem == NULL) ||
        (State->Irp == NULL) || (State->History == NULL)) {

        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceEnd;
    }

    Status = STATUS_SUCCESS;

InitializeDeviceEnd:
    if (!KSUCCESS(Status)) {
        PmpDestroyDevice(Device);
    }

    return Status;
}

VOID
PmpDeviceIdleTimerDpc (
    PDPC Dpc
    )

/*++

Routine Description:

    This routine is called at dispatch level when the device's idle timer
    expires.

Arguments:

    Dpc - Supplies a pointer to the DPC that is running.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    //
    // The user data for the DPC is the work item itself, which is important
    // since the power state structure is paged and cannot be touched here.
    //

    Status = KeQueueWorkItem(Dpc->UserData);

    ASSERT(KSUCCESS(Status));

    return;
}

VOID
PmpDeviceIdleWorker (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine implements the work item queued when a device's idle timer
    expires.

Arguments:

    Parameter - Supplies a context parameter, which in this case is a pointer
        to the device.

Return Value:

    None.

--*/

{

    ULONGLONG CurrentTime;
    PDEVICE Device;
    PDEVICE_POWER State;
    KSTATUS Status;
    ULONGLONG Timeout;
    ULONG TimerQueued;

    Device = Parameter;
    State = Device->Power;

    //
    // The timer gets left on a lot, even if it's no longer needed. If there
    // are references on the device now, just do nothing.
    //

    if (State->ReferenceCount != 0) {
        RtlAtomicExchange32(&(State->TimerQueued), 0);

        //
        // Assuming there are still references, then this is done.
        //

        if (State->ReferenceCount != 0) {
            return;

        //
        // The references went away, but may have lost the race to queue the
        // timer. Try to reclaim the right to queue the timer. If someone else
        // got there first, then the timer is requeued and doesn't need
        // the rest of this routine.
        //

        } else {
            TimerQueued = RtlAtomicCompareExchange32(&(State->TimerQueued),
                                                     1,
                                                     0);

            if (TimerQueued == 0) {
                return;
            }
        }
    }

    //
    // If the idle timeout has moved beyond the current time, then re-queue
    // the timer for that new time.
    //

    CurrentTime = HlQueryTimeCounter();
    Timeout = State->IdleTimeout;
    if (CurrentTime < Timeout) {
        Status = KeQueueTimer(State->IdleTimer,
                              TimerQueueSoftWake,
                              Timeout,
                              0,
                              0,
                              State->IdleTimerDpc);

        if (!KSUCCESS(Status)) {
            RtlAtomicExchange32(&(State->TimerQueued), 0);
        }

    //
    // The idle timer really did expire. Reset the timer queued variable, and
    // queue the idle transition.
    //

    } else {
        RtlAtomicExchange32(&(State->TimerQueued), 0);
        Status = PmpDeviceQueuePowerTransition(Device, DevicePowerRequestIdle);
        if (!KSUCCESS(Status)) {
            RtlDebugPrint("PM: Failed to queue idle work: %x %x\n",
                          Device,
                          Status);
        }
    }

    return;
}

VOID
PmpDeviceDecrementActiveChildren (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine decrements the active child count on a given device.

Arguments:

    Device - Supplies a pointer to the device to subtract an active child count
        from.

Return Value:

    None.

--*/

{

    UINTN PreviousCount;
    PDEVICE_POWER State;

    State = Device->Power;
    if (State == NULL) {
        return;
    }

    PreviousCount = RtlAtomicAdd(&(State->ActiveChildren), -1);

    ASSERT((PreviousCount != 0) && (PreviousCount < 0x10000000));

    //
    // If this is the first active child, add a power reference on this device.
    //

    if (PreviousCount == 1) {
        PmDeviceReleaseReference(Device);
    }

    return;
}

KSTATUS
PmpDeviceAddReference (
    PDEVICE Device,
    DEVICE_POWER_REQUEST Request
    )

/*++

Routine Description:

    This routine asynchronously adds a power management reference on the given
    device.

Arguments:

    Device - Supplies a pointer to the device to add a power reference to.

    Request - Supplies the request type, which can either be actually becoming
        active or just marking it as such.

Return Value:

    Status code.

--*/

{

    UINTN PreviousCount;
    PDEVICE_POWER State;
    KSTATUS Status;

    State = Device->Power;
    PreviousCount = RtlAtomicAdd(&(State->ReferenceCount), 1);

    ASSERT(PreviousCount < 0x10000000);

    //
    // If there are already other references or active children, the device is
    // already active.
    //

    if (PreviousCount != 0) {
        return STATUS_SUCCESS;
    }

    State->Request = Request;
    Status = PmpDeviceResume(Device);
    if (!KSUCCESS(Status)) {
        RtlAtomicAdd(&(State->ReferenceCount), -1);
    }

    return Status;
}

KSTATUS
PmpDeviceQueuePowerTransition (
    PDEVICE Device,
    DEVICE_POWER_REQUEST Request
    )

/*++

Routine Description:

    This routine queues a power request on a device.

Arguments:

    Device - Supplies a pointer to the device to affect.

    Request - Supplies the type of request to queue.

Return Value:

    Status code.

--*/

{

    BOOL QueueRequest;
    PDEVICE_POWER State;
    KSTATUS Status;

    State = Device->Power;
    Status = STATUS_SUCCESS;

    //
    // Do a quick exit for resuming a device that's not idle.
    //

    if ((Request == DevicePowerRequestResume) &&
        (State->State == DevicePowerStateActive)) {

        return STATUS_SUCCESS;
    }

    //
    // If the state is already what it should be and there are no other
    // requests, also exit.
    //

    KeAcquireSharedExclusiveLockExclusive(Device->Lock);
    QueueRequest = FALSE;

    //
    // Don't bother if the same request is already queued.
    //

    if ((State->State != DevicePowerStateRemoved) &&
        ((State->State != DevicePowerStateTransitioning) ||
         (State->Request != Request))) {

        switch (Request) {

        //
        // Resume trumps all other requests.
        //

        case DevicePowerRequestResume:
        case DevicePowerRequestMarkActive:
            if (State->State != DevicePowerStateActive) {
                State->Request = Request;
                QueueRequest = TRUE;
                KeSignalEvent(State->ActiveEvent, SignalOptionUnsignal);
            }

            break;

        //
        // Suspend trumps idle.
        //

        case DevicePowerRequestSuspend:
            if (State->State != DevicePowerStateSuspended) {
                if ((State->Request != DevicePowerRequestResume) &&
                    (State->Request != DevicePowerRequestMarkActive)) {

                    State->Request = Request;
                    QueueRequest = TRUE;
                }
            }

            break;

        //
        // Idle only happens if nothing else is going on.
        //

        case DevicePowerRequestIdle:
            if (State->State != DevicePowerStateIdle) {
                if (State->Request == DevicePowerRequestNone) {
                    State->Request = Request;
                    QueueRequest = TRUE;
                }
            }

            break;

        default:

            ASSERT(FALSE);

            break;
        }
    }

    //
    // If needed, actually queue the work request.
    //

    if (QueueRequest != FALSE) {
        if (State->State != DevicePowerStateTransitioning) {
            State->PreviousState = State->State;
        }

        State->State = DevicePowerStateTransitioning;
        Status = IopQueueDeviceWork(Device,
                                    DeviceActionPowerTransition,
                                    NULL,
                                    0);
    }

    KeReleaseSharedExclusiveLockExclusive(Device->Lock);
    return Status;
}

KSTATUS
PmpDeviceResume (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine performs the actual resume action for a given device.

Arguments:

    Device - Supplies a pointer to the device to resume.

Return Value:

    Status code.

--*/

{

    ULONGLONG CurrentTime;
    ULONGLONG Duration;
    PIRP Irp;
    PDEVICE Parent;
    PDEVICE_POWER ParentState;
    UINTN PreviousCount;
    DEVICE_POWER_REQUEST Request;
    PDEVICE_POWER State;
    KSTATUS Status;

    //
    // Do a quick exit if this request is stale.
    //

    State = Device->Power;
    Request = State->Request;
    State->Request = DevicePowerRequestNone;
    if (State->State == DevicePowerStateActive) {
        return STATUS_SUCCESS;
    }

    CurrentTime = HlQueryTimeCounter();

    //
    // Nothing should override a resume request.
    //

    ASSERT((Request == DevicePowerRequestResume) ||
           (Request == DevicePowerRequestMarkActive));

    //
    // First resume the parent recursively.
    //

    Parent = Device->ParentDevice;
    ParentState = Parent->Power;
    if (ParentState != NULL) {
        PreviousCount = RtlAtomicAdd(&(ParentState->ActiveChildren), 1);

        ASSERT(PreviousCount < 0x10000000);

        //
        // If this is the first active child, up the reference count on the
        // device.
        //

        if (PreviousCount == 0) {
            PreviousCount = RtlAtomicAdd(&(ParentState->ReferenceCount), 1);

            ASSERT(PreviousCount < 0x10000000);

            //
            // If this was the first power reference on this device, resume that
            // device, recursing up parents as needed.
            //

            if (PreviousCount == 0) {
                Parent->Power->Request = DevicePowerRequestResume;
                Status = PmpDeviceResume(Parent);
                if (!KSUCCESS(Status)) {

                    //
                    // This may not work all the way, leaving the reference
                    // counts screwy.
                    //

                    PmpDeviceDecrementActiveChildren(Device->ParentDevice);
                    RtlDebugPrint("PM: Failed to resume %x: %x\n",
                                  Parent,
                                  Status);

                    return Status;
                }
            }
        }
    }

    State = Device->Power;
    KeAcquireSharedExclusiveLockExclusive(Device->Lock);
    if (State->State == DevicePowerStateRemoved) {
        Status = STATUS_DEVICE_NOT_CONNECTED;
        goto DeviceResumeEnd;
    }

    //
    // If the device was transitioning but never made it, then an extra parent
    // reference count was taken (as idle/suspend will return early and not
    // release it).
    //

    if ((State->State == DevicePowerStateTransitioning) &&
        (State->PreviousState == DevicePowerStateActive)) {

        ASSERT((ParentState == NULL) || (ParentState->ActiveChildren > 1));

        PmpDeviceDecrementActiveChildren(Parent);
    }

    if (Request == DevicePowerRequestResume) {
        Irp = State->Irp;
        IoInitializeIrp(Irp);
        Irp->MinorCode = IrpMinorResume;
        Status = IoSendSynchronousIrp(Irp);
        if (KSUCCESS(Status)) {
            Status = IoGetIrpStatus(Irp);
        }

    } else {

        ASSERT(Request == DevicePowerRequestMarkActive);

        Status = STATUS_SUCCESS;
    }

    if (PmDebugPowerTransitions != FALSE) {
        RtlDebugPrint("PM: %x Active: %x\n", Device, Status);
    }

    if (KSUCCESS(Status)) {

        //
        // If the device just switched from idle to active, then compute the
        // idle duration.
        //

        if (State->State == DevicePowerStateIdle) {

            ASSERT(State->TransitionTime != 0);

            Duration = CurrentTime - State->TransitionTime;
            PmpIdleHistoryAddDataPoint(State->History, Duration);
        }

        State->State = DevicePowerStateActive;
        State->TransitionTime = CurrentTime;

    } else {
        RtlDebugPrint("PM: Failed to resume %x: %x\n", Device, Status);
        State->State = DevicePowerStateSuspended;
    }

    KeSignalEvent(State->ActiveEvent, SignalOptionSignalAll);

DeviceResumeEnd:
    KeReleaseSharedExclusiveLockExclusive(Device->Lock);

    //
    // If it failed, release the references taken on the parent.
    //

    if (!KSUCCESS(Status)) {
        PmpDeviceDecrementActiveChildren(Device->ParentDevice);
    }

    return Status;
}

VOID
PmpDeviceIdle (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine performs the actual idle action for a given device.

Arguments:

    Device - Supplies a pointer to the device to resume.

Return Value:

    None.

--*/

{

    ULONGLONG CurrentTime;
    PIRP Irp;
    ULONG Milliseconds;
    PDEVICE_POWER State;
    KSTATUS Status;

    //
    // Exit quickly if there are references now, which there often will be.
    //

    State = Device->Power;
    if (State->ReferenceCount != 0) {
        return;
    }

    Status = STATUS_UNSUCCESSFUL;
    KeAcquireSharedExclusiveLockExclusive(Device->Lock);
    if (State->State == DevicePowerStateRemoved) {
        Status = STATUS_DEVICE_NOT_CONNECTED;
        goto DeviceIdleEnd;
    }

    //
    // Unsignal the event now so that there isn't a window in between
    // 1) Checking the references here, and
    // 2) Unsignaling the event
    // where an add reference call could zoom through, get past the event, and
    // then get in trouble when this lumbering idle finally rolls through. It
    // means add reference calls may get stuck briefly while this routine
    // figures out it's not needed.
    //

    KeSignalEvent(State->ActiveEvent, SignalOptionUnsignal);

    //
    // Do nothing if it turns out this request was stale.
    //

    if ((State->State != DevicePowerStateTransitioning) ||
        (State->Request != DevicePowerRequestIdle) ||
        (State->ReferenceCount != 0)) {

        if (State->State == DevicePowerStateActive) {
            KeSignalEvent(State->ActiveEvent, SignalOptionSignalAll);
        }

        Status = STATUS_SUCCESS;
        goto DeviceIdleEnd;
    }

    Irp = State->Irp;
    IoInitializeIrp(Irp);
    Irp->MinorCode = IrpMinorIdle;
    Irp->U.Idle.ExpectedDuration = PmpIdleHistoryGetAverage(State->History);
    Status = IoSendSynchronousIrp(Irp);
    if (KSUCCESS(Status)) {
        Status = IoGetIrpStatus(Irp);
    }

    if (PmDebugPowerTransitions != FALSE) {
        Milliseconds = (Irp->U.Idle.ExpectedDuration * 1000ULL) /
                       HlQueryTimeCounterFrequency();

        RtlDebugPrint("PM: %x Idle (%d ms): %x\n",
                      Device,
                      Milliseconds,
                      Status);
    }

    State->Request = DevicePowerRequestNone;
    if (KSUCCESS(Status)) {
        CurrentTime = HlQueryTimeCounter();
        State->State = DevicePowerStateIdle;
        State->TransitionTime = CurrentTime;
    }

DeviceIdleEnd:
    KeReleaseSharedExclusiveLockExclusive(Device->Lock);

    //
    // If the device was put down, then decrement the active child count of
    // the parent.
    //

    if (KSUCCESS(Status)) {
        PmpDeviceDecrementActiveChildren(Device->ParentDevice);
    }

    return;
}

VOID
PmpDeviceSuspend (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine performs the actual device suspension.

Arguments:

    Device - Supplies a pointer to the device to resume.

Return Value:

    None.

--*/

{

    PIRP Irp;
    PDEVICE_POWER State;
    KSTATUS Status;

    //
    // Exit quickly if there are references now, which there often will be.
    //

    State = Device->Power;
    KeAcquireSharedExclusiveLockExclusive(Device->Lock);
    if (State->State == DevicePowerStateRemoved) {
        Status = STATUS_DEVICE_NOT_CONNECTED;
        goto DeviceSuspendEnd;
    }

    KeSignalEvent(State->ActiveEvent, SignalOptionUnsignal);

    //
    // Do nothing if it turns out this request was stale.
    //

    if ((State->State != DevicePowerStateTransitioning) ||
        (State->Request != DevicePowerRequestSuspend)) {

        if (State->State == DevicePowerStateActive) {
            KeSignalEvent(State->ActiveEvent, SignalOptionSignalAll);
        }

        Status = STATUS_SUCCESS;
        goto DeviceSuspendEnd;
    }

    Irp = State->Irp;
    IoInitializeIrp(Irp);
    Irp->MinorCode = IrpMinorSuspend;
    Status = IoSendSynchronousIrp(Irp);
    if (KSUCCESS(Status)) {
        Status = IoGetIrpStatus(Irp);
    }

    if (PmDebugPowerTransitions != FALSE) {
        RtlDebugPrint("PM: %x Suspend: %x\n", Device, Status);
    }

    State->Request = DevicePowerRequestNone;
    if (KSUCCESS(Status)) {
        State->State = DevicePowerStateSuspended;
    }

DeviceSuspendEnd:
    KeReleaseSharedExclusiveLockExclusive(Device->Lock);

    //
    // If the device was put down, then decrement the active child count of
    // the parent.
    //

    if (KSUCCESS(Status)) {
        PmpDeviceDecrementActiveChildren(Device->ParentDevice);
    }

    return;
}

