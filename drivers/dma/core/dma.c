/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dma.c

Abstract:

    This module implements common infrastructure support for DMA controller
    drivers.

Author:

    Evan Green 1-Feb-2016

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "dmap.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
DmaDriverUnload (
    PVOID Driver
    );

KSTATUS
DmaGetInformation (
    PDMA_INTERFACE Interface,
    PDMA_INFORMATION Information
    );

KSTATUS
DmaSubmit (
    PDMA_INTERFACE Interface,
    PDMA_TRANSFER Transfer
    );

KSTATUS
DmaCancel (
    PDMA_INTERFACE Interface,
    PDMA_TRANSFER Transfer
    );

KSTATUS
DmaControlRequest (
    PDMA_INTERFACE Interface,
    PDMA_TRANSFER Transfer,
    PVOID Request,
    UINTN RequestSize
    );

KSTATUS
DmaAllocateTransfer (
    PDMA_INTERFACE Interface,
    PDMA_TRANSFER *Transfer
    );

VOID
DmaFreeTransfer (
    PDMA_INTERFACE Interface,
    PDMA_TRANSFER Transfer
    );

RUNLEVEL
DmapAcquireChannelLock (
    PDMA_CONTROLLER Controller,
    PDMA_CHANNEL Channel
    );

VOID
DmapReleaseChannelLock (
    PDMA_CONTROLLER Controller,
    PDMA_CHANNEL Channel,
    RUNLEVEL OldRunLevel
    );

//
// -------------------------------------------------------------------- Globals
//

UUID DmaInterfaceUuid = UUID_DMA_INTERFACE;

DMA_INTERFACE DmaInterfaceTemplate = {
    NULL,
    DmaGetInformation,
    DmaSubmit,
    DmaCancel,
    DmaControlRequest,
    DmaAllocateTransfer,
    DmaFreeTransfer
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

    This routine implements the initial entry point of the DMA core
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
    FunctionTable.Unload = DmaDriverUnload;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

DMA_API
KSTATUS
DmaCreateController (
    PDMA_CONTROLLER_INFORMATION Registration,
    PDMA_CONTROLLER *Controller
    )

/*++

Routine Description:

    This routine creates a new Direct Memory Access controller.

Arguments:

    Registration - Supplies a pointer to the host registration information.

    Controller - Supplies a pointer where a pointer to the new controller will
        be returned on success.

Return Value:

    Status code.

--*/

{

    UINTN AllocationSize;
    PDMA_CHANNEL Channel;
    ULONG ChannelCount;
    ULONG ChannelIndex;
    PDMA_CONTROLLER NewController;
    KSTATUS Status;

    if ((Registration->Version < DMA_CONTROLLER_INFORMATION_VERSION) ||
        (Registration->Version > DMA_CONTROLLER_INFORMATION_MAX_VERSION) ||
        (Registration->Device == NULL)) {

        return STATUS_INVALID_PARAMETER;
    }

    ChannelCount = Registration->Information.ChannelCount;
    AllocationSize = sizeof(DMA_CONTROLLER) +
                     (ChannelCount * sizeof(DMA_CHANNEL));

    NewController = MmAllocateNonPagedPool(AllocationSize, DMA_ALLOCATION_TAG);
    if (NewController == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateControllerEnd;
    }

    RtlZeroMemory(NewController, AllocationSize);
    RtlCopyMemory(&(NewController->Host),
                  Registration,
                  sizeof(DMA_CONTROLLER_INFORMATION));

    RtlCopyMemory(&(NewController->Interface),
                  &DmaInterfaceTemplate,
                  sizeof(DMA_INTERFACE));

    NewController->Magic = DMA_CONTROLLER_MAGIC;
    NewController->ChannelCount = ChannelCount;
    NewController->Channels = (PDMA_CHANNEL)(NewController + 1);
    for (ChannelIndex = 0; ChannelIndex < ChannelCount; ChannelIndex += 1) {
        Channel = &(NewController->Channels[ChannelIndex]);
        INITIALIZE_LIST_HEAD(&(Channel->Queue));
        KeInitializeSpinLock(&(Channel->Lock));
    }

    Status = STATUS_SUCCESS;

CreateControllerEnd:
    if (!KSUCCESS(Status)) {
        if (NewController != NULL) {
            MmFreeNonPagedPool(NewController);
            NewController = NULL;
        }
    }

    *Controller = NewController;
    return Status;
}

DMA_API
VOID
DmaDestroyController (
    PDMA_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine destroys a Direct Memory Access controller.

Arguments:

    Controller - Supplies a pointer to the controller to tear down.

Return Value:

    None.

--*/

{

    PDMA_CHANNEL Channel;
    ULONG ChannelIndex;

    for (ChannelIndex = 0;
         ChannelIndex < Controller->ChannelCount;
         ChannelIndex += 1) {

        Channel = &(Controller->Channels[ChannelIndex]);

        ASSERT((Channel->Transfer == NULL) &&
               ((Channel->Queue.Next == NULL) ||
                (LIST_EMPTY(&(Channel->Queue)))));
    }

    //
    // Ruin the magic (but in a way that's still identifiable to a human).
    //

    Controller->Magic += 1;
    MmFreeNonPagedPool(Controller);
    return;
}

DMA_API
KSTATUS
DmaStartController (
    PDMA_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine starts a Direct Memory Access controller. This function is
    not thread safe, as it is meant to be called during the start IRP, which is
    always serialized.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    PDMA_CONTROLLER_INFORMATION Host;
    KSTATUS Status;

    ASSERT(Controller->Interface.Context == NULL);
    ASSERT(KeGetRunLevel() == RunLevelLow);

    Host = &(Controller->Host);
    Controller->Interface.Context = Controller;
    Status = IoCreateInterface(&DmaInterfaceUuid,
                               Host->Device,
                               &(Controller->Interface),
                               sizeof(DMA_INTERFACE));

    if (!KSUCCESS(Status)) {
        Controller->Interface.Context = NULL;
        goto StartControllerEnd;
    }

    //
    // Create a resource arbiter for these pins so that other devices can
    // allocate them as part of their official resource requirements.
    //

    if (Controller->ArbiterCreated == FALSE) {
        Status = IoCreateResourceArbiter(Host->Device, ResourceTypeDmaChannel);
        if ((!KSUCCESS(Status)) && (Status != STATUS_ALREADY_INITIALIZED)) {
            goto StartControllerEnd;
        }

        Status = IoAddFreeSpaceToArbiter(Host->Device,
                                         ResourceTypeDmaChannel,
                                         0,
                                         Controller->ChannelCount,
                                         0,
                                         NULL,
                                         0);

        if (!KSUCCESS(Status)) {
            goto StartControllerEnd;
        }

        Controller->ArbiterCreated = TRUE;
    }

StartControllerEnd:
    return Status;
}

DMA_API
VOID
DmaStopController (
    PDMA_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine stops a Direct Memory Access controller. This function is not
    thread safe, as it is meant to be called during a state transition IRP,
    which is always serialized.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    ASSERT(Controller->Interface.Context == &(Controller->Interface));
    ASSERT(KeGetRunLevel() == RunLevelLow);

    Status = IoDestroyInterface(&DmaInterfaceUuid,
                                Controller->Host.Device,
                                &(Controller->Interface));

    ASSERT(KSUCCESS(Status));

    Controller->Interface.Context = NULL;
    return;
}

DMA_API
PDMA_TRANSFER
DmaTransferCompletion (
    PDMA_CONTROLLER Controller,
    PDMA_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine is called by a DMA host controller when a transfer has
    completed. This function must be called at or below dispatch level. The
    host should have already filled in the number of bytes completed and the
    status.

Arguments:

    Controller - Supplies a pointer to the controller.

    Transfer - Supplies a pointer to the transfer that completed.

Return Value:

    Returns a pointer to the next transfer to start.

    NULL if no more transfers are queued.

--*/

{

    PDMA_CHANNEL Channel;
    PDMA_TRANSFER NextTransfer;
    RUNLEVEL OldRunLevel;

    ASSERT(Transfer->Allocation->Allocation < Controller->ChannelCount);
    ASSERT(Transfer->ListEntry.Next == NULL);

    NextTransfer = NULL;
    Channel = &(Controller->Channels[Transfer->Allocation->Allocation]);
    OldRunLevel = DmapAcquireChannelLock(Controller, Channel);

    ASSERT(Channel->Transfer == Transfer);

    Channel->Transfer = NULL;
    if (!LIST_EMPTY(&(Channel->Queue))) {
        NextTransfer = LIST_VALUE(Channel->Queue.Next, DMA_TRANSFER, ListEntry);
        LIST_REMOVE(&(NextTransfer->ListEntry));
        NextTransfer->ListEntry.Next = NULL;
        Channel->Transfer = NextTransfer;
    }

    DmapReleaseChannelLock(Controller, Channel, OldRunLevel);
    Transfer->CompletionCallback(Transfer);
    return NextTransfer;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
DmaDriverUnload (
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
DmaGetInformation (
    PDMA_INTERFACE Interface,
    PDMA_INFORMATION Information
    )

/*++

Routine Description:

    This routine returns information about a given DMA controller.

Arguments:

    Interface - Supplies a pointer to the interface instance, used to identify
        which specific controller is being queried.

    Information - Supplies a pointer where the DMA controller information is
        returned on success. The caller should initialize the version number of
        this structure.

Return Value:

    Status code.

--*/

{

    PDMA_CONTROLLER Controller;

    Controller = Interface->Context;
    if (Information == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    if ((Information->Version == 0) ||
        (Information->Version > DMA_INFORMATION_MAX_VERSION) ||
        (Information->Version < Controller->Host.Information.Version)) {

        Information->Version = Controller->Host.Information.Version;
        return STATUS_VERSION_MISMATCH;
    }

    if (Controller->Host.Information.Version == DMA_INFORMATION_VERSION) {
        RtlCopyMemory(Information,
                      &(Controller->Host.Information),
                      sizeof(DMA_INFORMATION));

    } else {
        return STATUS_VERSION_MISMATCH;
    }

    return STATUS_SUCCESS;
}

KSTATUS
DmaSubmit (
    PDMA_INTERFACE Interface,
    PDMA_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine submits a transfer to the DMA controller for execution. This
    routine will ensure that other devices do not perform transfers on the
    given channel while this transfer is in progress. The submission is
    asynchronous, this routine will return immediately, and the callback
    function will be called when the transfer is complete.

Arguments:

    Interface - Supplies a pointer to the DMA controller interface.

    Transfer - Supplies a pointer to the transfer to execute.

Return Value:

    Status code. This routine will return immediately, the transfer will not
    have been complete. The caller should utilize the callback function to get
    notified when a transfer has completed.

--*/

{

    PDMA_CHANNEL Channel;
    PDMA_CONTROLLER Controller;
    PRESOURCE_DMA_DATA DmaAllocation;
    ULONG Mask;
    RUNLEVEL OldRunLevel;
    KSTATUS Status;
    ULONG Width;

    Controller = Interface->Context;
    if ((Transfer->Allocation == NULL) ||
        (Transfer->Allocation->Allocation >= Controller->ChannelCount) ||
        (Transfer->Memory == NULL) ||
        (Transfer->CompletionCallback == NULL) ||
        (((Transfer->Flags & DMA_TRANSFER_CONTINUOUS) != 0) &&
         ((Controller->Host.Information.Capabilities &
           DMA_CAPABILITY_CONTINUOUS_MODE) == 0))) {

        return STATUS_INVALID_PARAMETER;
    }

    Transfer->Status = STATUS_NOT_STARTED;

    //
    // Try to figure out the width based on the resource allocation.
    //

    if (Transfer->Width == 0) {

        //
        // Grab the custom one if there is one.
        //

        if (((Transfer->Allocation->Characteristics &
              DMA_TRANSFER_SIZE_CUSTOM) != 0) &&
            (Transfer->Allocation->Data != NULL) &&
            (Transfer->Allocation->DataSize >= sizeof(RESOURCE_DMA_DATA))) {

            DmaAllocation = Transfer->Allocation->Data;
            Transfer->Width = DmaAllocation->Width;

        //
        // Try to find the width based on one of the characteristics flags.
        //

        } else {
            Mask = DMA_TRANSFER_SIZE_256;
            Width = 256;
            while (Mask >= DMA_TRANSFER_SIZE_8) {
                if ((Transfer->Allocation->Characteristics & Mask) != 0) {
                    Transfer->Width = Width;
                    break;
                }

                Width >>= 1;
                Mask >>= 1;
            }
        }
    }

    if (Transfer->Width == 0) {
        return STATUS_INVALID_CONFIGURATION;
    }

    ASSERT(Transfer->ListEntry.Next == NULL);

    Channel = &(Controller->Channels[Transfer->Allocation->Allocation]);
    OldRunLevel = DmapAcquireChannelLock(Controller, Channel);
    if (Channel->Transfer == NULL) {
        Channel->Transfer = Transfer;
        Transfer->ListEntry.Next = NULL;

    } else {
        INSERT_BEFORE(&(Transfer->ListEntry), &(Channel->Queue));
        Transfer = NULL;
    }

    DmapReleaseChannelLock(Controller, Channel, OldRunLevel);
    Status = STATUS_SUCCESS;

    //
    // If the transfer wasn't queued, kick it off now.
    //

    if (Transfer != NULL) {
        Status = Controller->Host.FunctionTable.SubmitTransfer(
                                                      Controller->Host.Context,
                                                      Transfer);
    }

    return Status;
}

KSTATUS
DmaCancel (
    PDMA_INTERFACE Interface,
    PDMA_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine attempts to cancel a transfer that is currently in flight.

Arguments:

    Interface - Supplies a pointer to the DMA controller interface.

    Transfer - Supplies a pointer to the transfer to cancel.

Return Value:

    STATUS_SUCCESS if the transfer was successfully canceled.

    STATUS_TOO_LATE if the transfer is already complete.

    Other status codes on other failures.

--*/

{

    PDMA_CHANNEL Channel;
    PDMA_CONTROLLER Controller;
    PDMA_TRANSFER NextTransfer;
    RUNLEVEL OldRunLevel;
    KSTATUS Status;
    KSTATUS SubmitStatus;

    Controller = Interface->Context;
    if ((Transfer->Allocation == NULL) ||
        (Transfer->Allocation->Allocation >= Controller->ChannelCount)) {

        return STATUS_INVALID_PARAMETER;
    }

    Channel = &(Controller->Channels[Transfer->Allocation->Allocation]);
    NextTransfer = NULL;
    OldRunLevel = DmapAcquireChannelLock(Controller, Channel);
    if (Channel->Transfer == Transfer) {
        Status = Controller->Host.FunctionTable.CancelTransfer(
                                                      Controller->Host.Context,
                                                      Transfer);

        if (KSUCCESS(Status)) {

            ASSERT(Channel->Transfer == Transfer);

            Channel->Transfer = NULL;

            //
            // Kick off the next transfer if this one was canceled and there is
            // more left to do.
            //

            if (!LIST_EMPTY(&(Channel->Queue))) {
                NextTransfer = LIST_VALUE(Channel->Queue.Next,
                                          DMA_TRANSFER,
                                          ListEntry);

                LIST_REMOVE(&(NextTransfer->ListEntry));
                NextTransfer->ListEntry.Next = NULL;
                Channel->Transfer = NextTransfer;
            }
        }

    } else if (Transfer->ListEntry.Next != NULL) {
        LIST_REMOVE(&(Transfer->ListEntry));
        Transfer->ListEntry.Next = NULL;
        Status = STATUS_SUCCESS;

    } else {
        Status = STATUS_TOO_LATE;
    }

    DmapReleaseChannelLock(Controller, Channel, OldRunLevel);

    //
    // If there's a next transfer, try to submit that. If that one fails,
    // process its completion and potentially submit the next one. Loop until
    // either a transfer is successfully submitted or there is nothing more
    // to do.
    //

    while (NextTransfer != NULL) {
        SubmitStatus = Controller->Host.FunctionTable.SubmitTransfer(
                                                      Controller->Host.Context,
                                                      Transfer);

        if (!KSUCCESS(SubmitStatus)) {
            Transfer->Status = SubmitStatus;
            NextTransfer = DmaTransferCompletion(Controller, NextTransfer);

        } else {
            break;
        }
    }

    return Status;
}

KSTATUS
DmaControlRequest (
    PDMA_INTERFACE Interface,
    PDMA_TRANSFER Transfer,
    PVOID Request,
    UINTN RequestSize
    )

/*++

Routine Description:

    This routine is called to perform a DMA controller-specific operation. It
    provides a direct link between DMA controllers and users, for controller-
    specific functionality.

Arguments:

    Interface - Supplies a pointer to the DMA controller interface.

    Transfer - Supplies an optional pointer to the transfer involved.

    Request - Supplies a pointer to the request/response data.

    RequestSize - Supplies the size of the request in bytes.

Return Value:

    Status code.

--*/

{

    PDMA_CONTROLLER Controller;
    KSTATUS Status;

    Controller = Interface->Context;
    if (Controller->Host.FunctionTable.ControlRequest == NULL) {
        return STATUS_NOT_SUPPORTED;
    }

    //
    // The common DMA library doesn't know much of anything, just pass it on
    // down.
    //

    Status = Controller->Host.FunctionTable.ControlRequest(
                                                      Controller->Host.Context,
                                                      Transfer,
                                                      Request,
                                                      RequestSize);

    return Status;
}

KSTATUS
DmaAllocateTransfer (
    PDMA_INTERFACE Interface,
    PDMA_TRANSFER *Transfer
    )

/*++

Routine Description:

    This routine creates a new DMA transfer structure.

Arguments:

    Interface - Supplies a pointer to the DMA controller interface.

    Transfer - Supplies a pointer where a pointer to the newly allocated
        transfer is returned on success.

Return Value:

    Status code.

--*/

{

    PDMA_TRANSFER DmaTransfer;

    *Transfer = NULL;
    DmaTransfer = MmAllocateNonPagedPool(sizeof(DMA_TRANSFER),
                                         DMA_ALLOCATION_TAG);

    if (DmaTransfer == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(DmaTransfer, sizeof(DMA_TRANSFER));
    *Transfer = DmaTransfer;
    return STATUS_SUCCESS;
}

VOID
DmaFreeTransfer (
    PDMA_INTERFACE Interface,
    PDMA_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine destroys a previously created DMA transfer. This transfer
    must not be actively submitted to any controller.

Arguments:

    Interface - Supplies a pointer to the DMA controller interface.

    Transfer - Supplies a pointer to the transfer to destroy.

Return Value:

    None.

--*/

{

    MmFreeNonPagedPool(Transfer);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

RUNLEVEL
DmapAcquireChannelLock (
    PDMA_CONTROLLER Controller,
    PDMA_CHANNEL Channel
    )

/*++

Routine Description:

    This routine raises to dispatch and acquires the DMA controller's channel
    lock.

Arguments:

    Controller - Supplies a pointer to the controller that owns the channel.

    Channel - Supplies a pointer to the channel to lock.

Return Value:

    Returns the previous runlevel, which should be passed into the release
    function.

--*/

{

    RUNLEVEL OldRunLevel;

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeAcquireSpinLock(&(Channel->Lock));
    return OldRunLevel;
}

VOID
DmapReleaseChannelLock (
    PDMA_CONTROLLER Controller,
    PDMA_CHANNEL Channel,
    RUNLEVEL OldRunLevel
    )

/*++

Routine Description:

    This routine releases the DMA channel's lock and lowers to the runlevel
    the system was at before the acquire.

Arguments:

    Controller - Supplies a pointer to the controller.

    Channel - Supplies a pointer to the channel to unlock.

    OldRunLevel - Supplies the runlevel returned by the acquire function.

Return Value:

    None.

--*/

{

    KeReleaseSpinLock(&(Channel->Lock));
    KeLowerRunLevel(OldRunLevel);
    return;
}

