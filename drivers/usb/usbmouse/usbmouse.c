/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    usbmouse.c

Abstract:

    This module implements support for the USB Mouse driver.

Author:

    Evan Green 20-Mar-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/usb/usb.h>
#include <minoca/usb/usbhid.h>
#include <minoca/usrinput/usrinput.h>

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the allocation tag used throughout the USB keyboard driver: UsbM.
//

#define USB_MOUSE_ALLOCATION_TAG 0x4D627355

//
// Defines an error code reported to the system if the IN endpoint is halted
// and cannot be cleared.
//

#define USB_MOUSE_ERROR_IN_ENDPOINT_HALTED 0x00000001

//
// Define the mouse button bits.
//

#define USB_MOUSE_REPORT_LEFT_BUTTON 0x01
#define USB_MOUSE_REPORT_RIGHT_BUTTON 0x02
#define USB_MOUSE_REPORT_MIDDLE_BUTTON 0x04

#define USB_MOUSE_MAX_BUTTONS 5

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores context about a USB mouse device.

Members:

    UsbCoreHandle - Stores the handle to the device as identified by the USB
        core library.

    InterfaceNumber - Stores the USB mouse interface number that this driver
        instance is attached to.

    InterfaceClaimed - Stores a boolean indicating whether or not the interface
        is claimed.

    IoBuffer - Stores a pointer to the I/O buffer used for transfers.

    InEndpoint - Stores the endpoint number for the interrupt IN endpoint.

    InMaxTransferSize - Stores the maximum transfer size on the interrupt IN
        endpoint, in bytes.

    InTransfer - Stores a pointer to the interrupt IN transfer used to receive
        HID reports.

    OutEndpoint - Stores the endpointer number for the optional interrupt OUT
        endpoint, or 0 to indicate that the default control endpoint should be
        used for out transfers.

    OutMaxTransferSize - Stores the maximum transfer size on the interrupt OUT
        endpoint, in bytes.

    UserInputHandle - Stores the handle given back by the user input library.

    ButtonCount - Stores the number of buttons.

--*/

typedef struct _USB_MOUSE_DEVICE {
    HANDLE UsbCoreHandle;
    UCHAR InterfaceNumber;
    BOOL InterfaceClaimed;
    PIO_BUFFER IoBuffer;
    UCHAR InEndpoint;
    ULONG InMaxTransferSize;
    PUSB_TRANSFER InTransfer;
    UCHAR OutEndpoint;
    ULONG OutMaxTransferSize;
    HANDLE UserInputHandle;
    PUSB_HID_PARSER HidParser;
    PUSB_HID_ITEM MovementX;
    PUSB_HID_ITEM MovementY;
    PUSB_HID_ITEM Buttons[USB_MOUSE_MAX_BUTTONS];
    PUSB_HID_ITEM ScrollX;
    PUSB_HID_ITEM ScrollY;
    ULONG ButtonCount;
} USB_MOUSE_DEVICE, *PUSB_MOUSE_DEVICE;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
UsbMouseAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
UsbMouseDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
UsbMouseDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
UsbMouseDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
UsbMouseDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
UsbMouseDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

KSTATUS
UsbMousepStartDevice (
    PIRP Irp,
    PUSB_MOUSE_DEVICE Device
    );

KSTATUS
UsbMousepSetUpUsbDevice (
    PIRP Irp,
    PUSB_MOUSE_DEVICE Device
    );

KSTATUS
UsbMousepSetupHid (
    PIRP Irp,
    PUSB_MOUSE_DEVICE Device
    );

KSTATUS
UsbMousepReadReportDescriptor (
    PUSB_MOUSE_DEVICE Device,
    USHORT Length
    );

VOID
UsbMousepRemoveDevice (
    PIRP Irp,
    PUSB_MOUSE_DEVICE Device
    );

VOID
UsbMousepTransferCompletionCallback (
    PUSB_TRANSFER Transfer
    );

VOID
UsbMousepProcessReport (
    PUSB_MOUSE_DEVICE Device,
    PUCHAR Report,
    ULONG Length
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER UsbMouseDriver = NULL;

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

    This routine is the entry point for the USB mouse driver. It registers
    the other dispatch functions, and performs driver-wide initialization.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    STATUS_SUCCESS on success.

    Failure code on error.

--*/

{

    DRIVER_FUNCTION_TABLE FunctionTable;
    KSTATUS Status;

    UsbMouseDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = UsbMouseAddDevice;
    FunctionTable.DispatchStateChange = UsbMouseDispatchStateChange;
    FunctionTable.DispatchOpen = UsbMouseDispatchOpen;
    FunctionTable.DispatchClose = UsbMouseDispatchClose;
    FunctionTable.DispatchIo = UsbMouseDispatchIo;
    FunctionTable.DispatchSystemControl = UsbMouseDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
UsbMouseAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a device is detected for which the USB mouse
    driver acts as the function driver. The driver will attach itself to the
    stack.

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

    PUSB_MOUSE_DEVICE NewDevice;
    KSTATUS Status;

    //
    // Create the device context and attach to the device.
    //

    NewDevice = MmAllocatePagedPool(sizeof(USB_MOUSE_DEVICE),
                                    USB_MOUSE_ALLOCATION_TAG);

    if (NewDevice == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(NewDevice, sizeof(USB_MOUSE_DEVICE));
    NewDevice->UsbCoreHandle = INVALID_HANDLE;
    NewDevice->UserInputHandle = INVALID_HANDLE;

    //
    // Attempt to attach to the USB core.
    //

    Status = UsbDriverAttach(DeviceToken,
                             UsbMouseDriver,
                             &(NewDevice->UsbCoreHandle));

    if (!KSUCCESS(Status)) {
        goto AddDeviceEnd;
    }

    ASSERT(NewDevice->UsbCoreHandle != INVALID_HANDLE);

    Status = IoAttachDriverToDevice(Driver, DeviceToken, NewDevice);

AddDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (NewDevice != NULL) {
            if (NewDevice->UsbCoreHandle != INVALID_HANDLE) {
                UsbDeviceClose(NewDevice->UsbCoreHandle);
            }

            MmFreePagedPool(NewDevice);
        }
    }

    return Status;
}

VOID
UsbMouseDispatchStateChange (
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

    PUSB_MOUSE_DEVICE Device;
    KSTATUS Status;

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    Device = (PUSB_MOUSE_DEVICE)DeviceContext;
    switch (Irp->MinorCode) {
    case IrpMinorQueryResources:
        if (Irp->Direction == IrpUp) {
            IoCompleteIrp(UsbMouseDriver, Irp, STATUS_SUCCESS);
        }

        break;

    case IrpMinorStartDevice:

        //
        // Attempt to fire the thing up if the bus has already started it.
        //

        if (Irp->Direction == IrpUp) {
            Status = UsbMousepStartDevice(Irp, Device);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(UsbMouseDriver, Irp, Status);
            }
        }

        break;

    case IrpMinorQueryChildren:
        IoCompleteIrp(UsbMouseDriver, Irp, STATUS_SUCCESS);
        break;

    case IrpMinorRemoveDevice:
        if (Irp->Direction == IrpUp) {
            UsbMousepRemoveDevice(Irp, Device);
        }

        break;

    //
    // For all other IRPs, do nothing.
    //

    default:
        break;
    }

    return;
}

VOID
UsbMouseDispatchOpen (
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

    return;
}

VOID
UsbMouseDispatchClose (
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

    return;
}

VOID
UsbMouseDispatchIo (
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

    return;
}

VOID
UsbMouseDispatchSystemControl (
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

    ASSERT(Irp->MajorCode == IrpMajorSystemControl);

    //
    // Do no processing on any IRPs. Let them flow.
    //

    return;
}

KSTATUS
UsbMousepStartDevice (
    PIRP Irp,
    PUSB_MOUSE_DEVICE Device
    )

/*++

Routine Description:

    This routine starts up the USB mouse device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this USB mouse device.

Return Value:

    Status code.

--*/

{

    ULONG AlignedMaxTransferSize;
    ULONG Alignment;
    USER_INPUT_DEVICE_DESCRIPTION Description;
    PIO_BUFFER_FRAGMENT Fragment;
    PIO_BUFFER IoBuffer;
    ULONG IoBufferFlags;
    USB_SETUP_PACKET Setup;
    KSTATUS Status;
    HANDLE UserInputHandle;

    //
    // Claim the interface.
    //

    Status = UsbMousepSetUpUsbDevice(Irp, Device);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    Status = UsbMousepSetupHid(Irp, Device);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    //
    // Send a Set Idle request so the device only completes the interrupt
    // transfers when something's changed. Our awful Azza mouse fails this
    // request, but it's not really a big deal.
    //

    Setup.RequestType = USB_SETUP_REQUEST_TO_DEVICE |
                        USB_SETUP_REQUEST_CLASS |
                        USB_SETUP_REQUEST_INTERFACE_RECIPIENT;

    Setup.Request = USB_HID_SET_IDLE;
    Setup.Value = 0;
    Setup.Index = Device->InterfaceNumber;
    Setup.Length = 0;
    UsbSendControlTransfer(Device->UsbCoreHandle,
                           UsbTransferDirectionOut,
                           &Setup,
                           NULL,
                           0,
                           NULL);

    //
    // Allocate an IN transfer if needed.
    //

    ASSERT(Device->InMaxTransferSize != 0);

    if (Device->InTransfer == NULL) {
        Alignment = MmGetIoBufferAlignment();
        AlignedMaxTransferSize = ALIGN_RANGE_UP(Device->InMaxTransferSize,
                                                Alignment);

        Device->InTransfer = UsbAllocateTransfer(Device->UsbCoreHandle,
                                                 Device->InEndpoint,
                                                 Device->InMaxTransferSize,
                                                 0);

        if (Device->InTransfer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto StartDeviceEnd;
        }

        ASSERT(Device->IoBuffer == NULL);

        //
        // Allocate an I/O buffer.
        //

        IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS;
        IoBuffer = MmAllocateNonPagedIoBuffer(0,
                                              MAX_ULONG,
                                              Alignment,
                                              AlignedMaxTransferSize,
                                              IoBufferFlags);

        if (IoBuffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto StartDeviceEnd;
        }

        ASSERT(IoBuffer->FragmentCount == 1);

        //
        // Wire up the USB transfer to use the I/O buffer.
        //

        Fragment = &(IoBuffer->Fragment[0]);
        Device->InTransfer->Buffer = Fragment->VirtualAddress;
        Device->InTransfer->BufferPhysicalAddress = Fragment->PhysicalAddress;
        Device->InTransfer->BufferActualLength = Fragment->Size;
        Device->IoBuffer = IoBuffer;
    }

    //
    // Create the user input device if needed.
    //

    if (Device->UserInputHandle == INVALID_HANDLE) {
        RtlZeroMemory(&Description, sizeof(USER_INPUT_DEVICE_DESCRIPTION));
        Description.Device = Irp->Device;
        Description.DeviceContext = Device;
        Description.Type = UserInputDeviceMouse;
        UserInputHandle = InRegisterInputDevice(&Description);
        if (UserInputHandle == INVALID_HANDLE) {
            Status = STATUS_INVALID_HANDLE;
            goto StartDeviceEnd;
        }

        Device->UserInputHandle = UserInputHandle;
    }

    //
    // Submit the interrupt in transfer to start polling for reports.
    //

    Device->InTransfer->Direction = UsbTransferDirectionIn;
    Device->InTransfer->Length = Device->InMaxTransferSize;
    Device->InTransfer->UserData = Device;
    Device->InTransfer->CallbackRoutine = UsbMousepTransferCompletionCallback;
    Status = UsbSubmitTransfer(Device->InTransfer);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    Status = STATUS_SUCCESS;

StartDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Device->InTransfer != NULL) {
            UsbDestroyTransfer(Device->InTransfer);
            Device->InTransfer = NULL;
            if (Device->IoBuffer != NULL) {
                MmFreeIoBuffer(Device->IoBuffer);
                Device->IoBuffer = NULL;
            }
        }

        if (Device->UserInputHandle != INVALID_HANDLE) {
            InDestroyInputDevice(Device->UserInputHandle);
            Device->UserInputHandle = INVALID_HANDLE;
        }

        ASSERT(Device->IoBuffer == NULL);
    }

    return Status;
}

KSTATUS
UsbMousepSetUpUsbDevice (
    PIRP Irp,
    PUSB_MOUSE_DEVICE Device
    )

/*++

Routine Description:

    This routine claims the mouse interface for the given device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this mouse device.

Return Value:

    Status code.

--*/

{

    PUSB_CONFIGURATION_DESCRIPTION Configuration;
    PLIST_ENTRY CurrentEntry;
    USB_TRANSFER_DIRECTION Direction;
    PUSB_ENDPOINT_DESCRIPTION Endpoint;
    UCHAR EndpointType;
    BOOL InEndpointFound;
    PUSB_INTERFACE_DESCRIPTION Interface;
    BOOL OutEndpointFound;
    KSTATUS Status;

    //
    // Sanity check that the interface has not already been claimed.
    //

    if (Device->InterfaceClaimed != FALSE) {
        Status = STATUS_SUCCESS;
        goto SetUpUsbDeviceEnd;
    }

    //
    // If the configuration isn't yet set, set the first one.
    //

    Configuration = UsbGetActiveConfiguration(Device->UsbCoreHandle);
    if (Configuration == NULL) {
        Status = UsbSetConfiguration(Device->UsbCoreHandle, 0, TRUE);
        if (!KSUCCESS(Status)) {
            goto SetUpUsbDeviceEnd;
        }

        Configuration = UsbGetActiveConfiguration(Device->UsbCoreHandle);

        ASSERT(Configuration != NULL);

    }

    //
    // Get and verify the interface.
    //

    Interface = UsbGetDesignatedInterface(Irp->Device, Device->UsbCoreHandle);
    if (Interface == NULL) {
        Status = STATUS_NO_INTERFACE;
        goto SetUpUsbDeviceEnd;
    }

    if (Interface->Descriptor.Class != UsbInterfaceClassHid) {
        Status = STATUS_NO_INTERFACE;
        goto SetUpUsbDeviceEnd;
    }

    //
    // Locate the IN and OUT endpoints.
    //

    InEndpointFound = FALSE;
    OutEndpointFound = FALSE;
    CurrentEntry = Interface->EndpointListHead.Next;
    while (CurrentEntry != &(Interface->EndpointListHead)) {
        Endpoint = LIST_VALUE(CurrentEntry,
                              USB_ENDPOINT_DESCRIPTION,
                              ListEntry);

        CurrentEntry = CurrentEntry->Next;

        //
        // Deconstruct the components of the endpoint descriptor.
        //

        EndpointType = Endpoint->Descriptor.Attributes &
                       USB_ENDPOINT_ATTRIBUTES_TYPE_MASK;

        if ((Endpoint->Descriptor.EndpointAddress &
             USB_ENDPOINT_ADDRESS_DIRECTION_IN) != 0) {

            Direction = UsbTransferDirectionIn;

        } else {
            Direction = UsbTransferDirectionOut;
        }

        //
        // Look to match the endpoint up to one of the required ones.
        //

        if (EndpointType == USB_ENDPOINT_ATTRIBUTES_TYPE_INTERRUPT) {
            if ((InEndpointFound == FALSE) &&
                (Direction == UsbTransferDirectionIn)) {

                InEndpointFound = TRUE;
                Device->InEndpoint = Endpoint->Descriptor.EndpointAddress;
                Device->InMaxTransferSize = Endpoint->Descriptor.MaxPacketSize;

            } else if ((OutEndpointFound == FALSE) &&
                       (Direction == UsbTransferDirectionOut)) {

                OutEndpointFound = TRUE;
                Device->OutEndpoint = Endpoint->Descriptor.EndpointAddress;
                Device->OutMaxTransferSize = Endpoint->Descriptor.MaxPacketSize;
            }
        }

        if ((InEndpointFound != FALSE) && (OutEndpointFound != FALSE)) {
            break;
        }
    }

    //
    // The IN endpoint is required, the OUT is not.
    //

    if (InEndpointFound == FALSE) {
        Status = STATUS_INVALID_CONFIGURATION;
        goto SetUpUsbDeviceEnd;
    }

    //
    // Everything's all ready, claim the interface.
    //

    Status = UsbClaimInterface(Device->UsbCoreHandle,
                               Interface->Descriptor.InterfaceNumber);

    if (!KSUCCESS(Status)) {
        goto SetUpUsbDeviceEnd;
    }

    Device->InterfaceNumber = Interface->Descriptor.InterfaceNumber;
    Device->InterfaceClaimed = TRUE;
    Status = STATUS_SUCCESS;

SetUpUsbDeviceEnd:
    return Status;
}

KSTATUS
UsbMousepSetupHid (
    PIRP Irp,
    PUSB_MOUSE_DEVICE Device
    )

/*++

Routine Description:

    This routine reads in the HID descriptor and stashes away important data
    items.

Arguments:

    Irp - Supplies a pointer to the IRP.

    Device - Supplies a pointer to this USB mouse device.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PUSB_UNKNOWN_DESCRIPTION Description;
    PUSB_HID_DESCRIPTOR_REPORT DescriptorReport;
    ULONG DescriptorReportIndex;
    PUSB_HID_DESCRIPTOR_REPORT End;
    PUSB_HID_DESCRIPTOR HidDescriptor;
    PUSB_INTERFACE_DESCRIPTION Interface;
    KSTATUS Status;

    if (Device->HidParser == NULL) {
        Device->HidParser = UsbhidCreateParser();
        if (Device->HidParser == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    Interface = UsbGetDesignatedInterface(Irp->Device, Device->UsbCoreHandle);

    ASSERT((Interface != NULL) && (Device->InterfaceClaimed != FALSE));

    CurrentEntry = Interface->UnknownListHead.Next;
    while (CurrentEntry != &(Interface->UnknownListHead)) {
        Description = LIST_VALUE(CurrentEntry,
                                 USB_UNKNOWN_DESCRIPTION,
                                 ListEntry);

        CurrentEntry = CurrentEntry->Next;
        HidDescriptor = (PUSB_HID_DESCRIPTOR)(Description->Descriptor);
        if ((HidDescriptor->DescriptorType != UsbDescriptorTypeHid) ||
            (HidDescriptor->Length < sizeof(USB_HID_DESCRIPTOR))) {

            continue;
        }

        End = (PUSB_HID_DESCRIPTOR_REPORT)((PUCHAR)HidDescriptor +
                                           HidDescriptor->Length);

        DescriptorReport = &(HidDescriptor->Descriptors[0]);
        DescriptorReportIndex = 0;
        while (((DescriptorReport + 1) <= End) &&
               (DescriptorReportIndex < HidDescriptor->DescriptorCount)) {

            if (DescriptorReport->Type == UsbDescriptorTypeHidReport) {
                Status = UsbMousepReadReportDescriptor(
                                                     Device,
                                                     DescriptorReport->Length);

                if (!KSUCCESS(Status)) {
                    RtlDebugPrint("USBMouse: Failed to parse HID report\n");
                    goto SetupHidEnd;
                }

                goto SetupHidEnd;
            }

            DescriptorReportIndex += 1;
            DescriptorReport += 1;
        }
    }

    Status = STATUS_INVALID_CONFIGURATION;

SetupHidEnd:
    if (KSUCCESS(Status)) {
        if ((Device->MovementX == NULL) || (Device->MovementY == NULL) ||
            (Device->Buttons[0] == NULL)) {

            RtlDebugPrint("USBMouse: Failed to get required HID items.\n");
            Status = STATUS_INVALID_CONFIGURATION;
        }
    }

    return Status;
}

KSTATUS
UsbMousepReadReportDescriptor (
    PUSB_MOUSE_DEVICE Device,
    USHORT Length
    )

/*++

Routine Description:

    This routine reads in the HID report descriptor and loads it into the HID
    parser.

Arguments:

    Device - Supplies a pointer to this USB mouse device.

    Length - Supplies the length of the report descriptor.

Return Value:

    None.

--*/

{

    ULONG LengthTransferred;
    PUCHAR Report;
    USB_SETUP_PACKET Setup;
    KSTATUS Status;
    USB_HID_USAGE Usage;

    Report = MmAllocateNonPagedPool(Length, USB_MOUSE_ALLOCATION_TAG);
    if (Report == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ReadReportDescriptorEnd;
    }

    RtlZeroMemory(&Setup, sizeof(USB_SETUP_PACKET));
    Setup.RequestType = USB_SETUP_REQUEST_TO_HOST |
                        USB_SETUP_REQUEST_STANDARD |
                        USB_SETUP_REQUEST_INTERFACE_RECIPIENT;

    Setup.Request = USB_DEVICE_REQUEST_GET_DESCRIPTOR;
    Setup.Value = UsbDescriptorTypeHidReport << 8;
    Setup.Index = 0;
    Setup.Length = Length;
    Status = UsbSendControlTransfer(Device->UsbCoreHandle,
                                    UsbTransferDirectionIn,
                                    &Setup,
                                    Report,
                                    Setup.Length,
                                    &LengthTransferred);

    if (!KSUCCESS(Status)) {
        goto ReadReportDescriptorEnd;
    }

    if (LengthTransferred != Setup.Length) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto ReadReportDescriptorEnd;
    }

    Status = UsbhidParseReportDescriptor(Device->HidParser, Report, Length);
    if (!KSUCCESS(Status)) {
        goto ReadReportDescriptorEnd;
    }

    //
    // Go find all the buttons.
    //

    Usage.Page = HidPageButton;
    while (Device->ButtonCount < USB_MOUSE_MAX_BUTTONS) {
        Usage.Value = Device->ButtonCount + 1;
        Device->Buttons[Device->ButtonCount] = UsbhidFindItem(Device->HidParser,
                                                              0,
                                                              UsbhidDataInput,
                                                              &Usage,
                                                              NULL);

        if (Device->Buttons[Device->ButtonCount] == NULL) {
            break;
        }

        Device->ButtonCount += 1;
    }

    Usage.Page = HidPageGenericDesktop;
    Usage.Value = HidDesktopX;
    Device->MovementX = UsbhidFindItem(Device->HidParser,
                                       0,
                                       UsbhidDataInput,
                                       &Usage,
                                       NULL);

    Usage.Value = HidDesktopY;
    Device->MovementY = UsbhidFindItem(Device->HidParser,
                                       0,
                                       UsbhidDataInput,
                                       &Usage,
                                       NULL);

    Usage.Value = HidDesktopWheel;
    Device->ScrollY = UsbhidFindItem(Device->HidParser,
                                     0,
                                     UsbhidDataInput,
                                     &Usage,
                                     NULL);

    Usage.Page = HidPageConsumer;
    Usage.Value = HidConsumerAcPan;
    Device->ScrollX = UsbhidFindItem(Device->HidParser,
                                     0,
                                     UsbhidDataInput,
                                     &Usage,
                                     NULL);

    Status = STATUS_SUCCESS;

ReadReportDescriptorEnd:
    if (Report != NULL) {
        MmFreeNonPagedPool(Report);
    }

    return Status;
}

VOID
UsbMousepRemoveDevice (
    PIRP Irp,
    PUSB_MOUSE_DEVICE Device
    )

/*++

Routine Description:

    This routine removes the USB mouse device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this USB mouse device.

Return Value:

    None.

--*/

{

    ASSERT(Irp->MinorCode == IrpMinorRemoveDevice);

    //
    // Detach the device from the USB core. This call marks the device as
    // disconnected and cancels all transfers.
    //

    UsbDetachDevice(Device->UsbCoreHandle);

    //
    // Now destroy the device. Since the above call waits for all transfers to
    // become inactive, the USB mouse's IN transfer can no longer be
    // running. It was either properly cancelled or failed to resubmit while
    // in the callback.
    //

    if (Device->InterfaceClaimed != FALSE) {
        UsbReleaseInterface(Device->UsbCoreHandle, Device->InterfaceNumber);
        Device->InterfaceClaimed = FALSE;
    }

    //
    // Destroy the IO buffer created during USB mouse initialization. It was
    // used by the transfer as well.
    //

    if (Device->IoBuffer != NULL) {
        MmFreeIoBuffer(Device->IoBuffer);
    }

    //
    // Destroy the USB mouse device's IN transfer.
    //

    if (Device->InTransfer != NULL) {
        UsbDestroyTransfer(Device->InTransfer);
    }

    //
    // Destroy the USB mouse input device.
    //

    if (Device->UserInputHandle != INVALID_HANDLE) {
        InDestroyInputDevice(Device->UserInputHandle);
    }

    //
    // Close the USB core handle, matching the open from when the driver
    // attached to the device.
    //

    ASSERT(Device->UsbCoreHandle != INVALID_HANDLE);

    UsbDeviceClose(Device->UsbCoreHandle);

    //
    // Destroy the HID parser.
    //

    if (Device->HidParser != NULL) {
        UsbhidDestroyParser(Device->HidParser);
    }

    MmFreePagedPool(Device);
    return;
}

VOID
UsbMousepTransferCompletionCallback (
    PUSB_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine is called when a USB transfer completes for the mouse.

Arguments:

    Transfer - Supplies a pointer to the transfer that completed.

Return Value:

    None.

--*/

{

    PUSB_MOUSE_DEVICE Device;
    PVOID DeviceToken;
    KSTATUS Status;

    Device = (PUSB_MOUSE_DEVICE)Transfer->UserData;

    ASSERT(Device != NULL);
    ASSERT(Transfer == Device->InTransfer);
    ASSERT(Transfer->Direction == UsbTransferDirectionIn);

    //
    // Handle transfer errors to determine whether or not to resubmit.
    //

    if (!KSUCCESS(Transfer->Status)) {

        //
        // Do not resubmit the transfer if it was cancelled.
        //

        if (Transfer->Status == STATUS_OPERATION_CANCELLED) {

            ASSERT(Transfer->Error == UsbErrorTransferCancelled);

            goto TransferCompletionCallbackEnd;

        //
        // If there was an I/O error, perform any steps to clear the error.
        //

        } else if (Transfer->Status == STATUS_DEVICE_IO_ERROR) {
            if (Transfer->Error == UsbErrorTransferStalled) {
                Status = UsbClearFeature(Device->UsbCoreHandle,
                                         USB_SETUP_REQUEST_ENDPOINT_RECIPIENT,
                                         USB_FEATURE_ENDPOINT_HALT,
                                         Device->InEndpoint);

                if (!KSUCCESS(Status)) {
                    DeviceToken = UsbGetDeviceToken(Device->UsbCoreHandle);
                    IoSetDeviceDriverError(DeviceToken,
                                           UsbMouseDriver,
                                           Status,
                                           USB_MOUSE_ERROR_IN_ENDPOINT_HALTED);

                    goto TransferCompletionCallbackEnd;
                }
            }

        //
        // Otherwise just send out a debug print and carry on.
        //

        } else {
            RtlDebugPrint("USBMouse: Unexpected error for IN transfer (0x%08x) "
                          "on device 0x%08x: Status %d, Error %d.\n",
                          Transfer,
                          Device,
                          Transfer->Status,
                          Transfer->Error);
        }

    } else {

        //
        // Otherwise, process the data and re-submit the IN transfer for the
        // USB mouse.
        //

        UsbMousepProcessReport(Device,
                               Transfer->Buffer,
                               Transfer->LengthTransferred);
    }

    //
    // If submission fails, exit.
    //

    Status = UsbSubmitTransfer(Device->InTransfer);
    if (!KSUCCESS(Status)) {
        goto TransferCompletionCallbackEnd;
    }

TransferCompletionCallbackEnd:
    return;
}

VOID
UsbMousepProcessReport (
    PUSB_MOUSE_DEVICE Device,
    PUCHAR Report,
    ULONG Length
    )

/*++

Routine Description:

    This routine processes a new USB mouse input report.

Arguments:

    Device - Supplies a pointer to the mouse device.

    Report - Supplies a pointer to the mouse report.

    Length - Supplies the number of bytes in the report.

Return Value:

    None.

--*/

{

    ULONG Button;
    ULONG ButtonCount;
    USER_INPUT_EVENT Event;

    RtlZeroMemory(&Event, sizeof(USER_INPUT_EVENT));
    Event.EventType = UserInputEventMouse;
    UsbhidReadReport(Device->HidParser, Report, Length);
    ButtonCount = Device->ButtonCount;
    for (Button = 0; Button < ButtonCount; Button += 1) {
        if (Device->Buttons[Button]->Value != 0) {
            Event.U.Mouse.Buttons |= 1 << Button;
        }
    }

    Event.U.Mouse.MovementX = Device->MovementX->Value;
    Event.U.Mouse.MovementY = Device->MovementY->Value;
    if (Device->ScrollY != NULL) {
        Event.U.Mouse.ScrollY = Device->ScrollY->Value;
    }

    if (Device->ScrollX != NULL) {
        Event.U.Mouse.ScrollX = Device->ScrollX->Value;
    }

    InReportInputEvent(Device->UserInputHandle, &Event);
    return;
}

