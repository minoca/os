/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    smbios.c

Abstract:

    This module implements SMBIOS tables for the Integrator/CP.

Author:

    Evan Green 7-May-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <minoca/fw/smbios.h>
#include "uefifw.h"
#include "integfw.h"

//
// ---------------------------------------------------------------- Definitions
//

#define INTEGRATOR_SMBIOS_BIOS_VENDOR "Minoca Corp"

#define INTEGRATOR_SMBIOS_SYSTEM_MANUFACTURER "Qemu"
#define INTEGRATOR_SMBIOS_SYSTEM_PRODUCT_NAME "Integrator/CP"
#define INTEGRATOR_SMBIOS_SYSTEM_PRODUCT_VERSION "0.13"

#define INTEGRATOR_SMBIOS_MODULE_MANUFACTURER "Qemu"
#define INTEGRATOR_SMBIOS_MODULE_PRODUCT "Integrator/CP"

#define INTEGRATOR_SMBIOS_PROCESSOR_MANUFACTURER "ARM"
#define INTEGRATOR_SMBIOS_PROCESSOR_PART "Generic ARMv7"
#define INTEGRATOR_SMBIOS_PROCESSOR_EXTERNAL_CLOCK 0
#define INTEGRATOR_SMBIOS_PROCESSOR_MAX_SPEED 0
#define INTEGRATOR_SMBIOS_PROCESSOR_CURRENT_SPEED 0
#define INTEGRATOR_SMBIOS_PROCESSOR_CORE_COUNT 1

#define INTEGRATOR_SMBIOS_CACHE_L1_SIZE 0
#define INTEGRATOR_SMBIOS_CACHE_L2_SIZE 0

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

SMBIOS_BIOS_INFORMATION EfiIntegratorSmbiosBiosInformation = {
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

SMBIOS_SYSTEM_INFORMATION EfiIntegratorSmbiosSystemInformation = {
    {
        SmbiosSystemInformation,
        sizeof(SMBIOS_SYSTEM_INFORMATION),
        0x0101
    },

    1,
    2,
    3,
    0,
    {0},
    SMBIOS_SYSTEM_WAKEUP_UNKNOWN,
    3,
    2
};

SMBIOS_MODULE_INFORMATION EfiIntegratorSmbiosModuleInformation = {
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

SMBIOS_ENCLOSURE EfiIntegratorSmbiosEnclosure = {
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

SMBIOS_PROCESSOR_INFORMATION EfiIntegratorSmbiosProcessorInformation = {
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
    INTEGRATOR_SMBIOS_PROCESSOR_EXTERNAL_CLOCK,
    INTEGRATOR_SMBIOS_PROCESSOR_MAX_SPEED,
    INTEGRATOR_SMBIOS_PROCESSOR_CURRENT_SPEED,
    SMBIOS_PROCESSOR_STATUS_ENABLED,
    0,
    0xFFFF,
    0xFFFF,
    0xFFFF,
    0,
    0,
    2,
    INTEGRATOR_SMBIOS_PROCESSOR_CORE_COUNT,
    0,
    SMBIOS_PROCESSOR_CHARACTERISTIC_UNKNOWN,
    0
};

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipIntegratorCreateSmbiosTables (
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

    EfiIntegratorSmbiosBiosInformation.BiosMajorRelease = EfiVersionMajor;
    EfiIntegratorSmbiosBiosInformation.BiosMinorRelease = EfiVersionMinor;
    Status = EfiSmbiosAddStructure(&EfiIntegratorSmbiosBiosInformation,
                                   INTEGRATOR_SMBIOS_BIOS_VENDOR,
                                   EfiBuildString,
                                   EfiBuildTimeString,
                                   NULL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfiSmbiosAddStructure(&EfiIntegratorSmbiosSystemInformation,
                                   INTEGRATOR_SMBIOS_SYSTEM_MANUFACTURER,
                                   INTEGRATOR_SMBIOS_SYSTEM_PRODUCT_NAME,
                                   INTEGRATOR_SMBIOS_SYSTEM_PRODUCT_VERSION,
                                   NULL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfiSmbiosAddStructure(&EfiIntegratorSmbiosModuleInformation,
                                   INTEGRATOR_SMBIOS_MODULE_MANUFACTURER,
                                   INTEGRATOR_SMBIOS_MODULE_PRODUCT,
                                   NULL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfiSmbiosAddStructure(&EfiIntegratorSmbiosEnclosure, NULL);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfiSmbiosAddStructure(&EfiIntegratorSmbiosProcessorInformation,
                                   INTEGRATOR_SMBIOS_PROCESSOR_MANUFACTURER,
                                   INTEGRATOR_SMBIOS_PROCESSOR_PART,
                                   NULL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

