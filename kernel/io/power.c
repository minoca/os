/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

#include <minoca/kernel/kernel.h>
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
    PDEVICE Device,
    DEVICE_POWER_REQUEST Request
    );

VOID
PmpDeviceIdle (
    PDEVICE Device
    );

VOID
PmpDeviceSuspend (
    PDEVICE Device
    );

VOID
PmpStartIdleTimer (
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

    return PmpDeviceAddReference(Device, DevicePowerRequestResume);
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

    ASSERT(State != NULL);

    PreviousCount = RtlAtomicAdd(&(State->ReferenceCount), 1);

    ASSERT(PreviousCount < 0x10000000);

    if (PreviousCount != 0) {
        return STATUS_SUCCESS;
    }

    Status = PmpDeviceQueuePowerTransition(Device, DevicePowerRequestResume);
    if (!KSUCCESS(Status)) {
        RtlAtomicAdd(&(State->ReferenceCount), -1);
    }

    return Status;
}

KERNEL_API
VOID
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

    None.

--*/

{

    UINTN PreviousCount;
    PDEVICE_POWER State;

    State = Device->Power;

    ASSERT(State != NULL);

    PreviousCount = RtlAtomicAdd(&(State->ReferenceCount), -1);

    ASSERT((PreviousCount != 0) && (PreviousCount < 0x10000000));

    if (PreviousCount > 1) {
        return;
    }

    //
    // Bump up the idle deadline even if the timer is already queued. The
    // timer will see this and requeue itself.
    //

    State->IdleTimeout = HlQueryTimeCounter() + State->IdleDelay;
    PmpStartIdleTimer(Device);
    return;
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
            break;
        }

        //
        // Add a reference on the device to bring it up, then release that
        // reference to send it down towards sleepytown.
        //

        Status = PmpDeviceAddReference(Device,
                                       DevicePowerRequestMarkActive);

        if (KSUCCESS(Status)) {
            PmDeviceReleaseReference(Device);
        }

        break;

    case DevicePowerStateSuspended:
        KeAcquireQueuedLock(State->Lock);
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

        KeReleaseQueuedLock(State->Lock);
        break;

    case DevicePowerStateIdle:
    default:

        ASSERT(FALSE);

        Status = STATUS_INVALID_PARAMETER;
        break;
    }

    return Status;
}

KSTATUS
PmInitializeLibrary (
    VOID
    )

/*++

Routine Description:

    This routine performs global initialization for the power management
    library. It is called towards the end of I/O initialization.

Arguments:

    None.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    Status = PmpArchInitialize();
    if (!KSUCCESS(Status)) {
        goto InitializeLibraryEnd;
    }

InitializeLibraryEnd:
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

    DEVICE_POWER_STATE OldPreviousState;
    DEVICE_POWER_STATE OldState;
    PDEVICE_POWER State;

    State = Device->Power;
    if (State == NULL) {
        return;
    }

    //
    // A transition to the removed state is effective immediately, but must be
    // synchronized with all other transitions.
    //

    KeAcquireQueuedLock(State->Lock);
    OldState = State->State;
    OldPreviousState = State->PreviousState;
    State->State = DevicePowerStateRemoved;
    State->Request = DevicePowerRequestNone;
    RtlAtomicExchange32(&(State->TimerQueued), 1);
    KeCancelTimer(State->IdleTimer);
    KeCancelDpc(State->IdleTimerDpc);
    KeCancelWorkItem(State->IdleTimerWorkItem);
    if (OldState != DevicePowerStateTransitioning) {
        State->PreviousState = OldState;
    }

    KeReleaseQueuedLock(State->Lock);

    //
    // If an active child was just removed, decrement the parent's count.
    //

    if ((OldState == DevicePowerStateActive) ||
        ((OldState == DevicePowerStateTransitioning) &&
         (OldPreviousState == DevicePowerStateActive))) {

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

        if (State->Lock != NULL) {
            KeDestroyQueuedLock(State->Lock);
        }

        if (State->Irp != NULL) {
            IoDestroyIrp(State->Irp);
        }

        if (State->History != NULL) {
            PmpDestroyIdleHistory(State->History);
        }

        Device->Power = NULL;
        MmFreeNonPagedPool(State);
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

        //
        // It is OK to do a second unprotected read of the state's request.
        // When a resume or activate request is set, no other request can trump
        // it, not even another resume or active (as only one thread grabs the
        // first reference on the device and starts the resume process).
        //

        case DevicePowerRequestResume:
        case DevicePowerRequestMarkActive:
            PmpDeviceResume(Device, State->Request);
            break;

        default:
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

    PDEVICE_POWER Power;
    KSTATUS Status;

    Power = MmAllocateNonPagedPool(sizeof(DEVICE_POWER),
                                   PM_DEVICE_ALLOCATION_TAG);

    if (Power == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Power, sizeof(DEVICE_POWER));
    Device->Power = Power;
    Power->State = DevicePowerStateSuspended;
    Power->IdleDelay = HlQueryTimeCounterFrequency() *
                       PM_INITIAL_IDLE_DELAY_SECONDS;

    Power->Lock = KeCreateQueuedLock();
    Power->ActiveEvent = KeCreateEvent(NULL);
    Power->IdleTimer = KeCreateTimer(PM_DEVICE_ALLOCATION_TAG);

    //
    // This work item should go on the same work queue as the device worker
    // thread to avoid an extra context switch.
    //

    Power->IdleTimerWorkItem = KeCreateWorkItem(IoDeviceWorkQueue,
                                                WorkPriorityNormal,
                                                PmpDeviceIdleWorker,
                                                Device,
                                                PM_DEVICE_ALLOCATION_TAG);

    Power->IdleTimerDpc = KeCreateDpc(PmpDeviceIdleTimerDpc,
                                      Power->IdleTimerWorkItem);

    Power->Irp = IoCreateIrp(Device, IrpMajorStateChange, 0);
    Power->History = PmpCreateIdleHistory(IDLE_HISTORY_NON_PAGED,
                                          PM_DEVICE_HISTORY_SIZE_SHIFT);

    if ((Power->Lock == NULL) ||
        (Power->ActiveEvent == NULL) ||
        (Power->IdleTimer == NULL) ||
        (Power->IdleTimerDpc == NULL) ||
        (Power->IdleTimerWorkItem == NULL) ||
        (Power->Irp == NULL) ||
        (Power->History == NULL)) {

        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceEnd;
    }

    //
    // Start the active event as unsignaled since the device is in the
    // suspended state.
    //

    KeSignalEvent(Power->ActiveEvent, SignalOptionUnsignal);
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

    Device = Parameter;
    State = Device->Power;

    //
    // The timer is no longer queued. After this, calls to release the final
    // reference will attempt to requeue the timer.
    //

    RtlAtomicExchange32(&(State->TimerQueued), 0);

    //
    // The timer gets left on a lot, even if it's no longer needed. If there
    // are references on the device now, just do nothing.
    //

    if (State->ReferenceCount != 0) {
        return;
    }

    //
    // If the idle timeout has moved beyond the current time, then re-queue
    // the timer for that new time.
    //

    CurrentTime = HlQueryTimeCounter();
    Timeout = State->IdleTimeout;
    if (CurrentTime < Timeout) {
        PmpStartIdleTimer(Device);

    //
    // The idle timer really did expire. Queue the idle transition.
    //

    } else {
        Status = PmpDeviceQueuePowerTransition(Device, DevicePowerRequestIdle);
        if (!KSUCCESS(Status)) {
            RtlDebugPrint("PM: Failed to queue idle work: 0x%x %d\n",
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
    // If this is the first active child, release a power reference on this
    // device.
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

    Status = STATUS_SUCCESS;
    State = Device->Power;

    ASSERT(State != NULL);

    PreviousCount = RtlAtomicAdd(&(State->ReferenceCount), 1);

    ASSERT(PreviousCount < 0x10000000);

    //
    // Attempt to the resume the device if this is the first reference.
    //

    if (PreviousCount == 0) {
        Status = PmpDeviceResume(Device, Request);
        if (!KSUCCESS(Status)) {
            RtlAtomicAdd(&(State->ReferenceCount), -1);
        }

    //
    // If the state is active, then go ahead. Otherwise, wait for the ready
    // event.
    //

    } else {
        if (Device->Power->State != DevicePowerStateActive) {
            KeWaitForEvent(Device->Power->ActiveEvent,
                           FALSE,
                           WAIT_TIME_INDEFINITE);

            if (Device->Power->State != DevicePowerStateActive) {
                RtlAtomicAdd(&(State->ReferenceCount), -1);
                Status = STATUS_NOT_READY;
            }
        }
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

    QueueRequest = FALSE;
    KeAcquireQueuedLock(State->Lock);

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
    // If a request is needed, set the state correctly while the lock is held.
    //

    if (QueueRequest != FALSE) {
        if (State->State != DevicePowerStateTransitioning) {
            State->PreviousState = State->State;
        }

        KeSignalEvent(State->ActiveEvent, SignalOptionUnsignal);
        State->State = DevicePowerStateTransitioning;
    }

    KeReleaseQueuedLock(State->Lock);

    //
    // If needed, actually queue the work request now that the lock is released.
    //

    if (QueueRequest != FALSE) {
        Status = IopQueueDeviceWork(Device,
                                    DeviceActionPowerTransition,
                                    NULL,
                                    0);

        //
        // If queueing the work fails, then attempt to transition the state
        // back to what it was. There may already be an item on the queue and
        // the request may still run, but there is no guarantee of that. The
        // state must be rolled back.
        //

        if (!KSUCCESS(Status)) {
            KeAcquireQueuedLock(State->Lock);

            //
            // If the request is still set, then roll back the state. If it's
            // not, then there is a subsequent attempt at queueing action that
            // may well succeed.
            //

            if (Request == State->Request) {
                State->State = State->PreviousState;
            }

            KeReleaseQueuedLock(State->Lock);

            //
            // If this is a failed resume action, then signal the event. Other
            // threads may be waiting on the event for the resume to succeed.
            //

            if ((Request == DevicePowerRequestResume) ||
                (Request == DevicePowerRequestMarkActive)) {

                KeSignalEvent(State->ActiveEvent, SignalOptionSignalAll);
            }
        }
    }

    return Status;
}

KSTATUS
PmpDeviceResume (
    PDEVICE Device,
    DEVICE_POWER_REQUEST Request
    )

/*++

Routine Description:

    This routine performs the actual resume action for a given device.

Arguments:

    Device - Supplies a pointer to the device to resume.

    Request - Supplies the type of resume to request.

Return Value:

    Status code.

--*/

{

    ULONGLONG CurrentTime;
    ULONGLONG Duration;
    PIRP Irp;
    BOOL LockHeld;
    PDEVICE Parent;
    PDEVICE_POWER ParentState;
    UINTN PreviousCount;
    PDEVICE_POWER State;
    KSTATUS Status;

    ASSERT((Request == DevicePowerRequestResume) ||
           (Request == DevicePowerRequestMarkActive));

    //
    // If the state isn't already active, then the caller won the race to
    // transition it out of an idle or suspended state by being the first to
    // increment the device's reference count. As such, other threads may be
    // waiting on the active event. Except for this case where the device is
    // already active, this routine always needs to release others waiting on
    // the resume transition.
    //

    State = Device->Power;
    if (State->State == DevicePowerStateActive) {
        return STATUS_SUCCESS;
    }

    LockHeld = FALSE;
    CurrentTime = HlQueryTimeCounter();

    //
    // First resume the parent recursively. Always resume the parent, even if
    // the initial request was to mark the device active. The parent is not
    // necessarily resumed.
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
            // If this was the first power reference on this device, resume
            // that device, recursing up parents as needed.
            //

            if (PreviousCount == 0) {
                Status = PmpDeviceResume(Parent, DevicePowerRequestResume);
                if (!KSUCCESS(Status)) {
                    goto DeviceResumeEnd;
                }
            }
        }

        //
        // Wait until the parent's state settles. If this thread does not set
        // the active child count to 1 or the reference count to 1, then
        // another thread is doing the work and the device is not resumed until
        // the active event is signaled. Fail the resume transition if the
        // parent doesn't make it into the resumed state.
        //

        KeWaitForEvent(ParentState->ActiveEvent, FALSE, WAIT_TIME_INDEFINITE);
        if (ParentState->State != DevicePowerStateActive) {
            Status = STATUS_NOT_READY;
            goto DeviceResumeEnd;
        }
    }

    //
    // Synchronize the transition to the active state with other requests and
    // work items that might be trying to send the device to idle or suspend.
    //

    KeAcquireQueuedLock(State->Lock);
    LockHeld = TRUE;

    ASSERT(State->State != DevicePowerStateActive);

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
        Status = STATUS_SUCCESS;

    //
    // Actually ask the driver to resume. The if case above prevents the resume
    // from being sent if the idle/suspend request never actually got sent.
    //

    } else if (Request == DevicePowerRequestResume) {
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
        RtlDebugPrint("PM: 0x%08x Active: %d\n", Device, Status);
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

        if (State->State != DevicePowerStateTransitioning) {
            State->PreviousState = State->State;
        }

        State->State = DevicePowerStateActive;
        State->TransitionTime = CurrentTime;

        //
        // Smash any outstanding request state. Now that the device is active
        // again, any request associated with a transition is stale.
        //

        State->Request = DevicePowerRequestNone;

    //
    // On failure, the state is either transitioning (with a request), idle, or
    // suspended. Let the device stay idle or suspended and keep any pending
    // transition unless it is a resume transition.
    //

    } else {

        ASSERT((State->State == DevicePowerStateIdle) ||
               (State->State == DevicePowerStateSuspended) ||
               ((State->State == DevicePowerStateTransitioning) &&
                (State->Request != DevicePowerRequestNone)));

        if ((State->State == DevicePowerStateTransitioning) &&
            ((State->Request == DevicePowerRequestResume) ||
             (State->Request == DevicePowerRequestMarkActive))) {

            ASSERT(State->PreviousState != DevicePowerStateTransitioning);
            ASSERT(State->PreviousState != DevicePowerStateActive);

            State->State = State->PreviousState;
            State->Request = DevicePowerRequestNone;
        }
    }

DeviceResumeEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(State->Lock);
    }

    //
    // Signal the event to release any threads waiting on the resume transition.
    // They need to check the state when they wake up in case the resume
    // failed.
    //

    KeSignalEvent(State->ActiveEvent, SignalOptionSignalAll);

    //
    // If it failed, release the references taken on the parent.
    //

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("PM: Failed to resume 0x%08x: %d\n", Device, Status);
        PmpDeviceDecrementActiveChildren(Parent);
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
    BOOL DecrementParent;
    PIRP Irp;
    ULONG Milliseconds;
    PDEVICE_POWER State;
    KSTATUS Status;

    State = Device->Power;

    //
    // If someone else has added a reference AND successfully killed the idle
    // transition, then exit quickly. (If there's a reference but the idle
    // transition has not been killed, then this routine will have to cancel
    // the idle request. This might happen if an add reference zooms through
    // just before the state is set to transitioning.)
    //

    if (State->ReferenceCount != 0) {
        if ((State->State != DevicePowerStateTransitioning) ||
            (State->Request != DevicePowerRequestIdle)) {

            return;
        }
    }

    DecrementParent = FALSE;
    Status = STATUS_UNSUCCESSFUL;
    KeAcquireQueuedLock(State->Lock);
    if (State->State == DevicePowerStateRemoved) {
        goto DeviceIdleEnd;
    }

    //
    // Do nothing if it turns out this request was stale.
    //

    if ((State->State != DevicePowerStateTransitioning) ||
        (State->Request != DevicePowerRequestIdle)) {

        goto DeviceIdleEnd;

    //
    // A reference might have come in before the state was set to transitioning,
    // in which case add reference would just exit and continue. Cancel the
    // transition the way resume was supposed to so the state doesn't get stuck
    // as transitioning.
    //

    } else if (State->ReferenceCount != 0) {

        ASSERT(State->PreviousState == DevicePowerStateActive);

        State->State = State->PreviousState;
        State->Request = DevicePowerRequestNone;
        KeSignalEvent(State->ActiveEvent, SignalOptionSignalAll);
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

        RtlDebugPrint("PM: 0x%x Idle (%d ms): %d\n",
                      Device,
                      Milliseconds,
                      Status);
    }

    ASSERT(State->PreviousState == DevicePowerStateActive);

    if (KSUCCESS(Status)) {
        CurrentTime = HlQueryTimeCounter();
        State->State = DevicePowerStateIdle;
        State->TransitionTime = CurrentTime;
        DecrementParent = TRUE;

    } else {
        State->State = State->PreviousState;
    }

    //
    // Success or failure, this request is old news. No additional idle
    // requests could have been queued while this one was in flight. And this
    // routine bails earlier if the request type is anything other than idle.
    //

    State->Request = DevicePowerRequestNone;

DeviceIdleEnd:

    //
    // If the device is active because a resume happened before the idle or the
    // idle failed, wake up everything waiting on the active event.
    //

    if (State->State == DevicePowerStateActive) {
        KeSignalEvent(State->ActiveEvent, SignalOptionSignalAll);
    }

    KeReleaseQueuedLock(State->Lock);

    //
    // If the device was put down, then decrement the active child count of
    // the parent. It moved to the idle state from the active state, which held
    // a reference on the parent.
    //

    if (DecrementParent != FALSE) {
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

    BOOL DecrementParent;
    PIRP Irp;
    PDEVICE_POWER State;
    KSTATUS Status;

    DecrementParent = FALSE;
    State = Device->Power;
    KeAcquireQueuedLock(State->Lock);
    if (State->State == DevicePowerStateRemoved) {
        goto DeviceSuspendEnd;
    }

    //
    // Do nothing if it turns out this request was stale.
    //

    if ((State->State != DevicePowerStateTransitioning) ||
        (State->Request != DevicePowerRequestSuspend)) {

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
        RtlDebugPrint("PM: 0x%x Suspend: %d\n", Device, Status);
    }

    ASSERT((State->PreviousState == DevicePowerStateActive) ||
           (State->PreviousState == DevicePowerStateIdle));

    if (KSUCCESS(Status)) {
        State->State = DevicePowerStateSuspended;
        if (State->PreviousState == DevicePowerStateActive) {
            DecrementParent = TRUE;
        }

    } else {
        State->State = State->PreviousState;
    }

    //
    // Success or failure, this request is old news. No additional suspend
    // requests could have been queued while this one was in flight. And this
    // routine bails earlier if the request type is anything other than suspend.
    //

    State->Request = DevicePowerRequestNone;

DeviceSuspendEnd:

    //
    // If the device is active because a resume happened before the suspend or
    // the suspend failed to transition from active to suspended, wake up
    // everything waiting on the active event.
    //

    if (State->State == DevicePowerStateActive) {
        KeSignalEvent(State->ActiveEvent, SignalOptionSignalAll);
    }

    KeReleaseQueuedLock(State->Lock);

    //
    // If the device was put down, then decrement the active child count of
    // the parent. This only needs to happen if the previous state was the
    // active state. The device may have already been idle, in which case it
    // would not have held a reference on its parent.
    //

    if (DecrementParent != FALSE) {
        PmpDeviceDecrementActiveChildren(Device->ParentDevice);
    }

    return;
}

VOID
PmpStartIdleTimer (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine queues the device's idle timer if it has not already been
    queued.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    PDEVICE_POWER PowerState;
    KSTATUS Status;
    ULONG TimerQueued;

    PowerState = Device->Power;
    if (PowerState->TimerQueued != 0) {
        return;
    }

    //
    // Try to win the race to queue the timer.
    //

    TimerQueued = RtlAtomicCompareExchange32(&(PowerState->TimerQueued), 1, 0);
    if (TimerQueued == 0) {
        Status = KeQueueTimer(PowerState->IdleTimer,
                              TimerQueueSoftWake,
                              PowerState->IdleTimeout,
                              0,
                              0,
                              PowerState->IdleTimerDpc);

        if (!KSUCCESS(Status)) {
            RtlAtomicExchange32(&(PowerState->TimerQueued), 0);
            RtlDebugPrint("PM: Cannot queue idle timer: device 0x%x: %d\n",
                          Device,
                          Status);
        }
    }

    return;
}

