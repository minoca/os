/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    smbios.c

Abstract:

    This module implements SMBIOS tables for the Raspberry Pi.

Author:

    Chris Stevens 8-Jan-2015

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/lib/rtl.h>
#include <minoca/fw/smbios.h>
#include "uefifw.h"
#include "rpifw.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Set the build date and version to a hardcoded value to avoid the SMBIOS
// table changing from build to build. The automated tests checksum the whole
// table to get a machine ID, so if the dates or versions change then each
// new firmware iteration looks like an entirely new machine.
//

#define RPI_FIRMWARE_VERSION_MAJOR 1
#define RPI_FIRMWARE_VERSION_MINOR 1
#define RPI_FIRMWARE_VERSION_STRING "1.1"
#define RPI_FIRMWARE_VERSION_DATE "05/06/2016"

#define RPI_SMBIOS_BIOS_VENDOR "Minoca Corp"

#define RPI_SMBIOS_SYSTEM_MANUFACTURER "Raspberry Pi Foundation"
#define RPI_SMBIOS_SYSTEM_PRODUCT_NAME "Raspberry Pi"

#define RPI_SMBIOS_MODULE_MANUFACTURER "Raspberry Pi Foundation"
#define RPI_SMBIOS_MODULE_PRODUCT "Raspberry Pi"

#define RPI_SMBIOS_PROCESSOR_MANUFACTURER "Broadcom"
#define RPI_SMBIOS_PROCESSOR_PART "BCM2835"
#define RPI_SMBIOS_PROCESSOR_EXTERNAL_CLOCK 250
#define RPI_SMBIOS_PROCESSOR_CORE_COUNT 1

#define RPI_SMBIOS_CACHE_L1_SIZE 16

#define HERTZ_PER_MEGAHERTZ 1000000ULL

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the data necessary to query the BCM2709 video core
    for SMBIOS related information.

Members:

    Header - Stores a header that defines the total size of the messages being
        sent to and received from the mailbox.

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
    BCM2709_MAILBOX_BOARD_REVISION RevisionMessage;
    BCM2709_MAILBOX_BOARD_SERIAL_NUMBER SerialMessage;
    BCM2709_MAILBOX_GET_CLOCK_RATE ArmClockRate;
    BCM2709_MAILBOX_GET_CLOCK_RATE ArmMaxClockRate;
    UINT32 EndTag;
} EFI_BCM2709_GET_SMBIOS_INFORMATION, *PEFI_BCM2709_GET_SMBIOS_INFORMATION;

/*++

Structure Description:

    This structure defines a Raspberry Pi revision.

Members:

    Revision - Stores the Raspberry Pi revision number.

    Name - Stores the friendly name of the revision.

--*/

typedef struct _RPI_REVISION {
    UINT32 Revision;
    CHAR8 *Name;
} RPI_REVISION, *PRPI_REVISION;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

SMBIOS_BIOS_INFORMATION EfiRpiSmbiosBiosInformation = {
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
    RPI_FIRMWARE_VERSION_MAJOR,
    RPI_FIRMWARE_VERSION_MINOR,
    0,
    0
};

SMBIOS_SYSTEM_INFORMATION EfiRpiSmbiosSystemInformation = {
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

SMBIOS_MODULE_INFORMATION EfiRpiSmbiosModuleInformation = {
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

SMBIOS_ENCLOSURE EfiRpiSmbiosEnclosure = {
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

SMBIOS_PROCESSOR_INFORMATION EfiRpiSmbiosProcessorInformation = {
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
    RPI_SMBIOS_PROCESSOR_EXTERNAL_CLOCK,
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
    RPI_SMBIOS_PROCESSOR_CORE_COUNT,
    0,
    SMBIOS_PROCESSOR_CHARACTERISTIC_UNKNOWN,
    0
};

SMBIOS_CACHE_INFORMATION EfiRpiSmbiosL1Cache = {
    {
        SmbiosCacheInformation,
        sizeof(SMBIOS_CACHE_INFORMATION),
        0x0106
    },

    0,
    SMBIOS_CACHE_ENABLED | SMBIOS_CACHE_WRITE_BACK,
    RPI_SMBIOS_CACHE_L1_SIZE,
    RPI_SMBIOS_CACHE_L1_SIZE,
    SMBIOS_CACHE_SRAM_UNKNOWN,
    SMBIOS_CACHE_SRAM_UNKNOWN,
    0,
    SMBIOS_CACHE_ERROR_CORRECTION_NONE,
    SMBIOS_CACHE_TYPE_DATA,
    SMBIOS_CACHE_ASSOCIATIVITY_4_WAY_SET
};

EFI_BCM2709_GET_SMBIOS_INFORMATION EfiRpiBoardInformationTemplate = {
    {
        sizeof(EFI_BCM2709_GET_SMBIOS_INFORMATION),
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

RPI_REVISION EfiRpiRevisions[] = {
    {0x00000001, "1 Model B (Beta)"},
    {0x00000002, "1 Model B Rev 1.0"},
    {0x00000003, "1 Model B Rev 1.0 (ECN0001)"},
    {0x00000004, "1 Model B Rev 2.0"},
    {0x00000005, "1 Model B Rev 2.0"},
    {0x00000006, "1 Model B Rev 2.0"},
    {0x00000007, "1 Model A Rev 2.0"},
    {0x00000008, "1 Model A Rev 2.0"},
    {0x00000009, "1 Model A Rev 2.0"},
    {0x0000000D, "1 Model B Rev 2.0"},
    {0x0000000E, "1 Model B Rev 2.0"},
    {0x0000000F, "1 Model B Rev 2.0"},
    {0x00000010, "1 Model B+ Rev 1.0"},
    {0x00000011, "Compute Module Rev 1.0"},
    {0x00000012, "1 Model A+ Rev 1.0"},
    {0x00000013, "1 Model B+ Rev 1.2"},
    {0x00900092, "Zero Rev 1.2"}
};

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipRpiCreateSmbiosTables (
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
    UINT32 Index;
    UINT32 Length;
    CHAR8 ProductBuffer[64];
    CHAR8 *ProductName;
    PRPI_REVISION Revision;
    UINT32 RevisionCount;
    CHAR8 SerialNumber[17];
    EFI_STATUS Status;
    CHAR8 Version[13];

    //
    // Query the BMC2835 mailbox to get version information and a serial number.
    //

    EfiCopyMem(&BoardInformation,
               &EfiRpiBoardInformationTemplate,
               sizeof(EFI_BCM2709_GET_SMBIOS_INFORMATION));

    Status = EfipBcm2709MailboxSendCommand(
                                 BCM2709_MAILBOX_PROPERTIES_CHANNEL,
                                 &BoardInformation,
                                 sizeof(EFI_BCM2709_GET_SMBIOS_INFORMATION),
                                 FALSE);

    if (EFI_ERROR(Status)) {
        return Status;
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

    EfiRpiSmbiosProcessorInformation.MaxSpeed =
                   BoardInformation.ArmMaxClockRate.Rate / HERTZ_PER_MEGAHERTZ;

    EfiRpiSmbiosProcessorInformation.CurrentSpeed =
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

    RtlCopyMemory(EfiRpiSmbiosSystemInformation.Uuid,
                  BoardInformation.SerialMessage.SerialNumber,
                  sizeof(BoardInformation.SerialMessage.SerialNumber));

    //
    // Convert the revision to a string.
    //

    RtlPrintToString(Version,
                     sizeof(Version),
                     CharacterEncodingAscii,
                     "Rev %08X",
                     BoardInformation.RevisionMessage.Revision);

    //
    // Generate the product name based on the revision.
    //

    Revision = NULL;
    RevisionCount = sizeof(EfiRpiRevisions) / sizeof(EfiRpiRevisions[0]);
    for (Index = 0; Index < RevisionCount; Index += 1) {
        if (EfiRpiRevisions[Index].Revision ==
            BoardInformation.RevisionMessage.Revision) {

            Revision = &(EfiRpiRevisions[Index]);
            break;
        }
    }

    if (Revision == NULL) {
        ProductName = RPI_SMBIOS_SYSTEM_PRODUCT_NAME;

    } else {
        RtlPrintToString(ProductBuffer,
                         sizeof(ProductBuffer),
                         CharacterEncodingAscii,
                         "%s %s",
                         RPI_SMBIOS_SYSTEM_PRODUCT_NAME,
                         Revision->Name);

        ProductName = ProductBuffer;
    }

    Status = EfiSmbiosAddStructure(&EfiRpiSmbiosBiosInformation,
                                   RPI_SMBIOS_BIOS_VENDOR,
                                   RPI_FIRMWARE_VERSION_STRING,
                                   RPI_FIRMWARE_VERSION_DATE,
                                   NULL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfiSmbiosAddStructure(&EfiRpiSmbiosSystemInformation,
                                   RPI_SMBIOS_SYSTEM_MANUFACTURER,
                                   ProductName,
                                   Version,
                                   SerialNumber,
                                   NULL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfiSmbiosAddStructure(&EfiRpiSmbiosModuleInformation,
                                   RPI_SMBIOS_MODULE_MANUFACTURER,
                                   ProductName,
                                   NULL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfiSmbiosAddStructure(&EfiRpiSmbiosEnclosure, NULL);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfiSmbiosAddStructure(&EfiRpiSmbiosProcessorInformation,
                                   RPI_SMBIOS_PROCESSOR_MANUFACTURER,
                                   SerialNumber,
                                   RPI_SMBIOS_PROCESSOR_PART,
                                   NULL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfiSmbiosAddStructure(&EfiRpiSmbiosL1Cache, NULL);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

