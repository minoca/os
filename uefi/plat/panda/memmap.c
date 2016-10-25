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
#include "pandafw.h"

//
// ---------------------------------------------------------------- Definitions
//

#define PANDA_BOARD_MEMORY_MAP_SIZE \
    (sizeof(EfiPandaBoardMemoryMap) / sizeof(EfiPandaBoardMemoryMap[0]))

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

EFI_MEMORY_DESCRIPTOR EfiPandaBoardMemoryMap[] = {
    {
        EfiConventionalMemory,
        0,
        PANDA_RAM_START,
        0,
        PANDA_RAM_SIZE / EFI_PAGE_SIZE,
        0
    },

    {
        EfiRuntimeServicesData,
        0,
        OMAP4430_L4_I2C_BASE,
        0,
        EFI_SIZE_TO_PAGES(OMAP4430_L4_I2C_SIZE),
        EFI_MEMORY_UC | EFI_MEMORY_RUNTIME
    },

    {
        EfiRuntimeServicesData,
        0,
        OMAP4430_PRM_BASE,
        0,
        EFI_SIZE_TO_PAGES(OMAP4430_PRM_SIZE),
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

    *Map = EfiPandaBoardMemoryMap;
    *MapSize = PANDA_BOARD_MEMORY_MAP_SIZE;
    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

