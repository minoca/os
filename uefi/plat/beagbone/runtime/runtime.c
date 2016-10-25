/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    runtime.c

Abstract:

    This module implements platform-specific runtime code for the BeagleBone
    system.

Author:

    Evan Green 6-Jan-2015

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include <minoca/soc/am335x.h>
#include "../bbonefw.h"

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

EFI_STATUS
EfiPlatformRuntimeInitialize (
    VOID
    )

/*++

Routine Description:

    This routine performs platform-specific firmware initialization in the
    runtime core driver. The runtime routines are in a separate binary from the
    firmware core routines as they need to be relocated for runtime. This
    routine should perform platform-specific initialization needed to provide
    the core runtime services.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    EfiRuntimeServices->GetTime = EfipAm335GetTime;
    EfiRuntimeServices->SetTime = EfipAm335SetTime;
    EfiRuntimeServices->GetWakeupTime = EfipAm335GetWakeupTime;
    EfiRuntimeServices->SetWakeupTime = EfipAm335SetWakeupTime;
    EfiRuntimeServices->ResetSystem = EfipAm335ResetSystem;
    return EFI_SUCCESS;
}

EFI_STATUS
EfiPlatformReadNonVolatileData (
    VOID *Data,
    UINTN DataSize
    )

/*++

Routine Description:

    This routine reads the EFI variable data from non-volatile storage.

Arguments:

    Data - Supplies a pointer where the platform returns the non-volatile
        data.

    DataSize - Supplies the size of the data to return.

Return Value:

    EFI_SUCCESS if some data was successfully loaded.

    EFI_UNSUPPORTED if the platform does not have non-volatile storage. In this
    case the firmware core saves the non-volatile variables to a file on the
    EFI system partition, and the variable library hopes to catch the same
    variable buffer on reboots to see variable writes that happened at
    runtime.

    EFI_DEVICE_IO_ERROR if a device error occurred during the operation.

    Other error codes on other failures.

--*/

{

    return EFI_UNSUPPORTED;
}

EFI_STATUS
EfiPlatformWriteNonVolatileData (
    VOID *Data,
    UINTN DataSize
    )

/*++

Routine Description:

    This routine writes the EFI variable data to non-volatile storage.

Arguments:

    Data - Supplies a pointer to the data to write.

    DataSize - Supplies the size of the data to write, in bytes.

Return Value:

    EFI_SUCCESS if some data was successfully loaded.

    EFI_UNSUPPORTED if the platform does not have non-volatile storage. In this
    case the firmware core saves the non-volatile variables to a file on the
    EFI system partition, and the variable library hopes to catch the same
    variable buffer on reboots to see variable writes that happened at
    runtime.

    EFI_DEVICE_IO_ERROR if a device error occurred during the operation.

    Other error codes on other failures.

--*/

{

    return EFI_UNSUPPORTED;
}

VOID
EfiPlatformRuntimeExitBootServices (
    VOID
    )

/*++

Routine Description:

    This routine is called in the runtime core driver when the firmware is in
    the process of terminating boot services. The platform can do any work it
    needs to prepare for the imminent termination of boot services.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // Turn off all the LEDs.
    //

    EfipBeagleBoneBlackSetLeds(0);
    return;
}

VOID
EfiPlatformRuntimeVirtualAddressChange (
    VOID
    )

/*++

Routine Description:

    This routine is called in the runtime core driver when the firmware is
    converting to virtual address mode. It should convert any pointers it's
    got. This routine is called after ExitBootServices, so no EFI boot services
    are available.

Arguments:

    None.

Return Value:

    None.

--*/

{

    EfiConvertPointer(0, &EfiAm335PrmDeviceBase);
    EfiConvertPointer(0, &EfiAm335RtcBase);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

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
    *((UINT32 *)(AM335_GPIO_1_BASE + AM335_GPIO_SET_DATA_OUT)) = Value;
    Value = (~Leds & 0x0F) << 21;
    *((UINT32 *)(AM335_GPIO_1_BASE + AM335_GPIO_CLEAR_DATA_OUT)) = Value;
    return;
}

