/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    romusb.c

Abstract:

    This module implements support for the OMAP4 ROM USB interface.

Author:

    Evan Green 1-Apr-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "init.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
EfipOmap4UsbQueueRead (
    PTI_ROM_USB_HANDLE Handle,
    VOID *Data,
    UINTN Length
    );

INTN
EfipOmap4UsbWaitForRead (
    PTI_ROM_USB_HANDLE Handle
    );

INT32
EfipOmap4UsbReadCallback (
    PTI_ROM_PER_HANDLE Handle
    );

VOID
EfipOmap4UsbQueueWrite (
    PTI_ROM_USB_HANDLE Handle,
    VOID *Data,
    UINTN Length
    );

INTN
EfipOmap4UsbWaitForWrite (
    PTI_ROM_USB_HANDLE Handle
    );

INT32
EfipOmap4UsbWriteCallback (
    PTI_ROM_PER_HANDLE Handle
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the handle of the transfer that was just queued.
//

PTI_ROM_USB_HANDLE EfiOmap4ActiveUsbReadHandle;
PTI_ROM_USB_HANDLE EfiOmap4ActiveUsbWriteHandle;

//
// ------------------------------------------------------------------ Functions
//

INTN
EfipOmap4UsbOpen (
    PTI_ROM_USB_HANDLE Handle
    )

/*++

Routine Description:

    This routine opens a connection to the ROM API for the USB device.

Arguments:

    Handle - Supplies a pointer where the connection state will be returned
        on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    UINT32 Base;
    PTI_ROM_PER_HANDLE BootHandle;
    PTI_ROM_GET_PER_DEVICE GetDevice;
    PTI_ROM_GET_PER_DRIVER GetDriver;
    INTN Result;

    EfipInitZeroMemory(Handle, sizeof(TI_ROM_USB_HANDLE));
    if (EfipOmap4GetRevision() >= Omap4460RevisionEs10) {
        Base = OMAP4460_PUBLIC_API_BASE;

    } else {
        Base = OMAP4430_PUBLIC_API_BASE;
    }

    GetDevice = TI_ROM_API(Base + PUBLIC_GET_DEVICE_PER_OFFSET);
    GetDriver = TI_ROM_API(Base + PUBLIC_GET_DRIVER_PER_OFFSET);
    BootHandle = NULL;
    Result = GetDevice(&BootHandle);
    if (Result != 0) {
        return Result;
    }

    if ((BootHandle->DeviceType != OMAP4_ROM_DEVICE_USB) &&
        (BootHandle->DeviceType != OMAP4_ROM_DEVICE_USBEXT)) {

        return -1;
    }

    Result = GetDriver(&(Handle->Driver), BootHandle->DeviceType);
    if (Result != 0) {
        return Result;
    }

    Handle->ReadHandle.TransferMode = BootHandle->TransferMode;
    Handle->ReadHandle.Options = BootHandle->Options;
    Handle->ReadHandle.DeviceType = BootHandle->DeviceType;
    Handle->WriteHandle.TransferMode = BootHandle->TransferMode;
    Handle->WriteHandle.Options = BootHandle->Options;
    Handle->WriteHandle.DeviceType = BootHandle->DeviceType;
    return 0;
}

INTN
EfipOmap4UsbRead (
    PTI_ROM_USB_HANDLE Handle,
    VOID *Data,
    UINTN Length
    )

/*++

Routine Description:

    This routine reads from the USB device.

Arguments:

    Handle - Supplies a pointer to the ROM handle.

    Data - Supplies a pointer where the data will be returned on success.

    Length - Supplies the length of the buffer in bytes.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    UINT8 *Buffer;
    INTN Result;
    UINT32 Transfer;

    Buffer = Data;
    while (Length > 0) {
        Transfer = TI_ROM_USB_MAX_IO_SIZE;
        if (Length < Transfer) {
            Transfer = Length;
        }

        EfipOmap4UsbQueueRead(Handle, Buffer, Transfer);
        Result = EfipOmap4UsbWaitForRead(Handle);
        if (Result != 0) {
            return Result;
        }

        Buffer += Transfer;
        Length -= Transfer;
    }

    return 0;
}

INTN
EfipOmap4UsbWrite (
    PTI_ROM_USB_HANDLE Handle,
    VOID *Data,
    UINTN Length
    )

/*++

Routine Description:

    This routine writes to the USB device.

Arguments:

    Handle - Supplies a pointer to the ROM handle.

    Data - Supplies a pointer to the data to write.

    Length - Supplies the length of the buffer in bytes.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    EfipOmap4UsbQueueWrite(Handle, Data, Length);
    return EfipOmap4UsbWaitForWrite(Handle);
}

VOID
EfipOmap4UsbClose (
    PTI_ROM_USB_HANDLE Handle
    )

/*++

Routine Description:

    This routine closes an open handle to the USB device.

Arguments:

    Handle - Supplies a pointer to the ROM handle.

Return Value:

    None.

--*/

{

    Handle->Driver->Close(&(Handle->ReadHandle));
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
EfipOmap4UsbQueueRead (
    PTI_ROM_USB_HANDLE Handle,
    VOID *Data,
    UINTN Length
    )

/*++

Routine Description:

    This routine queues a read request from the USB device.

Arguments:

    Handle - Supplies a pointer to the ROM handle.

    Data - Supplies a pointer where the data will be returned on success.

    Length - Supplies the length of the buffer in bytes.

Return Value:

    None.

--*/

{

    INTN Result;

    Handle->ReadHandle.Data = Data;
    Handle->ReadHandle.Length = Length;
    Handle->ReadHandle.Status = -1;
    Handle->ReadHandle.TransferMode = TI_ROM_TRANSFER_MODE_DMA;
    Handle->ReadHandle.Callback = EfipOmap4UsbReadCallback;
    EfiOmap4ActiveUsbReadHandle = Handle;
    Result = Handle->Driver->Read(&(Handle->ReadHandle));
    if (Result != 0) {
        Handle->ReadHandle.Status = Result;
    }

    return;
}

INTN
EfipOmap4UsbWaitForRead (
    PTI_ROM_USB_HANDLE Handle
    )

/*++

Routine Description:

    This routine waits for a USB transfer to complete and returns its status.

Arguments:

    Handle - Supplies a pointer to the ROM handle.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    while (TRUE) {
        if ((Handle->ReadHandle.Status != -1) &&
            (Handle->ReadHandle.Status != TI_ROM_STATUS_WAITING)) {

            return Handle->ReadHandle.Status;
        }
    }

    return -1;
}

INT32
EfipOmap4UsbReadCallback (
    PTI_ROM_PER_HANDLE Handle
    )

/*++

Routine Description:

    This routine is called by the ROM when I/O completes.

Arguments:

    Handle - Supplies the handle with the I/O.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    EfiOmap4ActiveUsbReadHandle->ReadHandle.Status = Handle->Status;
    return 0;
}

VOID
EfipOmap4UsbQueueWrite (
    PTI_ROM_USB_HANDLE Handle,
    VOID *Data,
    UINTN Length
    )

/*++

Routine Description:

    This routine queues a write request to the USB device.

Arguments:

    Handle - Supplies a pointer to the ROM handle.

    Data - Supplies a pointer where the data will be returned on success.

    Length - Supplies the length of the buffer in bytes.

Return Value:

    None.

--*/

{

    INTN Result;

    Handle->WriteHandle.Data = Data;
    Handle->WriteHandle.Length = Length;
    Handle->WriteHandle.Status = -1;
    Handle->WriteHandle.TransferMode = TI_ROM_TRANSFER_MODE_DMA;
    Handle->WriteHandle.Callback = EfipOmap4UsbWriteCallback;
    EfiOmap4ActiveUsbWriteHandle = Handle;
    Result = Handle->Driver->Write(&(Handle->WriteHandle));
    if (Result != 0) {
        Handle->WriteHandle.Status = Result;
    }

    return;
}

INTN
EfipOmap4UsbWaitForWrite (
    PTI_ROM_USB_HANDLE Handle
    )

/*++

Routine Description:

    This routine waits for a USB transfer to complete and returns its status.

Arguments:

    Handle - Supplies a pointer to the ROM handle.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    while (TRUE) {
        if ((Handle->WriteHandle.Status != -1) &&
            (Handle->WriteHandle.Status != TI_ROM_STATUS_WAITING)) {

            return Handle->WriteHandle.Status;
        }
    }

    return -1;
}

INT32
EfipOmap4UsbWriteCallback (
    PTI_ROM_PER_HANDLE Handle
    )

/*++

Routine Description:

    This routine is called by the ROM when I/O completes.

Arguments:

    Handle - Supplies the handle with the I/O.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    EfiOmap4ActiveUsbWriteHandle->ReadHandle.Status = Handle->Status;
    return 0;
}

