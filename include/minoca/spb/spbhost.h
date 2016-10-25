/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    spbhost.h

Abstract:

    This header contains definitions for creating and managing Simple
    Peripheral Bus controllers.

Author:

    Evan Green 14-Aug-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/spb/spb.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifndef SPB_API

#define SPB_API __DLLIMPORT

#endif

#define SPB_CONTROLLER_INFORMATION_VERSION 1

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _SPB_CONTROLLER SPB_CONTROLLER, *PSPB_CONTROLLER;

typedef
KSTATUS
(*PSPB_HOST_CONFIGURE) (
    PVOID Context,
    PRESOURCE_SPB_DATA Configuration
    );

/*++

Routine Description:

    This routine configures the given Simple Peripheral Bus controller.

Arguments:

    Context - Supplies the host controller context.

    Configuration - Supplies a pointer to the new configuration to set.

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PSPB_HOST_SUBMIT_TRANSFER) (
    PVOID Context,
    PSPB_TRANSFER Transfer
    );

/*++

Routine Description:

    This routine is called to execute a single transfer on the Simple
    Peripheral Bus. The host controller is responsible for implementing the
    delay set in the transfer.

Arguments:

    Context - Supplies the host controller context.

    Transfer - Supplies a pointer to the transfer to begin executing. The
        controller can return immediately, and should call
        SpbProcessCompletedTransfer when the transfer completes.

Return Value:

    Status code indicating whether or not the transfer was successfully
    started.

--*/

typedef
VOID
(*PSPB_HOST_LOCK_BUS) (
    PVOID Context,
    PRESOURCE_SPB_DATA Configuration
    );

/*++

Routine Description:

    This routine is called when the bus is being locked for a particular
    transfer set or directly via the interface. The software synchronization
    portion of locking the bus is handled by the SPB library, this routine
    only needs to do hardware-specific actions (like selecting or deselecting
    device lines).

Arguments:

    Context - Supplies the host controller context.

    Configuration - Supplies a pointer to the configuration of the handle that
        locked this bus. The configure bus function will still be called, this
        is only passed for reference if bus-specific actions need to be
        performed (like selecting or deselecting the device).

Return Value:

    None.

--*/

typedef
VOID
(*PSPB_HOST_UNLOCK_BUS) (
    PVOID Context
    );

/*++

Routine Description:

    This routine is called when the bus is being unlocked.

Arguments:

    Context - Supplies the host controller context.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure stores the set of Simple Peripheral Bus controller functions
    called by the SPB library.

Members:

    Configure - Stores a pointer to a function used to set the current bus
        parameters.

    SubmitTransfer - Stores a pointer to a function used to begin a new
        transfer.

    LockBus - Stores an optional pointer to a function that is called when the
        bus is being locked.

    UnlockBus - Stores an optional pointer to a function that is called when
        the bus is being unlocked.

--*/

typedef struct _SPB_FUNCTION_TABLE {
    PSPB_HOST_CONFIGURE Configure;
    PSPB_HOST_SUBMIT_TRANSFER SubmitTransfer;
    PSPB_HOST_LOCK_BUS LockBus;
    PSPB_HOST_UNLOCK_BUS UnlockBus;
} SPB_FUNCTION_TABLE, *PSPB_FUNCTION_TABLE;

/*++

Structure Description:

    This structure defines the information provided to the SPB library by a
    Simple Peripheral Bus controller.

Members:

    Version - Stores the value SPB_CONTROLLER_INFORMATION_VERSION, used to
        enable future expansion of this structure.

    Context - Stores an opaque context pointer that is passed to the SPB
        controller functions.

    Device - Stores a pointer to the OS device associated with this controller.

    MaxFrequency - Stores the maximum bus clock frequency.

    BusType - Stores the bus type for this controller.

    Features - Stores a bitfield of features about this controller. See
        SPB_FEATURE_* definitions.

    FunctionTable - Stores the table of functions the library uses to call back
        into the controller.

--*/

typedef struct _SPB_CONTROLLER_INFORMATION {
    ULONG Version;
    PVOID Context;
    PDEVICE Device;
    ULONG MaxFrequency;
    RESOURCE_SPB_BUS_TYPE BusType;
    ULONG Features;
    SPB_FUNCTION_TABLE FunctionTable;
} SPB_CONTROLLER_INFORMATION, *PSPB_CONTROLLER_INFORMATION;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

SPB_API
KSTATUS
SpbCreateController (
    PSPB_CONTROLLER_INFORMATION Registration,
    PSPB_CONTROLLER *Controller
    );

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

SPB_API
VOID
SpbDestroyController (
    PSPB_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine destroys a Simple Peripheral Bus controller.

Arguments:

    Controller - Supplies a pointer to the controller to tear down.

Return Value:

    None.

--*/

SPB_API
KSTATUS
SpbStartController (
    PSPB_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine starts a Simple Peripheral Bus controller.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

SPB_API
VOID
SpbStopController (
    PSPB_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine stops a Simple Peripheral Bus controller.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    None.

--*/

SPB_API
PSPB_TRANSFER
SpbTransferCompletion (
    PSPB_CONTROLLER Controller,
    PSPB_TRANSFER Transfer,
    KSTATUS Status
    );

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

