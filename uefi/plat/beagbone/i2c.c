/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    i2c.c

Abstract:

    This module implements I2C support for the TI AM335x SoC in UEFI.

Author:

    Evan Green 5-Jan-2015

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
// --------------------------------------------------------------------- Macros
//

#define AM335_I2C_READ(_Register) \
    EfiReadRegister32((VOID *)(AM335_I2C_0_BASE + _Register))

#define AM335_I2C_WRITE(_Register, _Value) \
    EfiWriteRegister32((VOID *)(AM335_I2C_0_BASE + _Register), _Value)

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
EfipAm335I2cConfigureBusClock (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

VOID
EfipAm335I2c0Initialize (
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

    EfiWriteRegister32(
            (VOID *)(AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_I2C0_SDA),
            Value);

    Value = (1 << AM335_SOC_CONF_MUX_PUTYPESEL_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_SLEWCTRL_SHIFT);

    EfiWriteRegister32(
            (VOID *)(AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_I2C0_SCL),
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
EfipAm335I2c0SetSlaveAddress (
    UINT8 SlaveAddress
    )

/*++

Routine Description:

    This routine sets which address on the I2C bus to talk to.

Arguments:

    SlaveAddress - Supplies the slave address to communicate with.

Return Value:

    None.

--*/

{

    AM335_I2C_WRITE(Am3I2cSlaveAddress, SlaveAddress);
    return;
}

VOID
EfipAm335I2c0Read (
    UINT32 Register,
    UINT32 Size,
    UINT8 *Data
    )

/*++

Routine Description:

    This routine performs a read from the I2C bus. This routine assumes the
    slave address has already been set.

Arguments:

    Register - Supplies the register to read from. Supply -1 to skip
        transmitting a register number.

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

    if (Register <= MAX_UINT8) {
        AM335_I2C_WRITE(Am3I2cCount, 1);
        AM335_I2C_WRITE(Am3I2cInterruptStatus,
                        AM335_I2C_INTERRUPT_STATUS_MASK);

        Value = AM335_I2C_CONTROL_MASTER | AM335_I2C_CONTROL_TRANSMIT |
                AM335_I2C_CONTROL_ENABLE;

        AM335_I2C_WRITE(Am3I2cControl, Value);
        Value |= AM335_I2C_CONTROL_START;
        AM335_I2C_WRITE(Am3I2cControl, Value);
        do {
            Value = AM335_I2C_READ(Am3I2cInterruptStatusRaw);

        } while ((Value & AM335_I2C_INTERRUPT_BUS_BUSY) == 0);

        AM335_I2C_WRITE(Am3I2cData, Register);
        AM335_I2C_WRITE(Am3I2cInterruptStatus,
                        AM335_I2C_INTERRUPT_TX_READY);

        do {
            Value = AM335_I2C_READ(Am3I2cInterruptStatusRaw);

        } while ((Value & AM335_I2C_INTERRUPT_ACCESS_READY) == 0);
    }

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

    AM335_I2C_WRITE(Am3I2cInterruptStatus,
                    AM335_I2C_INTERRUPT_BUS_FREE);

    return;
}

VOID
EfipAm335I2c0Write (
    UINT32 Register,
    UINT32 Size,
    UINT8 *Data
    )

/*++

Routine Description:

    This routine performs a write to the I2C bus. This routine assumes the
    slave address has already been set.

Arguments:

    Register - Supplies the register to write to. Supply -1 to skip transmitting
        a register number.

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

    if (Register <= MAX_UINT8) {
        Size += 1;
    }

    AM335_I2C_WRITE(Am3I2cCount, Size);
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
    while (Index < Size) {
        Value = AM335_I2C_READ(Am3I2cInterruptStatusRaw);
        if ((Value & AM335_I2C_INTERRUPT_TX_READY) == 0) {
            break;
        }

        //
        // The first time around write the register number, all the others
        // write the data.
        //

        if (Register <= MAX_UINT8) {
            if (Index == 0) {
                Value = Register;

            } else {
                Value = Data[Index - 1];
            }

        //
        // No register, just transmit the data.
        //

        } else {
            Value = Data[Index];
        }

        AM335_I2C_WRITE(Am3I2cData, Value);
        AM335_I2C_WRITE(Am3I2cInterruptStatus,
                        AM335_I2C_INTERRUPT_TX_READY);

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

    AM335_I2C_WRITE(Am3I2cInterruptStatus,
                    AM335_I2C_INTERRUPT_BUS_FREE);

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

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

