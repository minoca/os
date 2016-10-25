/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    smbios.c

Abstract:

    This module implements SMBIOS tables for the RK3288-based Veyron board.

Author:

    Evan Green 9-Jul-2015

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <minoca/fw/smbios.h>
#include "uefifw.h"
#include "veyronfw.h"

//
// ---------------------------------------------------------------- Definitions
//

#define VEYRON_SMBIOS_BIOS_VENDOR "Minoca Corp"

#define VEYRON_SMBIOS_SYSTEM_MANUFACTURER "ASUS"
#define VEYRON_SMBIOS_SYSTEM_PRODUCT "C201"
#define VEYRON_SMBIOS_SYSTEM_VERSION "1"

#define VEYRON_SMBIOS_MODULE_MANUFACTURER "RockChip"
#define VEYRON_SMBIOS_MODULE_PRODUCT "RK3288"

#define VEYRON_SMBIOS_PROCESSOR_MANUFACTURER "ARM"
#define VEYRON_SMBIOS_PROCESSOR_PART "A17"
#define VEYRON_SMBIOS_PROCESSOR_EXTERNAL_CLOCK 24
#define VEYRON_SMBIOS_PROCESSOR_MAX_SPEED 1800
#define VEYRON_SMBIOS_PROCESSOR_CURRENT_SPEED 1800
#define VEYRON_SMBIOS_PROCESSOR_CORE_COUNT 4

#define VEYRON_SMBIOS_CACHE_L1_SIZE 32
#define VEYRON_SMBIOS_CACHE_L2_SIZE 1024

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

SMBIOS_BIOS_INFORMATION EfiVeyronSmbiosBiosInformation = {
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
    0,
    0,
    0,
    0
};

SMBIOS_SYSTEM_INFORMATION EfiVeyronSmbiosSystemInformation = {
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

SMBIOS_MODULE_INFORMATION EfiVeyronSmbiosModuleInformation = {
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

SMBIOS_ENCLOSURE EfiVeyronSmbiosEnclosure = {
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

SMBIOS_PROCESSOR_INFORMATION EfiVeyronSmbiosProcessorInformation = {
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
    VEYRON_SMBIOS_PROCESSOR_EXTERNAL_CLOCK,
    VEYRON_SMBIOS_PROCESSOR_MAX_SPEED,
    VEYRON_SMBIOS_PROCESSOR_CURRENT_SPEED,
    SMBIOS_PROCESSOR_STATUS_ENABLED,
    0,
    0x0106,
    0x0107,
    0xFFFF,
    2,
    0,
    3,
    VEYRON_SMBIOS_PROCESSOR_CORE_COUNT,
    0,
    SMBIOS_PROCESSOR_CHARACTERISTIC_UNKNOWN,
    0
};

SMBIOS_CACHE_INFORMATION EfiVeyronSmbiosL1Cache = {
    {
        SmbiosCacheInformation,
        sizeof(SMBIOS_CACHE_INFORMATION),
        0x0106
    },

    0,
    SMBIOS_CACHE_ENABLED | SMBIOS_CACHE_WRITE_BACK,
    VEYRON_SMBIOS_CACHE_L1_SIZE,
    VEYRON_SMBIOS_CACHE_L1_SIZE,
    SMBIOS_CACHE_SRAM_UNKNOWN,
    SMBIOS_CACHE_SRAM_UNKNOWN,
    0,
    SMBIOS_CACHE_ERROR_CORRECTION_NONE,
    SMBIOS_CACHE_TYPE_DATA,
    SMBIOS_CACHE_ASSOCIATIVITY_4_WAY_SET
};

SMBIOS_CACHE_INFORMATION EfiVeyronSmbiosL2Cache = {
    {
        SmbiosCacheInformation,
        sizeof(SMBIOS_CACHE_INFORMATION),
        0x0107
    },

    0,
    SMBIOS_CACHE_ENABLED | SMBIOS_CACHE_WRITE_BACK,
    VEYRON_SMBIOS_CACHE_L2_SIZE,
    VEYRON_SMBIOS_CACHE_L2_SIZE,
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
EfipVeyronCreateSmbiosTables (
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

    EFI_STATUS Status;

    EfiVeyronSmbiosBiosInformation.BiosMajorRelease = EfiVersionMajor;
    EfiVeyronSmbiosBiosInformation.BiosMinorRelease = EfiVersionMinor;
    Status = EfiSmbiosAddStructure(&EfiVeyronSmbiosBiosInformation,
                                   VEYRON_SMBIOS_BIOS_VENDOR,
                                   EfiBuildString,
                                   EfiBuildTimeString,
                                   NULL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // TODO: Determine if there is some sort of SoC or board serial number.
    //

    Status = EfiSmbiosAddStructure(&EfiVeyronSmbiosSystemInformation,
                                   VEYRON_SMBIOS_SYSTEM_MANUFACTURER,
                                   VEYRON_SMBIOS_SYSTEM_PRODUCT,
                                   VEYRON_SMBIOS_SYSTEM_VERSION,
                                   "",
                                   NULL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfiSmbiosAddStructure(&EfiVeyronSmbiosModuleInformation,
                                   VEYRON_SMBIOS_MODULE_MANUFACTURER,
                                   VEYRON_SMBIOS_MODULE_PRODUCT,
                                   NULL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfiSmbiosAddStructure(&EfiVeyronSmbiosEnclosure, NULL);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfiSmbiosAddStructure(&EfiVeyronSmbiosProcessorInformation,
                                   VEYRON_SMBIOS_PROCESSOR_MANUFACTURER,
                                   "",
                                   VEYRON_SMBIOS_PROCESSOR_PART,
                                   NULL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfiSmbiosAddStructure(&EfiVeyronSmbiosL1Cache, NULL);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfiSmbiosAddStructure(&EfiVeyronSmbiosL2Cache, NULL);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

