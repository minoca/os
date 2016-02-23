/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    smbios.c

Abstract:

    This module implements SMBIOS tables for the Raspberry Pi 2.

Author:

    Chris Stevens 19-Mar-2015

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/lib/rtl.h>
#include <minoca/smbios.h>
#include "uefifw.h"
#include "rpi2fw.h"

//
// ---------------------------------------------------------------- Definitions
//

#define RPI2_SMBIOS_BIOS_VENDOR "Minoca Corp"

#define RPI2_SMBIOS_SYSTEM_MANUFACTURER "Raspberry Pi 2"
#define RPI2_SMBIOS_SYSTEM_PRODUCT_NAME "Raspberry Pi 2"

#define RPI2_SMBIOS_MODULE_MANUFACTURER "Raspberry Pi 2"
#define RPI2_SMBIOS_MODULE_PRODUCT "Raspberry Pi 2"

#define RPI2_SMBIOS_PROCESSOR_MANUFACTURER "Broadcom"
#define RPI2_SMBIOS_PROCESSOR_PART "BCM2836"
#define RPI2_SMBIOS_PROCESSOR_EXTERNAL_CLOCK 250
#define RPI2_SMBIOS_PROCESSOR_CORE_COUNT 4

#define RPI2_SMBIOS_CACHE_L1_SIZE 32

#define HERTZ_PER_MEGAHERTZ 1000000ULL

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

typedef struct _EFI_BCM2709_GET_SMBIOS_INFORMATION {
    BCM2709_MAILBOX_HEADER Header;
    BCM2709_MAILBOX_BOARD_MODEL ModelMessage;
    BCM2709_MAILBOX_BOARD_REVISION RevisionMessage;
    BCM2709_MAILBOX_BOARD_SERIAL_NUMBER SerialMessage;
    BCM2709_MAILBOX_GET_CLOCK_RATE ArmClockRate;
    BCM2709_MAILBOX_GET_CLOCK_RATE ArmMaxClockRate;
    UINT32 EndTag;
} EFI_BCM2709_GET_SMBIOS_INFORMATION, *PEFI_BCM2709_GET_SMBIOS_INFORMATION;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

SMBIOS_BIOS_INFORMATION EfiRpi2SmbiosBiosInformation = {
    {
        SmbiosBiosInformation,
        sizeof(SMBIOS_BIOS_INFORMATION),
        0x0100
    },

    1,
    2,
    0,
    3,
    0,
    SMBIOS_BIOS_CHARACTERISTIC_UNSUPPORTED,
    0,
    FIRMWARE_VERSION_MAJOR,
    FIRMWARE_VERSION_MINOR,
    0,
    0
};

SMBIOS_SYSTEM_INFORMATION EfiRpi2SmbiosSystemInformation = {
    {
        SmbiosSystemInformation,
        sizeof(SMBIOS_SYSTEM_INFORMATION),
        0x0101
    },

    1,
    2,
    3,
    4,
    {0},
    SMBIOS_SYSTEM_WAKEUP_UNKNOWN,
    3,
    2
};

SMBIOS_MODULE_INFORMATION EfiRpi2SmbiosModuleInformation = {
    {
        SmbiosModuleInformation,
        sizeof(SMBIOS_MODULE_INFORMATION),
        0x0102
    },

    1,
    2,
    0,
    0,
    0,
    SMBIOS_MODULE_MOTHERBOARD,
    0,
    0x0104,
    SMBIOS_MODULE_TYPE_MOTHERBOARD,
    0
};

SMBIOS_ENCLOSURE EfiRpi2SmbiosEnclosure = {
    {
        SmbiosSystemEnclosure,
        sizeof(SMBIOS_ENCLOSURE),
        0x0104
    },

    0,
    SMBIOS_ENCLOSURE_TYPE_UNKNOWN,
    0,
    0,
    0,
    SMBIOS_ENCLOSURE_STATE_UNKNOWN,
    SMBIOS_ENCLOSURE_STATE_UNKNOWN,
    SMBIOS_ENCLOSURE_SECURITY_STATE_UNKNOWN,
    0,
    0,
    0,
    0,
    0,
    0
};

SMBIOS_PROCESSOR_INFORMATION EfiRpi2SmbiosProcessorInformation = {
    {
        SmbiosProcessorInformation,
        sizeof(SMBIOS_PROCESSOR_INFORMATION),
        0x0105
    },

    0,
    SMBIOS_PROCESSOR_TYPE_CENTRAL_PROCESSOR,
    0x2,
    1,
    0,
    0,
    0,
    RPI2_SMBIOS_PROCESSOR_EXTERNAL_CLOCK,
    0,
    0,
    SMBIOS_PROCESSOR_STATUS_ENABLED,
    0,
    0x0106,
    0xFFFF,
    0xFFFF,
    2,
    0,
    3,
    RPI2_SMBIOS_PROCESSOR_CORE_COUNT,
    0,
    SMBIOS_PROCESSOR_CHARACTERISTIC_UNKNOWN,
    0
};

SMBIOS_CACHE_INFORMATION EfiRpi2SmbiosL1Cache = {
    {
        SmbiosCacheInformation,
        sizeof(SMBIOS_CACHE_INFORMATION),
        0x0106
    },

    0,
    SMBIOS_CACHE_ENABLED | SMBIOS_CACHE_WRITE_BACK,
    RPI2_SMBIOS_CACHE_L1_SIZE,
    RPI2_SMBIOS_CACHE_L1_SIZE,
    SMBIOS_CACHE_SRAM_UNKNOWN,
    SMBIOS_CACHE_SRAM_UNKNOWN,
    0,
    SMBIOS_CACHE_ERROR_CORRECTION_NONE,
    SMBIOS_CACHE_TYPE_DATA,
    SMBIOS_CACHE_ASSOCIATIVITY_4_WAY_SET
};

EFI_BCM2709_GET_SMBIOS_INFORMATION EfiRpi2BoardInformationTemplate = {
    {
        sizeof(EFI_BCM2709_GET_SMBIOS_INFORMATION),
        0
    },

    {
        {
            BCM2709_MAILBOX_TAG_GET_BOARD_MODEL,
            sizeof(UINT32),
            0
        },

        0
    },

    {
        {
            BCM2709_MAILBOX_TAG_GET_BOARD_REVISION,
            sizeof(UINT32),
            0
        },

        0
    },

    {
        {
            BCM2709_MAILBOX_TAG_GET_BOARD_SERIAL,
            sizeof(UINT32) * 2,
            0
        },

        {0, 0}
    },

    {
        {
            BCM2709_MAILBOX_TAG_GET_CLOCK_RATE,
            sizeof(UINT32) + sizeof(UINT32),
            sizeof(UINT32)
        },

        BCM2709_MAILBOX_CLOCK_ID_ARM,
        0
    },

    {
        {
            BCM2709_MAILBOX_TAG_GET_CLOCK_MAX_RATE,
            sizeof(UINT32) + sizeof(UINT32),
            sizeof(UINT32)
        },

        BCM2709_MAILBOX_CLOCK_ID_ARM,
        0
    },

    0
};

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipRpi2CreateSmbiosTables (
    VOID
    )

/*++

Routine Description:

    This routine creates the SMBIOS tables.

Arguments:

    None.

Return Value:

    EFI Status code.

--*/

{

    EFI_BCM2709_GET_SMBIOS_INFORMATION BoardInformation;
    UINT32 ExpectedLength;
    UINT32 Length;
    CHAR8 SerialNumber[17];
    EFI_STATUS Status;
    CHAR8 Version[28];

    //
    // Query the BMC2836 mailbox to get version information and a serial number.
    //

    EfiCopyMem(&BoardInformation,
               &EfiRpi2BoardInformationTemplate,
               sizeof(EFI_BCM2709_GET_SMBIOS_INFORMATION));

    Status = EfipBcm2709MailboxSendCommand(
                                 BCM2709_MAILBOX_PROPERTIES_CHANNEL,
                                 &BoardInformation,
                                 sizeof(EFI_BCM2709_GET_SMBIOS_INFORMATION),
                                 FALSE);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Length = BoardInformation.ModelMessage.TagHeader.Length;
    ExpectedLength = sizeof(BCM2709_MAILBOX_BOARD_MODEL) -
                     sizeof(BCM2709_MAILBOX_TAG);

    if (BCM2709_MAILBOX_CHECK_TAG_LENGTH(Length, ExpectedLength) == FALSE) {
        return EFI_DEVICE_ERROR;
    }

    Length = BoardInformation.RevisionMessage.TagHeader.Length;
    ExpectedLength = sizeof(BCM2709_MAILBOX_BOARD_REVISION) -
                     sizeof(BCM2709_MAILBOX_TAG);

    if (BCM2709_MAILBOX_CHECK_TAG_LENGTH(Length, ExpectedLength) == FALSE) {
        return EFI_DEVICE_ERROR;
    }

    Length = BoardInformation.SerialMessage.TagHeader.Length;
    ExpectedLength = sizeof(BCM2709_MAILBOX_BOARD_SERIAL_NUMBER) -
                     sizeof(BCM2709_MAILBOX_TAG);

    if (BCM2709_MAILBOX_CHECK_TAG_LENGTH(Length, ExpectedLength) == FALSE) {
        return EFI_DEVICE_ERROR;
    }

    Length = BoardInformation.ArmClockRate.TagHeader.Length;
    ExpectedLength = sizeof(BCM2709_MAILBOX_GET_CLOCK_RATE) -
                     sizeof(BCM2709_MAILBOX_TAG);

    if (BCM2709_MAILBOX_CHECK_TAG_LENGTH(Length, ExpectedLength) == FALSE) {
        return EFI_DEVICE_ERROR;
    }

    Length = BoardInformation.ArmMaxClockRate.TagHeader.Length;
    ExpectedLength = sizeof(BCM2709_MAILBOX_GET_CLOCK_RATE) -
                     sizeof(BCM2709_MAILBOX_TAG);

    if (BCM2709_MAILBOX_CHECK_TAG_LENGTH(Length, ExpectedLength) == FALSE) {
        return EFI_DEVICE_ERROR;
    }

    //
    // Update the clock information.
    //

    EfiRpi2SmbiosProcessorInformation.MaxSpeed =
                   BoardInformation.ArmMaxClockRate.Rate / HERTZ_PER_MEGAHERTZ;

    EfiRpi2SmbiosProcessorInformation.CurrentSpeed =
                      BoardInformation.ArmClockRate.Rate / HERTZ_PER_MEGAHERTZ;

    //
    // Convert the serial number to a string.
    //

    RtlPrintToString(SerialNumber,
                     sizeof(SerialNumber),
                     CharacterEncodingAscii,
                     "%08X%08X",
                     BoardInformation.SerialMessage.SerialNumber[1],
                     BoardInformation.SerialMessage.SerialNumber[0]);

    RtlCopyMemory(EfiRpi2SmbiosSystemInformation.Uuid,
                  BoardInformation.SerialMessage.SerialNumber,
                  sizeof(BoardInformation.SerialMessage.SerialNumber));

    //
    // Convert the model and revision to a string.
    //

    RtlPrintToString(Version,
                     sizeof(Version),
                     CharacterEncodingAscii,
                     "Model %08X Rev %08X",
                     BoardInformation.ModelMessage.ModelNumber,
                     BoardInformation.RevisionMessage.Revision);

    Status = EfiSmbiosAddStructure(&EfiRpi2SmbiosBiosInformation,
                                   RPI2_SMBIOS_BIOS_VENDOR,
                                   FIRMWARE_VERSION_STRING,
                                   FIRMWARE_BUILD_DATE,
                                   NULL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfiSmbiosAddStructure(&EfiRpi2SmbiosSystemInformation,
                                   RPI2_SMBIOS_SYSTEM_MANUFACTURER,
                                   RPI2_SMBIOS_SYSTEM_PRODUCT_NAME,
                                   Version,
                                   SerialNumber,
                                   NULL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfiSmbiosAddStructure(&EfiRpi2SmbiosModuleInformation,
                                   RPI2_SMBIOS_MODULE_MANUFACTURER,
                                   RPI2_SMBIOS_MODULE_PRODUCT,
                                   NULL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfiSmbiosAddStructure(&EfiRpi2SmbiosEnclosure, NULL);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfiSmbiosAddStructure(&EfiRpi2SmbiosProcessorInformation,
                                   RPI2_SMBIOS_PROCESSOR_MANUFACTURER,
                                   SerialNumber,
                                   RPI2_SMBIOS_PROCESSOR_PART,
                                   NULL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfiSmbiosAddStructure(&EfiRpi2SmbiosL1Cache, NULL);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

