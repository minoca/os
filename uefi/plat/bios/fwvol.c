/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fwvol.c

Abstract:

    This module implements support for the builtin UEFI firmware volume.

Author:

    Evan Green 10-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "uefifw.h"
#include "biosfw.h"

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

extern UINT8 _binary_biosfwv_start;
extern UINT8 _binary_biosfwv_end;

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

    Base = (EFI_PHYSICAL_ADDRESS)(UINTN)&_binary_biosfwv_start;
    Size = (UINTN)&_binary_biosfwv_end - (UINTN)&_binary_biosfwv_start;
    Status = EfiCreateFirmwareVolume(Base, Size, NULL, 0, NULL);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

