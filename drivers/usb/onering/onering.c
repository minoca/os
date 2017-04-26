/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    onering.c

Abstract:

    This module implements support for the USB LED and USB Relay devices
    created for demo purposes. These are extremely simple devices that either
    display a number on a seven-segment display (USB LED), or control up to
    five AC line voltage switches (USB Relay). The USB LED comes in two forms:
    the USB LED contains eight 7-segment digits, and the USB LED Mini is
    smaller but contains two rows of eight 7-segment digits. They communicate
    using only device-specific control transfers.

Author:

    Evan Green 15-Jul-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/devinfo/onering.h>
#include <minoca/usb/usb.h>

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the allocation tag used throughout the driver.
//

#define ONE_RING_ALLOCATION_TAG 0x52656E4F // 'CbsU'

//
// Define the device ID of the USB Relay.
//

#define ONE_RING_USB_RELAY_DEVICE_ID "VID_8619&PID_0650"

//
// Define the device ID of the USBLED displays.
//

#define ONE_RING_USB_LED_DEVICE_ID "VID_8619&PID_0651"
#define ONE_RING_USB_LED_MINI_DEVICE_ID "VID_8619&PID_0652"

//
// Define the maximum amount of space needed to represent the display:
// "8.8.8.8.8.8.8.8.\n8.8.8.8.8.8.8.8.".
//

#define ONE_RING_MAX_BUFFER 36

//
// Define control requests for the devices.
//

#define ONE_RING_USB_LED_COMMAND_WRITE 0x0
#define ONE_RING_USB_RELAY_COMMAND_SET 0x0

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores context about a One Ring Road device.

Members:

    UsbCoreHandle - Stores the handle to the device as identified by the USB
        core library.

    CreationTime - Stores the time the device was created.

    InterfaceClaimed - Stores a boolean indicating if the interface has been
        claimed for this driver or not.

    InformationPublished - Stores a boolean indicating if the device
        information for the device has been published.

    DeviceType - Stores the type of connected device.

    DeviceInformationUuid - Stores a pointer to the device's information UUID.

    SerialNumber - Stores a pointer to the device serial number.

    ReferenceCount - Stores the reference count on the device.

    CurrentValue - Stores the most recently written value of the display or
        USB relay.

--*/

typedef struct _ONE_RING_DEVICE {
    HANDLE UsbCoreHandle;
    SYSTEM_TIME CreationTime;
    BOOL InterfaceClaimed;
    BOOL InformationPublished;
    ONE_RING_DEVICE_TYPE DeviceType;
    PUUID DeviceInformationUuid;
    PSTR SerialNumber;
    ULONG ReferenceCount;
    CHAR CurrentValue[ONE_RING_MAX_BUFFER];
} ONE_RING_DEVICE, *PONE_RING_DEVICE;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
OneRingAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
OneRingDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
OneRingDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
OneRingDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
OneRingDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
OneRingDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

KSTATUS
OneRingpStartDevice (
    PIRP Irp,
    PONE_RING_DEVICE Device
    );

VOID
OneRingpRemoveDevice (
    PIRP Irp,
    PONE_RING_DEVICE Device
    );

VOID
OneRingpHandleDeviceInformationRequest (
    PIRP Irp,
    PONE_RING_DEVICE Device
    );

PSTR
OneRingpCreateAnsiStringFromStringDescriptor (
    PUSB_STRING_DESCRIPTOR StringDescriptor
    );

VOID
OneRingpReleaseDeviceReference (
    PONE_RING_DEVICE Device
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER OneRingDriver = NULL;

//
// Store instances of the information UUIDs.
//

UUID OneRingUsbRelayDeviceInformationUuid =
                                    ONE_RING_USB_RELAY_DEVICE_INFORMATION_UUID;

UUID OneRingUsbLedDeviceInformationUuid =
                                      ONE_RING_USB_LED_DEVICE_INFORMATION_UUID;

UUID OneRingUsbLedMiniDeviceInformationUuid =
                                 ONE_RING_USB_LED_MINI_DEVICE_INFORMATION_UUID;

//
// ------------------------------------------------------------------ Functions
//

__USED
KSTATUS
DriverEntry (
    PDRIVER Driver
    )

/*++

Routine Description:

    This routine is the entry point for the One Ring device driver. It
    registers the other dispatch functions, and performs driver-wide
    initialization.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    STATUS_SUCCESS on success.

    Failure code on error.

--*/

{

    DRIVER_FUNCTION_TABLE FunctionTable;
    KSTATUS Status;

    OneRingDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = OneRingAddDevice;
    FunctionTable.DispatchStateChange = OneRingDispatchStateChange;
    FunctionTable.DispatchOpen = OneRingDispatchOpen;
    FunctionTable.DispatchClose = OneRingDispatchClose;
    FunctionTable.DispatchIo = OneRingDispatchIo;
    FunctionTable.DispatchSystemControl = OneRingDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
OneRingAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a device is detected for which the USB compound
    device driver acts as the function driver. The driver will attach itself to
    the stack.

Arguments:

    Driver - Supplies a pointer to the driver being called.

    DeviceId - Supplies a pointer to a string with the device ID.

    ClassId - Supplies a pointer to a string containing the device's class ID.

    CompatibleIds - Supplies a pointer to a string containing device IDs
        that would be compatible with this device.

    DeviceToken - Supplies an opaque token that the driver can use to identify
        the device in the system. This token should be used when attaching to
        the stack.

Return Value:

    STATUS_SUCCESS on success.

    Failure code if the driver was unsuccessful in attaching itself.

--*/

{

    ONE_RING_DEVICE_TYPE DeviceType;
    PUUID InformationUuid;
    PONE_RING_DEVICE NewDevice;
    KSTATUS Status;

    NewDevice = NULL;
    DeviceType = OneRingDeviceInvalid;
    InformationUuid = NULL;
    if (IoAreDeviceIdsEqual(DeviceId, ONE_RING_USB_RELAY_DEVICE_ID) != FALSE) {
        DeviceType = OneRingDeviceUsbRelay;
        InformationUuid = &OneRingUsbRelayDeviceInformationUuid;

    } else if (IoAreDeviceIdsEqual(DeviceId, ONE_RING_USB_LED_DEVICE_ID) !=
               FALSE) {

        DeviceType = OneRingDeviceUsbLed;
        InformationUuid = &OneRingUsbLedDeviceInformationUuid;

    } else if (IoAreDeviceIdsEqual(DeviceId, ONE_RING_USB_LED_MINI_DEVICE_ID) !=
               FALSE) {

        DeviceType = OneRingDeviceUsbLedMini;
        InformationUuid = &OneRingUsbLedMiniDeviceInformationUuid;

    } else {
        Status = STATUS_INVALID_CONFIGURATION;
        goto AddDeviceEnd;
    }

    //
    // Create the device context and attach to the device.
    //

    NewDevice = MmAllocatePagedPool(sizeof(ONE_RING_DEVICE),
                                    ONE_RING_ALLOCATION_TAG);

    if (NewDevice == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(NewDevice, sizeof(ONE_RING_DEVICE));
    NewDevice->UsbCoreHandle = INVALID_HANDLE;
    NewDevice->DeviceType = DeviceType;
    NewDevice->DeviceInformationUuid = InformationUuid;
    NewDevice->ReferenceCount = 1;
    KeGetSystemTime(&(NewDevice->CreationTime));

    //
    // Attempt to attach to the USB core.
    //

    Status = UsbDriverAttach(DeviceToken,
                             OneRingDriver,
                             &(NewDevice->UsbCoreHandle));

    if (!KSUCCESS(Status)) {
        goto AddDeviceEnd;
    }

    ASSERT(NewDevice->UsbCoreHandle != INVALID_HANDLE);

    Status = IoAttachDriverToDevice(Driver, DeviceToken, NewDevice);

AddDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (NewDevice->UsbCoreHandle != INVALID_HANDLE) {
            UsbDeviceClose(NewDevice->UsbCoreHandle);
        }

        if (NewDevice != NULL) {
            MmFreePagedPool(NewDevice);
        }
    }

    return Status;
}

VOID
OneRingDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles State Change IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    PONE_RING_DEVICE Device;
    KSTATUS Status;

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    Device = (PONE_RING_DEVICE)DeviceContext;

    //
    // If this is the parent device, enumerate children.
    //

    if (Device != NULL) {
        switch (Irp->MinorCode) {
        case IrpMinorStartDevice:

            //
            // Attempt to fire the thing up if the bus has already started it.
            //

            if (Irp->Direction == IrpUp) {
                Status = OneRingpStartDevice(Irp, Device);
                IoCompleteIrp(OneRingDriver, Irp, Status);
            }

            break;

        case IrpMinorQueryChildren:
            if (Irp->Direction == IrpUp) {
                IoCompleteIrp(OneRingDriver, Irp, STATUS_SUCCESS);
            }

            break;

        case IrpMinorRemoveDevice:
            if (Irp->Direction == IrpUp) {
                OneRingpRemoveDevice(Irp, Device);
            }

            break;

        //
        // For all other IRPs, do nothing.
        //

        default:
            break;
        }
    }

    return;
}

VOID
OneRingDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles Open IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    PONE_RING_DEVICE Device;
    ULONG OldCount;
    KSTATUS Status;

    Device = DeviceContext;
    Status = STATUS_SUCCESS;
    if (Device->InterfaceClaimed == FALSE) {
        Status = STATUS_DEVICE_NOT_CONNECTED;

    } else {
        OldCount = RtlAtomicAdd32(&(Device->ReferenceCount), 1);

        ASSERT((OldCount != 0) && (OldCount < 0x10000000));
    }

    IoCompleteIrp(OneRingDriver, Irp, Status);
    return;
}

VOID
OneRingDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles Close IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    OneRingpReleaseDeviceReference(DeviceContext);
    IoCompleteIrp(OneRingDriver, Irp, STATUS_SUCCESS);
    return;
}

VOID
OneRingDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles I/O IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    ULONG DataLength;
    PONE_RING_DEVICE Device;
    UINTN Length;
    USB_SETUP_PACKET Setup;
    KSTATUS Status;

    ASSERT(Irp->Direction == IrpDown);

    Device = (PONE_RING_DEVICE)DeviceContext;
    Length = ONE_RING_MAX_BUFFER;
    if (Irp->U.ReadWrite.IoSizeInBytes == 0) {
        Status = STATUS_SUCCESS;
        goto DispatchIoEnd;
    }

    Setup.RequestType = USB_SETUP_REQUEST_DEVICE_RECIPIENT |
                        USB_SETUP_REQUEST_TO_DEVICE |
                        USB_SETUP_REQUEST_VENDOR;

    switch (Device->DeviceType) {

    //
    // Handle I/O to and from the USB LED displays.
    //

    case OneRingDeviceUsbLed:
    case OneRingDeviceUsbLedMini:
        Setup.Request = ONE_RING_USB_LED_COMMAND_WRITE;
        Length = ONE_RING_MAX_BUFFER;
        if (Irp->U.ReadWrite.IoSizeInBytes < Length) {
            Length = Irp->U.ReadWrite.IoSizeInBytes;
        }

        ASSERT(Length <= MAX_USHORT);

        Setup.Length = (USHORT)Length;
        break;

    //
    // Handle I/O to and from the USB relay board.
    //

    case OneRingDeviceUsbRelay:

        //
        // GAAAAHHHH OH NO YOU FOUND A PERFORMANCE BUG!!!!!!!!!!!!!
        // This is an unnecessary and hugely wasteful busy-spin, smack dab
        // in the I/O path. Had this been a real bug chewing up CPU time,
        // Minoca's real-time profiling tools would have pointed you straight
        // here, allowing you to quickly identify hot spots and keep your
        // system lean and mean.
        //

        HlBusySpin(10000);
        Setup.Request = ONE_RING_USB_RELAY_COMMAND_SET;
        Length = 1;
        Setup.Length = 0;
        break;

    default:
        Status = STATUS_INVALID_CONFIGURATION;
        goto DispatchIoEnd;
    }

    //
    // For reads, just return EOF.
    //

    if (Irp->MinorCode == IrpMinorIoRead) {
        Status = STATUS_END_OF_FILE;
        goto DispatchIoEnd;
    }

    ASSERT(Irp->MinorCode == IrpMinorIoWrite);

    RtlZeroMemory(Device->CurrentValue, ONE_RING_MAX_BUFFER);
    Status = MmCopyIoBufferData(Irp->U.ReadWrite.IoBuffer,
                                Device->CurrentValue,
                                0,
                                Length,
                                FALSE);

    if (!KSUCCESS(Status)) {
        goto DispatchIoEnd;
    }

    Setup.Value = 0;
    if (Device->DeviceType == OneRingDeviceUsbRelay) {
        Setup.Value = Device->CurrentValue[0];
    }

    //
    // Execute the USB control transfer on the device.
    //

    Status = UsbSendControlTransfer(Device->UsbCoreHandle,
                                    UsbTransferDirectionOut,
                                    &Setup,
                                    Device->CurrentValue,
                                    Setup.Length,
                                    &DataLength);

    if (!KSUCCESS(Status)) {
        goto DispatchIoEnd;
    }

    Irp->U.ReadWrite.IoBytesCompleted = Length;

DispatchIoEnd:
    IoCompleteIrp(OneRingDriver, Irp, Status);
    return;
}

VOID
OneRingDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles System Control IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    PVOID Context;
    PONE_RING_DEVICE Device;
    PSYSTEM_CONTROL_LOOKUP Lookup;
    PFILE_PROPERTIES Properties;
    KSTATUS Status;

    Device = (PONE_RING_DEVICE)DeviceContext;
    Context = Irp->U.SystemControl.SystemContext;
    switch (Irp->MinorCode) {
    case IrpMinorSystemControlLookup:
        Lookup = (PSYSTEM_CONTROL_LOOKUP)Context;
        Status = STATUS_PATH_NOT_FOUND;
        if (Lookup->Root != FALSE) {

            //
            // Enable opening of the root as a single file.
            //

            Properties = Lookup->Properties;
            Properties->FileId = 0;
            Properties->Type = IoObjectCharacterDevice;
            Properties->HardLinkCount = 1;
            Properties->BlockSize = 1;
            Properties->BlockCount = 0;
            Properties->StatusChangeTime = Device->CreationTime;
            Properties->ModifiedTime = Properties->StatusChangeTime;
            Properties->AccessTime = Properties->StatusChangeTime;
            Properties->Permissions = FILE_PERMISSION_ALL;
            Properties->Size = 0;
            Status = STATUS_SUCCESS;
        }

        IoCompleteIrp(OneRingDriver, Irp, Status);
        break;

    //
    // Succeed for the basics.
    //

    case IrpMinorSystemControlWriteFileProperties:
    case IrpMinorSystemControlTruncate:
        Status = STATUS_SUCCESS;
        IoCompleteIrp(OneRingDriver, Irp, Status);
        break;

    //
    // Handle get/set device information requests.
    //

    case IrpMinorSystemControlDeviceInformation:
        OneRingpHandleDeviceInformationRequest(Irp, Device);
        break;

    //
    // Ignore everything unrecognized.
    //

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

KSTATUS
OneRingpStartDevice (
    PIRP Irp,
    PONE_RING_DEVICE Device
    )

/*++

Routine Description:

    This routine starts up the USB compound device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this USB compound device.

Return Value:

    Status code.

--*/

{

    PUSB_CONFIGURATION_DESCRIPTION Configuration;
    USB_DEVICE_DESCRIPTOR DeviceDescriptor;
    ULONG LengthTransferred;
    USB_SETUP_PACKET Setup;
    KSTATUS Status;
    PUSB_STRING_DESCRIPTOR StringDescriptor;
    CHAR StringDescriptorBuffer[USB_MAX_DESCRIPTOR_SIZE];

    //
    // If the configuration isn't yet set, set the first one.
    //

    Configuration = UsbGetActiveConfiguration(Device->UsbCoreHandle);
    if (Configuration == NULL) {
        Status = UsbSetConfiguration(Device->UsbCoreHandle, 0, TRUE);
        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }

        Configuration = UsbGetActiveConfiguration(Device->UsbCoreHandle);

        ASSERT(Configuration != NULL);

    }

    if (Device->InterfaceClaimed == FALSE) {
        Status = UsbClaimInterface(Device->UsbCoreHandle, 0);
        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }

        Device->InterfaceClaimed = TRUE;
    }

    //
    // Get the device descriptor.
    //

    Setup.RequestType = USB_SETUP_REQUEST_TO_HOST |
                        USB_SETUP_REQUEST_STANDARD |
                        USB_SETUP_REQUEST_DEVICE_RECIPIENT;

    Setup.Request = USB_DEVICE_REQUEST_GET_DESCRIPTOR;
    Setup.Value = UsbDescriptorTypeDevice << 8;
    Setup.Index = 0;
    Setup.Length = sizeof(USB_DEVICE_DESCRIPTOR);
    Status = UsbSendControlTransfer(Device->UsbCoreHandle,
                                    UsbTransferDirectionIn,
                                    &Setup,
                                    &DeviceDescriptor,
                                    Setup.Length,
                                    &LengthTransferred);

    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    if (LengthTransferred != Setup.Length) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto StartDeviceEnd;
    }

    //
    // Try to read the serial number string.
    //

    if (DeviceDescriptor.SerialNumberStringIndex != 0) {
        StringDescriptor = (PUSB_STRING_DESCRIPTOR)StringDescriptorBuffer;
        Status = UsbReadDeviceString(Device->UsbCoreHandle,
                                     DeviceDescriptor.SerialNumberStringIndex,
                                     USB_LANGUAGE_ENGLISH_US,
                                     StringDescriptor);

        if (KSUCCESS(Status)) {
            if (Device->SerialNumber != NULL) {
                MmFreePagedPool(Device->SerialNumber);
            }

            Device->SerialNumber = OneRingpCreateAnsiStringFromStringDescriptor(
                                                              StringDescriptor);
        }
    }

    if ((Device->InformationPublished == FALSE) &&
        (Device->DeviceInformationUuid != NULL)) {

        Status = IoRegisterDeviceInformation(Irp->Device,
                                             Device->DeviceInformationUuid,
                                             TRUE);

        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }
    }

    Status = STATUS_SUCCESS;

StartDeviceEnd:
    return Status;
}

VOID
OneRingpRemoveDevice (
    PIRP Irp,
    PONE_RING_DEVICE Device
    )

/*++

Routine Description:

    This routine removes the One Ring Road device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this USB compound device.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    if (Device->InformationPublished != FALSE) {

        ASSERT(Device->DeviceInformationUuid != NULL);

        Status = IoRegisterDeviceInformation(Irp->Device,
                                             Device->DeviceInformationUuid,
                                             FALSE);

        ASSERT(KSUCCESS(Status));
    }

    UsbDetachDevice(Device->UsbCoreHandle);
    if (Device->InterfaceClaimed != FALSE) {
        UsbReleaseInterface(Device->UsbCoreHandle, 0);
        Device->InterfaceClaimed = FALSE;
    }

    UsbDeviceClose(Device->UsbCoreHandle);
    Device->UsbCoreHandle = INVALID_HANDLE;

    //
    // Release the original reference created by add device.
    //

    OneRingpReleaseDeviceReference(Device);
    return;
}

VOID
OneRingpHandleDeviceInformationRequest (
    PIRP Irp,
    PONE_RING_DEVICE Device
    )

/*++

Routine Description:

    This routine handles requests to get and set device information.

Arguments:

    Irp - Supplies a pointer to the IRP making the request.

    Device - Supplies a pointer to the device.

Return Value:

    None. Any completion status is set in the IRP.

--*/

{

    PONE_RING_DEVICE_INFORMATION Information;
    BOOL Match;
    PSYSTEM_CONTROL_DEVICE_INFORMATION Request;
    KSTATUS Status;

    Request = Irp->U.SystemControl.SystemContext;

    //
    // If this is not a request for the partition device information, ignore it.
    //

    Match = RtlAreUuidsEqual(&(Request->Uuid), Device->DeviceInformationUuid);
    if (Match == FALSE) {
        return;
    }

    //
    // Setting information is not supported.
    //

    if (Request->Set != FALSE) {
        Status = STATUS_ACCESS_DENIED;
        goto HandleDeviceInformationRequestEnd;
    }

    //
    // Make sure the size is large enough.
    //

    if (Request->DataSize < sizeof(ONE_RING_DEVICE_INFORMATION)) {
        Request->DataSize = sizeof(ONE_RING_DEVICE_INFORMATION);
        Status = STATUS_BUFFER_TOO_SMALL;
        goto HandleDeviceInformationRequestEnd;
    }

    Request->DataSize = sizeof(ONE_RING_DEVICE_INFORMATION);
    Information = Request->Data;
    RtlZeroMemory(Information, sizeof(ONE_RING_DEVICE_INFORMATION));
    Information->DeviceType = Device->DeviceType;
    if (Device->SerialNumber != NULL) {
        RtlStringCopy(Information->SerialNumber,
                      Device->SerialNumber,
                      ONE_RING_SERIAL_NUMBER_LENGTH);
    }

    Status = STATUS_SUCCESS;

HandleDeviceInformationRequestEnd:
    IoCompleteIrp(OneRingDriver, Irp, Status);
    return;
}

PSTR
OneRingpCreateAnsiStringFromStringDescriptor (
    PUSB_STRING_DESCRIPTOR StringDescriptor
    )

/*++

Routine Description:

    This routine converts a unicode string descriptor into an ANSI string.

Arguments:

    StringDescriptor - Supplies a pointer to the string descriptor to convert.

Return Value:

    Returns a pointer to the string on success. The caller is responsible for
    freeing this new string from paged pool.

    NULL on failure.

--*/

{

    ULONG Index;
    ULONG Length;
    PSTR NewString;
    PSTR UnicodeString;

    if ((StringDescriptor->Length < 2) ||
        ((StringDescriptor->Length & 0x1) != 0)) {

        return NULL;
    }

    Length = (StringDescriptor->Length / 2) - 1;
    NewString = MmAllocatePagedPool(Length + 1, ONE_RING_ALLOCATION_TAG);
    if (NewString == NULL) {
        return NULL;
    }

    Index = 0;
    UnicodeString = (PSTR)(StringDescriptor + 1);
    while (Index < Length) {
        NewString[Index] = *UnicodeString;
        Index += 1;
        UnicodeString += 2;
    }

    NewString[Index] = '\0';
    return NewString;
}

VOID
OneRingpReleaseDeviceReference (
    PONE_RING_DEVICE Device
    )

/*++

Routine Description:

    This routine releases a reference on the device structure, freeing it if
    it was the last one.

Arguments:

    Device - Supplies a pointer to the device structure.

Return Value:

    None. The device may be destroyed if the last reference was just released.

--*/

{

    ULONG OldCount;

    OldCount = RtlAtomicAdd32(&(Device->ReferenceCount), -1);

    ASSERT((OldCount != 0) && (OldCount < 0x10000000));

    if (OldCount == 1) {
        if (Device->SerialNumber != NULL) {
            MmFreePagedPool(Device->SerialNumber);
            Device->SerialNumber = NULL;
        }

        MmFreePagedPool(Device);
    }

    return;
}

