/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    serial.c

Abstract:

    This module implements support for the serial device on the TI PandaBoard.

Author:

    Evan Green 8-Apr-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include <minoca/uefi/protocol/serio.h>
#include "pandafw.h"
#include "dev/omapuart.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro returns a pointer to the disk I/O data given a pointer to the
// block I/O protocol instance.
//

#define EFI_PANDA_SERIAL_FROM_THIS(_SerialIo)                 \
        (EFI_PANDA_SERIAL_CONTEXT *)((VOID *)(_SerialIo) -    \
                      ((VOID *)(&(((EFI_PANDA_SERIAL_CONTEXT *)0)->SerialIo))))

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_PANDA_SERIAL_MAGIC 0x72655350 // 'reSP'

#define EFI_PANDA_DEFAULT_SERIAL_BAUD_RATE 115200

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes the PandaBoard Serial I/O device context.

Members:

    Magic - Stores the magic constand EFI_PANDA_SERIAL_MAGIC.

    Handle - Stores the handle to the device.

    DevicePath - Stores a pointer to the device path.

    Uart - Stores the UART context.

    SerialIo - Stores the Serial I/O protocol.

    Mode - Stores the mode information.

--*/

typedef struct _EFI_PANDA_SERIAL_CONTEXT {
    UINT32 Magic;
    EFI_HANDLE Handle;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    OMAP_UART_CONTEXT Uart;
    EFI_SERIAL_IO_PROTOCOL SerialIo;
    EFI_SERIAL_IO_MODE Mode;
} EFI_PANDA_SERIAL_CONTEXT, *PEFI_PANDA_SERIAL_CONTEXT;

/*++

Structure Description:

    This structure defines the PandaBoard Serial I/O device path node.

Members:

    DevicePath - Stores the standard vendor-specific device path.

    ControllerBase - Stores the controller base address.

--*/

typedef struct _EFI_PANDA_SERIAL_IO_DEVICE_PATH_NODE {
    VENDOR_DEVICE_PATH DevicePath;
    UINT32 ControllerBase;
} EFI_PANDA_SERIAL_IO_DEVICE_PATH_NODE, *PEFI_PANDA_SERIAL_IO_DEVICE_PATH_NODE;

/*++

Structure Description:

    This structure defines the PandaBoard Serial I/O device path form.

Members:

    Device - Stores the serial port device path node.

    End - Stores the end device path node.

--*/

#pragma pack(push, 1)

typedef struct _EFI_PANDA_SERIAL_IO_DEVICE_PATH {
    EFI_PANDA_SERIAL_IO_DEVICE_PATH_NODE Device;
    EFI_DEVICE_PATH_PROTOCOL End;
} PACKED EFI_PANDA_SERIAL_IO_DEVICE_PATH, *PEFI_PANDA_SERIAL_IO_DEVICE_PATH;

#pragma pack(pop)

//
// ----------------------------------------------- Internal Function Prototypes
//

EFIAPI
EFI_STATUS
EfipPandaSerialReset (
    EFI_SERIAL_IO_PROTOCOL *This
    );

EFIAPI
EFI_STATUS
EfipPandaSerialSetAttributes (
    EFI_SERIAL_IO_PROTOCOL *This,
    UINT64 BaudRate,
    UINT32 ReceiveFifoDepth,
    UINT32 Timeout,
    EFI_PARITY_TYPE Parity,
    UINT8 DataBits,
    EFI_STOP_BITS_TYPE StopBits
    );

EFIAPI
EFI_STATUS
EfipPandaSerialSetControlBits (
    EFI_SERIAL_IO_PROTOCOL *This,
    UINT32 Control
    );

EFIAPI
EFI_STATUS
EfipPandaSerialGetControlBits (
    EFI_SERIAL_IO_PROTOCOL *This,
    UINT32 *Control
    );

EFIAPI
EFI_STATUS
EfipPandaSerialWrite (
    EFI_SERIAL_IO_PROTOCOL *This,
    UINTN *BufferSize,
    VOID *Buffer
    );

EFIAPI
EFI_STATUS
EfipPandaSerialRead (
    EFI_SERIAL_IO_PROTOCOL *This,
    UINTN *BufferSize,
    VOID *Buffer
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the device path template.
//

EFI_PANDA_SERIAL_IO_DEVICE_PATH EfiPandaSerialIoDevicePathTemplate = {
    {
        {
            {
                HARDWARE_DEVICE_PATH,
                HW_VENDOR_DP,
                sizeof(EFI_PANDA_SERIAL_IO_DEVICE_PATH_NODE)
            },

            EFI_SERIAL_IO_PROTOCOL_GUID,
        },

        0xFFFFFFFF
    },

    {
        END_DEVICE_PATH_TYPE,
        END_ENTIRE_DEVICE_PATH_SUBTYPE,
        END_DEVICE_PATH_LENGTH
    }
};

EFI_PANDA_SERIAL_CONTEXT EfiPandaSerialTemplate = {
    EFI_PANDA_SERIAL_MAGIC,
    NULL,
    NULL,
    {0},
    {
        EFI_SERIAL_IO_PROTOCOL_REVISION,
        EfipPandaSerialReset,
        EfipPandaSerialSetAttributes,
        EfipPandaSerialSetControlBits,
        EfipPandaSerialGetControlBits,
        EfipPandaSerialWrite,
        EfipPandaSerialRead,
        NULL
    },

    {
        EFI_SERIAL_INPUT_BUFFER_EMPTY,
        0,
        0,
        0,
        8,
        DefaultParity,
        DefaultStopBits
    }
};

EFI_GUID EfiSerialIoProtocolGuid = EFI_SERIAL_IO_PROTOCOL_GUID;

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipPandaEnumerateSerial (
    VOID
    )

/*++

Routine Description:

    This routine enumerates the serial port on the PandaBoard.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    VOID *ControllerBase;
    PEFI_PANDA_SERIAL_CONTEXT Device;
    PEFI_PANDA_SERIAL_IO_DEVICE_PATH DevicePath;
    EFI_STATUS Status;

    ControllerBase = (VOID *)OMAP4430_UART3_BASE;

    //
    // Allocate and initialize the context structure.
    //

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_PANDA_SERIAL_CONTEXT),
                             (VOID **)&Device);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfiCopyMem(Device,
               &EfiPandaSerialTemplate,
               sizeof(EFI_PANDA_SERIAL_CONTEXT));

    Device->Handle = NULL;
    Device->SerialIo.Mode = &(Device->Mode);
    Device->Uart.UartBase = ControllerBase;

    //
    // Create the device path.
    //

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_PANDA_SERIAL_IO_DEVICE_PATH),
                             (VOID **)&DevicePath);

    if (EFI_ERROR(Status)) {
        goto PandaEnumerateSerialEnd;
    }

    EfiCopyMem(DevicePath,
               &EfiPandaSerialIoDevicePathTemplate,
               sizeof(EFI_PANDA_SERIAL_IO_DEVICE_PATH));

    DevicePath->Device.ControllerBase = (UINT32)ControllerBase;
    Device->DevicePath = (EFI_DEVICE_PATH_PROTOCOL *)DevicePath;
    Device->Mode.BaudRate = EFI_PANDA_DEFAULT_SERIAL_BAUD_RATE;
    Status = EfiInstallMultipleProtocolInterfaces(&(Device->Handle),
                                                  &EfiDevicePathProtocolGuid,
                                                  Device->DevicePath,
                                                  &EfiSerialIoProtocolGuid,
                                                  &(Device->SerialIo),
                                                  NULL);

PandaEnumerateSerialEnd:
    if (EFI_ERROR(Status)) {
        if (Device != NULL) {
            if (Device->DevicePath != NULL) {
                EfiFreePool(DevicePath);
            }

            EfiFreePool(Device);
        }
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

EFIAPI
EFI_STATUS
EfipPandaSerialReset (
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

    PEFI_PANDA_SERIAL_CONTEXT Device;
    EFI_STATUS Status;

    Device = EFI_PANDA_SERIAL_FROM_THIS(This);
    Status = EfipUartOmapComputeDivisor(Device->Mode.BaudRate,
                                        &(Device->Uart.BaudRateRegister));

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfipUartOmapInitialize(&(Device->Uart));
    return Status;
}

EFIAPI
EFI_STATUS
EfipPandaSerialSetAttributes (
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

    PEFI_PANDA_SERIAL_CONTEXT Device;

    Device = EFI_PANDA_SERIAL_FROM_THIS(This);
    if (BaudRate == 0) {
        BaudRate = EFI_PANDA_DEFAULT_SERIAL_BAUD_RATE;
    }

    if ((ReceiveFifoDepth != 0) ||
        (Timeout != 0) ||
        ((Parity != DefaultParity) && (Parity != NoParity)) ||
        ((DataBits != 0) && (DataBits != 8)) ||
        ((StopBits != DefaultStopBits) && (StopBits != OneStopBit))) {

        return EFI_UNSUPPORTED;
    }

    Device->Mode.BaudRate = BaudRate;
    return This->Reset(This);
}

EFIAPI
EFI_STATUS
EfipPandaSerialSetControlBits (
    EFI_SERIAL_IO_PROTOCOL *This,
    UINT32 Control
    )

/*++

Routine Description:

    This routine sets the control bits on a serial device.

Arguments:

    This - Supplies a pointer to the protocol instance.

    Control - Supplies the control bits to set.

Return Value:

    EFI_SUCCESS if the new control bits were set.

    EFI_UNSUPPORTED if the serial device does not support this operation.

    EFI_DEVICE_ERROR if the device is not functioning properly.

--*/

{

    return EFI_UNSUPPORTED;
}

EFIAPI
EFI_STATUS
EfipPandaSerialGetControlBits (
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

    PEFI_PANDA_SERIAL_CONTEXT Device;
    BOOLEAN ReceiveDataAvailable;
    EFI_STATUS Status;

    Device = EFI_PANDA_SERIAL_FROM_THIS(This);
    if (Device->Uart.BaudRateRegister == 0) {
        Status = This->Reset(This);
        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    Status = EfipUartOmapGetStatus(&(Device->Uart), &ReceiveDataAvailable);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    *Control = 0;
    if (ReceiveDataAvailable == FALSE) {
        *Control |= EFI_SERIAL_INPUT_BUFFER_EMPTY;
    }

    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfipPandaSerialWrite (
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

    PEFI_PANDA_SERIAL_CONTEXT Device;
    UINTN Size;
    EFI_STATUS Status;

    Size = *BufferSize;
    *BufferSize = 0;
    Device = EFI_PANDA_SERIAL_FROM_THIS(This);
    if (Device->Uart.BaudRateRegister == 0) {
        Status = This->Reset(This);
        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    Status = EfipUartOmapTransmit(&(Device->Uart), Buffer, Size);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    *BufferSize = Size;
    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfipPandaSerialRead (
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

    PEFI_PANDA_SERIAL_CONTEXT Device;
    EFI_STATUS Status;

    Device = EFI_PANDA_SERIAL_FROM_THIS(This);
    if (Device->Uart.BaudRateRegister == 0) {
        Status = This->Reset(This);
        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    Status = EfipUartOmapReceive(&(Device->Uart), Buffer, BufferSize);
    if (Status == EFI_NOT_READY) {
        Status = EFI_TIMEOUT;
    }

    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

