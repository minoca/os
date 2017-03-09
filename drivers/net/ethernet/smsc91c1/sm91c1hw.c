/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sm91c1hw.c

Abstract:

    This module implements device support for the SMSC91C111 LAN Ethernet
    Controller.

Author:

    Chris Stevens 16-Apr-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/net/netdrv.h>
#include "sm91c1.h"

//
// --------------------------------------------------------------------- Macros
//
#define SM91C1_WRITE_ZERO_TO_MI(_Device)                                   \
    Sm91c1pWriteRegister(_Device, Sm91c1RegisterManagementInterface, 0x8); \
    Sm91c1pWriteRegister(_Device, Sm91c1RegisterManagementInterface, 0xC); \
    Sm91c1pWriteRegister(_Device, Sm91c1RegisterManagementInterface, 0x8);

#define SM91C1_WRITE_ONE_TO_MI(_Device)                                    \
    Sm91c1pWriteRegister(_Device, Sm91c1RegisterManagementInterface, 0x9); \
    Sm91c1pWriteRegister(_Device, Sm91c1RegisterManagementInterface, 0xD); \
    Sm91c1pWriteRegister(_Device, Sm91c1RegisterManagementInterface, 0x9);

#define SM91C1_WRITE_Z_TO_MI(_Device)                                      \
    Sm91c1pWriteRegister(_Device, Sm91c1RegisterManagementInterface, 0x0); \
    Sm91c1pWriteRegister(_Device, Sm91c1RegisterManagementInterface, 0x4); \
    Sm91c1pWriteRegister(_Device, Sm91c1RegisterManagementInterface, 0x0);

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum number of pending transfers allowed.
//

#define SM91C1_MAX_TRANSMIT_PACKET_LIST_COUNT 64

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
Sm91c1pSendPacket (
    PSM91C1_DEVICE Device,
    PNET_PACKET_BUFFER Packet
    );

VOID
Sm91c1pReceivePacket (
    PSM91C1_DEVICE Device
    );

VOID
Sm91c1pInitializePhy (
    PSM91C1_DEVICE Device
    );

VOID
Sm91c1pUpdateFilterMode (
    PSM91C1_DEVICE Device
    );

KSTATUS
Sm91c1pReadMacAddress (
    PSM91C1_DEVICE Device
    );

USHORT
Sm91c1pReadRegister (
    PSM91C1_DEVICE Device,
    SM91C1_REGISTER Register
    );

VOID
Sm91c1pWriteRegister (
    PSM91C1_DEVICE Device,
    SM91C1_REGISTER Register,
    USHORT Value
    );

USHORT
Sm91c1pReadMdio (
    PSM91C1_DEVICE Device,
    SM91C1_MII_REGISTER Register
    );

VOID
Sm91c1pWriteMdio (
    PSM91C1_DEVICE Device,
    SM91C1_MII_REGISTER Register,
    USHORT Value
    );

VOID
Sm91c1SynchronizeMdio (
    PSM91C1_DEVICE Device
    );

//
// -------------------------------------------------------------------- Globals
//

BOOL Sm91c1DisablePacketDropping = FALSE;

// ------------------------------------------------------------------ Functions
//

KSTATUS
Sm91c1Send (
    PVOID DeviceContext,
    PNET_PACKET_LIST PacketList
    )

/*++

Routine Description:

    This routine sends data through the network.

Arguments:

    DeviceContext - Supplies a pointer to the device context associated with
        the link down which this data is to be sent.

    PacketList - Supplies a pointer to a list of network packets to send. Data
        in these packets may be modified by this routine, but must not be used
        once this routine returns.

Return Value:

    STATUS_SUCCESS if all packets were sent.

    STATUS_RESOURCE_IN_USE if some or all of the packets were dropped due to
    the hardware being backed up with too many packets to send.

    Other failure codes indicate that none of the packets were sent.

--*/

{

    BOOL AllocatePacket;
    PSM91C1_DEVICE Device;
    USHORT InterruptMask;
    USHORT MmuCommand;
    UINTN PacketListCount;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    AllocatePacket = FALSE;
    Device = (PSM91C1_DEVICE)DeviceContext;

    //
    // If there is any room in the packet list (or dropping packets is
    // disabled), add all of the packets to the list waiting to be sent.
    //

    KeAcquireQueuedLock(Device->Lock);
    PacketListCount = Device->TransmitPacketList.Count;
    if ((PacketListCount < SM91C1_MAX_TRANSMIT_PACKET_LIST_COUNT) ||
        (Sm91c1DisablePacketDropping != FALSE)) {

        NET_APPEND_PACKET_LIST(PacketList, &(Device->TransmitPacketList));
        AllocatePacket = TRUE;
        Status = STATUS_SUCCESS;

    //
    // Otherwise report that the resource is use as it is too busy to handle
    // more packets.
    //

    } else {
        Status = STATUS_RESOURCE_IN_USE;
    }

    //
    // If necessary and an allocation isn't already in flight, allocate a
    // packet. The actual sending of a packet is handled when the allocate
    // interrupt is fired.
    //

    if ((AllocatePacket != FALSE) && (Device->AllocateInProgress == FALSE)) {
        Device->AllocateInProgress = TRUE;
        MmuCommand = (SM91C1_MMU_OPERATION_ALLOCATE_FOR_TRANSMIT <<
                      SM91C1_MMU_COMMAND_OPERATION_SHIFT) &
                     SM91C1_MMU_COMMAND_OPERATION_MASK;

        Sm91c1pWriteRegister(Device, Sm91c1RegisterMmuCommand, MmuCommand);

        //
        // Re-enable the allocation interrupt. Do this after the allocate
        // command is set because the previous allocate interrupt is not
        // cleared until a new allocate command is sent.
        //

        InterruptMask = Sm91c1pReadRegister(Device,
                                            Sm91c1RegisterInterruptMask);

        InterruptMask |= SM91C1_INTERRUPT_ALLOCATE;
        Sm91c1pWriteRegister(Device,
                             Sm91c1RegisterInterruptMask,
                             InterruptMask);
    }

    KeReleaseQueuedLock(Device->Lock);
    return Status;
}

KSTATUS
Sm91c1GetSetInformation (
    PVOID DeviceContext,
    NET_LINK_INFORMATION_TYPE InformationType,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets the network device layer's link information.

Arguments:

    DeviceContext - Supplies a pointer to the device context associated with
        the link for which information is being set or queried.

    InformationType - Supplies the type of information being queried or set.

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the data
        buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or a
        set operation (TRUE).

Return Value:

    Status code.

--*/

{

    PULONG BooleanOption;
    ULONG Capabilities;
    PSM91C1_DEVICE Device;
    PULONG Flags;
    KSTATUS Status;

    Status = STATUS_SUCCESS;
    Device = (PSM91C1_DEVICE)DeviceContext;
    switch (InformationType) {
    case NetLinkInformationChecksumOffload:
        if (*DataSize != sizeof(ULONG)) {
            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        if (Set != FALSE) {
            Status = STATUS_NOT_SUPPORTED;
            break;
        }

        Flags = (PULONG)Data;
        *Flags = Device->EnabledCapabilities &
                 NET_LINK_CAPABILITY_CHECKSUM_MASK;

        break;

    case NetLinkInformationPromiscuousMode:
        if (*DataSize != sizeof(ULONG)) {
            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        BooleanOption = (PULONG)Data;
        if (Set == FALSE) {
            if ((Device->EnabledCapabilities &
                 NET_LINK_CAPABILITY_PROMISCUOUS_MODE) != 0) {

                *BooleanOption = TRUE;

            } else {
                *BooleanOption = FALSE;
            }

            break;
        }

        //
        // Fail if promiscuous mode is not supported.
        //

        if ((Device->SupportedCapabilities &
             NET_LINK_CAPABILITY_PROMISCUOUS_MODE) == 0) {

            Status = STATUS_NOT_SUPPORTED;
            break;
        }

        KeAcquireQueuedLock(Device->Lock);
        Capabilities = Device->EnabledCapabilities;
        if (*BooleanOption != FALSE) {
            Capabilities |= NET_LINK_CAPABILITY_PROMISCUOUS_MODE;

        } else {
            Capabilities &= ~NET_LINK_CAPABILITY_PROMISCUOUS_MODE;
        }

        if ((Capabilities ^ Device->EnabledCapabilities) != 0) {
            Device->EnabledCapabilities = Capabilities;
            Sm91c1pUpdateFilterMode(Device);
        }

        KeReleaseQueuedLock(Device->Lock);
        break;

    default:
        Status = STATUS_NOT_SUPPORTED;
        break;
    }

    return Status;
}

KSTATUS
Sm91c1pInitializeDeviceStructures (
    PSM91C1_DEVICE Device
    )

/*++

Routine Description:

    This routine performs housekeeping preparation for resetting and enabling
    an SM91C1 device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    ULONG IoBufferFlags;
    KSTATUS Status;

    KeInitializeSpinLock(&(Device->InterruptLock));
    KeInitializeSpinLock(&(Device->BankLock));
    NET_INITIALIZE_PACKET_LIST(&(Device->TransmitPacketList));
    Device->SupportedCapabilities |= NET_LINK_CAPABILITY_PROMISCUOUS_MODE;
    Device->SelectedBank = -1;

    ASSERT(Device->Lock == NULL);

    Device->Lock = KeCreateQueuedLock();
    if (Device->Lock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS;
    Device->ReceiveIoBuffer = MmAllocateNonPagedIoBuffer(0,
                                                         MAX_ULONGLONG,
                                                         0,
                                                         SM91C1_MAX_PACKET_SIZE,
                                                         IoBufferFlags);

    if (Device->ReceiveIoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    Status = STATUS_SUCCESS;

InitializeDeviceStructuresEnd:
    return Status;
}

VOID
Sm91c1pDestroyDeviceStructures (
    PSM91C1_DEVICE Device
    )

/*++

Routine Description:

    This routine performs destroy any device structures allocated for the
    SM91C1 device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    if (Device->ReceiveIoBuffer == NULL) {
        MmFreeIoBuffer(Device->ReceiveIoBuffer);
    }

    return;
}

KSTATUS
Sm91c1pInitialize (
    PSM91C1_DEVICE Device
    )

/*++

Routine Description:

    This routine initializes and enables the SMSC91C1 device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    BOOL LinkUp;
    USHORT MmuCommand;
    KSTATUS Status;
    USHORT Value;

    //
    // Reset the device.
    //

    Sm91c1pWriteRegister(Device,
                         Sm91c1RegisterReceiveControl,
                         SM91C1_RECEIVE_CONTROL_SOFT_RESET);

    Sm91c1pWriteRegister(Device, Sm91c1RegisterReceiveControl, 0);

    //
    // Delay here to let the reset settle down.
    //

    KeDelayExecution(FALSE, FALSE, 50 * MICROSECONDS_PER_MILLISECOND);

    //
    // Disable all interrupts.
    //

    Sm91c1pWriteRegister(Device, Sm91c1RegisterInterruptMask, 0);

    //
    // Enable the power by setting the EPH Power Enable bit in the
    // configuration register.
    //

    Value = Sm91c1pReadRegister(Device, Sm91c1RegisterConfiguration);
    Value |= SM91C1_CONFIGURATION_REGISTER_EPH_POWER_ENABLE;
    Sm91c1pWriteRegister(Device, Sm91c1RegisterConfiguration, Value);

    //
    // Clear the power down bit in the PHY MII control register.
    //

    Value = Sm91c1pReadMdio(Device, Sm91c1MiiRegisterBasicControl);
    Value &= ~SM91C1_MII_BASIC_CONTROL_POWER_DOWN;
    Sm91c1pWriteMdio(Device, Sm91c1MiiRegisterBasicControl, Value);

    //
    // Reset the MMU.
    //

    MmuCommand = (SM91C1_MMU_OPERATION_RESET <<
                  SM91C1_MMU_COMMAND_OPERATION_SHIFT) &
                 SM91C1_MMU_COMMAND_OPERATION_MASK;

    Sm91c1pWriteRegister(Device, Sm91c1RegisterMmuCommand, MmuCommand);

    //
    // Initialize the PHY, starting auto-negotiation.
    //

    Sm91c1pInitializePhy(Device);

    //
    // Set the transmit packets to auto-release.
    //

    Value = Sm91c1pReadRegister(Device, Sm91c1RegisterControl);
    Value |= SM91C1_CONTROL_AUTO_RELEASE;
    Sm91c1pWriteRegister(Device, Sm91c1RegisterControl, Value);

    //
    // Enable transmitter by setting the TXENA bit in the Transmit Control
    // Register.
    //

    Sm91c1pWriteRegister(Device,
                         Sm91c1RegisterTransmitControl,
                         SM91C1_TRANSMIT_CONTROL_ENABLE);

    //
    // Enable the receiver by setting the RXENA bit in the Receive Control
    // Register.
    //

    Sm91c1pWriteRegister(Device,
                         Sm91c1RegisterReceiveControl,
                         SM91C1_RECEIVE_CONTROL_ENABLE);

    //
    // Get the MAC address out of the EEPROM.
    //

    Status = Sm91c1pReadMacAddress(Device);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    //
    // Set the initial filter mode. This acts based on the enabled capabilities.
    //

    Sm91c1pUpdateFilterMode(Device);

    //
    // Notify the networking core of this new link now that the device is ready
    // to send and receive data, pending media being present.
    //

    Status = Sm91c1pAddNetworkDevice(Device);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    //
    // If the network link is up, notify networking core.
    //

    LinkUp = FALSE;
    Value = Sm91c1pReadRegister(Device, Sm91c1RegisterEphStatus);
    if ((Value & SM91C1_EPH_STATUS_LINK_OK) != 0) {
        LinkUp = TRUE;

    } else {
        Value = Sm91c1pReadMdio(Device, Sm91c1MiiRegisterBasicStatus);
        if (((Value & SM91C1_MII_BASIC_STATUS_LINK_STATUS) != 0) &&
            ((Value & SM91C1_MII_BASIC_STATUS_AUTONEGOTIATE_COMPLETE) != 0)) {

            LinkUp = TRUE;
        }
    }

    if (LinkUp != FALSE) {

        //
        // TODO: Get the real device speed when generic MII support is added.
        //

        NetSetLinkState(Device->NetworkLink, TRUE, NET_SPEED_100_MBPS);
    }

    //
    // Clear all the interrupts and then enable the desired ones.
    //

    Sm91c1pWriteRegister(Device, Sm91c1RegisterInterrupt, 0xFF);
    Sm91c1pWriteRegister(Device,
                         Sm91c1RegisterInterruptMask,
                         SM91C1_DEFAULT_INTERRUPTS);

    Status = STATUS_SUCCESS;

InitializeEnd:
    return Status;
}

INTERRUPT_STATUS
Sm91c1pInterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the SM91C1 interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the SM91c1 device
        structure.

Return Value:

    Interrupt status.

--*/

{

    PSM91C1_DEVICE Device;
    USHORT Interrupts;
    USHORT InterruptsMask;
    INTERRUPT_STATUS InterruptStatus;
    USHORT PacketNumber;
    USHORT PhyInterrupts;

    Device = (PSM91C1_DEVICE)Context;
    InterruptStatus = InterruptStatusNotClaimed;
    PhyInterrupts = 0;

    //
    // Read the interrupt register.
    //

    Interrupts = Sm91c1pReadRegister(Device, Sm91c1RegisterInterrupt);
    InterruptsMask = Sm91c1pReadRegister(Device, Sm91c1RegisterInterruptMask);
    Interrupts &= InterruptsMask;
    if (Interrupts != 0) {
        KeAcquireSpinLock(&(Device->InterruptLock));

        //
        // If the MD interrupt bit is set, then gather the interrupt state from
        // the PHY MII. This read clears the interrupts as well.
        //

        if ((Interrupts & SM91C1_INTERRUPT_MD) != 0) {
            PhyInterrupts = Sm91c1pReadMdio(Device, Sm91c1MiiRegisterInterrupt);
        }

        //
        // The allocate interrupt remains high until the next interrupt
        // attempt. Unmask it now.
        //

        if ((Interrupts & SM91C1_INTERRUPT_ALLOCATE) != 0) {

            ASSERT(Device->AllocateInProgress != FALSE);

            InterruptsMask &= ~SM91C1_INTERRUPT_ALLOCATE;
            Sm91c1pWriteRegister(Device,
                                 Sm91c1RegisterInterruptMask,
                                 InterruptsMask);
        }

        //
        // The receive interrupt remains high until the receive FIFO is empty,
        // but only one receive interrupt can really be handled at a time.
        // Unmask it until it's handled.
        //

        if ((Interrupts & SM91C1_INTERRUPT_RECEIVE) != 0) {
            InterruptsMask &= ~SM91C1_INTERRUPT_RECEIVE;
            Sm91c1pWriteRegister(Device,
                                 Sm91c1RegisterInterruptMask,
                                 InterruptsMask);
        }

        //
        // The device is set to auto-release transmit packets. If a packet
        // interrupt is fired, that means there was a transmit failure. Save
        // the packet.
        //

        if ((Interrupts & SM91C1_INTERRUPT_TRANSMIT) != 0) {
            PacketNumber = Sm91c1pReadRegister(Device,
                                               Sm91c1RegisterTransmitFifo);

            ASSERT((PacketNumber & SM91C1_FIFO_PORTS_TRANSMIT_EMPTY) == 0);

            PacketNumber &= SM91C1_FIFO_PORTS_TRANSMIT_PACKET_NUMBER_MASK;
        }

        InterruptStatus = InterruptStatusClaimed;
        RtlAtomicOr32(&(Device->PendingInterrupts), Interrupts);
        RtlAtomicOr32(&(Device->PendingPhyInterrupts), PhyInterrupts);
        if ((Interrupts & SM91C1_INTERRUPT_TRANSMIT) != 0) {
            Device->PendingTransmitPacket = PacketNumber;
        }

        Device->PendingInterrupts |= Interrupts;
        Device->PendingPhyInterrupts |= PhyInterrupts;

        //
        // Clear the pending interrupt bits that can be acknowledged through
        // standard means.
        //

        Interrupts &= SM91C1_ACKNOWLEDGE_INTERRUPT_MASK;
        if (Interrupts != 0) {
            Sm91c1pWriteRegister(Device, Sm91c1RegisterInterrupt, Interrupts);
        }

        KeReleaseSpinLock(&(Device->InterruptLock));
    }

    return InterruptStatus;
}

INTERRUPT_STATUS
Sm91c1pInterruptServiceWorker (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the SM91C1 low level interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the SM91c1 device
        structure.

Return Value:

    Interrupt status.

--*/

{

    PSM91C1_DEVICE Device;
    PLIST_ENTRY FirstEntry;
    ULONG Interrupts;
    USHORT InterruptsMask;
    USHORT MmuCommand;
    RUNLEVEL OldRunLevel;
    PNET_PACKET_BUFFER Packet;
    USHORT PendingPacket;
    ULONG PhyInterrupts;
    USHORT PointerValue;
    USHORT StatusWord;
    USHORT Value;

    Device = (PSM91C1_DEVICE)Context;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Clear out the pending bits.
    //

    Interrupts = RtlAtomicExchange32(&(Device->PendingInterrupts), 0);
    PhyInterrupts = RtlAtomicExchange32(&(Device->PendingPhyInterrupts), 0);
    PendingPacket = Device->PendingTransmitPacket;
    if ((Interrupts == 0) && (PhyInterrupts == 0)) {
        return InterruptStatusNotClaimed;
    }

    ASSERT((PhyInterrupts == 0) ||
           ((Interrupts & SM91C1_INTERRUPT_MD) != 0));

    //
    // Handle link status changes.
    //

    if ((Interrupts & SM91C1_INTERRUPT_MD) != 0) {
        OldRunLevel = IoRaiseToInterruptRunLevel(Device->InterruptHandle);
        KeAcquireSpinLock(&(Device->InterruptLock));
        Value = Sm91c1pReadMdio(Device, Sm91c1MiiRegisterBasicStatus);
        KeReleaseSpinLock(&(Device->InterruptLock));
        KeLowerRunLevel(OldRunLevel);
        if ((Value & SM91C1_MII_BASIC_STATUS_LINK_STATUS) != 0) {
            if ((Value & SM91C1_MII_BASIC_STATUS_AUTONEGOTIATE_COMPLETE) != 0) {

                //
                // TODO: Get the real device speed when generic MII support is
                // added.
                //

                NetSetLinkState(Device->NetworkLink, TRUE, NET_SPEED_100_MBPS);
            }

        } else {
            NetSetLinkState(Device->NetworkLink, FALSE, 0);
        }
    }

    //
    // If the transmit interrupt was returned, check the transmit status.
    //

    if ((Interrupts & SM91C1_INTERRUPT_TRANSMIT) != 0 ) {
        KeAcquireQueuedLock(Device->Lock);
        Sm91c1pWriteRegister(Device, Sm91c1RegisterPacketNumber, PendingPacket);
        PointerValue = SM91C1_POINTER_READ |
                       SM91C1_POINTER_AUTO_INCREMENT |
                       SM91C1_POINTER_TRANSMIT;

        Sm91c1pWriteRegister(Device, Sm91c1RegisterPointer, PointerValue);
        StatusWord = Sm91c1pReadRegister(Device, Sm91c1RegisterData);

        //
        // Release the packet now that its status has been retrieved.
        //

        MmuCommand = (SM91C1_MMU_OPERATION_RELEASE_PACKET <<
                      SM91C1_MMU_COMMAND_OPERATION_SHIFT) &
                     SM91C1_MMU_COMMAND_OPERATION_MASK;

        Sm91c1pWriteRegister(Device, Sm91c1RegisterMmuCommand, MmuCommand);
        KeReleaseQueuedLock(Device->Lock);
        RtlDebugPrint("SM91C1: TX failed with status 0x%04x.\n", StatusWord);

        //
        // Re-enable transmission. It was disabled when the packet failed.
        //

        Sm91c1pWriteRegister(Device,
                             Sm91c1RegisterTransmitControl,
                             SM91C1_TRANSMIT_CONTROL_ENABLE);
    }

    //
    // If the receive interrupt was returned, process the data.
    //

    if ((Interrupts & SM91C1_INTERRUPT_RECEIVE) != 0) {
        Sm91c1pReceivePacket(Device);

        //
        // Re-enable the receive interrupt. It was disabled by the ISR.
        //

        InterruptsMask = Sm91c1pReadRegister(Device,
                                             Sm91c1RegisterInterruptMask);

        ASSERT((InterruptsMask & SM91C1_INTERRUPT_RECEIVE) == 0);

        InterruptsMask |= SM91C1_INTERRUPT_RECEIVE;
        Sm91c1pWriteRegister(Device,
                             Sm91c1RegisterInterruptMask,
                             InterruptsMask);
    }

    //
    // If a packet was allocated and there are packets to transmit, try to
    // send some data.
    //

    if ((Interrupts & SM91C1_INTERRUPT_ALLOCATE) != 0) {
        Packet = NULL;
        KeAcquireQueuedLock(Device->Lock);

        //
        // Send the first packet on the transmission list using the packet
        // that was allocated.
        //

        if (NET_PACKET_LIST_EMPTY(&(Device->TransmitPacketList)) == FALSE) {
            FirstEntry = Device->TransmitPacketList.Head.Next;
            Packet = LIST_VALUE(FirstEntry, NET_PACKET_BUFFER, ListEntry);
            NET_REMOVE_PACKET_FROM_LIST(Packet, &(Device->TransmitPacketList));
            Sm91c1pSendPacket(Device, Packet);
        }

        //
        // If the list is still not empty then allocate another packet.
        //

        if (NET_PACKET_LIST_EMPTY(&(Device->TransmitPacketList)) == FALSE) {

            ASSERT(Device->AllocateInProgress != FALSE);

            MmuCommand = (SM91C1_MMU_OPERATION_ALLOCATE_FOR_TRANSMIT <<
                          SM91C1_MMU_COMMAND_OPERATION_SHIFT) &
                         SM91C1_MMU_COMMAND_OPERATION_MASK;

            Sm91c1pWriteRegister(Device, Sm91c1RegisterMmuCommand, MmuCommand);

            //
            // Re-enable the allocation interrupt. Do this after the allocate
            // command is set because the previous allocate interrupt is not
            // cleared until a new allocate command is sent.
            //

            InterruptsMask = Sm91c1pReadRegister(Device,
                                                 Sm91c1RegisterInterruptMask);

            ASSERT((InterruptsMask & SM91C1_INTERRUPT_ALLOCATE) == 0);

            InterruptsMask |= SM91C1_INTERRUPT_ALLOCATE;
            Sm91c1pWriteRegister(Device,
                                 Sm91c1RegisterInterruptMask,
                                 InterruptsMask);

        //
        // Otherwise note that no allocations are in progress, meaning that the
        // next send call should trigger an allocation.
        //

        } else {
            Device->AllocateInProgress = FALSE;
        }

        KeReleaseQueuedLock(Device->Lock);

        //
        // If a packet was transmitted, release it now.
        //

        if (Packet != NULL) {
            NetFreeBuffer(Packet);
        }
    }

    return InterruptStatusClaimed;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
Sm91c1pSendPacket (
    PSM91C1_DEVICE Device,
    PNET_PACKET_BUFFER Packet
    )

/*++

Routine Description:

    This routine sends the given packet using the packet sitting in the
    allocation result register. This routine assumes that the device spin lock
    is held.

Arguments:

    Device - Supplies a pointer to the SM91C1 device that owns the packet.

    Packet - Supplies a pointer to the network packet to send.

Return Value:

    None.

--*/

{

    BYTE AllocationResult;
    USHORT ByteCount;
    PUSHORT Data;
    ULONG DataSize;
    PBYTE Footer;
    PUSHORT Header;
    USHORT MmuCommand;
    BYTE PacketNumber;
    USHORT PointerValue;

    ASSERT(KeIsQueuedLockHeld(Device->Lock) != FALSE);

    //
    // There should be space in the packet for the header.
    //

    ASSERT(Packet->DataOffset == SM91C1_PACKET_HEADER_SIZE);

    //
    // Get the current data size.
    //

    DataSize = Packet->FooterOffset - Packet->DataOffset;

    ASSERT(DataSize == (USHORT)DataSize);

    Packet->DataOffset -= SM91C1_PACKET_HEADER_SIZE;
    Header = Packet->Buffer;

    //
    // Initialize the SM91c111 packet header. The first two bytes are the
    // status word. This gets set to 0. The second word is the byte count,
    // which includes the data size, the status word, the byte count word,
    // and the control word. The byte count is always even because any odd
    // byte in the data is included in the lower byte of the control word.
    //

    *Header = 0x0;
    ByteCount = DataSize +
                SM91C1_PACKET_HEADER_SIZE +
                SM91C1_PACKET_FOOTER_SIZE;

    *(Header + 1) = ALIGN_RANGE_DOWN(ByteCount, sizeof(USHORT));
    Footer = Packet->Buffer + Packet->FooterOffset;

    //
    // If the original byte count was odd, then the footer points at the
    // high byte of the control word. Set the ODD bit there. The low byte
    // of the control word correctly contains the last byte of data.
    //

    if ((ByteCount & 0x1) != 0) {
        *Footer = SM91C1_CONTROL_BYTE_ODD;
        Packet->FooterOffset += 1;

    //
    // Otherwise, the footer points at the low byte of the control word.
    // Zero the entire control word.
    //

    } else {
        *Footer = 0;
        *(Footer + 1) = 0;
        Packet->FooterOffset += 2;
    }

    //
    // Read the allocated packet from the allocation result register.
    //

    AllocationResult = Sm91c1pReadRegister(Device,
                                           Sm91c1RegisterAllocationResult);

    ASSERT((AllocationResult & SM91C1_ALLOCATION_RESULT_FAILED) == 0);

    PacketNumber = AllocationResult &
                   SM91C1_ALLOCATION_RESULT_PACKET_NUMBER_MASK;

    //
    // Write the packet number to the packet number register.
    //

    Sm91c1pWriteRegister(Device, Sm91c1RegisterPacketNumber, PacketNumber);

    //
    // Initialize the pointer register for transmit, write, and auto-increment.
    //

    PointerValue = SM91C1_POINTER_WRITE |
                   SM91C1_POINTER_AUTO_INCREMENT |
                   SM91C1_POINTER_TRANSMIT;

    Sm91c1pWriteRegister(Device, Sm91c1RegisterPointer, PointerValue);

    //
    // Now write the packet data into the data register.
    //

    Data = Packet->Buffer;
    DataSize = Packet->FooterOffset - Packet->DataOffset;

    ASSERT(IS_ALIGNED(DataSize, sizeof(USHORT)) != FALSE);
    ASSERT(DataSize <= Packet->BufferSize);

    while (DataSize != 0) {
        Sm91c1pWriteRegister(Device, Sm91c1RegisterData, *Data);
        Data += 1;
        DataSize -= sizeof(USHORT);
    }

    //
    // Queue the packet. It will get automatically release once it is sent.
    //

    MmuCommand = (SM91C1_MMU_OPERATION_QUEUE_PACKET_FOR_TRANSMIT <<
                  SM91C1_MMU_COMMAND_OPERATION_SHIFT) &
                 SM91C1_MMU_COMMAND_OPERATION_MASK;

    Sm91c1pWriteRegister(Device, Sm91c1RegisterMmuCommand, MmuCommand);
    return;
}

VOID
Sm91c1pReceivePacket (
    PSM91C1_DEVICE Device
    )

/*++

Routine Description:

    This routine handles receiving and processing a packet for the SM91c111 LAN
    Ethernet Controller.

Arguments:

    Device - Supplies a pointer to the SM91C1 device.

Return Value:

    None.

--*/

{

    USHORT ByteCount;
    USHORT BytesRemaining;
    BYTE ControlByte;
    USHORT ControlWord;
    PUSHORT Data;
    ULONG Index;
    USHORT MmuCommand;
    NET_PACKET_BUFFER Packet;
    USHORT PacketNumber;
    ULONG PacketSize;
    USHORT PointerValue;
    USHORT StatusWord;

    //
    // Read the packet number from the received FIFO.
    //

    PacketNumber = Sm91c1pReadRegister(Device, Sm91c1RegisterReceiveFifo);
    if ((PacketNumber & SM91C1_FIFO_PORTS_RECEIVE_EMPTY) != 0) {
        RtlDebugPrint("SM91C1: Receive interrupt lacks packet.\n");
        return;
    }

    //
    // Acquire the lock to protect access to the pointer and data registers.
    //

    KeAcquireQueuedLock(Device->Lock);

    //
    // Set the pointer register to receive, read, and auto-increment.
    //

    PointerValue = SM91C1_POINTER_READ |
                   SM91C1_POINTER_AUTO_INCREMENT |
                   SM91C1_POINTER_RECEIVE;

    Sm91c1pWriteRegister(Device, Sm91c1RegisterPointer, PointerValue);

    //
    // Read the status word.
    //

    StatusWord = Sm91c1pReadRegister(Device, Sm91c1RegisterData);

    //
    // Read the byte count and calculate the packet size. The byte count
    // contains the header, footer, and CRC size.
    //

    ByteCount = Sm91c1pReadRegister(Device, Sm91c1RegisterData);
    PacketSize = ByteCount -
                 (SM91C1_PACKET_HEADER_SIZE +
                  SM91C1_PACKET_FOOTER_SIZE +
                  SM91C1_PACKET_CRC_SIZE);

    //
    // Read the data out of the data register and into the receive I/O buffer.
    //

    Data = Device->ReceiveIoBuffer->Fragment[0].VirtualAddress;
    BytesRemaining = PacketSize;

    ASSERT((BytesRemaining & 0x1) == 0);

    while (BytesRemaining != 0) {
        *Data = Sm91c1pReadRegister(Device, Sm91c1RegisterData);
        Data += 1;
        BytesRemaining -= 2;
    }

    //
    // Read the CRC.
    //

    for (Index = 0; Index < SM91C1_PACKET_CRC_SIZE; Index += sizeof(USHORT)) {
        Sm91c1pReadRegister(Device, Sm91c1RegisterData);
    }

    //
    // Read the control word. If the high byte (the control byte) indicates
    // that the packet has an odd length, the the low byte is the last byte of
    // data.
    //

    ControlWord = Sm91c1pReadRegister(Device, Sm91c1RegisterData);
    ControlByte = (BYTE)(ControlWord >> 8);
    if ((ControlByte & SM91C1_CONTROL_BYTE_ODD) != 0) {

        ASSERT((StatusWord & 0x1000) != 0);

        *Data = (BYTE)ControlWord;
        PacketSize += 1;
    }

    //
    // Relesae the lock as use of the data register is done. The receive buffer
    // is protected as there is only ever one receive in flight at a time.
    //

    KeReleaseQueuedLock(Device->Lock);

    //
    // Initialize the packet and notify the networking core.
    //

    Packet.Buffer = Device->ReceiveIoBuffer->Fragment[0].VirtualAddress;
    Packet.BufferPhysicalAddress =
                          Device->ReceiveIoBuffer->Fragment[0].PhysicalAddress;

    Packet.IoBuffer = Device->ReceiveIoBuffer;
    Packet.Flags = 0;
    Packet.BufferSize = PacketSize;
    Packet.DataSize = PacketSize;
    Packet.DataOffset = 0;
    Packet.FooterOffset = PacketSize;
    NetProcessReceivedPacket(Device->NetworkLink, &Packet);

    //
    // Release the packet.
    //

    MmuCommand = (SM91C1_MMU_OPERATION_RECEiVE_FIFO_REMOVE_AND_RELEASE <<
                  SM91C1_MMU_COMMAND_OPERATION_SHIFT) &
                 SM91C1_MMU_COMMAND_OPERATION_MASK;

    Sm91c1pWriteRegister(Device, Sm91c1RegisterMmuCommand, MmuCommand);
    return;
}

VOID
Sm91c1pInitializePhy (
    PSM91C1_DEVICE Device
    )

/*++

Routine Description:

    This routine initializes the PHY on the SMSC91C111.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    USHORT PhyControl;
    ULONG Value;

    //
    // Enable auto-negotiation and set the LED state. LED A remains in the
    // default 10/100 link detected state and LED B gets set to full-duplex.
    //

    PhyControl = SM91C1_PHY_CONTROL_AUTONEGOTIATION |
                 SM91C1_PHY_CONTROL_LED_SELECT_0B |
                 SM91C1_PHY_CONTROL_LED_SELECT_1B;

    Sm91c1pWriteRegister(Device, Sm91c1RegisterPhyControl, PhyControl);

    //
    // Reset the PHY.
    //

    Sm91c1pWriteMdio(Device,
                     Sm91c1MiiRegisterBasicControl,
                     SM91C1_MII_BASIC_CONTROL_RESET);

    while (TRUE) {
        KeDelayExecution(FALSE, FALSE, 50 * MICROSECONDS_PER_MILLISECOND);
        Value = Sm91c1pReadMdio(Device, Sm91c1MiiRegisterBasicControl);
        if ((Value & SM91C1_MII_BASIC_CONTROL_RESET) == 0) {
            break;
        }
    }

    //
    // Start the auto-negotiation process.
    //

    Value = Sm91c1pReadMdio(Device, Sm91c1MiiRegisterBasicControl);
    Value |= SM91C1_MII_BASIC_CONTROL_ENABLE_AUTONEGOTIATION;
    Sm91c1pWriteMdio(Device, Sm91c1MiiRegisterBasicControl, Value);

    //
    // Read the interrupt status register to clear the bits.
    //

    Sm91c1pReadMdio(Device, Sm91c1MiiRegisterInterrupt);

    //
    // Write the interrupt mask.
    //

    Value = SM91C1_MII_INTERRUPT_STATUS_LINK_FAIL |
            SM91C1_MII_INTERRUPT_STATUS_INTERRUPT;

    Sm91c1pWriteMdio(Device, Sm91c1MiiRegisterInterruptMask, ~Value);
    return;
}

VOID
Sm91c1pUpdateFilterMode (
    PSM91C1_DEVICE Device
    )

/*++

Routine Description:

    This routine updates an SMSC91C1 device's filter mode based on the
    currently enabled capabilities.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    USHORT Value;

    Value = Sm91c1pReadRegister(Device, Sm91c1RegisterReceiveControl);
    if ((Device->EnabledCapabilities &
         NET_LINK_CAPABILITY_PROMISCUOUS_MODE) != 0) {

        Value |= SM91C1_RECEIVE_CONTROL_PROMISCUOUS;

    } else {
        Value &= ~SM91C1_RECEIVE_CONTROL_PROMISCUOUS;
    }

    Sm91c1pWriteRegister(Device, Sm91c1RegisterReceiveControl, Value);
    return;
}

KSTATUS
Sm91c1pReadMacAddress (
    PSM91C1_DEVICE Device
    )

/*++

Routine Description:

    This routine reads the MAC address out of the EEPROM on the SMSC91C1. The
    MAC address will be stored in the device structure.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    SM91C1_REGISTER AddressRegister;
    ULONG Index;
    USHORT Value;

    //
    // Trigger a reload of the EEPROM values into the configuration, base, and
    // individual address registers. Do not set the EEPROM select bit and set
    // the RELOAD bit.
    //

    Value = Sm91c1pReadRegister(Device, Sm91c1RegisterControl);
    Value |= SM91C1_CONTROL_EEPROM_RELOAD;
    Sm91c1pWriteRegister(Device, Sm91c1RegisterControl, Value);

    //
    // Wait until the reload bit is cleared.
    //

    while (TRUE) {
        KeDelayExecution(FALSE, FALSE, 50 * MICROSECONDS_PER_MILLISECOND);
        Value = Sm91c1pReadRegister(Device, Sm91c1RegisterControl);
        if ((Value & SM91C1_CONTROL_EEPROM_RELOAD) == 0) {
            break;
        }
    }

    //
    // Now the MAC address should be filled into the individual address
    // registers. There is one byte in each but two can be read at a time as
    // they are sequential registers.
    //

    AddressRegister = Sm91c1RegisterIndividualAddress0;
    for (Index = 0; Index < sizeof(Device->MacAddress); Index += 1) {
        Value = Sm91c1pReadRegister(Device, AddressRegister);
        Device->MacAddress[Index] = (BYTE)Value;
        AddressRegister += 1;
    }

    //
    // Check to determine if this is a valid MAC address.
    //

    if (NetIsEthernetAddressValid(Device->MacAddress) == FALSE) {
        return STATUS_INVALID_ADDRESS;
    }

    return STATUS_SUCCESS;
}

USHORT
Sm91c1pReadRegister (
    PSM91C1_DEVICE Device,
    SM91C1_REGISTER Register
    )

/*++

Routine Description:

    This routine reads from the specified register for the given SMSC91C1
    device.

Arguments:

    Device - Supplies a pointer to the SMSC91C1 device whose register is to be
        read.

    Register - Supplies the register to read.

Return Value:

    Returns the value of the register.

--*/

{

    BYTE Bank;
    BYTE ByteCount;
    BYTE Offset;
    RUNLEVEL OldRunLevel;
    USHORT Value;

    ASSERT((Sm91c1RegisterBankSelect & SM91C1_REGISTER_BANK_MASK) == 0);

    if (Device->InterruptHandle == INVALID_HANDLE) {
        OldRunLevel = KeRaiseRunLevel(RunLevelHigh);

    } else {
        OldRunLevel = IoRaiseToInterruptRunLevel(Device->InterruptHandle);
    }

    KeAcquireSpinLock(&(Device->BankLock));

    //
    // First select the correct bank. The bank register can always be accessed
    // from the currently selected bank.
    //

    Bank = (Register & SM91C1_REGISTER_BANK_MASK) >> SM91C1_REGISTER_BANK_SHIFT;
    if (Bank != Device->SelectedBank) {
        Offset = (Sm91c1RegisterBankSelect & SM91C1_REGISTER_OFFSET_MASK) >>
                 SM91C1_REGISTER_OFFSET_SHIFT;

        ByteCount = (Sm91c1RegisterBankSelect &
                     SM91C1_REGISTER_BYTE_COUNT_MASK) >>
                    SM91C1_REGISTER_BYTE_COUNT_SHIFT;

        ASSERT(ByteCount == sizeof(USHORT));

        HlWriteRegister16(Device->ControllerBase + Offset, Bank);
        Device->SelectedBank = Bank;
    }

    //
    // Now read the register. Act according to the byte count.
    //

    ByteCount = (Register & SM91C1_REGISTER_BYTE_COUNT_MASK) >>
                SM91C1_REGISTER_BYTE_COUNT_SHIFT;

    Offset = (Register & SM91C1_REGISTER_OFFSET_MASK) >>
             SM91C1_REGISTER_OFFSET_SHIFT;

    if (ByteCount == sizeof(BYTE)) {
        Value = HlReadRegister8(Device->ControllerBase + Offset);

    } else {

        ASSERT(ByteCount == sizeof(USHORT));

        Value = HlReadRegister16(Device->ControllerBase + Offset);
    }

    KeReleaseSpinLock(&(Device->BankLock));
    KeLowerRunLevel(OldRunLevel);
    return Value;
}

VOID
Sm91c1pWriteRegister (
    PSM91C1_DEVICE Device,
    SM91C1_REGISTER Register,
    USHORT Value
    )

/*++

Routine Description:

    This routine writes to the specified register for the given SMSC91C1
    device.

Arguments:

    Device - Supplies a pointer to the SMSC91C1 device whose register is to be
        written.

    Register - Supplies the register to write.

    Value - Supplies the value to write to the given register.

Return Value:

    Returns the value of the register.

--*/

{

    BYTE Bank;
    BYTE ByteCount;
    BYTE Offset;
    RUNLEVEL OldRunLevel;

    if (Device->InterruptHandle == INVALID_HANDLE) {
        OldRunLevel = KeRaiseRunLevel(RunLevelHigh);

    } else {
        OldRunLevel = IoRaiseToInterruptRunLevel(Device->InterruptHandle);
    }

    //
    // First select the correct bank, if necessary. The bank register can
    // always be accessed from the currently selected bank.
    //

    KeAcquireSpinLock(&(Device->BankLock));
    Bank = (Register & SM91C1_REGISTER_BANK_MASK) >> SM91C1_REGISTER_BANK_SHIFT;
    if (Bank != Device->SelectedBank) {
        Offset = (Sm91c1RegisterBankSelect & SM91C1_REGISTER_OFFSET_MASK) >>
                 SM91C1_REGISTER_OFFSET_SHIFT;

        ByteCount = (Sm91c1RegisterBankSelect &
                     SM91C1_REGISTER_BYTE_COUNT_MASK) >>
                    SM91C1_REGISTER_BYTE_COUNT_SHIFT;

        ASSERT(ByteCount == sizeof(USHORT));

        HlWriteRegister16(Device->ControllerBase + Offset, Bank);
        Device->SelectedBank = Bank;
    }

    //
    // Now write the register. Act according to the byte count.
    //

    ByteCount = (Register & SM91C1_REGISTER_BYTE_COUNT_MASK) >>
                SM91C1_REGISTER_BYTE_COUNT_SHIFT;

    Offset = (Register & SM91C1_REGISTER_OFFSET_MASK) >>
             SM91C1_REGISTER_OFFSET_SHIFT;

    if (ByteCount == sizeof(BYTE)) {
        HlWriteRegister8(Device->ControllerBase + Offset, (BYTE)Value);

    } else {

        ASSERT(ByteCount == sizeof(USHORT));

        HlWriteRegister16(Device->ControllerBase + Offset, Value);
    }

    KeReleaseSpinLock(&(Device->BankLock));
    KeLowerRunLevel(OldRunLevel);
    return;
}

USHORT
Sm91c1pReadMdio (
    PSM91C1_DEVICE Device,
    SM91C1_MII_REGISTER Register
    )

/*++

Routine Description:

    This routine performs an MDIO register read.

Arguments:

    Device - Supplies a pointer to the SMSC91C1 device context for the read.

    Register - Supplies the SMSC91C1 MII register to read.

Return Value:

    Returns the value of the MDO register.

--*/

{

    USHORT Data;
    ULONG Index;
    USHORT Value;

    //
    // Synchronize the MI to prepare for the start bits.
    //

    Sm91c1SynchronizeMdio(Device);

    //
    // Issue the start bits: a 0 followed by a 1.
    //

    SM91C1_WRITE_ZERO_TO_MI(Device);
    SM91C1_WRITE_ONE_TO_MI(Device);

    //
    // Issue a read command by writing a 1 followed by a 0.
    //

    SM91C1_WRITE_ONE_TO_MI(Device);
    SM91C1_WRITE_ZERO_TO_MI(Device);

    //
    // Write the PHY device address which is 00000.
    //

    for (Index = 0; Index < 5; Index += 1) {
        SM91C1_WRITE_ZERO_TO_MI(Device);
    }

    //
    // Write the MII register to read. Most significant bit first.
    //

    for (Index = 0; Index < 5; Index += 1) {
        if ((Register & 0x10) != 0) {
            SM91C1_WRITE_ONE_TO_MI(Device);

        } else {
            SM91C1_WRITE_ZERO_TO_MI(Device);
        }

        Register <<= 1;
    }

    //
    // Write Z for the turnaround time.
    //

    SM91C1_WRITE_Z_TO_MI(Device);

    //
    // Read the data bit by bit.
    //

    Data = 0;
    for (Index = 0; Index < 16; Index += 1) {
        Data <<= 1;
        Sm91c1pWriteRegister(Device, Sm91c1RegisterManagementInterface, 0x0);
        Sm91c1pWriteRegister(Device, Sm91c1RegisterManagementInterface, 0x4);
        Value = Sm91c1pReadRegister(Device, Sm91c1RegisterManagementInterface);
        Sm91c1pWriteRegister(Device, Sm91c1RegisterManagementInterface, 0x0);
        if ((Value & SM91C1_MANAGEMENT_INTERFACE_MII_MDI) != 0) {
            Data |= 0x1;
        }
    }

    //
    // Send the turnaround bit again.
    //

    SM91C1_WRITE_Z_TO_MI(Device);
    return Data;
}

VOID
Sm91c1pWriteMdio (
    PSM91C1_DEVICE Device,
    SM91C1_MII_REGISTER Register,
    USHORT Value
    )

/*++

Routine Description:

    This routine performs a write to an MDIO register.

Arguments:

    Device - Supplies a pointer to the SMSC91C1 device context for the write.

    Register - Supplies the SMSC91C1 MII register to write.

    Value - Supplies the value to be written to the MDIO register.

Return Value:

    None.

--*/

{

    ULONG Index;

    //
    // Synchronize the MI to prepare for the start bits.
    //

    Sm91c1SynchronizeMdio(Device);

    //
    // Issue the start bits: a 0 followed by a 1.
    //

    SM91C1_WRITE_ZERO_TO_MI(Device);
    SM91C1_WRITE_ONE_TO_MI(Device);

    //
    // Issue a write command by writing a 0 followed by a 1.
    //

    SM91C1_WRITE_ZERO_TO_MI(Device);
    SM91C1_WRITE_ONE_TO_MI(Device);

    //
    // Write the PHY device address which is 00000.
    //

    for (Index = 0; Index < 5; Index += 1) {
        SM91C1_WRITE_ZERO_TO_MI(Device);
    }

    //
    // Write the MII register to read. Most significant bit first.
    //

    for (Index = 0; Index < 5; Index += 1) {
        if ((Register & 0x10) != 0) {
            SM91C1_WRITE_ONE_TO_MI(Device);

        } else {
            SM91C1_WRITE_ZERO_TO_MI(Device);
        }

        Register <<= 1;
    }

    //
    // Send the turnaround sequence: a 1 and then a 0.
    //

    SM91C1_WRITE_ONE_TO_MI(Device);
    SM91C1_WRITE_ZERO_TO_MI(Device);

    //
    // Write the data bit by bit, starting with the most significant bit.
    //

    for (Index = 0; Index < 16; Index += 1) {
        if ((Value & 0x8000) != 0) {
            SM91C1_WRITE_ONE_TO_MI(Device);

        } else {
            SM91C1_WRITE_ZERO_TO_MI(Device);
        }

        Value <<= 1;
    }

    //
    // Send the turnaround Z.
    //

    SM91C1_WRITE_Z_TO_MI(Device);
    return;
}

VOID
Sm91c1SynchronizeMdio (
    PSM91C1_DEVICE Device
    )

/*++

Routine Description:

    This routine synchronizes the MDIO to prepare it for a register read or
    write.

Arguments:

    Device - Supplies a pointer to the SMSC91C1 device context.

Return Value:

    None.

--*/

{

    ULONG Index;

    //
    // Synchronize the MII by writing at least 32 ones.
    //

    for (Index = 0; Index < SM91C1_MII_SYNCHRONIZE_COUNT; Index += 1) {
        SM91C1_WRITE_ONE_TO_MI(Device);
    }

    return;
}

