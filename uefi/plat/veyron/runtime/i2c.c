/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    i2c.c

Abstract:

    This module implements I2C PMU bus support for Rk32xx SoCs.

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
// --------------------------------------------------------------------- Macros
//

#define RK32_I2C_READ_REGISTER(_Register) \
    EfiReadRegister32(EfiRk32I2cPmuBase + (_Register))

#define RK32_I2C_WRITE_REGISTER(_Register, _Value) \
    EfiWriteRegister32(EfiRk32I2cPmuBase + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the I2C timeout in milliseconds.
//

#define I2C_TIMEOUT 1000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

EFI_STATUS
EfipRk32I2cStart (
    UINT32 Mode
    );

EFI_STATUS
EfipRk32I2cStop (
    VOID
    );

EFI_STATUS
EfipRk32I2cWaitForEvent (
    UINT32 Mask
    );

//
// -------------------------------------------------------------------- Globals
//

VOID *EfiRk32I2cPmuBase = (VOID *)RK32_I2C_PMU_BASE;

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipRk32I2cInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes the I2C device.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    RK32_I2C_WRITE_REGISTER(Rk32I2cInterruptEnable, 0);
    return EFI_SUCCESS;
}

EFI_STATUS
EfipRk32I2cWrite (
    UINT8 Chip,
    UINT32 Address,
    UINT32 AddressLength,
    VOID *Buffer,
    UINT32 Length
    )

/*++

Routine Description:

    This routine writes the given buffer out to the given i2c device.

Arguments:

    Chip - Supplies the device to write to.

    Address - Supplies the address.

    AddressLength - Supplies the width of the address. Valid values are zero
        through two.

    Buffer - Supplies the buffer containing the data to write.

    Length - Supplies the length of the buffer in bytes.

Return Value:

    Status code.

--*/

{

    UINT32 ByteIndex;
    UINT8 *Bytes;
    UINT32 BytesRemaining;
    UINT32 BytesThisRound;
    UINT32 Data;
    UINT32 Mask;
    UINT32 RoundIndex;
    EFI_STATUS Status;
    EFI_STATUS StopStatus;
    UINT32 TransmitRegister;
    UINT32 Value;

    Status = EfipRk32I2cStart(RK32_I2C_CONTROL_MODE_TRANSMIT);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Set the chip as the first byte, shifted by 1 to account for the
    // read/write bit.
    //

    Data = 0;
    Data |= (Chip & 0x7F) << 1;
    ByteIndex = 1;

    //
    // Set the address bytes. There should be 0-2 bytes, meaning that it will
    // all fit in the first transmit word with the chip information.
    //

    while (AddressLength != 0) {
        AddressLength -= 1;
        Data |= ((Address >> (AddressLength * 8)) & 0xFF) << (ByteIndex * 8);
        ByteIndex += 1;
    }

    //
    // Send the data bytes.
    //

    TransmitRegister = Rk32I2cTransmitData0;
    RoundIndex = ByteIndex;
    Bytes = Buffer;
    BytesRemaining = Length + RoundIndex;
    while (BytesRemaining != 0) {
        if (BytesRemaining > 32) {
            BytesThisRound = 32;

        } else {
            BytesThisRound = BytesRemaining;
        }

        while (RoundIndex < BytesThisRound) {
            while (ByteIndex < 4) {
                if (RoundIndex == BytesThisRound) {
                    break;
                }

                Data |= *Bytes << (ByteIndex * 8);
                Bytes += 1;
                ByteIndex += 1;
                RoundIndex += 1;
            }

            RK32_I2C_WRITE_REGISTER(TransmitRegister, Data);
            Data = 0;
            TransmitRegister += 4;
            ByteIndex = 0;
        }

        RK32_I2C_WRITE_REGISTER(Rk32I2cInterruptPending,
                                RK32_I2C_INTERRUPT_MASK);

        Value = RK32_I2C_CONTROL_ENABLE |
                RK32_I2C_CONTROL_MODE_TRANSMIT |
                RK32_I2C_CONTROL_STOP_ON_NAK;

        RK32_I2C_WRITE_REGISTER(Rk32I2cControl, Value);
        RK32_I2C_WRITE_REGISTER(Rk32I2cMasterTransmitCount, BytesThisRound);
        Mask = RK32_I2C_INTERRUPT_MASTER_TRANSMIT_FINISHED;
        Status = EfipRk32I2cWaitForEvent(Mask);
        if (EFI_ERROR(Status)) {
            RK32_I2C_WRITE_REGISTER(Rk32I2cControl, 0);
            goto I2cWriteEnd;
        }

        BytesRemaining -= BytesThisRound;
        RoundIndex = 0;
        TransmitRegister = Rk32I2cTransmitData0;
    }

I2cWriteEnd:
    StopStatus = EfipRk32I2cStop();
    if (!EFI_ERROR(Status)) {
        Status = StopStatus;
    }

    return Status;
}

EFI_STATUS
EfipRk32I2cRead (
    UINT8 Chip,
    UINT32 Address,
    UINT32 AddressLength,
    VOID *Buffer,
    UINT32 Length
    )

/*++

Routine Description:

    This routine reads from the given i2c device into the given buffer.

Arguments:

    Chip - Supplies the device to read from.

    Address - Supplies the address.

    AddressLength - Supplies the width of the address. Valid values are zero
        through two.

    Buffer - Supplies a pointer to the buffer where the read data will be
        returned.

    Length - Supplies the length of the buffer in bytes.

Return Value:

    Status code.

--*/

{

    UINT32 ByteIndex;
    UINT8 *Bytes;
    UINT32 BytesRemaining;
    UINT32 BytesThisRound;
    UINT32 ControlValue;
    UINT32 Data;
    UINT32 Mask;
    UINT32 ReceiveRegister;
    EFI_STATUS Status;
    EFI_STATUS StopStatus;
    UINT32 Value;

    RK32_I2C_WRITE_REGISTER(Rk32I2cControl, 0);

    //
    // Set the chip in the master receive address register.
    //

    Value = RK32_I2C_MASTER_RECEIVE_SLAVE_ADDRESS_LOW_BYTE_VALID |
            RK32_I2C_MASTER_RECEIVE_SLAVE_ADDRESS_WRITE |
            ((Chip << RK32_I2C_MASTER_RECEIVE_SLAVE_ADDRESS_SHIFT) &
             RK32_I2C_MASTER_RECEIVE_SLAVE_ADDRESS_MASK);

    RK32_I2C_WRITE_REGISTER(Rk32I2cMasterReceiveSlaveAddress, Value);

    //
    // Set the address bytes. There should be 0-2 bytes, meaning that it will
    // all fit in the first transmit word with the chip information.
    //

    ByteIndex = 0;
    Value = 0;
    while (AddressLength != 0) {
        AddressLength -= 1;
        Value |= ((Address >> (AddressLength * 8)) & 0xFF) << (ByteIndex * 8);
        Value |= (RK32_I2C_MASTER_RECEIVE_SLAVE_REGISTER_LOW_BYTE_VALID <<
                  ByteIndex);

        ByteIndex += 1;
    }

    RK32_I2C_WRITE_REGISTER(Rk32I2cMasterReceiveSlaveRegister, Value);

    //
    // Begin the read.
    //

    Status = EfipRk32I2cStart(RK32_I2C_CONTROL_MODE_TRANSMIT_RECEIVE);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Receive the data bytes.
    //

    Bytes = Buffer;
    BytesRemaining = Length;
    ControlValue = RK32_I2C_CONTROL_ENABLE |
                   RK32_I2C_CONTROL_MODE_TRANSMIT_RECEIVE |
                   RK32_I2C_CONTROL_STOP_ON_NAK;

    while (BytesRemaining != 0) {
        if (BytesRemaining > 32) {
            BytesThisRound = 32;

        } else {
            BytesThisRound = BytesRemaining;
        }

        BytesRemaining -= BytesThisRound;
        if (BytesRemaining == 0) {
            ControlValue |= RK32_I2C_CONTROL_SEND_NAK;
        }

        RK32_I2C_WRITE_REGISTER(Rk32I2cInterruptPending,
                                RK32_I2C_INTERRUPT_MASK);

        RK32_I2C_WRITE_REGISTER(Rk32I2cControl, ControlValue);
        RK32_I2C_WRITE_REGISTER(Rk32I2cMasterReceiveCount, BytesThisRound);
        Mask = RK32_I2C_INTERRUPT_MASTER_RECEIVE_FINISHED;
        Status = EfipRk32I2cWaitForEvent(Mask);
        if (EFI_ERROR(Status)) {
            RK32_I2C_WRITE_REGISTER(Rk32I2cControl, 0);
            goto I2cReadEnd;
        }

        //
        // Read the data out of the receive registers.
        //

        ReceiveRegister = Rk32I2cReceiveData0;
        while (BytesThisRound != 0) {
            Data = RK32_I2C_READ_REGISTER(ReceiveRegister);
            ByteIndex = 0;
            while (ByteIndex < 4) {
                if (ByteIndex == BytesThisRound) {
                    break;
                }

                *Bytes = (Data >> (ByteIndex * 8)) & 0xFF;
                Bytes += 1;
                ByteIndex += 1;
            }

            BytesThisRound -= ByteIndex;
            ReceiveRegister += 4;
        }

        ControlValue = RK32_I2C_CONTROL_ENABLE |
                       RK32_I2C_CONTROL_MODE_RECEIVE |
                       RK32_I2C_CONTROL_STOP_ON_NAK;
    }

I2cReadEnd:
    StopStatus = EfipRk32I2cStop();
    if (!EFI_ERROR(Status)) {
        Status = StopStatus;
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

EFI_STATUS
EfipRk32I2cStart (
    UINT32 Mode
    )

/*++

Routine Description:

    This routine starts the I2C bus.

Arguments:

    Mode - Supplies the mode to set when starting the I2C operation.

Return Value:

    Status code.

--*/

{

    UINT32 Interrupts;
    UINT64 Time;
    UINT64 Timeout;
    UINT32 Value;

    //
    // Set the start bit and wait for the start interrupt to be set.
    //

    RK32_I2C_WRITE_REGISTER(Rk32I2cInterruptPending, RK32_I2C_INTERRUPT_MASK);
    Value = RK32_I2C_CONTROL_ENABLE | RK32_I2C_CONTROL_START | Mode;
    RK32_I2C_WRITE_REGISTER(Rk32I2cControl, Value);
    Time = 0;
    Timeout = I2C_TIMEOUT;
    do {
        Interrupts = RK32_I2C_READ_REGISTER(Rk32I2cInterruptPending);
        if ((Interrupts & RK32_I2C_INTERRUPT_START) != 0) {
            RK32_I2C_WRITE_REGISTER(Rk32I2cInterruptPending,
                                    RK32_I2C_INTERRUPT_START);

            Value = RK32_I2C_READ_REGISTER(Rk32I2cControl);
            Value &= ~RK32_I2C_CONTROL_START;
            RK32_I2C_WRITE_REGISTER(Rk32I2cControl, Value);
            return EFI_SUCCESS;
        }

        if (EfiBootServices != NULL) {
            EfiStall(50);
            Time += 50;
        }

    } while (Time <= Timeout);

    return EFI_TIMEOUT;
}

EFI_STATUS
EfipRk32I2cStop (
    VOID
    )

/*++

Routine Description:

    This routine stops the I2C bus.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    UINT32 Interrupts;
    UINT64 Time;
    UINT64 Timeout;
    UINT32 Value;

    //
    // Set the start bit and wait for the start interrupt to be set.
    //

    RK32_I2C_WRITE_REGISTER(Rk32I2cInterruptPending, RK32_I2C_INTERRUPT_MASK);
    Value = RK32_I2C_CONTROL_ENABLE | RK32_I2C_CONTROL_STOP;
    RK32_I2C_WRITE_REGISTER(Rk32I2cControl, Value);
    Time = 0;
    Timeout = I2C_TIMEOUT;
    do {
        Interrupts = RK32_I2C_READ_REGISTER(Rk32I2cInterruptPending);
        if ((Interrupts & RK32_I2C_INTERRUPT_STOP) != 0) {
            RK32_I2C_WRITE_REGISTER(Rk32I2cInterruptPending,
                                    RK32_I2C_INTERRUPT_STOP);

            RK32_I2C_WRITE_REGISTER(Rk32I2cControl, 0);
            return EFI_SUCCESS;
        }

        if (EfiBootServices != NULL) {
            EfiStall(50);
            Time += 50;
        }

    } while (Time <= Timeout);

    return EFI_TIMEOUT;
}

EFI_STATUS
EfipRk32I2cWaitForEvent (
    UINT32 Mask
    )

/*++

Routine Description:

    This routine waits for an interrupt in the given mask to fire.

Arguments:

    Mask - Supplies the mask to wait to become non-zero.

Return Value:

    Status code.

--*/

{

    UINT32 Interrupts;
    UINT64 Time;
    UINT64 Timeout;

    Time = 0;
    Timeout = I2C_TIMEOUT;
    do {
        Interrupts = RK32_I2C_READ_REGISTER(Rk32I2cInterruptPending);
        if ((Interrupts & RK32_I2C_INTERRUPT_NAK) != 0) {
            return EFI_NO_RESPONSE;
        }

        if ((Interrupts & Mask) != 0) {
            return EFI_SUCCESS;
        }

        if (EfiBootServices != NULL) {
            EfiStall(50);
            Time += 50;
        }

    } while (Time <= Timeout);

    return EFI_TIMEOUT;
}

