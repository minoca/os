/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    kdehci.c

Abstract:

    This module implements kernel debugger transport support over EHCI USB host
    controllers.

Author:

    Evan Green 26-Mar-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/kdebug.h>
#include <minoca/kernel/kdusb.h>
#include "ehcihw.h"
#include "ehcidbg.h"

//
// --------------------------------------------------------------------- Macros
//

//
// These macros read and write EHCI debug device registers.
//

#define EHCI_READ_REGISTER(_Controller, _Register) \
    HlReadRegister32((_Controller)->OperationalBase + (_Register))

#define EHCI_WRITE_REGISTER(_Controller, _Register, _Value) \
    HlWriteRegister32((_Controller)->OperationalBase + (_Register), (_Value));

#define EHCI_READ_PORT_REGISTER(_Controller, _PortIndex) \
    EHCI_READ_REGISTER(                                  \
                   _Controller,                          \
                   EhciRegisterPortStatusBase + ((_PortIndex) * sizeof(ULONG)))

#define EHCI_WRITE_PORT_REGISTER(_Controller, _PortIndex, _Value) \
    EHCI_WRITE_REGISTER(                                                       \
                  _Controller,                                                 \
                  EhciRegisterPortStatusBase + ((_PortIndex) * sizeof(ULONG)), \
                  _Value)

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
KdEhciInitialize (
    PVOID Context
    );

KSTATUS
KdEhciGetRootHubStatus (
    PVOID Context,
    ULONG PortIndex,
    PULONG PortStatus
    );

KSTATUS
KdEhciSetRootHubStatus (
    PVOID Context,
    ULONG PortIndex,
    ULONG PortStatus
    );

KSTATUS
KdEhciSetupTransfer (
    PVOID Context,
    PDEBUG_USB_TRANSFER Transfer
    );

KSTATUS
KdEhciSubmitTransfer (
    PVOID Context,
    PDEBUG_USB_TRANSFER Transfer,
    BOOL WaitForCompletion
    );

KSTATUS
KdEhciCheckTransfer (
    PVOID Context,
    PDEBUG_USB_TRANSFER Transfer
    );

KSTATUS
KdEhciRetireTransfer (
    PVOID Context,
    PDEBUG_USB_TRANSFER Transfer
    );

KSTATUS
KdEhciStall (
    PVOID Context,
    ULONG Milliseconds
    );

KSTATUS
KdEhciGetHandoffData (
    PVOID Context,
    PDEBUG_USB_HANDOFF_DATA HandoffData
    );

KSTATUS
KdEhciResetController (
    PEHCI_DEBUG_DEVICE Controller
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the step and registration status EHCI got to, for debugging.
//

KSTATUS KdEhciRegistrationStatus;

//
// Define the default template for the EHCI USB host description.
//

DEBUG_USB_HOST_DESCRIPTION KdEhciDescriptionTemplate = {
    DEBUG_USB_HOST_DESCRIPTION_VERSION,
    {
        KdEhciInitialize,
        KdEhciGetRootHubStatus,
        KdEhciSetRootHubStatus,
        KdEhciSetupTransfer,
        KdEhciSubmitTransfer,
        KdEhciCheckTransfer,
        KdEhciRetireTransfer,
        KdEhciStall,
        KdEhciGetHandoffData
    },

    NULL,
    0,
    DEBUG_PORT_USB_EHCI
};

//
// ------------------------------------------------------------------ Functions
//

VOID
KdEhciModuleEntry (
    VOID
    )

/*++

Routine Description:

    This routine is the entry point for a hardware module. Its role is to
    detect the prescense of any of the hardware modules it contains
    implementations for and instantiate them with the kernel.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PEHCI_DEBUG_DEVICE Context;
    PDEBUG_PORT_TABLE2 DebugTable;
    PDEBUG_DEVICE_INFORMATION Device;
    UINTN DeviceIndex;
    PGENERIC_ADDRESS GenericAddress;
    ULONG Size;
    PULONG SizePointer;
    KSTATUS Status;

    DebugTable = HlGetAcpiTable(DBG2_SIGNATURE, NULL);
    if (DebugTable == NULL) {
        return;
    }

    //
    // Loop through looking for an EHCI debug device.
    //

    Device = (PDEBUG_DEVICE_INFORMATION)((PVOID)DebugTable +
                                         DebugTable->DeviceInformationOffset);

    for (DeviceIndex = 0;
         DeviceIndex < DebugTable->DeviceInformationCount;
         DeviceIndex += 1) {

        //
        // Skip anything but EHCI.
        //

        if ((Device->PortType != DEBUG_PORT_TYPE_USB) ||
            (Device->PortSubType != DEBUG_PORT_USB_EHCI)) {

            continue;
        }

        //
        // There's supposed to be just one address.
        //

        if (Device->GenericAddressCount != 1) {
            continue;
        }

        GenericAddress = (PGENERIC_ADDRESS)((PVOID)Device +
                                            Device->BaseAddressRegisterOffset);

        SizePointer = (PULONG)((PVOID)Device + Device->AddressSizeOffset);
        Size = *SizePointer;
        if ((GenericAddress->AddressSpaceId != AddressSpaceMemory) ||
            (GenericAddress->Address <= 1) ||
            (Size == 0)) {

            continue;
        }

        //
        // Allocate and initialize the device context.
        //

        Context = HlAllocateMemory(sizeof(EHCI_DEBUG_DEVICE),
                                   EHCI_DEBUG_ALLOCATION_TAG,
                                   FALSE,
                                   NULL);

        if (Context == NULL) {
            KdEhciRegistrationStatus = STATUS_INSUFFICIENT_RESOURCES;
            continue;
        }

        RtlZeroMemory(Context, sizeof(EHCI_DEBUG_DEVICE));
        Context->RegisterBase = HlMapPhysicalAddress(GenericAddress->Address,
                                                     Size,
                                                     TRUE);

        if (Context->RegisterBase == NULL) {
            continue;
        }

        //
        // Register the host controller.
        //

        KdEhciDescriptionTemplate.Context = Context;
        KdEhciDescriptionTemplate.Identifier = GenericAddress->Address;
        Status = HlRegisterHardware(HardwareModuleDebugUsbHostController,
                                    &KdEhciDescriptionTemplate);

        KdEhciRegistrationStatus = Status;
        Device = (PDEBUG_DEVICE_INFORMATION)((PVOID)Device +
                                          READ_UNALIGNED16(&(Device->Length)));
    }

    return;
}

KSTATUS
KdEhciInitialize (
    PVOID Context
    )

/*++

Routine Description:

    This routine initializes a USB debug device, preparing it to return the
    root hub status and ultimately send and receive transfers.

Arguments:

    Context - Supplies a pointer to the device context.

Return Value:

    Status code.

--*/

{

    PVOID Buffer;
    PHYSICAL_ADDRESS BufferPhysical;
    PHYSICAL_ADDRESS BufferPhysicalEnd;
    PEHCI_DEBUG_DEVICE Device;
    PVOID LengthRegister;
    PHYSICAL_ADDRESS NextBuffer;
    ULONG Parameters;
    PVOID ParametersRegister;
    KSTATUS Status;

    Device = (PEHCI_DEBUG_DEVICE)Context;

    //
    // Get the offset of the operational registers.
    //

    LengthRegister = Device->RegisterBase + EHCI_CAPABILITY_LENGTH_REGISTER;
    Device->OperationalBase = Device->RegisterBase +
                              HlReadRegister8(LengthRegister);

    if (Device->OperationalBase == Device->RegisterBase) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto InitializeEnd;
    }

    //
    // Compute the port count.
    //

    ParametersRegister = Device->RegisterBase +
                         EHCI_CAPABILITY_PARAMETERS_REGISTER;

    Parameters = HlReadRegister32(ParametersRegister);
    Device->PortCount = Parameters & EHCI_CAPABILITY_PARAMETERS_PORT_COUNT_MASK;
    if (Device->PortCount == 0) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto InitializeEnd;
    }

    //
    // Allocate and initialize the queue heads if not done.
    //

    if (Device->Data.ReclamationQueue == NULL) {
        Buffer = HlAllocateMemory(EHCI_MEMORY_ALLOCATION_SIZE,
                                  EHCI_DEBUG_ALLOCATION_TAG,
                                  TRUE,
                                  &BufferPhysical);

        if (Buffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializeEnd;
        }

        BufferPhysicalEnd = BufferPhysical + EHCI_MEMORY_ALLOCATION_SIZE;
        RtlZeroMemory(Buffer, EHCI_MEMORY_ALLOCATION_SIZE);

        //
        // Chop up the buffer, reserving the first region for the reclamation
        // queue and end queue, and splitting the remainder between the two
        // transfers.
        //

        Device->Data.ReclamationQueue =
                       (PVOID)(UINTN)ALIGN_RANGE_UP((UINTN)Buffer,
                                                    EHCI_DEBUG_LINK_ALIGNMENT);

        Device->Data.ReclamationQueuePhysical =
                                     ALIGN_RANGE_UP(BufferPhysical,
                                                    EHCI_DEBUG_LINK_ALIGNMENT);

        Buffer = Device->Data.ReclamationQueue + 1;
        Buffer = (PVOID)(UINTN)ALIGN_RANGE_UP((UINTN)Buffer,
                                              EHCI_DEBUG_LINK_ALIGNMENT);

        BufferPhysical = Device->Data.ReclamationQueuePhysical +
                         sizeof(EHCI_QUEUE_HEAD);

        BufferPhysical = ALIGN_RANGE_UP(BufferPhysical,
                                        EHCI_DEBUG_LINK_ALIGNMENT);

        Device->Data.EndQueue = Buffer;
        Device->Data.EndQueuePhysical = BufferPhysical;
        Buffer = Device->Data.EndQueue + 1;
        Buffer = (PVOID)(UINTN)ALIGN_RANGE_UP((UINTN)Buffer,
                                              EHCI_DEBUG_LINK_ALIGNMENT);

        BufferPhysical = Device->Data.EndQueuePhysical +
                         sizeof(EHCI_QUEUE_HEAD);

        BufferPhysical = ALIGN_RANGE_UP(BufferPhysical,
                                        EHCI_DEBUG_LINK_ALIGNMENT);

        //
        // Compute the start of the second transfer.
        //

        NextBuffer = BufferPhysical +
                     ((BufferPhysicalEnd - BufferPhysical) / 2);

        NextBuffer = ALIGN_RANGE_UP(NextBuffer, EHCI_DEBUG_LINK_ALIGNMENT);

        //
        // Fill in the first transfer.
        //

        Device->Transfers[0].Queue = Buffer;
        Device->Transfers[0].QueuePhysical = BufferPhysical;
        Buffer = (PVOID)(UINTN)ALIGN_RANGE_UP(
                                      (UINTN)Buffer + sizeof(EHCI_QUEUE_HEAD),
                                       EHCI_DEBUG_LINK_ALIGNMENT);

        BufferPhysical =
                       ALIGN_RANGE_UP(BufferPhysical + sizeof(EHCI_QUEUE_HEAD),
                                      EHCI_DEBUG_LINK_ALIGNMENT);

        Device->Transfers[0].Buffer = Buffer;
        Device->Transfers[0].BufferPhysical = BufferPhysical;
        Device->Transfers[0].BufferSize = NextBuffer - BufferPhysical;
        Device->Transfers[0].Allocated = FALSE;
        Buffer += Device->Transfers[0].BufferSize;
        BufferPhysical += Device->Transfers[0].BufferSize;

        //
        // Fill in the second transfer.
        //

        Device->Transfers[1].Queue = Buffer;
        Device->Transfers[1].QueuePhysical = BufferPhysical;
        Buffer = (PVOID)(UINTN)ALIGN_RANGE_UP(
                                       (UINTN)Buffer + sizeof(EHCI_QUEUE_HEAD),
                                       EHCI_DEBUG_LINK_ALIGNMENT);

        BufferPhysical =
                       ALIGN_RANGE_UP(BufferPhysical + sizeof(EHCI_QUEUE_HEAD),
                                      EHCI_DEBUG_LINK_ALIGNMENT);

        Device->Transfers[1].Buffer = Buffer;
        Device->Transfers[1].BufferPhysical = BufferPhysical;
        Device->Transfers[1].BufferSize = BufferPhysicalEnd - BufferPhysical;
        Device->Transfers[1].Allocated = FALSE;

        //
        // Initialize the reclamation queue and the end queue to point to each
        // other in a tight little circle. Transfer descriptors get added to
        // their transfer queue, and then the transfer queue gets added after
        // the reclamation list but before the end queue. This way the debugger
        // can add or remove queue heads without worrying that the real EHCI
        // driver is in the process of removing a queue.
        //

        Device->Data.ReclamationQueue->HorizontalLink =
                                       (ULONG)(Device->Data.EndQueuePhysical) |
                                        EHCI_LINK_TYPE_QUEUE_HEAD;

        Device->Data.ReclamationQueue->Destination =
                                                   EHCI_QUEUE_RECLAMATION_HEAD;

        Device->Data.ReclamationQueue->SplitInformation =
                                      EHCI_QUEUE_1_TRANSACTION_PER_MICRO_FRAME;

        Device->Data.ReclamationQueue->TransferOverlay.NextTransfer =
                                                           EHCI_LINK_TERMINATE;

        Device->Data.ReclamationQueue->TransferOverlay.AlternateNextTransfer =
                                                           EHCI_LINK_TERMINATE;

        Device->Data.ReclamationQueue->TransferOverlay.Token =
                                                   EHCI_TRANSFER_STATUS_HALTED;

        Device->Data.EndQueue->HorizontalLink =
                               (ULONG)(Device->Data.ReclamationQueuePhysical) |
                                EHCI_LINK_TYPE_QUEUE_HEAD;

        Device->Data.EndQueue->Destination = 0;
        Device->Data.EndQueue->SplitInformation =
                                      EHCI_QUEUE_1_TRANSACTION_PER_MICRO_FRAME;

        Device->Data.EndQueue->TransferOverlay.NextTransfer =
                                                           EHCI_LINK_TERMINATE;

        Device->Data.EndQueue->TransferOverlay.AlternateNextTransfer =
                                                           EHCI_LINK_TERMINATE;

        Device->Data.EndQueue->TransferOverlay.Token =
                                                   EHCI_TRANSFER_STATUS_HALTED;
    }

    if (Device->HandoffComplete == FALSE) {
        Status = KdEhciResetController(Device);
        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }
    }

    Status = STATUS_SUCCESS;

InitializeEnd:
    return Status;
}

KSTATUS
KdEhciGetRootHubStatus (
    PVOID Context,
    ULONG PortIndex,
    PULONG PortStatus
    )

/*++

Routine Description:

    This routine queries the host controller for the status of a root hub port.

Arguments:

    Context - Supplies a pointer to the device context.

    PortIndex - Supplies the zero-based port number to query.

    PortStatus - Supplies a pointer where the port status is returned.

Return Value:

    STATUS_SUCCESS if the hub status was successfully queried.

    STATUS_OUT_OF_BOUNDS if the controller port index is out of range.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

--*/

{

    PEHCI_DEBUG_DEVICE Controller;
    USHORT HardwareStatus;

    Controller = (PEHCI_DEBUG_DEVICE)Context;
    if (PortIndex >= Controller->PortCount) {
        return STATUS_OUT_OF_BOUNDS;
    }

    HardwareStatus = EHCI_READ_PORT_REGISTER(Controller, PortIndex);

    //
    // Set the corresponding software bits. If the owner bit is set,
    // pretend like there's nothing here.
    //

    *PortStatus = 0;
    if (((HardwareStatus & EHCI_PORT_CONNECT_STATUS) != 0) &&
        ((HardwareStatus & EHCI_PORT_OWNER) == 0)) {

        *PortStatus |= DEBUG_USB_PORT_STATUS_CONNECTED;

        //
        // If the port is presenting a K state, then it's a low speed.
        // Otherwise, assume that if it hasn't yet been passed off to the
        // companion controller that it's a high speed device. If it turns
        // out to be a full speed device, it will eventually get
        // disconnected from here and passed on to the companion controller.
        //

        if ((HardwareStatus & EHCI_PORT_LINE_STATE_MASK) ==
            EHCI_PORT_LINE_STATE_K) {

            *PortStatus |= DEBUG_USB_PORT_STATUS_LOW_SPEED;

            //
            // Release ownership of this device.
            //

            HardwareStatus |= EHCI_PORT_OWNER;
            EHCI_WRITE_PORT_REGISTER(Controller, PortIndex, HardwareStatus);
            HardwareStatus = 0;

        } else {
            *PortStatus |= DEBUG_USB_PORT_STATUS_HIGH_SPEED;
        }
    }

    if ((HardwareStatus & EHCI_PORT_ENABLE) != 0) {
        *PortStatus |= DEBUG_USB_PORT_STATUS_ENABLED;
    }

    if ((HardwareStatus & EHCI_PORT_RESET) != 0) {
        *PortStatus |= DEBUG_USB_PORT_STATUS_RESET;
    }

    if ((HardwareStatus & EHCI_PORT_OVER_CURRENT_ACTIVE) != 0) {
        *PortStatus |= DEBUG_USB_PORT_STATUS_OVER_CURRENT;
    }

    //
    // Acknowledge the over current change bit if it is set.
    //

    if ((HardwareStatus & EHCI_PORT_OVER_CURRENT_CHANGE) != 0) {
        EHCI_WRITE_PORT_REGISTER(Controller, PortIndex, HardwareStatus);
    }

    //
    // Acknowledge the port connection status change in the hardware.
    //

    if ((HardwareStatus & EHCI_PORT_CONNECT_STATUS_CHANGE) != 0) {

        //
        // If the port is not in the middle of a reset, clear the connect
        // status change bit in the hardware by setting it to 1. Resets
        // clear the connect status changed bit.
        //

        if ((HardwareStatus & EHCI_PORT_RESET) == 0) {
            EHCI_WRITE_PORT_REGISTER(Controller, PortIndex, HardwareStatus);
        }
    }

    return STATUS_SUCCESS;
}

KSTATUS
KdEhciSetRootHubStatus (
    PVOID Context,
    ULONG PortIndex,
    ULONG PortStatus
    )

/*++

Routine Description:

    This routine sets the host controller for the status of a root hub port.

Arguments:

    Context - Supplies a pointer to the device context.

    PortIndex - Supplies the zero-based port number to set.

    PortStatus - Supplies the port status to set.

Return Value:

    STATUS_SUCCESS if the hub status was successfully queried.

    STATUS_OUT_OF_BOUNDS if the controller port index is out of range.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

--*/

{

    PEHCI_DEBUG_DEVICE Controller;
    USHORT HardwareStatus;
    USHORT OriginalHardwareStatus;

    Controller = (PEHCI_DEBUG_DEVICE)Context;
    if (PortIndex >= Controller->PortCount) {
        return STATUS_OUT_OF_BOUNDS;
    }

    OriginalHardwareStatus = EHCI_READ_PORT_REGISTER(Controller, PortIndex);
    HardwareStatus = OriginalHardwareStatus;

    //
    // Leave the port alone if it's not owned by EHCI and there isn't an
    // active reset.
    //

    if (((HardwareStatus & EHCI_PORT_OWNER) != 0) &&
        ((PortStatus & DEBUG_USB_PORT_STATUS_RESET) == 0)) {

        return STATUS_SUCCESS;
    }

    //
    // Clear out the bits that may potentially be adjusted.
    //

    HardwareStatus &= ~(EHCI_PORT_ENABLE | EHCI_PORT_RESET |
                        EHCI_PORT_SUSPEND | EHCI_PORT_INDICATOR_MASK |
                        EHCI_PORT_OWNER);

    //
    // Set the hardware bits according to what's passed in.
    //

    if ((PortStatus & DEBUG_USB_PORT_STATUS_ENABLED) != 0) {
        HardwareStatus |= EHCI_PORT_ENABLE |
                          EHCI_PORT_INDICATOR_GREEN |
                          EHCI_PORT_POWER;
    }

    //
    // The EHCI spec says that whenever the reset bit is set, the enable
    // bit must be cleared. If the port is high speed, the enable bit will
    // be set automatically once the reset completes.
    //

    if ((PortStatus & DEBUG_USB_PORT_STATUS_RESET) != 0) {
        HardwareStatus |= EHCI_PORT_RESET;
        HardwareStatus &= ~EHCI_PORT_ENABLE;
    }

    //
    // Suspend the port if requested.
    //

    if ((PortStatus & DEBUG_USB_PORT_STATUS_SUSPENDED) != 0) {
        HardwareStatus |= EHCI_PORT_SUSPEND;
    }

    //
    // Write out the new value if it is different than the old one.
    //

    if (HardwareStatus != OriginalHardwareStatus) {
        EHCI_WRITE_PORT_REGISTER(Controller, PortIndex, HardwareStatus);
    }

    //
    // If reset was set, wait the required amount of time and then clear
    // the reset bit, as if this were a hub and it was cleared
    // automatically.
    //

    if ((HardwareStatus & EHCI_PORT_RESET) != 0) {
        KdEhciStall(Context, 20);
        HardwareStatus = EHCI_READ_PORT_REGISTER(Controller, PortIndex);
        HardwareStatus &= ~EHCI_PORT_RESET;
        EHCI_WRITE_PORT_REGISTER(Controller, PortIndex, HardwareStatus);

        //
        // Wait a further 5ms (the EHCI spec says the host controller has
        // to have it done in 2ms), and if the port is not enabled, then
        // it's a full speed device, and should be handed off to the
        // companion controller.
        //

        KdEhciStall(Context, 5);
        HardwareStatus = EHCI_READ_PORT_REGISTER(Controller, PortIndex);
        if ((HardwareStatus & EHCI_PORT_ENABLE) == 0) {
            HardwareStatus |= EHCI_PORT_OWNER;
            EHCI_WRITE_PORT_REGISTER(Controller, PortIndex, HardwareStatus);
        }
    }

    return STATUS_SUCCESS;
}

KSTATUS
KdEhciSetupTransfer (
    PVOID Context,
    PDEBUG_USB_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine allocates a buffer and initializes the given USB transfer.

Arguments:

    Context - Supplies a pointer to the device context.

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

    ULONG AlignedSize;
    ULONG BufferIndex;
    ULONG BufferPhysical;
    PEHCI_DEBUG_TRANSFER_DESCRIPTOR Descriptor;
    ULONG DescriptorCount;
    ULONG DescriptorIndex;
    ULONG DescriptorPhysical;
    PEHCI_DEBUG_TRANSFER_DESCRIPTOR Descriptors;
    PEHCI_DEBUG_DEVICE Device;
    PEHCI_DEBUG_TRANSFER EhciTransfer;
    ULONG EndMask;
    PDEBUG_USB_ENDPOINT Endpoint;
    ULONG Length;
    ULONG NakReloadCount;
    ULONG Offset;
    ULONG StartMicroFrame;
    ULONG StatusLink;
    ULONG Token;
    ULONG TransferIndex;

    Device = (PEHCI_DEBUG_DEVICE)Context;
    Endpoint = Transfer->Endpoint;
    if ((Endpoint == NULL) || (Endpoint->MaxPacketSize == 0)) {
        return STATUS_INVALID_PARAMETER;
    }

    if ((Transfer->Direction != DebugUsbTransferDirectionIn) &&
        (Transfer->Direction != DebugUsbTransferDirectionOut)) {

        return STATUS_INVALID_PARAMETER;
    }

    EhciTransfer = NULL;
    for (TransferIndex = 0;
         TransferIndex < EHCI_DEBUG_TRANSFER_COUNT;
         TransferIndex += 1) {

        EhciTransfer = &(Device->Transfers[TransferIndex]);
        if (EhciTransfer->Allocated == FALSE) {
            break;
        }
    }

    if (TransferIndex == EHCI_DEBUG_TRANSFER_COUNT) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if (Transfer->Length >= EhciTransfer->BufferSize) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if (Endpoint->Type == DebugUsbTransferTypeIsochronous) {
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Start by filling out the transfer queue head.
    //
    // Set the NAK reload count to the maximum for control and bulk transfers.
    // Interrupt and isochronous transfers must have the NAK reload count set
    // to zero.
    //

    if ((Endpoint->Type == DebugUsbTransferTypeControl) ||
        (Endpoint->Type == DebugUsbTransferTypeBulk)) {

        NakReloadCount = EHCI_QUEUE_DEFAULT_NAK_RELOAD_COUNT;

    } else {
        NakReloadCount = 0;
    }

    //
    // Initialize the hardware queue entry. Notice one thing conspicuously
    // missing is the device address. This gets initialized to zero, and fixed
    // up during transfer submissions (when the device is potentially moved off
    // address zero).
    //

    RtlZeroMemory(EhciTransfer->Queue, sizeof(EHCI_QUEUE_HEAD));
    EhciTransfer->Queue->HorizontalLink =
                                 Device->Data.ReclamationQueue->HorizontalLink;

    EhciTransfer->Queue->Destination =
           (NakReloadCount << EHCI_QUEUE_NAK_RELOAD_COUNT_SHIFT) |
           ((Endpoint->MaxPacketSize << EHCI_QUEUE_MAX_PACKET_LENGTH_SHIFT) &
            EHCI_QUEUE_MAX_PACKET_LENGTH_MASK) |
           ((Endpoint->EndpointNumber & DEBUG_USB_ENDPOINT_ADDRESS_MASK) <<
            EHCI_QUEUE_ENDPOINT_SHIFT) |
           (Endpoint->DeviceAddress & EHCI_QUEUE_DEVICE_ADDRESS_MASK);

    switch (Endpoint->Speed) {
    case DebugUsbDeviceSpeedLow:
        EhciTransfer->Queue->Destination |= EHCI_QUEUE_LOW_SPEED;
        break;

    case DebugUsbDeviceSpeedFull:
        EhciTransfer->Queue->Destination |= EHCI_QUEUE_FULL_SPEED;
        break;

    case DebugUsbDeviceSpeedHigh:
        EhciTransfer->Queue->Destination |= EHCI_QUEUE_HIGH_SPEED;
        break;

    default:
        return STATUS_INVALID_PARAMETER;
    }

    //
    // All control transfers handle the data toggle without hardware
    // assistance. Non-high speed control transfers must have the control
    // endpoint flag set. High speed control transfers should not have said
    // flag set.
    //

    if (Endpoint->Type == DebugUsbTransferTypeControl) {
        EhciTransfer->Queue->Destination |=
                                EHCI_QUEUE_USE_TRANSFER_DESCRIPTOR_DATA_TOGGLE;

        if (Endpoint->Speed != DebugUsbDeviceSpeedHigh) {
            EhciTransfer->Queue->Destination |= EHCI_QUEUE_CONTROL_ENDPOINT;
        }
    }

    EhciTransfer->Queue->SplitInformation =
                                      EHCI_QUEUE_1_TRANSACTION_PER_MICRO_FRAME;

    if ((Endpoint->Speed == DebugUsbDeviceSpeedLow) ||
        (Endpoint->Speed == DebugUsbDeviceSpeedFull)) {

        if ((Endpoint->HubAddress == 0) ||
            (Endpoint->HubPort == 0)) {

            return STATUS_INVALID_PARAMETER;
        }

        EhciTransfer->Queue->SplitInformation |=
                      ((Endpoint->HubPort << EHCI_QUEUE_PORT_NUMBER_SHIFT) &
                       EHCI_QUEUE_PORT_NUMBER_MASK) |
                      ((Endpoint->HubAddress << EHCI_QUEUE_HUB_ADDRESS_SHIFT) &
                       EHCI_QUEUE_HUB_ADDRESS_MASK);

        if (Endpoint->Type == DebugUsbTransferTypeInterrupt) {

            //
            // Make a weak attempt at spreading out these transfers throughout
            // micro frames. Only start in 0-4 to avoid dealing with Frame Split
            // Transaction Nodes.
            //

            StartMicroFrame = Endpoint->EndpointNumber & 0x3;

            //
            // Isochronous OUT endpoints don't use complete splits, but
            // interrupt and other endpoints usually skip a microframe and then
            // issue complete splits for the next three.
            //

            if ((Endpoint->Type == DebugUsbTransferTypeIsochronous) &&
                (Endpoint->Direction == DebugUsbTransferDirectionOut)) {

                EndMask = 0;

            } else {
                EndMask = (1 << (StartMicroFrame + 2)) |
                          (1 << (StartMicroFrame + 3)) |
                          (1 << (StartMicroFrame + 4));
            }

            EhciTransfer->Queue->SplitInformation |=
                                          (EndMask <<
                                           EHCI_QUEUE_SPLIT_COMPLETION_SHIFT) |
                                          (1 << StartMicroFrame);
        }

    } else {

        //
        // Make a weak attempt at spreading the transfers throughout micro-
        // frames.
        //

        if (Endpoint->Type == DebugUsbTransferTypeInterrupt) {
            EhciTransfer->Queue->SplitInformation |=
                                      (1 << (Endpoint->EndpointNumber & 0x7));
        }
    }

    //
    // Next fill out the transfer descriptors. If it's a control transfer, then
    // theres a transfer descriptor specifically for the setup packet (the
    // first eight bytes of the transfer buffer) and a transfer descriptor
    // specifically for the status phase with zero size.
    //

    AlignedSize = ALIGN_RANGE_UP(sizeof(EHCI_DEBUG_TRANSFER_DESCRIPTOR),
                                 EHCI_DEBUG_LINK_ALIGNMENT);

    Descriptors = EhciTransfer->Buffer;
    if (Endpoint->Type == DebugUsbTransferTypeControl) {
        if (Transfer->Length < DEBUG_USB_SETUP_PACKET_SIZE) {
            return STATUS_INVALID_PARAMETER;
        }

        //
        // Get the inner number of descriptors, but always round up.
        //

        DescriptorCount = (Transfer->Length - DEBUG_USB_SETUP_PACKET_SIZE +
                           Endpoint->MaxPacketSize - 1) /
                          Endpoint->MaxPacketSize;

        DescriptorCount += 2;

    } else {
        DescriptorCount = (Transfer->Length + Endpoint->MaxPacketSize - 1) /
                          Endpoint->MaxPacketSize;

        //
        // Add one for a transfer descriptor that just does a zero length
        // packet and stops for shorts.
        //

        DescriptorCount += 1;
    }

    StatusLink = EhciTransfer->BufferPhysical +
                 ((DescriptorCount - 1) * AlignedSize);

    //
    // If the remaining buffer size after the transfer descriptors have been
    // carved out is too small, then bail.
    //

    if (Transfer->Length >
        (EhciTransfer->BufferSize -
         (DescriptorCount * AlignedSize))) {

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Transfer->Buffer = EhciTransfer->Buffer +
                       (DescriptorCount * AlignedSize);

    Transfer->BufferPhysicalAddress = EhciTransfer->BufferPhysical +
                                      (DescriptorCount * AlignedSize);

    Transfer->HostDescriptorCount = DescriptorCount;
    BufferPhysical = (ULONG)(Transfer->BufferPhysicalAddress);
    DescriptorPhysical = (ULONG)(EhciTransfer->BufferPhysical);
    EhciTransfer->Queue->CurrentTransferDescriptorLink = 0;
    EhciTransfer->Queue->TransferOverlay.NextTransfer = DescriptorPhysical;
    EhciTransfer->Queue->TransferOverlay.AlternateNextTransfer =
                                                           EHCI_LINK_TERMINATE;

    EhciTransfer->Queue->TransferOverlay.Token = 0;

    //
    // Fill out each transfer descriptor.
    //

    Offset = 0;
    for (DescriptorIndex = 0;
         DescriptorIndex < DescriptorCount;
         DescriptorIndex += 1) {

        Descriptor = (PEHCI_DEBUG_TRANSFER_DESCRIPTOR)(((UINTN)Descriptors) +
                                                       (DescriptorIndex *
                                                        AlignedSize));

        Token = EHCI_TRANSFER_3_ERRORS_ALLOWED | EHCI_TRANSFER_STATUS_ACTIVE;

        //
        // The first packet in a control transfer is our friend the setup
        // packet.
        //

        if ((Endpoint->Type == DebugUsbTransferTypeControl) &&
            (DescriptorIndex == 0)) {

            Length = DEBUG_USB_SETUP_PACKET_SIZE;
            Token |= EHCI_TRANSFER_PID_CODE_SETUP;
            Endpoint->DataToggle = FALSE;

        //
        // The last packet in a control transfer is the status phase. It has
        // the opposite direction of the transfer itself.
        //

        } else if ((Endpoint->Type == DebugUsbTransferTypeControl) &&
                   (DescriptorIndex == DescriptorCount - 1)) {

            Length = 0;
            StatusLink = EHCI_LINK_TERMINATE;
            Endpoint->DataToggle = TRUE;
            if (Transfer->Direction == DebugUsbTransferDirectionIn) {
                Token |= EHCI_TRANSFER_PID_CODE_OUT;

            } else {
                Token |= EHCI_TRANSFER_PID_CODE_IN;
            }

        //
        // This is a normal packet.
        //

        } else {
            if (Transfer->Direction == DebugUsbTransferDirectionIn) {
                Token |= EHCI_TRANSFER_PID_CODE_IN;

            } else {
                Token |= EHCI_TRANSFER_PID_CODE_OUT;
            }

            Length = Endpoint->MaxPacketSize;
            if ((Transfer->Length - Offset) < Length) {
                Length = Transfer->Length - Offset;
            }

            //
            // If this is the last transfer, it's just a stub halted transfer
            // descriptor on the end.
            //

            if (DescriptorIndex == (DescriptorCount - 1)) {
                StatusLink = EHCI_LINK_TERMINATE;
                Token = EHCI_TRANSFER_STATUS_HALTED;
            }
        }

        Token |= (Length << EHCI_TRANSFER_TOTAL_BYTES_SHIFT);
        Descriptor->Descriptor.Token = Token;

        //
        // Set up the link pointers of the transfer descriptor.
        //

        if (DescriptorIndex == (DescriptorCount - 1)) {
            Descriptor->Descriptor.NextTransfer = EHCI_LINK_TERMINATE;

        } else {
            Descriptor->Descriptor.NextTransfer = DescriptorPhysical +
                                                  AlignedSize;
        }

        Descriptor->Descriptor.AlternateNextTransfer = StatusLink;
        if (Transfer->Endpoint->Type == DebugUsbTransferTypeIsochronous) {
            return STATUS_NOT_SUPPORTED;

        //
        // If the transfer is not isochronous, set the data toggle bit.
        //

        } else {
            if (Endpoint->DataToggle != FALSE) {
                if (Endpoint->Type == DebugUsbTransferTypeControl) {
                    Descriptor->Descriptor.Token |= EHCI_TRANSFER_DATA_TOGGLE;
                }

                Endpoint->DataToggle = FALSE;

                //
                // Set the overlay too.
                //

                if (DescriptorIndex == 0) {
                    EhciTransfer->Queue->TransferOverlay.Token |=
                                                     EHCI_TRANSFER_DATA_TOGGLE;
                }

            } else {
                Endpoint->DataToggle = TRUE;
            }
        }

        for (BufferIndex = 0;
             BufferIndex < EHCI_TRANSFER_POINTER_COUNT;
             BufferIndex += 1) {

            Descriptor->Descriptor.BufferPointer[BufferIndex] = 0;
            Descriptor->Descriptor.BufferAddressHigh[BufferIndex] = 0;
        }

        Descriptor->Descriptor.BufferPointer[0] = BufferPhysical + Offset;
        Descriptor->TransferLength = Length;
        DescriptorPhysical += AlignedSize;
        Offset += Length;
    }

    Transfer->Status = STATUS_NOT_STARTED;
    Transfer->LengthTransferred = 0;
    Transfer->HostContext = EhciTransfer;
    EhciTransfer->Allocated = TRUE;
    EhciTransfer->CheckIndex = 0;
    return STATUS_SUCCESS;
}

KSTATUS
KdEhciSubmitTransfer (
    PVOID Context,
    PDEBUG_USB_TRANSFER Transfer,
    BOOL WaitForCompletion
    )

/*++

Routine Description:

    This routine submits a previously set up USB transfer.

Arguments:

    Context - Supplies a pointer to the device context.

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

    PEHCI_DEBUG_DEVICE Device;
    PEHCI_DEBUG_TRANSFER EhciTransfer;
    KSTATUS Status;
    ULONG WaitedTime;

    Device = (PEHCI_DEBUG_DEVICE)Context;
    EhciTransfer = Transfer->HostContext;

    //
    // Try to detect if the caller is submitting a transfer that was never
    // set up.
    //

    if ((EhciTransfer->Allocated == FALSE) ||
        (Transfer->HostDescriptorCount == 0)) {

        return STATUS_INVALID_PARAMETER;
    }

    //
    // Submitting is very easy, just set the horizontal link of the reclamation
    // queue to the new transfer queue.
    //

    EhciTransfer->Queue->HorizontalLink =
                                 Device->Data.ReclamationQueue->HorizontalLink;

    HlWriteRegister32(
             &(Device->Data.ReclamationQueue->HorizontalLink),
             (ULONG)(EhciTransfer->QueuePhysical) | EHCI_LINK_TYPE_QUEUE_HEAD);

    Status = STATUS_SUCCESS;
    WaitedTime = 0;
    Transfer->Status = STATUS_MORE_PROCESSING_REQUIRED;
    if (WaitForCompletion != FALSE) {
        while (WaitedTime < EHCI_SYNCHRONOUS_TIMEOUT) {
            Status = KdEhciCheckTransfer(Context, Transfer);
            if (KSUCCESS(Status)) {
                break;

            } else if (Status != STATUS_MORE_PROCESSING_REQUIRED) {
                break;
            }

            Status = KdEhciStall(Context, 1);
            if (!KSUCCESS(Status)) {
                break;
            }

            WaitedTime += 1;
        }

        ASSERT(Status != STATUS_NOT_STARTED);

        Transfer->Status = Status;
    }

    return Status;
}

KSTATUS
KdEhciCheckTransfer (
    PVOID Context,
    PDEBUG_USB_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine checks on the completion status of a transfer.

Arguments:

    Context - Supplies a pointer to the device context.

    Transfer - Supplies a pointer to the transfer to check.

Return Value:

    STATUS_SUCCESS if the hub status was successfully queried.

    STATUS_INVALID_PARAMETER if the given transfer was not initialized
    correctly.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

    STATUS_MORE_PROCESSING_REQUIRED if the transfer is still in progress.

--*/

{

    PEHCI_DEBUG_DEVICE Controller;
    PEHCI_DEBUG_TRANSFER_DESCRIPTOR Descriptor;
    ULONG DescriptorCount;
    ULONG DescriptorIndex;
    PVOID Descriptors;
    ULONG DescriptorSize;
    PEHCI_DEBUG_TRANSFER EhciTransfer;
    ULONG LengthTransferred;
    BOOL Shorted;
    KSTATUS Status;
    ULONG Token;

    Controller = Context;
    EhciTransfer = Transfer->HostContext;
    if (EhciTransfer == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Try to detect if the caller is checking on a transfer that was never set
    // up, never submitted, or already completed.
    //

    if ((EhciTransfer->Allocated == FALSE) ||
        (Transfer->HostDescriptorCount == 0)) {

        return STATUS_INVALID_PARAMETER;
    }

    if (Transfer->Status != STATUS_MORE_PROCESSING_REQUIRED) {
        return STATUS_NOT_READY;
    }

    if ((EHCI_READ_REGISTER(Controller, EhciRegisterUsbStatus) &
         EHCI_STATUS_HALTED) != 0) {

        return STATUS_DEVICE_IO_ERROR;
    }

    Descriptors = EhciTransfer->Buffer;
    DescriptorSize = ALIGN_RANGE_UP(sizeof(EHCI_DEBUG_TRANSFER_DESCRIPTOR),
                                    EHCI_DEBUG_LINK_ALIGNMENT);

    DescriptorCount = Transfer->HostDescriptorCount;
    Status = STATUS_SUCCESS;
    Shorted = FALSE;
    for (DescriptorIndex = EhciTransfer->CheckIndex;
         DescriptorIndex < DescriptorCount;
         DescriptorIndex += 1) {

        Descriptor = (PEHCI_DEBUG_TRANSFER_DESCRIPTOR)(Descriptors +
                                                       (DescriptorIndex *
                                                        DescriptorSize));

        Token = Descriptor->Descriptor.Token;
        if ((Token & EHCI_TRANSFER_STATUS_ACTIVE) == 0) {
            LengthTransferred = Descriptor->TransferLength -
                                ((Token & EHCI_TRANSFER_TOTAL_BYTES_MASK) >>
                                 EHCI_TRANSFER_TOTAL_BYTES_SHIFT);

            Transfer->LengthTransferred += LengthTransferred;

            //
            // If error bits were set, it's curtains for this transfer. A
            // halted error is first in line even if another bit (e.g. Babble)
            // is set, because the driver may want to clear the halted state.
            //

            if ((Token & EHCI_TRANSFER_ERROR_MASK) != 0) {
                Transfer->Status = STATUS_DEVICE_IO_ERROR;
                if ((Token & EHCI_TRANSFER_STATUS_HALTED) != 0) {
                    Transfer->Endpoint->Halted = TRUE;

                //
                // If it was a CRC/timeout error, assume it was a timeout and
                // drop a hint in the transfer status.
                //

                } else if ((Token & EHCI_TRANSFER_TRANSACTION_ERROR) != 0) {
                    Transfer->Status = STATUS_TIMEOUT;
                }
            }

            //
            // If it's an IN transfer that came back with less than asked, then
            // the transfer was shorted, and other descriptors should not
            // expect to complete.
            //

            if ((Transfer->Direction == DebugUsbTransferDirectionIn) &&
                (LengthTransferred < Descriptor->TransferLength) &&
                (DescriptorIndex != (Transfer->HostDescriptorCount - 1))) {

                Shorted = TRUE;
            }

        //
        // This transfer is still active, stop looking.
        //

        } else {
            Status = STATUS_MORE_PROCESSING_REQUIRED;
            break;
        }

        //
        // If the transfer was shorted, move directly to the last transfer.
        //

        if (Shorted != FALSE) {
            DescriptorIndex = DescriptorCount - 2;
            Shorted = FALSE;
        }
    }

    EhciTransfer->CheckIndex = DescriptorIndex;
    return Status;
}

KSTATUS
KdEhciRetireTransfer (
    PVOID Context,
    PDEBUG_USB_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine retires an EHCI USB transfer. This frees the buffer allocated
    during setup.

Arguments:

    Context - Supplies a pointer to the device context.

    Transfer - Supplies a pointer to the transfer to retire.

Return Value:

    STATUS_SUCCESS if the hub status was successfully queried.

    STATUS_INVALID_PARAMETER if the given transfer was not initialized
    correctly.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

--*/

{

    ULONG CommandRegister;
    PEHCI_DEBUG_DEVICE Device;
    PEHCI_DEBUG_TRANSFER EhciTransfer;
    ULONG Index;
    ULONG OriginalUsbStatus;
    ULONG UsbStatus;

    Device = (PEHCI_DEBUG_DEVICE)Context;
    EhciTransfer = Transfer->HostContext;

    //
    // Try to detect if the caller is submitting a transfer that was never
    // set up.
    //

    if ((EhciTransfer->Allocated == FALSE) ||
        (Transfer->HostDescriptorCount == 0)) {

        return STATUS_INVALID_PARAMETER;
    }

    //
    // If the transfer was never submitted, then just skip to the end in order
    // to reclaim it.
    //

    if (Transfer->Status == STATUS_NOT_STARTED) {
        goto RetireTransferEnd;
    }

    //
    // If the reclamation queue points at this queue, point it away.
    //

    if (Device->Data.ReclamationQueue->HorizontalLink ==
        (EhciTransfer->QueuePhysical | EHCI_LINK_TYPE_QUEUE_HEAD)) {

        HlWriteRegister32(&(Device->Data.ReclamationQueue->HorizontalLink),
                          EhciTransfer->Queue->HorizontalLink);
    }

    //
    // If any of the other transfers point at this queue, point them away.
    //

    for (Index = 0; Index < EHCI_DEBUG_TRANSFER_COUNT; Index += 1) {
        if (Device->Transfers[Index].Queue->HorizontalLink ==
            (EhciTransfer->QueuePhysical | EHCI_LINK_TYPE_QUEUE_HEAD)) {

            HlWriteRegister32(&(Device->Transfers[Index].Queue->HorizontalLink),
                              EhciTransfer->Queue->HorizontalLink);
        }
    }

    //
    // Use the doorbell to ensure the hardware is not using the queue being
    // removed. If the async advance was requested but not yet pending, wait
    // for it to become set.
    //

    do {
        CommandRegister = EHCI_READ_REGISTER(Device, EhciRegisterUsbCommand);
        UsbStatus = EHCI_READ_REGISTER(Device, EhciRegisterUsbStatus);
        if ((UsbStatus & EHCI_STATUS_HALTED) != 0) {
            break;
        }

    } while ((CommandRegister & EHCI_COMMAND_INTERRUPT_ON_ASYNC_ADVANCE) != 0);

    //
    // If the async advance interrupt is already pending (from the real host
    // driver), clear it.
    //

    OriginalUsbStatus = EHCI_READ_REGISTER(Device, EhciRegisterUsbStatus);
    if ((OriginalUsbStatus & EHCI_STATUS_INTERRUPT_ON_ASYNC_ADVANCE) != 0) {
        EHCI_WRITE_REGISTER(Device,
                            EhciRegisterUsbStatus,
                            EHCI_STATUS_INTERRUPT_ON_ASYNC_ADVANCE);
    }

    CommandRegister = EHCI_READ_REGISTER(Device, EhciRegisterUsbCommand);
    CommandRegister |= EHCI_COMMAND_INTERRUPT_ON_ASYNC_ADVANCE;
    EHCI_WRITE_REGISTER(Device,
                        EhciRegisterUsbCommand,
                        CommandRegister);

    do {
        UsbStatus = EHCI_READ_REGISTER(Device, EhciRegisterUsbStatus);
        if ((UsbStatus & EHCI_STATUS_HALTED) != 0) {
            break;
        }

    } while ((UsbStatus & EHCI_STATUS_INTERRUPT_ON_ASYNC_ADVANCE) == 0);

    //
    // Clear the interrupt status only if the bit was not set originally.
    // If it was set originally, leave it alone so the real EHCI driver
    // receives it.
    //

    if ((OriginalUsbStatus & EHCI_STATUS_INTERRUPT_ON_ASYNC_ADVANCE) == 0) {
        EHCI_WRITE_REGISTER(Device,
                            EhciRegisterUsbStatus,
                            EHCI_STATUS_INTERRUPT_ON_ASYNC_ADVANCE);
    }

RetireTransferEnd:

    //
    // Figure out what the next data toggle should be based on what's in the
    // transfer overlay. This needs to be done even if the transfer was not
    // submitted.
    //

    Transfer->Endpoint->DataToggle = FALSE;
    if ((EhciTransfer->Queue->TransferOverlay.Token &
         EHCI_TRANSFER_DATA_TOGGLE) != 0) {

        Transfer->Endpoint->DataToggle = TRUE;
    }

    //
    // Whew, the transfer is out of there. "Free" it. Also clear fields out of
    // the transfer to try to foul up folks using the transfer after it was
    // freed.
    //

    Transfer->Buffer = NULL;
    Transfer->BufferPhysicalAddress = 0;
    Transfer->Length = 0;
    Transfer->Status = STATUS_NOT_STARTED;
    Transfer->LengthTransferred = 0;
    Transfer->HostContext = NULL;
    Transfer->HostDescriptorCount = 0;
    EhciTransfer->Allocated = FALSE;
    return STATUS_SUCCESS;
}

KSTATUS
KdEhciStall (
    PVOID Context,
    ULONG Milliseconds
    )

/*++

Routine Description:

    This routine burns time using the EHCI frame index register to mark time.

Arguments:

    Context - Supplies a pointer to the device context.

    Milliseconds - Supplies the number of milliseconds to stall for.

Return Value:

    STATUS_SUCCESS if the hub status was successfully queried.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

--*/

{

    PEHCI_DEBUG_DEVICE Controller;
    ULONG CurrentTime;
    ULONG EndTime;
    ULONG Frame;
    ULONG PreviousFrame;
    ULONG Status;

    Controller = (PEHCI_DEBUG_DEVICE)Context;
    CurrentTime = EHCI_READ_REGISTER(Controller, EhciRegisterFrameNumber);
    PreviousFrame = CurrentTime;
    EndTime = CurrentTime + (Milliseconds << 3);
    while (CurrentTime < EndTime) {
        Status = EHCI_READ_REGISTER(Controller, EhciRegisterUsbStatus);
        if ((Status & EHCI_STATUS_HALTED) != 0) {
            return STATUS_DEVICE_IO_ERROR;
        }

        Frame = EHCI_READ_REGISTER(Controller, EhciRegisterFrameNumber);

        //
        // If the frame number went up, accumulate time. If it appeared to
        // go down, it probably rolled over. To avoid miscalculating the max
        // value, just ignore the tick.
        //

        if (Frame > PreviousFrame) {
            CurrentTime += Frame - PreviousFrame;
        }

        PreviousFrame = Frame;
    }

    return STATUS_SUCCESS;
}

KSTATUS
KdEhciGetHandoffData (
    PVOID Context,
    PDEBUG_USB_HANDOFF_DATA HandoffData
    )

/*++

Routine Description:

    This routine returns the controller specific handoff data in preparation
    for the real USB driver taking over primary functionality.

Arguments:

    Context - Supplies a pointer to the device context.

    HandoffData - Supplies a pointer to the handoff data. The controller should
        fill in the host controller specific data and size fields.

Return Value:

    Status code.

--*/

{

    PEHCI_DEBUG_DEVICE Controller;

    Controller = (PEHCI_DEBUG_DEVICE)Context;
    HandoffData->HostData = &(Controller->Data);
    HandoffData->HostDataSize = sizeof(EHCI_DEBUG_HANDOFF_DATA);
    Controller->HandoffComplete = TRUE;
    return STATUS_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
KdEhciResetController (
    PEHCI_DEBUG_DEVICE Controller
    )

/*++

Routine Description:

    This routine resets and starts the EHCI controller.

Arguments:

    Controller - Supplies a pointer to the EHCI controller state of the
        controller to reset.

Return Value:

    Status code.

--*/

{

    ULONG CommandRegister;
    ULONG PhysicalAddress;
    ULONG PortIndex;
    ULONG PortStatusRegister;

    //
    // Reset the host controller and wait for the hardware to clear the bit,
    // which indicates that the reset is complete.
    //

    CommandRegister = EHCI_COMMAND_CONTROLLER_RESET;
    EHCI_WRITE_REGISTER(Controller, EhciRegisterUsbCommand, CommandRegister);
    do {

        //
        // AND in the hardware register to see if the bit has cleared.
        //

        CommandRegister &= EHCI_READ_REGISTER(Controller,
                                              EhciRegisterUsbCommand);

    } while (CommandRegister != 0);

    //
    // Clear the status register.
    //

    EHCI_WRITE_REGISTER(Controller, EhciRegisterUsbStatus, 0);

    //
    // Write the the segment selector to use the first 4GB of physical memory.
    //

    EHCI_WRITE_REGISTER(Controller, EhciRegisterSegmentSelector, 0);

    //
    // Disable interrupts, this is the debugger son.
    //

    EHCI_WRITE_REGISTER(Controller, EhciRegisterUsbInterruptEnable, 0);

    //
    // Write the asynchronous list base to the reclamation list head.
    //

    PhysicalAddress = (ULONG)Controller->Data.ReclamationQueuePhysical;
    EHCI_WRITE_REGISTER(Controller,
                        EhciRegisterAsynchronousListAddress,
                        PhysicalAddress);

    //
    // Write to the command register to start the controller.
    //

    CommandRegister = EHCI_COMMAND_INTERRUPT_EVERY_8_UFRAMES |
                      ECHI_COMMAND_ASYNC_PARK_ENABLE |
                      (3 << EHCI_COMMAND_PARK_COUNT_SHIFT) |
                      EHCI_COMMAND_ENABLE_ASYNC_SCHEDULE |
                      EHCI_COMMAND_1024_FRAME_LIST_ENTRIES |
                      EHCI_COMMAND_RUN;

    EHCI_WRITE_REGISTER(Controller, EhciRegisterUsbCommand, CommandRegister);

    //
    // Set the config flag, which switches all the ports to EHCI away from the
    // companion controllers.
    //

    EHCI_WRITE_REGISTER(Controller, EhciRegisterConfigured, 1);

    //
    // Fire up the ports.
    //

    for (PortIndex = 0; PortIndex < Controller->PortCount; PortIndex += 1) {
        PortStatusRegister = EHCI_READ_PORT_REGISTER(Controller, PortIndex);
        if ((PortStatusRegister & EHCI_PORT_POWER) == 0) {
            PortStatusRegister |= EHCI_PORT_POWER;
            EHCI_WRITE_PORT_REGISTER(Controller, PortIndex, PortStatusRegister);
        }
    }

    KdEhciStall(Controller, 20);
    return STATUS_SUCCESS;
}

