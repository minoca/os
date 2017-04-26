/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    main.c

Abstract:

    This module implements the entry point for the UEFI firmware running on top
    of a legacy PC/AT BIOS.

Author:

    Evan Green 27-Feb-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "biosfw.h"

//
// ---------------------------------------------------------------- Definitions
//

#define FIRMWARE_IMAGE_NAME "biosfw"

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
// Variables defined in the linker script that mark the start and end of the
// image.
//

extern INT8 _end;
extern INT8 __executable_start;

//
// ------------------------------------------------------------------ Functions
//

__USED
VOID
EfiBiosMain (
    VOID *TopOfStack,
    UINTN StackSize,
    UINT64 PartitionOffset,
    UINTN BootDriveNumber
    )

/*++

Routine Description:

    This routine is the C entry point for the firmware.

Arguments:

    TopOfStack - Supplies the top of the stack that has been set up for the
        loader.

    StackSize - Supplies the total size of the stack set up for the loader, in
        bytes.

    PartitionOffset - Supplies the offset, in sectors, from the start of the
        disk where this boot partition resides.

    BootDriveNumber - Supplies the drive number for the device that booted
        the system.

Return Value:

    This routine does not return.

--*/

{

    UINTN FirmwareSize;

    FirmwareSize = (UINTN)&_end - (UINTN)&__executable_start;
    TopOfStack = (VOID *)((UINTN)TopOfStack & (~EFI_PAGE_MASK));
    EfiCoreMain((VOID *)-1,
                &__executable_start,
                FirmwareSize,
                FIRMWARE_IMAGE_NAME,
                TopOfStack - StackSize,
                StackSize);

    return;
}

EFI_STATUS
EfiPlatformInitialize (
    UINT32 Phase
    )

/*++

Routine Description:

    This routine performs platform-specific firmware initialization.

Arguments:

    Phase - Supplies the iteration number this routine is being called on.
        Phase zero occurs very early, just after the debugger comes up.
        Phase one occurs a bit later, after timer and interrupt services are
        initialized. Phase two happens right before boot, after all platform
        devices have been enumerated.

Return Value:

    EFI status code.

--*/

{

    EFI_STATUS Status;

    Status = EFI_SUCCESS;
    if (Phase == 0) {
        EfiRsdpPointer = EfipPcatFindRsdp();
        if (EfiRsdpPointer == NULL) {
            return EFI_NOT_FOUND;
        }

    } else if (Phase == 1) {
        Status = EfipPcatInstallRsdp();
        if (EFI_ERROR(Status)) {
            return Status;
        }

        Status = EfipPcatInstallSmbios();
        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    return Status;
}

EFI_STATUS
EfiPlatformEnumerateDevices (
    VOID
    )

/*++

Routine Description:

    This routine enumerates and connects any builtin devices the platform
    contains.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    EFI_STATUS Status;

    Status = EfipPcatEnumerateDisks();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfipPcatEnumerateVideo();
    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

