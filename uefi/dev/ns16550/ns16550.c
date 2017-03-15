/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ns16550.c

Abstract:

    This module implements the firmware serial port interface on a 16550
    standard UART.

Author:

    Chris Stevens 10-Jul-2015

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "uefifw.h"
#include "dev/ns16550.h"

//
// --------------------------------------------------------------------- Macros
//

//
// Macros to read from and write to 16550 registers.
//

#define NS16550_READ8(_Device, _Register) \
    ((PNS16550_READ8)(_Device)->Read8)((_Device), (_Register))

#define NS16550_WRITE8(_Device, _Register, _Value) \
    ((PNS16550_WRITE8)(_Device)->Write8)((_Device), (_Register), (_Value))

//
// This macro returns the offset of a given register from its base.
//

#define NS16550_REGISTER_OFFSET(_Device, _Register) \
    ((_Device)->RegisterOffset + ((_Register) << (_Device)->RegisterShift))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the bits for the PC UART Line Status register.
//

#define NS16550_LINE_STATUS_DATA_READY     0x01
#define NS16550_LINE_STATUS_TRANSMIT_EMPTY 0x20
#define NS16550_LINE_STATUS_ERRORS         0x8E

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _NS16550_REGISTER {
    Ns16550Data            = 0,
    Ns16550DivisorLow      = 0,
    Ns16550InterruptEnable = 1,
    Ns16550DivisorHigh     = 1,
    Ns16550InterruptStatus = 2,
    Ns16550FifoControl     = 2,
    Ns16550LineControl     = 3,
    Ns16550ModemControl    = 4,
    Ns16550LineStatus      = 5,
    Ns16550ModemStatus     = 6,
    Ns16550Scratch         = 7
} NS16550_REGISTER, *PNS16550_REGISTER;

typedef
UINT8
(*PNS16550_READ8) (
    PNS16550_CONTEXT Context,
    NS16550_REGISTER Register
    );

/*++

Routine Description:

    This routine reads a 16550 register.

Arguments:

    Context - Supplies a pointer to the device context.

    Register - Supplies the register to read.

Return Value:

    Returns the value at the register.

--*/

typedef
VOID
(*PNS16550_WRITE8) (
    PNS16550_CONTEXT Context,
    NS16550_REGISTER Register,
    UINT8 Value
    );

/*++

Routine Description:

    This routine writes to a 16550 register.

Arguments:

    Context - Supplies a pointer to the device context.

    Register - Supplies the register to write to.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

//
// ----------------------------------------------- Internal Function Prototypes
//

UINT8
EfipNs16550ReadIo8 (
    PNS16550_CONTEXT Device,
    NS16550_REGISTER Register
    );

VOID
EfipNs16550WriteIo8 (
    PNS16550_CONTEXT Device,
    NS16550_REGISTER Register,
    UINT8 Value
    );

UINT8
EfipNs16550ReadMemory8 (
    PNS16550_CONTEXT Device,
    NS16550_REGISTER Register
    );

VOID
EfipNs16550WriteMemory8 (
    PNS16550_CONTEXT Device,
    NS16550_REGISTER Register,
    UINT8 Value
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

//
// --------------------------------------------------------- Internal Functions
//

EFI_STATUS
EfipNs16550ComputeDivisor (
    UINT32 BaseBaud,
    UINT32 BaudRate,
    UINT16 *Divisor
    )

/*++

Routine Description:

    This routine computes the divisor rates for a NS 16550 UART at a given baud
    rate.

Arguments:

    BaseBaud - Supplies the baud rate for a divisor of 1.

    BaudRate - Supplies the desired baud rate.

    Divisor - Supplies a pointer where the divisor will be returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_UNSUPPORTED if the given baud rate cannot be achieved.

--*/

{

    UINT32 CurrentBaud;
    UINT32 LocalDivisor;

    //
    // Compute the baud rate divisor.
    //

    if (BaudRate > BaseBaud) {
        return EFI_UNSUPPORTED;
    }

    LocalDivisor = 1;
    while (TRUE) {
        CurrentBaud = BaseBaud / LocalDivisor;
        if ((CurrentBaud <= BaudRate) || (CurrentBaud == 0)) {
            break;
        }

        LocalDivisor += 1;
    }

    if ((CurrentBaud == 0) || (LocalDivisor > MAX_UINT16)) {
        return EFI_UNSUPPORTED;
    }

    *Divisor = (UINT16)LocalDivisor;
    return EFI_SUCCESS;
}

EFI_STATUS
EfipNs16550Initialize (
    PNS16550_CONTEXT Context
    )

/*++

Routine Description:

    This routine initializes the NS 16550 serial port hardware. The caller
    should have initialized at least some of the context structure.

Arguments:

    Context - Supplies the pointer to the port's initialized context.

Return Value:

    EFI Status code.

--*/

{

    UINT8 Value;

    //
    // Determine the correct register access function.
    //

    if (Context->MemoryBase != NULL) {
        Context->Read8 = EfipNs16550ReadMemory8;
        Context->Write8 = EfipNs16550WriteMemory8;

    } else {
        Context->Read8 = EfipNs16550ReadIo8;
        Context->Write8 = EfipNs16550WriteIo8;
    }

    //
    // Begin programming the 16550 controller. The topmost bit in the line
    // control register turns the DLAB (Data Latch Address Byte) on. This
    // changes the meanings of the registers, allowing us to program the baud
    // rate divisor values.
    //

    Value = NS16550_READ8(Context, Ns16550LineControl);
    Value |= 0x80;
    NS16550_WRITE8(Context, Ns16550LineControl, Value);

    //
    // Set the divisor bytes. This programs the baud rate generator.
    //

    NS16550_WRITE8(Context,
                   Ns16550DivisorLow,
                   (UINT8)(Context->BaudRateDivisor & 0x00FF));

    NS16550_WRITE8(Context,
                   Ns16550DivisorHigh,
                   (UINT8)((Context->BaudRateDivisor >> 8) & 0x00FF));

    //
    // Now program the FIFO queue configuration. It is assumed that the FIFOs
    // are operational, which is not true on certain machines with very old
    // UARTs. Setting bit 0 enables the FIFO. Setting bits 1 and 2 clears both
    // FIFOs. Clearing bit 3 disables DMA mode. The top 4 bits vary depending
    // on the version. Setting bit 5 enables the 64 byte FIFO, which is only
    // available on 16750s. Bit 4 is reserved. Otherwise bits 4 and 5 are
    // either reserved or dictate the transmit FIFO's empty trigger. Bits 6 and
    // 7 set the receive FIFO's trigger, where setting both bits means that
    // "2 less than full", which for the default 16 byte FIFO means 14 bytes
    // are in the buffer.
    //

    Value = 0xC7;
    if ((Context->Flags & NS16550_FLAG_TRANSMIT_TRIGGER_2_CHARACTERS) != 0) {
        Value |= 0x10;

    } else if ((Context->Flags & NS16550_FLAG_64_BYTE_FIFO) != 0) {
        Value |= 0x20;
    }

    NS16550_WRITE8(Context, Ns16550FifoControl, Value);

    //
    // Now program the Line Control register again. Setting bits 0 and 1 sets
    // 8 data bits. Clearing bit 2 sets one stop bit. Clearing bit 3 sets no
    // parity. Additionally, clearing bit 7 turns the DLAB latch off, changing
    // the meaning of the registers back and allowing other control registers to
    // be accessed.
    //

    NS16550_WRITE8(Context, Ns16550LineControl, 0x03);

    //
    // Setting the Modem Control register to zero disables all hardware flow
    // control.
    //

    NS16550_WRITE8(Context, Ns16550ModemControl, 0x00);
    return EFI_SUCCESS;
}

EFI_STATUS
EfipNs16550Transmit (
    PNS16550_CONTEXT Context,
    VOID *Data,
    UINTN Size
    )

/*++

Routine Description:

    This routine writes data out the serial port. This routine should busily
    spin if the previously sent byte has not finished transmitting.

Arguments:

    Context - Supplies the pointer to the port context.

    Data - Supplies a pointer to the data to write.

    Size - Supplies the size to write, in bytes.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if a device error occurred.

--*/

{

    UINT32 ByteIndex;
    UINT8 *Bytes;
    UINT8 StatusRegister;

    Bytes = Data;
    for (ByteIndex = 0; ByteIndex < Size; ByteIndex += 1) {

        //
        // Spin waiting for the buffer to become ready to send. If an error is
        // detected, bail out and report to the caller.
        //

        do {
            StatusRegister = NS16550_READ8(Context, Ns16550LineStatus);
            if ((StatusRegister & NS16550_LINE_STATUS_ERRORS) != 0) {
                return EFI_DEVICE_ERROR;
            }

        } while ((StatusRegister & NS16550_LINE_STATUS_TRANSMIT_EMPTY) == 0);

        //
        // Send the byte and return.
        //

        NS16550_WRITE8(Context, Ns16550Data, Bytes[ByteIndex]);
    }

    return EFI_SUCCESS;
}

EFI_STATUS
EfipNs16550Receive (
    PNS16550_CONTEXT Context,
    VOID *Data,
    UINTN *Size
    )

/*++

Routine Description:

    This routine reads bytes from the serial port.

Arguments:

    Context - Supplies the pointer to the port context.

    Data - Supplies a pointer where the read data will be returned on success.

    Size - Supplies a pointer that on input contains the size of the receive
        buffer. On output, returns the number of bytes read.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_READY if there was no data to be read at the current time.

    EFI_DEVICE_ERROR if a device error occurred.

--*/

{

    UINT32 ByteCount;
    UINT32 ByteIndex;
    UINT8 *Bytes;
    EFI_STATUS Status;
    UINT8 StatusRegister;

    ByteCount = *Size;
    Bytes = Data;
    Status = EFI_NOT_READY;
    for (ByteIndex = 0; ByteIndex < ByteCount; ByteIndex += 1) {
        StatusRegister = NS16550_READ8(Context, Ns16550LineStatus);
        if ((StatusRegister & NS16550_LINE_STATUS_ERRORS) != 0) {
            Status = EFI_DEVICE_ERROR;
            break;
        }

        if ((StatusRegister & NS16550_LINE_STATUS_DATA_READY) == 0) {
            break;
        }

        Bytes[ByteIndex] = NS16550_READ8(Context, Ns16550Data);
        Status = EFI_SUCCESS;
    }

    *Size = ByteIndex;
    return Status;
}

EFI_STATUS
EfipNs16550GetStatus (
    PNS16550_CONTEXT Context,
    BOOLEAN *ReceiveDataAvailable
    )

/*++

Routine Description:

    This routine returns the current device status.

Arguments:

    Context - Supplies a pointer to the serial port context.

    ReceiveDataAvailable - Supplies a pointer where a boolean will be returned
        indicating whether or not receive data is available.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if a device error occurred.

--*/

{

    UINT8 StatusRegister;

    *ReceiveDataAvailable = FALSE;
    StatusRegister = NS16550_READ8(Context, Ns16550LineStatus);
    if ((StatusRegister & NS16550_LINE_STATUS_DATA_READY) != 0) {
        *ReceiveDataAvailable = TRUE;
    }

    return EFI_SUCCESS;
}

UINT8
EfipNs16550ReadIo8 (
    PNS16550_CONTEXT Context,
    NS16550_REGISTER Register
    )

/*++

Routine Description:

    This routine reads a 16550 register from an I/O port.

Arguments:

    Context - Supplies a pointer to the device context.

    Register - Supplies the register to read.

Return Value:

    Returns the value at the register.

--*/

{

    UINT16 Port;

    Port = Context->IoBase + NS16550_REGISTER_OFFSET(Context, Register);
    return EfiIoPortIn8(Port);
}

VOID
EfipNs16550WriteIo8 (
    PNS16550_CONTEXT Context,
    NS16550_REGISTER Register,
    UINT8 Value
    )

/*++

Routine Description:

    This routine writes to an I/O port based 16550 register.

Arguments:

    Context - Supplies a pointer to the device context.

    Register - Supplies the register to write to.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

{

    UINT16 Port;

    Port = Context->IoBase + NS16550_REGISTER_OFFSET(Context, Register);
    EfiIoPortOut8(Port, Value);
    return;
}

UINT8
EfipNs16550ReadMemory8 (
    PNS16550_CONTEXT Context,
    NS16550_REGISTER Register
    )

/*++

Routine Description:

    This routine reads a 16550 register from a memory mapped register.

Arguments:

    Context - Supplies a pointer to the device context.

    Register - Supplies the register to read.

Return Value:

    Returns the value at the register.

--*/

{

    VOID *Address;
    UINT8 Value;

    Address = Context->MemoryBase + NS16550_REGISTER_OFFSET(Context, Register);
    switch (Context->RegisterShift) {
    case NS16550_1_BYTE_REGISTER_SHIFT:
        Value = EfiReadRegister8(Address);
        break;

    case NS16550_2_BYTE_REGISTER_SHIFT:
        Value = EfiReadRegister16(Address);
        break;

    case NS16550_4_BYTE_REGISTER_SHIFT:
    default:
        Value = EfiReadRegister32(Address);
        break;
    }

    return Value;
}

VOID
EfipNs16550WriteMemory8 (
    PNS16550_CONTEXT Context,
    NS16550_REGISTER Register,
    UINT8 Value
    )

/*++

Routine Description:

    This routine writes to a memory mapped 16550 register.

Arguments:

    Context - Supplies a pointer to the device context.

    Register - Supplies the register to write to.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

{

    VOID *Address;

    Address = Context->MemoryBase + NS16550_REGISTER_OFFSET(Context, Register);
    switch (Context->RegisterShift) {
    case NS16550_1_BYTE_REGISTER_SHIFT:
        EfiWriteRegister8(Address, Value);
        break;

    case NS16550_2_BYTE_REGISTER_SHIFT:
        EfiWriteRegister16(Address, (UINT16)Value);
        break;

    case NS16550_4_BYTE_REGISTER_SHIFT:
    default:
        EfiWriteRegister32(Address, (UINT32)Value);
        break;
    }

    return;
}

