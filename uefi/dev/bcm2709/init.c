/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    init.c

Abstract:

    This module implements initialization support for the BCM2709 UEFI device
    library.

Author:

    Chris Stevens 19-Mar-2015

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include <dev/bcm2709.h>

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
// Store the base address of the BCM2709 device registers.
//

VOID *EfiBcm2709Base;

//
// Store whether or not the BCM2709 device library has been initialized.
//

BOOLEAN EfiBcm2709Initialized = FALSE;

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipBcm2709Initialize (
    VOID *PlatformBase
    )

/*++

Routine Description:

    This routine initializes the BCM2709 UEFI device library.

Arguments:

    PlatformBase - Supplies the base address for the BCM2709 device registers.

Return Value:

    Status code.

--*/

{

    if (PlatformBase == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    EfiBcm2709Base = PlatformBase;
    EfiBcm2709Initialized = TRUE;
    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

