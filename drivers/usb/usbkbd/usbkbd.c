/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    usbkbd.c

Abstract:

    This module implements support for the USB Keyboard driver.

Author:

    Evan Green 20-Mar-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "usbkbd.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Defines an error code reported to the system if the IN endpoint is halted
// and cannot be cleared.
//

#define USB_KBD_ERROR_IN_ENDPOINT_HALTED 0x00000001

//
// Define the report ID for setting LED state.
//

#define USB_KBD_SET_LED_REPORT_ID 0

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores context about a USB keyboard device.

Members:

    UsbCoreHandle - Stores the handle to the device as identified by the USB
        core library.

    InterfaceNumber - Stores the USB keyboard interface number that this
        driver instance is attached to.

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

    PreviousReport - Stores a pointer to the previous keyboard report.

--*/

typedef struct _USB_KEYBOARD_DEVICE {
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
    USB_KEYBOARD_REPORT PreviousReport;
} USB_KEYBOARD_DEVICE, *PUSB_KEYBOARD_DEVICE;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
UsbKbdAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
UsbKbdDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
UsbKbdDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
UsbKbdDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
UsbKbdDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
UsbKbdDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

KSTATUS
UsbKbdpStartDevice (
    PIRP Irp,
    PUSB_KEYBOARD_DEVICE Device
    );

KSTATUS
UsbKbdpSetUpUsbDevice (
    PIRP Irp,
    PUSB_KEYBOARD_DEVICE Device
    );

VOID
UsbKbdpRemoveDevice (
    PIRP Irp,
    PUSB_KEYBOARD_DEVICE Device
    );

VOID
UsbKbdpTransferCompletionCallback (
    PUSB_TRANSFER Transfer
    );

VOID
UsbKbdpProcessReport (
    PUSB_KEYBOARD_DEVICE Device,
    PUSB_KEYBOARD_REPORT Report
    );

KSTATUS
UsbKbdpSetLedState (
    PVOID Device,
    PVOID DeviceContext,
    ULONG LedState
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER UsbKbdDriver = NULL;

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

    This routine is the entry point for the USB keyboard driver. It registers
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

    UsbKbdDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = UsbKbdAddDevice;
    FunctionTable.DispatchStateChange = UsbKbdDispatchStateChange;
    FunctionTable.DispatchOpen = UsbKbdDispatchOpen;
    FunctionTable.DispatchClose = UsbKbdDispatchClose;
    FunctionTable.DispatchIo = UsbKbdDispatchIo;
    FunctionTable.DispatchSystemControl = UsbKbdDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
UsbKbdAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a device is detected for which the USB keyboard
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

    PUSB_KEYBOARD_DEVICE NewDevice;
    KSTATUS Status;

    //
    // Create the device context and attach to the device.
    //

    NewDevice = MmAllocatePagedPool(sizeof(USB_KEYBOARD_DEVICE),
                                    USB_KEYBOARD_ALLOCATION_TAG);

    if (NewDevice == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(NewDevice, sizeof(USB_KEYBOARD_DEVICE));
    NewDevice->UsbCoreHandle = INVALID_HANDLE;
    NewDevice->UserInputHandle = INVALID_HANDLE;

    //
    // Attempt to attach to the USB core.
    //

    Status = UsbDriverAttach(DeviceToken,
                             UsbKbdDriver,
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
UsbKbdDispatchStateChange (
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

    PUSB_KEYBOARD_DEVICE Device;
    KSTATUS Status;

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    Device = (PUSB_KEYBOARD_DEVICE)DeviceContext;
    switch (Irp->MinorCode) {
    case IrpMinorQueryResources:
        if (Irp->Direction == IrpUp) {
            IoCompleteIrp(UsbKbdDriver, Irp, STATUS_SUCCESS);
        }

        break;

    case IrpMinorStartDevice:

        //
        // Attempt to fire the thing up if the bus has already started it.
        //

        if (Irp->Direction == IrpUp) {
            Status = UsbKbdpStartDevice(Irp, Device);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(UsbKbdDriver, Irp, Status);
            }
        }

        break;

    case IrpMinorQueryChildren:
        IoCompleteIrp(UsbKbdDriver, Irp, STATUS_SUCCESS);
        break;

    case IrpMinorRemoveDevice:
        if (Irp->Direction == IrpUp) {
            UsbKbdpRemoveDevice(Irp, Device);
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
UsbKbdDispatchOpen (
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
UsbKbdDispatchClose (
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
UsbKbdDispatchIo (
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
UsbKbdDispatchSystemControl (
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
UsbKbdpStartDevice (
    PIRP Irp,
    PUSB_KEYBOARD_DEVICE Device
    )

/*++

Routine Description:

    This routine starts up the USB keyboard device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this USB keyboard device.

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

    Status = UsbKbdpSetUpUsbDevice(Irp, Device);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    //
    // Make sure that the device is in boot protocol mode. This driver does not
    // parse the report descriptor.
    //

    Setup.RequestType = USB_SETUP_REQUEST_TO_DEVICE |
                        USB_SETUP_REQUEST_CLASS |
                        USB_SETUP_REQUEST_INTERFACE_RECIPIENT;

    Setup.Request = USB_HID_SET_PROTOCOL;
    Setup.Value = USB_HID_PROTOCOL_VALUE_BOOT;
    Setup.Index = Device->InterfaceNumber;
    Setup.Length = 0;
    Status = UsbSendControlTransfer(Device->UsbCoreHandle,
                                    UsbTransferDirectionOut,
                                    &Setup,
                                    NULL,
                                    0,
                                    NULL);

    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    //
    // Send a Set Idle request so the device only completes the interrupt
    // transfers when something's changed.
    //

    Setup.RequestType = USB_SETUP_REQUEST_TO_DEVICE |
                        USB_SETUP_REQUEST_CLASS |
                        USB_SETUP_REQUEST_INTERFACE_RECIPIENT;

    Setup.Request = USB_HID_SET_IDLE;
    Setup.Value = 0;
    Setup.Index = Device->InterfaceNumber;
    Setup.Length = 0;
    Status = UsbSendControlTransfer(Device->UsbCoreHandle,
                                    UsbTransferDirectionOut,
                                    &Setup,
                                    NULL,
                                    0,
                                    NULL);

    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

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
        Description.Type = UserInputDeviceKeyboard;
        Description.InterfaceVersion =
                                  USER_INPUT_KEYBOARD_DEVICE_INTERFACE_VERSION;

        Description.U.KeyboardInterface.SetLedState = UsbKbdpSetLedState;
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
    Device->InTransfer->CallbackRoutine = UsbKbdpTransferCompletionCallback;
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
UsbKbdpSetUpUsbDevice (
    PIRP Irp,
    PUSB_KEYBOARD_DEVICE Device
    )

/*++

Routine Description:

    This routine claims the keyboard interface for the given device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this keyboard device.

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
    // Also ensure that the keyboard supports the boot protocol, as that's what
    // this driver assumes (as opposed to actually parsing out HID reports).
    //

    if ((Interface->Descriptor.Subclass != USB_HID_BOOT_INTERFACE_SUBCLASS) ||
        (Interface->Descriptor.Protocol != USB_HID_BOOT_KEYBOARD_PROTOCOL)) {

        RtlDebugPrint("The attached USB keyboard does not follow the boot "
                      "protocol, and as such is not currently supported.\n");

        Status = STATUS_NOT_SUPPORTED;
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

VOID
UsbKbdpRemoveDevice (
    PIRP Irp,
    PUSB_KEYBOARD_DEVICE Device
    )

/*++

Routine Description:

    This routine removes the USB keyboard device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this USB keyboard device.

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
    // become inactive, the USB keyboard's IN transfer can no longer be
    // running. It was either properly cancelled or failed to resubmit while
    // in the callback.
    //

    if (Device->InterfaceClaimed != FALSE) {
        UsbReleaseInterface(Device->UsbCoreHandle, Device->InterfaceNumber);
        Device->InterfaceClaimed = FALSE;
    }

    //
    // Destroy the IO buffer created during USB keyboard initialization. It was
    // used by the transfer as well.
    //

    if (Device->IoBuffer != NULL) {
        MmFreeIoBuffer(Device->IoBuffer);
    }

    //
    // Destroy the USB keyboard device's IN transfer.
    //

    if (Device->InTransfer != NULL) {
        UsbDestroyTransfer(Device->InTransfer);
    }

    //
    // Destroy the USB keyboard input device.
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
    MmFreePagedPool(Device);
    return;
}

VOID
UsbKbdpTransferCompletionCallback (
    PUSB_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine is called when a USB transfer completes for the keyboard.

Arguments:

    Transfer - Supplies a pointer to the transfer that completed.

Return Value:

    None.

--*/

{

    PUSB_KEYBOARD_DEVICE Device;
    PVOID DeviceToken;
    PUSB_KEYBOARD_REPORT Report;
    KSTATUS Status;

    Device = (PUSB_KEYBOARD_DEVICE)Transfer->UserData;

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
                                           UsbKbdDriver,
                                           Status,
                                           USB_KBD_ERROR_IN_ENDPOINT_HALTED);

                    goto TransferCompletionCallbackEnd;
                }
            }

        //
        // Otherwise just send out a debug print and carry on.
        //

        } else {
            RtlDebugPrint("USB KBD: Unexpected error for IN transfer (0x%08x) "
                          "on device 0x%08x: Status %d, Error %d.\n",
                          Transfer,
                          Device,
                          Transfer->Status,
                          Transfer->Error);
        }
    }

    //
    // Otherwise, process the data and re-submit the IN transfer for the USB
    // keyboard.
    //

    if (Transfer->LengthTransferred >= sizeof(USB_KEYBOARD_REPORT)) {
        Report = (PUSB_KEYBOARD_REPORT)Transfer->Buffer;
        UsbKbdpProcessReport(Device, Report);
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
UsbKbdpProcessReport (
    PUSB_KEYBOARD_DEVICE Device,
    PUSB_KEYBOARD_REPORT Report
    )

/*++

Routine Description:

    This routine processes a new USB keyboard input report.

Arguments:

    Device - Supplies a pointer to the keyboard device.

    Report - Supplies a pointer to the latest keyboard report.

Return Value:

    None.

--*/

{

    ULONG BitIndex;
    UCHAR ChangedModifiers;
    USER_INPUT_EVENT Event;
    UCHAR Key;
    ULONG KeyIndex;
    UCHAR Mask;
    PUSB_KEYBOARD_REPORT Previous;
    USB_KEYBOARD_REPORT PreviousCopy;
    ULONG SearchIndex;
    BOOL ValidData;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // First look for the all ones combination, which indicates that too many
    // keys are pressed.
    //

    ValidData = FALSE;
    for (KeyIndex = 0;
         KeyIndex < USB_KEYBOARD_REPORT_KEY_COUNT;
         KeyIndex += 1) {

        if (Report->Keycode[KeyIndex] != USB_KEYBOARD_INVALID_KEY_CODE) {
            ValidData = TRUE;
            break;
        }
    }

    if (ValidData == FALSE) {
        return;
    }

    RtlZeroMemory(&Event, sizeof(USER_INPUT_EVENT));
    Previous = &(Device->PreviousReport);
    RtlCopyMemory(&PreviousCopy, Previous, sizeof(USB_KEYBOARD_REPORT));

    //
    // Handle changes in the modifier keys.
    //

    ChangedModifiers = Previous->ModifierKeys ^ Report->ModifierKeys;
    for (BitIndex = 0; BitIndex < BITS_PER_BYTE; BitIndex += 1) {
        Mask = 1 << BitIndex;

        //
        // Send the change report if the modifier key changed.
        //

        if ((ChangedModifiers & Mask) != 0) {
            if ((Report->ModifierKeys & Mask) != 0) {
                Event.EventType = UserInputEventKeyDown;

            } else {
                Event.EventType = UserInputEventKeyUp;
            }

            Event.U.Key = UsbKbdControlKeys[BitIndex];
            InReportInputEvent(Device->UserInputHandle, &Event);
        }
    }

    //
    // Loop over every key down in the new report, and send key down reports if
    // it's not down in the previous report.
    //

    Event.EventType = UserInputEventKeyDown;
    for (KeyIndex = 0;
         KeyIndex < USB_KEYBOARD_REPORT_KEY_COUNT;
         KeyIndex += 1) {

        Key = Report->Keycode[KeyIndex];
        if ((Key >= USB_KEYBOARD_FIRST_VALID_KEY_CODE) &&
            (Key < USB_KEYCOARD_KEY_CODE_COUNT)) {

            //
            // Do a quick check in the same slot in the previous report, as
            // it's probably where the corresponding key is.
            //

            if (Previous->Keycode[KeyIndex] == Key) {
                PreviousCopy.Keycode[KeyIndex] = 0;
                continue;
            }

            //
            // Search for the keycode in the previous array.
            //

            for (SearchIndex = 0;
                 SearchIndex < USB_KEYBOARD_REPORT_KEY_COUNT;
                 SearchIndex += 1) {

                if (Previous->Keycode[SearchIndex] == Key) {
                    PreviousCopy.Keycode[SearchIndex] = 0;
                    break;
                }
            }

            //
            // If the key was found, then nothing's changed, so don't send a
            // report.
            //

            if (SearchIndex != USB_KEYBOARD_REPORT_KEY_COUNT) {
                continue;
            }

            //
            // This key just went down, so send a key down message.
            //

            Event.U.Key = UsbKbdKeys[Key];
            if (Event.U.Key != KeyboardKeyInvalid) {
                InReportInputEvent(Device->UserInputHandle, &Event);
            }
        }
    }

    //
    // Now go through the remaining key in the previous copy. Any of those
    // keys that haven't been blanked out by the last loop are keys that must
    // not have existed in the most recent report, meaning they're key up
    // events.
    //

    Event.EventType = UserInputEventKeyUp;
    for (KeyIndex = 0;
         KeyIndex < USB_KEYBOARD_REPORT_KEY_COUNT;
         KeyIndex += 1) {

        Key = PreviousCopy.Keycode[KeyIndex];
        if ((Key >= USB_KEYBOARD_FIRST_VALID_KEY_CODE) &&
            (Key < USB_KEYCOARD_KEY_CODE_COUNT)) {

            Event.U.Key = UsbKbdKeys[Key];
            if (Event.U.Key != KeyboardKeyInvalid) {
                InReportInputEvent(Device->UserInputHandle, &Event);
            }
        }
    }

    //
    // Copy the current report over the previous one.
    //

    RtlCopyMemory(Previous, Report, sizeof(USB_KEYBOARD_REPORT));
    return;
}

KSTATUS
UsbKbdpSetLedState (
    PVOID Device,
    PVOID DeviceContext,
    ULONG LedState
    )

/*++

Routine Description:

    This routine sets a keyboard's LED state (e.g. Number lock, Caps lock and
    scroll lock). The state is absolute; the desired state for each LED must be
    supplied.

Arguments:

    Device - Supplies a pointer to the OS device representing the user input
        device.

    DeviceContext - Supplies the opaque device context supplied in the device
        description upon registration with the user input library.

    LedState - Supplies a bitmask of flags describing the desired LED state.
        See USER_INPUT_KEYBOARD_LED_* for definition.

Return Value:

    Status code.

--*/

{

    ULONG AlignedMaxTransferSize;
    ULONG Alignment;
    PIO_BUFFER_FRAGMENT Fragment;
    PIO_BUFFER IoBuffer;
    ULONG IoBufferFlags;
    USB_SETUP_PACKET Setup;
    KSTATUS Status;
    PUSB_TRANSFER Transfer;
    PUSB_KEYBOARD_DEVICE UsbDevice;
    UCHAR UsbLedState;

    UsbDevice = (PUSB_KEYBOARD_DEVICE)DeviceContext;

    //
    // Convert from the user input library LED state to the USB keyboard LED
    // state.
    //

    UsbLedState = 0;
    if ((LedState & USER_INPUT_KEYBOARD_LED_SCROLL_LOCK) != 0) {
        UsbLedState |= USB_KEYBOARD_LED_SCROLL_LOCK;
    }

    if ((LedState & USER_INPUT_KEYBOARD_LED_NUM_LOCK) != 0) {
        UsbLedState |= USB_KEYBOARD_LED_NUM_LOCK;
    }

    if ((LedState & USER_INPUT_KEYBOARD_LED_CAPS_LOCK) != 0) {
        UsbLedState |= USB_KEYBOARD_LED_CAPS_LOCK;
    }

    if ((LedState & USER_INPUT_KEYBOARD_LED_COMPOSE) != 0) {
        UsbLedState |= USB_KEYBOARD_LED_COMPOSE;
    }

    if ((LedState & USER_INPUT_KEYBOARD_LED_KANA) != 0) {
        UsbLedState |= USB_KEYBOARD_LED_KANA;
    }

    //
    // If an out endpoint exists, then just send the raw LED state. That's what
    // it is expecting.
    //

    IoBuffer = NULL;
    Transfer = NULL;
    if (UsbDevice->OutEndpoint != 0) {

        ASSERT(UsbDevice->OutMaxTransferSize >= sizeof(UCHAR));

        Alignment = MmGetIoBufferAlignment();
        AlignedMaxTransferSize = ALIGN_RANGE_UP(UsbDevice->OutMaxTransferSize,
                                                Alignment);

        Transfer = UsbAllocateTransfer(UsbDevice->UsbCoreHandle,
                                       UsbDevice->OutEndpoint,
                                       UsbDevice->OutMaxTransferSize,
                                       0);

        if (Transfer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto SetLedStateEnd;
        }

        ASSERT(UsbDevice->IoBuffer == NULL);

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
            goto SetLedStateEnd;
        }

        ASSERT(IoBuffer->FragmentCount == 1);

        //
        // Wire up the USB transfer to use the I/O buffer.
        //

        Fragment = &(IoBuffer->Fragment[0]);
        Transfer->Buffer = Fragment->VirtualAddress;
        Transfer->BufferPhysicalAddress = Fragment->PhysicalAddress;
        Transfer->BufferActualLength = Fragment->Size;

        //
        // Prepare the data and send it synchronously.
        //

        *((PUCHAR)Transfer->Buffer) = UsbLedState;
        Transfer->Length = sizeof(UCHAR);
        Transfer->Direction = UsbTransferDirectionOut;
        Status = UsbSubmitSynchronousTransfer(Transfer);
        if (!KSUCCESS(Status)) {
            goto SetLedStateEnd;
        }

    //
    // Otherwise the LED state can be set using a "set report" control transfer.
    //

    } else {
        Setup.RequestType = USB_SETUP_REQUEST_TO_DEVICE |
                            USB_SETUP_REQUEST_CLASS |
                            USB_SETUP_REQUEST_INTERFACE_RECIPIENT;

        Setup.Request = USB_HID_SET_REPORT;
        Setup.Value = (USB_HID_REPORT_VALUE_TYPE_OUTPUT <<
                       USB_HID_REPORT_VALUE_TYPE_SHIFT) |
                      ((USB_KBD_SET_LED_REPORT_ID <<
                        USB_HID_REPORT_VALUE_ID_SHIFT) &
                       USB_HID_REPORT_VALUE_ID_MASK);

        Setup.Index = UsbDevice->InterfaceNumber;
        Setup.Length = sizeof(UCHAR);
        Status = UsbSendControlTransfer(UsbDevice->UsbCoreHandle,
                                        UsbTransferDirectionOut,
                                        &Setup,
                                        &UsbLedState,
                                        sizeof(UCHAR),
                                        NULL);

        if (!KSUCCESS(Status)) {
            goto SetLedStateEnd;
        }
    }

SetLedStateEnd:
    if (Transfer != NULL) {
        UsbDestroyTransfer(Transfer);
    }

    if (IoBuffer != NULL) {
        MmFreeIoBuffer(IoBuffer);
    }

    return Status;
}

