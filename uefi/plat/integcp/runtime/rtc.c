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
    Integrator/CP.

Author:

    Evan Green 7-Apr-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "../integfw.h"
#include "dev/pl031.h"

//
// ---------------------------------------------------------------- Definitions
//

#define INTEGRATOR_TIME_TO_EPOCH_DELTA (978307200LL)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

PL031_CONTEXT EfiIntegratorRtc;

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipIntegratorInitializeRtc (
    VOID
    )

/*++

Routine Description:

    This routine initializes support for the EFI time runtime services.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    EFI_STATUS Status;

    EfiSetMem(&EfiIntegratorRtc, sizeof(PL031_CONTEXT), 0);
    EfiIntegratorRtc.Base = (VOID *)INTEGRATOR_PL031_RTC_BASE;
    Status = EfipPl031Initialize(&EfiIntegratorRtc);
    if (!EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

VOID
EfipIntegratorRtcVirtualAddressChange (
    VOID
    )

/*++

Routine Description:

    This routine is called when the firmware is converting to virtual address
    mode. It converts any pointers it's got. This routine is called after
    ExitBootServices, so no EFI boot services are available.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    EfiConvertPointer(0, &(EfiIntegratorRtc.Base));
    return;
}

EFIAPI
EFI_STATUS
EfipIntegratorGetTime (
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

    EFI_STATUS Status;
    INT64 Value;
    INT32 Value32;

    if (Capabilities != NULL) {
        Capabilities->Resolution = 1;
        Capabilities->Accuracy = 0;
        Capabilities->SetsToZero = FALSE;
    }

    if (Time == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    EfiSetMem(Time, sizeof(EFI_TIME), 0);
    Status = EfipPl031GetTime(&EfiIntegratorRtc, (UINT32 *)&Value32);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Value = Value32 - INTEGRATOR_TIME_TO_EPOCH_DELTA;
    Status = EfiConvertCounterToEfiTime(Value, Time);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfipIntegratorSetTime (
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

    EFI_STATUS Status;
    INT64 Value;
    INT32 Value32;

    if (Time == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    Status = EfiConvertEfiTimeToCounter(Time, &Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Value += INTEGRATOR_TIME_TO_EPOCH_DELTA;
    Value32 = Value;
    if (Value32 != Value) {
        return EFI_INVALID_PARAMETER;
    }

    Status = EfipPl031SetTime(&EfiIntegratorRtc, Value32);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfipIntegratorGetWakeupTime (
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

    EFI_STATUS Status;
    INT64 Value;
    INT32 Value32;

    if ((Enabled == NULL) || (Pending == NULL) || (Time == NULL)) {
        return EFI_INVALID_PARAMETER;
    }

    EfiSetMem(Time, sizeof(EFI_TIME), 0);
    Status = EfipPl031GetWakeupTime(&EfiIntegratorRtc,
                                    Enabled,
                                    Pending,
                                    (UINT32 *)&Value32);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Value = Value32 - INTEGRATOR_TIME_TO_EPOCH_DELTA;
    Status = EfiConvertCounterToEfiTime(Value, Time);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfipIntegratorSetWakeupTime (
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

    EFI_STATUS Status;
    INT64 Value;
    INT32 Value32;

    Value = 0;
    if (Time != NULL) {
        Status = EfiConvertEfiTimeToCounter(Time, &Value);
        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    Value += INTEGRATOR_TIME_TO_EPOCH_DELTA;
    Value32 = Value;
    if (Value32 != Value) {
        return EFI_INVALID_PARAMETER;
    }

    Status = EfipPl031SetWakeupTime(&EfiIntegratorRtc, Enable, Value32);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

