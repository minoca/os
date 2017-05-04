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
    of the TI BeagleBone Black.

Author:

    Evan Green 19-Dec-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include <minoca/soc/am335x.h>
#include "bbonefw.h"

//
// ---------------------------------------------------------------- Definitions
//

#define FIRMWARE_IMAGE_NAME "bbonefw.elf"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
EfipBeagleBoneBlackSetMacAddresses (
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
// Store the boot device type.
//

UINT32 EfiBootDeviceCode;

//
// ------------------------------------------------------------------ Functions
//

__USED
VOID
EfiBeagleBoneMain (
    VOID *TopOfStack,
    UINTN StackSize,
    UINT32 BootDevice
    )

/*++

Routine Description:

    This routine is the C entry point for the firmware.

Arguments:

    TopOfStack - Supplies the top of the stack that has been set up for the
        loader.

    StackSize - Supplies the total size of the stack set up for the loader, in
        bytes.

    BootDevice - Supplies the boot device code. See AM335_ROM_DEVICE_*
        definitions.

Return Value:

    This routine does not return.

--*/

{

    UINTN FirmwareSize;

    EfipBeagleBoneBlackSetLeds(4);
    FirmwareSize = (UINTN)&_end - (UINTN)&__executable_start;
    EfiBootDeviceCode = BootDevice;
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

        EfipAm335InitializePowerAndClocks();
        EfipBeagleBoneBlackInitializeRtc();

    } else if (Phase == 1) {
        Status = EfipBeagleBoneCreateSmbiosTables();
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

    EfipBeagleBoneBlackEnumerateVideo();
    EfipBeagleBoneBlackSetMacAddresses();
    Status = EfipBeagleBoneEnumerateStorage();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfipBeagleBoneEnumerateSerial();
    Status = EfipEnumerateRamDisks();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

VOID
EfipBeagleBoneBlackSetLeds (
    UINT32 Leds
    )

/*++

Routine Description:

    This routine sets the LEDs to a new value.

Arguments:

    Leds - Supplies the four bits containing whether to set the LEDs high or
        low.

Return Value:

    None.

--*/

{

    UINT32 Value;

    Value = (Leds & 0x0F) << 21;
    EfiWriteRegister32((VOID *)(AM335_GPIO_1_BASE + AM335_GPIO_SET_DATA_OUT),
                       Value);

    Value = (~Leds & 0x0F) << 21;
    EfiWriteRegister32((VOID *)(AM335_GPIO_1_BASE + AM335_GPIO_CLEAR_DATA_OUT),
                       Value);

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
EfipBeagleBoneBlackSetMacAddresses (
    VOID
    )

/*++

Routine Description:

    This routine sets the MAC addresses in the CPSW based on the data in the
    SOC Control region.

Arguments:

    None.

Return Value:

    None.

--*/

{

    VOID *CpswPort;
    VOID *SocControl;
    UINT32 Value;

    CpswPort = (VOID *)AM335_CPSW_PORT_REGISTERS;
    SocControl = (VOID *)AM335_SOC_CONTROL_REGISTERS;

    //
    // The SOC ID region stores unique MAC addresses for the two external
    // ethernet ports. Copy those values over to the ethernet controller so the
    // OS can discover them.
    //

    Value = EfiReadRegister32(SocControl + AM335_SOC_CONTROL_MAC_ID0_LOW);
    EfiWriteRegister32(CpswPort + AM335_CPSW_PORT1_SOURCE_ADDRESS_LOW, Value);
    Value = EfiReadRegister32(SocControl + AM335_SOC_CONTROL_MAC_ID0_HIGH);
    EfiWriteRegister32(CpswPort + AM335_CPSW_PORT1_SOURCE_ADDRESS_HIGH, Value);
    Value = EfiReadRegister32(SocControl + AM335_SOC_CONTROL_MAC_ID1_LOW);
    EfiWriteRegister32(CpswPort + AM335_CPSW_PORT2_SOURCE_ADDRESS_LOW, Value);
    Value = EfiReadRegister32(SocControl + AM335_SOC_CONTROL_MAC_ID1_HIGH);
    EfiWriteRegister32(CpswPort + AM335_CPSW_PORT2_SOURCE_ADDRESS_HIGH, Value);
    return;
}

