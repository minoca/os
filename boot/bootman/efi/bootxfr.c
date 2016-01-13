/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    bootxfr.c

Abstract:

    This module implements support for transition between the boot manager and
    another boot application.

Author:

    Evan Green 24-Feb-2014

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include <minoca/uefi/uefi.h>
#include "firmware.h"
#include "bootlib.h"
#include "efisup.h"
#include "bootman.h"

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

KSTATUS
BmpFwInitializeBootBlock (
    PBOOT_INITIALIZATION_BLOCK Parameters,
    PBOOT_VOLUME OsVolume
    )

/*++

Routine Description:

    This routine initializes the boot initialization block that is passed when
    control is handed off to the next boot application.

Arguments:

    Parameters - Supplies a pointer to the boot initialization block.

    OsVolume - Supplies a pointer to the open volume containing the application
        to be launched.

Return Value:

    Status code.

--*/

{

    //
    // All memory regions are reflected in the firmware memory map, so there's
    // no need to set up the reserved region array. Save pointers to what EFI
    // passed this application. Note that the image handle type is a pointer to
    // and EFI image handle, in case EFI ever changes the size of an EFI_HANDLE.
    //

    Parameters->EfiImageHandle = &BoEfiImageHandle;
    Parameters->EfiSystemTable = BoEfiSystemTable;
    return STATUS_SUCCESS;
}

INT
BmpFwTransferToBootApplication (
    PBOOT_INITIALIZATION_BLOCK Parameters,
    PBOOT_APPLICATION_ENTRY EntryPoint
    )

/*++

Routine Description:

    This routine transfers control to another boot application.

Arguments:

    Parameters - Supplies a pointer to the initialization block.

    EntryPoint - Supplies tne address of the entry point routine of the new
        application.

Return Value:

    Returns the integer return value from the application. Often does not
    return on success.

--*/

{

    INT Result;

    BopEfiRestoreFirmwareContext();
    Result = EntryPoint(Parameters);
    BopEfiRestoreApplicationContext();
    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

