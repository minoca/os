/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

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

extern UINT8 _binary_veyrnfwv_start;
extern UINT8 _binary_veyrnfwv_end;

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

    Base = (EFI_PHYSICAL_ADDRESS)(UINTN)&_binary_veyrnfwv_start;
    Size = (UINTN)&_binary_veyrnfwv_end - (UINTN)&_binary_veyrnfwv_start;
    Status = EfiCreateFirmwareVolume(Base, Size, NULL, 0, NULL);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

