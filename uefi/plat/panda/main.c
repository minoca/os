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
    of the TI PandaBoard.

Author:

    Evan Green 27-Feb-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "dev/omap4.h"
#include "pandafw.h"

//
// ---------------------------------------------------------------- Definitions
//

#define FIRMWARE_IMAGE_NAME "pandafw.elf"

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
// Store a pointer to the GPIO register blocks.
//

VOID *EfiOmap4Gpio1Address = (VOID *)OMAP4430_GPIO1_BASE;
VOID *EfiOmap4Gpio2Address = (VOID *)OMAP4430_GPIO2_BASE;

//
// Store a boolean used for debugging that disables the watchdog timer.
//

BOOLEAN EfiDisableWatchdog = FALSE;

//
// ------------------------------------------------------------------ Functions
//

__USED
VOID
EfiPandaBoardMain (
    VOID *TopOfStack,
    UINTN StackSize
    )

/*++

Routine Description:

    This routine is the C entry point for the firmware.

Arguments:

    TopOfStack - Supplies the top of the stack that has been set up for the
        loader.

    StackSize - Supplies the total size of the stack set up for the loader, in
        bytes.

Return Value:

    This routine does not return.

--*/

{

    UINTN FirmwareSize;

    //
    // Initialize UEFI enough to get into the debugger.
    //

    FirmwareSize = (UINTN)&_end - (UINTN)&__executable_start;
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

    if (Phase == 0) {
        if (EfiDisableWatchdog != FALSE) {
            EfiPlatformSetWatchdogTimer(0, 0, 0, NULL);
        }

        EfipOmap4InitializePowerAndClocks();

    } else if (Phase == 1) {
        EfipOmap4UsbInitialize();
        Status = EfipSmpInitialize();
        if (EFI_ERROR(Status)) {
            return Status;
        }

        Status = EfipPandaCreateSmbiosTables();
        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    return EFI_SUCCESS;
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

    Status = EfipPandaEnumerateSd();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfipPandaEnumerateVideo();
    EfipPandaEnumerateSerial();
    Status = EfipEnumerateRamDisks();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

