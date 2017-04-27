/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    kdusb.c

Abstract:

    This module implements support for USB Host based kernel debugger
    transports.

Author:

    Evan Green 26-Mar-2014

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

//
// Define the maximum number of ports supported in a hub by KD USB.
//

#define KD_USB_MAX_PORT_COUNT 64

//
// Define the maximum size of a configuration descriptor and friends.
//

#define KD_USB_CONFIGURATION_LENGTH 0xFF

#define USB_MAX_DEVICE_ADDRESS 0x7F

#define KD_TEST_WELCOME_STRING                                              \
    "Minoca KD Interface Test. Type 'exit' to leave.\r\n"

#define KD_TEST_GOODBYE_STRING "\r\nAdios!\r\n"
#define KD_TEST_EXIT_STRING "exit"
#define KD_TEST_RECEIVE_BUFFER_SIZE 16

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
KdpTestInterface (
    PDEBUG_DEVICE_DESCRIPTION Interface
    );

KSTATUS
KdpUsbResetPort (
    PKD_USB_DEVICE Device
    );

KSTATUS
KdpUsbEnumerateDevice (
    PKD_USB_DEVICE Device
    );

KSTATUS
KdpUsbGetRootHubStatus (
    PHARDWARE_USB_DEBUG_DEVICE Device,
    ULONG PortIndex,
    PULONG PortStatus
    );

KSTATUS
KdpUsbSetRootHubStatus (
    PHARDWARE_USB_DEBUG_DEVICE Device,
    ULONG PortIndex,
    ULONG PortStatus
    );

PKD_USB_DRIVER_MAPPING
KdpIsDeviceSupported (
    PKD_USB_DEVICE Device
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a boolean indicating whether to debug KD USB. Note that this variable
// cannot be set while KD USB is the active transport!
//

BOOL KdUsbDebug = FALSE;
BOOL KdUsbDebugAllTransfers = FALSE;

//
// Store the device path to the debug device.
//

KD_USB_DEVICE KdUsbDevicePath[DEBUG_USB_DEVICE_PATH_SIZE];
ULONG KdUsbDevicePathSize;

//
// Store the next free device address. Don't use zero!
//

ULONG KdUsbNextDeviceAddress = 1;

//
// Store a pointer to the final interface the USB device driver surfaced.
//

DEBUG_DEVICE_DESCRIPTION KdUsbDeviceInterface;

//
// Store the device
//

HARDWARE_USB_DEBUG_DEVICE KdUsbDebugHost;

//
// Define the mapping of support KD USB devices to their internal drivers.
//

KD_USB_DRIVER_MAPPING KdUsbDriverMappings[] = {
    {0x0403, 0x6001, KdpFtdiDriverEntry},
};

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
KdUsbInitialize (
    PDEBUG_USB_HOST_DESCRIPTION Host,
    BOOL TestInterface
    )

/*++

Routine Description:

    This routine initializes a USB debug based transport.

Arguments:

    Host - Supplies a pointer to the host controller.

    TestInterface - Supplies a boolean indicating if the interface test should
        be run. This is only true under debugging scenarios where the USB
        debug transport itself is being debugged.

Return Value:

    Status code.

--*/

{

    PKD_USB_DEVICE CurrentDevice;
    BOOL DeviceIsDebugDevice;
    PKD_USB_DEVICE Hub;
    UCHAR HubAddress;
    PKD_USB_DRIVER_MAPPING Mapping;
    PHARDWARE_USB_DEBUG_DEVICE Module;
    ULONG PortCount;
    ULONG PortNumber;
    ULONG RootPortCount;
    KSTATUS Status;

    Mapping = NULL;
    Module = &KdUsbDebugHost;

    //
    // If there's already a fired up controller, don't bother with this one.
    //

    if (Module->Host != NULL) {
        Status = STATUS_SUCCESS;
        goto InitializeEnd;
    }

    Module->Host = Host;

    //
    // Initialize the controller.
    //

    Status = Module->Host->FunctionTable.Initialize(Module->Host->Context);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    KdpUsbStall(Module, 50);

    //
    // Loop enumerating every device in the tree.
    //

    KdUsbNextDeviceAddress = 1;
    KdUsbDevicePathSize = 0;
    CurrentDevice = &(KdUsbDevicePath[KdUsbDevicePathSize]);
    RootPortCount = KD_USB_MAX_PORT_COUNT;
    PortCount = RootPortCount;
    PortNumber = 1;
    Hub = NULL;
    while (TRUE) {
        if (KdUsbDebug != FALSE) {
            HubAddress = 0;
            if (Hub != NULL) {
                HubAddress = Hub->DeviceAddress;
            }

            RtlDebugPrint("Enumerating Hub 0x%x, Port %d\n",
                          HubAddress,
                          PortNumber);
        }

        //
        // Initialize the new device structure.
        //

        RtlZeroMemory(CurrentDevice, sizeof(KD_USB_DEVICE));
        CurrentDevice->Controller = Module;
        CurrentDevice->Hub = Hub;
        CurrentDevice->HubPortNumber = PortNumber;
        DeviceIsDebugDevice = FALSE;
        Status = STATUS_NO_SUCH_DEVICE;

        //
        // If the port number is within bounds, reset the port and attempt to
        // enumerate the device.
        //

        if (PortNumber <= PortCount) {
            Status = KdpUsbResetPort(CurrentDevice);
            if ((Status == STATUS_OUT_OF_BOUNDS) && (Hub == NULL)) {
                RootPortCount = PortNumber - 1;
                PortCount = RootPortCount;
            }

            if ((KSUCCESS(Status)) &&
                (CurrentDevice->Speed != DebugUsbDeviceSpeedInvalid)) {

                Status = KdpUsbEnumerateDevice(CurrentDevice);
                if ((!KSUCCESS(Status)) && (KdUsbDebug != FALSE)) {
                    RtlDebugPrint("Failed to enumerate: %d\n", Status);
                }
            }
        }

        //
        // Determine if this is a supported debug device.
        //

        if ((KSUCCESS(Status)) &&
            (CurrentDevice->Speed != DebugUsbDeviceSpeedInvalid)) {

            Mapping = KdpIsDeviceSupported(CurrentDevice);
            if (Mapping != NULL) {
                DeviceIsDebugDevice = TRUE;
            }

            if (KdUsbDebug != FALSE) {
                RtlDebugPrint("Found Device %04X:%04X, speed %d, address "
                              "0x%x, port count %d, Supported %d\n",
                              CurrentDevice->VendorId,
                              CurrentDevice->ProductId,
                              CurrentDevice->Speed,
                              CurrentDevice->DeviceAddress,
                              CurrentDevice->PortCount,
                              DeviceIsDebugDevice);
            }
        }

        //
        // If the device is the debug device, then rejoice, it's been found.
        //

        if (DeviceIsDebugDevice != FALSE) {
            KdUsbDevicePathSize += 1;
            break;
        }

        //
        // If the device is a hub, enumerate it depth first.
        //

        if (CurrentDevice->PortCount != 0) {
            if (KdUsbDevicePathSize + 1 < DEBUG_USB_DEVICE_PATH_SIZE) {
                Status = KdpUsbHubReset(CurrentDevice);
                if (KSUCCESS(Status)) {
                    if (KdUsbDebug != FALSE) {
                        RtlDebugPrint("Moving into hub 0x%x, %d ports.\n",
                                      CurrentDevice->DeviceAddress,
                                      CurrentDevice->PortCount);
                    }

                    PortCount = CurrentDevice->PortCount;
                    PortNumber = 1;
                    Hub = CurrentDevice;
                    KdUsbDevicePathSize += 1;
                    CurrentDevice = &(KdUsbDevicePath[KdUsbDevicePathSize]);
                    continue;
                }
            }
        }

        //
        // The device is neither a debug device nor a hub, so just move to the
        // next port in the hub.
        //

        PortNumber += 1;

        //
        // If this was the last port in the hub, then move back up to the
        // parent.
        //

        if (PortNumber > PortCount) {

            //
            // If this was the root hub, then the entire tree was
            // enumerated and nothing was found.
            //

            if (KdUsbDevicePathSize == 0) {
                if (KdUsbDebug != FALSE) {
                    RtlDebugPrint("Enumeration complete, no devices.\n");
                }

                Status = STATUS_NO_ELIGIBLE_DEVICES;
                break;
            }

            //
            // Back up to the parent and advance to the next port.
            //

            KdUsbDevicePathSize -= 1;
            CurrentDevice = &(KdUsbDevicePath[KdUsbDevicePathSize]);
            PortNumber = CurrentDevice->HubPortNumber + 1;
            Hub = CurrentDevice->Hub;
            if (Hub != NULL) {
                PortCount = Hub->PortCount;

            } else {
                PortCount = RootPortCount;
            }
        }
    }

    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    //
    // Call the driver to fire up the device.
    //

    RtlZeroMemory(&KdUsbDeviceInterface, sizeof(DEBUG_DEVICE_DESCRIPTION));
    Status = Mapping->DriverEntry(CurrentDevice, &KdUsbDeviceInterface);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    //
    // Set the type, subtype and identifier to match the USB host controller.
    //

    KdUsbDeviceInterface.PortType = DEBUG_PORT_TYPE_USB;
    KdUsbDeviceInterface.PortSubType = Module->Host->PortSubType;
    KdUsbDeviceInterface.Identifier = Module->Host->Identifier;

    //
    // If debugging the USB interface itself, fire up the test. This implements
    // a basic echo terminal, and requires a user at the other end type stuff.
    //

    if (TestInterface != FALSE) {
        Status = KdpTestInterface(&KdUsbDeviceInterface);
        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }

    //
    // Register the end interface so that it can be picked up by KD.
    //

    } else {
        Status = HlRegisterHardware(HardwareModuleDebugDevice,
                                    &KdUsbDeviceInterface);

        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }
    }

InitializeEnd:
    if (!KSUCCESS(Status)) {
        KdUsbDebugHost.Host = NULL;
    }

    return Status;
}

KSTATUS
KdpUsbGetHandoffData (
    PDEBUG_HANDOFF_DATA Data
    )

/*++

Routine Description:

    This routine returns a pointer to the handoff data the USB driver needs to
    operate with a USB debug host controller.

Arguments:

    Data - Supplies a pointer where a pointer to the handoff data is returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_ELIGIBLE_DEVICES if there is no USB debug device.

--*/

{

    PKD_USB_DEVICE Device;
    ULONG PathIndex;
    KSTATUS Status;

    if ((KdUsbDeviceInterface.TableVersion <
         DEBUG_DEVICE_DESCRIPTION_VERSION) ||
        (KdUsbDevicePathSize == 0)) {

        return STATUS_NO_ELIGIBLE_DEVICES;
    }

    Data->U.Usb.DevicePathSize = KdUsbDevicePathSize;
    for (PathIndex = 0; PathIndex < KdUsbDevicePathSize; PathIndex += 1) {
        Data->U.Usb.DevicePath[PathIndex] =
                                      KdUsbDevicePath[PathIndex].HubPortNumber;
    }

    Device = &(KdUsbDevicePath[KdUsbDevicePathSize - 1]);
    Data->U.Usb.DeviceAddress = Device->DeviceAddress;
    Data->U.Usb.HubAddress = 0;
    if (Device->Hub != NULL) {
        Data->U.Usb.HubAddress = Device->Hub->DeviceAddress;
    }

    Data->U.Usb.Configuration = Device->Configuration;
    Data->U.Usb.VendorId = Device->VendorId;
    Data->U.Usb.ProductId = Device->ProductId;
    Status = KdUsbDebugHost.Host->FunctionTable.GetHandoffData(
                                                 KdUsbDebugHost.Host->Context,
                                                 &(Data->U.Usb));

    if (!KSUCCESS(Status)) {
        return Status;
    }

    return Status;
}

KSTATUS
KdpTestInterface (
    PDEBUG_DEVICE_DESCRIPTION Interface
    )

/*++

Routine Description:

    This routine performs a basic interactive test of an interface.

Arguments:

    Interface - Supplies a pointer to the debug transport interface.

Return Value:

    Status code.

--*/

{

    CHAR Buffer[KD_TEST_RECEIVE_BUFFER_SIZE];
    ULONG ExitOffset;
    ULONG ExitSize;
    PSTR ExitString;
    ULONG Index;
    BOOL ReceiveDataAvailable;
    ULONG Size;
    KSTATUS Status;

    ExitString = KD_TEST_EXIT_STRING;
    ExitSize = sizeof(KD_TEST_EXIT_STRING) - 1;
    Status = Interface->FunctionTable.Reset(Interface->Context, 115200);
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Failed to reset: %d\n", Status);
        return Status;
    }

    Status = Interface->FunctionTable.Transmit(Interface->Context,
                                               KD_TEST_WELCOME_STRING,
                                               sizeof(KD_TEST_WELCOME_STRING));

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Failed to transmit: %d\n", Status);
        return Status;
    }

    //
    // Loop echoing data.
    //

    ExitOffset = 0;
    while (TRUE) {
        Status = Interface->FunctionTable.GetStatus(Interface->Context,
                                                    &ReceiveDataAvailable);

        if (!KSUCCESS(Status)) {
            RtlDebugPrint("Failed to get status: %d\n", Status);
            return Status;
        }

        if (ReceiveDataAvailable == FALSE) {
            continue;
        }

        Size = KD_TEST_RECEIVE_BUFFER_SIZE;
        Status = Interface->FunctionTable.Receive(Interface->Context,
                                                  Buffer,
                                                  &Size);

        if (Status == STATUS_NO_DATA_AVAILABLE) {
            continue;
        }

        if (!KSUCCESS(Status)) {
            RtlDebugPrint("Failed to receive: %d\n", Status);
        }

        if (Size > KD_TEST_RECEIVE_BUFFER_SIZE) {
            RtlDebugPrint("Received %d bytes in a buffer %d big!\n",
                          Size,
                          KD_TEST_RECEIVE_BUFFER_SIZE);

            return STATUS_BUFFER_OVERRUN;
        }

        //
        // Loop over the buffer, both adding one to the character if it's
        // non-control and checking for the exit string.
        //

        for (Index = 0; Index < Size; Index += 1) {
            RtlDebugPrint("%02x ", (UCHAR)(Buffer[Index]));

            //
            // If the user typed exit, all they need to do is hit enter.
            //

            if (ExitOffset == ExitSize) {
                if ((Buffer[Index] == '\r') ||
                    (Buffer[Index] == '\n')) {

                    Status = Interface->FunctionTable.Transmit(
                                               Interface->Context,
                                               KD_TEST_GOODBYE_STRING,
                                               sizeof(KD_TEST_GOODBYE_STRING));

                    if (!KSUCCESS(Status)) {
                        RtlDebugPrint("Failed to transmit: %d\n", Status);
                        return Status;
                    }

                    goto KdpTestInterfaceEnd;

                //
                // They didn't hit enter, so it must not have been exit.
                //

                } else {
                    ExitOffset = 0;
                }

            //
            // Check to see if it lines up with the exit string.
            //

            } else if (Buffer[Index] == ExitString[ExitOffset]) {
                ExitOffset += 1;

            //
            // It does not line up with the exit string, so reset the
            // search.
            //

            } else {
                ExitOffset = 0;
            }
        }

        RtlDebugPrint("\n");

        //
        // Echo those bytes back to the user.
        //

        Status = Interface->FunctionTable.Transmit(Interface->Context,
                                                   Buffer,
                                                   Size);

        if (!KSUCCESS(Status)) {
            RtlDebugPrint("Failed to transmit: %d\n", Status);
            return Status;
        }
    }

KdpTestInterfaceEnd:
    RtlDebugPrint("Exiting KD Test: %d\n", Status);
    return Status;
}

KSTATUS
KdpUsbDefaultControlTransfer (
    PKD_USB_DEVICE Device,
    PUSB_SETUP_PACKET Setup,
    DEBUG_USB_TRANSFER_DIRECTION Direction,
    PVOID Buffer,
    PULONG BufferSize
    )

/*++

Routine Description:

    This routine performs a control transfer to endpoint zero of the given
    device.

Arguments:

    Device - Supplies a pointer to the target of the transfer.

    Setup - Supplies a pointer to the initialized setup packet.

    Direction - Supplies the transfer direction.

    Buffer - Supplies the transfer buffer.

    BufferSize - Supplies an optional pointer that upon input contains the size
        of the buffer in bytes. On output returns the number of bytes actually
        transferred.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = KdpUsbControlTransfer(Device,
                                   &(Device->EndpointZero),
                                   Setup,
                                   Direction,
                                   Buffer,
                                   BufferSize);

    return Status;
}

KSTATUS
KdpUsbControlTransfer (
    PKD_USB_DEVICE Device,
    PDEBUG_USB_ENDPOINT Endpoint,
    PUSB_SETUP_PACKET Setup,
    DEBUG_USB_TRANSFER_DIRECTION Direction,
    PVOID Buffer,
    PULONG BufferSize
    )

/*++

Routine Description:

    This routine performs a control transfer to the given endpoint of the
    given device.

Arguments:

    Device - Supplies a pointer to the target of the transfer.

    Endpoint - Supplies a pointer to the endpoint.

    Setup - Supplies a pointer to the initialized setup packet.

    Direction - Supplies the transfer direction.

    Buffer - Supplies the transfer buffer.

    BufferSize - Supplies an optional pointer that upon input contains the size
        of the buffer in bytes. On output returns the number of bytes actually
        transferred.

Return Value:

    Status code.

--*/

{

    ULONG BufferLength;
    ULONG Length;
    KSTATUS Status;
    DEBUG_USB_TRANSFER Transfer;
    BOOL TransferSetup;

    TransferSetup = FALSE;
    BufferLength = 0;
    if (BufferSize != NULL) {
        BufferLength = *BufferSize;
    }

    Length = BufferLength + sizeof(USB_SETUP_PACKET);

    //
    // Create the transfer.
    //

    RtlZeroMemory(&Transfer, sizeof(DEBUG_USB_TRANSFER));
    Transfer.Endpoint = Endpoint;
    Transfer.Direction = Direction;
    Transfer.Length = Length;
    if (KdUsbDebugAllTransfers != FALSE) {
        RtlDebugPrint("CONTROL Dev %02X EP %02X: %02X %02X %04X %04X %04x ... ",
                      Device->DeviceAddress,
                      Endpoint->EndpointNumber,
                      Setup->RequestType,
                      Setup->Request,
                      Setup->Value,
                      Setup->Index,
                      Setup->Length);
    }

    Status = KdpUsbSetupTransfer(Device->Controller, &Transfer);
    if (!KSUCCESS(Status)) {
        goto UsbControlTransferEnd;
    }

    TransferSetup = TRUE;
    RtlCopyMemory(Transfer.Buffer, Setup, sizeof(USB_SETUP_PACKET));
    if ((Direction == DebugUsbTransferDirectionOut) ||
        (Direction == DebugUsbTransferBidirectional)) {

        if (BufferLength != 0) {
            RtlCopyMemory(Transfer.Buffer + sizeof(USB_SETUP_PACKET),
                          Buffer,
                          BufferLength);
        }
    }

    //
    // Execute the transfer.
    //

    Status = KdpUsbSubmitTransfer(Device->Controller, &Transfer, TRUE);

    //
    // If bytes of data were copied and this is an IN transfer, copy the data
    // back to the caller's buffer.
    //

    if (Transfer.LengthTransferred > sizeof(USB_SETUP_PACKET)) {
        if ((Direction == DebugUsbTransferDirectionIn) ||
            (Direction == DebugUsbTransferBidirectional)) {

            ASSERT(Transfer.LengthTransferred - sizeof(USB_SETUP_PACKET) <=
                   BufferLength);

            RtlCopyMemory(
                        Buffer,
                        Transfer.Buffer + sizeof(USB_SETUP_PACKET),
                        Transfer.LengthTransferred - sizeof(USB_SETUP_PACKET));
        }

        BufferLength = Transfer.LengthTransferred - sizeof(USB_SETUP_PACKET);

    } else {
        BufferLength = 0;
    }

UsbControlTransferEnd:
    if (BufferSize != NULL) {
        *BufferSize = BufferLength;
    }

    if (TransferSetup != FALSE) {
        KdpUsbRetireTransfer(Device->Controller, &Transfer);
    }

    if (KdUsbDebugAllTransfers != FALSE) {
        RtlDebugPrint("%04X %d\n", BufferLength, Status);
    }

    return Status;
}

KSTATUS
KdpUsbSetupTransfer (
    PHARDWARE_USB_DEBUG_DEVICE Device,
    PDEBUG_USB_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine allocates a buffer and initializes the given USB transfer.

Arguments:

    Device - Supplies a pointer to the hardware debug device.

    Transfer - Supplies a pointer to the transfer to initialize to. The caller
        must have filled out the endpoint, direction, and length members. This
        routine will allocate buffer space for the transfer data.

Return Value:

    STATUS_SUCCESS if the hub status was successfully queried.

    STATUS_INVALID_PARAMETER if the given transfer was not filled out correctly.

    STATUS_INSUFFICIENT_RESOURCES if a buffer for the transfer could not be
    allocated.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

--*/

{

    PDEBUG_USB_HOST_SETUP_TRANSFER SetupTransfer;
    KSTATUS Status;

    SetupTransfer = Device->Host->FunctionTable.SetupTransfer;
    Status = SetupTransfer(Device->Host->Context, Transfer);
    return Status;
}

KSTATUS
KdpUsbSubmitTransfer (
    PHARDWARE_USB_DEBUG_DEVICE Device,
    PDEBUG_USB_TRANSFER Transfer,
    BOOL WaitForCompletion
    )

/*++

Routine Description:

    This routine submits a previously set up USB transfer.

Arguments:

    Device - Supplies a pointer to the hardware debug device.

    Transfer - Supplies a pointer to the transfer to submit.

    WaitForCompletion - Supplies a boolean indicating if the routine should
        block until the transfer completes.

Return Value:

    STATUS_SUCCESS if the hub status was successfully queried.

    STATUS_INVALID_PARAMETER if the given transfer was not initialized
    correctly.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

    STATUS_TIMEOUT if the caller wanted to wait for the transfer but it
    never completed.

--*/

{

    KSTATUS Status;
    PDEBUG_USB_HOST_SUBMIT_TRANSFER SubmitTransfer;

    SubmitTransfer = Device->Host->FunctionTable.SubmitTransfer;
    Status = SubmitTransfer(Device->Host->Context,
                            Transfer,
                            WaitForCompletion);

    return Status;
}

KSTATUS
KdpUsbCheckTransfer (
    PHARDWARE_USB_DEBUG_DEVICE Device,
    PDEBUG_USB_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine checks on the completion status of a transfer.

Arguments:

    Device - Supplies a pointer to the hardware debug device.

    Transfer - Supplies a pointer to the transfer to check.

Return Value:

    STATUS_SUCCESS if the hub status was successfully queried.

    STATUS_INVALID_PARAMETER if the given transfer was not initialized
    correctly.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

    STATUS_MORE_PROCESSING_REQUIRED if the transfer is still in progress.

--*/

{

    PDEBUG_USB_HOST_CHECK_TRANSFER CheckTransfer;
    KSTATUS Status;

    CheckTransfer = Device->Host->FunctionTable.CheckTransfer;
    Status = CheckTransfer(Device->Host->Context, Transfer);
    return Status;
}

KSTATUS
KdpUsbRetireTransfer (
    PHARDWARE_USB_DEBUG_DEVICE Device,
    PDEBUG_USB_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine retires a USB transfer. This frees the buffer allocated
    during setup.

Arguments:

    Device - Supplies a pointer to the hardware debug device.

    Transfer - Supplies a pointer to the transfer to retire.

Return Value:

    STATUS_SUCCESS if the hub status was successfully queried.

    STATUS_INVALID_PARAMETER if the given transfer was not initialized
    correctly.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

--*/

{

    PDEBUG_USB_HOST_RETIRE_TRANSFER RetireTransfer;
    KSTATUS Status;

    RetireTransfer = Device->Host->FunctionTable.RetireTransfer;
    Status = RetireTransfer(Device->Host->Context, Transfer);
    return Status;
}

KSTATUS
KdpUsbStall (
    PHARDWARE_USB_DEBUG_DEVICE Device,
    ULONG Milliseconds
    )

/*++

Routine Description:

    This routine stalls execution for the given duration.

Arguments:

    Device - Supplies a pointer to the hardware debug device.

    Milliseconds - Supplies the number of milliseconds to stall for.

Return Value:

    STATUS_SUCCESS if the hub status was successfully queried.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

--*/

{

    PDEBUG_USB_HOST_STALL Stall;
    KSTATUS Status;

    Stall = Device->Host->FunctionTable.Stall;
    Status = Stall(Device->Host->Context, Milliseconds);
    return Status;
}

KSTATUS
KdpUsbInitializeEndpoint (
    PKD_USB_DEVICE Device,
    PUSB_ENDPOINT_DESCRIPTOR Descriptor,
    PDEBUG_USB_ENDPOINT Endpoint
    )

/*++

Routine Description:

    This routine initializes an endpoint based on a given descriptor.

Arguments:

    Device - Supplies a pointer to the KD USB device.

    Descriptor - Supplies a pointer to the endpoint descriptor.

    Endpoint - Supplies a pointer to the endpoint to initialize.

Return Value:

    STATUS_SUCCESS if the hub status was successfully queried.

    STATUS_INVALID_PARAMETER if the device or descriptor was invalid.

--*/

{

    if ((Descriptor->DescriptorType != UsbDescriptorTypeEndpoint) ||
        (Descriptor->Length < sizeof(USB_ENDPOINT_DESCRIPTOR))) {

        return STATUS_INVALID_PARAMETER;
    }

    RtlCopyMemory(Endpoint,
                  &(Device->EndpointZero),
                  sizeof(DEBUG_USB_ENDPOINT));

    Endpoint->DataToggle = FALSE;
    Endpoint->Halted = FALSE;
    Endpoint->EndpointNumber = Descriptor->EndpointAddress;
    Endpoint->MaxPacketSize = Descriptor->MaxPacketSize;
    Endpoint->Direction = DebugUsbTransferDirectionOut;
    if ((Descriptor->EndpointAddress &
         USB_ENDPOINT_ADDRESS_DIRECTION_IN) != 0) {

        Endpoint->Direction = DebugUsbTransferDirectionIn;
    }

    switch (Descriptor->Attributes & USB_ENDPOINT_ATTRIBUTES_TYPE_MASK) {
    case USB_ENDPOINT_ATTRIBUTES_TYPE_CONTROL:
        Endpoint->Type = DebugUsbTransferTypeControl;
        break;

    case USB_ENDPOINT_ATTRIBUTES_TYPE_INTERRUPT:
        Endpoint->Type = DebugUsbTransferTypeInterrupt;
        break;

    case USB_ENDPOINT_ATTRIBUTES_TYPE_BULK:
        Endpoint->Type = DebugUsbTransferTypeBulk;
        break;

    case USB_ENDPOINT_ATTRIBUTES_TYPE_ISOCHRONOUS:
        Endpoint->Type = DebugUsbTransferTypeIsochronous;
        break;

    default:
        return STATUS_INVALID_PARAMETER;
    }

    return STATUS_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
KdpUsbResetPort (
    PKD_USB_DEVICE Device
    )

/*++

Routine Description:

    This routine performs a port reset on the given device. The device's
    controller and hub details need to be filled in. This routine will fill in
    the device's speed and reset the address to zero on success.

Arguments:

    Device - Supplies a pointer to the USB device to reset.

Return Value:

    Status code.

--*/

{

    PHARDWARE_USB_DEBUG_DEVICE Controller;
    ULONG PortNumber;
    ULONG PortStatus;
    KSTATUS Status;

    Device->Speed = DebugUsbDeviceSpeedInvalid;
    Controller = Device->Controller;
    PortNumber = Device->HubPortNumber;

    ASSERT(PortNumber != 0);

    if (Device->Hub == NULL) {
        Status = KdpUsbGetRootHubStatus(Controller,
                                        PortNumber - 1,
                                        &PortStatus);

    } else {
        Status = KdpUsbHubGetStatus(Device->Hub, PortNumber, &PortStatus);
    }

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Reset the port.
    //

    PortStatus |= DEBUG_USB_PORT_STATUS_RESET;
    PortStatus &= ~(DEBUG_USB_PORT_STATUS_ENABLED |
                    DEBUG_USB_PORT_STATUS_SUSPENDED);

    if (Device->Hub == NULL) {
        Status = KdpUsbSetRootHubStatus(Controller, PortNumber - 1, PortStatus);

    } else {
        Status = KdpUsbHubSetStatus(Device->Hub, PortNumber, PortStatus);
    }

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Stall for 10ms per section 7.1.7.5 of the USB specification (TDRST).
    // This is reduced because around 10ms devices start to suspend themselves
    // and stop responding to requests.
    //

    KdpUsbStall(Controller, 2);

    //
    // Now enable the port.
    //

    PortStatus &= ~(DEBUG_USB_PORT_STATUS_RESET |
                    DEBUG_USB_PORT_STATUS_SUSPENDED);

    PortStatus |= DEBUG_USB_PORT_STATUS_ENABLED;
    if (Device->Hub == NULL) {
        Status = KdpUsbSetRootHubStatus(Controller, PortNumber - 1, PortStatus);

    } else {
        Status = KdpUsbHubSetStatus(Device->Hub, PortNumber, PortStatus);
    }

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Stall for 10ms per section 7.1.7.5 of the USB specification (TRSTRCY).
    //

    KdpUsbStall(Controller, 20);

    //
    // Get the status of the port now.
    //

    if (Device->Hub == NULL) {
        Status = KdpUsbGetRootHubStatus(Controller,
                                        PortNumber - 1,
                                        &PortStatus);

    } else {
        Status = KdpUsbHubGetStatus(Device->Hub, PortNumber, &PortStatus);
    }

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // If the device is not present, then exit claiming success. It may have
    // been removed during the reset.
    //

    if ((PortStatus & DEBUG_USB_PORT_STATUS_CONNECTED) == 0) {
        return STATUS_SUCCESS;
    }

    //
    // If the port got disabled, fail the reset. Note that a device might still
    // be in the connected state even though it is in the disabled state, so
    // this must fail. See Section 11.24.2.7.1 PORT_CONNECTION of the USB 2.0
    // Specification.
    //

    if ((PortStatus & DEBUG_USB_PORT_STATUS_ENABLED) == 0) {
        return STATUS_SUCCESS;
    }

    if ((PortStatus & DEBUG_USB_PORT_STATUS_LOW_SPEED) != 0) {
        Device->Speed = DebugUsbDeviceSpeedLow;

    } else if ((PortStatus & DEBUG_USB_PORT_STATUS_FULL_SPEED) != 0) {
        Device->Speed = DebugUsbDeviceSpeedFull;

    } else if ((PortStatus & DEBUG_USB_PORT_STATUS_HIGH_SPEED) != 0) {
        Device->Speed = DebugUsbDeviceSpeedHigh;

    } else if ((PortStatus & DEBUG_USB_PORT_STATUS_SUPER_SPEED) != 0) {
        Device->Speed = DebugUsbDeviceSpeedSuper;

    } else {

        ASSERT(FALSE);

        return STATUS_INVALID_CONFIGURATION;
    }

    //
    // Stall again to allow the device time to initialize.
    //

    KdpUsbStall(Controller, 20);
    return STATUS_SUCCESS;
}

KSTATUS
KdpUsbEnumerateDevice (
    PKD_USB_DEVICE Device
    )

/*++

Routine Description:

    This routine performs enumeration on the given USB device. The device
    should be powered on, enabled, and at address zero.

Arguments:

    Device - Supplies a pointer to the device to enumerate.

Return Value:

    Status code.

--*/

{

    PUSB_CONFIGURATION_DESCRIPTOR Configuration;
    UCHAR ConfigurationBuffer[KD_USB_CONFIGURATION_LENGTH];
    USB_DEVICE_DESCRIPTOR DeviceDescriptor;
    PUSB_HUB_DESCRIPTOR HubDescriptor;
    PUSB_INTERFACE_DESCRIPTOR Interface;
    ULONG Length;
    USB_SETUP_PACKET Setup;
    KSTATUS Status;

    //
    // Set up endpoint zero.
    //

    RtlZeroMemory(&(Device->EndpointZero), sizeof(DEBUG_USB_ENDPOINT));
    Device->EndpointZero.Type = DebugUsbTransferTypeControl;
    Device->EndpointZero.Direction = DebugUsbTransferBidirectional;
    Device->EndpointZero.Speed = Device->Speed;
    Device->EndpointZero.HubAddress = 0;
    if (Device->Hub != NULL) {
        Device->EndpointZero.HubAddress = Device->Hub->DeviceAddress;
    }

    Device->EndpointZero.HubPort = Device->HubPortNumber;
    Device->EndpointZero.MaxPacketSize = 8;

    //
    // Read the device descriptor. Start by requesting only the first 8 bytes
    // to know what the endpoint size is (it has to be at least eight).
    //

    Setup.RequestType = USB_SETUP_REQUEST_TO_HOST |
                        USB_SETUP_REQUEST_STANDARD |
                        USB_SETUP_REQUEST_DEVICE_RECIPIENT;

    Setup.Request = USB_DEVICE_REQUEST_GET_DESCRIPTOR;
    Setup.Value = UsbDescriptorTypeDevice << 8;
    Setup.Index = 0;
    Setup.Length = 8;
    Length = Setup.Length;
    Status = KdpUsbDefaultControlTransfer(Device,
                                          &Setup,
                                          DebugUsbTransferDirectionIn,
                                          &DeviceDescriptor,
                                          &Length);

    if (!KSUCCESS(Status)) {
        goto UsbEnumerateDeviceEnd;
    }

    if (Length != Setup.Length) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto UsbEnumerateDeviceEnd;
    }

    if (DeviceDescriptor.DescriptorType != UsbDescriptorTypeDevice) {
        Status = STATUS_DEVICE_IO_ERROR;
        goto UsbEnumerateDeviceEnd;
    }

    Device->EndpointZero.MaxPacketSize = DeviceDescriptor.MaxPacketSize;

    //
    // Reset the device again, as some devices may get confused sending only
    // part of the device descriptor.
    //

    Status = KdpUsbResetPort(Device);
    if (!KSUCCESS(Status)) {
        goto UsbEnumerateDeviceEnd;
    }

    //
    // Don't tolerate the device changing speeds between resets.
    //

    if (Device->Speed != Device->EndpointZero.Speed) {
        Status = STATUS_DEVICE_IO_ERROR;
        goto UsbEnumerateDeviceEnd;
    }

    //
    // Now request the entire device descriptor.
    //

    Setup.Length = sizeof(USB_DEVICE_DESCRIPTOR);
    Length = Setup.Length;
    Status = KdpUsbDefaultControlTransfer(Device,
                                          &Setup,
                                          DebugUsbTransferDirectionIn,
                                          &DeviceDescriptor,
                                          &Length);

    if (!KSUCCESS(Status)) {
        goto UsbEnumerateDeviceEnd;
    }

    if (Length != Setup.Length) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto UsbEnumerateDeviceEnd;
    }

    if (DeviceDescriptor.DescriptorType != UsbDescriptorTypeDevice) {
        Status = STATUS_DEVICE_IO_ERROR;
        goto UsbEnumerateDeviceEnd;
    }

    Device->VendorId = DeviceDescriptor.VendorId;
    Device->ProductId = DeviceDescriptor.ProductId;

    //
    // If the device descriptor indicates this is a hub, set the port count to
    // a non-zero to remember to query it later.
    //

    if (DeviceDescriptor.Class == UsbDeviceClassHub) {
        Device->PortCount = -1;
        Device->InterfaceNumber = 0xFF;
    }

    //
    // Send a SET_ADDRESS command to the device to get it off of address zero.
    //

    if (KdUsbNextDeviceAddress > USB_MAX_DEVICE_ADDRESS) {
        Status = STATUS_RESOURCE_IN_USE;
        goto UsbEnumerateDeviceEnd;
    }

    RtlZeroMemory(&Setup, sizeof(USB_SETUP_PACKET));
    Setup.RequestType = USB_SETUP_REQUEST_TO_DEVICE |
                        USB_SETUP_REQUEST_STANDARD |
                        USB_SETUP_REQUEST_DEVICE_RECIPIENT;

    Setup.Request = USB_DEVICE_REQUEST_SET_ADDRESS;
    Setup.Value = KdUsbNextDeviceAddress;
    Setup.Index = 0;
    Setup.Length = 0;
    Status = KdpUsbDefaultControlTransfer(Device,
                                          &Setup,
                                          DebugUsbTransferDirectionOut,
                                          NULL,
                                          NULL);

    if (!KSUCCESS(Status)) {
        goto UsbEnumerateDeviceEnd;
    }

    //
    // Wait 2ms for the set address request to settle (see section 9.2.6.3 of
    // the USB 2.0 specification).
    //

    KdpUsbStall(Device->Controller, 2);
    Device->DeviceAddress = KdUsbNextDeviceAddress;
    Device->EndpointZero.DeviceAddress = Device->DeviceAddress;
    KdUsbNextDeviceAddress += 1;

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
    Setup.Length = KD_USB_CONFIGURATION_LENGTH;
    Length = Setup.Length;
    Configuration = (PUSB_CONFIGURATION_DESCRIPTOR)ConfigurationBuffer;
    Status = KdpUsbDefaultControlTransfer(Device,
                                          &Setup,
                                          DebugUsbTransferDirectionIn,
                                          Configuration,
                                          &Length);

    if (!KSUCCESS(Status)) {
        goto UsbEnumerateDeviceEnd;
    }

    if (Length < sizeof(USB_CONFIGURATION_DESCRIPTOR)) {
        Status = STATUS_INVALID_CONFIGURATION;
        goto UsbEnumerateDeviceEnd;
    }

    //
    // Set the default configuration.
    //

    RtlZeroMemory(&Setup, sizeof(USB_SETUP_PACKET));
    Setup.RequestType = USB_SETUP_REQUEST_TO_DEVICE |
                        USB_SETUP_REQUEST_STANDARD |
                        USB_SETUP_REQUEST_DEVICE_RECIPIENT;

    Setup.Request = USB_DEVICE_REQUEST_SET_CONFIGURATION;
    Setup.Value = Configuration->ConfigurationValue;
    Setup.Index = 0;
    Setup.Length = 0;
    Status = KdpUsbDefaultControlTransfer(Device,
                                          &Setup,
                                          DebugUsbTransferDirectionOut,
                                          NULL,
                                          NULL);

    if (!KSUCCESS(Status)) {
        goto UsbEnumerateDeviceEnd;
    }

    Device->Configuration = Configuration->ConfigurationValue;

    //
    // Loop through the interfaces looking for a hub interface.
    //

    Interface = (PUSB_INTERFACE_DESCRIPTOR)(Configuration + 1);
    while ((UINTN)(Interface + 1) <= (UINTN)ConfigurationBuffer + Length) {
        if (Interface->DescriptorType == UsbDescriptorTypeInterface) {
            if (Interface->Class == UsbDeviceClassHub) {
                Device->InterfaceNumber = Interface->InterfaceNumber;
                Device->PortCount = -1;
                break;
            }
        }

        Interface = ((PVOID)Interface) + Interface->Length;
    }

    //
    // If the device is a USB hub, get the hub descriptor to figure out how
    // many ports there are.
    //

    if (Device->PortCount != 0) {
        HubDescriptor = (PUSB_HUB_DESCRIPTOR)ConfigurationBuffer;
        Setup.RequestType = USB_SETUP_REQUEST_TO_HOST |
                            USB_SETUP_REQUEST_CLASS |
                            USB_SETUP_REQUEST_DEVICE_RECIPIENT;

        Setup.Request = USB_DEVICE_REQUEST_GET_DESCRIPTOR;
        Setup.Value = UsbDescriptorTypeHub << 8;
        Setup.Index = 0;
        Setup.Length = USB_HUB_DESCRIPTOR_MAX_SIZE;
        Length = Setup.Length;
        Status = KdpUsbDefaultControlTransfer(Device,
                                              &Setup,
                                              DebugUsbTransferDirectionIn,
                                              Configuration,
                                              &Length);

        if (!KSUCCESS(Status)) {
            goto UsbEnumerateDeviceEnd;
        }

        if (Length < sizeof(USB_HUB_DESCRIPTOR)) {
            Status = STATUS_INVALID_CONFIGURATION;
            goto UsbEnumerateDeviceEnd;
        }

        if ((HubDescriptor->DescriptorType != UsbDescriptorTypeHub) ||
            (HubDescriptor->Length < sizeof(USB_HUB_DESCRIPTOR))) {

            Status = STATUS_NOT_SUPPORTED;
            goto UsbEnumerateDeviceEnd;
        }

        Device->PortCount = HubDescriptor->PortCount;
    }

    Status = STATUS_SUCCESS;

UsbEnumerateDeviceEnd:
    return Status;
}

KSTATUS
KdpUsbGetRootHubStatus (
    PHARDWARE_USB_DEBUG_DEVICE Device,
    ULONG PortIndex,
    PULONG PortStatus
    )

/*++

Routine Description:

    This routine queries the host controller for the status of a root hub port.

Arguments:

    Device - Supplies a pointer to the hardware debug device.

    PortIndex - Supplies the zero-based port number to query.

    PortStatus - Supplies a pointer where the port status is returned.

Return Value:

    STATUS_SUCCESS if the hub status was successfully queried.

    STATUS_OUT_OF_BOUNDS if the controller port index is out of range.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

--*/

{

    PDEBUG_USB_HOST_GET_ROOT_HUB_STATUS GetRootHubStatus;
    KSTATUS Status;

    GetRootHubStatus = Device->Host->FunctionTable.GetRootHubStatus;
    Status = GetRootHubStatus(Device->Host->Context, PortIndex, PortStatus);
    return Status;
}

KSTATUS
KdpUsbSetRootHubStatus (
    PHARDWARE_USB_DEBUG_DEVICE Device,
    ULONG PortIndex,
    ULONG PortStatus
    )

/*++

Routine Description:

    This routine sets the host controller for the status of a root hub port.

Arguments:

    Device - Supplies a pointer to the hardware debug device.

    PortIndex - Supplies the zero-based port number to set.

    PortStatus - Supplies the port status to set.

Return Value:

    STATUS_SUCCESS if the hub status was successfully queried.

    STATUS_OUT_OF_BOUNDS if the controller port index is out of range.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

--*/

{

    PDEBUG_USB_HOST_SET_ROOT_HUB_STATUS SetRootHubStatus;
    KSTATUS Status;

    SetRootHubStatus = Device->Host->FunctionTable.SetRootHubStatus;
    Status = SetRootHubStatus(Device->Host->Context, PortIndex, PortStatus);
    return Status;
}

PKD_USB_DRIVER_MAPPING
KdpIsDeviceSupported (
    PKD_USB_DEVICE Device
    )

/*++

Routine Description:

    This routine returns the driver mapping entry for a given KD USB device.

Arguments:

    Device - Supplies a pointer to the enumerated device.

Return Value:

    Returns a pointer to the driver mapping if the device is supported.

    NULL if the device is not a supported KD USB device.

--*/

{

    ULONG Index;
    PKD_USB_DRIVER_MAPPING Mapping;
    ULONG MappingCount;

    MappingCount = sizeof(KdUsbDriverMappings) /
                   sizeof(KdUsbDriverMappings[0]);

    for (Index = 0; Index < MappingCount; Index += 1) {
        Mapping = &(KdUsbDriverMappings[Index]);
        if ((Mapping->VendorId == Device->VendorId) &&
            (Mapping->ProductId == Device->ProductId)) {

            return Mapping;
        }
    }

    return NULL;
}

