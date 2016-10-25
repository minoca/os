/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dbgser.c

Abstract:

    This module implements common debug device routines.

Author:

    Evan Green 26-Feb-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"
#include <minoca/kernel/hmod.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
EfiCoreDebugDeviceReset (
    PVOID Context,
    ULONG BaudRate
    );

KSTATUS
EfiCoreDebugDeviceTransmit (
    PVOID Context,
    PVOID Data,
    ULONG Size
    );

KSTATUS
EfiCoreDebugDeviceReceive (
    PVOID Context,
    PVOID Data,
    PULONG Size
    );

KSTATUS
EfiCoreDebugDeviceGetStatus (
    PVOID Context,
    PBOOL ReceiveDataAvailable
    );

VOID
EfiCoreDebugDeviceDisconnect (
    PVOID Context
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the EFI debug device description.
//

DEBUG_DEVICE_DESCRIPTION EfiDebugDevice = {
    DEBUG_DEVICE_DESCRIPTION_VERSION,
    {
        EfiCoreDebugDeviceReset,
        EfiCoreDebugDeviceTransmit,
        EfiCoreDebugDeviceReceive,
        EfiCoreDebugDeviceGetStatus,
        EfiCoreDebugDeviceDisconnect,
    },

    NULL,
    1
};

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
EfiCoreDebugDeviceReset (
    PVOID Context,
    ULONG BaudRate
    )

/*++

Routine Description:

    This routine initializes and resets a debug device, preparing it to send
    and receive data.

Arguments:

    Context - Supplies the pointer to the port's context, provided by the
        hardware module upon initialization.

    BaudRate - Supplies the baud rate to set.

Return Value:

    STATUS_SUCCESS on success.

    Other status codes on failure. The device will not be used if a failure
    status code is returned.

--*/

{

    EFI_STATUS EfiStatus;

    EfiStatus = EfiPlatformDebugDeviceReset(BaudRate);
    if (EfiStatus == EFI_UNSUPPORTED) {
        return STATUS_NOT_SUPPORTED;

    } else if (EFI_ERROR(EfiStatus)) {
        return STATUS_DEVICE_IO_ERROR;
    }

    return STATUS_SUCCESS;
}

KSTATUS
EfiCoreDebugDeviceTransmit (
    PVOID Context,
    PVOID Data,
    ULONG Size
    )

/*++

Routine Description:

    This routine transmits data from the host out through the debug device.

Arguments:

    Context - Supplies the pointer to the port's context, provided by the
        hardware module upon initialization.

    Data - Supplies a pointer to the data to write.

    Size - Supplies the size to write, in bytes.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

--*/

{

    EFI_STATUS EfiStatus;

    EfiStatus = EfiPlatformDebugDeviceTransmit(Data, Size);
    if (EFI_ERROR(EfiStatus)) {
        return STATUS_DEVICE_IO_ERROR;
    }

    return STATUS_SUCCESS;
}

KSTATUS
EfiCoreDebugDeviceReceive (
    PVOID Context,
    PVOID Data,
    PULONG Size
    )

/*++

Routine Description:

    This routine receives incoming data from the debug device.

Arguments:

    Context - Supplies the pointer to the port's context, provided by the
        hardware module upon initialization.

    Data - Supplies a pointer where the read data will be returned on success.

    Size - Supplies a pointer that on input contains the size of the receive
        buffer. On output, returns the number of bytes read.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_DATA_AVAILABLE if there was no data to be read at the current
    time.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

--*/

{

    EFI_STATUS EfiStatus;
    UINTN NaturalSize;

    NaturalSize = *Size;
    EfiStatus = EfiPlatformDebugDeviceReceive(Data, &NaturalSize);
    *Size = NaturalSize;
    if (EfiStatus == EFI_NOT_READY) {
        return STATUS_NO_DATA_AVAILABLE;

    } else if (EFI_ERROR(EfiStatus)) {
        return STATUS_DEVICE_IO_ERROR;
    }

    return STATUS_SUCCESS;
}

KSTATUS
EfiCoreDebugDeviceGetStatus (
    PVOID Context,
    PBOOL ReceiveDataAvailable
    )

/*++

Routine Description:

    This routine returns the current device status.

Arguments:

    Context - Supplies the pointer to the port's context, provided by the
        hardware module upon initialization.

    ReceiveDataAvailable - Supplies a pointer where a boolean will be returned
        indicating whether or not receive data is available.

Return Value:

    Status code.

--*/

{

    EFI_STATUS EfiStatus;
    BOOLEAN ReceiveAvailable;

    EfiStatus = EfiPlatformDebugDeviceGetStatus(&ReceiveAvailable);
    *ReceiveDataAvailable = ReceiveAvailable;
    if (EFI_ERROR(EfiStatus)) {
        return STATUS_DEVICE_IO_ERROR;
    }

    return STATUS_SUCCESS;
}

VOID
EfiCoreDebugDeviceDisconnect (
    PVOID Context
    )

/*++

Routine Description:

    This routine disconnects a device, taking it offline.

Arguments:

    Context - Supplies the pointer to the port's context, provided by the
        hardware module upon initialization.

Return Value:

    None.

--*/

{

    EfiPlatformDebugDeviceDisconnect();
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

