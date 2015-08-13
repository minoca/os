/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    main.c

Abstract:

    This module implements the entry point for the UEFI firmware running on top
    of the Raspberry Pi 2.

Author:

    Chris Stevens 19-Mar-2015

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "rpi2fw.h"

//
// ---------------------------------------------------------------- Definitions
//

#define FIRMWARE_IMAGE_NAME "rpi2fw.elf"

#define RPI2_DEFAULT_ARM_FREQUENCY 900000000

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the data necessary to query the BCM2709 video core
    for SMBIOS related information.

Members:

    Header - Stores a header that defines the total size of the messages being
        sent to and received from the mailbox.

    ModelMessage - Stores a message indicating that the board's model number is
        being queried. Contains the model number on return.

    RevisionMessage - Stores a message indicating that the board's revision is
        being queried. Contains the revision on return.

    SerialMessage - Stores a messagin indicating that the board's serial number
        is being queried. Contains the serial number on return.

    ArmClockRate - Stores a message requesting the ARM core's clock rate.

    ArmMaxClockRate - Stores a message requesting the ARM core's maximum clock
        rate.

    EndTag - Stores the tag to denote the end of the mailbox message.

--*/

typedef struct _EFI_BCM2709_SET_CLOCK_RATE {
    BCM2709_MAILBOX_HEADER Header;
    BCM2709_MAILBOX_SET_CLOCK_RATE ArmClockRate;
    UINT32 EndTag;
} EFI_BCM2709_SET_CLOCK_RATE, *PEFI_BCM2709_SET_CLOCK_RATE;

//
// ----------------------------------------------- Internal Function Prototypes
//

EFI_STATUS
EfipBcm2836ConfigureArmClock (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Variables defined in the linker script that mark the start and end of the
// image.
//

extern INT8 _end;
extern INT8 __executable_start;

//
// Store a template to set the Raspberry Pi 2's ARM clock frequency.
//

EFI_BCM2709_SET_CLOCK_RATE EfiRpi2SetClockTemplate = {
    {
        sizeof(EFI_BCM2709_SET_CLOCK_RATE),
        0
    },

    {
        {
            BCM2709_MAILBOX_TAG_SET_CLOCK_RATE,
            sizeof(UINT32) * 3,
            sizeof(UINT32) * 3
        },

        BCM2709_MAILBOX_CLOCK_ID_ARM,
        RPI2_DEFAULT_ARM_FREQUENCY,
        0
    },

    0
};

//
// ------------------------------------------------------------------ Functions
//

VOID
EfiRpi2Main (
    VOID *TopOfStack,
    UINTN StackSize
    )

/*++

Routine Description:

    This routine is the C entry point for the firmware.

Arguments:

    TopOfStack - Supplies the top of the stack that has been set up for the
        loader.

    StackSize - Supplies the total size of the stack set up for the loader, in
        bytes.

Return Value:

    This routine does not return.

--*/

{

    UINTN FirmwareSize;

    //
    // Initialize UEFI enough to get into the debugger.
    //

    FirmwareSize = (UINTN)&_end - (UINTN)&__executable_start;
    EfiCoreMain((VOID *)-1,
                &__executable_start,
                FirmwareSize,
                FIRMWARE_IMAGE_NAME,
                TopOfStack - StackSize,
                StackSize);

    return;
}

EFI_STATUS
EfiPlatformInitialize (
    UINT32 Phase
    )

/*++

Routine Description:

    This routine performs platform-specific firmware initialization.

Arguments:

    Phase - Supplies the iteration number this routine is being called on.
        Phase zero occurs very early, just after the debugger comes up.
        Phase one occurs a bit later, after timer and interrupt services are
        initialized.

Return Value:

    EFI status code.

--*/

{

    EFI_STATUS Status;

    if (Phase == 0) {
        Status = EfipBcm2709Initialize((VOID *)BCM2836_BASE);
        if (EFI_ERROR(Status)) {
            return Status;
        }

        Status = EfipBcm2836SmpInitialize(0);
        if (EFI_ERROR(Status)) {
            return Status;
        }

    } else if (Phase == 1) {
        Status = EfipBcm2836ConfigureArmClock();
        if (EFI_ERROR(Status)) {
            return Status;
        }

        Status = EfipBcm2709UsbInitialize();
        if (EFI_ERROR(Status)) {
            return Status;
        }

        Status = EfipBcm2836SmpInitialize(1);
        if (EFI_ERROR(Status)) {
            return Status;
        }

        Status = EfipRpi2CreateSmbiosTables();
        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    return EFI_SUCCESS;
}

EFI_STATUS
EfiPlatformEnumerateDevices (
    VOID
    )

/*++

Routine Description:

    This routine enumerates and connects any builtin devices the platform
    contains.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    EFI_STATUS Status;

    Status = EfipBcm2709EnumerateSd();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfipBcm2709EnumerateVideo();
    EfipBcm2709EnumerateSerial();
    Status = EfipEnumerateRamDisks();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

EFI_STATUS
EfipBcm2836ConfigureArmClock (
    VOID
    )

/*++

Routine Description:

    This routine initialized the ARM clock since the firmware initializes it to
    600Mhz, rather than the 900Mhz max.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    EFI_BCM2709_SET_CLOCK_RATE SetClockRate;
    EFI_STATUS Status;

    EfiCopyMem(&SetClockRate,
               &EfiRpi2SetClockTemplate,
               sizeof(EFI_BCM2709_SET_CLOCK_RATE));

    Status = EfipBcm2709MailboxSendCommand(
                                        BCM2709_MAILBOX_PROPERTIES_CHANNEL,
                                        &SetClockRate,
                                        sizeof(EFI_BCM2709_SET_CLOCK_RATE),
                                        TRUE);

    return Status;
}

