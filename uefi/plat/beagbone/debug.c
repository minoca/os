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

    Evan Green 19-Dec-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "uefifw.h"
#include "dev/omapuart.h"
#include <minoca/soc/am335x.h>

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the hard-coded debug serial port.
//

#define EFI_BEAGLEBONE_DEBUG_SERIAL_BASE (VOID *)AM335_UART_0_BASE

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

OMAP_UART_CONTEXT EfiBeagleBoneDebugUart;

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

    Status = EfipUartOmapComputeDivisor(
                                   BaudRate,
                                   &(EfiBeagleBoneDebugUart.BaudRateRegister));

    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfiBeagleBoneDebugUart.UartBase = EFI_BEAGLEBONE_DEBUG_SERIAL_BASE;
    Status = EfipUartOmapInitialize(&EfiBeagleBoneDebugUart);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
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

    EFI_STATUS Status;

    Status = EfipUartOmapTransmit(&EfiBeagleBoneDebugUart, Data, Size);
    return Status;
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

    EFI_STATUS Status;

    Status = EfipUartOmapReceive(&EfiBeagleBoneDebugUart, Data, Size);
    return Status;
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

    return EfipUartOmapGetStatus(&EfiBeagleBoneDebugUart, ReceiveDataAvailable);
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

