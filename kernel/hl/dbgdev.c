/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dbgdev.c

Abstract:

    This module implements support for hardware module debug devices.

Author:

    Evan Green 8-Apr-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/bootload.h>
#include "hlp.h"
#include "dbgdev.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure store information about a debug device that has been
    registered with the system.

Members:

    ListEntry - Stores pointers to the next and previous debug devices in the
        system.

    Description - Stores the debug device description.

--*/

typedef struct _DEBUG_DEVICE {
    LIST_ENTRY ListEntry;
    DEBUG_DEVICE_DESCRIPTION Description;
} DEBUG_DEVICE, *PDEBUG_DEVICE;

/*++

Structure Description:

    This structure store information about a debug USB host controller that has
    been registered with the system.

Members:

    ListEntry - Stores pointers to the next and previous debug devices in the
        system.

    Description - Stores the debug USB host controller description.

--*/

typedef struct _DEBUG_USB_HOST {
    LIST_ENTRY ListEntry;
    DEBUG_USB_HOST_DESCRIPTION Description;
} DEBUG_USB_HOST, *PDEBUG_USB_HOST;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

LIST_ENTRY HlDebugDeviceList;
LIST_ENTRY HlDebugUsbHostList;

//
// Set this boolean to skip USB debug device enumeration.
//

BOOL HlSkipUsbDebug = FALSE;

//
// Set this boolean to enable testing of the USB host interface via an
// alternate debug interface.
//

BOOL HlTestUsbHostDevice = FALSE;

//
// Store whether or not the USB host controllers have been enumerated.
//

BOOL HlUsbHostsEnumerated = FALSE;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
HlpInitializeDebugDevices (
    ULONG DebugDeviceIndex,
    PDEBUG_DEVICE_DESCRIPTION *DebugDevice
    )

/*++

Routine Description:

    This routine initializes the hardware layer's debug device support.
    This routine is called on the boot processor before the debugger is online.

Arguments:

    DebugDeviceIndex - Supplies the index of successfully enumerated debug
        interfaces to return.

    DebugDevice - Supplies a pointer where a pointer to the debug device
        description will be returned on success.

Return Value:

    Kernel status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PDEBUG_DEVICE Device;
    KSTATUS Status;

    *DebugDevice = NULL;
    INITIALIZE_LIST_HEAD(&HlDebugDeviceList);
    INITIALIZE_LIST_HEAD(&HlDebugUsbHostList);

    //
    // Perform architecture-specific initialization, including registering
    // "built in" debug devices.
    //

    Status = HlpArchInitializeDebugDevices();
    if (!KSUCCESS(Status)) {
        goto InitializeDebugDevicesEnd;
    }

    //
    // If there are no other debug devices, try to fire up a USB debug device.
    //

    if ((LIST_EMPTY(&HlDebugDeviceList) != FALSE) &&
        (HlSkipUsbDebug == FALSE)) {

        KdEhciModuleEntry();
        HlUsbHostsEnumerated = TRUE;
    }

    //
    // Find the specified debug interface.
    //

    CurrentEntry = HlDebugDeviceList.Next;
    while ((DebugDeviceIndex != 0) && (CurrentEntry != &HlDebugDeviceList)) {
        CurrentEntry = CurrentEntry->Next;
        DebugDeviceIndex -= 1;
    }

    if (CurrentEntry == &HlDebugDeviceList) {
        Status = STATUS_NO_ELIGIBLE_DEVICES;
        goto InitializeDebugDevicesEnd;
    }

    Device = LIST_VALUE(CurrentEntry, DEBUG_DEVICE, ListEntry);
    *DebugDevice = &(Device->Description);
    Status = STATUS_SUCCESS;

InitializeDebugDevicesEnd:
    return Status;
}

VOID
HlpTestUsbDebugInterface (
    VOID
    )

/*++

Routine Description:

    This routine runs the interface test on a USB debug interface if debugging
    the USB transport itself.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PDEBUG_USB_HOST Host;

    if (HlTestUsbHostDevice == FALSE) {
        return;
    }

    if (HlUsbHostsEnumerated == FALSE) {
        KdEhciModuleEntry();
    }

    CurrentEntry = HlDebugUsbHostList.Next;
    while (CurrentEntry != &HlDebugUsbHostList) {
        Host = LIST_VALUE(CurrentEntry, DEBUG_USB_HOST, ListEntry);
        KdUsbInitialize(&(Host->Description), TRUE);
        CurrentEntry = CurrentEntry->Next;
    }

    return;
}

KSTATUS
HlpDebugDeviceRegisterHardware (
    PDEBUG_DEVICE_DESCRIPTION Description
    )

/*++

Routine Description:

    This routine is called to register a new debug device with the system.

Arguments:

    Description - Supplies a pointer to a structure describing the new debug
        device.

Return Value:

    Status code.

--*/

{

    PDEBUG_DEVICE Device;
    KSTATUS Status;

    Device = NULL;

    //
    // Check the table version.
    //

    if (Description->TableVersion < DEBUG_DEVICE_DESCRIPTION_VERSION) {
        Status = STATUS_INVALID_PARAMETER;
        goto DebugDeviceRegisterHardwareEnd;
    }

    //
    // Check required function pointers.
    //

    if ((Description->FunctionTable.Reset == NULL) ||
        (Description->FunctionTable.Transmit == NULL) ||
        (Description->FunctionTable.Receive == NULL) ||
        (Description->FunctionTable.GetStatus == NULL) ||
        (Description->FunctionTable.Disconnect == NULL)) {

        Status = STATUS_INVALID_PARAMETER;
        goto DebugDeviceRegisterHardwareEnd;
    }

    //
    // Allocate the new serial port object.
    //

    Device = HlAllocateMemory(sizeof(DEBUG_DEVICE), HL_POOL_TAG, FALSE, NULL);
    if (Device == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto DebugDeviceRegisterHardwareEnd;
    }

    RtlZeroMemory(Device, sizeof(DEBUG_DEVICE));

    //
    // Initialize the new serial port based on the description.
    //

    RtlCopyMemory(&(Device->Description),
                  Description,
                  sizeof(DEBUG_DEVICE_DESCRIPTION));

    //
    // Insert the serial device on the list.
    //

    INSERT_BEFORE(&(Device->ListEntry), &HlDebugDeviceList);
    Status = STATUS_SUCCESS;

DebugDeviceRegisterHardwareEnd:
    return Status;
}

KSTATUS
HlpDebugUsbHostRegisterHardware (
    PDEBUG_USB_HOST_DESCRIPTION Description
    )

/*++

Routine Description:

    This routine is called to register a new debug USB host controller with the
    system.

Arguments:

    Description - Supplies a pointer to a structure describing the new debug
        device.

Return Value:

    Status code.

--*/

{

    PDEBUG_USB_HOST Device;
    KSTATUS Status;

    Device = NULL;

    //
    // Check the table version.
    //

    if (Description->TableVersion < DEBUG_USB_HOST_DESCRIPTION_VERSION) {
        Status = STATUS_INVALID_PARAMETER;
        goto DebugDeviceRegisterHardwareEnd;
    }

    //
    // Check required function pointers.
    //

    if ((Description->FunctionTable.Initialize == NULL) ||
        (Description->FunctionTable.GetRootHubStatus == NULL) ||
        (Description->FunctionTable.SetRootHubStatus == NULL) ||
        (Description->FunctionTable.SetupTransfer == NULL) ||
        (Description->FunctionTable.SubmitTransfer == NULL) ||
        (Description->FunctionTable.CheckTransfer == NULL) ||
        (Description->FunctionTable.RetireTransfer == NULL) ||
        (Description->FunctionTable.Stall == NULL) ||
        (Description->FunctionTable.GetHandoffData == NULL)) {

        Status = STATUS_INVALID_PARAMETER;
        goto DebugDeviceRegisterHardwareEnd;
    }

    //
    // Allocate the new serial port object.
    //

    Device = HlAllocateMemory(sizeof(DEBUG_USB_HOST), HL_POOL_TAG, FALSE, NULL);
    if (Device == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto DebugDeviceRegisterHardwareEnd;
    }

    RtlZeroMemory(Device, sizeof(DEBUG_USB_HOST));

    //
    // Initialize the new controller based on the description.
    //

    RtlCopyMemory(&(Device->Description),
                  Description,
                  sizeof(DEBUG_USB_HOST_DESCRIPTION));

    //
    // Insert the controller on the list.
    //

    INSERT_BEFORE(&(Device->ListEntry), &HlDebugUsbHostList);

    //
    // Unless the USB debugging is under test, fire up the device.
    //

    if (HlTestUsbHostDevice == FALSE) {
        KdUsbInitialize(&(Device->Description), FALSE);
    }

    Status = STATUS_SUCCESS;

DebugDeviceRegisterHardwareEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

