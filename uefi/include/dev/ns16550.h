/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ns16550.h

Abstract:

    This header contains definitions for the NS 16550 Serial UART.

Author:

    Chris Stevens 10-Jul-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the set of NS 16550 flags.
//

#define NS16550_FLAG_64_BYTE_FIFO                  0x00000001
#define NS16550_FLAG_TRANSMIT_TRIGGER_2_CHARACTERS 0x00000002

//
// Define the possible register shift values.
//

#define NS16550_1_BYTE_REGISTER_SHIFT 0
#define NS16550_2_BYTE_REGISTER_SHIFT 1
#define NS16550_4_BYTE_REGISTER_SHIFT 2

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the context for a 16550 UART.

Members:

    MemoryBase - Stores the optional address of the registers, if the registers
        are memory mapped. This contains NULL for I/O port implementations.

    IoBase - Stores the I/O port base of the registers if they are accessed via
        I/O ports.

    RegisterOffset - Stores the offset in bytes from the start of the register
        base to the 16550 registers.

    RegisterShift - Stores the amount to shift the register number by to get
        the real register.

    BaudRateDivisor - Stores the baud rate divisor.

    Flags - Stores a bitmask of flags. See NS16550_FLAG_* for definitions.

    Read8 - Stores the pointer to the function used to read from the registers.

    Write8 - Stores the pointer to the function used to write to the registers.

--*/

typedef struct _NS16550_CONTEXT {
    VOID *MemoryBase;
    UINT16 IoBase;
    UINTN RegisterOffset;
    UINT32 RegisterShift;
    UINT16 BaudRateDivisor;
    UINT32 Flags;
    VOID *Read8;
    VOID *Write8;
} NS16550_CONTEXT, *PNS16550_CONTEXT;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

EFI_STATUS
EfipNs16550ComputeDivisor (
    UINT32 BaseBaud,
    UINT32 BaudRate,
    UINT16 *Divisor
    );

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

EFI_STATUS
EfipNs16550Initialize (
    PNS16550_CONTEXT Context
    );

/*++

Routine Description:

    This routine initializes the NS 16550 serial port hardware. The caller
    should have initialized at least some of the context structure.

Arguments:

    Context - Supplies the pointer to the port's initialized context.

Return Value:

    EFI Status code.

--*/

EFI_STATUS
EfipNs16550Transmit (
    PNS16550_CONTEXT Context,
    VOID *Data,
    UINTN Size
    );

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

EFI_STATUS
EfipNs16550Receive (
    PNS16550_CONTEXT Context,
    VOID *Data,
    UINTN *Size
    );

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

EFI_STATUS
EfipNs16550GetStatus (
    PNS16550_CONTEXT Context,
    BOOLEAN *ReceiveDataAvailable
    );

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

