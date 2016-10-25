/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    power.c

Abstract:

    This module implements power support for the TI AM335x first stage loader.

Author:

    Evan Green 18-Dec-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "init.h"
#include "util.h"

//
// --------------------------------------------------------------------- Macros
//

#define AM335_I2C_READ(_Register) AM3_READ32(AM335_I2C_0_BASE + _Register)
#define AM335_I2C_WRITE(_Register, _Value) \
    AM3_WRITE32(AM335_I2C_0_BASE + _Register, _Value)

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
EfipTps65217VoltageUpdate (
    UINT8 Register,
    UINT8 Selection
    );

VOID
EfipTps65217Read (
    UINT8 Register,
    UINT8 *Data
    );

VOID
EfipTps65217Write (
    UINT8 Protection,
    UINT8 Register,
    UINT8 Value,
    UINT8 Mask
    );

VOID
EfipAm335I2cInitialize (
    VOID
    );

VOID
EfipAm335I2cConfigureBusClock (
    VOID
    );

VOID
EfipAm335I2cRead (
    UINT8 Register,
    UINT32 Size,
    UINT8 *Data
    );

VOID
EfipAm335I2cWrite (
    UINT8 Register,
    UINT32 Size,
    UINT8 *Data
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the operating conditions table.
//

AM335_OPP_TABLE_ENTRY EfiAm335OppTable[] = {
    {AM335_MPU_PLL_M_275MHZ, AM335_PMIC_VOLTAGE_1100MV},
    {AM335_MPU_PLL_M_500MHZ, AM335_PMIC_VOLTAGE_1100MV},
    {AM335_MPU_PLL_M_600MHZ, AM335_PMIC_VOLTAGE_1200MV},
    {AM335_MPU_PLL_M_720MHZ, AM335_PMIC_VOLTAGE_1260MV},
    {AM335_MPU_PLL_M_300MHZ, AM335_PMIC_VOLTAGE_950MV},
    {AM335_MPU_PLL_M_300MHZ, AM335_PMIC_VOLTAGE_1100MV},
    {AM335_MPU_PLL_M_600MHZ, AM335_PMIC_VOLTAGE_1100MV},
    {AM335_MPU_PLL_M_720MHZ, AM335_PMIC_VOLTAGE_1200MV},
    {AM335_MPU_PLL_M_800MHZ, AM335_PMIC_VOLTAGE_1260MV},
    {AM335_MPU_PLL_M_1000MHZ, AM335_PMIC_VOLTAGE_1325MV},
};

//
// ------------------------------------------------------------------ Functions
//

VOID
EfipAm335ConfigureVddOpVoltage (
    VOID
    )

/*++

Routine Description:

    This routine configures the Vdd op voltage for the AM335x, assuming a
    TPS65217 PMIC hanging off of I2C bus 0.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT8 Status;

    EfipAm335I2cInitialize();

    //
    // Set the address of the PMIC to talk to.
    //

    AM335_I2C_WRITE(Am3I2cSlaveAddress, AM335_TPS65217_I2C_ADDRESS);
    EfipTps65217Read(TPS65217_STATUS, &Status);

    //
    // Increase the USB current limit to 1300mA.
    //

    EfipTps65217Write(TPS65217_PROTECTION_NONE,
                      TPS65217_POWER_PATH,
                      TPS65217_POWER_PATH_USB_INPUT_CURRENT_LIMIT_1300MA,
                      TPS65217_POWER_PATH_USB_INPUT_CURRENT_LIMIT_MASK);

    //
    // Set the DCDC2 voltage (MPU) to 1.275V.
    //

    EfipTps65217VoltageUpdate(TPS65217_DEFDCDC2, TPS65217_DCDC_VOLTAGE_1275MV);

    //
    // Set LDO3 and LDO4 output voltage to 3.3V.
    //

    EfipTps65217Write(TPS65217_PROTECTION_LEVEL_2,
                      TPS65217_DEFLS1,
                      TPS65217_LDO_VOLTAGE_OUT_1_8,
                      TPS65217_LDO_MASK);

    EfipTps65217Write(TPS65217_PROTECTION_LEVEL_2,
                      TPS65217_DEFLS2,
                      TPS65217_LDO_VOLTAGE_OUT_3_3,
                      TPS65217_LDO_MASK);

    return;
}

VOID
EfipAm335SetVdd1Voltage (
    UINT32 PmicVoltage
    )

/*++

Routine Description:

    This routine configures the Vdd1 voltage for the given operating condition.

Arguments:

    PmicVoltage - Supplies the selected PMIC voltage.

Return Value:

    None.

--*/

{

    EfipTps65217VoltageUpdate(TPS65217_DEFDCDC2, PmicVoltage);
    return;
}

UINT32
EfipAm335GetMaxOpp (
    VOID
    )

/*++

Routine Description:

    This routine determines the maximum operating conditions for this SoC.

Arguments:

    None.

Return Value:

    Returns the index into the opp table that this SoC can support. See
    AM335_EFUSE_OPP* definitions.

--*/

{

    UINT32 OppIndex;
    UINT32 OppSupport;

    if (EfiAm335DeviceVersion == AM335_SOC_DEVICE_VERSION_1_0) {
        OppIndex = AM335_EFUSE_OPPTB_720;

    } else if (EfiAm335DeviceVersion == AM335_SOC_DEVICE_VERSION_2_0) {
        OppIndex = AM335_EFUSE_OPPTB_800;

    } else if (EfiAm335DeviceVersion == AM335_SOC_DEVICE_VERSION_2_1) {
        OppSupport = AM3_READ32(
                    AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_EFUSE_SMA);

        OppSupport &= AM335_SOC_CONTROL_EFUSE_OPP_MASK;
        if ((OppSupport & AM335_EFUSE_OPPNT_1000_MASK) == 0) {
            OppIndex = AM335_EFUSE_OPPNT_1000;

        } else if ((OppSupport & AM335_EFUSE_OPPTB_800_MASK) == 0) {
            OppIndex = AM335_EFUSE_OPPTB_800;

        } else if ((OppSupport & AM335_EFUSE_OPP120_720_MASK) == 0) {
            OppIndex = AM335_EFUSE_OPP120_720;

        } else if ((OppSupport & AM335_EFUSE_OPP100_600_MASK) == 0) {
            OppIndex = AM335_EFUSE_OPP100_600;

        } else if ((OppSupport & AM335_EFUSE_OPP100_300_MASK) == 0) {
            OppIndex = AM335_EFUSE_OPP100_300;

        } else {
            OppIndex = AM335_EFUSE_OPP50_300;
        }

    } else {
        OppIndex = AM335_OPP_NONE;
    }

    return OppIndex;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
EfipTps65217VoltageUpdate (
    UINT8 Register,
    UINT8 Selection
    )

/*++

Routine Description:

    This routine sets up and enacts a voltage change on the TPS65217 PMIC.

Arguments:

    Register - Supplies the register to change.

    Selection - Supplies the new voltage selection.

Return Value:

    None.

--*/

{

    //
    // Set the new voltage level.
    //

    EfipTps65217Write(TPS65217_PROTECTION_LEVEL_2,
                      Register,
                      Selection,
                      0xFF);

    //
    // Set the go bit to initiate the transition.
    //

    EfipTps65217Write(TPS65217_PROTECTION_LEVEL_2,
                      TPS65217_DEFSLEW,
                      TPS65217_DCDC_GO,
                      TPS65217_DCDC_GO);

    return;
}

VOID
EfipTps65217Read (
    UINT8 Register,
    UINT8 *Data
    )

/*++

Routine Description:

    This routine reads a register from the TPS65217 PMIC.

Arguments:

    Register - Supplies the register to read.

    Data - Supplies a pointer where the data will be returned on success.

Return Value:

    None.

--*/

{

    EfipAm335I2cRead(Register, 1, Data);
    return;
}

VOID
EfipTps65217Write (
    UINT8 Protection,
    UINT8 Register,
    UINT8 Value,
    UINT8 Mask
    )

/*++

Routine Description:

    This routine writes a to the TPS65217 PMIC registers.

Arguments:

    Protection - Supplies the register password protection. See
        TPS65217_PROTECTION_* definitions.

    Register - Supplies the register to write.

    Value - Supplies the value to write.

    Mask - Supplies the mask of bits to actually change.

Return Value:

    None.

--*/

{

    UINT8 EncodedRegister;
    UINT8 ReadValue;

    //
    // Read the register and mask in the proper bits if needed.
    //

    if (Mask != 0xFF) {
        EfipAm335I2cRead(Register, 1, &ReadValue);
        ReadValue &= ~Mask;
        Value = ReadValue | (Value & Mask);
    }

    //
    // If there is protection on the register, write the password.
    //

    if (Protection != TPS65217_PROTECTION_NONE) {
        EncodedRegister = Register ^ TPS65217_PASSWORD_UNLOCK;
        EfipAm335I2cWrite(TPS65217_PASSWORD, 1, &EncodedRegister);
    }

    EfipAm335I2cWrite(Register, 1, &Value);
    if (Protection == TPS65217_PROTECTION_LEVEL_2) {
        EfipAm335I2cWrite(TPS65217_PASSWORD, 1, &EncodedRegister);
        EfipAm335I2cWrite(Register, 1, &Value);
    }

    return;
}

VOID
EfipAm335I2cInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes the I2c bus.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Value;

    //
    // Set the pin muxing on I2C 0.
    //

    Value = (1 << AM335_SOC_CONF_MUX_PUTYPESEL_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_SLEWCTRL_SHIFT);

    AM3_WRITE32(AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_I2C0_SDA,
                Value);

    Value = (1 << AM335_SOC_CONF_MUX_PUTYPESEL_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_SLEWCTRL_SHIFT);

    AM3_WRITE32(AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_I2C0_SCL,
                Value);

    //
    // Disable the I2C controller.
    //

    Value = AM335_I2C_READ(Am3I2cControl);
    Value &= ~AM335_I2C_CONTROL_ENABLE;
    AM335_I2C_WRITE(Am3I2cControl, Value);

    //
    // Reset the controller.
    //

    Value = AM335_I2C_READ(Am3I2cSysControl);
    Value |= AM335_I2C_SYSTEM_CONTROL_SOFT_RESET;
    AM335_I2C_WRITE(Am3I2cSysControl, Value);

    //
    // Disable auto idle.
    //

    Value = AM335_I2C_READ(Am3I2cSysControl);
    Value &= ~AM335_I2C_SYSTEM_CONTROL_AUTO_IDLE;
    AM335_I2C_WRITE(Am3I2cSysControl, Value);

    //
    // Configure the bus speed to be 100kHz.
    //

    EfipAm335I2cConfigureBusClock();

    //
    // Enable the I2C controller.
    //

    Value = AM335_I2C_READ(Am3I2cControl);
    Value |= AM335_I2C_CONTROL_ENABLE;
    AM335_I2C_WRITE(Am3I2cControl, Value);

    //
    // Wait for the system status to indicate the controller is ready.
    //

    do {
        Value = AM335_I2C_READ(Am3I2cSysStatus);

    } while ((Value & AM335_I2C_SYSTEM_STATUS_RESET_DONE) == 0);

    return;
}

VOID
EfipAm335I2cConfigureBusClock (
    VOID
    )

/*++

Routine Description:

    This routine initializes the bus clock of the I2C module.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Divider;
    UINT32 Prescaler;

    Prescaler = (AM335_I2C_SYSTEM_CLOCK_SPEED /
                 AM335_I2C_INTERNAL_CLOCK_SPEED) - 1;

    AM335_I2C_WRITE(Am3I2cPrescale, Prescaler);
    Divider = (AM335_I2C_INTERNAL_CLOCK_SPEED /
               AM335_I2C_OUTPUT_CLOCK_SPEED) / 2;

    AM335_I2C_WRITE(Am3I2cSclLowTime, Divider - 7);
    AM335_I2C_WRITE(Am3I2cSclHighTime, Divider - 5);
    return;
}

VOID
EfipAm335I2cRead (
    UINT8 Register,
    UINT32 Size,
    UINT8 *Data
    )

/*++

Routine Description:

    This routine performs a read from the I2C bus. This routine assumes the
    slave address has already been set.

Arguments:

    Register - Supplies the register to read from.

    Size - Supplies the number of data bytes to read.

    Data - Supplies a pointer where the data will be returned on success.

Return Value:

    None.

--*/

{

    UINT32 Value;

    //
    // Transmit the register number. Set the size to 1, clear all interrupts,
    // start the transfer, write the byte, clear the transmit ready interrupt,
    // and wait for the ready bit.
    //

    AM335_I2C_WRITE(Am3I2cCount, 1);
    AM335_I2C_WRITE(Am3I2cInterruptStatus, AM335_I2C_INTERRUPT_STATUS_MASK);
    Value = AM335_I2C_CONTROL_MASTER | AM335_I2C_CONTROL_TRANSMIT |
            AM335_I2C_CONTROL_ENABLE;

    AM335_I2C_WRITE(Am3I2cControl, Value);
    Value |= AM335_I2C_CONTROL_START;
    AM335_I2C_WRITE(Am3I2cControl, Value);
    do {
        Value = AM335_I2C_READ(Am3I2cInterruptStatusRaw);

    } while ((Value & AM335_I2C_INTERRUPT_BUS_BUSY) == 0);

    AM335_I2C_WRITE(Am3I2cData, Register);
    AM335_I2C_WRITE(Am3I2cInterruptStatus, AM335_I2C_INTERRUPT_TX_READY);
    do {
        Value = AM335_I2C_READ(Am3I2cInterruptStatusRaw);

    } while ((Value & AM335_I2C_INTERRUPT_ACCESS_READY) == 0);

    //
    // Now set the data count to the number of bytes, and set up the receive.
    //

    AM335_I2C_WRITE(Am3I2cCount, Size);
    AM335_I2C_WRITE(Am3I2cInterruptStatus, AM335_I2C_INTERRUPT_STATUS_MASK);
    Value = AM335_I2C_CONTROL_MASTER | AM335_I2C_CONTROL_ENABLE;
    AM335_I2C_WRITE(Am3I2cControl, Value);
    Value |= AM335_I2C_CONTROL_START;
    AM335_I2C_WRITE(Am3I2cControl, Value);
    do {
        Value = AM335_I2C_READ(Am3I2cInterruptStatusRaw);

    } while ((Value & AM335_I2C_INTERRUPT_BUS_BUSY) == 0);

    //
    // Loop reading the data bytes.
    //

    while (Size != 0) {
        do {
            Value = AM335_I2C_READ(Am3I2cBufferStatus);
            Value = (Value & AM335_I2C_BUFFER_STATUS_RX_MASK) >>
                    AM335_I2C_BUFFER_STATUS_RX_SHIFT;

        } while (Value == 0);

        *Data = AM335_I2C_READ(Am3I2cData);
        Data += 1;
        Size -= 1;
    }

    //
    // Make it stop.
    //

    Value = AM335_I2C_READ(Am3I2cControl);
    Value |= AM335_I2C_CONTROL_STOP;
    AM335_I2C_WRITE(Am3I2cControl, Value);
    do {
        Value = AM335_I2C_READ(Am3I2cInterruptStatusRaw);

    } while ((Value & AM335_I2C_INTERRUPT_BUS_FREE) == 0);

    AM335_I2C_WRITE(Am3I2cInterruptStatus, AM335_I2C_INTERRUPT_BUS_FREE);
    return;
}

VOID
EfipAm335I2cWrite (
    UINT8 Register,
    UINT32 Size,
    UINT8 *Data
    )

/*++

Routine Description:

    This routine performs a write to the I2C bus. This routine assumes the
    slave address has already been set.

Arguments:

    Register - Supplies the register to write to.

    Size - Supplies the number of data bytes to write (not including the
        register byte itself).

    Data - Supplies a pointer to the data to write.

Return Value:

    None.

--*/

{

    UINT32 Index;
    UINT32 Value;

    //
    // Transmit the register number. Set the size to the register plus the
    // data, clear all interrupts, start the transfer, write the byte, clear
    // the transmit ready interrupt, and wait for the ready bit.
    //

    AM335_I2C_WRITE(Am3I2cCount, Size + 1);
    AM335_I2C_WRITE(Am3I2cInterruptStatus, AM335_I2C_INTERRUPT_STATUS_MASK);
    Value = AM335_I2C_CONTROL_MASTER | AM335_I2C_CONTROL_TRANSMIT |
            AM335_I2C_CONTROL_ENABLE;

    AM335_I2C_WRITE(Am3I2cControl, Value);
    Value |= AM335_I2C_CONTROL_START;
    AM335_I2C_WRITE(Am3I2cControl, Value);
    do {
        Value = AM335_I2C_READ(Am3I2cInterruptStatusRaw);

    } while ((Value & AM335_I2C_INTERRUPT_BUS_BUSY) == 0);

    Index = 0;
    while (Index < Size + 1) {
        Value = AM335_I2C_READ(Am3I2cInterruptStatusRaw);
        if ((Value & AM335_I2C_INTERRUPT_TX_READY) == 0) {
            break;
        }

        //
        // The first time around write the register number, all the others
        // write the data.
        //

        if (Index == 0) {
            Value = Register;

        } else {
            Value = Data[Index - 1];
        }

        AM335_I2C_WRITE(Am3I2cData, Value);
        AM335_I2C_WRITE(Am3I2cInterruptStatus, AM335_I2C_INTERRUPT_TX_READY);
        Index += 1;
    }

    //
    // Make it stop.
    //

    Value = AM335_I2C_READ(Am3I2cControl);
    Value |= AM335_I2C_CONTROL_STOP;
    AM335_I2C_WRITE(Am3I2cControl, Value);
    do {
        Value = AM335_I2C_READ(Am3I2cInterruptStatusRaw);

    } while ((Value & AM335_I2C_INTERRUPT_BUS_FREE) == 0);

    AM335_I2C_WRITE(Am3I2cInterruptStatus, AM335_I2C_INTERRUPT_BUS_FREE);
    return;
}

