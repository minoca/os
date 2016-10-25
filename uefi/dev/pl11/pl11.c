/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pl11.c

Abstract:

    This module implements the firmware serial port interface on a PrimeCell
    PL-011 UART.

Author:

    Evan Green 27-Feb-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "uefifw.h"
#include "dev/pl11.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro performs a 32-bit read from the serial port.
//

#define READ_SERIAL_REGISTER(_Context, _Register) \
    EfiReadRegister32((_Context)->UartBase + _Register)

//
// This macro performs a 32-bit write to the serial port.
//

#define WRITE_SERIAL_REGISTER(_Context, _Register, _Value) \
    EfiWriteRegister32((_Context)->UartBase + _Register, _Value)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define bits for the PL11 UART Line Control Register.
//

#define PL11_UART_LINE_CONTROL_FIFO_ENABLE       0x10
#define PL11_UART_LINE_CONTROL_WORD_LENGTH_8BITS 0x60

//
// Define bits for the PL11 UART Control Register.
//

#define PL11_UART_CONTROL_UART_ENABLE        0x001
#define PL11_UART_CONTROL_TRANSMITTER_ENABLE 0x100
#define PL11_UART_CONTROL_RECEIVER_ENABLE    0x200

//
// Define the interrupt mask for the UART Interrupt Mask Register.
//

#define PL11_UART_INTERRUPT_MASK 0x7FF

//
// Define bits for the PL11 UART Flags Register.
//

#define PL11_UART_FLAG_CLEAR_TO_SEND       0x001
#define PL11_UART_FLAG_DATA_SET_READY      0x002
#define PL11_UART_FLAG_DATA_CARRIER_DETECT 0x004
#define PL11_UART_FLAG_TRANSMIT_BUSY       0x008
#define PL11_UART_FLAG_RECEIVE_EMPTY       0x010
#define PL11_UART_FLAG_TRANSMIT_FULL       0x020
#define PL11_UART_FLAG_RECEIVE_FULL        0x040
#define PL11_UART_FLAG_TRANSMIT_EMPTY      0x080
#define PL11_UART_FLAG_RING_INDICATOR      0x100

//
// Define bits for the PL11 UART Receive Status register.
//

#define PL11_UART_RECEIVE_STATUS_FRAMING_ERROR 0x0001
#define PL11_UART_RECEIVE_STATUS_PARITY_ERROR  0x0002
#define PL11_UART_RECEIVE_STATUS_BREAK_ERROR   0x0004
#define PL11_UART_RECEIVE_STATUS_OVERRUN_ERROR 0x0008
#define PL11_UART_RECEIVE_STATUS_ERROR_MASK    0x000F
#define PL11_UART_RECEIVE_STATUS_ERROR_CLEAR   0xFF00

//
// Define the bits for the PL11 UART data register.
//

#define PL11_UART_DATA_BYTE_MASK     0x00FF
#define PL11_UART_DATA_FRAMING_ERROR 0x0100
#define PL11_UART_DATA_PARITY_ERROR  0x0200
#define PL11_UART_DATA_BREAK_ERROR   0x0400
#define PL11_UART_DATA_OVERRUN_ERROR 0x0800
#define PL11_UART_DATA_ERROR_MASK    0x0F00

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Register set definition for the PL-011. These are offsets in bytes, not
// words.
//

typedef enum _PL011_REGISTER {
    UartDataBuffer          = 0x0,
    UartReceiveStatus       = 0x4,
    UartFlags               = 0x18,
    UartIrDaLowPowerCounter = 0x20,
    UartIntegerBaudRate     = 0x24,
    UartFractionalBaudRate  = 0x28,
    UartLineControl         = 0x2C,
    UartControl             = 0x30,
    UartFifoInterruptLevel  = 0x34,
    UartInterruptMask       = 0x38,
    UartInterruptStatus     = 0x3C,
    UartMaskedInterrupts    = 0x40,
    UartInterruptClear      = 0x44,
    UartDmaControl          = 0x48,
    UartPeripheralId0       = 0xFE0,
    UartPeripheralId1       = 0xFE4,
    UartPeripheralId2       = 0xFE8,
    UartPeripheralId3       = 0xFEC,
    UartPcellId0            = 0xFF0,
    UartPcellId1            = 0xFF4,
    UartPcellId2            = 0xFF8,
    UartPcellId3            = 0xFFC
} PL011_REGISTER, *PPL011_REGISTER;

/*++

Structure Description:

    This structures defines a baud rate for the PL011 UART.

Members:

    BaudRate - Stores the baud rate value.

    IntegerDivisor - Stores the integer divisor to program into the PL011.

    FractionalDivisor - Stores the fractional divisor to program into the PL011.

--*/

typedef struct _BAUD_RATE {
    UINT32 BaudRate;
    UINT32 IntegerDivisor;
    UINT32 FractionalDivisor;
} BAUD_RATE, *PBAUD_RATE;

//
// -------------------------------------------------------------------- Globals
//

//
// Integer and fractional baud rates for an input clock of 14.7456 MHz.
//

BAUD_RATE EfiPl11Available14MhzRates[] = {
    {9600, 0x60, 0},
    {19200, 0x30, 0},
    {38400, 0x18, 0},
    {57600, 0x10, 0},
    {115200, 0x8, 0}
};

//
// Integer and fractional baud rates for an input clock of 3 MHz.
//

BAUD_RATE EfiPl11Available3MhzRates[] = {
    {9600, 19, 34},
    {19200, 9, 49},
    {38400, 4, 57},
    {57600, 3, 16},
    {115200, 1, 40}
};

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipPl11ComputeDivisor (
    UINT32 InputClock,
    UINT32 BaudRate,
    UINT16 *IntegerDivisor,
    UINT16 *FractionalDivisor
    )

/*++

Routine Description:

    This routine computes the divisor rates for a PL-011 UART at a given baud
    rate.

Arguments:

    InputClock - Supplies the input clock frequency in Hertz.

    BaudRate - Supplies the desired baud rate.

    IntegerDivisor - Supplies a pointer where the integer divisor will be
        returned on success.

    FractionalDivisor - Supplies a pointer where the fractional divisor will
        be returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_UNSUPPORTED if the given baud rate cannot be achieved.

--*/

{

    UINT32 BaudRateCount;
    PBAUD_RATE BaudRateData;
    PBAUD_RATE BaudRates;
    UINT32 RateIndex;

    BaudRateData = NULL;
    switch (InputClock) {
    case PL11_CLOCK_FREQUENCY_3MHZ:
        BaudRates = EfiPl11Available3MhzRates;
        BaudRateCount = sizeof(EfiPl11Available3MhzRates) /
                        sizeof(EfiPl11Available3MhzRates[0]);

        break;

    case PL11_CLOCK_FREQUENCY_14MHZ:
        BaudRates = EfiPl11Available14MhzRates;
        BaudRateCount = sizeof(EfiPl11Available14MhzRates) /
                        sizeof(EfiPl11Available14MhzRates[0]);

        break;

    default:
        return EFI_UNSUPPORTED;
    }

    for (RateIndex = 0; RateIndex < BaudRateCount; RateIndex += 1) {
        BaudRateData = &(BaudRates[RateIndex]);
        if (BaudRateData->BaudRate == BaudRate) {
            *IntegerDivisor = BaudRateData->IntegerDivisor;
            *FractionalDivisor = BaudRateData->FractionalDivisor;
            return EFI_SUCCESS;
        }
    }

    return EFI_UNSUPPORTED;
}

EFI_STATUS
EfipPl11Initialize (
    PPL11_CONTEXT Context
    )

/*++

Routine Description:

    This routine initializes the PL-11 serial port hardware. The caller should
    have initialized at least some of the context structure.

Arguments:

    Context - Supplies a pointer to the port's initialized context.

Return Value:

    EFI Status code.

--*/

{

    UINT32 UartControlValue;
    UINT32 UartLineControlValue;

    if ((Context->UartBase == NULL) ||
        ((Context->BaudRateInteger == 0) && (Context->BaudRateFraction == 0))) {

        return EFI_INVALID_PARAMETER;
    }

    //
    // Program the Control Register. Enable the UART, transmitter, and receiver.
    // Clearing the other bits turns off hardware flow control, disables
    // loop-back mode, and disables IrDA features.
    //

    UartControlValue = PL11_UART_CONTROL_UART_ENABLE |
                       PL11_UART_CONTROL_TRANSMITTER_ENABLE |
                       PL11_UART_CONTROL_RECEIVER_ENABLE;

    WRITE_SERIAL_REGISTER(Context, UartControl, UartControlValue);

    //
    // Mask all interrupts.
    //

    WRITE_SERIAL_REGISTER(Context, UartInterruptMask, PL11_UART_INTERRUPT_MASK);

    //
    // Disable DMA.
    //

    WRITE_SERIAL_REGISTER(Context, UartDmaControl, 0);

    //
    // Set the correct divisor values for the chosen baud rate.
    //

    WRITE_SERIAL_REGISTER(Context,
                          UartIntegerBaudRate,
                          Context->BaudRateInteger);

    WRITE_SERIAL_REGISTER(Context,
                          UartFractionalBaudRate,
                          Context->BaudRateFraction);

    //
    // Program the Line Control Register. Clearing bit 4 turns off the FIFO.
    // Clearing bit 3 sets 1 stop bit. Clearing bit 1 sets no parity. Clearing
    // bit 0 means not sending a break. The TRM for the PL-011 implies that the
    // ordering of the Integer Baud Rate, Fractional Baud Rate, and Line Control
    // registers is somewhat fixed, so observe that order here.
    //

    UartLineControlValue = PL11_UART_LINE_CONTROL_FIFO_ENABLE |
                           PL11_UART_LINE_CONTROL_WORD_LENGTH_8BITS;

    WRITE_SERIAL_REGISTER(Context, UartLineControl, UartLineControlValue);

    //
    // Write a 0 to the receive status register to clear all errors.
    //

    WRITE_SERIAL_REGISTER(Context, UartReceiveStatus, 0);
    return EFI_SUCCESS;
}

EFI_STATUS
EfipPl11Transmit (
    PPL11_CONTEXT Context,
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

    UINTN ByteIndex;
    UINT8 *Bytes;

    Bytes = Data;
    for (ByteIndex = 0; ByteIndex < Size; ByteIndex += 1) {

        //
        // Spin waiting for the buffer to become ready to send. If an error is
        // detected, bail out and report to the caller.
        //

        do {
            if ((READ_SERIAL_REGISTER(Context, UartReceiveStatus) &
                 PL11_UART_RECEIVE_STATUS_ERROR_MASK) != 0) {

                return EFI_DEVICE_ERROR;
            }

        } while ((READ_SERIAL_REGISTER(Context, UartFlags) &
                  PL11_UART_FLAG_TRANSMIT_BUSY) != 0);

        //
        // Send the byte and return.
        //

        WRITE_SERIAL_REGISTER(Context, UartDataBuffer, Bytes[ByteIndex]);
    }

    return EFI_SUCCESS;
}

EFI_STATUS
EfipPl11Receive (
    PPL11_CONTEXT Context,
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
    UINT32 DataRegister;
    EFI_STATUS Status;

    ByteCount = *Size;
    Bytes = Data;
    Status = EFI_NOT_READY;

    //
    // The receive status register contains the break, framing, and parity
    // error status for the character read prior to the read of the status. The
    // overrun error is set as soon as an overrun occurs. As a result, read the
    // data register rather than the status register; the data register also
    // returns the status bits.
    //

    for (ByteIndex = 0; ByteIndex < ByteCount; ByteIndex += 1) {
        if ((READ_SERIAL_REGISTER(Context, UartFlags) &
             PL11_UART_FLAG_RECEIVE_EMPTY) != 0) {

            break;
        }

        DataRegister = READ_SERIAL_REGISTER(Context, UartDataBuffer);
        if ((DataRegister & PL11_UART_DATA_ERROR_MASK) != 0) {

            //
            // Clear the errors and return.
            //

            WRITE_SERIAL_REGISTER(Context,
                                  UartReceiveStatus,
                                  PL11_UART_RECEIVE_STATUS_ERROR_CLEAR);

            Status = EFI_DEVICE_ERROR;
            break;
        }

        Bytes[ByteIndex] = DataRegister & PL11_UART_DATA_BYTE_MASK;
        Status = EFI_SUCCESS;
    }

    *Size = ByteIndex;
    return Status;
}

EFI_STATUS
EfipPl11GetStatus (
    PPL11_CONTEXT Context,
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

    UINT32 Flags;

    *ReceiveDataAvailable = FALSE;
    Flags = READ_SERIAL_REGISTER(Context, UartFlags);
    if ((Flags & PL11_UART_FLAG_RECEIVE_EMPTY) == 0) {
        *ReceiveDataAvailable = TRUE;
    }

    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

