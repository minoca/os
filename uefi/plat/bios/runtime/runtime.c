/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    runtime.c

Abstract:

    This module implements platform-specific runtime code for the PC/AT BIOS
    system.

Author:

    Evan Green 18-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "../biosfw.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

EFIAPI
EFI_STATUS
EfipPcatGetTime (
    EFI_TIME *Time,
    EFI_TIME_CAPABILITIES *Capabilities
    );

EFIAPI
EFI_STATUS
EfipPcatSetTime (
    EFI_TIME *Time
    );

EFIAPI
EFI_STATUS
EfipPcatGetWakeupTime (
    BOOLEAN *Enabled,
    BOOLEAN *Pending,
    EFI_TIME *Time
    );

EFIAPI
EFI_STATUS
EfipPcatSetWakeupTime (
    BOOLEAN Enable,
    EFI_TIME *Time
    );

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

    EfipPcatInitializeReset();

    //
    // Take over the runtime services. The runtime library recomputes the
    // CRC so there's no need to do it here.
    //

    EfiRuntimeServices->GetTime = EfipPcatGetTime;
    EfiRuntimeServices->SetTime = EfipPcatSetTime;
    EfiRuntimeServices->GetWakeupTime = EfipPcatGetWakeupTime;
    EfiRuntimeServices->SetWakeupTime = EfipPcatSetWakeupTime;
    EfiRuntimeServices->ResetSystem = EfipPcatResetSystem;
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

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

EFIAPI
EFI_STATUS
EfipPcatGetTime (
    EFI_TIME *Time,
    EFI_TIME_CAPABILITIES *Capabilities
    )

/*++

Routine Description:

    This routine returns the current time and dat information, and
    timekeeping capabilities of the hardware platform.

Arguments:

    Time - Supplies a pointer where the current time will be returned.

    Capabilities - Supplies an optional pointer where the capabilities will be
        returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the time parameter was NULL.

    EFI_DEVICE_ERROR if there was a hardware error accessing the device.

    EFI_UNSUPPORTED if the wakeup timer is not supported on this platform.

--*/

{

    return EFI_UNSUPPORTED;
}

EFIAPI
EFI_STATUS
EfipPcatSetTime (
    EFI_TIME *Time
    )

/*++

Routine Description:

    This routine sets the current local time and date information.

Arguments:

    Time - Supplies a pointer to the time to set.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if a time field is out of range.

    EFI_DEVICE_ERROR if there was a hardware error accessing the device.

    EFI_UNSUPPORTED if the wakeup timer is not supported on this platform.

--*/

{

    return EFI_UNSUPPORTED;
}

EFIAPI
EFI_STATUS
EfipPcatGetWakeupTime (
    BOOLEAN *Enabled,
    BOOLEAN *Pending,
    EFI_TIME *Time
    )

/*++

Routine Description:

    This routine gets the current wake alarm setting.

Arguments:

    Enabled - Supplies a pointer that receives a boolean indicating if the
        alarm is currently enabled or disabled.

    Pending - Supplies a pointer that receives a boolean indicating if the
        alarm signal is pending and requires acknowledgement.

    Time - Supplies a pointer that receives the current wake time.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if any parameter is NULL.

    EFI_DEVICE_ERROR if there was a hardware error accessing the device.

    EFI_UNSUPPORTED if the wakeup timer is not supported on this platform.

--*/

{

    return EFI_UNSUPPORTED;
}

EFIAPI
EFI_STATUS
EfipPcatSetWakeupTime (
    BOOLEAN Enable,
    EFI_TIME *Time
    )

/*++

Routine Description:

    This routine sets the current wake alarm setting.

Arguments:

    Enable - Supplies a boolean enabling or disabling the wakeup timer.

    Time - Supplies an optional pointer to the time to set. This parameter is
        only optional if the enable parameter is FALSE.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if a time field is out of range.

    EFI_DEVICE_ERROR if there was a hardware error accessing the device.

    EFI_UNSUPPORTED if the wakeup timer is not supported on this platform.

--*/

{

    return EFI_UNSUPPORTED;
}

