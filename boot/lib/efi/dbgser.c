/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dbgser.c

Abstract:

    This module implements support for enumerating a debug device out of the
    EFI serial I/O protocol.

Author:

    Evan Green 8-Apr-2014

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/uefi/uefi.h>
#include <minoca/uefi/protocol/serio.h>
#include "firmware.h"
#include "bootlibp.h"
#include "efisup.h"

//
// ---------------------------------------------------------------- Definitions
//

#define BOOT_TEST_BAUD_RATE 115200

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure define an EFI serial I/O debug device.

Members:

    Description - Stores the device description.

    SerialIo - Stores the pointer to the serial I/O protocol instance.

--*/

typedef struct _BOOT_EFI_DEBUG_DEVICE {
    DEBUG_DEVICE_DESCRIPTION Description;
    EFI_SERIAL_IO_PROTOCOL *SerialIo;
} BOOT_EFI_DEBUG_DEVICE, *PBOOT_EFI_DEBUG_DEVICE;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
BopEfiDebugDeviceReset (
    PVOID Context,
    ULONG BaudRate
    );

KSTATUS
BopEfiDebugDeviceTransmit (
    PVOID Context,
    PVOID Data,
    ULONG Size
    );

KSTATUS
BopEfiDebugDeviceReceive (
    PVOID Context,
    PVOID Data,
    PULONG Size
    );

KSTATUS
BopEfiDebugDeviceGetStatus (
    PVOID Context,
    PBOOL ReceiveDataAvailable
    );

VOID
BopEfiDebugDeviceDisconnect (
    PVOID Context
    );

EFI_STATUS
BopEfiSerialReset (
    EFI_SERIAL_IO_PROTOCOL *This
    );

EFI_STATUS
BopEfiSerialSetAttributes (
    EFI_SERIAL_IO_PROTOCOL *This,
    UINT64 BaudRate,
    UINT32 ReceiveFifoDepth,
    UINT32 Timeout,
    EFI_PARITY_TYPE Parity,
    UINT8 DataBits,
    EFI_STOP_BITS_TYPE StopBits
    );

EFI_STATUS
BopEfiSerialGetControlBits (
    EFI_SERIAL_IO_PROTOCOL *This,
    UINT32 *Control
    );

EFI_STATUS
BopEfiSerialSetControlBits (
    EFI_SERIAL_IO_PROTOCOL *This,
    UINT32 Control
    );

EFI_STATUS
BopEfiSerialWrite (
    EFI_SERIAL_IO_PROTOCOL *This,
    UINTN *BufferSize,
    VOID *Buffer
    );

EFI_STATUS
BopEfiSerialRead (
    EFI_SERIAL_IO_PROTOCOL *This,
    UINTN *BufferSize,
    VOID *Buffer
    );

//
// -------------------------------------------------------------------- Globals
//

//
// This boolean disables use of a firmware debug device. Useful for debugging.
//

BOOL BoDisableFirmwareDebugDevice = FALSE;

//
// Store a pointer to an enumerated firmware debug device.
//

PDEBUG_DEVICE_DESCRIPTION BoFirmwareDebugDevice = NULL;

//
// Store the initialized instantiation of the debug device.
//

BOOT_EFI_DEBUG_DEVICE BoEfiDebugDevice = {
    {
        DEBUG_DEVICE_DESCRIPTION_VERSION,
        {
            BopEfiDebugDeviceReset,
            BopEfiDebugDeviceTransmit,
            BopEfiDebugDeviceReceive,
            BopEfiDebugDeviceGetStatus,
            BopEfiDebugDeviceDisconnect,
        },

        NULL,
        1
    },

    NULL
};

EFI_GUID BoEfiSerialIoProtocolGuid = EFI_SERIAL_IO_PROTOCOL_GUID;

//
// ------------------------------------------------------------------ Functions
//

VOID
BopEfiGetDebugDevice (
    VOID
    )

/*++

Routine Description:

    This routine searches for the Serial I/O protocol and enumerates a debug
    devices with it if found.

Arguments:

    None.

Return Value:

    None. Failure is not fatal.

--*/

{

    EFI_STATUS EfiStatus;
    UINTN HandleCount;
    UINTN HandleIndex;
    EFI_HANDLE *Handles;
    EFI_SERIAL_IO_PROTOCOL *SerialIo;

    BoFirmwareDebugDevice = NULL;
    if (BoDisableFirmwareDebugDevice != FALSE) {
        return;
    }

    Handles = NULL;
    HandleCount = 0;
    EfiStatus = BopEfiLocateHandleBuffer(ByProtocol,
                                         &BoEfiSerialIoProtocolGuid,
                                         NULL,
                                         &HandleCount,
                                         &Handles);

    if ((EFI_ERROR(EfiStatus)) || (HandleCount == 0)) {
        return;
    }

    //
    // Loop through all the handles until one is successfully configured.
    //

    for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex += 1) {

        //
        // Get the serial I/O protocol.
        //

        EfiStatus = BopEfiHandleProtocol(Handles[HandleIndex],
                                         &BoEfiSerialIoProtocolGuid,
                                         (VOID **)&SerialIo);

        if (EFI_ERROR(EfiStatus)) {
            continue;
        }

        //
        // Attempt to configure the serial port.
        //

        EfiStatus = BopEfiSerialSetAttributes(SerialIo,
                                              BOOT_TEST_BAUD_RATE,
                                              0,
                                              0,
                                              NoParity,
                                              8,
                                              OneStopBit);

        if (EFI_ERROR(EfiStatus)) {
            continue;
        }

        //
        // The serial port configured correctly. Initialize the debug device.
        //

        BoEfiDebugDevice.SerialIo = SerialIo;
        BoEfiDebugDevice.Description.Identifier = HandleIndex + 1;
        BoEfiDebugDevice.Description.Context = &BoEfiDebugDevice;
        BoFirmwareDebugDevice = &(BoEfiDebugDevice.Description);
        break;
    }

    if (HandleCount != 0) {
        BopEfiFreePool(Handles);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
BopEfiDebugDeviceReset (
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

    PBOOT_EFI_DEBUG_DEVICE Device;
    EFI_STATUS Status;

    Device = Context;
    Status = BopEfiSerialReset(Device->SerialIo);
    if (EFI_ERROR(Status)) {
        return STATUS_DEVICE_IO_ERROR;
    }

    Status = BopEfiSerialSetAttributes(Device->SerialIo,
                                       BaudRate,
                                       0,
                                       0,
                                       NoParity,
                                       8,
                                       OneStopBit);

    if (EFI_ERROR(Status)) {
        return STATUS_DEVICE_IO_ERROR;
    }

    return STATUS_SUCCESS;
}

KSTATUS
BopEfiDebugDeviceTransmit (
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

    PBOOT_EFI_DEBUG_DEVICE Device;
    UINTN EfiSize;
    EFI_STATUS Status;

    Device = Context;
    while (Size != 0) {
        EfiSize = Size;
        Status = BopEfiSerialWrite(Device->SerialIo, &EfiSize, Data);
        if (EFI_ERROR(Status)) {
            return STATUS_DEVICE_IO_ERROR;
        }

        Size -= EfiSize;
        Data += EfiSize;
    }

    return STATUS_SUCCESS;
}

KSTATUS
BopEfiDebugDeviceReceive (
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

    PBOOT_EFI_DEBUG_DEVICE Device;
    UINTN EfiSize;
    EFI_STATUS Status;

    Device = Context;
    EfiSize = *Size;
    Status = BopEfiSerialRead(Device->SerialIo, &EfiSize, Data);
    *Size = EfiSize;
    if (Status == EFI_TIMEOUT) {
        if (EfiSize == 0) {
            return STATUS_NO_DATA_AVAILABLE;

        } else {
            return STATUS_SUCCESS;
        }

    } else if (EFI_ERROR(Status)) {
        return STATUS_DEVICE_IO_ERROR;
    }

    return STATUS_SUCCESS;
}

KSTATUS
BopEfiDebugDeviceGetStatus (
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

    UINT32 Control;
    PBOOT_EFI_DEBUG_DEVICE Device;
    EFI_STATUS Status;

    Device = Context;
    *ReceiveDataAvailable = FALSE;
    Status = BopEfiSerialGetControlBits(Device->SerialIo, &Control);
    if (EFI_ERROR(Status)) {
        return STATUS_DEVICE_IO_ERROR;
    }

    if ((Control & EFI_SERIAL_INPUT_BUFFER_EMPTY) == 0) {
        *ReceiveDataAvailable = TRUE;
    }

    return STATUS_SUCCESS;
}

VOID
BopEfiDebugDeviceDisconnect (
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

    return;
}

EFI_STATUS
BopEfiSerialReset (
    EFI_SERIAL_IO_PROTOCOL *This
    )

/*++

Routine Description:

    This routine resets the serial device.

Arguments:

    This - Supplies a pointer to the protocol instance.

Return Value:

    EFI_SUCCESS if the device was reset.

    EFI_DEVICE_ERROR if the device could not be reset.

--*/

{

    EFI_STATUS Status;

    BopEfiRestoreFirmwareContext();
    Status = This->Reset(This);
    BopEfiRestoreApplicationContext();
    return Status;
}

EFI_STATUS
BopEfiSerialSetAttributes (
    EFI_SERIAL_IO_PROTOCOL *This,
    UINT64 BaudRate,
    UINT32 ReceiveFifoDepth,
    UINT32 Timeout,
    EFI_PARITY_TYPE Parity,
    UINT8 DataBits,
    EFI_STOP_BITS_TYPE StopBits
    )

/*++

Routine Description:

    This routine sets the baud rate, receive FIFO depth, transmit/receive
    timeout, parity, data bits, and stop bits on a serial device.

Arguments:

    This - Supplies a pointer to the protocol instance.

    BaudRate - Supplies the desired baud rate. A value of zero will use the
        default interface speed.

    ReceiveFifoDepth - Supplies the requested depth of the receive FIFO. A
        value of zero uses the default FIFO size.

    Timeout - Supplies the timeout in microseconds for attempting to receive
        a single character. A timeout of zero uses the default timeout.

    Parity - Supplies the type of parity to use on the device.

    DataBits - Supplies the number of bits per byte on the serial device. A
        value of zero uses a default value.

    StopBits - Supplies the number of stop bits to use on the serial device.
        A value of zero uses a default value.

Return Value:

    EFI_SUCCESS if the device was reset.

    EFI_DEVICE_ERROR if the device could not be reset.

--*/

{

    EFI_STATUS Status;

    BopEfiRestoreFirmwareContext();
    Status = This->SetAttributes(This,
                                 BaudRate,
                                 ReceiveFifoDepth,
                                 Timeout,
                                 Parity,
                                 DataBits,
                                 StopBits);

    BopEfiRestoreApplicationContext();
    return Status;
}

EFI_STATUS
BopEfiSerialGetControlBits (
    EFI_SERIAL_IO_PROTOCOL *This,
    UINT32 *Control
    )

/*++

Routine Description:

    This routine gets the control bits on a serial device.

Arguments:

    This - Supplies a pointer to the protocol instance.

    Control - Supplies a pointer where the current control bits will be
        returned.

Return Value:

    EFI_SUCCESS if the new control bits were set.

    EFI_DEVICE_ERROR if the device is not functioning properly.

--*/

{

    EFI_STATUS Status;

    BopEfiRestoreFirmwareContext();
    Status = This->GetControl(This, Control);
    BopEfiRestoreApplicationContext();
    return Status;
}

EFI_STATUS
BopEfiSerialSetControlBits (
    EFI_SERIAL_IO_PROTOCOL *This,
    UINT32 Control
    )

/*++

Routine Description:

    This routine sets the control bits on a serial device.

Arguments:

    This - Supplies a pointer to the protocol instance.

    Control - Supplies the new bits to set.

Return Value:

    EFI_SUCCESS if the new control bits were set.

    EFI_DEVICE_ERROR if the device is not functioning properly.

--*/

{

    EFI_STATUS Status;

    BopEfiRestoreFirmwareContext();
    Status = This->SetControl(This, Control);
    BopEfiRestoreApplicationContext();
    return Status;
}

EFI_STATUS
BopEfiSerialWrite (
    EFI_SERIAL_IO_PROTOCOL *This,
    UINTN *BufferSize,
    VOID *Buffer
    )

/*++

Routine Description:

    This routine writes data to a serial device.

Arguments:

    This - Supplies a pointer to the protocol instance.

    BufferSize - Supplies a pointer that on input contains the size of the
        buffer. On output, the number of bytes successfully written will be
        returned.

    Buffer - Supplies a pointer to the data to write.

Return Value:

    EFI_SUCCESS if the data was written

    EFI_DEVICE_ERROR if the device is not functioning properly.

    EFI_TIMEOUT if the operation timed out before the data could be written.

--*/

{

    EFI_STATUS Status;

    BopEfiRestoreFirmwareContext();
    Status = This->Write(This, BufferSize, Buffer);
    BopEfiRestoreApplicationContext();
    return Status;
}

EFI_STATUS
BopEfiSerialRead (
    EFI_SERIAL_IO_PROTOCOL *This,
    UINTN *BufferSize,
    VOID *Buffer
    )

/*++

Routine Description:

    This routine reads data from a serial device.

Arguments:

    This - Supplies a pointer to the protocol instance.

    BufferSize - Supplies a pointer that on input contains the size of the
        buffer. On output, the number of bytes successfully read will be
        returned.

    Buffer - Supplies a pointer where the read data will be returned on success.

Return Value:

    EFI_SUCCESS if the data was read.

    EFI_DEVICE_ERROR if the device is not functioning properly.

    EFI_TIMEOUT if the operation timed out before the data could be read.

--*/

{

    EFI_STATUS Status;

    BopEfiRestoreFirmwareContext();
    Status = This->Read(This, BufferSize, Buffer);
    BopEfiRestoreApplicationContext();
    return Status;
}

