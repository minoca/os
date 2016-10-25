/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pmic.c

Abstract:

    This module implements support for the TWL4030 PMIC that usually
    accompanies the TI OMAP4.

Author:

    Evan Green 13-Mar-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "../twl6030.h"
#include "../pandafw.h"

//
// ---------------------------------------------------------------- Definitions
//

#define OMAP4_SYSCTRL_PADCONF_CORE_BASE 0x4A100000
#define OMAP4_SYSTEM_CONTROL_PBIASLITE 0x600

#define OMAP4_MMC1_VMODE (1 << 21)
#define OMAP4_MMC1_PBIASLITE_PWRDNZ (1 << 22)
#define OMAP4_MMC1_PWRDNZ (1 << 26)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

EFI_STATUS
Omap4Twl6030I2cWrite8 (
    UINT8 ChipNumber,
    UINT8 Register,
    UINT8 Value
    );

EFI_STATUS
Omap4Twl6030I2cRead8 (
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
Omap4Twl6030InitializeMmcPower (
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

    volatile UINT32 *PbiasLite;
    EFI_STATUS Status;
    VOID *SystemControlBase;
    UINT32 Value;

    SystemControlBase = (VOID *)OMAP4_SYSCTRL_PADCONF_CORE_BASE;
    PbiasLite = SystemControlBase + OMAP4_SYSTEM_CONTROL_PBIASLITE;
    Value = *PbiasLite;
    Value &= ~(OMAP4_MMC1_PBIASLITE_PWRDNZ | OMAP4_MMC1_PWRDNZ);
    *PbiasLite = Value;

    //
    // Set VMMC1 to 3.00 Volts.
    //

    Status = Omap4Twl6030I2cWrite8(TWL6030_CHIP_PM,
                                   VMMC_CFG_VOLTAGE,
                                   VMMC_VOLTAGE_3V0);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = Omap4Twl6030I2cWrite8(TWL6030_CHIP_PM, VMMC_CFG_STATE, 0x21);
    if (!EFI_ERROR(Status)) {
        Value = *PbiasLite;
        Value |= OMAP4_MMC1_PBIASLITE_PWRDNZ | OMAP4_MMC1_PWRDNZ |
                 OMAP4_MMC1_VMODE;

        *PbiasLite = Value;
    }

    return Status;
}

EFI_STATUS
Omap4Twl6030InitializeRtc (
    VOID
    )

/*++

Routine Description:

    This routine enables the RTC controlled by the TWL4030.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    EFI_STATUS Status;
    UINT8 Value;

    //
    // Write to the RTC control register to start the counter ticking if it
    // isn't already.
    //

    Value = TWL6030_RTC_CONTROL_RUN;
    Status = Omap4Twl6030I2cWrite8(TWL6030_CHIP_PM, TWL6030_RTC_CONTROL, Value);
    return Status;
}

EFI_STATUS
Omap4Twl6030ReadRtc (
    EFI_TIME *Time
    )

/*++

Routine Description:

    This routine reads the current time from the TWL6030.

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

    Status = Omap4Twl6030I2cRead8(TWL6030_CHIP_PM,
                                  TWL6030_RTC_STATUS,
                                  &Value);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = Omap4Twl6030I2cWrite8(TWL6030_CHIP_PM, TWL6030_RTC_STATUS, Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Write a zero and then a one to snap the current time into the shadow
    // registers.
    //

    Value = TWL6030_RTC_CONTROL_READ_SHADOWED | TWL6030_RTC_CONTROL_RUN;
    Status = Omap4Twl6030I2cWrite8(TWL6030_CHIP_PM,
                                   TWL6030_RTC_CONTROL,
                                   Value);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Value |= TWL6030_RTC_CONTROL_GET_TIME;
    Status = Omap4Twl6030I2cWrite8(TWL6030_CHIP_PM,
                                   TWL6030_RTC_CONTROL,
                                   Value);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = Omap4Twl6030I2cRead8(TWL6030_CHIP_PM, TWL6030_RTC_SECONDS, &Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Time->Second = EFI_BCD_TO_BINARY(Value);
    Status = Omap4Twl6030I2cRead8(TWL6030_CHIP_PM, TWL6030_RTC_MINUTES, &Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Time->Minute = EFI_BCD_TO_BINARY(Value);
    Status = Omap4Twl6030I2cRead8(TWL6030_CHIP_PM, TWL6030_RTC_HOURS, &Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Time->Hour = EFI_BCD_TO_BINARY(Value);
    Status = Omap4Twl6030I2cRead8(TWL6030_CHIP_PM, TWL6030_RTC_DAYS, &Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Time->Day = EFI_BCD_TO_BINARY(Value);
    Status = Omap4Twl6030I2cRead8(TWL6030_CHIP_PM, TWL6030_RTC_MONTHS, &Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Time->Month = EFI_BCD_TO_BINARY(Value);
    Status = Omap4Twl6030I2cRead8(TWL6030_CHIP_PM, TWL6030_RTC_YEARS, &Value);
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
Omap4Twl6030ReadRtcWakeupTime (
    BOOLEAN *Enabled,
    BOOLEAN *Pending,
    EFI_TIME *Time
    )

/*++

Routine Description:

    This routine reads the wake alarm time from the TWL6030.

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
    Status = Omap4Twl6030I2cRead8(TWL6030_CHIP_PM,
                                  TWL6030_RTC_INTERRUPTS,
                                  &Value);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    if ((Value & TWL6030_RTC_INTERRUPT_ALARM) != 0) {
        *Enabled = TRUE;
    }

    Status = Omap4Twl6030I2cRead8(TWL6030_CHIP_PM, TWL6030_RTC_STATUS, &Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    if ((Value & TWL6030_RTC_STATUS_ALARM) != 0) {
        *Pending = TRUE;
    }

    Status = Omap4Twl6030I2cRead8(TWL6030_CHIP_PM,
                                  TWL6030_RTC_ALARM_SECONDS,
                                  &Value);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Time->Second = EFI_BCD_TO_BINARY(Value);
    Status = Omap4Twl6030I2cRead8(TWL6030_CHIP_PM,
                                  TWL6030_RTC_ALARM_MINUTES,
                                  &Value);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Time->Minute = EFI_BCD_TO_BINARY(Value);
    Status = Omap4Twl6030I2cRead8(TWL6030_CHIP_PM,
                                  TWL6030_RTC_ALARM_HOURS,
                                  &Value);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Time->Hour = EFI_BCD_TO_BINARY(Value);
    Status = Omap4Twl6030I2cRead8(TWL6030_CHIP_PM,
                                  TWL6030_RTC_ALARM_DAYS,
                                  &Value);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Time->Day = EFI_BCD_TO_BINARY(Value);
    Status = Omap4Twl6030I2cRead8(TWL6030_CHIP_PM,
                                  TWL6030_RTC_ALARM_MONTHS,
                                  &Value);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Time->Month = EFI_BCD_TO_BINARY(Value);
    Status = Omap4Twl6030I2cRead8(TWL6030_CHIP_PM,
                                  TWL6030_RTC_ALARM_YEARS,
                                  &Value);

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
Omap4Twl6030WriteRtc (
    EFI_TIME *Time
    )

/*++

Routine Description:

    This routine writes the current time to the TWL6030.

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

    Status = Omap4Twl6030I2cWrite8(TWL6030_CHIP_PM, TWL6030_RTC_CONTROL, 0);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Value = EFI_BINARY_TO_BCD(Time->Second);
    Status = Omap4Twl6030I2cWrite8(TWL6030_CHIP_PM, TWL6030_RTC_SECONDS, Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Value = EFI_BINARY_TO_BCD(Time->Minute);
    Status = Omap4Twl6030I2cWrite8(TWL6030_CHIP_PM, TWL6030_RTC_MINUTES, Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Value = EFI_BINARY_TO_BCD(Time->Hour);
    Status = Omap4Twl6030I2cWrite8(TWL6030_CHIP_PM, TWL6030_RTC_HOURS, Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Value = EFI_BINARY_TO_BCD(Time->Day);
    Status = Omap4Twl6030I2cWrite8(TWL6030_CHIP_PM, TWL6030_RTC_DAYS, Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Value = EFI_BINARY_TO_BCD(Time->Month);
    Status = Omap4Twl6030I2cWrite8(TWL6030_CHIP_PM, TWL6030_RTC_MONTHS, Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    if (Time->Year < 2000) {
        Value = EFI_BINARY_TO_BCD(Time->Year - 1900);

    } else {
        Value = EFI_BINARY_TO_BCD(Time->Year - 2000);
    }

    Status = Omap4Twl6030I2cWrite8(TWL6030_CHIP_PM, TWL6030_RTC_YEARS, Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Fire the clock back up.
    //

    Value = TWL6030_RTC_CONTROL_RUN;
    Status = Omap4Twl6030I2cWrite8(TWL6030_CHIP_PM, TWL6030_RTC_CONTROL, Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    return Status;
}

EFI_STATUS
Omap4Twl6030WriteRtcWakeupTime (
    BOOLEAN Enable,
    EFI_TIME *Time
    )

/*++

Routine Description:

    This routine writes the alarm time to the TWL6030.

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

    Status = Omap4Twl6030I2cRead8(TWL6030_CHIP_PM,
                                  TWL6030_RTC_INTERRUPTS,
                                  &Interrupts);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Interrupts &= ~TWL6030_RTC_INTERRUPT_ALARM;
    Status = Omap4Twl6030I2cWrite8(TWL6030_CHIP_PM,
                                   TWL6030_RTC_INTERRUPTS,
                                   Interrupts);

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
    Status = Omap4Twl6030I2cWrite8(TWL6030_CHIP_PM,
                                   TWL6030_RTC_ALARM_SECONDS,
                                   Value);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Value = EFI_BINARY_TO_BCD(Time->Minute);
    Status = Omap4Twl6030I2cWrite8(TWL6030_CHIP_PM,
                                   TWL6030_RTC_ALARM_MINUTES,
                                   Value);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Value = EFI_BINARY_TO_BCD(Time->Hour);
    Status = Omap4Twl6030I2cWrite8(TWL6030_CHIP_PM,
                                   TWL6030_RTC_ALARM_HOURS,
                                   Value);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Value = EFI_BINARY_TO_BCD(Time->Day);
    Status = Omap4Twl6030I2cWrite8(TWL6030_CHIP_PM,
                                   TWL6030_RTC_ALARM_DAYS,
                                   Value);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Value = EFI_BINARY_TO_BCD(Time->Month);
    Status = Omap4Twl6030I2cWrite8(TWL6030_CHIP_PM,
                                   TWL6030_RTC_ALARM_MONTHS,
                                   Value);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    if (Time->Year < 2000) {
        Value = EFI_BINARY_TO_BCD(Time->Year - 1900);

    } else {
        Value = EFI_BINARY_TO_BCD(Time->Year - 2000);
    }

    Status = Omap4Twl6030I2cWrite8(TWL6030_CHIP_PM,
                                   TWL6030_RTC_ALARM_YEARS,
                                   Value);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Enable the interrupt.
    //

    Interrupts |= TWL6030_RTC_INTERRUPT_ALARM;
    Status = Omap4Twl6030I2cWrite8(TWL6030_CHIP_PM,
                                   TWL6030_RTC_INTERRUPTS,
                                   Interrupts);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

EFI_STATUS
Omap4Twl6030I2cWrite8 (
    UINT8 ChipNumber,
    UINT8 Register,
    UINT8 Value
    )

/*++

Routine Description:

    This routine writes a register on the TWL6030.

Arguments:

    ChipNumber - Supplies the device address of the TWL4030 on the I2C bus.

    Register - Supplies the register number to write.

    Value - Supplies the register value.

Return Value:

    Status code.

--*/

{

    return EfipOmapI2cWrite(ChipNumber, Register, 1, &Value, 1);
}

EFI_STATUS
Omap4Twl6030I2cRead8 (
    UINT8 ChipNumber,
    UINT8 Register,
    UINT8 *Value
    )

/*++

Routine Description:

    This routine reads a register on the TWL6030.

Arguments:

    ChipNumber - Supplies the device address of the TWL4030 on the I2C bus.

    Register - Supplies the register number to write.

    Value - Supplies a pointer where the read value will be returned.

Return Value:

    Status code.

--*/

{

    return EfipOmapI2cRead(ChipNumber, Register, 1, Value, 1);
}

