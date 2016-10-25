/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    i2c.c

Abstract:

    This module implements I2C bus support for OMAP3 and OMAP4 SoCs. This
    file should be removed when firmware enables this hardware.

Author:

    Evan Green 12-Mar-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "sdomap4.h"

//
// --------------------------------------------------------------------- Macros
//

#define OMAP_I2C_READ_REGISTER(_Register) \
    HlReadRegister32(OmapI2cBase + (_Register))

#define OMAP_I2C_WRITE_REGISTER(_Register, _Value) \
    HlWriteRegister32(OmapI2cBase + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

#define OMAP44XX_L4_PER_BASE 0x48000000
#define I2C_BASE (OMAP44XX_L4_PER_BASE + 0x70000)

#define I2C_SIZE 0x1000

#define I2C_BUSY_RETRY_COUNT 10000
#define I2C_STATUS_RETRY_COUNT 10000

//
// Define the I2C timeout in seconds.
//

#define I2C_TIMEOUT 1

//
// Control register bit definitions.
//

#define OMAP_I2C_CONTROL_ENABLE             (1 << 15)
#define OMAP_I2C_CONTROL_MASTER             (1 << 10)
#define OMAP_I2C_CONTROL_TRANSMIT           (1 << 9)
#define OMAP_I2C_CONTROL_STOP_CONDITION     (1 << 1)
#define OMAP_I2C_CONTROL_START_CONDITION    (1 << 0)

//
// Interrupt bit definitions.
//

#define OMAP_I2C_INTERRUPT_ARBITRATION_LOST (1 << 0)
#define OMAP_I2C_INTERRUPT_NACK             (1 << 1)
#define OMAP_I2C_INTERRUPT_ACCESS_READY     (1 << 2)
#define OMAP_I2C_INTERRUPT_RECEIVE_READY    (1 << 3)
#define OMAP_I2C_INTERRUPT_TRANSMIT_READY   (1 << 4)

#define OMAP_I2C_STATUS_BUSY (1 << 12)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _OMAP_I2C_REGISTER {
    OmapI2cRevisionLow            = 0x00,
    OmapI2cRevisionHigh           = 0x04,
    OmapI2cSystemControl          = 0x10,
    OmapI2cInterruptStatusRaw     = 0x24,
    OmapI2cInterruptStatus        = 0x28,
    OmapI2cInterruptEnableSet     = 0x2C,
    OmapI2cInterruptEnableClear   = 0x30,
    OmapI2cWakeupEnable           = 0x34,
    OmapI2cDmaReceiveEnableSet    = 0x38,
    OmapI2cDmaTransmitEnableSet   = 0x3C,
    OmapI2cDmaReceiveEnableClear  = 0x40,
    OmapI2cDmaTransmitEnableClear = 0x44,
    OmapI2cDmaReceiveWakeEnable   = 0x48,
    OmapI2cDmaTransmitWakeEnable  = 0x4C,
    OmapI2cInterruptEnableLegacy  = 0x84,
    OmapI2cInterruptStatusLegacy  = 0x88,
    OmapI2cSystemStatus           = 0x90,
    OmapI2cBufferConfiguration    = 0x94,
    OmapI2cCount                  = 0x98,
    OmapI2cData                   = 0x9C,
    OmapI2cControl                = 0xA4,
    OmapI2cOwnAddress             = 0xA8,
    OmapI2cSlaveAddress           = 0xAC,
    OmapI2cPrescaler              = 0xB0,
    OmapI2cClockLowTime           = 0xB4,
    OmapI2cClockHighTime          = 0xB8,
    OmapI2cSystemTest             = 0xBC,
    OmapI2cBufferStatus           = 0xC0,
    OmapI2cOwnAddress1            = 0xC4,
    OmapI2cOwnAddress2            = 0xC8,
    OmapI2cOwnAddress3            = 0xCC,
    OmapI2cActiveOwnAddress       = 0xD0,
    OmapI2cClockBlockingEnable    = 0xD4
} OMAP_I2C_REGISTER, *POMAP_I2C_REGISTER;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
OmapI2cWaitForBusyBit (
    VOID
    );

KSTATUS
OmapI2cWaitForEvent (
    ULONG Mask
    );

//
// -------------------------------------------------------------------- Globals
//

PVOID OmapI2cBase;

//
// ------------------------------------------------------------------ Functions
//

VOID
OmapI2cInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes the I2C device.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG Value;

    if (OmapI2cBase == NULL) {
        OmapI2cBase = MmMapPhysicalAddress(I2C_BASE,
                                           I2C_SIZE,
                                           TRUE,
                                           FALSE,
                                           TRUE);
    }

    ASSERT(OmapI2cBase != NULL);

    //
    // Set up the divisors.
    //

    OMAP_I2C_WRITE_REGISTER(OmapI2cPrescaler, 0);
    OMAP_I2C_WRITE_REGISTER(OmapI2cClockLowTime, 0x35);
    OMAP_I2C_WRITE_REGISTER(OmapI2cClockHighTime, 0x35);

    //
    // Take the i2c controller out of reset.
    //

    Value = OMAP_I2C_READ_REGISTER(OmapI2cControl);
    Value |= OMAP_I2C_CONTROL_ENABLE | OMAP_I2C_CONTROL_MASTER;
    OMAP_I2C_WRITE_REGISTER(OmapI2cControl, Value);

    //
    // Enable interrupts to be able to get their status.
    //

    Value = OMAP_I2C_INTERRUPT_NACK |
            OMAP_I2C_INTERRUPT_ACCESS_READY |
            OMAP_I2C_INTERRUPT_RECEIVE_READY |
            OMAP_I2C_INTERRUPT_TRANSMIT_READY;

    OMAP_I2C_WRITE_REGISTER(OmapI2cInterruptEnableLegacy, Value);
    HlBusySpin(1000);
    OmapI2cFlushData();
    OMAP_I2C_WRITE_REGISTER(OmapI2cInterruptStatusLegacy, 0xFFFFFFFF);
    OMAP_I2C_WRITE_REGISTER(OmapI2cCount, 0);
    return;
}

VOID
OmapI2cFlushData (
    VOID
    )

/*++

Routine Description:

    This routine flushes extraneous data out of the internal FIFOs.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG Status;

    while (TRUE) {
        Status = OMAP_I2C_READ_REGISTER(OmapI2cInterruptStatusLegacy);
        if ((Status & OMAP_I2C_INTERRUPT_RECEIVE_READY) != 0) {
            OMAP_I2C_READ_REGISTER(OmapI2cData);
            OMAP_I2C_WRITE_REGISTER(OmapI2cInterruptStatusLegacy,
                                    OMAP_I2C_INTERRUPT_RECEIVE_READY);

            HlBusySpin(1000);

        } else {
            break;
        }
    }

    return;
}

KSTATUS
OmapI2cWrite (
    UCHAR Chip,
    ULONG Address,
    ULONG AddressLength,
    PVOID Buffer,
    ULONG Length
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

    PUCHAR Bytes;
    ULONG DataIndex;
    KSTATUS Status;
    ULONG Value;

    ASSERT((Buffer != NULL) && (AddressLength <= 2) &&
           (Address + Length < 0x10000));

    Status = OmapI2cWaitForBusyBit();
    if (!KSUCCESS(Status)) {
        return Status;
    }

    OMAP_I2C_WRITE_REGISTER(OmapI2cCount, AddressLength + Length);
    OMAP_I2C_WRITE_REGISTER(OmapI2cSlaveAddress, Chip);
    Value = OMAP_I2C_CONTROL_ENABLE | OMAP_I2C_CONTROL_MASTER |
            OMAP_I2C_CONTROL_START_CONDITION |
            OMAP_I2C_CONTROL_STOP_CONDITION | OMAP_I2C_CONTROL_TRANSMIT;

    OMAP_I2C_WRITE_REGISTER(OmapI2cControl, Value);
    while (AddressLength != 0) {
        Status = OmapI2cWaitForEvent(OMAP_I2C_INTERRUPT_TRANSMIT_READY);
        if (!KSUCCESS(Status)) {
            goto I2cWriteEnd;
        }

        AddressLength -= 1;
        Value = (Address >> (AddressLength * BITS_PER_BYTE)) & 0xFF;
        OMAP_I2C_WRITE_REGISTER(OmapI2cData, Value);
        OMAP_I2C_WRITE_REGISTER(OmapI2cInterruptStatusLegacy,
                                OMAP_I2C_INTERRUPT_TRANSMIT_READY);
    }

    Bytes = Buffer;
    for (DataIndex = 0; DataIndex < Length; DataIndex += 1) {
        Status = OmapI2cWaitForEvent(OMAP_I2C_INTERRUPT_TRANSMIT_READY);
        if (!KSUCCESS(Status)) {
            goto I2cWriteEnd;
        }

        OMAP_I2C_WRITE_REGISTER(OmapI2cData, Bytes[DataIndex]);
        OMAP_I2C_WRITE_REGISTER(OmapI2cInterruptStatusLegacy,
                                OMAP_I2C_INTERRUPT_TRANSMIT_READY);
    }

    Status = STATUS_SUCCESS;

I2cWriteEnd:
    OMAP_I2C_WRITE_REGISTER(OmapI2cInterruptStatusLegacy, 0xFFFFFFFF);
    return Status;
}

KSTATUS
OmapI2cRead (
    UCHAR Chip,
    ULONG Address,
    ULONG AddressLength,
    PVOID Buffer,
    ULONG Length
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

    PUCHAR Bytes;
    ULONG DataIndex;
    ULONG InterruptStatus;
    ULONG Mask;
    KSTATUS Status;
    ULONG Value;

    ASSERT((Buffer != NULL) && (AddressLength <= 2) &&
           (Address + Length < 0x10000));

    Status = OmapI2cWaitForBusyBit();
    if (!KSUCCESS(Status)) {
        return Status;
    }

    OMAP_I2C_WRITE_REGISTER(OmapI2cCount, AddressLength);
    OMAP_I2C_WRITE_REGISTER(OmapI2cSlaveAddress, Chip);
    Value = OMAP_I2C_CONTROL_ENABLE | OMAP_I2C_CONTROL_MASTER |
            OMAP_I2C_CONTROL_START_CONDITION |
            OMAP_I2C_CONTROL_STOP_CONDITION | OMAP_I2C_CONTROL_TRANSMIT;

    OMAP_I2C_WRITE_REGISTER(OmapI2cControl, Value);
    Mask = OMAP_I2C_INTERRUPT_TRANSMIT_READY |
           OMAP_I2C_INTERRUPT_ACCESS_READY;

    while (TRUE) {
        Status = OmapI2cWaitForEvent(Mask);
        if (!KSUCCESS(Status)) {
            goto I2cReadEnd;
        }

        InterruptStatus = OMAP_I2C_READ_REGISTER(OmapI2cInterruptStatusLegacy);
        if (AddressLength != 0) {
            if ((InterruptStatus & OMAP_I2C_INTERRUPT_TRANSMIT_READY) != 0) {
                AddressLength -= 1;
                Value = (Address >> (AddressLength * BITS_PER_BYTE)) & 0xFF;
                OMAP_I2C_WRITE_REGISTER(OmapI2cData, Value);
                OMAP_I2C_WRITE_REGISTER(OmapI2cInterruptStatusLegacy,
                                        OMAP_I2C_INTERRUPT_TRANSMIT_READY);
            }
        }

        if ((InterruptStatus & OMAP_I2C_INTERRUPT_ACCESS_READY) != 0) {
            OMAP_I2C_WRITE_REGISTER(OmapI2cInterruptStatusLegacy,
                                    OMAP_I2C_INTERRUPT_ACCESS_READY);

            break;
        }
    }

    Status = OmapI2cWaitForBusyBit();
    if (!KSUCCESS(Status)) {
        goto I2cReadEnd;
    }

    OMAP_I2C_WRITE_REGISTER(OmapI2cSlaveAddress, Chip);
    OMAP_I2C_WRITE_REGISTER(OmapI2cCount, Length);
    Value = OMAP_I2C_CONTROL_ENABLE | OMAP_I2C_CONTROL_MASTER |
            OMAP_I2C_CONTROL_START_CONDITION |
            OMAP_I2C_CONTROL_STOP_CONDITION;

    OMAP_I2C_WRITE_REGISTER(OmapI2cControl, Value);
    Bytes = Buffer;
    Mask = OMAP_I2C_INTERRUPT_RECEIVE_READY |
           OMAP_I2C_INTERRUPT_ACCESS_READY;

    DataIndex = 0;
    while (DataIndex < Length) {
        Status = OmapI2cWaitForEvent(Mask);
        if (!KSUCCESS(Status)) {
            goto I2cReadEnd;
        }

        InterruptStatus = OMAP_I2C_READ_REGISTER(OmapI2cInterruptStatusLegacy);
        if ((InterruptStatus & OMAP_I2C_INTERRUPT_RECEIVE_READY) != 0) {
            Bytes[DataIndex] = OMAP_I2C_READ_REGISTER(OmapI2cData);
            DataIndex += 1;
            OMAP_I2C_WRITE_REGISTER(OmapI2cInterruptStatusLegacy,
                                    OMAP_I2C_INTERRUPT_RECEIVE_READY);
        }

        if ((InterruptStatus & OMAP_I2C_INTERRUPT_ACCESS_READY) != 0) {
            OMAP_I2C_WRITE_REGISTER(OmapI2cInterruptStatusLegacy,
                                    OMAP_I2C_INTERRUPT_ACCESS_READY);
        }
    }

    Status = STATUS_SUCCESS;

I2cReadEnd:
    OMAP_I2C_WRITE_REGISTER(OmapI2cInterruptStatusLegacy, 0xFFFFFFFF);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
OmapI2cWaitForBusyBit (
    VOID
    )

/*++

Routine Description:

    This routine waits for the busy bit to clear.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    ULONGLONG Timeout;

    Timeout = KeGetRecentTimeCounter() +
              (HlQueryTimeCounterFrequency() * I2C_TIMEOUT);

    do {
        if ((OMAP_I2C_READ_REGISTER(OmapI2cInterruptStatusLegacy) &
             OMAP_I2C_STATUS_BUSY) == 0) {

            return STATUS_SUCCESS;
        }

    } while (KeGetRecentTimeCounter() <= Timeout);

    return STATUS_TIMEOUT;
}

KSTATUS
OmapI2cWaitForEvent (
    ULONG Mask
    )

/*++

Routine Description:

    This routine waits for the busy bit to clear.

Arguments:

    Mask - Supplies the mask to wait to become non-zero.

Return Value:

    Status code.

--*/

{

    ULONG Status;
    ULONGLONG Timeout;

    Timeout = KeGetRecentTimeCounter() +
              (HlQueryTimeCounterFrequency() * I2C_TIMEOUT);

    do {
        Status = OMAP_I2C_READ_REGISTER(OmapI2cInterruptStatusLegacy);
        if ((Status & Mask) != 0) {
            return STATUS_SUCCESS;
        }

    } while (KeGetRecentTimeCounter() <= Timeout);

    return STATUS_TIMEOUT;
}

