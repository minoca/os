/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    debug.c

Abstract:

    This module implements debug UART support for UEFI platforms.

Author:

    Chris Stevens 21-Dec-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include <dev/pl11.h>
#include "rpifw.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// Define the context for the debug UART.
//

PL11_CONTEXT EfiRaspberryPiDebugUart;

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

    EFI_STATUS Status;

    EfiRaspberryPiDebugUart.UartBase =
                                    (VOID *)BCM2835_BASE + BCM2709_UART_OFFSET;

    Status = EfipPl11ComputeDivisor(
                                  PL11_CLOCK_FREQUENCY_3MHZ,
                                  BaudRate,
                                  &(EfiRaspberryPiDebugUart.BaudRateInteger),
                                  &(EfiRaspberryPiDebugUart.BaudRateFraction));

    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EfipPl11Initialize(&EfiRaspberryPiDebugUart);
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

    return EfipPl11Transmit(&EfiRaspberryPiDebugUart, Data, Size);
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

    return EfipPl11Receive(&EfiRaspberryPiDebugUart, Data, Size);
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

    return EfipPl11GetStatus(&EfiRaspberryPiDebugUart, ReceiveDataAvailable);
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

