/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    hub.c

Abstract:

    This module implements support for interacting with USB hubs in the
    debug transport.

Author:

    Evan Green 2-Apr-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "kdusbp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
KdpUsbHubSetOrClearFeature (
    PKD_USB_DEVICE Hub,
    BOOL SetFeature,
    USHORT Feature,
    USHORT Port
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
KdpUsbHubReset (
    PKD_USB_DEVICE Device
    )

/*++

Routine Description:

    This routine resets a USB hub.

Arguments:

    Device - Supplies a pointer to the USB device.

Return Value:

    Status code.

--*/

{

    ULONG PortIndex;
    KSTATUS Status;

    //
    // Loop through and power on each port.
    //

    for (PortIndex = 0; PortIndex < Device->PortCount; PortIndex += 1) {
        Status = KdpUsbHubSetOrClearFeature(Device,
                                            TRUE,
                                            USB_HUB_FEATURE_PORT_POWER,
                                            PortIndex + 1);

        if (!KSUCCESS(Status)) {
            goto UsbHubResetEnd;
        }
    }

    //
    // The correct way to do this is to read the hub descriptor to figure out
    // how long it needs to stabilize power. KD doesn't have all the bells
    // and whistles, so just take a conservative guess and go for it.
    //

    KdpUsbStall(Device->Controller, 100);
    Status = STATUS_SUCCESS;

UsbHubResetEnd:
    return Status;
}

KSTATUS
KdpUsbHubGetStatus (
    PKD_USB_DEVICE Device,
    ULONG PortNumber,
    PULONG PortStatus
    )

/*++

Routine Description:

    This routine queries a USB hub for a port status.

Arguments:

    Device - Supplies a pointer to the USB hub device.

    PortNumber - Supplies the one-based port number to query.

    PortStatus - Supplies a pointer where the port status is returned.

Return Value:

    Status code.

--*/

{

    ULONG HardwareStatus;
    USB_SETUP_PACKET Setup;
    ULONG Size;
    ULONG SoftwareStatus;
    KSTATUS Status;

    Setup.RequestType = USB_SETUP_REQUEST_TO_HOST |
                        USB_SETUP_REQUEST_CLASS |
                        USB_SETUP_REQUEST_OTHER_RECIPIENT;

    Setup.Request = USB_DEVICE_REQUEST_GET_STATUS;
    Setup.Value = 0;
    Setup.Index = PortNumber;
    Setup.Length = sizeof(ULONG);
    Size = Setup.Length;
    Status = KdpUsbDefaultControlTransfer(Device,
                                          &Setup,
                                          DebugUsbTransferDirectionIn,
                                          &HardwareStatus,
                                          &Size);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    SoftwareStatus = 0;
    if ((HardwareStatus & USB_HUB_PORT_STATUS_DEVICE_CONNECTED) != 0) {
        SoftwareStatus |= DEBUG_USB_PORT_STATUS_CONNECTED;
        if ((HardwareStatus & USB_HUB_PORT_STATUS_HIGH_SPEED) != 0) {
            SoftwareStatus |= DEBUG_USB_PORT_STATUS_HIGH_SPEED;

        } else if ((HardwareStatus & USB_HUB_PORT_STATUS_LOW_SPEED) != 0) {
            SoftwareStatus |= DEBUG_USB_PORT_STATUS_LOW_SPEED;

        } else {
            SoftwareStatus |= DEBUG_USB_PORT_STATUS_FULL_SPEED;
        }
    }

    if ((HardwareStatus & USB_HUB_PORT_STATUS_ENABLED) != 0) {
        SoftwareStatus |= DEBUG_USB_PORT_STATUS_ENABLED;
    }

    if ((HardwareStatus & USB_HUB_PORT_STATUS_SUSPENDED) != 0) {
        SoftwareStatus |= DEBUG_USB_PORT_STATUS_SUSPENDED;
    }

    if ((HardwareStatus & USB_HUB_PORT_STATUS_OVER_CURRENT) != 0) {
        SoftwareStatus |= DEBUG_USB_PORT_STATUS_OVER_CURRENT;
    }

    *PortStatus = SoftwareStatus;
    return STATUS_SUCCESS;
}

KSTATUS
KdpUsbHubSetStatus (
    PKD_USB_DEVICE Device,
    ULONG PortNumber,
    ULONG PortStatus
    )

/*++

Routine Description:

    This routine sets the port status on a USB hub.

Arguments:

    Device - Supplies a pointer to the USB hub device.

    PortNumber - Supplies the one-based port number to query.

    PortStatus - Supplies the port status to set.

Return Value:

    Status code.

--*/

{

    ULONG Change;
    ULONG CurrentStatus;
    BOOL SetFeature;
    KSTATUS Status;

    Status = KdpUsbHubGetStatus(Device, PortNumber, &CurrentStatus);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    if (CurrentStatus == PortStatus) {
        return STATUS_SUCCESS;
    }

    //
    // Handle port enabled change events.
    //

    Change = CurrentStatus ^ PortStatus;
    if ((Change & DEBUG_USB_PORT_STATUS_ENABLED) != 0) {

        //
        // Disable the port if it changed and is no longer enabled.
        // Enabling a port directly is not allowed. This must be done
        // through a reset.
        //

        if ((PortStatus & DEBUG_USB_PORT_STATUS_ENABLED) == 0) {
            Status = KdpUsbHubSetOrClearFeature(Device,
                                                FALSE,
                                                USB_HUB_FEATURE_PORT_ENABLE,
                                                PortNumber);

            if (!KSUCCESS(Status)) {
                goto UsbHubSetStatusEnd;
            }
        }
    }

    //
    // Handle port reset changes.
    //

    if ((Change & DEBUG_USB_PORT_STATUS_RESET) != 0) {

        //
        // If the port is to be reset, then issue a reset. Note that a port
        // cannot be "un-reset", the hardware handles this.
        //

        if ((PortStatus & DEBUG_USB_PORT_STATUS_RESET) != 0) {
            Status = KdpUsbHubSetOrClearFeature(Device,
                                                TRUE,
                                                USB_HUB_FEATURE_PORT_RESET,
                                                PortNumber);

            if (!KSUCCESS(Status)) {
                goto UsbHubSetStatusEnd;
            }
        }
    }

    //
    // Handle port suspend changes.
    //

    if ((Change & DEBUG_USB_PORT_STATUS_SUSPENDED) != 0) {
        if ((PortStatus & DEBUG_USB_PORT_STATUS_SUSPENDED) != 0) {
            SetFeature = TRUE;

        } else {
            SetFeature = FALSE;
        }

        Status = KdpUsbHubSetOrClearFeature(Device,
                                            SetFeature,
                                            USB_HUB_FEATURE_PORT_SUSPEND,
                                            PortNumber);

        if (!KSUCCESS(Status)) {
            goto UsbHubSetStatusEnd;
        }
    }

    Status = STATUS_SUCCESS;

UsbHubSetStatusEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
KdpUsbHubSetOrClearFeature (
    PKD_USB_DEVICE Hub,
    BOOL SetFeature,
    USHORT Feature,
    USHORT Port
    )

/*++

Routine Description:

    This routine sends a set feature or clear feature request to the hub.

Arguments:

    Hub - Supplies a pointer to the hub to send the transfer to.

    SetFeature - Supplies a boolean indicating whether this is a SET_FEATURE
        request (TRUE) or a CLEAR_FEATURE request (FALSE).

    Feature - Supplies the feature selector to set or clear. This is the value
        that goes in the Value field of the setup packet.

    Port - Supplies the port number to set or clear. This is the value that
        goes in the Index field of the setup packet. The first port is port 1.
        Supply 0 to set or clear a hub feature.

Return Value:

    Status code.

--*/

{

    USB_SETUP_PACKET Setup;
    ULONG Size;
    KSTATUS Status;

    Setup.RequestType = USB_SETUP_REQUEST_TO_DEVICE |
                        USB_SETUP_REQUEST_CLASS;

    //
    // Treat port 0 as the hub itself.
    //

    if (Port == 0) {
        Setup.RequestType |= USB_SETUP_REQUEST_DEVICE_RECIPIENT;

    } else {
        Setup.RequestType |= USB_SETUP_REQUEST_OTHER_RECIPIENT;
    }

    if (SetFeature != FALSE) {
        Setup.Request = USB_DEVICE_REQUEST_SET_FEATURE;

    } else {
        Setup.Request = USB_DEVICE_REQUEST_CLEAR_FEATURE;
    }

    Setup.Value = Feature;
    Setup.Index = Port;
    Setup.Length = 0;
    Size = Setup.Length;
    Status = KdpUsbDefaultControlTransfer(Hub,
                                          &Setup,
                                          DebugUsbTransferDirectionOut,
                                          NULL,
                                          &Size);

    return Status;
}

