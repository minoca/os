/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fwvol.c

Abstract:

    This module implements support for the builtin UEFI firmware volume.

Author:

    Evan Green 10-Jul-2015

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "uefifw.h"
#include "veyronfw.h"

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
// Objcopy adds these symbols surrounding the added file.
//

extern UINT8 _binary_veyronfwv_start;
extern UINT8 _binary_veyronfwv_end;

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfiPlatformEnumerateFirmwareVolumes (
    VOID
    )

/*++

Routine Description:

    This routine enumerates any firmware volumes the platform may have
    tucked away. The platform should load them into memory and call
    EfiCreateFirmwareVolume for each one.

Arguments:

    None.

Return Value:

    EFI Status code.

--*/

{

    EFI_PHYSICAL_ADDRESS Base;
    UINTN Size;
    EFI_STATUS Status;

    Base = (EFI_PHYSICAL_ADDRESS)(UINTN)&_binary_veyronfwv_start;
    Size = (UINTN)&_binary_veyronfwv_end - (UINTN)&_binary_veyronfwv_start;
    Status = EfiCreateFirmwareVolume(Base, Size, NULL, 0, NULL);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

