/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ftdi.c

Abstract:

    This module implements the FTDI USB to Serial Port KD USB driver.

Author:

    Evan Green 3-Apr-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "kdusbp.h"

//
// ---------------------------------------------------------------- Definitions
//

#define KD_FTDI_CONFIGURATION_BUFFER_SIZE 256

#define FTDI_REQUEST_RESET          0x00
#define FTDI_REQUEST_SET_BAUD_RATE  0x03

#define FTDI_INTERFACE_ANY  0
#define FTDI_INTERFACE_A    1
#define FTDI_INTERFACE_B    2
#define FTDI_INTERFACE_C    3
#define FTDI_INTERFACE_D    4

#define FTDI_REVISION_AM 0x200
#define FTDI_REVISION_BM 0x400
#define FTDI_REVISION_2232C 0x500

#define FTDI_FUNDAMENTAL_CLOCK 24000000

#define FTDI_MAX_DIVISOR_AM 0x1FFF8
#define FTDI_MAX_DIVISOR_BM 0x1FFFF

//
// Define the number of status bytes that come in before every read.
//

#define FTDI_READ_STATUS_SIZE 2

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _FTDI_CHIP_TYPE {
    FtdiTypeInvalid,
    FtdiTypeOld,
    FtdiTypeAm,
    FtdiTypeBm,
    FtdiType2232C
} FTDI_CHIP_TYPE, *PFTDI_CHIP_TYPE;

/*++

Structure Description:

    This structure describes the context of an FTDI USB device.

Members:

    Device - Stores a pointer to the USB device.

    BulkOutEndpoint - Stores the bulk out endpoint information.

    BulkInEndpoint - Stores the bulk in endpoint information.

    TransferOut - Stores the outgoing transfer.

    TransferIn - Stores the incoming transfer.

    TransferInOffset - Stores the current offset to the IN transfer data where
        the next byte to be returned is.

    TransferInQueued - Stores a boolean indicating whether or not the
        receive transfer has been submitted.

    TransferInSetup - Stores a boolean indicating whether or not the receive
        transfer has been set up (and therefore needs retiring).

    Index - Stores the serial port to talk to. See FTDI_INTERFACE_* definitions.

    ChipType - Stores the chip type.

--*/

typedef struct _KD_FTDI_DEVICE {
    PKD_USB_DEVICE Device;
    DEBUG_USB_ENDPOINT BulkOutEndpoint;
    DEBUG_USB_ENDPOINT BulkInEndpoint;
    DEBUG_USB_TRANSFER TransferOut;
    DEBUG_USB_TRANSFER TransferIn;
    ULONG TransferInOffset;
    BOOL TransferInQueued;
    BOOL TransferInSetup;
    USHORT Index;
    FTDI_CHIP_TYPE ChipType;
} KD_FTDI_DEVICE, *PKD_FTDI_DEVICE;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
KdpFtdiReset (
    PVOID Context,
    ULONG BaudRate
    );

KSTATUS
KdpFtdiTransmit (
    PVOID Context,
    PVOID Data,
    ULONG Size
    );

KSTATUS
KdpFtdiReceive (
    PVOID Context,
    PVOID Data,
    PULONG Size
    );

KSTATUS
KdpFtdiGetStatus (
    PVOID Context,
    PBOOL ReceiveDataAvailable
    );

VOID
KdpFtdiDisconnect (
    PVOID Context
    );

KSTATUS
KdpFtdiInitializeEndpoints (
    PKD_FTDI_DEVICE Device
    );

ULONG
KdpFtdiCalculateDivisor (
    PKD_FTDI_DEVICE Device,
    ULONG BaudRate,
    PUSHORT Value,
    PUSHORT Index
    );

//
// -------------------------------------------------------------------- Globals
//

CHAR KdFtdiAmAdjustUp[8] = {0, 0, 0, 1, 0, 3, 2, 1};
CHAR KdFtdiAmAdjustDown[8] = {0, 0, 0, 1, 0, 1, 2, 3};
CHAR KdFtdiFractionCode[8] = {0, 3, 2, 4, 1, 5, 6, 7};

DEBUG_DEVICE_DESCRIPTION KdFtdiInterfaceTemplate = {
    DEBUG_DEVICE_DESCRIPTION_VERSION,
    {
        KdpFtdiReset,
        KdpFtdiTransmit,
        KdpFtdiReceive,
        KdpFtdiGetStatus,
        KdpFtdiDisconnect
    },

    NULL
};

//
// Store the single device instance.
//

KD_FTDI_DEVICE KdFtdiDevice;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
KdpFtdiDriverEntry (
    PKD_USB_DEVICE Device,
    PDEBUG_DEVICE_DESCRIPTION Interface
    )

/*++

Routine Description:

    This routine initializes an FTDI USB to Serial KD USB device.

Arguments:

    Device - Supplies a pointer to the device the driver lives on.

    Interface - Supplies a pointer where the driver fills in the I/O
        interface on success.

Return Value:

    Status code.

--*/

{

    PKD_FTDI_DEVICE FtdiDevice;
    KSTATUS Status;

    //
    // Use a single static device.
    //

    FtdiDevice = &KdFtdiDevice;
    RtlZeroMemory(FtdiDevice, sizeof(KD_FTDI_DEVICE));
    FtdiDevice->Device = Device;
    Status = KdpFtdiInitializeEndpoints(FtdiDevice);
    if (!KSUCCESS(Status)) {
        goto FtdiDriverEntry;
    }

    RtlCopyMemory(Interface,
                  &KdFtdiInterfaceTemplate,
                  sizeof(DEBUG_DEVICE_DESCRIPTION));

    Interface->Context = FtdiDevice;

FtdiDriverEntry:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
KdpFtdiReset (
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

    PKD_FTDI_DEVICE Device;
    ULONG Length;
    USB_SETUP_PACKET Setup;
    KSTATUS Status;

    Device = Context;
    if ((Device->TransferInQueued != FALSE) ||
        (Device->TransferInSetup != FALSE)) {

        Status = KdpUsbRetireTransfer(Device->Device->Controller,
                                      &(Device->TransferIn));

        if (!KSUCCESS(Status)) {
            goto FtdiResetEnd;
        }

        Device->TransferInQueued = FALSE;
        Device->TransferInSetup = FALSE;
    }

    //
    // Always skip the two status bytes at the beginning of each read.
    //

    Device->TransferInOffset = FTDI_READ_STATUS_SIZE;

    //
    // Reset the device.
    //

    Setup.RequestType = USB_SETUP_REQUEST_TO_DEVICE |
                        USB_SETUP_REQUEST_VENDOR |
                        USB_SETUP_REQUEST_DEVICE_RECIPIENT;

    Setup.Request = FTDI_REQUEST_RESET;
    Setup.Value = 0;
    Setup.Index = Device->Index;
    Setup.Length = 0;
    Length = Setup.Length;
    Status = KdpUsbDefaultControlTransfer(Device->Device,
                                          &Setup,
                                          DebugUsbTransferDirectionOut,
                                          NULL,
                                          &Length);

    if (!KSUCCESS(Status)) {
        goto FtdiResetEnd;
    }

    //
    // Set the baud rate.
    //

    Setup.Request = FTDI_REQUEST_SET_BAUD_RATE;
    KdpFtdiCalculateDivisor(Device, BaudRate, &(Setup.Value), &(Setup.Index));
    Status = KdpUsbDefaultControlTransfer(Device->Device,
                                          &Setup,
                                          DebugUsbTransferDirectionOut,
                                          NULL,
                                          &Length);

    if (!KSUCCESS(Status)) {
        goto FtdiResetEnd;
    }

    //
    // Initialize the outbound and inbound transfers.
    //

    RtlZeroMemory(&(Device->TransferOut), sizeof(DEBUG_USB_TRANSFER));
    Device->TransferOut.Endpoint = &(Device->BulkOutEndpoint);
    Device->TransferOut.Direction = DebugUsbTransferDirectionOut;
    Device->TransferOut.Length = 0;
    RtlZeroMemory(&(Device->TransferIn), sizeof(DEBUG_USB_TRANSFER));
    Device->TransferIn.Endpoint = &(Device->BulkInEndpoint);
    Device->TransferIn.Direction = DebugUsbTransferDirectionIn;
    Device->TransferIn.Length = 0;

FtdiResetEnd:
    return Status;
}

KSTATUS
KdpFtdiTransmit (
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

    ULONG BytesThisRound;
    PKD_FTDI_DEVICE Device;
    KSTATUS Status;

    Device = Context;
    while (Size != 0) {
        BytesThisRound = Device->BulkOutEndpoint.MaxPacketSize;
        if (BytesThisRound > Size) {
            BytesThisRound = Size;
        }

        Device->TransferOut.Length = BytesThisRound;
        Status = KdpUsbSetupTransfer(Device->Device->Controller,
                                     &(Device->TransferOut));

        if (!KSUCCESS(Status)) {
            goto FtdiTransmitEnd;
        }

        RtlCopyMemory(Device->TransferOut.Buffer, Data, BytesThisRound);
        Status = KdpUsbSubmitTransfer(Device->Device->Controller,
                                      &(Device->TransferOut),
                                      TRUE);

        KdpUsbRetireTransfer(Device->Device->Controller,
                             &(Device->TransferOut));

        if (!KSUCCESS(Status)) {
            goto FtdiTransmitEnd;
        }

        Data += BytesThisRound;
        Size -= BytesThisRound;
    }

    Status = STATUS_SUCCESS;

FtdiTransmitEnd:
    return Status;
}

KSTATUS
KdpFtdiReceive (
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

    ULONG BytesThisRound;
    ULONG BytesToRead;
    PHARDWARE_USB_DEBUG_DEVICE Controller;
    PKD_FTDI_DEVICE Device;
    KSTATUS Status;
    PDEBUG_USB_TRANSFER TransferIn;

    Device = Context;
    Controller = Device->Device->Controller;
    TransferIn = &(Device->TransferIn);
    BytesToRead = *Size;
    Status = STATUS_SUCCESS;
    while (BytesToRead != 0) {

        //
        // If the transfer is currently queued, check to see if it's finished.
        //

        if (Device->TransferInQueued != FALSE) {
            Status = KdpUsbCheckTransfer(Controller, TransferIn);
            if (Status == STATUS_MORE_PROCESSING_REQUIRED) {
                if (BytesToRead != *Size) {
                    Status = STATUS_SUCCESS;

                } else {
                    Status = STATUS_NO_DATA_AVAILABLE;
                }

                break;

            //
            // If checking the transfer failed, retire it and stop.
            //

            } else if (!KSUCCESS(Status)) {
                KdpUsbRetireTransfer(Controller, TransferIn);
                Device->TransferIn.LengthTransferred = 0;
                Device->TransferInQueued = FALSE;
                Device->TransferInSetup = FALSE;
                break;
            }

            //
            // The transfer is complete. Set the offset to skip the two status
            // bytes.
            //

            Device->TransferInQueued = FALSE;
            Device->TransferInOffset = FTDI_READ_STATUS_SIZE;
        }

        //
        // Copy bytes from the completed transfer.
        //

        if (Device->TransferInOffset < TransferIn->LengthTransferred) {
            BytesThisRound = TransferIn->LengthTransferred -
                             Device->TransferInOffset;

            if (BytesThisRound > BytesToRead) {
                BytesThisRound = BytesToRead;
            }

            RtlCopyMemory(Data,
                          TransferIn->Buffer + Device->TransferInOffset,
                          BytesThisRound);

            Device->TransferInOffset += BytesThisRound;
            BytesToRead -= BytesThisRound;
            Data += BytesThisRound;
        }

        //
        // If the transfer was completely consumed by the caller, resubmit the
        // transfer.
        //

        if (Device->TransferInOffset >= TransferIn->LengthTransferred) {

            //
            // Retire the transfer if it hasn't been done yet.
            //

            if (Device->TransferInSetup != FALSE) {
                KdpUsbRetireTransfer(Controller, TransferIn);
                Device->TransferInSetup = FALSE;
            }

            //
            // Set up the transfer.
            //

            Device->TransferIn.Length = Device->BulkInEndpoint.MaxPacketSize;
            Status = KdpUsbSetupTransfer(Controller, TransferIn);
            if (!KSUCCESS(Status)) {
                break;
            }

            Device->TransferInSetup = TRUE;

            //
            // Submit the transfer asynchronously.
            //

            Status = KdpUsbSubmitTransfer(Controller, TransferIn, FALSE);
            if (!KSUCCESS(Status)) {
                KdpUsbRetireTransfer(Controller, TransferIn);
                Device->TransferInSetup = FALSE;
                break;
            }

            Device->TransferInQueued = TRUE;
        }
    }

    //
    // Return the number of bytes transferred.
    //

    *Size = *Size - BytesToRead;
    return Status;
}

KSTATUS
KdpFtdiGetStatus (
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

    PHARDWARE_USB_DEBUG_DEVICE Controller;
    PKD_FTDI_DEVICE Device;
    KSTATUS Status;
    PDEBUG_USB_TRANSFER TransferIn;

    *ReceiveDataAvailable = FALSE;
    Device = Context;
    Controller = Device->Device->Controller;
    TransferIn = &(Device->TransferIn);

    //
    // If there is still data to read from a previous transfer, return that
    // there is data available.
    //

    if ((Device->TransferInSetup != FALSE) &&
        (Device->TransferInQueued == FALSE)) {

        if (Device->TransferInOffset < TransferIn->LengthTransferred) {
            *ReceiveDataAvailable = TRUE;
            Status = STATUS_SUCCESS;
            goto FtdiGetStatusEnd;

        //
        // This situation shouldn't hit, as it implies that the receive loop
        // ran out of data but didn't retire the transfer. Handle it anyway.
        //

        } else {
            KdpUsbRetireTransfer(Controller, TransferIn);
            Device->TransferInSetup = FALSE;
        }
    }

    //
    // Set up the transfer if it is not yet created.
    //

    if (Device->TransferInSetup == FALSE) {
        Device->TransferIn.Length = Device->BulkInEndpoint.MaxPacketSize;
        Status = KdpUsbSetupTransfer(Controller, TransferIn);
        if (!KSUCCESS(Status)) {
            goto FtdiGetStatusEnd;
        }

        Device->TransferInSetup = TRUE;
    }

    //
    // Submit the transfer (asynchronously) if it is not already queued.
    //

    if (Device->TransferInQueued == FALSE) {
        Status = KdpUsbSubmitTransfer(Controller, TransferIn, FALSE);
        if (!KSUCCESS(Status)) {
            KdpUsbRetireTransfer(Controller, TransferIn);
            Device->TransferInSetup = FALSE;
            goto FtdiGetStatusEnd;
        }

        Device->TransferInQueued = TRUE;
    }

    //
    // Check the transfer to see if it's finished.
    //

    Status = KdpUsbCheckTransfer(Controller, TransferIn);
    if (Status == STATUS_MORE_PROCESSING_REQUIRED) {
        Status = STATUS_SUCCESS;

    } else if ((!KSUCCESS(Status)) ||
               (TransferIn->LengthTransferred <= FTDI_READ_STATUS_SIZE)) {

        KdpUsbRetireTransfer(Controller, TransferIn);
        Device->TransferInQueued = FALSE;
        Device->TransferInSetup = FALSE;

    //
    // If there was data other than the status bytes, return it.
    //

    } else {
        Device->TransferInQueued = FALSE;
        Device->TransferInOffset = FTDI_READ_STATUS_SIZE;
        *ReceiveDataAvailable = TRUE;
    }

FtdiGetStatusEnd:
    return Status;
}

VOID
KdpFtdiDisconnect (
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

    PKD_FTDI_DEVICE Device;

    Device = Context;

    //
    // Cancel the IN transfer.
    //

    if (Device->TransferInSetup != FALSE) {
        KdpUsbRetireTransfer(Device->Device->Controller, &(Device->TransferIn));
        Device->TransferInQueued = FALSE;
        Device->TransferInSetup = FALSE;
    }

    return;
}

KSTATUS
KdpFtdiInitializeEndpoints (
    PKD_FTDI_DEVICE Device
    )

/*++

Routine Description:

    This routine reads the configuration descriptor and initializes the FTDI
    endpoint information.

Arguments:

    Device - Supplies a pointer to the FTDI device.

Return Value:

    Status code.

--*/

{

    UCHAR Buffer[KD_FTDI_CONFIGURATION_BUFFER_SIZE];
    PUSB_CONFIGURATION_DESCRIPTOR Configuration;
    PUSB_DEVICE_DESCRIPTOR DeviceDescriptor;
    PUSB_ENDPOINT_DESCRIPTOR Endpoint;
    ULONG EndpointCount;
    ULONG EndpointIndex;
    BOOL FoundIn;
    BOOL FoundOut;
    PUSB_INTERFACE_DESCRIPTOR Interface;
    ULONG Length;
    USB_SETUP_PACKET Setup;
    KSTATUS Status;

    Device->ChipType = FtdiTypeBm;

    //
    // Request the default configuration.
    //

    RtlZeroMemory(&Setup, sizeof(USB_SETUP_PACKET));
    Setup.RequestType = USB_SETUP_REQUEST_TO_HOST |
                        USB_SETUP_REQUEST_STANDARD |
                        USB_SETUP_REQUEST_DEVICE_RECIPIENT;

    Setup.Request = USB_DEVICE_REQUEST_GET_DESCRIPTOR;
    Setup.Value = (UsbDescriptorTypeConfiguration << 8) | 0;
    Setup.Index = 0;
    Setup.Length = sizeof(Buffer);
    Length = Setup.Length;
    Configuration = (PUSB_CONFIGURATION_DESCRIPTOR)Buffer;
    Status = KdpUsbDefaultControlTransfer(Device->Device,
                                          &Setup,
                                          DebugUsbTransferDirectionIn,
                                          Configuration,
                                          &Length);

    if (!KSUCCESS(Status)) {
        goto FtdiInitializeEndpointsEnd;
    }

    if (Length < sizeof(USB_CONFIGURATION_DESCRIPTOR)) {
        Status = STATUS_INVALID_CONFIGURATION;
        goto FtdiInitializeEndpointsEnd;
    }

    //
    // Loop through the interfaces looking for a hub interface.
    //

    FoundIn = FALSE;
    FoundOut = FALSE;
    Interface = (PUSB_INTERFACE_DESCRIPTOR)((PVOID)Configuration +
                                            Configuration->Length);

    if (Configuration->InterfaceCount == 2) {
        Device->ChipType = FtdiType2232C;
        if (Device->Index == FTDI_INTERFACE_ANY) {
            Device->Index = FTDI_INTERFACE_A;
        }
    }

    while ((UINTN)(Interface + 1) <= (UINTN)Buffer + Length) {
        Endpoint = ((PVOID)Interface) + Interface->Length;
        if (Interface->DescriptorType == UsbDescriptorTypeInterface) {
            FoundIn = FALSE;
            FoundOut = FALSE;

            //
            // Loop through all the endpoints in the interface.
            //

            EndpointIndex = 0;
            EndpointCount = Interface->EndpointCount;
            while (((UINTN)(Endpoint + 1) <= ((UINTN)Buffer + Length)) &&
                   (EndpointIndex < EndpointCount)) {

                if (Endpoint->DescriptorType == UsbDescriptorTypeEndpoint) {
                    if ((Endpoint->Attributes &
                         USB_ENDPOINT_ATTRIBUTES_TYPE_MASK) ==
                        USB_ENDPOINT_ATTRIBUTES_TYPE_BULK) {

                        if ((Endpoint->EndpointAddress &
                             USB_ENDPOINT_ADDRESS_DIRECTION_IN) != 0) {

                            Status = KdpUsbInitializeEndpoint(
                                                    Device->Device,
                                                    Endpoint,
                                                    &(Device->BulkInEndpoint));

                            if (KSUCCESS(Status)) {
                                FoundIn = TRUE;
                            }

                        } else {
                            Status = KdpUsbInitializeEndpoint(
                                                   Device->Device,
                                                   Endpoint,
                                                   &(Device->BulkOutEndpoint));

                            if (KSUCCESS(Status)) {
                                FoundOut = TRUE;
                            }
                        }
                    }

                    EndpointIndex += 1;
                }

                Endpoint = ((PVOID)Endpoint) + Endpoint->Length;
            }

            if ((FoundIn != FALSE) && (FoundOut != FALSE)) {
                break;
            }
        }

        Interface = (PVOID)Endpoint;
    }

    if ((FoundIn == FALSE) || (FoundOut == FALSE)) {
        Status = STATUS_INVALID_CONFIGURATION;
        goto FtdiInitializeEndpointsEnd;
    }

    //
    // Request the device descriptor to get the version out of it.
    //

    Setup.RequestType = USB_SETUP_REQUEST_TO_HOST |
                        USB_SETUP_REQUEST_STANDARD |
                        USB_SETUP_REQUEST_DEVICE_RECIPIENT;

    Setup.Request = USB_DEVICE_REQUEST_GET_DESCRIPTOR;
    Setup.Value = UsbDescriptorTypeDevice << 8;
    Setup.Index = 0;
    Setup.Length = sizeof(USB_DEVICE_DESCRIPTOR);
    Length = Setup.Length;
    DeviceDescriptor = (PUSB_DEVICE_DESCRIPTOR)Buffer;
    Status = KdpUsbDefaultControlTransfer(Device->Device,
                                          &Setup,
                                          DebugUsbTransferDirectionIn,
                                          DeviceDescriptor,
                                          &Length);

    if (!KSUCCESS(Status)) {
        goto FtdiInitializeEndpointsEnd;
    }

    if (Length != Setup.Length) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto FtdiInitializeEndpointsEnd;
    }

    if (DeviceDescriptor->DescriptorType != UsbDescriptorTypeDevice) {
        Status = STATUS_DEVICE_IO_ERROR;
        goto FtdiInitializeEndpointsEnd;
    }

    if (DeviceDescriptor->DeviceRevision < FTDI_REVISION_AM) {
        Device->ChipType = FtdiTypeOld;

    } else if ((DeviceDescriptor->DeviceRevision < FTDI_REVISION_BM) &&
               (DeviceDescriptor->SerialNumberStringIndex != 0)) {

        Device->ChipType = FtdiTypeAm;

    } else if (DeviceDescriptor->DeviceRevision < FTDI_REVISION_2232C) {
        Device->ChipType = FtdiTypeBm;
    }

    Status = STATUS_SUCCESS;

FtdiInitializeEndpointsEnd:
    return Status;
}

ULONG
KdpFtdiCalculateDivisor (
    PKD_FTDI_DEVICE Device,
    ULONG BaudRate,
    PUSHORT Value,
    PUSHORT Index
    )

/*++

Routine Description:

    This routine computes the divisor (in the form of index and value fields of
    a setup packet) for a given baud rate.

Arguments:

    Device - Supplies a pointer to the FTDI device.

    BaudRate - Supplies the desired baud rate.

    Value - Supplies a pointer where the value field will be returned on
        success.

    Index - Supplies a pointer where th index field will be returned on success.

Return Value:

    Returns the actual baud rate.

--*/

{

    ULONG BaudRateDifference;
    ULONG BaudRateEstimate;
    ULONG BestBaudRate;
    ULONG BestBaudRateDifference;
    ULONG BestDivisor;
    ULONG Divisor;
    ULONG EncodedDivisor;
    ULONG Try;
    ULONG TryDivisor;

    if (BaudRate == 0) {
        return 0;
    }

    Divisor = FTDI_FUNDAMENTAL_CLOCK / BaudRate;

    //
    // On AM devices, round down to one of the supported fractional values.
    //

    if (Device->ChipType == FtdiTypeAm) {
        Divisor -= KdFtdiAmAdjustDown[Divisor & 0x7];
    }

    //
    // Try this divisor and the one above it to see which one is closer.
    //

    BestBaudRate = 0;
    BestBaudRateDifference = -1;
    BestDivisor = Divisor;
    for (Try = 0; Try < 2; Try += 1) {
        TryDivisor = Divisor + Try;

        //
        // Round up to the minimum divisor value. BM doesn't support 9
        // through 11, AM doesn't support 9 through 15.
        //

        if (TryDivisor <= 8) {
            TryDivisor = 8;

        } else if ((Device->ChipType > FtdiTypeAm) && (TryDivisor < 12)) {
            TryDivisor = 12;

        } else if (Divisor < 16) {
            TryDivisor = 16;

        } else {

            //
            // For AM devices, round up to the nearest supported fraction. Also
            // make sure the divisor doesn't exceed the maximum.
            //

            if (Device->ChipType <= FtdiTypeAm) {
                TryDivisor = KdFtdiAmAdjustUp[TryDivisor & 7];
                if (TryDivisor > FTDI_MAX_DIVISOR_AM) {
                    TryDivisor = FTDI_MAX_DIVISOR_AM;
                }

            } else {
                if (TryDivisor > FTDI_MAX_DIVISOR_BM) {
                    TryDivisor = FTDI_MAX_DIVISOR_BM;
                }
            }
        }

        //
        // Go back from the divisor to the baud rate to see how bad the error
        // is.
        //

        BaudRateEstimate = (FTDI_FUNDAMENTAL_CLOCK + (TryDivisor / 2)) /
                           TryDivisor;

        if (BaudRateEstimate < BaudRate) {
            BaudRateDifference = BaudRate - BaudRateEstimate;

        } else {
            BaudRateDifference = BaudRateEstimate - BaudRate;
        }

        if ((Try == 0) || (BaudRateDifference < BestBaudRateDifference)) {
            BestDivisor = TryDivisor;
            BestBaudRate = BaudRateEstimate;
            BestBaudRateDifference = BaudRateDifference;
            if (BaudRateDifference == 0) {
                break;
            }
        }
    }

    //
    // Encode the winning divisor.
    //

    EncodedDivisor = (BestDivisor >> 3) |
                     (KdFtdiFractionCode[BestDivisor & 0x7] << 14);

    //
    // Handle some special cases outlined in the FTDI spec. An encoded divisor
    // of 0 is 3000000 baud, and 1 is 2000000 baud.
    //

    if (EncodedDivisor == 1) {
        EncodedDivisor = 0;

    } else if (EncodedDivisor == 0x4001) {
        EncodedDivisor = 1;
    }

    //
    // Split the encoded divisor into index and value fields.
    //

    *Value = (USHORT)(EncodedDivisor & 0xFFFF);
    if (Device->ChipType == FtdiType2232C) {
        *Index = ((USHORT)(EncodedDivisor >> 8) & 0xFF00) | Device->Index;

    } else {
        *Index = (USHORT)(EncodedDivisor >> 16);
    }

    return BestBaudRate;
}

