/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    memmap.c

Abstract:

    This module implements support for returning the initial memory map on the
    Raspberry Pi 2.

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

    return EfipBcm2709GetInitialMemoryMap(Map, MapSize);
}

//
// --------------------------------------------------------- Internal Functions
//

