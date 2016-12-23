/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rtc.c

Abstract:

    This module implements support for speaking to the RTC module on the
    TWL6030 PMIC of the PandaBoard.

Author:

    Evan Green 26-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "../pandafw.h"

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

EFIAPI
EFI_STATUS
EfipOmap4GetTime (
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

    if (Capabilities != NULL) {
        Capabilities->Resolution = 1;
        Capabilities->Accuracy = 0;
        Capabilities->SetsToZero = FALSE;
    }

    return Omap4Twl6030ReadRtc(Time);
}

EFIAPI
EFI_STATUS
EfipOmap4SetTime (
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

    return Omap4Twl6030WriteRtc(Time);
}

EFIAPI
EFI_STATUS
EfipOmap4GetWakeupTime (
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

    return Omap4Twl6030ReadRtcWakeupTime(Enabled, Pending, Time);
}

EFIAPI
EFI_STATUS
EfipOmap4SetWakeupTime (
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

    return Omap4Twl6030WriteRtcWakeupTime(Enable, Time);
}

//
// --------------------------------------------------------- Internal Functions
//

