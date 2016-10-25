/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ramdenum.c

Abstract:

    This module implements support for creating a Block I/O protocol from a
    RAM Disk device.

Author:

    Chris Stevens 19-Mar-2015

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>

//
// --------------------------------------------------------------------- Macros
//

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
// The RAM disk is embedded in the firmware image.
//

extern CHAR8 _binary_ramdisk_start;
extern CHAR8 _binary_ramdisk_end;

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipEnumerateRamDisks (
    VOID
    )

/*++

Routine Description:

    This routine enumerates any RAM disks embedded in the firmware.

Arguments:

    None.

Return Value:

    EFI Status code.

--*/

{

    EFI_PHYSICAL_ADDRESS Base;
    UINT64 Length;
    EFI_STATUS Status;

    Base = (EFI_PHYSICAL_ADDRESS)(UINTN)(&_binary_ramdisk_start);
    Length = (UINTN)(&_binary_ramdisk_end) - (UINTN)(&_binary_ramdisk_start);
    if (Length <= 0x100) {
        return EFI_SUCCESS;
    }

    Status = EfiCoreEnumerateRamDisk(Base, Length);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

