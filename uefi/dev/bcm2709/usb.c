/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    usb.c

Abstract:

    This module implements platform USB support for the BCM2709 SoC family.

Author:

    Chris Stevens 31-Dec-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include <dev/bcm2709.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the data necessary to enable the USB device.

Members:

    Header - Stores a header that defines the total size of the messages being
        sent to and received from the mailbox.

    DeviceState - Stores a request to set the state for a particular device.

    EndTag - Stores the tag to denote the end of the mailbox message.

--*/

typedef struct _EFI_USB_BCM2709_ENABLE {
    BCM2709_MAILBOX_HEADER Header;
    BCM2709_MAILBOX_DEVICE_STATE DeviceState;
    UINT32 EndTag;
} EFI_USB_BCM2709_ENABLE, *PEFI_USB_BCM2709_ENABLE;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// Define a template for the call to enable the USB power.
//

EFI_USB_BCM2709_ENABLE Bcm2709UsbPowerTemplate = {
    {
        sizeof(EFI_USB_BCM2709_ENABLE),
        0
    },

    {
        {
            BCM2709_MAILBOX_TAG_SET_POWER_STATE,
            sizeof(UINT32) + sizeof(UINT32),
            sizeof(UINT32) + sizeof(UINT32)
        },

        BCM2709_MAILBOX_DEVICE_USB,
        BCM2709_MAILBOX_POWER_STATE_ON
    },

    0
};

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipBcm2709UsbInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initialize the USB device on Broadcom 2709 SoCs.

Arguments:

    None.

Return Value:

    None.

--*/

{

    EFI_STATUS Status;

    //
    // The BCM2709 device library must be initialized.
    //

    if (EfiBcm2709Initialized == FALSE) {
        return EFI_NOT_READY;
    }

    Status = EfipBcm2709MailboxSendCommand(BCM2709_MAILBOX_PROPERTIES_CHANNEL,
                                           &Bcm2709UsbPowerTemplate,
                                           sizeof(EFI_USB_BCM2709_ENABLE),
                                           TRUE);

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

