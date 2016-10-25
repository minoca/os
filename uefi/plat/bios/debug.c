/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    debug.c

Abstract:

    This module implements debug UART support for BIOS platforms.

Author:

    Evan Green 26-Feb-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include <dev/ns16550.h>

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the hard-coded debug serial port.
//

#define EFI_BIOS_DEBUG_SERIAL_PORT 1

//
// Define the number of serial ports that exist in a PC.
//

#define SERIAL_PORT_COUNT 4

//
// Define the bits for the PC UART Line Status register.
//

#define PC_UART_LINE_STATUS_DATA_READY     0x01
#define PC_UART_LINE_STATUS_TRANSMIT_EMPTY 0x20
#define PC_UART_LINE_STATUS_ERRORS         0x8E

//
// Define the base baud rate for the PC UART. This corresponds to a divisor of
// 1.
//

#define PC_UART_BASE_BAUD 115200

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a baud rate for the PC UART.

Members:

    BaudRate - Stores the baud rate.

    Divisor - Stores the divisor to set for this baud rate.

--*/

typedef struct _BAUD_RATE {
    UINT32 BaudRate;
    UINT16 Divisor;
} BAUD_RATE, *PBAUD_RATE;

typedef enum _COM_REGISTER {
    ComDataBuffer      = 0,
    ComDivisorLow      = 0,
    ComInterruptEnable = 1,
    ComDivisorHigh     = 1,
    ComInterruptStatus = 2,
    ComFifoControl     = 2,
    ComLineControl     = 3,
    ComModemControl    = 4,
    ComLineStatus      = 5,
    ComModemStatus     = 6,
    ComScratch         = 7
} COM_REGISTER, *PCOM_REGISTER;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

NS16550_CONTEXT EfiPcDebugUart;

UINT16 EfiPcSerialIoPortBase[SERIAL_PORT_COUNT] = {
    0x3F8,
    0x2F8,
    0x3E8,
    0x2E8
};

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfiPlatformDebugDeviceReset (
    UINT32 BaudRate
    )

/*++

Routine Description:

    This routine attempts to initialize the serial UART used for debugging.

Arguments:

    BaudRate - Supplies the desired baud rate.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if a device error occurred while resetting the device.

    EFI_UNSUPPORTED if the given baud rate cannot be achieved.

--*/

{

    UINT16 PortBase;
    EFI_STATUS Status;

    Status = EfipNs16550ComputeDivisor(PC_UART_BASE_BAUD,
                                       BaudRate,
                                       &(EfiPcDebugUart.BaudRateDivisor));

    if (EFI_ERROR(Status)) {
        return Status;
    }

    PortBase = EfiPcSerialIoPortBase[EFI_BIOS_DEBUG_SERIAL_PORT - 1];
    EfiPcDebugUart.MemoryBase = NULL;
    EfiPcDebugUart.IoBase = PortBase;
    EfiPcDebugUart.RegisterOffset = 0;
    EfiPcDebugUart.RegisterShift = 0;
    EfiPcDebugUart.Flags = NS16550_FLAG_64_BYTE_FIFO;
    return EfipNs16550Initialize(&EfiPcDebugUart);
}

EFI_STATUS
EfiPlatformDebugDeviceTransmit (
    VOID *Data,
    UINTN Size
    )

/*++

Routine Description:

    This routine transmits data from the host out through the debug device.

Arguments:

    Data - Supplies a pointer to the data to write.

    Size - Supplies the size to write, in bytes.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if a device error occurred.

--*/

{

    return EfipNs16550Transmit(&EfiPcDebugUart, Data, Size);
}

EFI_STATUS
EfiPlatformDebugDeviceReceive (
    VOID *Data,
    UINTN *Size
    )

/*++

Routine Description:

    This routine receives incoming data from the debug device.

Arguments:

    Data - Supplies a pointer where the read data will be returned on success.

    Size - Supplies a pointer that on input contains the size of the receive
        buffer. On output, returns the number of bytes read.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_READY if there was no data to be read at the current time.

    EFI_DEVICE_ERROR if a device error occurred.

--*/

{

    return EfipNs16550Receive(&EfiPcDebugUart, Data, Size);
}

EFI_STATUS
EfiPlatformDebugDeviceGetStatus (
    BOOLEAN *ReceiveDataAvailable
    )

/*++

Routine Description:

    This routine returns the current device status.

Arguments:

    ReceiveDataAvailable - Supplies a pointer where a boolean will be returned
        indicating whether or not receive data is available.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if a device error occurred.

--*/

{

    return EfipNs16550GetStatus(&EfiPcDebugUart, ReceiveDataAvailable);
}

VOID
EfiPlatformDebugDeviceDisconnect (
    VOID
    )

/*++

Routine Description:

    This routine disconnects a device, taking it offline.

Arguments:

    None.

Return Value:

    None.

--*/

{

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

