/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pmic.c

Abstract:

    This module implements support for the RK808 PMIC that usually accompanies
    the Rk32xx.

Author:

    Chris Stevens 20-Aug-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "../veyronfw.h"

//
// ---------------------------------------------------------------- Definitions
//

#define RK808_CHIP 0x1B

//
// Define the RK808 registers.
//

#define RK808_RTC_SECONDS             0x00
#define RK808_RTC_MINUTES             0x01
#define RK808_RTC_HOURS               0x02
#define RK808_RTC_DAYS                0x03
#define RK808_RTC_MONTHS              0x04
#define RK808_RTC_YEARS               0x05
#define RK808_RTC_WEEKS               0x06
#define RK808_RTC_ALARM_SECONDS       0x08
#define RK808_RTC_ALARM_MINUTES       0x09
#define RK808_RTC_ALARM_HOURS         0x0A
#define RK808_RTC_ALARM_DAYS          0x0B
#define RK808_RTC_ALARM_MONTHS        0x0C
#define RK808_RTC_ALARM_YEARS         0x0D
#define RK808_RTC_CONTROL             0x10
#define RK808_RTC_STATUS              0x11
#define RK808_RTC_INTERRUPTS          0x12
#define RK808_RTC_COMPENSATION_LOW    0x13
#define RK808_RTC_COMPENSATION_HIGH   0x14
#define RK808_RTC_RESET_STATUS        0x16
#define RK808_DEVICE_CONTROL          0x4B

//
// RTC status bits.
//

#define RK808_RTC_STATUS_RUNNING          0x02
#define RK808_RTC_STATUS_1_SECOND_EVENT   0x04
#define RK808_RTC_STATUS_1_MINUTE_EVENT   0x08
#define RK808_RTC_STATUS_1_HOUR_EVENT     0x10
#define RK808_RTC_STATUS_1_DAY_EVENT      0x20
#define RK808_RTC_STATUS_ALARM            0x40
#define RK808_RTC_STATUS_RESET            0x80

//
// RTC control bits.
//

#define RK808_RTC_CONTROL_STOP                0x01
#define RK808_RTC_CONTROL_GET_TIME            0x40
#define RK808_RTC_CONTROL_READ_SHADOWED       0x80

//
// RTC interrupt bits.
//

#define RK808_RTC_INTERRUPT_PERIODIC_MASK     0x03
#define RK808_RTC_INTERRUPT_EVERY_SECOND      0x00
#define RK808_RTC_INTERRUPT_EVERY_MINUTE      0x01
#define RK808_RTC_INTERRUPT_EVERY_HOUR        0x02
#define RK808_RTC_INTERRUPT_EVERY_DAY         0x03
#define RK808_RTC_INTERRUPT_PERIODIC          0x04
#define RK808_RTC_INTERRUPT_ALARM             0x08
#define RK808_RTC_INTERRUPT_MASK_DURING_SLEEP 0x10

//
// Device control register bits.
//

#define RK808_DEVICE_CONTROL_SHUTDOWN (1 << 3)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

EFI_STATUS
EfipRk808RtcStart (
    VOID
    );

EFI_STATUS
EfipRk808RtcStop (
    VOID
    );

EFI_STATUS
EfipRk808I2cWrite8 (
    UINT8 ChipNumber,
    UINT8 Register,
    UINT8 Value
    );

EFI_STATUS
EfipRk808I2cRead8 (
    UINT8 ChipNumber,
    UINT8 Register,
    UINT8 *Value
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipRk808InitializeRtc (
    VOID
    )

/*++

Routine Description:

    This routine enables the MMC power rails controlled by the TWL4030.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    //
    // Start the RTC running.
    //

    return EfipRk808RtcStart();
}

EFI_STATUS
EfipRk808ReadRtc (
    EFI_TIME *Time
    )

/*++

Routine Description:

    This routine reads the current time from the RK808.

Arguments:

    Time - Supplies a pointer where the time is returned on success.

Return Value:

    Status code.

--*/

{

    EFI_STATUS Status;
    UINT8 Value;

    //
    // Read and clear the power up status and alarm bits.
    //

    Status = EfipRk808I2cRead8(RK808_CHIP, RK808_RTC_STATUS, &Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfipRk808I2cWrite8(RK808_CHIP, RK808_RTC_STATUS, Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Write a zero and then a one to snap the current time into the shadow
    // registers.
    //

    Status = EfipRk808I2cRead8(RK808_CHIP, RK808_RTC_CONTROL, &Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Value &= ~RK808_RTC_CONTROL_GET_TIME;
    Value |= RK808_RTC_CONTROL_READ_SHADOWED;
    Status = EfipRk808I2cWrite8(RK808_CHIP, RK808_RTC_CONTROL, Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Value |= RK808_RTC_CONTROL_GET_TIME;
    Status = EfipRk808I2cWrite8(RK808_CHIP, RK808_RTC_CONTROL, Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfipRk808I2cRead8(RK808_CHIP, RK808_RTC_SECONDS, &Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Time->Second = EFI_BCD_TO_BINARY(Value);
    Status = EfipRk808I2cRead8(RK808_CHIP, RK808_RTC_MINUTES, &Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Time->Minute = EFI_BCD_TO_BINARY(Value);
    Status = EfipRk808I2cRead8(RK808_CHIP, RK808_RTC_HOURS, &Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Time->Hour = EFI_BCD_TO_BINARY(Value);
    Status = EfipRk808I2cRead8(RK808_CHIP, RK808_RTC_DAYS, &Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Time->Day = EFI_BCD_TO_BINARY(Value);
    Status = EfipRk808I2cRead8(RK808_CHIP, RK808_RTC_MONTHS, &Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Time->Month = EFI_BCD_TO_BINARY(Value);
    Status = EfipRk808I2cRead8(RK808_CHIP, RK808_RTC_YEARS, &Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Time->Year = EFI_BCD_TO_BINARY(Value) + 2000;
    Time->Nanosecond = 0;
    Time->TimeZone = EFI_UNSPECIFIED_TIMEZONE;
    Time->Daylight = 0;
    return EFI_SUCCESS;
}

EFI_STATUS
EfipRk808ReadRtcWakeupTime (
    BOOLEAN *Enabled,
    BOOLEAN *Pending,
    EFI_TIME *Time
    )

/*++

Routine Description:

    This routine reads the wake alarm time from the RK808.

Arguments:

    Enabled - Supplies a pointer where a boolean will be returned indicating if
        the wake time interrupt is enabled.

    Pending - Supplies a pointer where a boolean will be returned indicating if
        the wake alarm interrupt is pending and requires service.

    Time - Supplies a pointer where the time is returned on success.

Return Value:

    Status code.

--*/

{

    EFI_STATUS Status;
    UINT8 Value;

    *Enabled = FALSE;
    *Pending = FALSE;
    Status = EfipRk808I2cRead8(RK808_CHIP, RK808_RTC_INTERRUPTS, &Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    if ((Value & RK808_RTC_INTERRUPT_ALARM) != 0) {
        *Enabled = TRUE;
    }

    Status = EfipRk808I2cRead8(RK808_CHIP, RK808_RTC_STATUS, &Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    if ((Value & RK808_RTC_STATUS_ALARM) != 0) {
        *Pending = TRUE;
    }

    Status = EfipRk808I2cRead8(RK808_CHIP, RK808_RTC_ALARM_SECONDS, &Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Time->Second = EFI_BCD_TO_BINARY(Value);
    Status = EfipRk808I2cRead8(RK808_CHIP, RK808_RTC_ALARM_MINUTES, &Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Time->Minute = EFI_BCD_TO_BINARY(Value);
    Status = EfipRk808I2cRead8(RK808_CHIP, RK808_RTC_ALARM_HOURS, &Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Time->Hour = EFI_BCD_TO_BINARY(Value);
    Status = EfipRk808I2cRead8(RK808_CHIP, RK808_RTC_ALARM_DAYS, &Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Time->Day = EFI_BCD_TO_BINARY(Value);
    Status = EfipRk808I2cRead8(RK808_CHIP, RK808_RTC_ALARM_MONTHS, &Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Time->Month = EFI_BCD_TO_BINARY(Value);
    Status = EfipRk808I2cRead8(RK808_CHIP, RK808_RTC_ALARM_YEARS, &Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Time->Year = EFI_BCD_TO_BINARY(Value) + 2000;
    Time->Nanosecond = 0;
    Time->TimeZone = EFI_UNSPECIFIED_TIMEZONE;
    Time->Daylight = 0;
    return EFI_SUCCESS;
}

EFI_STATUS
EfipRk808WriteRtc (
    EFI_TIME *Time
    )

/*++

Routine Description:

    This routine writes the current time to the RK808.

Arguments:

    Time - Supplies a pointer to the new time to set.

Return Value:

    Status code.

--*/

{

    EFI_STATUS Status;
    UINT8 Value;

    //
    // Stop the clock while programming.
    //

    Status = EfipRk808RtcStop();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Value = EFI_BINARY_TO_BCD(Time->Second);
    Status = EfipRk808I2cWrite8(RK808_CHIP, RK808_RTC_SECONDS, Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Value = EFI_BINARY_TO_BCD(Time->Minute);
    Status = EfipRk808I2cWrite8(RK808_CHIP, RK808_RTC_MINUTES, Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Value = EFI_BINARY_TO_BCD(Time->Hour);
    Status = EfipRk808I2cWrite8(RK808_CHIP, RK808_RTC_HOURS, Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Value = EFI_BINARY_TO_BCD(Time->Day);
    Status = EfipRk808I2cWrite8(RK808_CHIP, RK808_RTC_DAYS, Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Value = EFI_BINARY_TO_BCD(Time->Month);
    Status = EfipRk808I2cWrite8(RK808_CHIP, RK808_RTC_MONTHS, Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    if (Time->Year < 2000) {
        Value = EFI_BINARY_TO_BCD(Time->Year - 1900);

    } else {
        Value = EFI_BINARY_TO_BCD(Time->Year - 2000);
    }

    Status = EfipRk808I2cWrite8(RK808_CHIP, RK808_RTC_YEARS, Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Fire the clock back up.
    //

    Status = EfipRk808RtcStart();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    return Status;
}

EFI_STATUS
EfipRk808WriteRtcWakeupTime (
    BOOLEAN Enable,
    EFI_TIME *Time
    )

/*++

Routine Description:

    This routine writes the alarm time to the RK808.

Arguments:

    Enable - Supplies a boolean enabling or disabling the wakeup timer.

    Time - Supplies an optional pointer to the time to set. This parameter is
        only optional if the enable parameter is FALSE.

Return Value:

    Status code.

--*/

{

    UINT8 Interrupts;
    EFI_STATUS Status;
    UINT8 Value;

    //
    // Clear the interrupt first.
    //

    Status = EfipRk808I2cRead8(RK808_CHIP, RK808_RTC_INTERRUPTS, &Interrupts);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Interrupts &= ~RK808_RTC_INTERRUPT_ALARM;
    Status = EfipRk808I2cWrite8(RK808_CHIP, RK808_RTC_INTERRUPTS, Interrupts);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    if (Enable == FALSE) {
        return Status;
    }

    if (Time == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    //
    // Program the new time.
    //

    Value = EFI_BINARY_TO_BCD(Time->Second);
    Status = EfipRk808I2cWrite8(RK808_CHIP, RK808_RTC_ALARM_SECONDS, Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Value = EFI_BINARY_TO_BCD(Time->Minute);
    Status = EfipRk808I2cWrite8(RK808_CHIP, RK808_RTC_ALARM_MINUTES, Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Value = EFI_BINARY_TO_BCD(Time->Hour);
    Status = EfipRk808I2cWrite8(RK808_CHIP, RK808_RTC_ALARM_HOURS, Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Value = EFI_BINARY_TO_BCD(Time->Day);
    Status = EfipRk808I2cWrite8(RK808_CHIP, RK808_RTC_ALARM_DAYS, Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Value = EFI_BINARY_TO_BCD(Time->Month);
    Status = EfipRk808I2cWrite8(RK808_CHIP, RK808_RTC_ALARM_MONTHS, Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    if (Time->Year < 2000) {
        Value = EFI_BINARY_TO_BCD(Time->Year - 1900);

    } else {
        Value = EFI_BINARY_TO_BCD(Time->Year - 2000);
    }

    Status = EfipRk808I2cWrite8(RK808_CHIP, RK808_RTC_ALARM_YEARS, Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Enable the interrupt.
    //

    Interrupts |= RK808_RTC_INTERRUPT_ALARM;
    Status = EfipRk808I2cWrite8(RK808_CHIP, RK808_RTC_INTERRUPTS, Interrupts);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    return Status;
}

EFI_STATUS
EfipRk808Shutdown (
    VOID
    )

/*++

Routine Description:

    This routine performs a system shutdown using the RK808.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    EFI_STATUS Status;
    UINT8 Value;

    Status = EfipRk32I2cInitialize();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfipRk808I2cRead8(RK808_CHIP, RK808_DEVICE_CONTROL, &Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Value |= RK808_DEVICE_CONTROL_SHUTDOWN;
    return EfipRk808I2cWrite8(RK808_CHIP, RK808_DEVICE_CONTROL, Value);
}

//
// --------------------------------------------------------- Internal Functions
//

EFI_STATUS
EfipRk808RtcStart (
    VOID
    )

/*++

Routine Description:

    This routine starts the RK808 RTC.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    EFI_STATUS Status;
    UINT8 Value;

    Status = EfipRk808I2cRead8(RK808_CHIP, RK808_RTC_CONTROL, &Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Value &= ~RK808_RTC_CONTROL_STOP;
    Status = EfipRk808I2cWrite8(RK808_CHIP, RK808_RTC_CONTROL, Value);
    return Status;
}

EFI_STATUS
EfipRk808RtcStop (
    VOID
    )

/*++

Routine Description:

    This routine stops the RK808 RTC.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    EFI_STATUS Status;
    UINT8 Value;

    Status = EfipRk808I2cRead8(RK808_CHIP, RK808_RTC_CONTROL, &Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Value |= RK808_RTC_CONTROL_STOP;
    Status = EfipRk808I2cWrite8(RK808_CHIP, RK808_RTC_CONTROL, Value);
    return Status;
}

EFI_STATUS
EfipRk808I2cWrite8 (
    UINT8 ChipNumber,
    UINT8 Register,
    UINT8 Value
    )

/*++

Routine Description:

    This routine writes a register on the RK808.

Arguments:

    ChipNumber - Supplies the device address of the RK808 on the I2C bus.

    Register - Supplies the register number to write.

    Value - Supplies the register value.

Return Value:

    Status code.

--*/

{

    return EfipRk32I2cWrite(ChipNumber, Register, 1, &Value, 1);
}

EFI_STATUS
EfipRk808I2cRead8 (
    UINT8 ChipNumber,
    UINT8 Register,
    UINT8 *Value
    )

/*++

Routine Description:

    This routine reads a register on the RK808.

Arguments:

    ChipNumber - Supplies the device address of the RK808 on the I2C bus.

    Register - Supplies the register number to write.

    Value - Supplies a pointer where the read value will be returned.

Return Value:

    Status code.

--*/

{

    return EfipRk32I2cRead(ChipNumber, Register, 1, Value, 1);
}

