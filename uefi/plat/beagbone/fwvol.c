/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    fwvol.c

Abstract:

    This module implements support for the builtin UEFI firmware volume.

Author:

    Evan Green 19-Dec-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "uefifw.h"
#include "bbonefw.h"

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

extern UINT8 _binary_bbonefwv_start;
extern UINT8 _binary_bbonefwv_end;

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

    Base = (EFI_PHYSICAL_ADDRESS)(UINTN)&_binary_bbonefwv_start;
    Size = (UINTN)&_binary_bbonefwv_end - (UINTN)&_binary_bbonefwv_start;
    Status = EfiCreateFirmwareVolume(Base, Size, NULL, 0, NULL);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

