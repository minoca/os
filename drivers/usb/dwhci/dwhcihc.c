/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dwhcihc.c

Abstract:

    This module implements support for the DesignWare Hi-Speed USB 2.O
    On-The-Go (HS OTG) Host Controller.

    Copyright (C) 2004-2013 by Synopsis, Inc.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice,
       this list of conditions, and the following disclaimer, without
       modification.

    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.

    3. The names of the above-listed copyright holders may not be used to
       endorse or promote products derived from this software without specific
       prior written permission.

    This software is provided by the copyright holders and contributors "AS IS"
    and any express or implied warranties, including, by not limited to, the
    implied warranties or mechantability and fitness for a particular purpose
    are disclained. In no event shall the copyright owner or contributors be
    liable for any direct, indirect, incidental, special, exemplary, or
    consequential damages (including, but not limited to, procurement of
    substitue goods or services; loss of use, data, or profits; or business
    interruption) however caused and on any theory of liability, whether in
    contract, strict liability, or tort (including negligence or otherwise)
    arising in any way out of the use of this software, even if advised of the
    possibility of such damage.

Author:

    Chris Stevens 38-Mar-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/usb/usbhost.h>
#include "dwhci.h"

//
// --------------------------------------------------------------------- Macros
//

//
// These macros read from and write to a DWHCI host controller register. The
// first argument is a PDWHCI_CONTROLLER, the second is a DWHCI_REGISTER. The
// value on write is a ULONG.
//

#define DWHCI_READ_REGISTER(_Controller, _Register) \
    HlReadRegister32((_Controller)->RegisterBase + _Register)

#define DWHCI_WRITE_REGISTER(_Controller, _Register, _Value) \
    HlWriteRegister32((_Controller)->RegisterBase + _Register, _Value)

//
// These macros read from and write to a DWHCI channel register. The first
// argument is a PDWHCI_CONTROLLER, the second is a DWHCI_CHANNEL_REGISTER and
// the third is a ULONG for the channel index. The value on write is a ULONG.
//

#define DWHCI_READ_CHANNEL_REGISTER(_Controller, _Register, _Channel) \
    HlReadRegister32((_Controller)->RegisterBase +                    \
                     DwhciRegisterChannelBase +                       \
                     (DwhciChannelRegistersSize * _Channel) +         \
                     _Register)

#define DWHCI_WRITE_CHANNEL_REGISTER(_Controller, _Register, _Channel, _Value) \
    HlWriteRegister32(((_Controller)->RegisterBase +                           \
                       DwhciRegisterChannelBase +                              \
                       (DwhciChannelRegistersSize * _Channel) +                \
                       _Register),                                             \
                      _Value)

//
// This macro reads the frame number.
//

#define DWHCI_READ_FRAME_NUMBER(_Controller)                          \
    (((DWHCI_READ_REGISTER((_Controller), DwhciRegisterFrameNumber) & \
       DWHCI_FRAME_NUMBER_MASK) >>                                    \
      DWHCI_FRAME_NUMBER_SHIFT) &                                     \
     DWHCI_FRAME_NUMBER_MAX)

//
// This macro evaluates whether two frame numbers are in descending order,
// taking wrapping into account.
//

#define DWHCI_FRAME_GREATER_THAN_OR_EQUAL(_Frame1, _Frame2) \
    (((((_Frame1) - (_Frame2)) & DWHCI_FRAME_NUMBER_MAX) &  \
      DWHCI_FRAME_NUMBER_HIGH_BIT) == 0)

//
// This macro evaluates whether two frame numbers are in ascending order,
// taking wrapping into account.
//

#define DWHCI_FRAME_LESS_THAN(_Frame1, _Frame2)            \
    (((((_Frame1) - (_Frame2)) & DWHCI_FRAME_NUMBER_MAX) & \
      DWHCI_FRAME_NUMBER_HIGH_BIT) != 0)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum number of channels that can exist in the host controller.
//

#define DWHCI_MAX_CHANNELS 16

//
// Define the default start frame offset for periodic transfers
//

#define DWHCI_DEFAULT_FRAME_OFFSET 15

//
// Define the maximum number of errors allowed on a split transfer.
//

#define DWHCI_SPLIT_ERROR_MAX 3

//
// Define the maximum number of complete splits allowed.
//

#define DWHCI_COMPLETE_SPLIT_MAX 3

//
// Define the mask to OR onto each interrupt split's next frame.
//

#define DWHCI_INTERRUPT_SPLIT_FRAME_MASK 0x7

//
// Define the number of microframes per frame.
//

#define DWHCI_MICROFRAMES_PER_FRAME 8
#define DWHCI_MICROFRAMES_PER_FRAME_SHIFT 3

//
// Define the required alignment for DMA buffers.
//

#define DWHCI_DMA_ALIGNMENT 0x8

//
// Define the size of the control status buffer used as a bit bucket.
//

#define DWHCI_CONTROL_STATUS_BUFFER_SIZE 64

//
// Define the initial set of interrupts that the host controller is interested
// in.
//

#define DWHCI_INITIAL_CORE_INTERRUPT_MASK       \
    (DWHCI_CORE_INTERRUPT_DISCONNECT |          \
     DWHCI_CORE_INTERRUPT_PORT |                \
     DWHCI_CORE_INTERRUPT_HOST_CHANNEL)

//
// Define flags for DWHCI debugging.
//

#define DWHCI_DEBUG_FLAG_PORTS     0x1
#define DWHCI_DEBUG_FLAG_TRANSFERS 0x2

//
// Define the value for an invalid frame.
//

#define DWHCI_INVALID_FRAME 0xFFFF

//
// Define the size of the window in which complete splits must finish, in
// microframes. The start frame is recorded, and the start split actually
// executes in the next microframe (1). Then there is a rest microframe (2),
// followed by three microframes in which the complete split can finish (5).
//

#define DWHCI_SPLIT_NOT_YET_FRAME_WINDOW 5

//
// Define the DWHCI host controller revision that first handled automatic PING
// processing for bulk and control transfers.
//

#define DWHCI_AUTOMATIC_PING_REVISION_MININUM 0x4f54271a

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
DwhcipCreateEndpoint (
    PVOID HostControllerContext,
    PUSB_HOST_ENDPOINT_CREATION_REQUEST Endpoint,
    PVOID *EndpointContext
    );

VOID
DwhcipResetEndpoint (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    ULONG MaxPacketSize
    );

VOID
DwhcipDestroyEndpoint (
    PVOID HostControllerContext,
    PVOID EndpointContext
    );

KSTATUS
DwhcipCreateTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    ULONG MaxBufferSize,
    ULONG Flags,
    PVOID *TransferContext
    );

VOID
DwhcipDestroyTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PVOID TransferContext
    );

KSTATUS
DwhcipSubmitTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PUSB_TRANSFER_INTERNAL Transfer,
    PVOID TransferContext
    );

KSTATUS
DwhcipCancelTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PUSB_TRANSFER_INTERNAL Transfer,
    PVOID TransferContext
    );

KSTATUS
DwhcipGetRootHubStatus (
    PVOID HostControllerContext,
    PUSB_HUB_STATUS HubStatus
    );

KSTATUS
DwhcipSetRootHubStatus (
    PVOID HostControllerContext,
    PUSB_HUB_STATUS NewStatus
    );

RUNLEVEL
DwhcipAcquireControllerLock (
    PDWHCI_CONTROLLER Controller
    );

VOID
DwhcipReleaseControllerLock (
    PDWHCI_CONTROLLER Controller,
    RUNLEVEL OldRunLevel
    );

VOID
DwhcipInterruptServiceDpc (
    PDPC Dpc
    );

VOID
DwhcipProcessInterrupt (
    PVOID Context
    );

VOID
DwhcipProcessStartOfFrameInterrupt (
    PDWHCI_CONTROLLER Controller
    );

VOID
DwhcipSaveChannelInterrupts (
    PDWHCI_CONTROLLER Controller
    );

VOID
DwhcipProcessChannelInterrupt (
    PDWHCI_CONTROLLER Controller,
    PULONG ChannelInterruptBits
    );

VOID
DwhcipProcessPotentiallyCompletedTransfer (
    PDWHCI_CONTROLLER Controller,
    PDWHCI_TRANSFER Transfer,
    ULONG Interrupts,
    PBOOL RemoveSet,
    PBOOL AdvanceEndpoint
    );

VOID
DwhcipRemoveTransferSet (
    PDWHCI_CONTROLLER Controller,
    PDWHCI_TRANSFER_SET TransferSet
    );

VOID
DwhcipProcessSplitEndpoint (
    PDWHCI_CONTROLLER Controller,
    PDWHCI_ENDPOINT Endpoint,
    PULONG Interrupts
    );

VOID
DwhcipProcessPingEndpoint (
    PDWHCI_CONTROLLER Controller,
    PDWHCI_ENDPOINT Endpoint,
    PULONG Interrupts
    );

VOID
DwhcipFillOutTransferDescriptor (
    PDWHCI_CONTROLLER Controller,
    PDWHCI_TRANSFER_SET TransferSet,
    PDWHCI_TRANSFER DwhciTransfer,
    ULONG Offset,
    ULONG Length,
    BOOL LastTransfer
    );

VOID
DwhcipProcessSchedule (
    PDWHCI_CONTROLLER Controller,
    BOOL PeriodicOnly
    );

KSTATUS
DwhcipAllocateChannel (
    PDWHCI_CONTROLLER Controller,
    PDWHCI_ENDPOINT Endpoint
    );

VOID
DwhcipFreeChannel (
    PDWHCI_CONTROLLER Controller,
    PDWHCI_CHANNEL Channel
    );

VOID
DwhcipScheduleTransfer (
    PDWHCI_CONTROLLER Controller,
    PDWHCI_ENDPOINT Endpoint
    );

VOID
DwhcipAdvanceEndpoint (
    PDWHCI_CONTROLLER Controller,
    PDWHCI_ENDPOINT Endpoint
    );

PDWHCI_TRANSFER
DwhcipGetEndpointTransfer (
    PDWHCI_ENDPOINT Endpoint
    );

KSTATUS
DwhcipSoftReset (
    PDWHCI_CONTROLLER Controller
    );

KSTATUS
DwhcipInitializePhy (
    PDWHCI_CONTROLLER Controller
    );

KSTATUS
DwhcipInitializeUsb (
    PDWHCI_CONTROLLER Controller,
    ULONG UsbMode
    );

KSTATUS
DwhcipInitializeHostMode (
    PDWHCI_CONTROLLER Controller,
    ULONG ReceiveFifoSize,
    ULONG NonPeriodicTransmitFifoSize,
    ULONG PeriodicTransmitFifoSize
    );

VOID
DwhcipFlushFifo (
    PDWHCI_CONTROLLER Controller,
    BOOL TransmitFifo,
    ULONG TransmitFifoMask
    );

KSTATUS
DwhcipResetChannel (
    PDWHCI_CONTROLLER Controller,
    ULONG ChannelNumber
    );

BOOL
DwhcipHaltChannel (
    PDWHCI_CONTROLLER Controller,
    PDWHCI_CHANNEL Channel
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define a bitfield of debug flags that enable various print messages for
// DWHCI. See DWHCI_DEBUG_* definitions.
//

ULONG DwhciDebugFlags = 0x0;

//
// ------------------------------------------------------------------ Functions
//

PDWHCI_CONTROLLER
DwhcipInitializeControllerState (
    PVOID RegisterBase,
    ULONG ChannelCount,
    USB_DEVICE_SPEED Speed,
    ULONG MaxTransferSize,
    ULONG MaxPacketCount,
    ULONG Revision
    )

/*++

Routine Description:

    This routine initializes the state and variables needed to start up a DWHCI
    host controller.

Arguments:

    RegisterBase - Supplies the virtual address of the base of the registers.

    ChannelCount - Supplies the number of host controller channels.

    Speed - Supplies the speed of the DWHCI host controller.

    MaxTransferSize - Supplies the maximum transfer size for the DWHCI host
        controller.

    MaxPacketCount - Supplies the maximum packet count for the DWHCI host
        controller.

    Revision - Supplies the revision of the DWHCI host controller.

Return Value:

    Returns a pointer to the DWHCI controller state object on success.

    NULL on failure.

--*/

{

    ULONG AllocationSize;
    PDWHCI_CHANNEL Channels;
    PDWHCI_CONTROLLER Controller;
    ULONG Index;
    ULONG IoBufferFlags;
    KSTATUS Status;

    //
    // Allocate the controller structure and fill it in.
    //

    AllocationSize = sizeof(DWHCI_CONTROLLER) +
                     ((ChannelCount - 1) * sizeof(DWHCI_CHANNEL));

    Controller = MmAllocateNonPagedPool(AllocationSize, DWHCI_ALLOCATION_TAG);
    if (Controller == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeControllerStateEnd;
    }

    RtlZeroMemory(Controller, AllocationSize);
    Controller->RegisterBase = RegisterBase;
    Controller->UsbCoreHandle = INVALID_HANDLE;
    Controller->InterruptHandle = INVALID_HANDLE;
    INITIALIZE_LIST_HEAD(&(Controller->PeriodicActiveListHead));
    INITIALIZE_LIST_HEAD(&(Controller->PeriodicInactiveListHead));
    INITIALIZE_LIST_HEAD(&(Controller->PeriodicReadyListHead));
    INITIALIZE_LIST_HEAD(&(Controller->NonPeriodicActiveListHead));
    INITIALIZE_LIST_HEAD(&(Controller->NonPeriodicReadyListHead));
    INITIALIZE_LIST_HEAD(&(Controller->FreeChannelListHead));
    KeInitializeSpinLock(&(Controller->Lock));
    KeInitializeSpinLock(&(Controller->InterruptLock));
    Controller->PortCount = DWHCI_HOST_PORT_COUNT;
    Controller->Revision = Revision;
    Controller->Speed = Speed;
    Controller->MaxTransferSize = MaxTransferSize;
    Controller->MaxPacketCount = MaxPacketCount;
    Controller->NextFrame = DWHCI_INVALID_FRAME;
    Controller->InterruptDpc = KeCreateDpc(DwhcipInterruptServiceDpc,
                                           Controller);

    if (Controller->InterruptDpc == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeControllerStateEnd;
    }

    Controller->BlockAllocator = MmCreateBlockAllocator(
                                         sizeof(DWHCI_TRANSFER),
                                         DWHCI_BLOCK_ALLOCATOR_ALIGNMENT,
                                         DWHCI_BLOCK_ALLOCATOR_EXPANSION_COUNT,
                                         BLOCK_ALLOCATOR_FLAG_NON_PAGED,
                                         DWHCI_BLOCK_ALLOCATION_TAG);

    if (Controller->BlockAllocator == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeControllerStateEnd;
    }

    IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS;
    Controller->ControlStatusBuffer = MmAllocateNonPagedIoBuffer(
                                              0,
                                              MAX_ULONG,
                                              DWHCI_DMA_ALIGNMENT,
                                              DWHCI_CONTROL_STATUS_BUFFER_SIZE,
                                              IoBufferFlags);

    if (Controller->ControlStatusBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeControllerStateEnd;
    }

    //
    // Initialize the channels.
    //

    Controller->ChannelCount = ChannelCount;
    Channels = (PDWHCI_CHANNEL)(Controller->Channel);
    for (Index = 0; Index < ChannelCount; Index += 1) {
        Channels[Index].ChannelNumber = Index;
    }

    Status = STATUS_SUCCESS;

InitializeControllerStateEnd:
    if (!KSUCCESS(Status)) {
        if (Controller != NULL) {
            DwhcipDestroyControllerState(Controller);
            Controller = NULL;
        }
    }

    return Controller;
}

VOID
DwhcipDestroyControllerState (
    PDWHCI_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine destroys the memory associated with a DWHCI controller.

Arguments:

    Controller - Supplies a pointer to the DWHCI controller state to release.

Return Value:

    None.

--*/

{

    ASSERT(LIST_EMPTY(&(Controller->PeriodicActiveListHead)) != FALSE);
    ASSERT(LIST_EMPTY(&(Controller->PeriodicReadyListHead)) != FALSE);
    ASSERT(LIST_EMPTY(&(Controller->NonPeriodicActiveListHead)) != FALSE);
    ASSERT(LIST_EMPTY(&(Controller->NonPeriodicReadyListHead)) != FALSE);

    if (Controller->InterruptDpc != NULL) {
        KeDestroyDpc(Controller->InterruptDpc);
    }

    if (Controller->UsbCoreHandle != INVALID_HANDLE) {
        UsbHostDestroyControllerState(Controller->UsbCoreHandle);
    }

    if (Controller->BlockAllocator != NULL) {
        MmDestroyBlockAllocator(Controller->BlockAllocator);
    }

    if (Controller->ControlStatusBuffer != NULL) {
        MmFreeIoBuffer(Controller->ControlStatusBuffer);
    }

    MmFreeNonPagedPool(Controller);
    return;
}

KSTATUS
DwhcipRegisterController (
    PDWHCI_CONTROLLER Controller,
    PDEVICE Device
    )

/*++

Routine Description:

    This routine registers the started DWHCI controller with the core USB
    library.

Arguments:

    Controller - Supplies a pointer to the DWHCI controller state of the
        controller to register.

    Device - Supplies a pointer to the device object.

Return Value:

    Status code.

--*/

{

    USB_HOST_CONTROLLER_INTERFACE Interface;
    KSTATUS Status;

    //
    // Fill out the functions that the USB core library will use to control
    // the DWHCI controller.
    //

    RtlZeroMemory(&Interface, sizeof(USB_HOST_CONTROLLER_INTERFACE));
    Interface.Version = USB_HOST_CONTROLLER_INTERFACE_VERSION;
    Interface.DriverObject = DwhciDriver;
    Interface.DeviceObject = Device;
    Interface.HostControllerContext = Controller;
    Interface.Speed = Controller->Speed;
    Interface.DebugPortSubType = -1;
    Interface.RootHubPortCount = Controller->PortCount;
    Interface.CreateEndpoint = DwhcipCreateEndpoint;
    Interface.ResetEndpoint = DwhcipResetEndpoint;
    Interface.DestroyEndpoint = DwhcipDestroyEndpoint;
    Interface.CreateTransfer = DwhcipCreateTransfer;
    Interface.DestroyTransfer = DwhcipDestroyTransfer;
    Interface.SubmitTransfer = DwhcipSubmitTransfer;
    Interface.CancelTransfer = DwhcipCancelTransfer;
    Interface.GetRootHubStatus = DwhcipGetRootHubStatus;
    Interface.SetRootHubStatus = DwhcipSetRootHubStatus;
    Status = UsbHostRegisterController(&Interface,
                                       &(Controller->UsbCoreHandle));

    if (!KSUCCESS(Status)) {
        goto RegisterControllerEnd;
    }

RegisterControllerEnd:
    return Status;
}

KSTATUS
DwhcipInitializeController (
    PDWHCI_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine initializes and starts the DWHCI controller.

Arguments:

    Controller - Supplies a pointer to the DWHCI controller state of the
        controller to reset.

Return Value:

    Status code.

--*/

{

    ULONG AhbConfiguration;
    ULONG BurstLength;
    ULONG CoreInterruptMask;
    ULONG Hardware2;
    ULONG Hardware4;
    ULONG HostConfiguration;
    ULONG NonPeriodicTransmitFifoSize;
    ULONG PeriodicTransmitFifoSize;
    ULONG ReceiveFifoSize;
    KSTATUS Status;
    ULONG UsbCapabilities;
    ULONG UsbConfiguration;

    //
    // Before resetting the controller, save the FIFO sizes that may have been
    // programmed by ACPI. The reset will undo any of the work done by ACPI.
    //

    ReceiveFifoSize = DWHCI_READ_REGISTER(Controller,
                                          DwhciRegisterReceiveFifoSize);

    NonPeriodicTransmitFifoSize = DWHCI_READ_REGISTER(
                                             Controller,
                                             DwhciRegisterNonPeriodicFifoSize);

    PeriodicTransmitFifoSize = DWHCI_READ_REGISTER(
                                                Controller,
                                                DwhciRegisterPeriodicFifoSize);

    //
    // Save the burst length configured by ACPI in the AHB register and disable
    // global interrupts.
    //

    AhbConfiguration = DWHCI_READ_REGISTER(Controller,
                                           DwhciRegisterAhbConfiguration);

    BurstLength = (AhbConfiguration &
                   DWHCI_AHB_CONFIGURATION_AXI_BURST_LENGTH_MASK);

    AhbConfiguration &= ~DWHCI_AHB_CONFIGURATION_INTERRUPT_ENABLE;
    DWHCI_WRITE_REGISTER(Controller,
                         DwhciRegisterAhbConfiguration,
                         AhbConfiguration);

    //
    // Clear the ULPI External VBUS and TS D-LINE pulse enable bits.
    //

    UsbConfiguration = DWHCI_READ_REGISTER(Controller,
                                           DwhciRegisterUsbConfiguration);

    //
    // Save the USB capability bits in the USB configuration register. These do
    // not always agree with the mode set in the hardware 2 register.
    //

    UsbCapabilities = UsbConfiguration & (DWHCI_USB_CONFIGURATION_SRP_CAPABLE |
                                          DWHCI_USB_CONFIGURATION_HNP_CAPABLE);

    UsbConfiguration &= ~DWHCI_USB_CONFIGURATION_ULPI_DRIVER_EXTERNAL_VBUS;
    UsbConfiguration &= ~DWHCI_USB_CONFIGURATION_TS_DLINE_PULSE_ENABLE;
    DWHCI_WRITE_REGISTER(Controller,
                         DwhciRegisterUsbConfiguration,
                         UsbConfiguration);

    //
    // Perform a soft reset of the DWHCI core.
    //

    Status = DwhcipSoftReset(Controller);
    if (!KSUCCESS(Status)) {
        goto InitializeControllerEnd;
    }

    //
    // Initialize the physical layer.
    //

    Status = DwhcipInitializePhy(Controller);
    if (!KSUCCESS(Status)) {
        goto InitializeControllerEnd;
    }

    //
    // The host controller driver currently only supports internal DMA modes.
    //

    Hardware2 = DWHCI_READ_REGISTER(Controller, DwhciRegisterHardware2);
    if ((Hardware2 & DWHCI_HARDWARE2_ARCHITECTURE_MASK) !=
        DWHCI_HARDWARE2_ARCHITECTURE_INTERNAL_DMA) {

        Status = STATUS_NOT_SUPPORTED;
        goto InitializeControllerEnd;
    }

    //
    // The controller currently only supports non-descriptor DMA mode.
    //

    Hardware4 = DWHCI_READ_REGISTER(Controller, DwhciRegisterHardware4);
    if ((Hardware4 & DWHCI_HARDWARE4_DMA_DESCRIPTOR_MODE) != 0) {
        HostConfiguration = DWHCI_READ_REGISTER(Controller,
                                                DwhciRegisterHostConfiguration);

        HostConfiguration &= ~DWHCI_HOST_CONFIGURATION_ENABLE_DMA_DESCRIPTOR;
        DWHCI_WRITE_REGISTER(Controller,
                             DwhciRegisterHostConfiguration,
                             HostConfiguration);
    }

    //
    // Enable DMA mode.
    //

    AhbConfiguration = DWHCI_READ_REGISTER(Controller,
                                           DwhciRegisterAhbConfiguration);

    AhbConfiguration |= DWHCI_AHB_CONFIGURATION_DMA_ENABLE;
    AhbConfiguration &= ~DWHCI_AHB_CONFIGURATION_DMA_REMAINDER_MODE_MASK;
    AhbConfiguration |= DWHCI_AHB_CONFIGURATION_DMA_REMAINDER_MODE_INCREMENTAL;
    AhbConfiguration |= BurstLength;
    DWHCI_WRITE_REGISTER(Controller,
                         DwhciRegisterAhbConfiguration,
                         AhbConfiguration);

    //
    // Perform the necessary steps to initialize the USB configuration.
    //

    Status = DwhcipInitializeUsb(Controller, UsbCapabilities);
    if (!KSUCCESS(Status)) {
        goto InitializeControllerEnd;
    }

    //
    // The DWHCI can operate in one of two modes. Host mode or device mode.
    // Configure the controller to run in host mode.
    //

    Status = DwhcipInitializeHostMode(Controller,
                                      ReceiveFifoSize,
                                      NonPeriodicTransmitFifoSize,
                                      PeriodicTransmitFifoSize);

    if (!KSUCCESS(Status)) {
        goto InitializeControllerEnd;
    }

    //
    // Enable interrupts for the core and channels. Do not enable global
    // interrupts until the interrupt handle is initialized.
    //

    DWHCI_WRITE_REGISTER(Controller, DwhciRegisterOtgInterrupt, 0xFFFFFFFF);
    DWHCI_WRITE_REGISTER(Controller, DwhciRegisterCoreInterrupt, 0xFFFFFFFF);
    CoreInterruptMask = DWHCI_INITIAL_CORE_INTERRUPT_MASK;
    DWHCI_WRITE_REGISTER(Controller,
                         DwhciRegisterCoreInterruptMask,
                         CoreInterruptMask);

    //
    // Re-enable the global interrupts.
    //

    AhbConfiguration = DWHCI_READ_REGISTER(Controller,
                                           DwhciRegisterAhbConfiguration);

    AhbConfiguration |= DWHCI_AHB_CONFIGURATION_INTERRUPT_ENABLE;
    DWHCI_WRITE_REGISTER(Controller,
                         DwhciRegisterAhbConfiguration,
                         AhbConfiguration);

InitializeControllerEnd:
    return Status;
}

INTERRUPT_STATUS
DwhcipInterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the DWHCI interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the DWHCI
        controller.

Return Value:

    Interrupt status.

--*/

{

    PDWHCI_CONTROLLER Controller;
    ULONG FrameNumber;
    ULONG Interrupts;
    ULONG InterruptsMask;
    INTERRUPT_STATUS InterruptStatus;
    ULONG OriginalInterrupts;
    ULONG OriginalPendingInterrupts;
    ULONG PortInterrupts;

    Controller = (PDWHCI_CONTROLLER)Context;
    InterruptStatus = InterruptStatusNotClaimed;

    //
    // Read the interrupt register. If there are interesting interrupts, then
    // handle them.
    //

    Interrupts = DWHCI_READ_REGISTER(Controller, DwhciRegisterCoreInterrupt);
    InterruptsMask = DWHCI_READ_REGISTER(Controller,
                                         DwhciRegisterCoreInterruptMask);

    Interrupts &= InterruptsMask;
    if (Interrupts != 0) {
        OriginalInterrupts = Interrupts;
        PortInterrupts = 0;
        InterruptStatus = InterruptStatusClaimed;
        KeAcquireSpinLock(&(Controller->InterruptLock));

        //
        // In order to clear the core host port interrupt, the host port
        // interrupt status must read and cleared.
        //

        if ((Interrupts & DWHCI_CORE_INTERRUPT_PORT) != 0) {
            PortInterrupts = DWHCI_READ_REGISTER(Controller,
                                                 DwhciRegisterHostPort);

            //
            // If none of the change bits are set, then ignore this host port
            // interrupt.
            //

            if ((PortInterrupts & DWHCI_HOST_PORT_INTERRUPT_MASK) == 0) {
                Interrupts &= ~DWHCI_CORE_INTERRUPT_PORT;
                PortInterrupts = 0;

            } else {
                PortInterrupts = (PortInterrupts &
                                  ~DWHCI_HOST_PORT_WRITE_TO_CLEAR_MASK) |
                                 (PortInterrupts &
                                  DWHCI_HOST_PORT_INTERRUPT_MASK);
            }
        }

        //
        // If there is a channel interrupt, then each channel's interrupt bits
        // needs to be saved and cleared in order to clear the core interrupt.
        //

        if ((Interrupts & DWHCI_CORE_INTERRUPT_HOST_CHANNEL) != 0) {
            DwhcipSaveChannelInterrupts(Controller);
        }

        //
        // On start of frame interrupts, check the current frame against the
        // next targeted start of frame. If it is less, then skip this start of
        // frame interrupt.
        //

        if ((Interrupts & DWHCI_CORE_INTERRUPT_START_OF_FRAME) != 0) {
            FrameNumber = DWHCI_READ_FRAME_NUMBER(Controller);
            if ((Controller->NextFrame == DWHCI_INVALID_FRAME) ||
                DWHCI_FRAME_LESS_THAN(FrameNumber, Controller->NextFrame)) {

                Interrupts &= ~DWHCI_CORE_INTERRUPT_START_OF_FRAME;
            }
        }

        //
        // If there were no pending interrupts to begin with and there are
        // interrupts left to process, then a DPC needs to be queued to process
        // these interrupts.
        //

        OriginalPendingInterrupts = Controller->PendingInterruptBits;
        Controller->PendingInterruptBits |= Interrupts;
        if ((OriginalPendingInterrupts == 0) && (Interrupts != 0)) {
            KeQueueDpc(Controller->InterruptDpc);
        }

        //
        // The host port register needs to be cleared of any change bits in
        // order to remove the core host port interrupt.
        //

        if (PortInterrupts != 0) {
            DWHCI_WRITE_REGISTER(Controller,
                                 DwhciRegisterHostPort,
                                 PortInterrupts);
        }

        //
        // Clear the bits in the core interrupt register to acknowledge the
        // interrupts.
        //

        DWHCI_WRITE_REGISTER(Controller,
                             DwhciRegisterCoreInterrupt,
                             OriginalInterrupts);

        KeReleaseSpinLock(&(Controller->InterruptLock));
    }

    return InterruptStatus;
}

VOID
DwhcipSetInterruptHandle (
    PDWHCI_CONTROLLER Controller,
    HANDLE InterruptHandle
    )

/*++

Routine Description:

    This routine saves the handle of the connected interrupt in the DWHCI
    controller.

Arguments:

    Controller - Supplies a pointer to the DWHCI controller state.

    InterruptHandle - Supplies the connected interrupt handle.

Return Value:

    None.

--*/

{

    Controller->InterruptHandle = InterruptHandle;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
DwhcipCreateEndpoint (
    PVOID HostControllerContext,
    PUSB_HOST_ENDPOINT_CREATION_REQUEST Endpoint,
    PVOID *EndpointContext
    )

/*++

Routine Description:

    This routine is called by the USB core when a new endpoint is being opened.
    It allows the host controller to create and store any context needed to
    support a new endpoint (such as a queue head).

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    Endpoint - Supplies a pointer containing information about the endpoint
        being created. The host controller cannot count on this buffer sticking
        around after the function returns. If it needs this information it
        should make a copy of it.

    EndpointContext - Supplies a pointer where the host controller can store a
        context pointer identifying the endpoint created.

Return Value:

    STATUS_SUCCESS if the endpoint can be successfully accommodated.

    Failing status code if the endpoint cannot be opened.

--*/

{

    ULONG ChannelControl;
    PDWHCI_CONTROLLER Controller;
    ULONG HubAddress;
    PDWHCI_ENDPOINT NewEndpoint;
    ULONG PortAddress;
    KSTATUS Status;

    Controller = (PDWHCI_CONTROLLER)HostControllerContext;
    NewEndpoint = MmAllocateNonPagedPool(sizeof(DWHCI_ENDPOINT),
                                         DWHCI_ALLOCATION_TAG);

    if (NewEndpoint == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateEndpointEnd;
    }

    RtlZeroMemory(NewEndpoint, sizeof(DWHCI_ENDPOINT));
    INITIALIZE_LIST_HEAD(&(NewEndpoint->TransferSetListHead));
    NewEndpoint->TransferType = Endpoint->Type;

    //
    // The endpoint speed better be appropriate for the controller.
    //

    ASSERT((Controller->Speed != UsbDeviceSpeedHigh) ||
           ((Endpoint->Speed == UsbDeviceSpeedLow) ||
            (Endpoint->Speed == UsbDeviceSpeedFull) ||
            (Endpoint->Speed == UsbDeviceSpeedHigh)));

    ASSERT((Controller->Speed != UsbDeviceSpeedFull) ||
           ((Endpoint->Speed == UsbDeviceSpeedLow) ||
            (Endpoint->Speed == UsbDeviceSpeedFull)));

    NewEndpoint->Speed = Endpoint->Speed;
    NewEndpoint->DataToggle = DWHCI_PID_CODE_DATA_0;
    NewEndpoint->PollRate = Endpoint->PollRate;

    ASSERT(Endpoint->MaxPacketSize != 0);

    //
    // If the endpoint is a full or low speed endpoint, then the poll rate is
    // in milliseconds. But if the controller is high speed, it operates in 125
    // microsecond frames. Do the conversion - multiply by 8.
    //

    if ((Controller->Speed == UsbDeviceSpeedHigh) &&
        ((NewEndpoint->Speed == UsbDeviceSpeedLow) ||
         (NewEndpoint->Speed == UsbDeviceSpeedFull))) {

        NewEndpoint->PollRate <<= DWHCI_MICROFRAMES_PER_FRAME_SHIFT;
        NewEndpoint->PollRate &= DWHCI_FRAME_NUMBER_MAX;
    }

    //
    // If this is a high speed bulk OUT endpoint, then always start with the
    // PING protocol.
    //

    NewEndpoint->PingRequired = FALSE;
    if ((Endpoint->Type == UsbTransferTypeBulk) &&
        (Endpoint->Speed == UsbDeviceSpeedHigh) &&
        (Endpoint->Direction == UsbTransferDirectionOut)) {

        NewEndpoint->PingRequired = TRUE;
    }

    //
    // If this is a low or full speed endpoint on a high speed controller, then
    // initialize the split control with the hub port and hub address.
    //

    ASSERT(NewEndpoint->SplitControl == 0);

    if ((Controller->Speed == UsbDeviceSpeedHigh) &&
        (Endpoint->HubAddress != 0) &&
        ((NewEndpoint->Speed == UsbDeviceSpeedLow) ||
         (NewEndpoint->Speed == UsbDeviceSpeedFull))) {

        ASSERT(Endpoint->HubPortNumber != 0);

        PortAddress = (Endpoint->HubPortNumber <<
                       DWHCI_CHANNEL_SPLIT_CONTROL_PORT_ADDRESS_SHIFT) &
                      DWHCI_CHANNEL_SPLIT_CONTROL_PORT_ADDRESS_MASK;

        HubAddress = (Endpoint->HubAddress <<
                      DWHCI_CHANNEL_SPLIT_CONTROL_HUB_ADDRESS_SHIFT) &
                     DWHCI_CHANNEL_SPLIT_CONTROL_HUB_ADDRESS_MASK;

        NewEndpoint->SplitControl = PortAddress | HubAddress;

        //
        // TODO: The isochronous split schedule should be more precise.
        //

        NewEndpoint->SplitControl |= DWHCI_CHANNEL_SPLIT_CONTROL_POSITION_ALL;
        NewEndpoint->SplitControl |= DWHCI_CHANNEL_SPLIT_CONTROL_ENABLE;
    }

    NewEndpoint->MaxPacketSize = Endpoint->MaxPacketSize;
    NewEndpoint->EndpointNumber = Endpoint->EndpointNumber;

    //
    // Save the maximum number of packets that can be sent over this endpoint
    // in a single transfer and the maximum size of each transfer.
    //

    NewEndpoint->MaxPacketCount = Controller->MaxTransferSize /
                                  NewEndpoint->MaxPacketSize;

    if (NewEndpoint->MaxPacketCount > Controller->MaxPacketCount) {
        NewEndpoint->MaxPacketCount = Controller->MaxPacketCount;
    }

    NewEndpoint->MaxTransferSize = NewEndpoint->MaxPacketCount *
                                   NewEndpoint->MaxPacketSize;

    ASSERT(NewEndpoint->MaxPacketCount <= DWHCI_MAX_PACKET_COUNT);
    ASSERT(NewEndpoint->MaxTransferSize <= DWHCI_MAX_TRANSFER_SIZE);

    //
    // High-bandwidth multiple count packets are not supported.
    //

    ASSERT((NewEndpoint->MaxPacketSize &
            ~DWHCI_CHANNEL_CONTROL_MAX_PACKET_SIZE_MASK) == 0);

    //
    // Initialize the endpoints's channel control.
    //

    ChannelControl = (NewEndpoint->EndpointNumber <<
                      DWHCI_CHANNEL_CONTROL_ENDPOINT_SHIFT) &
                     DWHCI_CHANNEL_CONTROL_ENDPOINT_MASK;

    ChannelControl |= (NewEndpoint->MaxPacketSize <<
                       DWHCI_CHANNEL_CONTROL_MAX_PACKET_SIZE_SHIFT) &
                      DWHCI_CHANNEL_CONTROL_MAX_PACKET_SIZE_MASK;

    switch (NewEndpoint->TransferType) {
    case UsbTransferTypeControl:
        ChannelControl |= DWHCI_CHANNEL_CONTROL_ENDPOINT_CONTROL;
        break;

    case UsbTransferTypeInterrupt:
        ChannelControl |= DWHCI_CHANNEL_CONTROL_ENDPOINT_INTERRUPT;
        break;

    case UsbTransferTypeBulk:
        ChannelControl |= DWHCI_CHANNEL_CONTROL_ENDPOINT_BULK;
        break;

    case UsbTransferTypeIsochronous:
        ChannelControl |= DWHCI_CHANNEL_CONTROL_ENDPOINT_ISOCHRONOUS;
        break;

    default:

        ASSERT(FALSE);

        break;
    }

    if (NewEndpoint->Speed == UsbDeviceSpeedLow) {
        ChannelControl |= DWHCI_CHANNEL_CONTROL_LOW_SPEED;
    }

    ChannelControl |= (0x1 << DWHCI_CHANNEL_CONTROL_PACKETS_PER_FRAME_SHIFT) &
                      DWHCI_CHANNEL_CONTROL_PACKETS_PER_FRAME_MASK;

    ChannelControl |= DWHCI_CHANNEL_CONTROL_ENABLE;

    ASSERT((ChannelControl & DWHCI_CHANNEL_CONTROL_DISABLE) == 0);

    NewEndpoint->ChannelControl = ChannelControl;
    Status = STATUS_SUCCESS;

CreateEndpointEnd:
    if (!KSUCCESS(Status)) {
        if (NewEndpoint != NULL) {
            MmFreeNonPagedPool(NewEndpoint);
            NewEndpoint = NULL;
        }
    }

    *EndpointContext = NewEndpoint;
    return Status;
}

VOID
DwhcipResetEndpoint (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    ULONG MaxPacketSize
    )

/*++

Routine Description:

    This routine is called by the USB core when an endpoint needs to be reset.

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    EndpointContext - Supplies a pointer to the context returned by the host
        controller when the endpoint was created.

    MaxPacketSize - Supplies the maximum transfer size of the endpoint.

Return Value:

    None.

--*/

{

    ULONG ChannelControl;
    PDWHCI_CONTROLLER Controller;
    PDWHCI_ENDPOINT Endpoint;

    Endpoint = (PDWHCI_ENDPOINT)EndpointContext;
    Endpoint->DataToggle = DWHCI_PID_CODE_DATA_0;
    Controller = (PDWHCI_CONTROLLER)HostControllerContext;
    if (MaxPacketSize != Endpoint->MaxPacketSize) {
        Endpoint->MaxPacketSize = MaxPacketSize;
        ChannelControl = Endpoint->ChannelControl &
                         ~DWHCI_CHANNEL_CONTROL_MAX_PACKET_SIZE_MASK;

        ChannelControl |= (Endpoint->MaxPacketSize <<
                           DWHCI_CHANNEL_CONTROL_MAX_PACKET_SIZE_SHIFT) &
                          DWHCI_CHANNEL_CONTROL_MAX_PACKET_SIZE_MASK;

        Endpoint->ChannelControl = ChannelControl;
        Endpoint->MaxPacketCount = Controller->MaxTransferSize / MaxPacketSize;
        if (Endpoint->MaxPacketCount > Controller->MaxPacketCount) {
            Endpoint->MaxPacketCount = Controller->MaxPacketCount;
        }

        Endpoint->MaxTransferSize = Endpoint->MaxPacketCount * MaxPacketSize;
    }

    return;
}

VOID
DwhcipDestroyEndpoint (
    PVOID HostControllerContext,
    PVOID EndpointContext
    )

/*++

Routine Description:

    This routine tears down and destroys an endpoint created with the endpoint
    creation routine.

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    EndpointContext - Supplies a pointer to the context returned by the host
        controller when the endpoint was created.

Return Value:

    None.

--*/

{

    PDWHCI_ENDPOINT Endpoint;

    Endpoint = (PDWHCI_ENDPOINT)EndpointContext;

    ASSERT(LIST_EMPTY(&(Endpoint->TransferSetListHead)) != FALSE);

    MmFreeNonPagedPool(Endpoint);
    return;
}

KSTATUS
DwhcipCreateTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    ULONG MaxBufferSize,
    ULONG Flags,
    PVOID *TransferContext
    )

/*++

Routine Description:

    This routine allocates structures needed for the USB host controller to
    support a transfer.

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    EndpointContext - Supplies a pointer to the host controller's context of
        the endpoint that this transfer will eventually be submitted to.

    MaxBufferSize - Supplies the maximum buffer length, in bytes, of the
        transfer when it is submitted. It is assumed that the host controller
        will set up as many transfer descriptors as are needed to support a
        transfer of this size.

    Flags - Supplies a bitfield of flags regarding the transaction. See
        USB_TRANSFER_FLAG_* definitions.

    TransferContext - Supplies a pointer where the host controller can store a
        context pointer containing any needed structures for the transfer.

Return Value:

    None.

--*/

{

    ULONG AllocationSize;
    PDWHCI_CONTROLLER Controller;
    PDWHCI_ENDPOINT Endpoint;
    BOOL ForceShortTransfer;
    KSTATUS Status;
    PDWHCI_TRANSFER Transfer;
    PDWHCI_TRANSFER *TransferArray;
    ULONG TransferCount;
    ULONG TransferIndex;
    PDWHCI_TRANSFER_SET TransferSet;

    ASSERT(TransferContext != NULL);

    Controller = (PDWHCI_CONTROLLER)HostControllerContext;
    Endpoint = (PDWHCI_ENDPOINT)EndpointContext;
    TransferArray = NULL;
    ForceShortTransfer = FALSE;
    if ((Flags & USB_TRANSFER_FLAG_FORCE_SHORT_TRANSFER) != 0) {
        ForceShortTransfer = TRUE;
    }

    //
    // Figure out the number of transfers needed. The first 8 bytes of a
    // control transfer (the setup packet) are always on their own. Control
    // transfers also have a status stage at the end.
    //

    TransferCount = 0;
    if (Endpoint->TransferType == UsbTransferTypeControl) {

        ASSERT(MaxBufferSize >= sizeof(USB_SETUP_PACKET));

        MaxBufferSize -= sizeof(USB_SETUP_PACKET);

        //
        // Account for both the setup and status stage here.
        //

        TransferCount += 2;
    }

    //
    // Try to fit as many packets into each transfer as possible. Low speed
    // endpoints on high speed controllers requiring split transfers can only
    // execute one max packet size per transfer.
    //

    if (MaxBufferSize != 0) {
        if (Endpoint->SplitControl == 0) {
            TransferCount += MaxBufferSize / Endpoint->MaxTransferSize;
            if ((MaxBufferSize % Endpoint->MaxTransferSize) != 0) {
                TransferCount += 1;
            }

        } else {
            TransferCount += MaxBufferSize / Endpoint->MaxPacketSize;
            if ((MaxBufferSize % Endpoint->MaxPacketSize) != 0) {
                TransferCount += 1;
            }
        }

        //
        // If this transfer needs to indicate completion with a short packet,
        // make sure another transfer is available. This is only necessary if
        // the last packet might not be a short packet. Unfortunately the
        // terminating zero length packet cannot be added to the end of a
        // multi-packet transfer, so it needs its own.
        //

        if ((ForceShortTransfer != FALSE) &&
            (MaxBufferSize >= Endpoint->MaxPacketSize)) {

            TransferCount += 1;
        }

    //
    // Account for a USB transfer that will only send zero length packets and
    // for control transfers that need to force a zero length packet in the
    // data phase.
    //

    } else if ((ForceShortTransfer != FALSE) ||
               (Endpoint->TransferType != UsbTransferTypeControl)) {

        TransferCount += 1;
    }

    //
    // Allocate the transfer set structure.
    //

    AllocationSize = sizeof(DWHCI_TRANSFER_SET);
    if (TransferCount > 1) {
        AllocationSize += sizeof(PDWHCI_TRANSFER) * (TransferCount - 1);
    }

    TransferSet = MmAllocateNonPagedPool(AllocationSize, DWHCI_ALLOCATION_TAG);
    if (TransferSet == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateTransferEnd;
    }

    RtlZeroMemory(TransferSet, AllocationSize);
    INITIALIZE_LIST_HEAD(&(TransferSet->TransferListHead));
    TransferSet->TransferCount = TransferCount;
    TransferSet->Endpoint = Endpoint;
    TransferArray = (PDWHCI_TRANSFER *)(TransferSet->Transfer);

    //
    // Create the new transfers.
    //

    for (TransferIndex = 0; TransferIndex < TransferCount; TransferIndex += 1) {
        Transfer = MmAllocateBlock(Controller->BlockAllocator, NULL);
        if (Transfer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CreateTransferEnd;
        }

        RtlZeroMemory(Transfer, sizeof(DWHCI_TRANSFER));
        TransferArray[TransferIndex] = Transfer;
    }

    Status = STATUS_SUCCESS;

CreateTransferEnd:
    if (!KSUCCESS(Status)) {
        if (TransferSet != NULL) {
            for (TransferIndex = 0;
                 TransferIndex < TransferCount;
                 TransferIndex += 1) {

                Transfer = TransferArray[TransferIndex];
                if (Transfer != NULL) {
                    MmFreeBlock(Controller->BlockAllocator, Transfer);
                }
            }

            MmFreeNonPagedPool(TransferSet);
            TransferSet = NULL;
        }
    }

    *TransferContext = TransferSet;
    return Status;
}

VOID
DwhcipDestroyTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PVOID TransferContext
    )

/*++

Routine Description:

    This routine destroys host controller structures associated with a USB
    transfer.

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    EndpointContext - Supplies a pointer to the host controller context for the
        endpoint this transfer belonged to.

    TransferContext - Supplies the pointer provided to the USB core by the host
        controller when the transfer was created.

Return Value:

    None.

--*/

{

    PDWHCI_CONTROLLER Controller;
    PDWHCI_TRANSFER Transfer;
    PDWHCI_TRANSFER *TransferArray;
    ULONG TransferIndex;
    PDWHCI_TRANSFER_SET TransferSet;

    Controller = (PDWHCI_CONTROLLER)HostControllerContext;
    TransferSet = (PDWHCI_TRANSFER_SET)TransferContext;
    TransferArray = (PDWHCI_TRANSFER *)(TransferSet->Transfer);

    //
    // Free all transfers that were allocated.
    //

    for (TransferIndex = 0;
         TransferIndex < TransferSet->TransferCount;
         TransferIndex += 1) {

        Transfer = TransferArray[TransferIndex];

        ASSERT(Transfer != NULL);

        MmFreeBlock(Controller->BlockAllocator, Transfer);
        TransferArray[TransferIndex] = NULL;
    }

    MmFreeNonPagedPool(TransferSet);
    return;
}

KSTATUS
DwhcipSubmitTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PUSB_TRANSFER_INTERNAL Transfer,
    PVOID TransferContext
    )

/*++

Routine Description:

    This routine submits a transfer to the USB host controller for execution.

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    EndpointContext - Supplies the context pointer provided to the USB core by
        the host controller when the endpoint was created.

    Transfer - Supplies a pointer to the USB transfer to execute.

    TransferContext - Supplies the pointer provided to the USB core by the host
        controller when the transfer was created.

Return Value:

    STATUS_SUCCESS if the transfer was successfully added to the hardware queue.

    Failure codes if the transfer could not be added.

--*/

{

    ULONG ChannelControl;
    PDWHCI_CONTROLLER Controller;
    BOOL ControlTransfer;
    ULONG CoreInterruptMask;
    PDWHCI_TRANSFER DwhciTransfer;
    PDWHCI_ENDPOINT Endpoint;
    UCHAR EndpointDeviceAddress;
    BOOL ForceShortTransfer;
    ULONG FrameNumber;
    ULONG FrameOffset;
    BOOL LastTransfer;
    ULONG Length;
    ULONG MaxTransferSize;
    ULONG NextFrame;
    ULONG Offset;
    RUNLEVEL OldRunLevel;
    ULONG TotalLength;
    PDWHCI_TRANSFER *TransferArray;
    ULONG TransferCount;
    ULONG TransferIndex;
    PDWHCI_TRANSFER_SET TransferSet;

    Controller = (PDWHCI_CONTROLLER)HostControllerContext;
    ControlTransfer = FALSE;
    Endpoint = (PDWHCI_ENDPOINT)EndpointContext;
    TransferSet = (PDWHCI_TRANSFER_SET)TransferContext;
    TransferArray = (PDWHCI_TRANSFER *)(TransferSet->Transfer);
    DwhciTransfer = NULL;

    //
    // Assume that this is going to be a rousing success.
    //

    Transfer->Public.Status = STATUS_SUCCESS;
    Transfer->Public.Error = UsbErrorNone;
    TransferSet->UsbTransfer = Transfer;

    //
    // Before filling out and inserting transfers, take a look to see if the
    // device address has changed. If it has, then it should still be in the
    // enumeration phase, meaning there are no pending transfers floating
    // around.
    //

    EndpointDeviceAddress = (Endpoint->ChannelControl &
                             DWHCI_CHANNEL_CONTROL_DEVICE_ADDRESS_MASK) >>
                            DWHCI_CHANNEL_CONTROL_DEVICE_ADDRESS_SHIFT;

    if (Transfer->DeviceAddress != EndpointDeviceAddress) {

        ASSERT(EndpointDeviceAddress == 0);
        ASSERT(Transfer->DeviceAddress != 0);
        ASSERT(LIST_EMPTY(&(Endpoint->TransferSetListHead)) != FALSE);

        ChannelControl = (Transfer->DeviceAddress <<
                          DWHCI_CHANNEL_CONTROL_DEVICE_ADDRESS_SHIFT) &
                         DWHCI_CHANNEL_CONTROL_DEVICE_ADDRESS_MASK;

        Endpoint->ChannelControl |= ChannelControl;
    }

    //
    // Determine the number of transfers needed for this transfer, and loop
    // filling them out. This is necessary because the number of transfers
    // per transfer is not constant; the system may re-use a transfer and
    // and change the length.
    //

    TransferCount = 0;
    TotalLength = Transfer->Public.Length;
    if (Endpoint->TransferType == UsbTransferTypeControl) {
        ControlTransfer = TRUE;

        ASSERT(TotalLength >= sizeof(USB_SETUP_PACKET));

        TotalLength -= sizeof(USB_SETUP_PACKET);

        //
        // Account for both the setup and status transfers.
        //

        TransferCount += 2;
    }

    ForceShortTransfer = FALSE;
    if ((Transfer->Public.Flags &
         USB_TRANSFER_FLAG_FORCE_SHORT_TRANSFER) != 0) {

        ForceShortTransfer = TRUE;
    }

    //
    // Determine the number of transfers in this set. Low speed endpoints on
    // high speed controllers requiring split transfers can only execute one
    // max packet size per transfer.
    //

    if (Endpoint->SplitControl == 0) {
        TransferCount += TotalLength / Endpoint->MaxTransferSize;
        if ((TotalLength % Endpoint->MaxTransferSize) != 0) {
            TransferCount += 1;
        }

        MaxTransferSize = Endpoint->MaxTransferSize;

    } else {
        TransferCount += TotalLength / Endpoint->MaxPacketSize;
        if ((TotalLength % Endpoint->MaxPacketSize) != 0) {
            TransferCount += 1;
        }

        MaxTransferSize = Endpoint->MaxPacketSize;
    }

    //
    // Add an extra transfer if it is needed for more data or to force a short
    // transfer. Make sure this accounts for non-control zero-length requests.
    //

    if (((ForceShortTransfer != FALSE) &&
         ((TotalLength % Endpoint->MaxPacketSize) == 0)) ||
        ((TotalLength == 0) &&
         (Endpoint->TransferType != UsbTransferTypeControl)) ) {

        TransferCount += 1;
    }

    ASSERT(TransferSet->TransferCount >= TransferCount);

    //
    // Initialize the DWHCI transfers required for this USB transfer and add
    // them to the transfer set's list head.
    //

    Offset = 0;
    LastTransfer = FALSE;
    INITIALIZE_LIST_HEAD(&(TransferSet->TransferListHead));
    for (TransferIndex = 0; TransferIndex < TransferCount; TransferIndex += 1) {

        //
        // Calculate the length for this transfer descriptor.
        //

        Length = MaxTransferSize;
        if (Offset + Length > Transfer->Public.Length) {
            Length = Transfer->Public.Length - Offset;
        }

        if (TransferIndex == (TransferCount - 1)) {
            LastTransfer = TRUE;
        }

        if (ControlTransfer != FALSE) {

            //
            // The first part of a control transfer is the setup packet, which
            // is always 8 bytes long.
            //

            if (Offset == 0) {
                Length = sizeof(USB_SETUP_PACKET);
            }

            //
            // The last part of a control transfer is the status phase and the
            // length better be zero.
            //

            ASSERT((LastTransfer == FALSE) || (Length == 0));
        }

        ASSERT((Length != 0) ||
               (LastTransfer != FALSE) ||
               ((ForceShortTransfer != FALSE) && (ControlTransfer != FALSE)));

        //
        // Fill out this transfer descriptor.
        //

        DwhciTransfer = TransferArray[TransferIndex];
        DwhcipFillOutTransferDescriptor(Controller,
                                        TransferSet,
                                        DwhciTransfer,
                                        Offset,
                                        Length,
                                        LastTransfer);

        //
        // Advance the buffer position.
        //

        Offset += Length;
    }

    //
    // Mark the current transfer as the last transfer.
    //

    DwhciTransfer->LastTransfer = TRUE;

    //
    // The controller lock is required for endpoint updates and schedule
    // processing.
    //

    OldRunLevel = DwhcipAcquireControllerLock(Controller);

    //
    // The transfer set is ready to go. Insert it into the endpoint's list of
    // transfer sets.
    //

    INSERT_BEFORE(&(TransferSet->EndpointListEntry),
                  &(Endpoint->TransferSetListHead));

    //
    // If the endpoint is not already inserted into the schedule, insert it.
    //

    if (Endpoint->ListEntry.Next == NULL) {

        ASSERT(Endpoint->Scheduled == FALSE);

        if ((Endpoint->TransferType == UsbTransferTypeControl) ||
            (Endpoint->TransferType == UsbTransferTypeBulk)) {

            INSERT_BEFORE(&(Endpoint->ListEntry),
                          &(Controller->NonPeriodicReadyListHead));

            //
            // There is now work on the non-periodic schedule that needs to be
            // done. Try to schedule it.
            //

            DwhcipProcessSchedule(Controller, FALSE);

        } else {

            ASSERT((Endpoint->TransferType == UsbTransferTypeInterrupt) ||
                   (Endpoint->TransferType == UsbTransferTypeIsochronous));

            //
            // Schedule this endpoint for a (micro)frame shortly in the future
            // to kick it off.
            //

            FrameNumber = DWHCI_READ_FRAME_NUMBER(Controller);

            ASSERT(Endpoint->NextFrame == 0);

            //
            // Schedule for a future (micro)frame, but not further than the
            // poll rate.
            //

            FrameOffset = DWHCI_DEFAULT_FRAME_OFFSET;
            if (FrameOffset > Endpoint->PollRate) {
                FrameOffset = Endpoint->PollRate;
            }

            NextFrame = (FrameNumber + FrameOffset) & DWHCI_FRAME_NUMBER_MAX;

            //
            // Start splits are not allowed to start in the 6th microframe and
            // get less time for the complete splits the later they get
            // scheduled within a frame. Schedule them all for the last
            // microframe.
            //

            if ((Endpoint->SplitControl != 0) &&
                (Endpoint->TransferType == UsbTransferTypeInterrupt)) {

                NextFrame |= DWHCI_INTERRUPT_SPLIT_FRAME_MASK;
            }

            if ((Controller->NextFrame == DWHCI_INVALID_FRAME) ||
                DWHCI_FRAME_LESS_THAN(NextFrame, Controller->NextFrame)) {

                Controller->NextFrame = NextFrame;
            }

            Endpoint->NextFrame = NextFrame;

            //
            // These transfer need to wait for the start of the appropriate
            // (micro)frame. Activate the start-of-frame interrupt if the
            // periodic inactive list is currently empty.
            //

            if (LIST_EMPTY(&(Controller->PeriodicInactiveListHead)) != FALSE) {
                CoreInterruptMask = DWHCI_READ_REGISTER(
                                               Controller,
                                               DwhciRegisterCoreInterruptMask);

                CoreInterruptMask |= DWHCI_CORE_INTERRUPT_START_OF_FRAME;
                DWHCI_WRITE_REGISTER(Controller,
                                     DwhciRegisterCoreInterruptMask,
                                     CoreInterruptMask);
            }

            INSERT_BEFORE(&(Endpoint->ListEntry),
                          &(Controller->PeriodicInactiveListHead));
        }
    }

    //
    // All done. Release the lock and return.
    //

    DwhcipReleaseControllerLock(Controller, OldRunLevel);
    return STATUS_SUCCESS;
}

KSTATUS
DwhcipCancelTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PUSB_TRANSFER_INTERNAL Transfer,
    PVOID TransferContext
    )

/*++

Routine Description:

    This routine submits attempts to cancel a transfer that was previously
    submitted for execution.

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    EndpointContext - Supplies the context pointer provided to the USB core by
        the host controller when the endpoint was created.

    Transfer - Supplies a pointer to the USB transfer to execute.

    TransferContext - Supplies the pointer provided to the USB core by the host
        controller when the transfer was created.

Return Value:

    STATUS_SUCCESS if the transfer was successfully removed from the hardware
    queue.

    STATUS_TOO_LATE if the transfer had already completed.

    Other failure codes if the transfer could not be cancelled but has not yet
    completed.

--*/

{

    PDWHCI_CONTROLLER Controller;
    PDWHCI_ENDPOINT Endpoint;
    BOOL FirstSet;
    PDWHCI_TRANSFER_SET FirstTransferSet;
    BOOL Halted;
    RUNLEVEL OldRunLevel;
    BOOL RemoveSet;
    KSTATUS Status;
    PDWHCI_TRANSFER_SET TransferSet;

    Controller = (PDWHCI_CONTROLLER)HostControllerContext;
    TransferSet = (PDWHCI_TRANSFER_SET)TransferContext;

    ASSERT(TransferSet->UsbTransfer == Transfer);

    //
    // Lock the controller to manipulate the endpoint lists.
    //

    OldRunLevel = DwhcipAcquireControllerLock(Controller);

    //
    // If the transfer set was already taken off its endpoint list, then the
    // transfer has already completed.
    //

    if (TransferSet->EndpointListEntry.Next == NULL) {

        ASSERT(TransferSet->EndpointListEntry.Next == NULL);

        Status = STATUS_TOO_LATE;
        goto CancelTransferEnd;
    }

    //
    // Isochronous transfers are handled differently.
    //

    if (Transfer->Type == UsbTransferTypeIsochronous) {

        //
        // TODO: Implement support for isochronous transfers.
        //

        ASSERT(FALSE);

        Status = STATUS_NOT_IMPLEMENTED;
        goto CancelTransferEnd;
    }

    Endpoint = TransferSet->Endpoint;

    ASSERT(LIST_EMPTY(&(Endpoint->TransferSetListHead)) == FALSE);

    //
    // Only move the endpoint forward if removing the first transfer set.
    //

    FirstTransferSet = LIST_VALUE(Endpoint->TransferSetListHead.Next,
                                  DWHCI_TRANSFER_SET,
                                  EndpointListEntry);

    FirstSet = FALSE;
    if (TransferSet == FirstTransferSet) {
        FirstSet = TRUE;
    }

    //
    // Set the error state for the channel. It will either get pulled out of
    // the schedule below or halted, in the case of an active transfer. Once
    // the active transfer halts, it will see why based on this status.
    //

    Transfer->Public.Status = STATUS_OPERATION_CANCELLED;
    Transfer->Public.Error = UsbErrorTransferCancelled;

    //
    // If the transfer set is active on the endpoint, the endpoint has been
    // assigned a channel and the endpoint is actually scheduled on the channel,
    // then halt the channel. Halting a channel is not supported if the root
    // port is not connected. Just remove the transfer set.
    //

    RemoveSet = TRUE;
    if ((Controller->PortConnected != FALSE) &&
        (FirstSet != FALSE) &&
        (Endpoint->Channel != NULL) &&
        (Endpoint->Scheduled != FALSE)) {

        Halted = DwhcipHaltChannel(Controller, Endpoint->Channel);
        if (Halted == FALSE) {
            RemoveSet = FALSE;
        }
    }

    //
    // If the transfer set can be removed because it was not active or the
    // channel was successfully halted, do it. Also complete the transfer and
    // advance the endpoint to the next transfer, if any.
    //

    if (RemoveSet != FALSE) {
        DwhcipRemoveTransferSet(Controller, TransferSet);
        UsbHostProcessCompletedTransfer(Transfer);
        if (FirstSet != FALSE) {
            DwhcipAdvanceEndpoint(Controller, Endpoint);
            DwhcipProcessSchedule(Controller, FALSE);
        }

        Status = STATUS_SUCCESS;

    } else {
        Status = STATUS_TOO_LATE;
    }

CancelTransferEnd:
    DwhcipReleaseControllerLock(Controller, OldRunLevel);
    return Status;
}

KSTATUS
DwhcipGetRootHubStatus (
    PVOID HostControllerContext,
    PUSB_HUB_STATUS HubStatus
    )

/*++

Routine Description:

    This routine queries the host controller for the status of the root hub.

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    HubStatus - Supplies a pointer where the host controller should fill out
        the root hub status.

Return Value:

    STATUS_SUCCESS if the hub status was successfully queried.

    Failure codes if the status could not be queried.

--*/

{

    ULONG AcknowledgeStatus;
    USHORT ChangeBits;
    PDWHCI_CONTROLLER Controller;
    ULONG HardwareStatus;
    PUSB_PORT_STATUS PortStatus;
    USHORT SoftwareStatus;

    Controller = (PDWHCI_CONTROLLER)HostControllerContext;

    ASSERT(Controller->PortCount == DWHCI_HOST_PORT_COUNT);
    ASSERT(HubStatus->PortStatus != NULL);

    HardwareStatus = DWHCI_READ_REGISTER(Controller, DwhciRegisterHostPort);
    SoftwareStatus = 0;

    //
    // Set the software bits that correspond to the queried hardware bits.
    //

    if ((HardwareStatus & DWHCI_HOST_PORT_CONNECT_STATUS) != 0) {
        SoftwareStatus |= USB_PORT_STATUS_CONNECTED;
        switch (HardwareStatus & DWHCI_HOST_PORT_SPEED_MASK) {
        case DWHCI_HOST_PORT_SPEED_LOW:
            HubStatus->PortDeviceSpeed[0] = UsbDeviceSpeedLow;
            break;

        case DWHCI_HOST_PORT_SPEED_FULL:
            HubStatus->PortDeviceSpeed[0] = UsbDeviceSpeedFull;
            break;

        case DWHCI_HOST_PORT_SPEED_HIGH:
            HubStatus->PortDeviceSpeed[0] = UsbDeviceSpeedHigh;
            break;

        default:

            ASSERT(FALSE);

            break;
        }

        Controller->PortConnected = TRUE;

    } else {
        Controller->PortConnected = FALSE;
    }

    if ((HardwareStatus & DWHCI_HOST_PORT_ENABLE) != 0) {
        SoftwareStatus |= USB_PORT_STATUS_ENABLED;
    }

    if ((HardwareStatus & DWHCI_HOST_PORT_RESET) != 0) {
        SoftwareStatus |= USB_PORT_STATUS_RESET;
    }

    if ((HardwareStatus & DWHCI_HOST_PORT_OVER_CURRENT_ACTIVE) != 0) {
        SoftwareStatus |= USB_PORT_STATUS_OVER_CURRENT;
    }

    //
    // If the new software status is different from the current software
    // status, record the change bits and set the new software status.
    //

    PortStatus = &(HubStatus->PortStatus[0]);
    if (SoftwareStatus != PortStatus->Status) {
        ChangeBits = SoftwareStatus ^ PortStatus->Status;

        //
        // Because the change bits correspond with the status bits 1-to-1, just
        // OR in the change bits.
        //

        PortStatus->Change |= ChangeBits;
        PortStatus->Status = SoftwareStatus;
    }

    //
    // Acknowledge the over current change bit if it is set.
    //

    if ((HardwareStatus & DWHCI_HOST_PORT_OVER_CURRENT_CHANGE) != 0) {
        PortStatus->Change |= USB_PORT_STATUS_OVER_CURRENT;
        AcknowledgeStatus = HardwareStatus &
                            ~DWHCI_HOST_PORT_WRITE_TO_CLEAR_MASK;

        AcknowledgeStatus |= DWHCI_HOST_PORT_OVER_CURRENT_CHANGE;
        DWHCI_WRITE_REGISTER(Controller,
                             DwhciRegisterHostPort,
                             AcknowledgeStatus);
    }

    //
    // Acknowledge the port connection status change in the hardware and set
    // the bit in the software's port status change bits. It may be that the
    // port transitioned from connected to connected and the above checks did
    // not pick up the change.
    //

    if ((HardwareStatus & DWHCI_HOST_PORT_CONNECT_STATUS_CHANGE) != 0) {
        PortStatus->Change |= USB_PORT_STATUS_CHANGE_CONNECTED;

        //
        // If the port is not in the middle of a reset, clear the connect
        // status change bit in the hardware by setting it to 1. Resets clear
        // the connect status changed bit.
        //

        if ((HardwareStatus & DWHCI_HOST_PORT_RESET) == 0) {
            AcknowledgeStatus = HardwareStatus &
                                ~DWHCI_HOST_PORT_WRITE_TO_CLEAR_MASK;

            AcknowledgeStatus |= DWHCI_HOST_PORT_CONNECT_STATUS_CHANGE;
            DWHCI_WRITE_REGISTER(Controller,
                                 DwhciRegisterHostPort,
                                 AcknowledgeStatus);
        }
    }

    if ((DwhciDebugFlags & DWHCI_DEBUG_FLAG_PORTS) != 0) {
        RtlDebugPrint(
                 "DWHCI: Controller 0x%x Port %d Status 0x%x. Connected %d, "
                 "Enabled %d, Reset %d, Changed %d.\n",
                 Controller,
                 0,
                 HardwareStatus,
                 (HardwareStatus & DWHCI_HOST_PORT_CONNECT_STATUS) != 0,
                 (HardwareStatus & DWHCI_HOST_PORT_ENABLE) != 0,
                 (HardwareStatus & DWHCI_HOST_PORT_RESET) != 0,
                 (HardwareStatus & DWHCI_HOST_PORT_CONNECT_STATUS_CHANGE) != 0);
    }

    return STATUS_SUCCESS;
}

KSTATUS
DwhcipSetRootHubStatus (
    PVOID HostControllerContext,
    PUSB_HUB_STATUS HubStatus
    )

/*++

Routine Description:

    This routine sets the state of the root hub in the USB host controller. It
    looks at the status change bits for each port in order to determine what
    needs to be set.

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    HubStatus - Supplies a pointer to the status that should be set in the root
        hub.

Return Value:

    STATUS_SUCCESS if the hub state was successfully programmed into the device.

    Failure codes if the status could not be set.

--*/

{

    PDWHCI_CONTROLLER Controller;
    ULONG HardwareStatus;
    ULONG OriginalHardwareStatus;
    PUSB_PORT_STATUS PortStatus;

    Controller = (PDWHCI_CONTROLLER)HostControllerContext;

    ASSERT(Controller->PortCount == DWHCI_HOST_PORT_COUNT);
    ASSERT(HubStatus->PortStatus != NULL);

    PortStatus = &(HubStatus->PortStatus[0]);
    if (PortStatus->Change == 0) {
        return STATUS_SUCCESS;
    }

    HardwareStatus = DWHCI_READ_REGISTER(Controller, DwhciRegisterHostPort);
    HardwareStatus &= ~DWHCI_HOST_PORT_WRITE_TO_CLEAR_MASK;
    OriginalHardwareStatus = HardwareStatus;

    //
    // Clear out the bits that may potentially be adjusted.
    //

    HardwareStatus &= ~(DWHCI_HOST_PORT_ENABLE |
                        DWHCI_HOST_PORT_RESET |
                        DWHCI_HOST_PORT_SUSPEND);

    //
    // Set the hardware bits according to the software bits passed in.
    //

    if ((PortStatus->Change & USB_PORT_STATUS_CHANGE_ENABLED) != 0) {

        //
        // If the port is being enabled, power it on.
        //

        if ((PortStatus->Status & USB_PORT_STATUS_ENABLED) != 0) {
            HardwareStatus |= DWHCI_HOST_PORT_POWER;

        //
        // Otherwise set the enable bit to disable.
        //

        } else {
            HardwareStatus |= DWHCI_HOST_PORT_ENABLE;
        }

        //
        // Acknowledge that the enable bit was handled.
        //

        PortStatus->Change &= ~ USB_PORT_STATUS_CHANGE_ENABLED;
    }

    if ((PortStatus->Change & USB_PORT_STATUS_CHANGE_RESET) != 0) {
        if ((PortStatus->Status & USB_PORT_STATUS_RESET) != 0) {
            HardwareStatus |= DWHCI_HOST_PORT_RESET | DWHCI_HOST_PORT_POWER;
        }

        //
        // Acknowledge that the reset bit was handled.
        //

        PortStatus->Change &= ~USB_PORT_STATUS_CHANGE_RESET;
    }

    //
    // Suspend the port if requested.
    //

    if ((PortStatus->Change & USB_PORT_STATUS_CHANGE_SUSPENDED) != 0) {
        if ((PortStatus->Status & USB_PORT_STATUS_SUSPENDED) != 0) {
            HardwareStatus |= DWHCI_HOST_PORT_SUSPEND;
        }

        //
        // Acknowledge that the suspended bit was handled.
        //

        PortStatus->Change &= ~USB_PORT_STATUS_CHANGE_SUSPENDED;
    }

    //
    // Write out the new value if it is different than the old one. If both the
    // enable (i.e. disable) bit and the reset bit are set, disable the port
    // first using the original hardware status.
    //

    if (HardwareStatus != OriginalHardwareStatus) {
        if (((HardwareStatus & DWHCI_HOST_PORT_ENABLE) != 0) &&
            ((HardwareStatus & DWHCI_HOST_PORT_RESET) != 0)) {

            OriginalHardwareStatus |= DWHCI_HOST_PORT_ENABLE;
            DWHCI_WRITE_REGISTER(Controller,
                                 DwhciRegisterHostPort,
                                 OriginalHardwareStatus);

            HardwareStatus &= ~DWHCI_HOST_PORT_ENABLE;
        }

        DWHCI_WRITE_REGISTER(Controller, DwhciRegisterHostPort, HardwareStatus);
    }

    //
    // If reset was set, wait a bit and then clear the reset flag.
    //

    if ((HardwareStatus & DWHCI_HOST_PORT_RESET) != 0) {
        KeDelayExecution(FALSE, FALSE, 50 * MICROSECONDS_PER_MILLISECOND);
        HardwareStatus = DWHCI_READ_REGISTER(Controller, DwhciRegisterHostPort);
        HardwareStatus &= ~DWHCI_HOST_PORT_WRITE_TO_CLEAR_MASK;
        HardwareStatus &= ~DWHCI_HOST_PORT_RESET;
        DWHCI_WRITE_REGISTER(Controller, DwhciRegisterHostPort, HardwareStatus);
    }

    return STATUS_SUCCESS;
}

RUNLEVEL
DwhcipAcquireControllerLock (
    PDWHCI_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine acquires the given DWHCI controller's lock at dispatch level.

Arguments:

    Controller - Supplies a pointer to the controller to lock.

Return Value:

    Returns the previous run-level, which must be passed in when the controller
    is unlocked.

--*/

{

    RUNLEVEL OldRunLevel;

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeAcquireSpinLock(&(Controller->Lock));
    return OldRunLevel;
}

VOID
DwhcipReleaseControllerLock (
    PDWHCI_CONTROLLER Controller,
    RUNLEVEL OldRunLevel
    )

/*++

Routine Description:

    This routine releases the given DWHCI controller's lock, and returns the
    run-level to its previous value.

Arguments:

    Controller - Supplies a pointer to the controller to unlock.

    OldRunLevel - Supplies the original run level returned when the lock was
        acquired.

Return Value:

    None.

--*/

{

    KeReleaseSpinLock(&(Controller->Lock));
    KeLowerRunLevel(OldRunLevel);
    return;
}

VOID
DwhcipInterruptServiceDpc (
    PDPC Dpc
    )

/*++

Routine Description:

    This routine implements the DWHCI DPC that is queued when an interrupt
    fires.

Arguments:

    Dpc - Supplies a pointer to the DPC that is running.

Return Value:

    None.

--*/

{

    ASSERT(KeGetRunLevel() == RunLevelDispatch);

    DwhcipProcessInterrupt(Dpc->UserData);
    return;
}

VOID
DwhcipProcessInterrupt (
    PVOID Context
    )

/*++

Routine Description:

    This routine performs the work associated with receiving an DWHCI
    interrupt. This routine runs at dispatch level.

Arguments:

    Context - Supplies a pointer to the controller to service.

Return Value:

    None.

--*/

{

    ULONG ChannelInterruptBits[DWHCI_MAX_CHANNELS];
    PDWHCI_CHANNEL Channels;
    PDWHCI_CONTROLLER Controller;
    ULONG Index;
    ULONG InterruptBits;
    RUNLEVEL OldRunLevel;

    Controller = (PDWHCI_CONTROLLER)Context;

    ASSERT(Controller->ChannelCount <= DWHCI_MAX_CHANNELS);

    //
    // Collect the pending interrupt bits and clear them to signal that another
    // DPC will need to be queued for any subsequent interrupts. If the
    // interrupt handle is not yet assigned, just raise to high. This will not
    // result in a priority inversion problem as this code always run at
    // dispatch, and thus cannot pre-empt the interrupt code while it has the
    // lock.
    //

    if (Controller->InterruptHandle == INVALID_HANDLE) {
        OldRunLevel = KeRaiseRunLevel(RunLevelHigh);

    } else {
        OldRunLevel = IoRaiseToInterruptRunLevel(Controller->InterruptHandle);
    }

    KeAcquireSpinLock(&(Controller->InterruptLock));
    InterruptBits = Controller->PendingInterruptBits;
    Controller->PendingInterruptBits = 0;

    //
    // Recording the pending interrupt bits for each channel.
    //

    if ((InterruptBits & DWHCI_CORE_INTERRUPT_HOST_CHANNEL) != 0) {
        Channels = (PDWHCI_CHANNEL)(Controller->Channel);
        for (Index = 0; Index < Controller->ChannelCount; Index += 1) {
            ChannelInterruptBits[Index] = Channels[Index].PendingInterruptBits;
            Channels[Index].PendingInterruptBits = 0;
        }
    }

    KeReleaseSpinLock(&(Controller->InterruptLock));
    KeLowerRunLevel(OldRunLevel);

    //
    // Lock the controller and loop until this routine has caught up with the
    // interrupts.
    //

    OldRunLevel = DwhcipAcquireControllerLock(Controller);

    //
    // If the start-of-frame interrupt fired, then try to schedule some of the
    // periodic transfers.
    //

    if ((InterruptBits & DWHCI_CORE_INTERRUPT_START_OF_FRAME) != 0) {
        DwhcipProcessStartOfFrameInterrupt(Controller);
    }

    //
    // If the port interrupt or the disconnect interrupt fired, then the host
    // port's status changed. Notify the USB core.
    //

    if (((InterruptBits & DWHCI_CORE_INTERRUPT_PORT) != 0) ||
        ((InterruptBits & DWHCI_CORE_INTERRUPT_DISCONNECT) != 0)) {

        UsbHostNotifyPortChange(Controller->UsbCoreHandle);
    }

    //
    // If the host channel interrupt fired, then iterate over the channel
    // interrupt array to determine which channels have work pending.
    //

    if ((InterruptBits & DWHCI_CORE_INTERRUPT_HOST_CHANNEL) != 0) {
        DwhcipProcessChannelInterrupt(Controller, ChannelInterruptBits);
    }

    DwhcipReleaseControllerLock(Controller, OldRunLevel);
    return;
}

VOID
DwhcipProcessStartOfFrameInterrupt (
    PDWHCI_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine processes the inactive periodic schedule to see if any
    transfer's period has expired. This routine assumes that the controller's
    lock is held.

Arguments:

    Controller - Supplies a pointer to the state of the DWHCI controller who
        has reached the start of a frame.

Return Value:

    None.

--*/

{

    ULONG CoreInterruptMask;
    PLIST_ENTRY CurrentEntry;
    PDWHCI_ENDPOINT Endpoint;
    ULONG FrameNumber;
    ULONG NextFrame;
    BOOL ProcessSchedule;

    //
    // The start of frame interrupt could have come in the middle of disabling
    // the interrupt. Check to make sure there is a valid next frame.
    //

    if (Controller->NextFrame == DWHCI_INVALID_FRAME) {
        return;
    }

    //
    // Iterate over the inactive periodic schedule looking for endpoints that
    // have something to submit for the current frame or some frame in the past.
    //

    NextFrame = DWHCI_INVALID_FRAME;
    ProcessSchedule = FALSE;
    FrameNumber = DWHCI_READ_FRAME_NUMBER(Controller);
    CurrentEntry = Controller->PeriodicInactiveListHead.Next;
    while (CurrentEntry != &(Controller->PeriodicInactiveListHead)) {
        Endpoint = LIST_VALUE(CurrentEntry, DWHCI_ENDPOINT, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // Skip any endpoints whose polling interval has not expired, but do
        // record the next frame.
        //

        if (DWHCI_FRAME_LESS_THAN(FrameNumber, Endpoint->NextFrame)) {
            if ((NextFrame == DWHCI_INVALID_FRAME) ||
                DWHCI_FRAME_LESS_THAN(Endpoint->NextFrame, NextFrame)) {

                NextFrame = Endpoint->NextFrame;
            }

            continue;
        }

        LIST_REMOVE(&(Endpoint->ListEntry));
        INSERT_BEFORE(&(Endpoint->ListEntry),
                      &(Controller->PeriodicReadyListHead));

        ProcessSchedule = TRUE;
    }

    //
    // If the inactive list is empty, then disable the start-of-frame interrupt.
    //

    if (LIST_EMPTY(&(Controller->PeriodicInactiveListHead)) != FALSE) {
        CoreInterruptMask = DWHCI_READ_REGISTER(Controller,
                                                DwhciRegisterCoreInterruptMask);

        CoreInterruptMask &= ~DWHCI_CORE_INTERRUPT_START_OF_FRAME;
        DWHCI_WRITE_REGISTER(Controller,
                             DwhciRegisterCoreInterruptMask,
                             CoreInterruptMask);

        ASSERT(NextFrame == DWHCI_INVALID_FRAME);
    }

    //
    // Update the controller's next start of frame to process. This is either
    // the smallest frame number out of the inactive periodic transfers or the
    // invalid frame number if there are no more inactive periodic transfers.
    //

    Controller->NextFrame = NextFrame;

    //
    // If something was switch from the inactive to the ready list, then kick
    // off the schedule.
    //

    if (ProcessSchedule != FALSE) {
        DwhcipProcessSchedule(Controller, TRUE);
    }

    return;
}

VOID
DwhcipSaveChannelInterrupts (
    PDWHCI_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine saves the current interupt status for each channel and clears
    any pending interrupts.

Arguments:

    Controller - Supplies a pointer to the state of the DWHCI controller whose
        channel interrupts are to be saved.

Return Value:

    None.

--*/

{

    ULONG Channel;
    ULONG ChannelBits;
    BOOL ChannelChanged;
    PDWHCI_CHANNEL Channels;
    ULONG Interrupts;

    //
    // A bit is set in the channel interrupt register for every channel that
    // needs attention.
    //

    ChannelBits = DWHCI_READ_REGISTER(Controller,
                                      DwhciRegisterHostChannelInterrupt);

    Channels = (PDWHCI_CHANNEL)(Controller->Channel);
    for (Channel = 0; Channel < Controller->ChannelCount; Channel += 1) {
        ChannelChanged = FALSE;
        if ((ChannelBits & 0x1) != 0) {
            ChannelChanged = TRUE;
        }

        ChannelBits = ChannelBits >> 1;
        if (ChannelChanged == FALSE) {
            continue;
        }

        Interrupts = DWHCI_READ_CHANNEL_REGISTER(Controller,
                                                 DwhciChannelRegisterInterrupt,
                                                 Channel);

        //
        // Acknowledge the interrupts.
        //

        DWHCI_WRITE_CHANNEL_REGISTER(Controller,
                                     DwhciChannelRegisterInterrupt,
                                     Channel,
                                     Interrupts);

        //
        // If there is no endpoint assigned to this channel, then something is
        // not quite right. The interrupts have been acknowledged, but don't
        // record the pending status.
        //

        if (Channels[Channel].Endpoint == NULL) {
            Channels[Channel].PendingInterruptBits = 0;
            continue;
        }

        //
        // Save the unmasked interrupts for this channel.
        //

        Channels[Channel].PendingInterruptBits |= Interrupts;
    }

    return;
}

VOID
DwhcipProcessChannelInterrupt (
    PDWHCI_CONTROLLER Controller,
    PULONG ChannelInterruptBits
    )

/*++

Routine Description:

    This routine handles a channel interrupt detected in the core interrupt
    register. It interates over the host channels processing any channel that
    has pending status.

Arguments:

    Controller - Supplies a pointer to the controller state of the DWHCI
        host controller whose channel interrupt fired.

    ChannelInterruptBits - Supplies an array of pending interrupt bits for
        each channel.

Return Value:

    None.

--*/

{

    BOOL AdvanceEndpoint;
    PDWHCI_CHANNEL Channels;
    PDWHCI_ENDPOINT Endpoint;
    ULONG Index;
    ULONG Interrupts;
    BOOL ProcessSchedule;
    BOOL RemoveSet;
    PDWHCI_TRANSFER Transfer;

    //
    // Iterate over all the channels, looking for pending interrupt bits.
    //

    Channels = (PDWHCI_CHANNEL)(Controller->Channel);
    ProcessSchedule = FALSE;
    for (Index = 0; Index < Controller->ChannelCount; Index += 1) {
        Interrupts = ChannelInterruptBits[Index];
        if (Interrupts == 0) {
            continue;
        }

        //
        // If there is no endpoint assigned to this channel, then something is
        // not quite right. Ignore the interrupts.
        //

        Endpoint = Channels[Index].Endpoint;
        if (Endpoint == NULL) {
            continue;
        }

        //
        // Pre-process endpoints using split transfers. This may modify the
        // interrupt state.
        //

        if (Endpoint->SplitControl != 0) {
            DwhcipProcessSplitEndpoint(Controller, Endpoint, &Interrupts);
        }

        //
        // Pre-process high speed bulk and control transfers to handle the
        // PING protocol.
        //

        if ((Endpoint->Speed == UsbDeviceSpeedHigh) &&
            ((Endpoint->TransferType == UsbTransferTypeBulk) ||
             (Endpoint->TransferType == UsbTransferTypeControl))) {

            DwhcipProcessPingEndpoint(Controller,
                                      Endpoint,
                                      &Interrupts);
        }

        //
        // Get the first transfer for the endpoint. That is the one to which
        // the interrupt status applies. Then process the endpoint.
        //

        Transfer = DwhcipGetEndpointTransfer(Endpoint);

        ASSERT(Transfer != NULL);

        DwhcipProcessPotentiallyCompletedTransfer(Controller,
                                                  Transfer,
                                                  Interrupts,
                                                  &RemoveSet,
                                                  &AdvanceEndpoint);

        if (RemoveSet != FALSE) {
            DwhcipRemoveTransferSet(Controller, Transfer->Set);
            UsbHostProcessCompletedTransfer(Transfer->Set->UsbTransfer);
        }

        //
        // Prepare the endpoint to move onto its next transfer.
        //

        if (AdvanceEndpoint != FALSE) {
            DwhcipAdvanceEndpoint(Controller, Endpoint);
            ProcessSchedule = TRUE;
        }
    }

    //
    // Try to pump other transfers through the schedule if some channels have
    // become available.
    //

    if (ProcessSchedule != FALSE) {
        DwhcipProcessSchedule(Controller, FALSE);
    }

    return;
}

VOID
DwhcipProcessPotentiallyCompletedTransfer (
    PDWHCI_CONTROLLER Controller,
    PDWHCI_TRANSFER Transfer,
    ULONG Interrupts,
    PBOOL RemoveSet,
    PBOOL AdvanceEndpoint
    )

/*++

Routine Description:

    This routine processes a potentially completed transfer, adjusting the USB
    transfer if the transfer errored out or completed.

Arguments:

    Controller - Supplies a pointer to the controller state of the DWHCI host
        controller who owns the potentially completed transfer.

    Transfer - Supplies a pointer to the transfer to evaluate.

    Interrupts - Supplies the interrupt status for this transfer's channel.

    RemoveSet - Supplies a pointer to a boolean that receives TRUE if the
        transfer's set should be removed from the active list due to completion
        or failure, or FALSE otherwise.

    AdvanceEndpoint - Supplies a pointer to a boolean that receives TRUE if the
        transfer's endpoint should be advanced to the next transfer, or FALSE
        otherwise.

Return Value:

    None.

--*/

{

    ULONG BytesRemaining;
    PDWHCI_CHANNEL Channel;
    PDWHCI_ENDPOINT Endpoint;
    ULONG Errors;
    BOOL Halted;
    ULONG LengthTransferred;
    BOOL RemoveTransfer;
    PDWHCI_TRANSFER StatusTransfer;
    ULONG Token;
    PDWHCI_TRANSFER_SET TransferSet;
    BOOL TransferShorted;
    PUSB_TRANSFER UsbTransfer;

    Channel = Transfer->Set->Endpoint->Channel;
    Endpoint = Transfer->Set->Endpoint;
    LengthTransferred = 0;
    RemoveTransfer = FALSE;
    *RemoveSet = FALSE;
    *AdvanceEndpoint = TRUE;
    TransferShorted = FALSE;
    UsbTransfer = &(Transfer->Set->UsbTransfer->Public);

    ASSERT(Channel != NULL);

    //
    // The transfer should not be removed if this routine is reached. Nor
    // should it's transfer set.
    //

    ASSERT(Transfer->SetListEntry.Next != NULL);
    ASSERT(Transfer->Set->EndpointListEntry.Next != NULL);

    //
    // Always read the transfer token to update the endpoint's data toggle.
    //

    Token = DWHCI_READ_CHANNEL_REGISTER(Controller,
                                        DwhciChannelRegisterToken,
                                        Channel->ChannelNumber);

    Endpoint->DataToggle = (Token & DWHCI_CHANNEL_TOKEN_PID_MASK) >>
                           DWHCI_CHANNEL_TOKEN_PID_SHIFT;

    //
    // DATA2 may be returned, so if the toggle is not DATA0, just force it to
    // DATA1.
    //

    if (Endpoint->DataToggle != DWHCI_PID_CODE_DATA_0) {
        Endpoint->DataToggle = DWHCI_PID_CODE_DATA_1;
    }

    ASSERT(Endpoint->DataToggle != DWHCI_PID_CODE_MORE_DATA);

    //
    // If the transfer was already cancelled, then just remove the set and
    // exit.
    //

    if (UsbTransfer->Error == UsbErrorTransferCancelled) {

        ASSERT(UsbTransfer->Status == STATUS_OPERATION_CANCELLED);

        *RemoveSet = TRUE;
        goto ProcessPotentiallyCompletedTransferEnd;
    }

    //
    // If a device I/O error is set in the transfer, then this is just the
    // channel halt operation completing. The AHB error was already handled.
    //

    if (UsbTransfer->Error == UsbErrorTransferDeviceIo) {
        *RemoveSet = TRUE;
        goto ProcessPotentiallyCompletedTransferEnd;
    }

    //
    // If there was an error on the channel, then update the USB transfer's
    // error state.
    //

    Errors = Interrupts & DWHCI_CHANNEL_INTERRUPT_ERROR_MASK;
    if (Errors != 0) {
        *RemoveSet = TRUE;
        UsbTransfer->Status = STATUS_DEVICE_IO_ERROR;
        if ((Errors & DWHCI_CHANNEL_INTERRUPT_STALL) != 0) {
            UsbTransfer->Error = UsbErrorTransferStalled;

        } else if ((Errors & DWHCI_CHANNEL_INTERRUPT_TRANSACTION_ERROR) != 0) {
            UsbTransfer->Error = UsbErrorTransferCrcOrTimeoutError;

        } else if ((Errors & DWHCI_CHANNEL_INTERRUPT_BABBLE_ERROR) != 0) {
            UsbTransfer->Error = UsbErrorTransferBabbleDetected;

        } else if ((Errors &
                    DWHCI_CHANNEL_INTERRUPT_DMA_BUFFER_NOT_AVAILABLE) != 0) {

            UsbTransfer->Error = UsbErrorTransferDataBuffer;

        } else if ((Errors & DWHCI_CHANNEL_INTERRUPT_AHB_ERROR) != 0) {
            UsbTransfer->Error = UsbErrorTransferDeviceIo;
            Halted = DwhcipHaltChannel(Controller, Channel);
            if (Halted == FALSE) {
                *RemoveSet = FALSE;
                *AdvanceEndpoint = FALSE;
            }
        }
    }

    //
    // If the transfer completed, then update the USB transfer's size. It is
    // only valid if the complete bit is set.
    //

    if ((Interrupts & DWHCI_CHANNEL_INTERRUPT_TRANSFER_COMPLETE) != 0) {

        //
        // For IN transfes, the channel token contains the number of unwritten
        // bytes in the transfer buffer.
        //

        if (Transfer->InTransfer != FALSE) {
            BytesRemaining = (Token & DWHCI_CHANNEL_TOKEN_TRANSFER_SIZE_MASK) >>
                             DWHCI_CHANNEL_TOKEN_TRANSFER_SIZE_SHIFT;

            LengthTransferred = Transfer->TransferLength - BytesRemaining;

        //
        // For completed OUT transfers, it is assumed that all the bytes were
        // accepted. There are no bytes remaining.
        //

        } else {
            LengthTransferred = Transfer->TransferLength;
        }

        UsbTransfer->LengthTransferred += LengthTransferred;

        //
        // If the whole set is not already scheduled for removal, process the
        // completed status information to decided what happens to the transfer
        // and/or its set.
        //

        if (*RemoveSet == FALSE) {
            if (Transfer->LastTransfer != FALSE) {
                *RemoveSet = TRUE;

            } else if (LengthTransferred != Transfer->TransferLength) {
                TransferShorted = TRUE;

            } else {
                RemoveTransfer = TRUE;
            }
        }
    }

    //
    // For shorted transfers, either skip ahead to the status phase of a
    // control transfer or just return that the whole set should be removed.
    //

    if (TransferShorted != FALSE) {
        if (Endpoint->TransferType == UsbTransferTypeControl) {
            *RemoveSet = FALSE;

            //
            // The last entry in the transfer set should be the status transfer.
            //

            TransferSet = Transfer->Set;

            ASSERT(LIST_EMPTY(&(TransferSet->TransferListHead)) == FALSE);

            StatusTransfer = LIST_VALUE(TransferSet->TransferListHead.Previous,
                                        DWHCI_TRANSFER,
                                        SetListEntry);

            ASSERT(StatusTransfer->LastTransfer != FALSE);

            //
            // Remove everything from the list by simply re-initializing it and
            // then re-insert the status transfer as the only transfer.
            //

            INITIALIZE_LIST_HEAD(&(TransferSet->TransferListHead));
            INSERT_BEFORE(&(StatusTransfer->SetListEntry),
                          &(TransferSet->TransferListHead));

        } else {
            *RemoveSet = TRUE;
        }

    //
    // Otherwise remove the single transfer if necessary.
    //

    } else if (RemoveTransfer != FALSE) {
        LIST_REMOVE(&(Transfer->SetListEntry));
    }

ProcessPotentiallyCompletedTransferEnd:
    return;
}

VOID
DwhcipRemoveTransferSet (
    PDWHCI_CONTROLLER Controller,
    PDWHCI_TRANSFER_SET TransferSet
    )

/*++

Routine Description:

    This routine removes a transfer set from the schedule. This routine
    assumes that the controller lock is already held.

Arguments:

    Controller - Supplies a pointer to the controller being operated on.

    TransferSet - Supplies a pointer to the set of transfers to remove.

Return Value:

    None.

--*/

{

    LIST_REMOVE(&(TransferSet->EndpointListEntry));
    TransferSet->EndpointListEntry.Next = NULL;
    return;
}

VOID
DwhcipProcessSplitEndpoint (
    PDWHCI_CONTROLLER Controller,
    PDWHCI_ENDPOINT Endpoint,
    PULONG Interrupts
    )

/*++

Routine Description:

    This routine pre-processes a potentially completed transfer for an endpoint
    that must use split transfers.

Arguments:

    Controller - Supplies a pointer to the controller state of the DWHCI host
        controller whose got a split endpoint with a potentially completed
        transfer.

    Endpoint - Supplies a pointer to an endpoint that sends split transfers.

    Interrupts - Supplies a pointer to the interrupt state for the endpoint's
        associated channel. This routine may modify the interrupt state.

Return Value:

    None.

--*/

{

    ULONG EndFrame;
    ULONG Frame;
    ULONG LocalInterrupts;
    PDWHCI_TRANSFER Transfer;

    ASSERT(Endpoint->SplitControl != 0);

    LocalInterrupts = *Interrupts;

    //
    // Get the active transfer on this endpoint.
    //

    Transfer = DwhcipGetEndpointTransfer(Endpoint);

    ASSERT(Transfer != NULL);

    //
    // If this is a start split there are three possible paths: NAK, ACK, or an
    // error.
    //

    if (Transfer->CompleteSplitCount == 0) {

        //
        // A maximum of 3 errors are allowed. If the are fewer than three
        // errors for this transfer, then mask out the errors and retry the
        // start split.
        //

        if ((LocalInterrupts & DWHCI_CHANNEL_INTERRUPT_ERROR_MASK) != 0) {
            Transfer->ErrorCount += 1;
            if (Transfer->ErrorCount < DWHCI_SPLIT_ERROR_MAX) {
                LocalInterrupts &= ~DWHCI_CHANNEL_INTERRUPT_ERROR_MASK;
            }

        //
        // An ACK on a start split rolls over to the complete split.
        //

        } else if ((LocalInterrupts & DWHCI_CHANNEL_INTERRUPT_ACK) != 0) {
            Transfer->CompleteSplitCount = 1;
            LocalInterrupts &= ~DWHCI_CHANNEL_INTERRUPT_TRANSFER_COMPLETE;

        //
        // A NAK on a start split should retry the start split.
        //

        } else if ((LocalInterrupts & DWHCI_CHANNEL_INTERRUPT_NAK) != 0) {
            LocalInterrupts &= ~DWHCI_CHANNEL_INTERRUPT_TRANSFER_COMPLETE;
        }

    //
    // If this is a complete split, then there are five possible paths: NAK,
    // ACK, stall, error, and 'not yet'.
    //

    } else {

        //
        // A stall should cause the transfer to just abort. Set the errors to
        // the max.
        //

        if ((LocalInterrupts & DWHCI_CHANNEL_INTERRUPT_STALL) != 0) {
            Transfer->ErrorCount = DWHCI_SPLIT_ERROR_MAX;
        }

        //
        // A maximum of 3 errors are allowed. If the are fewer than three
        // errors on this endpoint, then mask out the errors. Control and bulk
        // data toggle errors cause the start split to be retried.
        //

        if ((LocalInterrupts & DWHCI_CHANNEL_INTERRUPT_ERROR_MASK) != 0) {
            Transfer->ErrorCount += 1;
            if (Transfer->ErrorCount < DWHCI_SPLIT_ERROR_MAX) {
                if (((Endpoint->TransferType == UsbTransferTypeBulk) ||
                     (Endpoint->TransferType == UsbTransferTypeControl)) &&
                    ((LocalInterrupts &
                      DWHCI_CHANNEL_INTERRUPT_DATA_TOGGLE_ERROR) != 0)) {

                    Transfer->CompleteSplitCount = 0;
                    Transfer->ErrorCount = 0;
                }

                LocalInterrupts &= ~DWHCI_CHANNEL_INTERRUPT_ERROR_MASK;
            }

        //
        // An ACK on a complete split should finish the transfer.
        //

        } else if ((LocalInterrupts & DWHCI_CHANNEL_INTERRUPT_ACK) != 0) {
            LocalInterrupts |= DWHCI_CHANNEL_INTERRUPT_TRANSFER_COMPLETE;

        //
        // A NAK on the complete split causes the start split to be retried.
        //

        } else if ((LocalInterrupts & DWHCI_CHANNEL_INTERRUPT_NAK) != 0) {
            Transfer->CompleteSplitCount = 0;
            Transfer->ErrorCount = 0;
            LocalInterrupts &= ~DWHCI_CHANNEL_INTERRUPT_TRANSFER_COMPLETE;

        //
        // A NYET on the complete split should retry the complete split.
        //

        } else if ((LocalInterrupts & DWHCI_CHANNEL_INTERRUPT_NOT_YET) != 0) {
            LocalInterrupts &= ~DWHCI_CHANNEL_INTERRUPT_TRANSFER_COMPLETE;

            //
            // Interrupt endpoints are the exception. If this is not the last
            // (3rd) complete split or the complete split window has not passed,
            // then NYETs indicate that the complete split should be tried
            // again. Otherwise NYETs count towards the error count and the
            // start split is tried again if the maximum error is yet to be
            // reached.
            //

            if (Endpoint->TransferType == UsbTransferTypeInterrupt) {
                Frame = DWHCI_READ_FRAME_NUMBER(Controller);
                EndFrame = (Endpoint->StartFrame +
                            DWHCI_SPLIT_NOT_YET_FRAME_WINDOW) &
                           DWHCI_FRAME_NUMBER_MAX;

                if (DWHCI_FRAME_LESS_THAN(EndFrame, Frame) != FALSE) {
                    LocalInterrupts |=
                                     DWHCI_CHANNEL_INTERRUPT_TRANSACTION_ERROR;

                    Transfer->CompleteSplitCount = 0;

                } else if (Transfer->CompleteSplitCount >=
                           DWHCI_COMPLETE_SPLIT_MAX) {

                    Transfer->ErrorCount += 1;
                    if (Transfer->ErrorCount >= DWHCI_SPLIT_ERROR_MAX) {
                        LocalInterrupts |=
                                     DWHCI_CHANNEL_INTERRUPT_TRANSACTION_ERROR;
                    }

                    Transfer->CompleteSplitCount = 0;

                } else {
                    Transfer->CompleteSplitCount += 1;
                }
            }
        }
    }

    *Interrupts = LocalInterrupts;
    return;
}

VOID
DwhcipProcessPingEndpoint (
    PDWHCI_CONTROLLER Controller,
    PDWHCI_ENDPOINT Endpoint,
    PULONG Interrupts
    )

/*++

Routine Description:

    This routine pre-processes a potentially completed transfer for an endpoint
    that must use PING protocol.

Arguments:

    Controller - Supplies a pointer to the controller state of the DWHCI host
        controller whose got a high speed bulk or control endpoint with a
        potentially completed transfer.

    Endpoint - Supplies a pointer to an endpoint that implements the PING
        protocol.

    Interrupts - Supplies a pointer to the interrupt state for the endpoint's
        associated channel. This routine may modify the interrupt state.

Return Value:

    None.

--*/

{

    ULONG LocalInterrupts;
    PDWHCI_TRANSFER NextTransfer;
    PDWHCI_TRANSFER Transfer;
    PDWHCI_TRANSFER_SET TransferSet;

    ASSERT(Endpoint->Speed == UsbDeviceSpeedHigh);
    ASSERT((Endpoint->TransferType == UsbTransferTypeBulk) ||
           (Endpoint->TransferType == UsbTransferTypeControl));

    ASSERT(Endpoint->SplitControl == 0);

    LocalInterrupts = *Interrupts;

    //
    // Get the active transfer on this endpoint.
    //

    Transfer = DwhcipGetEndpointTransfer(Endpoint);

    ASSERT(Transfer != NULL);

    TransferSet = Transfer->Set;

    //
    // IN endpoints do not implement the PING protocol.
    //

    if (TransferSet->UsbTransfer->Public.Direction == UsbTransferDirectionIn) {
        return;
    }

    //
    // Newer revisions do not require manual handling of the PING protocol.
    //

    if (Controller->Revision >= DWHCI_AUTOMATIC_PING_REVISION_MININUM) {
        return;
    }

    ASSERT(Endpoint->PingRequired == FALSE);

    //
    // For OUT bulk transfers, NAKs and NYETs require that the PING protocol
    // should be triggered on the next transfer for the endpoint.
    //

    if (Endpoint->TransferType == UsbTransferTypeBulk) {
        if (((LocalInterrupts & DWHCI_CHANNEL_INTERRUPT_NAK) != 0) ||
            ((LocalInterrupts & DWHCI_CHANNEL_INTERRUPT_NOT_YET) != 0)) {

            Endpoint->PingRequired = TRUE;
        }

    //
    // For control transfers, the PING protocol is only required on OUT data or
    // status phases so separate this between SETUP and not setup.
    //

    } else {

        ASSERT(Endpoint->TransferType == UsbTransferTypeControl);

        //
        // The PING protocol is not supported for the SETUP phase. If this is
        // the setup phase completing, then potentially set PING for the next
        // transfer, if it is OUT.
        //

        if ((Transfer->Token & DWHCI_CHANNEL_TOKEN_PID_MASK) ==
            DWHCI_CHANNEL_TOKEN_PID_CODE_SETUP) {

            if ((LocalInterrupts &
                 DWHCI_CHANNEL_INTERRUPT_TRANSFER_COMPLETE) != 0) {

                ASSERT(Transfer->SetListEntry.Next !=
                       &(TransferSet->TransferListHead));

                NextTransfer = LIST_VALUE(Transfer->SetListEntry.Next,
                                          DWHCI_TRANSFER,
                                          SetListEntry);

                if (NextTransfer->InTransfer == FALSE) {
                    Endpoint->PingRequired = TRUE;
                }
            }

        //
        // Handle DATA transfers.
        //

        } else if (Transfer->LastTransfer == FALSE) {

            //
            // A DATA OUT that did not complete and sent NAK or NYET requires
            // a PING when the transfer is resent. Completed DATA OUTs do not
            // need to set the PING, because the status phase goes in the
            // opposite direction.
            //

            if ((Transfer->InTransfer == FALSE) &&
                (((LocalInterrupts & DWHCI_CHANNEL_INTERRUPT_NAK) != 0) ||
                 ((LocalInterrupts & DWHCI_CHANNEL_INTERRUPT_NOT_YET) != 0))) {

                ASSERT((LocalInterrupts &
                        DWHCI_CHANNEL_INTERRUPT_TRANSFER_COMPLETE) == 0);

                Endpoint->PingRequired = TRUE;

            //
            // Otherwise a completed DATA IN will transfer to the status phase,
            // which should begin with the PING protocol, as it is an OUT
            // transfer.
            //

            } else if ((Transfer->InTransfer != FALSE) &&
                       ((LocalInterrupts &
                         DWHCI_CHANNEL_INTERRUPT_TRANSFER_COMPLETE) != 0)) {

                Endpoint->PingRequired = TRUE;
            }

        //
        // Handle OUT status phases.
        //

        } else if ((Transfer->LastTransfer != FALSE) &&
                   (Transfer->InTransfer == FALSE)) {

            //
            // If the OUT status phase NAKs or NYETs, then the PING protocol
            // needs to be invoked on the retry.
            //

            if (((LocalInterrupts & DWHCI_CHANNEL_INTERRUPT_NAK) != 0) ||
                ((LocalInterrupts & DWHCI_CHANNEL_INTERRUPT_NOT_YET) != 0)) {

                Endpoint->PingRequired = TRUE;
            }
        }
    }

    return;
}

VOID
DwhcipFillOutTransferDescriptor (
    PDWHCI_CONTROLLER Controller,
    PDWHCI_TRANSFER_SET TransferSet,
    PDWHCI_TRANSFER DwhciTransfer,
    ULONG Offset,
    ULONG Length,
    BOOL LastTransfer
    )

/*++

Routine Description:

    This routine fills out an DWHCI transfer descriptor.

Arguments:

    Controller - Supplies a pointer to the DWHCI controller.

    TransferSet - Supplies a pointer to the transfer set this transfer belongs
        to.

    DwhciTransfer - Supplies a pointer to DWHCI's transfer descriptor
        information.

    Offset - Supplies the offset from the public transfer physical address that
        this transfer descriptor should be initialize to.

    Length - Supplies the length of the transfer, in bytes.

    LastTransfer - Supplies a boolean indicating if this transfer descriptor
        represents the last transfer in a set. For control transfers, this is
        the status phase where the in/out is reversed and the length had better
        be zero.

Return Value:

    None.

--*/

{

    PDWHCI_ENDPOINT Endpoint;
    ULONG PacketCount;
    UCHAR PidCode;
    ULONG Token;
    PUSB_TRANSFER_INTERNAL Transfer;

    Endpoint = TransferSet->Endpoint;
    Transfer = TransferSet->UsbTransfer;
    DwhciTransfer->LastTransfer = FALSE;
    DwhciTransfer->TransferLength = Length;
    DwhciTransfer->Set = TransferSet;
    DwhciTransfer->ErrorCount = 0;
    DwhciTransfer->PhysicalAddress = Transfer->Public.BufferPhysicalAddress +
                                     Offset;

    //
    // The first packet in a control transfer is always a setup packet and is
    // not an IN transfer.
    //

    PidCode = 0;
    if ((Endpoint->TransferType == UsbTransferTypeControl) && (Offset == 0)) {
        PidCode = DWHCI_PID_CODE_SETUP;
        DwhciTransfer->InTransfer = FALSE;

    //
    // Do it backwards if this is the status phase. Status phases always have
    // a data toggle of 1 and the transfer direction is opposite that of the
    // transfer. The exception is if there was no data phase for the control
    // transfer - just the setup and status phases. In that case, the status
    // phase is always in the IN direction.
    //

    } else if ((Endpoint->TransferType == UsbTransferTypeControl) &&
               (LastTransfer != FALSE)) {

        ASSERT(Length == 0);

        PidCode = DWHCI_PID_CODE_DATA_1;
        if (Offset == sizeof(USB_SETUP_PACKET)) {
            DwhciTransfer->InTransfer = TRUE;

        } else if (Transfer->Public.Direction == UsbTransferDirectionIn) {
            DwhciTransfer->InTransfer = FALSE;

        } else {

            ASSERT(Transfer->Public.Direction == UsbTransferDirectionOut);

            DwhciTransfer->InTransfer = TRUE;
        }

        DwhciTransfer->PhysicalAddress =
                  Controller->ControlStatusBuffer->Fragment[0].PhysicalAddress;

    //
    // Not setup and not status, fill this out like a normal descriptor.
    //

    } else {
        if (Transfer->Public.Direction == UsbTransferDirectionIn) {
            DwhciTransfer->InTransfer = TRUE;

        } else {

            ASSERT(Transfer->Public.Direction == UsbTransferDirectionOut);

            DwhciTransfer->InTransfer = FALSE;
        }
    }

    //
    // Determine which channel interrupts to set.
    //

    switch (Endpoint->TransferType) {
    case UsbTransferTypeIsochronous:

        //
        // TODO: Implement support for isochronous transfers.
        //

        ASSERT(FALSE);

        break;

    case UsbTransferTypeInterrupt:
    case UsbTransferTypeControl:
    case UsbTransferTypeBulk:
        DwhciTransfer->InterruptMask = DWHCI_CHANNEL_INTERRUPT_HALTED |
                                       DWHCI_CHANNEL_INTERRUPT_AHB_ERROR;

        break;

    default:

        ASSERT(FALSE);

        break;
    }

    //
    // If this transfer uses the split procotol, it will always begin with the
    // start split (i.e. a complete split count of zero).
    //

    DwhciTransfer->CompleteSplitCount = 0;

    //
    // Determine the number of packets in the transfer.
    //

    PacketCount = 1;
    if (DwhciTransfer->TransferLength > Endpoint->MaxPacketSize) {
        PacketCount = DwhciTransfer->TransferLength / Endpoint->MaxPacketSize;
        if ((DwhciTransfer->TransferLength % Endpoint->MaxPacketSize) != 0) {
            PacketCount += 1;
        }
    }

    ASSERT(PacketCount <= Endpoint->MaxPacketCount);

    //
    // Initialize the token that is to be written to a channel's transfer setup
    // register when submitting this transfer.
    //

    Token = (PacketCount << DWHCI_CHANNEL_TOKEN_PACKET_COUNT_SHIFT) &
            DWHCI_CHANNEL_TOKEN_PACKET_COUNT_MASK;

    Token |= (PidCode << DWHCI_CHANNEL_TOKEN_PID_SHIFT) &
             DWHCI_CHANNEL_TOKEN_PID_MASK;

    Token |= (DwhciTransfer->TransferLength <<
              DWHCI_CHANNEL_TOKEN_TRANSFER_SIZE_SHIFT) &
             DWHCI_CHANNEL_TOKEN_TRANSFER_SIZE_MASK;

    DwhciTransfer->Token = Token;

    //
    // Add the transfer into the transfer set's queue.
    //

    INSERT_BEFORE(&(DwhciTransfer->SetListEntry),
                  &(TransferSet->TransferListHead));

    if ((DwhciDebugFlags & DWHCI_DEBUG_FLAG_TRANSFERS) != 0) {
        RtlDebugPrint("DWHCI: Adding transfer (0x%08x) to endpoint (0x%08x): "
                      "TOKEN 0x%x, IN 0x%x, LAST 0x%x, INT 0x%08x, "
                      "LENGTH 0x%x.\n",
                      DwhciTransfer,
                      Endpoint,
                      DwhciTransfer->Token,
                      DwhciTransfer->InTransfer,
                      DwhciTransfer->LastTransfer,
                      DwhciTransfer->InterruptMask,
                      DwhciTransfer->TransferLength);
    }

    return;
}

VOID
DwhcipProcessSchedule (
    PDWHCI_CONTROLLER Controller,
    BOOL PeriodicOnly
    )

/*++

Routine Description:

    This routine processes any pending activity on the given host controller's
    periodic and non-periodic schedules. If there are channels available to
    schedule work on, then work will be scheduled. This routine expects the
    controller lock to be held.

Arguments:

    Controller - Supplies a pointer to the state for the DWHCI controller
        whose schedule needs to be processed.

    PeriodicOnly - Supplies a boolean indicating whether or not to only schedule
        periodic transfers.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PDWHCI_ENDPOINT Endpoint;
    KSTATUS Status;

    //
    // If there are any periodic endpoints waiting to be assigned a channel,
    // then try to move the endpoints from the ready list to the active list.
    //

    while (LIST_EMPTY(&(Controller->PeriodicReadyListHead)) == FALSE) {
        Endpoint = LIST_VALUE(Controller->PeriodicReadyListHead.Next,
                              DWHCI_ENDPOINT,
                              ListEntry);

        //
        // Initialize the channel to accept transfers from this endpoint.
        //

        Status = DwhcipAllocateChannel(Controller, Endpoint);
        if (!KSUCCESS(Status)) {
            break;
        }

        LIST_REMOVE(&(Endpoint->ListEntry));
        INSERT_BEFORE(&(Endpoint->ListEntry),
                      &(Controller->PeriodicActiveListHead));
    }

    //
    // Process the active periodic endpoint list to try to push them through
    // the periodic queue.
    //

    CurrentEntry = Controller->PeriodicActiveListHead.Next;
    while (CurrentEntry != &(Controller->PeriodicActiveListHead)) {
        Endpoint = LIST_VALUE(CurrentEntry, DWHCI_ENDPOINT, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (Endpoint->Scheduled != FALSE) {
            continue;
        }

        DwhcipScheduleTransfer(Controller, Endpoint);
    }

    //
    // If only the periodic schedule was requested to be processed, then exit
    // immediately.
    //

    if (PeriodicOnly != FALSE) {
        return;
    }

    //
    // If there are any non-periodic endpoints waiting to be assigned a
    // channel, then try to move the endpoints from the ready list to the
    // active list.
    //

    while (LIST_EMPTY(&(Controller->NonPeriodicReadyListHead)) == FALSE) {
        Endpoint = LIST_VALUE(Controller->NonPeriodicReadyListHead.Next,
                              DWHCI_ENDPOINT,
                              ListEntry);

        //
        // Initialize the channel to accept transfers from this endpoint.
        //

        Status = DwhcipAllocateChannel(Controller, Endpoint);
        if (!KSUCCESS(Status)) {
            break;
        }

        LIST_REMOVE(&(Endpoint->ListEntry));
        INSERT_BEFORE(&(Endpoint->ListEntry),
                      &(Controller->NonPeriodicActiveListHead));
    }

    //
    // Process the active non-periodic endpoint list to try to push them
    // through the non-periodic queue.
    //

    CurrentEntry = Controller->NonPeriodicActiveListHead.Next;
    while (CurrentEntry != &(Controller->NonPeriodicActiveListHead)) {
        Endpoint = LIST_VALUE(CurrentEntry, DWHCI_ENDPOINT, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (Endpoint->Scheduled != FALSE) {
            continue;
        }

        DwhcipScheduleTransfer(Controller, Endpoint);
    }

    return;
}

KSTATUS
DwhcipAllocateChannel (
    PDWHCI_CONTROLLER Controller,
    PDWHCI_ENDPOINT Endpoint
    )

/*++

Routine Description:

    This routine allocates a channel for use by the given endpoint.

Arguments:

    Controller - Supplies a pointer to the state of the DWHCI controller from
        which to allocate a channel.

    Endpoint - Supplies a pointer to the DWHCI endpoint that is to be assigned
        to the channel.

Return Value:

    Status code.

--*/

{

    PDWHCI_CHANNEL Channel;

    //
    // If the free channel list is empty, then exit immediately.
    //

    if (LIST_EMPTY(&(Controller->FreeChannelListHead)) != FALSE) {
        return STATUS_RESOURCE_IN_USE;
    }

    //
    // If this is a periodic endpoint and there is only one channel left, exit,
    // allowing the non-periodic endpoints some guaranteed progress.
    //

    if ((Controller->FreeChannelListHead.Next ==
         Controller->FreeChannelListHead.Previous) &&
        ((Endpoint->TransferType == UsbTransferTypeInterrupt) ||
         (Endpoint->TransferType == UsbTransferTypeIsochronous))) {

         return STATUS_RESOURCE_IN_USE;
    }

    //
    // Allocate the first channel in the free list.
    //

    Channel = LIST_VALUE(Controller->FreeChannelListHead.Next,
                         DWHCI_CHANNEL,
                         FreeListEntry);

    LIST_REMOVE(&(Channel->FreeListEntry));

    ASSERT(Channel->Endpoint == NULL);

    //
    // Associate the allocated channel with the given endpoint.
    //

    Channel->Endpoint = Endpoint;
    Endpoint->Channel = Channel;
    return STATUS_SUCCESS;
}

VOID
DwhcipFreeChannel (
    PDWHCI_CONTROLLER Controller,
    PDWHCI_CHANNEL Channel
    )

/*++

Routine Description:

    This routine frees the given channel from use by an endpoint.

Arguments:

    Controller - Supplies a pointer to the state of the DWHCI controller from
        which to allocate a channel.

    Channel - Supplies a pointer to the DWHCI channel that is to be released.

Return Value:

    None.

--*/

{

    ASSERT(Channel->Endpoint != NULL);

    Channel->Endpoint->Channel = NULL;
    Channel->Endpoint = NULL;
    INSERT_BEFORE(&(Channel->FreeListEntry),
                  &(Controller->FreeChannelListHead));

    return;
}

VOID
DwhcipScheduleTransfer (
    PDWHCI_CONTROLLER Controller,
    PDWHCI_ENDPOINT Endpoint
    )

/*++

Routine Description:

    This routine schedules the next transfer for the given endpoint.

Arguments:

    Controller - Supplies a pointer to the state of the DWHCI controller to
        which the endpoint belongs.

    Endpoint - Supplies a pointer to the endpoint whose next transfer is to be
        scheduled.

Return Value:

    None.

--*/

{

    PDWHCI_CHANNEL Channel;
    ULONG Control;
    ULONG Frame;
    ULONG Interrupts;
    ULONG SplitControl;
    KSTATUS Status;
    ULONG Token;
    PDWHCI_TRANSFER Transfer;

    ASSERT(Endpoint->Channel != NULL);

    Channel = Endpoint->Channel;

    //
    // Find the next transfer for this endpoint. This transfer is the first
    // transfer in the first transfer set.
    //

    Transfer = DwhcipGetEndpointTransfer(Endpoint);

    ASSERT(Transfer != NULL);

    //
    // Initialize the host channel for use by the endpoint. Start by clearing
    // any interrupts on the channel.
    //

    DWHCI_WRITE_CHANNEL_REGISTER(Controller,
                                 DwhciChannelRegisterInterrupt,
                                 Channel->ChannelNumber,
                                 0xFFFFFFFF);

    DWHCI_WRITE_CHANNEL_REGISTER(Controller,
                                 DwhciChannelRegisterInterruptMask,
                                 Channel->ChannelNumber,
                                 Transfer->InterruptMask);

    //
    // Enable host level interrupts for this channel.
    //

    Interrupts = DWHCI_READ_REGISTER(Controller,
                                     DwhciRegisterHostChannelInterruptMask);

    Interrupts |= (1 << Channel->ChannelNumber);
    DWHCI_WRITE_REGISTER(Controller,
                         DwhciRegisterHostChannelInterruptMask,
                         Interrupts);

    //
    // If this is a full or low-speed device, then configure the split register.
    //

    Token = Transfer->Token;
    SplitControl = Endpoint->SplitControl;
    if (SplitControl != 0) {

        ASSERT((Endpoint->Speed == UsbDeviceSpeedLow) ||
               (Endpoint->Speed == UsbDeviceSpeedFull));

        ASSERT((SplitControl & DWHCI_CHANNEL_SPLIT_CONTROL_ENABLE) != 0);

        if (Transfer->CompleteSplitCount != 0) {
            if (Transfer->InTransfer == FALSE) {
                Token &= ~DWHCI_CHANNEL_TOKEN_TRANSFER_SIZE_MASK;
            }

            SplitControl |= DWHCI_CHANNEL_SPLIT_CONTROL_COMPLETE_SPLIT;

        //
        // Interrupt start splits are not allowed to be started in the 6th
        // microframe.
        //

        } else if (Endpoint->TransferType == UsbTransferTypeInterrupt) {
            Frame = DWHCI_READ_FRAME_NUMBER(Controller);
            if ((Frame & 0x7) == 0x6) {
                Status = STATUS_TRY_AGAIN;
                goto ScheduleTransferEnd;
            }

            Endpoint->StartFrame = Frame;
        }
    }

    //
    // Setup up the transfer register based on the transfer token. This
    // includes information on the transfer length, the PID, and number of
    // packets. If the PID is preset in the token, then use what is there,
    // otherwise use the current toggle pid stored in the endpoint.
    //

    if ((Transfer->Token & DWHCI_CHANNEL_TOKEN_PID_MASK) == 0) {
        Token |= (Endpoint->DataToggle << DWHCI_CHANNEL_TOKEN_PID_SHIFT) &
                 DWHCI_CHANNEL_TOKEN_PID_MASK;

    } else {

        ASSERT(Endpoint->TransferType == UsbTransferTypeControl);
    }

    //
    // Set the PING protocol bit in the token if required.
    //

    if (Endpoint->PingRequired != FALSE) {

        ASSERT(Transfer->InTransfer == FALSE);
        ASSERT(Endpoint->Speed == UsbDeviceSpeedHigh);
        ASSERT((Endpoint->TransferType == UsbTransferTypeBulk) ||
               (Endpoint->TransferType == UsbTransferTypeControl));

        ASSERT((Endpoint->TransferType != UsbTransferTypeControl) ||
               ((Token & DWHCI_CHANNEL_TOKEN_PID_MASK) !=
                DWHCI_CHANNEL_TOKEN_PID_CODE_SETUP));

        Token |= DWHCI_CHANNEL_TOKEN_PING;

        //
        // Let the status of this transfer determine if another PING is
        // required.
        //

        Endpoint->PingRequired = FALSE;
    }

    DWHCI_WRITE_CHANNEL_REGISTER(Controller,
                                 DwhciChannelRegisterToken,
                                 Channel->ChannelNumber,
                                 Token);

    //
    // Program the DMA register.
    //

    ASSERT(Transfer->PhysicalAddress == (ULONG)Transfer->PhysicalAddress);
    ASSERT(IS_ALIGNED(Transfer->PhysicalAddress, DWHCI_DMA_ALIGNMENT) != FALSE);

    DWHCI_WRITE_CHANNEL_REGISTER(Controller,
                                 DwhciChannelRegisterDmaAddress,
                                 Channel->ChannelNumber,
                                 (ULONG)Transfer->PhysicalAddress);

    //
    // Program the split control register.
    //

    DWHCI_WRITE_CHANNEL_REGISTER(Controller,
                                 DwhciChannelRegisterSplitControl,
                                 Channel->ChannelNumber,
                                 SplitControl);

    //
    // Execute the final steps, enabling the channel to handle the transfer.
    //

    Control = Endpoint->ChannelControl;
    if (Transfer->InTransfer != FALSE) {
        Control |= DWHCI_CHANNEL_CONTROL_ENDPOINT_DIRECTION_IN;
    }

    switch (Endpoint->TransferType) {
    case UsbTransferTypeIsochronous:
    case UsbTransferTypeInterrupt:

        //
        // Set the odd frame bit if the current frame is even.
        //

        Frame = DWHCI_READ_FRAME_NUMBER(Controller);
        if ((Frame & 0x1) == 0) {
            Control |= DWHCI_CHANNEL_CONTROL_ODD_FRAME;
        }

        break;

    case UsbTransferTypeControl:
    case UsbTransferTypeBulk:
        break;

    default:

        ASSERT(FALSE);

        break;
    }

    ASSERT((Control & DWHCI_CHANNEL_CONTROL_ENABLE) != 0);
    ASSERT((Control & DWHCI_CHANNEL_CONTROL_DISABLE) == 0);

    DWHCI_WRITE_CHANNEL_REGISTER(Controller,
                                 DwhciChannelRegisterControl,
                                 Channel->ChannelNumber,
                                 Control);

    Endpoint->Scheduled = TRUE;
    Status = STATUS_SUCCESS;

ScheduleTransferEnd:
    if (!KSUCCESS(Status)) {

        //
        // Disable interrupts for this channel.
        //

        DWHCI_WRITE_CHANNEL_REGISTER(Controller,
                                     DwhciChannelRegisterInterruptMask,
                                     Channel->ChannelNumber,
                                     0);

        Interrupts = DWHCI_READ_REGISTER(Controller,
                                         DwhciRegisterHostChannelInterruptMask);

        Interrupts &= ~(1 << Channel->ChannelNumber);
        DWHCI_WRITE_REGISTER(Controller,
                             DwhciRegisterHostChannelInterruptMask,
                             Interrupts);

        //
        // This should be an interrupt endpoint and it needs to try again. Just
        // move it back to the inactive list and trigger the start-of-frame
        // interrupt. Release the channel as well.
        //

        ASSERT(Status == STATUS_TRY_AGAIN);
        ASSERT(Endpoint->TransferType == UsbTransferTypeInterrupt);

        LIST_REMOVE(&(Endpoint->ListEntry));
        if (LIST_EMPTY(&(Controller->PeriodicInactiveListHead)) != FALSE) {
            Interrupts = DWHCI_READ_REGISTER(Controller,
                                             DwhciRegisterCoreInterruptMask);

            Interrupts |= DWHCI_CORE_INTERRUPT_START_OF_FRAME;
            DWHCI_WRITE_REGISTER(Controller,
                                 DwhciRegisterCoreInterruptMask,
                                 Interrupts);
        }

        INSERT_BEFORE(&(Endpoint->ListEntry),
                      &(Controller->PeriodicInactiveListHead));

        DwhcipFreeChannel(Controller, Endpoint->Channel);
        Endpoint->Scheduled = FALSE;
    }

    return;
}

VOID
DwhcipAdvanceEndpoint (
    PDWHCI_CONTROLLER Controller,
    PDWHCI_ENDPOINT Endpoint
    )

/*++

Routine Description:

    This routine prepares the given endpoint for its next transfer. This may
    or may not release the channel. This routine assumes that the caller
    will process the host controller's schedule shortly after calling this
    routine.

Arguments:

    Controller - Supplies a pointer to the state of the DWHCI controller that
        owns the given endpoint.

    Endpoint - Supplies a pointer to the endpoint that needs to be advanced
        to its next transfer.

Return Value:

    Returns TRUE if the endpoint had been using a channel and this routine
    released it. Returns FALSE otherwise.

--*/

{

    ULONG Base;
    PDWHCI_CHANNEL Channel;
    ULONG CoreInterruptMask;
    ULONG Delta;
    ULONG FrameNumber;
    BOOL FreeChannel;
    ULONG Interrupts;
    ULONG NextFrame;
    BOOL PeriodicInactiveWasEmpty;
    PDWHCI_TRANSFER Transfer;

    Channel = Endpoint->Channel;
    FreeChannel = FALSE;

    //
    // Disable and clear all interrupts on the current channel.
    //

    if (Channel != NULL) {
        DWHCI_WRITE_CHANNEL_REGISTER(Controller,
                                     DwhciChannelRegisterInterruptMask,
                                     Channel->ChannelNumber,
                                     0);

        DWHCI_WRITE_CHANNEL_REGISTER(Controller,
                                     DwhciChannelRegisterInterrupt,
                                     Channel->ChannelNumber,
                                     0xFFFFFFFF);

        //
        // Disable host level interrupts for this channel.
        //

        Interrupts = DWHCI_READ_REGISTER(Controller,
                                         DwhciRegisterHostChannelInterruptMask);

        Interrupts &= ~(1 << Channel->ChannelNumber);
        DWHCI_WRITE_REGISTER(Controller,
                             DwhciRegisterHostChannelInterruptMask,
                             Interrupts);

        //
        // Assume that the channel will become available for other transfers.
        //

        FreeChannel = TRUE;
    }

    //
    // Before the endpoint is removed, determine the state of the periodic
    // inactive list.
    //

    PeriodicInactiveWasEmpty = FALSE;
    if (LIST_EMPTY(&(Controller->PeriodicInactiveListHead)) != FALSE) {
        PeriodicInactiveWasEmpty = TRUE;
    }

    //
    // Completely remove the endpoint from the schedule.
    //

    LIST_REMOVE(&(Endpoint->ListEntry));

    //
    // If there is more work left to do on this endpoint, then add it back to
    // the appropriate list.
    //

    if (LIST_EMPTY(&(Endpoint->TransferSetListHead)) == FALSE) {
        if ((Endpoint->TransferType == UsbTransferTypeControl) ||
            (Endpoint->TransferType == UsbTransferTypeBulk)) {

            INSERT_BEFORE(&(Endpoint->ListEntry),
                          &(Controller->NonPeriodicReadyListHead));

        } else {

            ASSERT((Endpoint->TransferType == UsbTransferTypeInterrupt) ||
                   (Endpoint->TransferType == UsbTransferTypeIsochronous));

            Transfer = DwhcipGetEndpointTransfer(Endpoint);

            ASSERT(Transfer != NULL);

            FrameNumber = DWHCI_READ_FRAME_NUMBER(Controller);

            //
            // When scheduling a complete split, schedule just ahead of the
            // start split's microframe.
            //

            if (Transfer->CompleteSplitCount != 0) {

                ASSERT(Endpoint->StartFrame != DWHCI_INVALID_FRAME);

                Base = Endpoint->StartFrame;
                Delta = 1 + Transfer->CompleteSplitCount;

            //
            // Otherwise the next (micro)frame is based on the current frame
            // and the poll rate, which is stored in (micro)frames.
            //

            } else {
                Base = FrameNumber;
                Delta = Endpoint->PollRate;
            }

            NextFrame = (Base + Delta) & DWHCI_FRAME_NUMBER_MAX;

            //
            // Start splits are not allowed to start in the 6th microframe and
            // get less time for the complete splits the later they get
            // scheduled within a frame. Schedule them all for the last
            // microframe.
            //

            if ((Endpoint->SplitControl != 0) &&
                (Endpoint->TransferType == UsbTransferTypeInterrupt) &&
                (Transfer->CompleteSplitCount == 0)) {

                NextFrame |= DWHCI_INTERRUPT_SPLIT_FRAME_MASK;
            }

            Endpoint->NextFrame = NextFrame;

            //
            // If the next frame has already come to pass and a channel is
            // assigned to the endpoint, then put the endpoint back on the
            // active list and do not free the channel.
            //

            if ((Channel != NULL) &&
                DWHCI_FRAME_GREATER_THAN_OR_EQUAL(FrameNumber, NextFrame)) {

                INSERT_BEFORE(&(Endpoint->ListEntry),
                              &(Controller->PeriodicActiveListHead));

                FreeChannel = FALSE;

            //
            // Otherwise the endpoint must wait for the start of the
            // appropriate (micro)frame.
            //

            } else {
                if ((Controller->NextFrame == DWHCI_INVALID_FRAME) ||
                    DWHCI_FRAME_LESS_THAN(NextFrame, Controller->NextFrame)) {

                    Controller->NextFrame = NextFrame;
                }

                //
                // Activate the start-of-frame interrupt if the periodic
                // inactive list was empty when checked above.
                //

                if (PeriodicInactiveWasEmpty != FALSE) {
                    CoreInterruptMask = DWHCI_READ_REGISTER(
                                               Controller,
                                               DwhciRegisterCoreInterruptMask);

                    CoreInterruptMask |= DWHCI_CORE_INTERRUPT_START_OF_FRAME;
                    DWHCI_WRITE_REGISTER(Controller,
                                         DwhciRegisterCoreInterruptMask,
                                         CoreInterruptMask);
                }

                INSERT_BEFORE(&(Endpoint->ListEntry),
                              &(Controller->PeriodicInactiveListHead));
            }
        }

    //
    // Otherwise keep the endpoint off of all lists.
    //

    } else {
        Endpoint->NextFrame = 0;
        Endpoint->StartFrame = 0;
        Endpoint->ListEntry.Next = NULL;
    }

    //
    // Release the channel if the endpoint no longer needs it.
    //

    if ((Channel != NULL) && (FreeChannel != FALSE)) {
        DwhcipFreeChannel(Controller, Channel);
    }

    //
    // If this caused the inactive periodic list to become empty, then disable
    // the start-of-frame interrupts.
    //

    if ((PeriodicInactiveWasEmpty == FALSE) &&
        (LIST_EMPTY(&(Controller->PeriodicInactiveListHead)) != FALSE)) {

        CoreInterruptMask = DWHCI_READ_REGISTER(Controller,
                                                DwhciRegisterCoreInterruptMask);

        CoreInterruptMask &= ~DWHCI_CORE_INTERRUPT_START_OF_FRAME;
        DWHCI_WRITE_REGISTER(Controller,
                             DwhciRegisterCoreInterruptMask,
                             CoreInterruptMask);

        Controller->NextFrame = DWHCI_INVALID_FRAME;
    }

    //
    // Note that the endpoint is not scheduled, so that it gets picked up the
    // next time the schedule is processed.
    //

    Endpoint->Scheduled = FALSE;
    return;
}

PDWHCI_TRANSFER
DwhcipGetEndpointTransfer (
    PDWHCI_ENDPOINT Endpoint
    )

/*++

Routine Description:

    This routine returns the first transfer in the given endpoint's queue.

Arguments:

    Endpoint - Supplies a pointer to an endpoint.

Return Value:

    Returns a pointer to the first transfer on the endpoint or NULL if no such
    transfer exists.

--*/

{

    PDWHCI_TRANSFER Transfer;
    PDWHCI_TRANSFER_SET TransferSet;

    //
    // Find the next transfer for this endpoint. This transfer is the first
    // transfer in the first transfer set.
    //

    if (LIST_EMPTY(&(Endpoint->TransferSetListHead)) != FALSE) {
        return NULL;
    }

    TransferSet = LIST_VALUE(Endpoint->TransferSetListHead.Next,
                             DWHCI_TRANSFER_SET,
                             EndpointListEntry);

    if (LIST_EMPTY(&(TransferSet->TransferListHead)) != FALSE) {
        return NULL;
    }

    Transfer = LIST_VALUE(TransferSet->TransferListHead.Next,
                          DWHCI_TRANSFER,
                          SetListEntry);

    return Transfer;
}

KSTATUS
DwhcipSoftReset (
    PDWHCI_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine performs a soft reset of the DWHCI controller.

Arguments:

    Controller - Supplies a pointer to the DWHCI controller state of the
        controller to reset.

Return Value:

    Status code.

--*/

{

    ULONG CoreReset;

    //
    // Wait for the core reset register to report that the AHB is idle.
    //

    while (TRUE) {
        CoreReset = DWHCI_READ_REGISTER(Controller, DwhciRegisterCoreReset);
        if ((CoreReset & DWHCI_CORE_RESET_AHB_MASTER_IDLE) != 0) {
            break;
        }

        KeDelayExecution(FALSE, FALSE, 20 * MICROSECONDS_PER_MILLISECOND);
    }

    //
    // Execute the core soft reset by writing the soft reset bit to the
    // register.
    //

    CoreReset |= DWHCI_CORE_RESET_CORE_SOFT_RESET;
    DWHCI_WRITE_REGISTER(Controller, DwhciRegisterCoreReset, CoreReset);

    //
    // Now wait for the bit to clear.
    //

    while (TRUE) {
        CoreReset = DWHCI_READ_REGISTER(Controller, DwhciRegisterCoreReset);
        if ((CoreReset & DWHCI_CORE_RESET_CORE_SOFT_RESET) == 0) {
            break;
        }

        KeDelayExecution(FALSE, FALSE, 20 * MICROSECONDS_PER_MILLISECOND);
    }

    //
    // Execute a long delay to keep the DWHCI core in host mode.
    //

    KeDelayExecution(FALSE, FALSE, 200 * MICROSECONDS_PER_MILLISECOND);
    return STATUS_SUCCESS;
}

KSTATUS
DwhcipInitializePhy (
    PDWHCI_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine initializes the USB physical layer.

Arguments:

    Controller - Supplies a pointer to the DWHCI controller state of the
        controller whose physical layer is to be initialized.

Return Value:

    Status code.

--*/

{

    ULONG FullSpeedType;
    ULONG Hardware2;
    ULONG HighSpeedType;
    ULONG HostConfiguration;
    KSTATUS Status;
    ULONG UsbConfiguration;
    ULONG UsbFlags;
    ULONG UtmiWidth;

    //
    // Get the high speed type and the full speed type.
    //

    Hardware2 = DWHCI_READ_REGISTER(Controller, DwhciRegisterHardware2);
    HighSpeedType = Hardware2 & DWHCI_HARDWARE2_HIGH_SPEED_MASK;
    FullSpeedType = Hardware2 & DWHCI_HARDWARE2_FULL_SPEED_MASK;

    //
    // If this is a full speed controller, then initialize portions of physical
    // layer that are specific to full speed.
    //

    if (Controller->Speed == UsbDeviceSpeedFull) {

        //
        // Set the PHY select bit in the USB configuration register.
        //

        UsbConfiguration = DWHCI_READ_REGISTER(Controller,
                                               DwhciRegisterUsbConfiguration);

        UsbConfiguration |= DWHCI_USB_CONFIGURATION_PHY_SELECT;
        DWHCI_WRITE_REGISTER(Controller,
                             DwhciRegisterUsbConfiguration,
                             UsbConfiguration);

        //
        // Perform a soft reset.
        //

        Status = DwhcipSoftReset(Controller);
        if (!KSUCCESS(Status)) {
            goto InitializePhysicalLayerEnd;
        }

        //
        // Set the full speed clock to 48 MHz in the host configuration
        // register.
        //

        HostConfiguration = DWHCI_READ_REGISTER(Controller,
                                                DwhciRegisterHostConfiguration);

        HostConfiguration &= ~DWHCI_HOST_CONFIGURATION_CLOCK_RATE_MASK;
        HostConfiguration |= (DWHCI_HOST_CONFIGURATION_CLOCK_48_MHZ <<
                              DWHCI_HOST_CONFIGURATION_CLOCK_RATE_SHIFT);

        DWHCI_WRITE_REGISTER(Controller,
                             DwhciRegisterHostConfiguration,
                             HostConfiguration);

    //
    // Otherwise, this is a high speed controller. Initialize high speed mode
    // in the physical layer.
    //

    } else {

        ASSERT(Controller->Speed == UsbDeviceSpeedHigh);
        ASSERT(HighSpeedType != DWHCI_HARDWARE2_HIGH_SPEED_NOT_SUPPORTED);

        //
        // Configure the USB based on the high speed type.
        //

        UsbConfiguration = DWHCI_READ_REGISTER(Controller,
                                               DwhciRegisterUsbConfiguration);

        if (HighSpeedType == DWHCI_HARDWARE2_HIGH_SPEED_ULPI) {
            UsbConfiguration &= ~(DWHCI_USB_CONFIGURATION_PHY_INTERFACE_16 |
                                  DWHCI_USB_CONFIGURATION_DDR_SELECT |
                                  DWHCI_USB_CONFIGURATION_MODE_SELECT_MASK);

            UsbConfiguration |= DWHCI_USB_CONFIGURATION_MODE_SELECT_ULPI;

        } else {

            ASSERT((HighSpeedType == DWHCI_HARDWARE2_HIGH_SPEED_UTMI) ||
                   (HighSpeedType == DWHCI_HARDWARE2_HIGH_SPEED_UTMI_ULPI));

            UsbConfiguration &= ~(DWHCI_USB_CONFIGURATION_MODE_SELECT_MASK |
                                  DWHCI_USB_CONFIGURATION_PHY_INTERFACE_16);

            UsbConfiguration |= DWHCI_USB_CONFIGURATION_MODE_SELECT_UTMI;

            //
            // Enable the physical interface 16 if the UTMI width is not 8 bit.
            //

            UtmiWidth = DWHCI_READ_REGISTER(Controller, DwhciRegisterHardware4);
            UtmiWidth &= ~DWHCI_HARDWARE4_UTMI_PHYSICAL_DATA_WIDTH_MASK;
            if (UtmiWidth != DWHCI_HARDWARE4_UTMI_PHYSICAL_DATA_WIDTH_8_BIT) {
                UsbConfiguration |= DWHCI_USB_CONFIGURATION_PHY_INTERFACE_16;
            }
        }

        DWHCI_WRITE_REGISTER(Controller,
                             DwhciRegisterUsbConfiguration,
                             UsbConfiguration);

        //
        // Perform a soft reset.
        //

        Status = DwhcipSoftReset(Controller);
        if (!KSUCCESS(Status)) {
            goto InitializePhysicalLayerEnd;
        }

        //
        // Set the high speed clock to 30-60 MHz in the host configuration
        // register.
        //

        HostConfiguration = DWHCI_READ_REGISTER(Controller,
                                                DwhciRegisterHostConfiguration);

        HostConfiguration &= ~DWHCI_HOST_CONFIGURATION_CLOCK_RATE_MASK;
        HostConfiguration |= (DWHCI_HOST_CONFIGURATION_CLOCK_30_60_MHZ <<
                              DWHCI_HOST_CONFIGURATION_CLOCK_RATE_SHIFT);

        DWHCI_WRITE_REGISTER(Controller,
                             DwhciRegisterHostConfiguration,
                             HostConfiguration);
    }

    //
    // Perform operations that are common to high and full speed.
    //

    UsbConfiguration = DWHCI_READ_REGISTER(Controller,
                                           DwhciRegisterUsbConfiguration);

    UsbFlags = DWHCI_USB_CONFIGURATION_ULPI_FULL_SPEED_LOW_SPEED_SELECT |
               DWHCI_USB_CONFIGURATION_ULPI_CLOCK_SUSPEND_MODE;

    if ((HighSpeedType == DWHCI_HARDWARE2_HIGH_SPEED_ULPI) &&
        (FullSpeedType == DWHCI_HARDWARE2_FULL_SPEED_DEDICATED)) {

        UsbConfiguration |= UsbFlags;

    } else {
        UsbConfiguration &= ~UsbFlags;
    }

    DWHCI_WRITE_REGISTER(Controller,
                         DwhciRegisterUsbConfiguration,
                         UsbConfiguration);

InitializePhysicalLayerEnd:
    return Status;
}

KSTATUS
DwhcipInitializeUsb (
    PDWHCI_CONTROLLER Controller,
    ULONG UsbCapabilities
    )

/*++

Routine Description:

    This routine initialize the USB register for the DWHCI host controller.

Arguments:

    Controller - Supplies a pointer to the DWHCI controller state of the
        controller whose USB register is to be initialized.

    UsbCapabilities - Supplies USB capability bits saved from the USB
        configuration register before the reset.

Return Value:

    Status code.

--*/

{

    ULONG Hardware2;
    ULONG Mask;
    ULONG Mode;
    KSTATUS Status;
    ULONG UsbConfiguration;

    Mask = DWHCI_USB_CONFIGURATION_HNP_CAPABLE |
           DWHCI_USB_CONFIGURATION_SRP_CAPABLE;

    ASSERT((UsbCapabilities & ~Mask) == 0);

    UsbConfiguration = DWHCI_READ_REGISTER(Controller,
                                           DwhciRegisterUsbConfiguration);

    UsbConfiguration &= ~Mask;
    Hardware2 = DWHCI_READ_REGISTER(Controller, DwhciRegisterHardware2);
    Mode = Hardware2 & DWHCI_HARDWARE2_MODE_MASK;
    Status = STATUS_SUCCESS;
    switch (Mode) {

        //
        // Not all controllers are made equal. Some that advertise HNP/SRP do
        // not actually support it and these bits must remain zero. Leave it up
        // to ACPI to set these bits. The supplied capabilities should hold the
        // values set by ACPI.
        //

        case DWHCI_HARDWARE2_MODE_HNP_SRP:
            UsbConfiguration |= UsbCapabilities;
            break;

        case DWHCI_HARDWARE2_MODE_SRP_ONLY:
        case DWHCI_HARDWARE2_MODE_SRP_DEVICE:
        case DWHCI_HARDWARE2_MODE_SRP_HOST:
            UsbConfiguration |= DWHCI_USB_CONFIGURATION_SRP_CAPABLE;
            break;

        case DWHCI_HARDWARE2_MODE_NO_HNP_SRP:
        case DWHCI_HARDWARE2_MODE_NO_SRP_DEVICE:
        case DWHCI_HARDWARE2_MODE_NO_SRP_HOST:
            break;

        default:

            ASSERT(FALSE);

            Status = STATUS_INVALID_CONFIGURATION;
            break;
    }

    if (KSUCCESS(Status)) {
        DWHCI_WRITE_REGISTER(Controller,
                             DwhciRegisterUsbConfiguration,
                             UsbConfiguration);
    }

    return Status;
}

KSTATUS
DwhcipInitializeHostMode (
    PDWHCI_CONTROLLER Controller,
    ULONG ReceiveFifoSize,
    ULONG NonPeriodicTransmitFifoSize,
    ULONG PeriodicTransmitFifoSize
    )

/*++

Routine Description:

    This routine initializes the DWHCI controller in host mode.

Arguments:

    Controller - Supplies a pointer to the DWHCI controller state of the
        controller whose USB register is to be initialized.

    ReceiveFifoSize - Supplies the receive FIFO size to set if the FIFO's are
        dynamic.

    NonPeriodicTransmitFifoSize - Supplies the non-periodic transmit FIFO size
        to set if the FIFO's are dynamic. This includes the FIFO offset.

    PeriodicTransmitFifoSize - Supplies the periodic transmit FIFO size to
        set if the FIFO's are dynamic. This includes the FIFO offset.

Return Value:

    Status code.

--*/

{

    PDWHCI_CHANNEL Channels;
    ULONG Control;
    ULONG Hardware2;
    ULONG HostConfiguration;
    ULONG Index;
    ULONG OtgControl;
    ULONG PortStatus;
    KSTATUS Status;

    //
    // Restart the PHY clock.
    //

    DWHCI_WRITE_REGISTER(Controller, DwhciRegisterPowerAndClock, 0);

    //
    // Initialize the speed of the host controller.
    //

    if (Controller->Speed == UsbDeviceSpeedFull) {
        HostConfiguration = DWHCI_READ_REGISTER(Controller,
                                                DwhciRegisterHostConfiguration);

        HostConfiguration |= DWHCI_HOST_CONFIGURATION_FULL_SPEED_LOW_SPEED_ONLY;
        DWHCI_WRITE_REGISTER(Controller,
                             DwhciRegisterHostConfiguration,
                             HostConfiguration);
    }

    //
    // If dynamic FIFO sizing is allowed, then set the FIFO sizes and
    // starting addresses using the provided values. Otherwise use what is
    // programmed in the registers.
    //

    Hardware2 = DWHCI_READ_REGISTER(Controller, DwhciRegisterHardware2);
    if ((Hardware2 & DWHCI_HARDWARE2_DYNAMIC_FIFO) != 0) {
        DWHCI_WRITE_REGISTER(Controller,
                             DwhciRegisterReceiveFifoSize,
                             ReceiveFifoSize);

        DWHCI_WRITE_REGISTER(Controller,
                             DwhciRegisterNonPeriodicFifoSize,
                             NonPeriodicTransmitFifoSize);

        DWHCI_WRITE_REGISTER(Controller,
                             DwhciRegisterPeriodicFifoSize,
                             PeriodicTransmitFifoSize);
    }

    //
    // Clear the Host Set HNP Enable in the OTG Control Register.
    //

    OtgControl = DWHCI_READ_REGISTER(Controller, DwhciRegisterOtgControl);
    OtgControl &= ~DWHCI_OTG_CONTROL_HOST_SET_HNP_ENABLE;
    DWHCI_WRITE_REGISTER(Controller, DwhciRegisterOtgControl, OtgControl);

    //
    // Flush the FIFOs.
    //

    DwhcipFlushFifo(Controller, TRUE, DWHCI_CORE_RESET_TRANSMIT_FIFO_FLUSH_ALL);
    DwhcipFlushFifo(Controller, FALSE, 0);

    //
    // First disable all the channels.
    //

    for (Index = 0; Index < Controller->ChannelCount; Index += 1) {
        Control = DWHCI_READ_CHANNEL_REGISTER(Controller,
                                              DwhciChannelRegisterControl,
                                              Index);

        Control &= ~(DWHCI_CHANNEL_CONTROL_ENDPOINT_DIRECTION_IN |
                     DWHCI_CHANNEL_CONTROL_ENABLE);

        Control |= DWHCI_CHANNEL_CONTROL_DISABLE;
        DWHCI_WRITE_CHANNEL_REGISTER(Controller,
                                     DwhciChannelRegisterControl,
                                     Index,
                                     Control);
    }

    //
    // Reset every channel and add them to the list of free channels.
    //

    Channels = (PDWHCI_CHANNEL)(Controller->Channel);
    for (Index = 0; Index < Controller->ChannelCount; Index += 1) {
        Status = DwhcipResetChannel(Controller, Index);
        if (!KSUCCESS(Status)) {
            goto InitializeHostModeEnd;
        }

        //
        // Since the channel was just disabled, add it to the free list.
        //

        ASSERT(Channels[Index].Endpoint == NULL);

        INSERT_BEFORE(&(Channels[Index].FreeListEntry),
                      &(Controller->FreeChannelListHead));
    }

    //
    // Initialize the power for the host controller.
    //

    PortStatus = DWHCI_READ_REGISTER(Controller, DwhciRegisterHostPort);
    if ((PortStatus & DWHCI_HOST_PORT_POWER) == 0) {
        PortStatus |= DWHCI_HOST_PORT_POWER;
        PortStatus &= ~DWHCI_HOST_PORT_WRITE_TO_CLEAR_MASK;
        DWHCI_WRITE_REGISTER(Controller, DwhciRegisterHostPort, PortStatus);
    }

    //
    // Disable all channel interrupts.
    //

    DWHCI_WRITE_REGISTER(Controller, DwhciRegisterHostChannelInterruptMask, 0);

InitializeHostModeEnd:
    return Status;
}

VOID
DwhcipFlushFifo (
    PDWHCI_CONTROLLER Controller,
    BOOL TransmitFifo,
    ULONG TransmitFifoMask
    )

/*++

Routine Description:

    This routine flushes either the one receive FIFO or the specified transmit
    FIFO.

Arguments:

    Controller - Supplies a pointer to the DWHCI controller state of the
        controller whose FIFO is to be flushed.

    TransmitFifo - Supplies a boolean indicating whether or not the flush is
        for a transmit FIFO.

    TransmitFifoMask - Supplies a bitmask of transmission FIFOs to flush. See
        DWHCI_CORE_RESET_TRAMSIT_FIFO_FLUSH_* for available options.

Return Value:

    None.

--*/

{

    ULONG CoreResetMask;
    ULONG CoreResetValue;

    //
    // Write the core reset register to initiate the FIFO flush.
    //

    if (TransmitFifo == FALSE) {
        CoreResetValue = DWHCI_CORE_RESET_RECEIVE_FIFO_FLUSH;
        CoreResetMask = CoreResetValue;

    } else {

        ASSERT((TransmitFifoMask &
                ~DWHCI_CORE_RESET_TRANSMIT_FIFO_FLUSH_MASK) ==
               0);

        CoreResetValue = TransmitFifoMask;
        CoreResetMask = DWHCI_CORE_RESET_TRANSMIT_FIFO_FLUSH;
    }

    DWHCI_WRITE_REGISTER(Controller, DwhciRegisterCoreReset, CoreResetValue);

    //
    // Wait for the mask to go to zero.
    //

    while (TRUE) {
        CoreResetValue = DWHCI_READ_REGISTER(Controller,
                                             DwhciRegisterCoreReset);

        if ((CoreResetValue & CoreResetMask) == 0) {
            break;
        }

        KeDelayExecution(FALSE, FALSE, 10);
    }

    KeDelayExecution(FALSE, FALSE, 10);
    return;
}

KSTATUS
DwhcipResetChannel (
    PDWHCI_CONTROLLER Controller,
    ULONG ChannelNumber
    )

/*++

Routine Description:

    This routine resets the given channel for the supplied DWHCI controller.

Arguments:

    Controller - Supplies a pointer to the controller state of the DWHCI
        controller whose channel is to be reset.

    ChannelNumber - Supplies the number of the channel to be reset.

Return Value:

    Status code.

--*/

{

    ULONG Control;

    //
    // Reset the channel by setting both the enable and disable bits and then
    // wait for the enable bit to clear.
    //

    Control = DWHCI_READ_CHANNEL_REGISTER(Controller,
                                          DwhciChannelRegisterControl,
                                          ChannelNumber);

    Control &= ~DWHCI_CHANNEL_CONTROL_ENDPOINT_DIRECTION_IN;
    Control |= (DWHCI_CHANNEL_CONTROL_ENABLE |
                DWHCI_CHANNEL_CONTROL_DISABLE);

    DWHCI_WRITE_CHANNEL_REGISTER(Controller,
                                 DwhciChannelRegisterControl,
                                 ChannelNumber,
                                 Control);

    while (TRUE) {
        Control = DWHCI_READ_CHANNEL_REGISTER(Controller,
                                              DwhciChannelRegisterControl,
                                              ChannelNumber);

        if ((Control & DWHCI_CHANNEL_CONTROL_ENABLE) == 0) {
            break;
        }

        KeDelayExecution(FALSE, FALSE, 10);
    }

    return STATUS_SUCCESS;
}

BOOL
DwhcipHaltChannel (
    PDWHCI_CONTROLLER Controller,
    PDWHCI_CHANNEL Channel
    )

/*++

Routine Description:

    This routine halts the given channel that belongs to the specified host
    controller.

Arguments:

    Controller - Supplies a pointer to the controller state of the DWHCI
        controller whose channel is to be halted.

    Channel - Supplies a pointer to the channel to be halted.

Return Value:

    Returns TRUE if the channel was successfully halted, or FALSE if an
    asynchronous halt was scheduled.

--*/

{

    ULONG ChannelControl;
    ULONG Interrupts;

    ASSERT(Channel->Endpoint != NULL);

    //
    // Make sure that the channel will only interrupt if it is halted.
    //

    DWHCI_WRITE_CHANNEL_REGISTER(Controller,
                                 DwhciChannelRegisterInterruptMask,
                                 Channel->ChannelNumber,
                                 DWHCI_CHANNEL_INTERRUPT_HALTED);

    //
    // Clear any other interrupts.
    //

    DWHCI_WRITE_CHANNEL_REGISTER(Controller,
                                 DwhciChannelRegisterInterrupt,
                                 Channel->ChannelNumber,
                                 ~DWHCI_CHANNEL_INTERRUPT_HALTED);

    //
    // If the channel is not currently enabled, then it is not active. There
    // should be no need to halt it.
    //

    ChannelControl = DWHCI_READ_CHANNEL_REGISTER(Controller,
                                                 DwhciChannelRegisterControl,
                                                 Channel->ChannelNumber);

    if ((ChannelControl & DWHCI_CHANNEL_CONTROL_ENABLE) == 0) {
        return TRUE;
    }

    //
    // Enable host level interrupts for this channel.
    //

    Interrupts = DWHCI_READ_REGISTER(Controller,
                                     DwhciRegisterHostChannelInterruptMask);

    Interrupts |= (1 << Channel->ChannelNumber);
    DWHCI_WRITE_REGISTER(Controller,
                         DwhciRegisterHostChannelInterruptMask,
                         Interrupts);

    //
    // Reset the channel by enabling and disabling it.
    //

    ChannelControl |= DWHCI_CHANNEL_CONTROL_DISABLE |
                      DWHCI_CHANNEL_CONTROL_ENABLE;

    DWHCI_WRITE_CHANNEL_REGISTER(Controller,
                                 DwhciChannelRegisterControl,
                                 Channel->ChannelNumber,
                                 ChannelControl);

    return FALSE;
}

