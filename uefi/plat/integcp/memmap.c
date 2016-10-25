/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    memmap.c

Abstract:

    This module implements support for returning the initial memory map on the
    TI PandaBoard.

Author:

    Evan Green 27-Feb-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "integfw.h"

//
// ---------------------------------------------------------------- Definitions
//

#define INTEGRATOR_MEMORY_MAP_SIZE \
    (sizeof(EfiIntegratorMemoryMap) / sizeof(EfiIntegratorMemoryMap[0]))

#define INTEGRATOR_SDRAM_REGISTER (INTEGRATOR_CM_BASE + 0x20)

#define INTEGRATOR_SDRAM_MASK 0x1C
#define INTEGRATOR_SDRAM_32M 0x04
#define INTEGRATOR_SDRAM_64M 0x08
#define INTEGRATOR_SDRAM_128M 0x0C
#define INTEGRATOR_SDRAM_256M 0x10

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// Define the initial memory map.
//

EFI_MEMORY_DESCRIPTOR EfiIntegratorMemoryMap[] = {
    {
        EfiConventionalMemory,
        0,
        INTEGRATOR_RAM_START,
        0,
        INTEGRATOR_RAM_SIZE / EFI_PAGE_SIZE,
        0
    },

    {
        EfiRuntimeServicesData,
        0,
        INTEGRATOR_CM_BASE,
        0,
        EFI_SIZE_TO_PAGES(INTEGRATOR_CM_SIZE),
        EFI_MEMORY_UC | EFI_MEMORY_RUNTIME
    },

    {
        EfiRuntimeServicesData,
        0,
        INTEGRATOR_PL031_RTC_BASE,
        0,
        EFI_SIZE_TO_PAGES(INTEGRATOR_PL031_RTC_SIZE),
        EFI_MEMORY_UC | EFI_MEMORY_RUNTIME
    },
};

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfiPlatformGetInitialMemoryMap (
    EFI_MEMORY_DESCRIPTOR **Map,
    UINTN *MapSize
    )

/*++

Routine Description:

    This routine returns the initial platform memory map to the EFI core. The
    core maintains this memory map. The memory map returned does not need to
    take into account the firmware image itself or stack, the EFI core will
    reserve those regions automatically.

Arguments:

    Map - Supplies a pointer where the array of memory descriptors constituting
        the initial memory map is returned on success. The EFI core will make
        a copy of these descriptors, so they can be in read-only or
        temporary memory.

    MapSize - Supplies a pointer where the number of elements in the initial
        memory map will be returned on success.

Return Value:

    EFI status code.

--*/

{

    UINT32 Megabyte;
    UINT32 SdRamRegister;

    SdRamRegister = EfiReadRegister32((VOID *)INTEGRATOR_SDRAM_REGISTER);
    Megabyte = (1024 * 1024) / EFI_PAGE_SIZE;
    switch (SdRamRegister & INTEGRATOR_SDRAM_MASK) {
    case INTEGRATOR_SDRAM_32M:
        EfiIntegratorMemoryMap[0].NumberOfPages = 32 * Megabyte;
        break;

    case INTEGRATOR_SDRAM_64M:
        EfiIntegratorMemoryMap[0].NumberOfPages = 64 * Megabyte;
        break;

    case INTEGRATOR_SDRAM_128M:
        EfiIntegratorMemoryMap[0].NumberOfPages = 128 * Megabyte;
        break;

    case INTEGRATOR_SDRAM_256M:
        EfiIntegratorMemoryMap[0].NumberOfPages = 256 * Megabyte;
        break;

    default:
        break;
    }

    *Map = EfiIntegratorMemoryMap;
    *MapSize = INTEGRATOR_MEMORY_MAP_SIZE;
    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

