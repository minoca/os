/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    main.c

Abstract:

    This module implements the entry point for the UEFI firmware running on top
    of the RK3288 Veyron.

Author:

    Evan Green 10-Jul-2015

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "veyronfw.h"

//
// ---------------------------------------------------------------- Definitions
//

#define FIRMWARE_IMAGE_NAME "veyronfw.elf"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
EfipVeyronConfigureArmPll (
    VOID
    );

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
// Store a boolean used for debugging that disables the watchdog timer.
//

BOOLEAN EfiDisableWatchdog = FALSE;

//
// ------------------------------------------------------------------ Functions
//

VOID
EfiVeyronMain (
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
        initialized.

Return Value:

    EFI status code.

--*/

{

    EFI_STATUS Status;

    if (Phase == 0) {
        if (EfiDisableWatchdog != FALSE) {
            EfiPlatformSetWatchdogTimer(0, 0, 0, NULL);
        }

        EfipVeyronConfigureArmPll();

    } else if (Phase == 1) {
        Status = EfipSmpInitialize();
        if (EFI_ERROR(Status)) {
            return Status;
        }

        Status = EfipVeyronCreateSmbiosTables();
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

    Status = EfipVeyronEnumerateSd();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfipVeyronEnumerateVideo();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfipVeyronEnumerateSerial();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfipEnumerateRamDisks();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
EfipVeyronConfigureArmPll (
    VOID
    )

/*++

Routine Description:

    This routine configures the ARM PLL, since the 1800MHz set by the firmware
    actually seems to be too fast to run correctly.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Frequency;
    UINT32 Nf;
    UINT32 No;
    UINT32 Nr;
    UINT32 Value;

    //
    // Put the PLL in slow mode to bypass the PLL while it's being configured.
    //

    Value = (RK32_CRU_MODE_CONTROL_ARM_PLL_MODE_MASK << 16) |
            RK32_CRU_MODE_CONTROL_ARM_PLL_MODE_SLOW;

    RK32_WRITE_CRU(Rk32CruModeControl, Value);
    Frequency = VEYRON_ARM_CPU_HERTZ;
    Nr = 1;
    No = 1;
    Nf = (UINT32)((UINT64)Frequency * Nr * No / VEYRON_OSC_HERTZ);

    //
    // Reset the PLL.
    //

    Value = (RK32_PLL_RESET << 16) | RK32_PLL_RESET;
    RK32_WRITE_CRU(Rk32CruArmPllConfiguration3, Value);

    //
    // Configure the PLL.
    //

    Value = ((RK32_PLL_NR_MASK | RK32_PLL_OD_MASK) << 16) |
            ((Nr - 1) << RK32_PLL_NR_SHIFT) | (No - 1);

    RK32_WRITE_CRU(Rk32CruArmPllConfiguration0, Value);
    Value = (RK32_PLL_NF_MASK << 16) | (Nf - 1);
    RK32_WRITE_CRU(Rk32CruArmPllConfiguration1, Value);
    Value = (RK32_PLL_BWADJ_MASK << 16) | ((Nf >> 1) - 1);
    RK32_WRITE_CRU(Rk32CruArmPllConfiguration2, Value);
    EfiStall(10);

    //
    // Clear reset.
    //

    Value = RK32_PLL_RESET << 16;
    RK32_WRITE_CRU(Rk32CruArmPllConfiguration3, Value);

    //
    // Wait for the PLL to lock itself.
    //

    do {
        Value = RK32_READ_GRF(Rk32GrfSocStatus1);

    } while ((Value & RK32_GRF_SOC_STATUS1_ARM_PLL_LOCK) == 0);

    //
    // Enter normal mode on the PLL.
    //

    Value = (RK32_CRU_MODE_CONTROL_ARM_PLL_MODE_MASK << 16) |
            RK32_CRU_MODE_CONTROL_ARM_PLL_MODE_NORMAL;

    RK32_WRITE_CRU(Rk32CruModeControl, Value);
    return;
}

