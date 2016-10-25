/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    omapuart.h

Abstract:

    This header contains library definitions for the serial UART found on
    Texas Instruments OMAP3 and OMAP4 SoCs.

Author:

    Evan Green 27-Feb-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the context for an OMAP UART controller. Upon
    initialization, the consumer of the library is responsible for initializing
    some of these values.

Members:

    UartBase - Stores the base address of the UART. The consumer of the library
        should have initialized this value before calling the library
        initialize function.

    BaudRateRegister - Stores the value to put in the baud rate register. The
        consumer of this library should have initialized this value before
        calling the library initialize function.

--*/

typedef struct _OMAP_UART_CONTEXT {
    VOID *UartBase;
    UINT16 BaudRateRegister;
} OMAP_UART_CONTEXT, *POMAP_UART_CONTEXT;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

EFI_STATUS
EfipUartOmapComputeDivisor (
    UINTN BaudRate,
    UINT16 *Divisor
    );

/*++

Routine Description:

    This routine computes the divisor for the given baud rate.

Arguments:

    BaudRate - Supplies the desired baud rate.

    Divisor - Supplies a pointer where the divisor will be returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_UNSUPPORTED if the baud rate cannot be achieved.

--*/

EFI_STATUS
EfipUartOmapInitialize (
    POMAP_UART_CONTEXT Context
    );

/*++

Routine Description:

    This routine initializes the OMAP UART controller.

Arguments:

    Context - Supplies the pointer to the port's context. The caller should
        have initialized some of these members.

Return Value:

    EFI status code.

--*/

EFI_STATUS
EfipUartOmapTransmit (
    POMAP_UART_CONTEXT Context,
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
EfipUartOmapReceive (
    POMAP_UART_CONTEXT Context,
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
EfipUartOmapGetStatus (
    POMAP_UART_CONTEXT Context,
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

