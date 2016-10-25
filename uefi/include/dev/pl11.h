/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pl11.h

Abstract:

    This header contains definitions for the ARM PrimeCell PL-011 Serial UART.

Author:

    Evan Green 27-Feb-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define PL11_CLOCK_FREQUENCY_3MHZ 3000000
#define PL11_CLOCK_FREQUENCY_14MHZ 14745600

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the context for a PL011 UART controller. Upon
    initialization, the consumer of the library is responsible for initializing
    some of these values.

Members:

    UartBase - Stores the base address of the UART. The consumer of the library
        should have initialized this value before calling the library
        initialize function.

    BaudRateInteger - Stores the integer portion of the baud rate divisor. The
        consumer of this library should have initialized this value before
        calling the library initialize function.

    BaudRateFraction - Stores the fractional portion of the baud rate divisor.
        The consumer of this library should have initialized this value before
        calling the library initialize function.

--*/

typedef struct _PL11_CONTEXT {
    VOID *UartBase;
    UINT16 BaudRateInteger;
    UINT16 BaudRateFraction;
} PL11_CONTEXT, *PPL11_CONTEXT;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

EFI_STATUS
EfipPl11ComputeDivisor (
    UINT32 InputClock,
    UINT32 BaudRate,
    UINT16 *IntegerDivisor,
    UINT16 *FractionalDivisor
    );

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

EFI_STATUS
EfipPl11Initialize (
    PPL11_CONTEXT Context
    );

/*++

Routine Description:

    This routine initializes the PL-11 serial port hardware. The caller should
    have initialized at least some of the context structure.

Arguments:

    Context - Supplies the pointer to the port's initialized context.

Return Value:

    EFI Status code.

--*/

EFI_STATUS
EfipPl11Transmit (
    PPL11_CONTEXT Context,
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
EfipPl11Receive (
    PPL11_CONTEXT Context,
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
EfipPl11GetStatus (
    PPL11_CONTEXT Context,
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

