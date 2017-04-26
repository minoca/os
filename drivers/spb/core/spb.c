/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    spb.c

Abstract:

    This module implements support for the Simple Peripheral Bus support
    library driver.

Author:

    Evan Green 14-Aug-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "spbp.h"

//
// ---------------------------------------------------------------- Definitions
//

#define SPB_CONTROLLER_INFORMATION_MAX_VERSION 0x1000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
SpbDriverUnload (
    PVOID Driver
    );

KSTATUS
SpbOpen (
    PSPB_INTERFACE Interface,
    PRESOURCE_SPB_DATA Configuration,
    PSPB_HANDLE Handle
    );

VOID
SpbClose (
    PSPB_INTERFACE Interface,
    SPB_HANDLE Handle
    );

KSTATUS
SpbSetConfiguration (
    SPB_HANDLE Handle,
    PRESOURCE_SPB_DATA Configuration
    );

VOID
SpbLockBus (
    SPB_HANDLE Handle
    );

VOID
SpbUnlockBus (
    SPB_HANDLE Handle
    );

KSTATUS
SpbSubmitTransferSet (
    SPB_HANDLE Handle,
    PSPB_TRANSFER_SET TransferSet
    );

KSTATUS
SpbExecuteTransferSet (
    SPB_HANDLE Handle,
    PSPB_TRANSFER_SET TransferSet
    );

KSTATUS
SpbpExecuteTransferSet (
    PSPB_CONTROLLER Controller,
    PSPB_TRANSFER_SET TransferSet
    );

VOID
SpbpCompleteTransferSet (
    PSPB_CONTROLLER Controller,
    PSPB_TRANSFER_SET TransferSet,
    KSTATUS Status
    );

VOID
SpbpSynchronousTransferCompletionCallback (
    PSPB_TRANSFER_SET TransferSet
    );

//
// -------------------------------------------------------------------- Globals
//

UUID SpbInterfaceUuid = UUID_SPB_INTERFACE;

SPB_INTERFACE SpbInterfaceTemplate = {
    NULL,
    SpbOpen,
    SpbClose,
    SpbSetConfiguration,
    SpbLockBus,
    SpbUnlockBus,
    SpbSubmitTransferSet,
    SpbExecuteTransferSet
};

//
// ------------------------------------------------------------------ Functions
//

__USED
KSTATUS
DriverEntry (
    PDRIVER Driver
    )

/*++

Routine Description:

    This routine implements the initial entry point of the SPB core
    library, called when the library is first loaded.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    Status code.

--*/

{

    DRIVER_FUNCTION_TABLE FunctionTable;
    KSTATUS Status;

    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.Unload = SpbDriverUnload;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

SPB_API
KSTATUS
SpbCreateController (
    PSPB_CONTROLLER_INFORMATION Registration,
    PSPB_CONTROLLER *Controller
    )

/*++

Routine Description:

    This routine creates a new Simple Peripheral Bus controller.

Arguments:

    Registration - Supplies a pointer to the host registration information.

    Controller - Supplies a pointer where a pointer to the new controller will
        be returned on success.

Return Value:

    Status code.

--*/

{

    UINTN AllocationSize;
    PSPB_CONTROLLER NewController;
    KSTATUS Status;

    if ((Registration->Version < SPB_CONTROLLER_INFORMATION_VERSION) ||
        (Registration->Version > SPB_CONTROLLER_INFORMATION_MAX_VERSION) ||
        (Registration->BusType <= ResourceSpbBusInvalid) ||
        (Registration->BusType >= ResourceSpbBusTypeCount) ||
        (Registration->Device == NULL)) {

        return STATUS_INVALID_PARAMETER;
    }

    AllocationSize = sizeof(SPB_CONTROLLER);
    NewController = MmAllocatePagedPool(AllocationSize, SPB_ALLOCATION_TAG);
    if (NewController == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateControllerEnd;
    }

    RtlZeroMemory(NewController, AllocationSize);
    RtlCopyMemory(&(NewController->Host),
                  Registration,
                  sizeof(SPB_CONTROLLER_INFORMATION));

    RtlCopyMemory(&(NewController->Interface),
                  &SpbInterfaceTemplate,
                  sizeof(SPB_INTERFACE));

    NewController->Magic = SPB_CONTROLLER_MAGIC;
    INITIALIZE_LIST_HEAD(&(NewController->HandleList));
    INITIALIZE_LIST_HEAD(&(NewController->TransferQueue));
    NewController->Lock = KeCreateQueuedLock();
    if (NewController->Lock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateControllerEnd;
    }

    NewController->BusLock = KeCreateQueuedLock();
    if (NewController->BusLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateControllerEnd;
    }

    Status = STATUS_SUCCESS;

CreateControllerEnd:
    if (!KSUCCESS(Status)) {
        if (NewController != NULL) {
            if (NewController->Lock != NULL) {
                KeDestroyQueuedLock(NewController->Lock);
            }

            if (NewController->BusLock != NULL) {
                KeDestroyQueuedLock(NewController->BusLock);
            }

            MmFreePagedPool(NewController);
            NewController = NULL;
        }
    }

    *Controller = NewController;
    return Status;
}

SPB_API
VOID
SpbDestroyController (
    PSPB_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine destroys a Simple Peripheral Bus controller.

Arguments:

    Controller - Supplies a pointer to the controller to tear down.

Return Value:

    None.

--*/

{

    ASSERT(LIST_EMPTY(&(Controller->HandleList)));

    if (Controller->Lock != NULL) {
        KeDestroyQueuedLock(Controller->Lock);
    }

    if (Controller->BusLock != NULL) {
        KeDestroyQueuedLock(Controller->BusLock);
    }

    //
    // Ruin the magic (but in a way that's still identifiable to a human).
    //

    Controller->Magic += 1;
    MmFreePagedPool(Controller);
    return;
}

SPB_API
KSTATUS
SpbStartController (
    PSPB_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine starts a Simple Peripheral Bus controller.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    PSPB_CONTROLLER_INFORMATION Host;
    KSTATUS Status;

    ASSERT(Controller->Interface.Context == NULL);
    ASSERT(KeGetRunLevel() == RunLevelLow);

    KeAcquireQueuedLock(Controller->Lock);
    Host = &(Controller->Host);
    Controller->Interface.Context = &(Controller->Interface);
    Status = IoCreateInterface(&SpbInterfaceUuid,
                               Host->Device,
                               &(Controller->Interface),
                               sizeof(SPB_INTERFACE));

    if (!KSUCCESS(Status)) {
        Controller->Interface.Context = NULL;
        goto StartControllerEnd;
    }

    //
    // Create a resource arbiter for these pins so that other devices can
    // allocate them as part of their official resource requirements.
    //

    if (Controller->ArbiterCreated == FALSE) {
        Status = IoCreateResourceArbiter(Host->Device, ResourceTypeSimpleBus);
        if ((!KSUCCESS(Status)) && (Status != STATUS_ALREADY_INITIALIZED)) {
            goto StartControllerEnd;
        }

        Status = IoAddFreeSpaceToArbiter(Host->Device,
                                         ResourceTypeSimpleBus,
                                         0,
                                         -1ULL,
                                         0,
                                         NULL,
                                         0);

        if (!KSUCCESS(Status)) {
            goto StartControllerEnd;
        }

        Controller->ArbiterCreated = TRUE;
    }

StartControllerEnd:
    KeReleaseQueuedLock(Controller->Lock);
    return Status;
}

SPB_API
VOID
SpbStopController (
    PSPB_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine stops a Simple Peripheral Bus controller.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    ASSERT(Controller->Interface.Context == &(Controller->Interface));
    ASSERT(KeGetRunLevel() == RunLevelLow);

    KeAcquireQueuedLock(Controller->Lock);
    Status = IoDestroyInterface(&SpbInterfaceUuid,
                                Controller->Host.Device,
                                &(Controller->Interface));

    ASSERT(KSUCCESS(Status));

    Controller->Interface.Context = NULL;

    ASSERT(LIST_EMPTY(&(Controller->HandleList)));

    KeReleaseQueuedLock(Controller->Lock);
    return;
}

SPB_API
PSPB_TRANSFER
SpbTransferCompletion (
    PSPB_CONTROLLER Controller,
    PSPB_TRANSFER Transfer,
    KSTATUS Status
    )

/*++

Routine Description:

    This routine is called by an SPB host controller when a transfer has
    completed.

Arguments:

    Controller - Supplies a pointer to the controller.

    Transfer - Supplies a pointer to the transfer that completed.

    Status - Supplies the status code the transfer completed with.

Return Value:

    Returns a new transfer to begin executing if there are additional transfers
    in this set and the previous transfer completed successfully.

    NULL if no new transfers should be started at this time.

--*/

{

    PSPB_TRANSFER NextTransfer;

    ASSERT(Controller->CurrentSet != NULL);

    NextTransfer = NULL;
    Controller->CurrentSet->EntriesProcessed += 1;

    //
    // On failure or if this is the last transfer, complete the whole set.
    //

    if ((!KSUCCESS(Status)) ||
        (Transfer->ListEntry.Next == &(Controller->CurrentSet->TransferList))) {

        SpbpCompleteTransferSet(Controller, Controller->CurrentSet, Status);

    } else {
        NextTransfer = LIST_VALUE(Transfer->ListEntry.Next,
                                  SPB_TRANSFER,
                                  ListEntry);

        NextTransfer->Flags &= ~SPB_TRANSFER_FLAG_AUTO_MASK;
        if (NextTransfer->ListEntry.Next ==
            &(Controller->CurrentSet->TransferList)) {

            NextTransfer->Flags |= SPB_TRANSFER_FLAG_LAST;
        }
    }

    return NextTransfer;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
SpbDriverUnload (
    PVOID Driver
    )

/*++

Routine Description:

    This routine is called before a driver is about to be unloaded from memory.
    The driver should take this opportunity to free any resources it may have
    set up in the driver entry routine.

Arguments:

    Driver - Supplies a pointer to the driver being torn down.

Return Value:

    None.

--*/

{

    return;
}

KSTATUS
SpbOpen (
    PSPB_INTERFACE Interface,
    PRESOURCE_SPB_DATA Configuration,
    PSPB_HANDLE Handle
    )

/*++

Routine Description:

    This routine opens a new connection to a Simple Peripheral Bus.

Arguments:

    Interface - Supplies a pointer to the interface instance, used to identify
        which specific bus is being opened.

    Configuration - Supplies a pointer to the configuration data that specifies
        bus specific configuration parameters.

    Handle - Supplies a pointer where a handle will be returned on success
        representing the connection to the device.

Return Value:

    Status code.

--*/

{

    PSPB_CONTROLLER Controller;
    PSPB_HANDLE_DATA HandleData;
    KSTATUS Status;

    Controller = PARENT_STRUCTURE(Interface->Context,
                                  SPB_CONTROLLER,
                                  Interface);

    ASSERT(Controller->Magic == SPB_CONTROLLER_MAGIC);

    HandleData = MmAllocatePagedPool(sizeof(SPB_HANDLE_DATA),
                                     SPB_ALLOCATION_TAG);

    if (HandleData == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto OpenEnd;
    }

    RtlZeroMemory(HandleData, sizeof(SPB_HANDLE_DATA));
    HandleData->Magic = SPB_HANDLE_MAGIC;
    HandleData->Controller = Controller;
    Status = SpbSetConfiguration(HandleData, Configuration);
    if (!KSUCCESS(Status)) {
        goto OpenEnd;
    }

    KeAcquireQueuedLock(Controller->Lock);
    INSERT_BEFORE(&(HandleData->ListEntry), &(Controller->HandleList));
    KeReleaseQueuedLock(Controller->Lock);

OpenEnd:
    if (!KSUCCESS(Status)) {
        if (HandleData != NULL) {

            ASSERT(HandleData->Configuration == NULL);

            MmFreePagedPool(HandleData);
            HandleData = NULL;
        }
    }

    *Handle = HandleData;
    return Status;
}

VOID
SpbClose (
    PSPB_INTERFACE Interface,
    SPB_HANDLE Handle
    )

/*++

Routine Description:

    This routine closes a previously opened to a Simple Peripheral Bus.

Arguments:

    Interface - Supplies a pointer to the interface instance, used to identify
        which specific bus is being operated on.

    Handle - Supplies the open handle to close.

Return Value:

    None.

--*/

{

    PSPB_CONTROLLER Controller;
    PSPB_HANDLE_DATA HandleData;

    HandleData = Handle;
    Controller = HandleData->Controller;

    ASSERT(HandleData->Magic == SPB_HANDLE_MAGIC);
    ASSERT(HandleData->BusReferenceCount == 0);
    ASSERT(Controller ==
           PARENT_STRUCTURE(Interface, SPB_CONTROLLER, Interface));

    if (HandleData->Event != NULL) {
        KeDestroyEvent(HandleData->Event);
    }

    KeAcquireQueuedLock(Controller->Lock);
    LIST_REMOVE(&(HandleData->ListEntry));
    if (Controller->CurrentConfiguration == HandleData->Configuration) {
        Controller->CurrentConfiguration = NULL;
    }

    KeReleaseQueuedLock(Controller->Lock);
    HandleData->ListEntry.Next = NULL;
    if (HandleData->Configuration != NULL) {
        MmFreePagedPool(HandleData->Configuration);
    }

    HandleData->Magic += 1;
    MmFreePagedPool(HandleData);
    return;
}

KSTATUS
SpbSetConfiguration (
    SPB_HANDLE Handle,
    PRESOURCE_SPB_DATA Configuration
    )

/*++

Routine Description:

    This routine writes a new set of bus parameters to the bus.

Arguments:

    Handle - Supplies the open handle to change configuration of.

    Configuration - Supplies the new configuration to set.

Return Value:

    Status code.

--*/

{

    PSPB_CONTROLLER Controller;
    PSPB_HANDLE_DATA HandleData;
    PVOID NewData;
    PVOID OldData;

    HandleData = Handle;
    Controller = HandleData->Controller;

    ASSERT(HandleData->Magic == SPB_HANDLE_MAGIC);

    //
    // Perform some checks against accidental misconfiguration. This isn't
    // nearly a bulletproof set of checks.
    //

    if ((Configuration == NULL) ||
        (Configuration->Size < sizeof(RESOURCE_SPB_DATA)) ||
        (Configuration->VendorDataSize >
         Configuration->Size - sizeof(RESOURCE_SPB_DATA)) ||
        (Configuration->BusType != HandleData->Controller->Host.BusType)) {

        return STATUS_INVALID_PARAMETER;
    }

    NewData = MmAllocatePagedPool(Configuration->Size, SPB_ALLOCATION_TAG);
    if (NewData == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyMemory(NewData, Configuration, Configuration->Size);
    KeAcquireQueuedLock(Controller->Lock);
    OldData = HandleData->Configuration;
    if (OldData != NULL) {
        if (Controller->CurrentConfiguration == OldData) {
            Controller->CurrentConfiguration = NULL;
        }
    }

    HandleData->Configuration = NewData;
    KeReleaseQueuedLock(Controller->Lock);
    if (OldData != NULL) {
        MmFreePagedPool(OldData);
    }

    return STATUS_SUCCESS;
}

VOID
SpbLockBus (
    SPB_HANDLE Handle
    )

/*++

Routine Description:

    This routine locks the bus so that this handle may perform a sequence of
    accesses without being interrupted.

Arguments:

    Handle - Supplies the open handle to the bus to lock.

Return Value:

    None.

--*/

{

    PSPB_CONTROLLER Controller;
    PSPB_HANDLE_DATA HandleData;
    PSPB_CONTROLLER_INFORMATION Host;
    ULONG OldValue;

    HandleData = Handle;

    ASSERT(HandleData->Magic == SPB_HANDLE_MAGIC);

    OldValue = RtlAtomicAdd32(&(HandleData->BusReferenceCount), 1);

    ASSERT(OldValue < 0x1000);

    if (OldValue == 0) {
        KeAcquireQueuedLock(HandleData->Controller->BusLock);
        Controller = HandleData->Controller;
        Host = &(Controller->Host);
        if (Host->FunctionTable.LockBus != NULL) {
            Host->FunctionTable.LockBus(Host->Context,
                                        HandleData->Configuration);
        }
    }

    return;
}

VOID
SpbUnlockBus (
    SPB_HANDLE Handle
    )

/*++

Routine Description:

    This routine unlocks a bus that was previously locked with the lock
    function.

Arguments:

    Handle - Supplies the open handle to the bus to unlock. The caller must
        have previously locked the bus.

Return Value:

    None.

--*/

{

    PSPB_CONTROLLER Controller;
    PSPB_HANDLE_DATA HandleData;
    PSPB_CONTROLLER_INFORMATION Host;
    PSPB_HANDLE_DATA NextHandleData;
    PSPB_TRANSFER_SET NextSet;
    ULONG OldValue;

    HandleData = Handle;
    Controller = HandleData->Controller;

    ASSERT(HandleData->Magic == SPB_HANDLE_MAGIC);

    OldValue = RtlAtomicAdd32(&(HandleData->BusReferenceCount), -1);

    ASSERT((OldValue != 0) && (OldValue < 0x1000));

    if (OldValue == 1) {
        Host = &(Controller->Host);

        //
        // Let the host know the bus is being unlocked.
        //

        if (Host->FunctionTable.UnlockBus != NULL) {
            Host->FunctionTable.UnlockBus(Host->Context);
        }

        //
        // If there are more items on the transfer queue and someone else
        // hasn't already started executing them, fire it off now. Do an
        // initial unsynchronized check to avoid acquiring the lock if possible.
        //

        NextSet = NULL;
        if ((Controller->CurrentSet == NULL) &&
            (!LIST_EMPTY(&(Controller->TransferQueue)))) {

            KeAcquireQueuedLock(Controller->Lock);
            if ((Controller->CurrentSet == NULL) &&
                (!LIST_EMPTY(&(Controller->TransferQueue)))) {

                NextSet = LIST_VALUE(Controller->TransferQueue.Next,
                                     SPB_TRANSFER_SET,
                                     ListEntry);

                Controller->CurrentSet = NextSet;

                //
                // Leave the actual lock acquired the whole time, and just
                // transfer the reference to the next handle.
                //

                HandleData = NextSet->Handle;
                RtlAtomicAdd32(&(HandleData->BusReferenceCount), 1);

                //
                // The host was told the bus was unlocked, so it needs to be
                // told its actually still locked.
                //

                if (Host->FunctionTable.LockBus != NULL) {
                    NextHandleData = NextSet->Handle;
                    Host->FunctionTable.LockBus(Host->Context,
                                                NextHandleData->Configuration);
                }

                KeReleaseQueuedLock(Controller->Lock);
                SpbpExecuteTransferSet(Controller, NextSet);

            } else {
                KeReleaseQueuedLock(Controller->Lock);
            }
        }

        //
        // If the bus lock wasn't transferred to another handle, then really
        // release the bus lock.
        //

        if (NextSet == NULL) {
            KeReleaseQueuedLock(Controller->BusLock);
        }
    }

    return;
}

KSTATUS
SpbSubmitTransferSet (
    SPB_HANDLE Handle,
    PSPB_TRANSFER_SET TransferSet
    )

/*++

Routine Description:

    This routine submits a set of transfers to the bus for execution. This
    routine will ensure that other devices do not perform transfers while any
    transfer in this set is in progress. The submission is asynchronous, this
    routine will return immediately, and the callback function will be called
    when the transfer is complete.

Arguments:

    Handle - Supplies the open handle to the bus to unlock.

    TransferSet - Supplies a pointer to the transfer set to execute.

Return Value:

    Status code. This routine will return immediately, the transfer will not
    have been complete. The caller should utilize the callback function to get
    notified when a transfer has completed.

--*/

{

    PSPB_CONTROLLER Controller;
    BOOL ExecuteTransfer;
    PSPB_HANDLE_DATA HandleData;
    KSTATUS Status;

    HandleData = Handle;
    if (HandleData->Configuration == NULL) {
        return STATUS_NOT_CONFIGURED;
    }

    ASSERT(HandleData->Magic == SPB_HANDLE_MAGIC);
    ASSERT(TransferSet->ListEntry.Next == NULL);

    Controller = HandleData->Controller;
    TransferSet->Handle = Handle;
    TransferSet->EntriesProcessed = 0;
    TransferSet->Status = STATUS_NOT_HANDLED;
    ExecuteTransfer = FALSE;
    KeAcquireQueuedLock(Controller->Lock);
    if (Controller->CurrentSet == NULL) {
        ExecuteTransfer = TRUE;
        Controller->CurrentSet = TransferSet;
    }

    INSERT_BEFORE(&(TransferSet->ListEntry), &(Controller->TransferQueue));
    KeReleaseQueuedLock(Controller->Lock);

    //
    // If this was the first item on an empty list, then kick off the party.
    //

    Status = STATUS_SUCCESS;
    if (ExecuteTransfer != FALSE) {
        SpbLockBus(TransferSet->Handle);
        Status = SpbpExecuteTransferSet(Controller, TransferSet);
    }

    return Status;
}

KSTATUS
SpbExecuteTransferSet (
    SPB_HANDLE Handle,
    PSPB_TRANSFER_SET TransferSet
    )

/*++

Routine Description:

    This routine submits a set of transfers to the bus for execution. This
    routine will ensure that other devices do not perform transfers while any
    transfer in this set is in progress. This routine is synchronous, it will
    not return until the transfer is complete.

Arguments:

    Handle - Supplies the open handle to the bus to unlock.

    TransferSet - Supplies a pointer to the transfer set to execute.

Return Value:

    Status code indicating completion status of the transfer. This routine will
    not return until the transfer is complete (or failed).

--*/

{

    PKEVENT Event;
    PSPB_HANDLE_DATA HandleData;
    KSTATUS Status;

    //
    // Create an event for the handle if there isn't one already. This is not
    // thread-safe, it is expected only one synchronous transfer will be
    // submitted at a time.
    //

    HandleData = Handle;
    if (HandleData->Event == NULL) {
        Event = KeCreateEvent(NULL);
        if (Event == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        ASSERT(HandleData->Event == NULL);

        HandleData->Event = Event;
    }

    Event = HandleData->Event;

    ASSERT(Event != NULL);
    ASSERT((TransferSet->CompletionRoutine == NULL) &&
           (TransferSet->Context == NULL));

    KeSignalEvent(Event, SignalOptionUnsignal);
    TransferSet->CompletionRoutine = SpbpSynchronousTransferCompletionCallback;
    TransferSet->Context = Event;
    Status = SpbSubmitTransferSet(Handle, TransferSet);
    if (KSUCCESS(Status)) {
        KeWaitForEvent(Event, FALSE, WAIT_TIME_INDEFINITE);
        TransferSet->CompletionRoutine = NULL;
        TransferSet->Context = NULL;
        Status = TransferSet->Status;
    }

    return Status;
}

KSTATUS
SpbpExecuteTransferSet (
    PSPB_CONTROLLER Controller,
    PSPB_TRANSFER_SET TransferSet
    )

/*++

Routine Description:

    This routine begins execution of a new transfer set. It is assumed that the
    bus lock is already held.

Arguments:

    Controller - Supplies a pointer to the controller to execute on.

    TransferSet - Supplies a pointer to the transfer set to execute.

Return Value:

    Status code. This routine will return immediately, the transfer will not
    have necessarily been completed. The caller should utilize the callback
    function to get notified when a transfer has completed.

--*/

{

    PSPB_HANDLE_DATA HandleData;
    KSTATUS Status;
    PSPB_TRANSFER Transfer;

    HandleData = TransferSet->Handle;

    ASSERT(HandleData->BusReferenceCount != 0);
    ASSERT(HandleData->Configuration != NULL);
    ASSERT(Controller->CurrentSet == TransferSet);

    //
    // Configure the bus if its configuration does not match what the handle
    // needs.
    //

    if (Controller->CurrentConfiguration != HandleData->Configuration) {
        Status = Controller->Host.FunctionTable.Configure(
                                                    Controller->Host.Context,
                                                    HandleData->Configuration);

        if (!KSUCCESS(Status)) {
            goto ExecuteTransferSetEnd;
        }

        Controller->CurrentConfiguration = HandleData->Configuration;
    }

    //
    // Execute the first transfer, or just complete the transfer if there are
    // none (bus configuration only).
    //

    if (LIST_EMPTY(&(TransferSet->TransferList))) {
        Status = STATUS_SUCCESS;
        SpbpCompleteTransferSet(Controller, TransferSet, Status);

    } else {
        Transfer = LIST_VALUE(TransferSet->TransferList.Next,
                              SPB_TRANSFER,
                              ListEntry);

        Transfer->Flags &= ~SPB_TRANSFER_FLAG_AUTO_MASK;
        Transfer->Flags |= SPB_TRANSFER_FLAG_FIRST;
        Status = Controller->Host.FunctionTable.SubmitTransfer(
                                                      Controller->Host.Context,
                                                      Transfer);

        if (!KSUCCESS(Status)) {
            goto ExecuteTransferSetEnd;
        }
    }

ExecuteTransferSetEnd:
    if (!KSUCCESS(Status)) {
        SpbpCompleteTransferSet(Controller, TransferSet, Status);
    }

    return Status;
}

VOID
SpbpCompleteTransferSet (
    PSPB_CONTROLLER Controller,
    PSPB_TRANSFER_SET TransferSet,
    KSTATUS Status
    )

/*++

Routine Description:

    This routine completes a transfer set. It is called with the bus lock held,
    and may release the bus lock.

Arguments:

    Controller - Supplies a pointer to the controller that owns the transfer
        set.

    TransferSet - Supplies a pointer to the transfer set to complete.

    Status - Supplies the completion status code.

Return Value:

    None.

--*/

{

    ASSERT(Controller->CurrentSet == TransferSet);

    Controller->CurrentSet = NULL;
    TransferSet->Status = Status;
    KeAcquireQueuedLock(Controller->Lock);
    LIST_REMOVE(&(TransferSet->ListEntry));
    TransferSet->ListEntry.Next = NULL;
    KeReleaseQueuedLock(Controller->Lock);

    //
    // Unlock the bus before calling the completion routine because the
    // transfer set can disappear as soon as the completion routine is called.
    //

    SpbUnlockBus(TransferSet->Handle);
    if (TransferSet->CompletionRoutine != NULL) {
        TransferSet->CompletionRoutine(TransferSet);
    }

    return;
}

VOID
SpbpSynchronousTransferCompletionCallback (
    PSPB_TRANSFER_SET TransferSet
    )

/*++

Routine Description:

    This routine is called when a transfer set has completed or errored out.

Arguments:

    TransferSet - Supplies a pointer to the transfer set that completed.

Return Value:

    None.

--*/

{

    KeSignalEvent(TransferSet->Context, SignalOptionSignalAll);
    return;
}

