/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    smbios.c

Abstract:

    This module implements SMBIOS tables for the PandaBoard.

Author:

    Evan Green 7-May-2014

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
#include "pandafw.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Set the build date and version to a hardcoded value to avoid the SMBIOS
// table changing from build to build. The automated tests checksum the whole
// table to get a machine ID, so if the dates or versions change then each
// new firmware iteration looks like an entirely new machine.
//

#define PANDA_FIRMWARE_VERSION_MAJOR 1
#define PANDA_FIRMWARE_VERSION_MINOR 0
#define PANDA_FIRMWARE_VERSION_STRING "1.0"
#define PANDA_FIRMWARE_BUILD_DATE "08/15/2014"

#define PANDA_SMBIOS_BIOS_VENDOR "Minoca Corp"

#define PANDA_SMBIOS_SYSTEM_MANUFACTURER "Texas Instruments"
#define PANDA_SMBIOS_SYSTEM_PRODUCT_NAME "PandaBoard"
#define PANDA_SMBIOS_SYSTEM_PRODUCT_NAME_ES "PandaBoard ES"

#define PANDA_SMBIOS_SYSTEM_PRODUCT_VERSION ""

#define PANDA_SMBIOS_MODULE_MANUFACTURER "Texas Instruments"

#define PANDA_SMBIOS_PROCESSOR_MANUFACTURER "Texas Instruments"
#define PANDA_SMBIOS_PROCESSOR_PART_4430 "OMAP4430"
#define PANDA_SMBIOS_PROCESSOR_PART_4460 "OMAP4460"
#define PANDA_SMBIOS_PROCESSOR_EXTERNAL_CLOCK 38
#define PANDA_SMBIOS_PROCESSOR_MAX_SPEED_4430 1000
#define PANDA_SMBIOS_PROCESSOR_MAX_SPEED_4460 1200
#define PANDA_SMBIOS_PROCESSOR_CURRENT_SPEED 1000
#define PANDA_SMBIOS_PROCESSOR_CORE_COUNT 2

#define PANDA_SMBIOS_CACHE_L1_SIZE 32
#define PANDA_SMBIOS_CACHE_L2_SIZE 1024

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

OMAP4_REVISION
EfipOmap4GetRevision (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

SMBIOS_BIOS_INFORMATION EfiPandaSmbiosBiosInformation = {
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
    PANDA_FIRMWARE_VERSION_MAJOR,
    PANDA_FIRMWARE_VERSION_MINOR,
    0,
    0
};

SMBIOS_SYSTEM_INFORMATION EfiPandaSmbiosSystemInformation = {
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

SMBIOS_MODULE_INFORMATION EfiPandaSmbiosModuleInformation = {
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

SMBIOS_ENCLOSURE EfiPandaSmbiosEnclosure = {
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

SMBIOS_PROCESSOR_INFORMATION EfiPandaSmbiosProcessorInformation = {
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
    PANDA_SMBIOS_PROCESSOR_EXTERNAL_CLOCK,
    PANDA_SMBIOS_PROCESSOR_MAX_SPEED_4430,
    PANDA_SMBIOS_PROCESSOR_CURRENT_SPEED,
    SMBIOS_PROCESSOR_STATUS_ENABLED,
    0,
    0x0106,
    0x0107,
    0xFFFF,
    2,
    0,
    3,
    PANDA_SMBIOS_PROCESSOR_CORE_COUNT,
    0,
    SMBIOS_PROCESSOR_CHARACTERISTIC_UNKNOWN,
    0
};

SMBIOS_CACHE_INFORMATION EfiPandaSmbiosL1Cache = {
    {
        SmbiosCacheInformation,
        sizeof(SMBIOS_CACHE_INFORMATION),
        0x0106
    },

    0,
    SMBIOS_CACHE_ENABLED | SMBIOS_CACHE_WRITE_BACK,
    PANDA_SMBIOS_CACHE_L1_SIZE,
    PANDA_SMBIOS_CACHE_L1_SIZE,
    SMBIOS_CACHE_SRAM_UNKNOWN,
    SMBIOS_CACHE_SRAM_UNKNOWN,
    0,
    SMBIOS_CACHE_ERROR_CORRECTION_NONE,
    SMBIOS_CACHE_TYPE_DATA,
    SMBIOS_CACHE_ASSOCIATIVITY_4_WAY_SET
};

SMBIOS_CACHE_INFORMATION EfiPandaSmbiosL2Cache = {
    {
        SmbiosCacheInformation,
        sizeof(SMBIOS_CACHE_INFORMATION),
        0x0107
    },

    0,
    SMBIOS_CACHE_ENABLED | SMBIOS_CACHE_WRITE_BACK,
    PANDA_SMBIOS_CACHE_L2_SIZE,
    PANDA_SMBIOS_CACHE_L2_SIZE,
    SMBIOS_CACHE_SRAM_UNKNOWN,
    SMBIOS_CACHE_SRAM_UNKNOWN,
    0,
    SMBIOS_CACHE_ERROR_CORRECTION_NONE,
    SMBIOS_CACHE_TYPE_DATA,
    SMBIOS_CACHE_ASSOCIATIVITY_16_WAY_SET
};

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipPandaCreateSmbiosTables (
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

    UINT32 DieId[4];
    CHAR8 *ProcessorPart;
    OMAP4_REVISION ProcessorRevision;
    CHAR8 *ProductName;
    CHAR8 SerialNumber[33];
    EFI_STATUS Status;

    Status = EfiSmbiosAddStructure(&EfiPandaSmbiosBiosInformation,
                                   PANDA_SMBIOS_BIOS_VENDOR,
                                   PANDA_FIRMWARE_VERSION_STRING,
                                   PANDA_FIRMWARE_BUILD_DATE,
                                   NULL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Set the die ID as the UUID.
    //

    DieId[0] = EfiReadRegister32((VOID *)OMAP4430_FUSE_DIE_ID0);
    DieId[1] = EfiReadRegister32((VOID *)OMAP4430_FUSE_DIE_ID1);
    DieId[2] = EfiReadRegister32((VOID *)OMAP4430_FUSE_DIE_ID2);
    DieId[3] = EfiReadRegister32((VOID *)OMAP4430_FUSE_DIE_ID3);
    RtlPrintToString(SerialNumber,
                     sizeof(SerialNumber),
                     CharacterEncodingAscii,
                     "%08X%08X%08X%08X",
                     DieId[0],
                     DieId[1],
                     DieId[2],
                     DieId[3]);

    RtlCopyMemory(EfiPandaSmbiosSystemInformation.Uuid,
                  DieId,
                  sizeof(EfiPandaSmbiosSystemInformation.Uuid));

    ProcessorRevision = EfipOmap4GetRevision();
    ProductName = PANDA_SMBIOS_SYSTEM_PRODUCT_NAME;
    if (ProcessorRevision >= Omap4460RevisionEs10) {
        ProductName = PANDA_SMBIOS_SYSTEM_PRODUCT_NAME_ES;
    }

    Status = EfiSmbiosAddStructure(&EfiPandaSmbiosSystemInformation,
                                   PANDA_SMBIOS_SYSTEM_MANUFACTURER,
                                   ProductName,
                                   PANDA_SMBIOS_SYSTEM_PRODUCT_VERSION,
                                   SerialNumber,
                                   NULL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfiSmbiosAddStructure(&EfiPandaSmbiosModuleInformation,
                                   PANDA_SMBIOS_MODULE_MANUFACTURER,
                                   ProductName,
                                   NULL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfiSmbiosAddStructure(&EfiPandaSmbiosEnclosure, NULL);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    ProcessorPart = PANDA_SMBIOS_PROCESSOR_PART_4430;
    if (ProcessorRevision >= Omap4460RevisionEs10) {
        ProcessorPart = PANDA_SMBIOS_PROCESSOR_PART_4460;
        EfiPandaSmbiosProcessorInformation.MaxSpeed =
                                         PANDA_SMBIOS_PROCESSOR_MAX_SPEED_4460;
    }

    Status = EfiSmbiosAddStructure(&EfiPandaSmbiosProcessorInformation,
                                   PANDA_SMBIOS_PROCESSOR_MANUFACTURER,
                                   SerialNumber,
                                   ProcessorPart,
                                   NULL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfiSmbiosAddStructure(&EfiPandaSmbiosL1Cache, NULL);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfiSmbiosAddStructure(&EfiPandaSmbiosL2Cache, NULL);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

