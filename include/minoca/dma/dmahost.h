/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dmahost.h

Abstract:

    This header contains definitions for creating and managing Direct Memory
    Access controllers.

Author:

    Evan Green 2-Feb-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/dma/dma.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifndef DMA_API

#define DMA_API __DLLIMPORT

#endif

#define DMA_CONTROLLER_INFORMATION_VERSION 1

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _DMA_CONTROLLER DMA_CONTROLLER, *PDMA_CONTROLLER;

typedef
KSTATUS
(*PDMA_HOST_SUBMIT_TRANSFER) (
    PVOID Context,
    PDMA_TRANSFER Transfer
    );

/*++

Routine Description:

    This routine is called to execute a transfer on the DMA controller.

Arguments:

    Context - Supplies the host controller context.

    Transfer - Supplies a pointer to the transfer to begin executing. The
        controller can return immediately, and should call
        DmaProcessCompletedTransfer when the transfer completes.

Return Value:

    Status code indicating whether or not the transfer was successfully
    started.

--*/

typedef
KSTATUS
(*PDMA_HOST_CANCEL_TRANSFER) (
    PVOID Context,
    PDMA_TRANSFER Transfer
    );

/*++

Routine Description:

    This routine is called to cancel an in-progress transfer. Once this routine
    returns, the transfer should be all the way out of the DMA controller and
    the controller should no longer interrupt because of this transfer. This
    routine is called at dispatch level.

Arguments:

    Context - Supplies the host controller context.

    Transfer - Supplies a pointer to the transfer to cancel.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_TOO_LATE if the transfer is already complete.

    Other errors on other failures.

--*/

typedef
KSTATUS
(*PDMA_HOST_CONTROL_REQUEST) (
    PVOID Context,
    PDMA_TRANSFER Transfer,
    PVOID Request,
    UINTN RequestSize
    );

/*++

Routine Description:

    This routine is called to perform a DMA controller-specific operation. It
    provides a direct link between DMA controllers and users, for controller-
    specific functionality.

Arguments:

    Context - Supplies the host controller context.

    Transfer - Supplies an optional pointer to the transfer involved.

    Request - Supplies a pointer to the request/response data.

    RequestSize - Supplies the size of the request in bytes.

Return Value:

    Status code.

--*/

/*++

Structure Description:

    This structure stores the set of Direct Memory Access controller functions
    called by the DMA library.

Members:

    SubmitTransfer - Stores a pointer to a function used to begin a new
        transfer.

    CancelTransfer - Stores a pointer to a function used to cancel a transfer.

    ControlRequest - Stores a pointer to a function used to implemented
        controller-specific functionality.

--*/

typedef struct _DMA_FUNCTION_TABLE {
    PDMA_HOST_SUBMIT_TRANSFER SubmitTransfer;
    PDMA_HOST_CANCEL_TRANSFER CancelTransfer;
    PDMA_HOST_CONTROL_REQUEST ControlRequest;
} DMA_FUNCTION_TABLE, *PDMA_FUNCTION_TABLE;

/*++

Structure Description:

    This structure defines the information provided to the DMA library by a
    Direct Memory Access controller.

Members:

    Version - Stores the value DMA_CONTROLLER_INFORMATION_VERSION, used to
        enable future expansion of this structure.

    Context - Stores an opaque context pointer that is passed to the DMA
        controller functions.

    Device - Stores a pointer to the OS device associated with this controller.

    Information - Stores the information to be returned to users via the
        interface.

    Features - Stores a bitfield of features about this controller. See
        DMA_FEATURE_* definitions.

    FunctionTable - Stores the table of functions the library uses to call back
        into the controller.

--*/

typedef struct _DMA_CONTROLLER_INFORMATION {
    ULONG Version;
    PVOID Context;
    PDEVICE Device;
    DMA_INFORMATION Information;
    ULONG Features;
    DMA_FUNCTION_TABLE FunctionTable;
} DMA_CONTROLLER_INFORMATION, *PDMA_CONTROLLER_INFORMATION;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

DMA_API
KSTATUS
DmaCreateController (
    PDMA_CONTROLLER_INFORMATION Registration,
    PDMA_CONTROLLER *Controller
    );

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

DMA_API
VOID
DmaDestroyController (
    PDMA_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine destroys a Direct Memory Access controller.

Arguments:

    Controller - Supplies a pointer to the controller to tear down.

Return Value:

    None.

--*/

DMA_API
KSTATUS
DmaStartController (
    PDMA_CONTROLLER Controller
    );

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

DMA_API
VOID
DmaStopController (
    PDMA_CONTROLLER Controller
    );

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

DMA_API
PDMA_TRANSFER
DmaTransferCompletion (
    PDMA_CONTROLLER Controller,
    PDMA_TRANSFER Transfer
    );

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

