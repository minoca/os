/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    atl1chw.c

Abstract:

    This module implements the portion of the ATL1c driver that actually
    interacts with the hardware.

Author:

    Evan Green 18-Apr-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/net/netdrv.h>
#include "atl1c.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum amount of packets that ATL will keep queued before it
// starts to drop packets.
//
//

#define ATL_MAX_TRANSMIT_PACKET_LIST_COUNT (ATL1C_TRANSMIT_DESCRIPTOR_COUNT * 2)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
AtlpReapCompletedTransmitDescriptors (
    PATL1C_DEVICE Device
    );

VOID
AtlpSendPendingPackets (
    PATL1C_DEVICE Device
    );

VOID
AtlpReapReceivedFrames (
    PATL1C_DEVICE Device
    );

VOID
AtlpDisableL0sAndL1 (
    PATL1C_DEVICE Device
    );

BOOL
AtlpResetPhy (
    PATL1C_DEVICE Device
    );

BOOL
AtlpApplyChipReset (
    PATL1C_DEVICE Device
    );

BOOL
AtlpStopController (
    PATL1C_DEVICE Device
    );

BOOL
AtlpStopMac (
    PATL1C_DEVICE Device
    );

BOOL
AtlpStopQueue (
    PATL1C_DEVICE Device
    );

VOID
AtlpSetupReceiveFilters (
    PATL1C_DEVICE Device
    );

KSTATUS
AtlpEnableDevice (
    PATL1C_DEVICE Device
    );

VOID
AtlpStartQueue (
    PATL1C_DEVICE Device
    );

VOID
AtlpConfigureMac (
    PATL1C_DEVICE Device
    );

BOOL
AtlpReadPhyDebugRegister (
    PATL1C_DEVICE Device,
    USHORT Register,
    PUSHORT Data
    );

BOOL
AtlpWritePhyDebugRegister (
    PATL1C_DEVICE Device,
    USHORT Register,
    USHORT Data
    );

BOOL
AtlpPerformPhyRegisterIo (
    PATL1C_DEVICE Device,
    BOOL Write,
    BOOL Extension,
    UCHAR Address,
    USHORT Register,
    PUSHORT Data
    );

ULONG
AtlpWaitForIdleUnit (
    PATL1C_DEVICE Device,
    ULONG BitsToBecomeClear
    );

BOOL
AtlpReadMacAddress (
    PATL1C_DEVICE Device
    );

BOOL
AtlpDoesEepromExist (
    PATL1C_DEVICE Device
    );

BOOL
AtlpReadCurrentMacAddress (
    PATL1C_DEVICE Device
    );

VOID
AtlpDisableDeviceInterrupts (
    PATL1C_DEVICE Device
    );

VOID
AtlpEnableDeviceInterrupts (
    PATL1C_DEVICE Device
    );

VOID
AtlpSetActiveStatePowerManagement (
    PATL1C_DEVICE Device,
    ATL_SPEED Speed
    );

VOID
AtlpResetTransmitRing (
    PATL1C_DEVICE Device
    );

VOID
AtlpResetReceiveRing (
    PATL1C_DEVICE Device
    );

BOOL
AtlpGetLinkCharacteristics (
    PATL1C_DEVICE Device,
    PATL_SPEED Speed,
    PATL_DUPLEX_MODE Duplex
    );

ULONG
AtlpHashAddress (
    PUCHAR MacAddress
    );

RUNLEVEL
AtlpAcquireInterruptLock (
    PATL1C_DEVICE Device
    );

VOID
AtlpReleaseInterruptLock (
    PATL1C_DEVICE Device,
    RUNLEVEL OldRunLevel
    );

//
// -------------------------------------------------------------------- Globals
//

BOOL AtlDisablePacketDropping = FALSE;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
AtlSend (
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

    PATL1C_DEVICE Device;
    UINTN PacketListCount;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Device = (PATL1C_DEVICE)DeviceContext;
    KeAcquireQueuedLock(Device->TransmitLock);
    if (Device->LinkActive == FALSE) {
        Status = STATUS_NO_NETWORK_CONNECTION;
        goto SendEnd;
    }

    //
    // If there is any room in the packet list (or dropping packets is
    // disabled), add all of the packets to the list waiting to be sent.
    //

    PacketListCount = Device->TransmitPacketList.Count;
    if ((PacketListCount < ATL_MAX_TRANSMIT_PACKET_LIST_COUNT) ||
        (AtlDisablePacketDropping != FALSE)) {

        NET_APPEND_PACKET_LIST(PacketList, &(Device->TransmitPacketList));
        AtlpSendPendingPackets(Device);
        Status = STATUS_SUCCESS;

    //
    // Otherwise report that the resource is use as it is too busy to handle
    // more packets.
    //

    } else {
        Status = STATUS_RESOURCE_IN_USE;
    }

SendEnd:
    KeReleaseQueuedLock(Device->TransmitLock);
    return Status;
}

KSTATUS
AtlGetSetInformation (
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
    PATL1C_DEVICE Device;
    PULONG Flags;
    KSTATUS Status;

    Status = STATUS_SUCCESS;
    Device = (PATL1C_DEVICE)DeviceContext;
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

        KeAcquireQueuedLock(Device->ConfigurationLock);
        Capabilities = Device->EnabledCapabilities;
        if (*BooleanOption != FALSE) {
            Capabilities |= NET_LINK_CAPABILITY_PROMISCUOUS_MODE;

        } else {
            Capabilities &= ~NET_LINK_CAPABILITY_PROMISCUOUS_MODE;
        }

        if ((Capabilities ^ Device->EnabledCapabilities) != 0) {
            Device->EnabledCapabilities = Capabilities;
            AtlpSetupReceiveFilters(Device);
        }

        KeReleaseQueuedLock(Device->ConfigurationLock);
        break;

    default:
        Status = STATUS_NOT_SUPPORTED;
        break;
    }

    return Status;
}

KSTATUS
AtlpInitializeDeviceStructures (
    PATL1C_DEVICE Device
    )

/*++

Routine Description:

    This routine performs housekeeping preparation for resetting and enabling
    an ATL1c device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    ULONG IoBufferFlags;
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONG Size;
    ULONG SlotIndex;
    KSTATUS Status;

    KeInitializeSpinLock(&(Device->InterruptLock));
    Device->Speed = AtlSpeedOff;
    Device->Duplex = AtlDuplexInvalid;
    Device->EnabledInterrupts = ATL_INTERRUPT_DEFAULT_MASK;

    //
    // Allocate the transmit and receive locks.
    //

    Device->TransmitLock = KeCreateQueuedLock();
    if (Device->TransmitLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    Device->ReceiveLock = KeCreateQueuedLock();
    if (Device->ReceiveLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    Device->ConfigurationLock = KeCreateQueuedLock();
    if (Device->ConfigurationLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    //
    // Allocate the descriptor buffer to hold the transmit descriptors,
    // transmit buffer array, receive slot array, received packet status array,
    // and the received frame data itself. The transmit queue has one extra
    // descriptor for the empty high priority queue.
    //

    AllocationSize = ((ATL1C_TRANSMIT_DESCRIPTOR_COUNT + 1) *
                      sizeof(ATL1C_TRANSMIT_DESCRIPTOR)) +
                     (ATL1C_TRANSMIT_DESCRIPTOR_COUNT * sizeof(PVOID)) +
                     (ATL1C_RECEIVE_FRAME_COUNT *
                      (sizeof(ATL1C_RECEIVE_SLOT) +
                       sizeof(ATL1C_RECEIVED_PACKET) +
                       ATL1C_RECEIVE_FRAME_DATA_SIZE));

    ASSERT(Device->DescriptorIoBuffer == NULL);

    IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS;
    Device->DescriptorIoBuffer = MmAllocateNonPagedIoBuffer(0,
                                                            MAX_ULONG,
                                                            8,
                                                            AllocationSize,
                                                            IoBufferFlags);

    if (Device->DescriptorIoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    ASSERT(Device->DescriptorIoBuffer->FragmentCount == 1);

    //
    // Zero out everything except the receive packet data buffers.
    //

    Size = AllocationSize -
           (ATL1C_RECEIVE_FRAME_DATA_SIZE * ATL1C_RECEIVE_FRAME_COUNT);

    RtlZeroMemory(Device->DescriptorIoBuffer->Fragment[0].VirtualAddress, Size);

    //
    // Carve up the buffer and give each array its piece.
    //

    Device->TransmitDescriptor =
                        Device->DescriptorIoBuffer->Fragment[0].VirtualAddress;

    Device->TransmitBuffer =
                (PNET_PACKET_BUFFER *)(Device->TransmitDescriptor +
                                       (ATL1C_TRANSMIT_DESCRIPTOR_COUNT + 1));

    Device->ReceiveSlot =
                    (PATL1C_RECEIVE_SLOT)(Device->TransmitBuffer +
                                          ATL1C_TRANSMIT_DESCRIPTOR_COUNT);

    Device->ReceivedPacket =
                           (PATL1C_RECEIVED_PACKET)(Device->ReceiveSlot +
                                                    ATL1C_RECEIVE_FRAME_COUNT);

    Device->ReceivedPacketData =
                   (PVOID)(Device->ReceivedPacket + ATL1C_RECEIVE_FRAME_COUNT);

    //
    // Initialize the receive slots.
    //

    PhysicalAddress = Device->DescriptorIoBuffer->Fragment[0].PhysicalAddress +
                      AllocationSize -
                      (ATL1C_RECEIVE_FRAME_COUNT *
                       ATL1C_RECEIVE_FRAME_DATA_SIZE);

    for (SlotIndex = 0; SlotIndex < ATL1C_RECEIVE_FRAME_COUNT; SlotIndex += 1) {
        Device->ReceiveSlot[SlotIndex].PhysicalAddress = PhysicalAddress;
        PhysicalAddress += ATL1C_RECEIVE_FRAME_DATA_SIZE;
    }

    NET_INITIALIZE_PACKET_LIST(&(Device->TransmitPacketList));
    Device->ReceiveNextToClean = 0;
    Device->TransmitNextToClean = 0;
    Device->TransmitNextToUse = 0;

    //
    // Promiscuous mode is always supported and starts disabled.
    //

    Device->SupportedCapabilities = NET_LINK_CAPABILITY_PROMISCUOUS_MODE;
    Status = STATUS_SUCCESS;

InitializeDeviceStructuresEnd:
    if (!KSUCCESS(Status)) {
        if (Device->DescriptorIoBuffer != NULL) {
            MmFreeIoBuffer(Device->DescriptorIoBuffer);
            Device->TransmitDescriptor = NULL;
            Device->TransmitBuffer = NULL;
            Device->ReceiveSlot = NULL;
            Device->ReceivedPacket = NULL;
        }

        if (Device->TransmitLock != NULL) {
            KeDestroyQueuedLock(Device->TransmitLock);
            Device->TransmitLock = NULL;
        }

        if (Device->ReceiveLock != NULL) {
            KeDestroyQueuedLock(Device->ReceiveLock);
            Device->ReceiveLock = NULL;
        }

        if (Device->ConfigurationLock != NULL) {
            KeDestroyQueuedLock(Device->ConfigurationLock);
            Device->ConfigurationLock = NULL;
        }
    }

    return Status;
}

KSTATUS
AtlpResetDevice (
    PATL1C_DEVICE Device
    )

/*++

Routine Description:

    This routine resets the ATL1c device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    PHYSICAL_ADDRESS PhysicalAddress;
    BOOL Result;
    KSTATUS Status;
    ULONG Value;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Status = STATUS_DEVICE_IO_ERROR;
    Device->Speed = AtlSpeedOff;
    Device->Duplex = AtlDuplexInvalid;

    //
    // Clear any lingering PCI express protocol errors.
    //

    Value = ATL_READ_REGISTER32(Device, AtlRegisterPexUncErrSev);
    Value &= ~(ATL_PEX_UNC_ERR_SEV_DLP | ATL_PEX_UNC_ERR_SEV_FCP);
    ATL_WRITE_REGISTER32(Device, AtlRegisterPexUncErrSev, Value);

    //
    // Reset the Link Training and Status State Machine.
    //

    Value = ATL_READ_REGISTER32(Device, AtlRegisterLtssmIdControl);
    Value &= ~ATL_LTSSM_ID_ENABLE_WRO;
    ATL_WRITE_REGISTER32(Device, AtlRegisterLtssmIdControl, Value);
    Value = ATL_READ_REGISTER32(Device, AtlRegisterPhyMiscellaneous);
    Value |= ATL_PHY_MISCELLANEOUS_FORCE_RECEIVE_DETECT;
    ATL_WRITE_REGISTER32(Device, AtlRegisterPhyMiscellaneous, Value);
    AtlpDisableL0sAndL1(Device);
    Result = AtlpResetPhy(Device);
    if (Result == FALSE) {
        goto ResetDeviceEnd;
    }

    //
    // Stop anything currently going on.
    //

    Result = AtlpStopController(Device);
    if (Result == FALSE) {
        goto ResetDeviceEnd;
    }

    //
    // Apply a reset to the master control register to get the chip in a
    // known state.
    //

    Result = AtlpApplyChipReset(Device);
    if (Result == FALSE) {
        goto ResetDeviceEnd;
    }

    Result = AtlpReadMacAddress(Device);
    if (Result == FALSE) {
        goto ResetDeviceEnd;
    }

    //
    // Notify the networking core of this new link now that the device is ready
    // to send and receive data, pending media being present.
    //

    if (Device->NetworkLink == NULL) {
        Status = AtlpAddNetworkDevice(Device);
        if (!KSUCCESS(Status)) {
            goto ResetDeviceEnd;
        }
    }

    KeAcquireQueuedLock(Device->ReceiveLock);
    AtlpResetReceiveRing(Device);
    KeReleaseQueuedLock(Device->ReceiveLock);
    KeAcquireQueuedLock(Device->TransmitLock);
    AtlpResetTransmitRing(Device);
    KeReleaseQueuedLock(Device->TransmitLock);

    //
    // Enable all clocks and disable WOL (which would interfere with normal
    // operation).
    //

    ATL_WRITE_REGISTER32(Device, AtlRegisterClockGatingControl, 0);
    ATL_READ_REGISTER32(Device, AtlRegisterWakeOnLanControl);
    ATL_WRITE_REGISTER32(Device, AtlRegisterWakeOnLanControl, 0);

    //
    // Configure the descriptor rings, starting with the transmit queue. The
    // normal priority queue (priority 0) gets all the descriptors. The high
    // priority queue, which is unused, gets a single descriptor to pacify it.
    //

    PhysicalAddress = Device->DescriptorIoBuffer->Fragment[0].PhysicalAddress;
    ATL_WRITE_REGISTER32(
                      Device,
                      AtlRegisterTransmitBaseAddressHigh,
                      (ULONG)(PhysicalAddress >> ATL_RING_HIGH_ADDRESS_SHIFT));

    ATL_WRITE_REGISTER32(Device,
                         AtlRegisterTransmitBaseAddressLow,
                         (ULONG)PhysicalAddress);

    PhysicalAddress += sizeof(ATL1C_TRANSMIT_DESCRIPTOR) *
                       ATL1C_TRANSMIT_DESCRIPTOR_COUNT;

    ATL_WRITE_REGISTER32(Device,
                         AtlRegisterTransmitBaseAddressLowHighPriority,
                         0);

    ATL_WRITE_REGISTER32(Device,
                         AtlRegisterTransmitRingSize,
                         ATL1C_TRANSMIT_DESCRIPTOR_COUNT);

    //
    // Set up the Receive Free Descriptor ring. Only the first queue is used.
    //

    PhysicalAddress += sizeof(ATL1C_TRANSMIT_DESCRIPTOR) +
                       (sizeof(PVOID) * ATL1C_TRANSMIT_DESCRIPTOR_COUNT);

    ATL_WRITE_REGISTER32(
                      Device,
                      AtlRegisterReceiveBaseAddressHigh,
                      (ULONG)(PhysicalAddress >> ATL_RING_HIGH_ADDRESS_SHIFT));

    ATL_WRITE_REGISTER32(Device,
                         AtlRegisterReceiveBaseAddressLow,
                         (ULONG)PhysicalAddress);

    ATL_WRITE_REGISTER32(Device, AtlRegisterReceive1BaseAddressLow, 0);
    ATL_WRITE_REGISTER32(Device, AtlRegisterReceive2BaseAddressLow, 0);
    ATL_WRITE_REGISTER32(Device, AtlRegisterReceive3BaseAddressLow, 0);
    ATL_WRITE_REGISTER32(Device,
                         AtlRegisterReceiveSlotRingSize,
                         ATL1C_RECEIVE_FRAME_COUNT);

    ATL_WRITE_REGISTER32(Device,
                         AtlRegisterReceiveBufferSize,
                         ATL1C_RECEIVE_FRAME_DATA_SIZE);

    //
    // Set up the Received Packet Status ring.
    //

    PhysicalAddress += sizeof(ATL1C_RECEIVE_SLOT) * ATL1C_RECEIVE_FRAME_COUNT;
    ATL_WRITE_REGISTER32(Device,
                         AtlRegisterReceiveRingBaseAddressLow,
                         (ULONG)PhysicalAddress);

    ATL_WRITE_REGISTER32(Device, AtlRegisterReceiveRing1BaseAddressLow, 0);
    ATL_WRITE_REGISTER32(Device, AtlRegisterReceiveRing2BaseAddressLow, 0);
    ATL_WRITE_REGISTER32(Device, AtlRegisterReceiveRing3BaseAddressLow, 0);
    ATL_WRITE_REGISTER32(Device,
                         AtlRegisterReceiveStatusRingSize,
                         ATL1C_RECEIVE_FRAME_COUNT);

    //
    // The CMB and SMB pointers aren't used.
    //

    ATL_WRITE_REGISTER32(Device, AtlRegisterCmbBaseAddressLow, 0);
    ATL_WRITE_REGISTER32(Device, AtlRegisterSmbBaseAddressHigh, 0);
    ATL_WRITE_REGISTER32(Device, AtlRegisterSmbBaseAddressLow, 0);

    //
    // Officially load all those ring pointers into the device.
    //

    ATL_WRITE_REGISTER32(Device,
                         AtlRegisterLoadRingPointers,
                         ATL_LOAD_POINTERS_COMMAND_GO);

    //
    // Set up the interrupt moderator timer.
    //

    Value = ((ATL_MICROSECONDS(ATL_TRANSMIT_INTERRUPT_TIMER_VALUE) &
              ATL_INTERRUPT_TIMER_TRANSMIT_MASK) <<
             ATL_INTERRUPT_TIMER_TRANSMIT_SHIFT) |
            ((ATL_MICROSECONDS(ATL_RECEIVE_INTERRUPT_TIMER_VALUE) &
              ATL_INTERRUPT_TIMER_RECEIVE_MASK) <<
             ATL_INTERRUPT_TIMER_RECEIVE_SHIFT);

    ATL_WRITE_REGISTER32(Device, AtlRegisterInterruptTimers, Value);

    //
    // Set the timers to be enabled, and disable interrupt status clear on
    // read.
    //

    Value = ATL_MASTER_CONTROL_SYSTEM_ALIVE_TIMER |
            ATL_MASTER_CONTROL_TRANSMIT_ITIMER_ENABLE |
            ATL_MASTER_CONTROL_RECEIVE_ITIMER_ENABLE;

    ATL_WRITE_REGISTER32(Device, AtlRegisterMasterControl, Value);

    //
    // Disable the interrupt retrigger timer to prevent unserviced interrupts
    // from coming back.
    //

    ATL_WRITE_REGISTER32(Device, AtlRegisterInterruptRetriggerTimer, 0);

    //
    // Disable the CMB and SMB timers.
    //

    ATL_WRITE_REGISTER32(Device, AtlRegisterCmbTransmitTimer, 0);
    ATL_WRITE_REGISTER32(Device, AtlRegisterSmbStatTimer, 0);

    //
    // Set the Maximum Transmission Unit.
    //

    ATL_WRITE_REGISTER32(Device,
                         AtlRegisterMaximumTransmissionUnit,
                         ATL_L2CB_MAX_TRANSMIT_LENGTH);

    ATL_WRITE_REGISTER32(Device, AtlRegisterHdsControl, 0);
    ATL_WRITE_REGISTER32(Device, AtlRegisterIpgIfgControl, ATL_IPG_IFG_VALUE);
    ATL_WRITE_REGISTER32(Device,
                         AtlRegisterHalfDuplexControl,
                         ATL_HALF_DUPLEX_CONTROL_VALUE);

    //
    // Set up the transmit parameters.
    //

    Value = (ATL_TRANSMIT_TCP_SEGMENTATION_OFFSET_FRAME_SIZE >>
             ATL_TCP_SEGMENTATION_OFFLOAD_THRESHOLD_DOWNSHIFT) &
            ATL_TCP_SEGMENTATION_OFFLOAD_THRESHOLD_MASK;

    ATL_WRITE_REGISTER32(Device,
                         AtlRegisterTcpSegmentationOffloadThreshold,
                         Value);

    Value = ((ATL_TRANSMIT_DESCRIPTOR_BURST_COUNT &
              ATL_TRANSMIT_QUEUE_CONTROL_BURST_MASK) <<
             ATL_TRANSMIT_QUEUE_CONTROL_BURST_SHIFT) |
            ATL_TRANSMIT_QUEUE_CONTROL_ENHANCED_MODE |
            ((ATL_L2CB_TRANSMIT_TXF_BURST_PREF &
              ATL_TRANSMIT_QUEUE_CONTROL_BURST_NUMBER_MASK) <<
             ATL_TRANSMIT_QUEUE_CONTROL_BURST_NUMBER_SHIFT);

    ATL_WRITE_REGISTER32(Device, AtlRegisterTransmitQueueControl, Value);

    //
    // Configure receive free slot pre-fetching.
    //

    ATL_WRITE_REGISTER32(Device,
                         AtlRegisterReceiveFreeThreshold,
                         ATL_RECEIVE_FREE_THRESHOLD_VALUE);

    //
    // Disable RSS.
    //

    ATL_WRITE_REGISTER32(Device, AtlRegisterRssIdtTable0, 0);
    ATL_WRITE_REGISTER32(Device, AtlRegisterRssCpu, 0);

    //
    // Configure the receive queue.
    //

    Value = ((ATL_RECEIVE_DESCRIPTOR_BURST_COUNT &
              ATL_RECEIVE_QUEUE_CONTROL_BURST_MASK) <<
             ATL_RECEIVE_QUEUE_CONTROL_BURST_SHIFT);

    ATL_WRITE_REGISTER32(Device, AtlRegisterReceiveQueueControl, Value);

    //
    // Configure DMA.
    //

    ATL_WRITE_REGISTER32(Device, AtlRegisterDmaControl, ATL_DMA_CONTROL_VALUE);

    //
    // Configure the MAC. The speed/duplex settings get reconfigured a bit once
    // the link is determined to be established.
    //

    KeAcquireQueuedLock(Device->ConfigurationLock);
    Value = ATL_MAC_CONTROL_ADD_CRC | ATL_MAC_CONTROL_PAD |
            ATL_MAC_CONTROL_DUPLEX |
           ((ATL_PREAMBLE_LENGTH &
             ATL_MAC_CONTROL_PREAMBLE_LENGTH_MASK) <<
            ATL_MAC_CONTROL_PREAMBLE_LENGTH_SHIFT) |
           (ATL_MAC_CONTROL_SPEED_10_100 << ATL_MAC_CONTROL_SPEED_SHIFT);

    ATL_WRITE_REGISTER32(Device, AtlRegisterMacControl, Value);
    AtlpSetupReceiveFilters(Device);
    KeReleaseQueuedLock(Device->ConfigurationLock);

    //
    // Disable hardware stripping of the VLAN tag. If VLAN support is added,
    // this bit would need to be set here.
    //

    Value = ATL_READ_REGISTER32(Device, AtlRegisterMacControl);
    Value &= ~ATL_MAC_CONTROL_STRIP_VLAN;
    ATL_WRITE_REGISTER32(Device, AtlRegisterMacControl, Value);

    //
    // Write the current producer index of the transmit ring.
    //

    ATL_WRITE_REGISTER16(Device,
                         AtlRegisterTransmitNextIndex,
                         Device->TransmitNextToClean);

    //
    // Clear any pending interrupts.
    //

    ATL_WRITE_REGISTER32(Device,
                         AtlRegisterInterruptStatus,
                         ATL_INTERRUPT_MASK);

    //
    // Everything's set up, re-enable interrupts and fire up the device.
    //

    Status = AtlpEnableDevice(Device);
    if (!KSUCCESS(Status)) {
        goto ResetDeviceEnd;
    }

    AtlpEnableDeviceInterrupts(Device);

ResetDeviceEnd:
    return Status;
}

INTERRUPT_STATUS
AtlpInterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the ATL1c interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the ATL1c device
        structure.

Return Value:

    Interrupt status.

--*/

{

    PATL1C_DEVICE Device;
    INTERRUPT_STATUS InterruptStatus;
    ULONG PendingBits;
    USHORT Value;

    Device = (PATL1C_DEVICE)Context;
    InterruptStatus = InterruptStatusNotClaimed;

    //
    // Read the status register, and if nothing is set then return immediately.
    //

    PendingBits = ATL_READ_REGISTER32(Device, AtlRegisterInterruptStatus);
    if (((PendingBits & Device->EnabledInterrupts) == 0) ||
        ((PendingBits & ATL_INTERRUPT_DISABLE) != 0)) {

        return InterruptStatus;
    }

    InterruptStatus = InterruptStatusClaimed;

    //
    // There are interrupt bits set, so mark this interrupt as claimed.
    //

    KeAcquireSpinLock(&(Device->InterruptLock));
    RtlAtomicOr32(&(Device->PendingInterrupts), PendingBits);

    //
    // The GPHY bit cannot be masked or cleared by the controller directly.
    // Read the PHY interrupt status register to clear the interrupt.
    //

    if ((PendingBits & ATL_INTERRUPT_GPHY) != 0) {
        AtlpPerformPhyRegisterIo(Device,
                                 FALSE,
                                 FALSE,
                                 0,
                                 ATL_PHY_MII_INTERRUPT_STATUS,
                                 &Value);
    }

    ATL_WRITE_REGISTER32(Device,
                         AtlRegisterInterruptStatus,
                         PendingBits | ATL_INTERRUPT_DISABLE);

    KeReleaseSpinLock(&(Device->InterruptLock));
    return InterruptStatus;
}

INTERRUPT_STATUS
AtlpInterruptServiceWorker (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine processes interrupts for the ATL1c controller at low level.

Arguments:

    Parameter - Supplies an optional parameter passed in by the creator of the
        work item.

Return Value:

    Interrupt Status.

--*/

{

    PATL1C_DEVICE Device;
    ULONG PendingBits;
    INTERRUPT_STATUS Status;

    Device = (PATL1C_DEVICE)(Parameter);

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Clear out the pending bits.
    //

    PendingBits = RtlAtomicExchange32(&(Device->PendingInterrupts), 0);
    if (PendingBits == 0) {
        Status = InterruptStatusNotClaimed;
        goto InterruptServiceWorkerEnd;
    }

    Status = InterruptStatusClaimed;
    if ((PendingBits & ATL_INTERRUPT_BUFFER_ERROR_MASK) != 0) {
        RtlDebugPrint("ATL: Buffer Error 0x%08x.\n", PendingBits);
    }

    //
    // If the interrupt indicates new packets are coming in, grab them.
    //

    if ((PendingBits & ATL_INTERRUPT_RECEIVE_PACKET_MASK) != 0) {
        AtlpReapReceivedFrames(Device);
    }

    //
    // If packets were sent out, reap the completed transmissions.
    //

    if ((PendingBits & ATL_INTERRUPT_TRANSMIT_PACKET) != 0) {
        AtlpReapCompletedTransmitDescriptors(Device);
    }

    //
    // If an error occurred, reset the MAC.
    //

    if ((PendingBits & ATL_INTERRUPT_ERROR_MASK) != 0) {
        NetSetLinkState(Device->NetworkLink, FALSE, 0);
        AtlpStopController(Device);
        AtlpEnableDevice(Device);
    }

    //
    // Handle a link event change.
    //

    if ((PendingBits & (ATL_INTERRUPT_MANUAL | ATL_INTERRUPT_GPHY)) != 0) {
        AtlpDisableDeviceInterrupts(Device);
        AtlpEnableDevice(Device);
        AtlpEnableDeviceInterrupts(Device);
    }

InterruptServiceWorkerEnd:
    ATL_WRITE_REGISTER32(Device, AtlRegisterInterruptStatus, 0);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
AtlpReapCompletedTransmitDescriptors (
    PATL1C_DEVICE Device
    )

/*++

Routine Description:

    This routine cleans out any transmit descriptors that have already been
    handled by the controller. This routine must be called at low level and
    assumes the transmit lock is already held.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    USHORT CurrentIndex;
    PATL1C_TRANSMIT_DESCRIPTOR Descriptor;
    BOOL DescriptorReaped;
    USHORT HardwareIndex;

    KeAcquireQueuedLock(Device->TransmitLock);
    HardwareIndex = ATL_READ_REGISTER16(Device,
                                        AtlRegisterTransmitCurrentIndex);

    DescriptorReaped = FALSE;
    if (Device->TransmitNextToClean != HardwareIndex) {
        DescriptorReaped = TRUE;
    }

    while (Device->TransmitNextToClean != HardwareIndex) {
        CurrentIndex = Device->TransmitNextToClean;
        Descriptor = &(Device->TransmitDescriptor[CurrentIndex]);

        ASSERT(Device->TransmitBuffer[CurrentIndex] != NULL);

        NetFreeBuffer(Device->TransmitBuffer[CurrentIndex]);
        Device->TransmitBuffer[CurrentIndex] = NULL;
        Descriptor->PhysicalAddress = 0;
        Descriptor->BufferLength = 0;
        Device->TransmitNextToClean += 1;
        if (Device->TransmitNextToClean == ATL1C_TRANSMIT_DESCRIPTOR_COUNT) {
            Device->TransmitNextToClean = 0;
        }
    }

    //
    // If space was freed up, signal the event unblocking any parties waiting
    // for transmit descriptors.
    //

    if (DescriptorReaped != FALSE) {
        AtlpSendPendingPackets(Device);
    }

    KeReleaseQueuedLock(Device->TransmitLock);
    return;
}

VOID
AtlpSendPendingPackets (
    PATL1C_DEVICE Device
    )

/*++

Routine Description:

    This routine sends as many packets as can fit in the hardware descriptor
    buffer. This routine assumes the transmit lock is already held.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    PATL1C_TRANSMIT_DESCRIPTOR Descriptor;
    ULONG DescriptorIndex;
    PNET_PACKET_BUFFER Packet;
    BOOL PacketQueued;

    ASSERT(KeIsQueuedLockHeld(Device->TransmitLock) != FALSE);

    //
    // Fill up the open descriptors with as many pending packets as possible.
    //

    PacketQueued = FALSE;
    while (NET_PACKET_LIST_EMPTY(&(Device->TransmitPacketList)) == FALSE) {
        Packet = LIST_VALUE(Device->TransmitPacketList.Head.Next,
                            NET_PACKET_BUFFER,
                            ListEntry);

        DescriptorIndex = Device->TransmitNextToUse;
        Descriptor = &(Device->TransmitDescriptor[DescriptorIndex]);

        //
        // If the length isn't zero, this is an active or unreaped entry.
        // Quit to try another day. The active packets should interrupt on
        // completion and drive more packets to be send.
        //

        if (Descriptor->BufferLength != 0) {
            break;
        }

        NET_REMOVE_PACKET_FROM_LIST(Packet, &(Device->TransmitPacketList));

        //
        // Success, a free transmit descriptor. Let's fill it out!
        //

        ASSERT(Device->TransmitBuffer[DescriptorIndex] == NULL);

        Device->TransmitBuffer[DescriptorIndex] = Packet;
        Descriptor->BufferLength = Packet->FooterOffset - Packet->DataOffset;
        Descriptor->PhysicalAddress = Packet->BufferPhysicalAddress +
                                       Packet->DataOffset;

        Descriptor->Flags = ATL_TRANSMIT_DESCRIPTOR_END_OF_PACKET;

        //
        // Advance the list past this entry.
        //

        Device->TransmitNextToUse += 1;
        if (Device->TransmitNextToUse == ATL1C_TRANSMIT_DESCRIPTOR_COUNT) {
            Device->TransmitNextToUse = 0;
        }

        PacketQueued = TRUE;
    }

    //
    // If some packets were queued, then send them now.
    //

    if (PacketQueued != FALSE) {
        RtlMemoryBarrier();
        ATL_WRITE_REGISTER16(Device,
                             AtlRegisterTransmitNextIndex,
                             Device->TransmitNextToUse);
    }

    return;
}

VOID
AtlpReapReceivedFrames (
    PATL1C_DEVICE Device
    )

/*++

Routine Description:

    This routine processes any received frames from the network.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    USHORT CurrentIndex;
    ULONG ErrorFlags;
    ULONG FramesProcessed;
    USHORT FreeIndex;
    USHORT OriginalNextToClean;
    NET_PACKET_BUFFER Packet;
    PATL1C_RECEIVED_PACKET ReceivedPacket;
    ULONG Value;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Loop grabbing completed frames.
    //

    FramesProcessed = 0;
    Packet.Flags = 0;
    KeAcquireQueuedLock(Device->ReceiveLock);
    OriginalNextToClean = Device->ReceiveNextToClean;
    while (TRUE) {
        CurrentIndex = Device->ReceiveNextToClean;
        ReceivedPacket = &(Device->ReceivedPacket[CurrentIndex]);

        //
        // If the packet is not valid, then stop.
        //

        if ((ReceivedPacket->FlagsAndLength &
             ATL_RECEIVED_PACKET_FLAG_VALID) == 0) {

            break;
        }

        //
        // This is a valid packet that needs to be reaped. Currently only
        // single packets are supported.
        //

        ASSERT(((ReceivedPacket->FreeIndex >>
                 ATL_RECEIVED_PACKET_COUNT_SHIFT) &
                ATL_RECEIVED_PACKET_COUNT_MASK) == 1);

        //
        // Process the packet, unless the error flags are set.
        //

        ErrorFlags = ATL_RECEIVED_PACKET_FLAG_802_3_LENGTH_ERROR |
                     ATL_RECEIVED_PACKET_FLAG_CHECKSUM_ERROR;

        if ((ReceivedPacket->FlagsAndLength & ErrorFlags) == 0) {
            FreeIndex = (ReceivedPacket->FreeIndex >>
                         ATL_RECEIVED_PACKET_FREE_INDEX_SHIFT) &
                        ATL_RECEIVED_PACKET_FREE_INDEX_MASK;

            ASSERT(FreeIndex < ATL1C_RECEIVE_FRAME_COUNT);

            Packet.Buffer = Device->ReceivedPacketData +
                            (FreeIndex * ATL1C_RECEIVE_FRAME_DATA_SIZE);

            Packet.BufferPhysicalAddress =
                                Device->ReceiveSlot[FreeIndex].PhysicalAddress;

            Packet.BufferSize = ReceivedPacket->FlagsAndLength &
                                ATL_RECEIVED_PACKET_SIZE_MASK;

            Packet.DataSize = Packet.BufferSize;
            Packet.DataOffset = 0;
            Packet.FooterOffset = Packet.DataSize;
            NetProcessReceivedPacket(Device->NetworkLink, &Packet);
        }

        //
        // Clear the flag set by the hardware and move the index forward.
        //

        FramesProcessed += 1;
        ReceivedPacket->FlagsAndLength &= ~ATL_RECEIVED_PACKET_FLAG_VALID;
        Device->ReceiveNextToClean += 1;
        if (Device->ReceiveNextToClean == ATL1C_RECEIVE_FRAME_COUNT) {
            Device->ReceiveNextToClean = 0;
        }
    }

    //
    // If progress was made, let the controller know.
    //

    if (FramesProcessed != 0) {

        ASSERT(Device->ReceiveNextToClean != OriginalNextToClean);

        if (Device->ReceiveNextToClean == 0) {
            Value = ATL1C_RECEIVE_FRAME_COUNT - 1;

        } else {
            Value = Device->ReceiveNextToClean - 1;
        }

        ATL_WRITE_REGISTER32(Device, AtlRegisterReceiveFrameIndex, Value);
    }

    KeReleaseQueuedLock(Device->ReceiveLock);
    return;
}

VOID
AtlpDisableL0sAndL1 (
    PATL1C_DEVICE Device
    )

/*++

Routine Description:

    This routine disables the L0s and L1 link states.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    ULONG Value;

    //
    // Read the power management register and clear out the bits that are going
    // to be set by this function.
    //

    Value = ATL_READ_REGISTER32(Device, AtlRegisterPowerManagementControl);
    Value &= ~((ATL_POWER_MANAGEMENT_CONTROL_L1_ENTRY_TIME_MASK <<
                ATL_POWER_MANAGEMENT_CONTROL_L1_ENTRY_TIME_SHIFT) |
               ATL_POWER_MANAGEMENT_CONTROL_CLK_SWH_L1 |
               ATL_POWER_MANAGEMENT_CONTROL_L0S_ENABLE |
               ATL_POWER_MANAGEMENT_CONTROL_L1_ENABLE |
               ATL_POWER_MANAGEMENT_CONTROL_ASPM_MAC_CHECK |
               ATL_POWER_MANAGEMENT_CONTROL_SERDES_PD_EX_L1);

    Value |= ATL_POWER_MANAGEMENT_CONTROL_SERDES_BUFS_RECEIVE_L1_ENABLE |
             ATL_POWER_MANAGEMENT_CONTROL_SERDES_PLL_L1_ENABLE |
             ATL_POWER_MANAGEMENT_CONTROL_SERDES_L1_ENABLE;

    ATL_WRITE_REGISTER32(Device, AtlRegisterPowerManagementControl, Value);
    return;
}

BOOL
AtlpResetPhy (
    PATL1C_DEVICE Device
    )

/*++

Routine Description:

    This routine resets the ATL1c device PHY.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    RUNLEVEL OldRunLevel;
    USHORT PhyValue;
    BOOL Result;
    USHORT ShortValue;
    ULONG Value;

    Value = ATL_PHY_CONTROL_SEL_ANA_RESET;
    ATL_WRITE_REGISTER16(Device, AtlRegisterPhyControl, Value);
    ATL_READ_REGISTER16(Device, AtlRegisterPhyControl);
    HlBusySpin(10000);
    Value = ATL_PHY_CONTROL_EXT_RESET | ATL_PHY_CONTROL_SEL_ANA_RESET;
    ATL_WRITE_REGISTER16(Device, AtlRegisterPhyControl, Value);
    ATL_READ_REGISTER16(Device, AtlRegisterPhyControl);
    HlBusySpin(10000);
    OldRunLevel = AtlpAcquireInterruptLock(Device);
    Result = AtlpWritePhyDebugRegister(Device,
                                       ATL_PHY_DEBUG_LEGCYPS_REGISTER,
                                       ATL_PHY_DEBUG_LEGCYPS_VALUE);

    if (Result == FALSE) {
        goto ResetPhyEnd;
    }

    PhyValue = ATL_PHY_TST10BTCFG_LOOP_SEL_10BT |
               ATL_PHY_TST10BTCFG_EN_MASK_TB |
               ATL_PHY_TST10BTCFG_EN_10BT_IDLE |
               ATL_PHY_TST10BTCFG_INTERVAL_SEL_TIMER_VALUE;

    Result = AtlpWritePhyDebugRegister(Device,
                                       ATL_PHY_DEBUG_TST10BTCFG_REGISTER,
                                       PhyValue);

    if (Result == FALSE) {
        goto ResetPhyEnd;
    }

    PhyValue = ATL_PHY_SRDSYSMOD_SERDES_CDR_BW_VALUE |
               ATL_PHY_SRDSYSMOD_SERDES_EN_DEEM |
               ATL_PHY_SRDSYSMOD_SERDES_SEL_HSP |
               ATL_PHY_SRDSYSMOD_SERDES_ENABLE_PLL |
               ATL_PHY_SRDSYSMOD_SERDES_EN_LCKDT;

    Result = AtlpWritePhyDebugRegister(Device,
                                       ATL_PHY_DEBUG_SRDSYSMOD_REGISTER,
                                       PhyValue);

    if (Result == FALSE) {
        goto ResetPhyEnd;
    }

    PhyValue = ATL_PHY_TST100BTCFG_LONG_CABLE_TH_100_VALUE |
               ATL_PHY_TST100BTCFG_SHORT_CABLE_TH_100_VALUE |
               ATL_PHY_TST100BTCFG_BP_BAD_LINK_ACCUM |
               ATL_PHY_TST100BTCFG_BP_SMALL_BW;

    Result = AtlpWritePhyDebugRegister(Device,
                                       ATL_PHY_DEBUG_TST100BTCFG_REGISTER,
                                       PhyValue);

    if (Result == FALSE) {
        goto ResetPhyEnd;
    }

    PhyValue = ATL_PHY_SYSMODCTRL_IECHO_ADJ_3_VALUE |
               ATL_PHY_SYSMODCTRL_IECHO_ADJ_2_VALUE |
               ATL_PHY_SYSMODCTRL_IECHO_ADJ_1_VALUE |
               ATL_PHY_SYSMODCTRL_IECHO_ADJ_0_VALUE;

    Result = AtlpWritePhyDebugRegister(Device,
                                       ATL_PHY_DEBUG_SYSMODCTRL_REGISTER,
                                       PhyValue);

    if (Result == FALSE) {
        goto ResetPhyEnd;
    }

    PhyValue = ATL_PHY_ANA_CONTROL_MANUAL_SWITCH_ON_VALUE |
               ATL_PHY_ANA_CONTROL_RESTART_CAL |
               ATL_PHY_ANA_CONTROL_MAN_ENABLE | ATL_PHY_ANA_CONTROL_SEL_HSP |
               ATL_PHY_ANA_CONTROL_EN_HB | ATL_PHY_ANA_CONTROL_OEN_125M;

    Result = AtlpWritePhyDebugRegister(Device,
                                       ATL_PHY_DEBUG_ANA_CONTROL_REGISTER,
                                       PhyValue);

    if (Result == FALSE) {
        goto ResetPhyEnd;
    }

    HlBusySpin(1000);

    //
    // Disable hibernation.
    //

    Result = AtlpReadPhyDebugRegister(Device,
                                      ATL_PHY_DEBUG_LEGCYPS_REGISTER,
                                      &PhyValue);

    if (Result == FALSE) {
        goto ResetPhyEnd;
    }

    PhyValue &= ~ATL_PHY_ANA_CONTROL_SEL_CLK125M_DSP;
    Result = AtlpWritePhyDebugRegister(Device,
                                       ATL_PHY_DEBUG_LEGCYPS_REGISTER,
                                       PhyValue);

    if (Result == FALSE) {
        goto ResetPhyEnd;
    }

    Result = AtlpReadPhyDebugRegister(Device,
                                      ATL_PHY_DEBUG_HIBNEG_REGISTER,
                                      &PhyValue);

    if (Result == FALSE) {
        goto ResetPhyEnd;
    }

    PhyValue &= ~ATL_PHY_HIBNEG_PSHIB_ENABLE;
    Result = AtlpWritePhyDebugRegister(Device,
                                       ATL_PHY_DEBUG_HIBNEG_REGISTER,
                                       PhyValue);

    if (Result == FALSE) {
        goto ResetPhyEnd;
    }

    //
    // Enable interrupts from the PHY whenever the link changes.
    //

    ShortValue = ATL_PHY_INTERRUPT_ENABLE_LINK_UP |
                 ATL_PHY_INTERRUPT_ENABLE_LINK_DOWN;

    Result = AtlpPerformPhyRegisterIo(Device,
                                      TRUE,
                                      FALSE,
                                      0,
                                      ATL_PHY_INTERRUPT_ENABLE_REGISTER,
                                      &ShortValue);

    if (Result == FALSE) {
        goto ResetPhyEnd;
    }

    //
    // Try some other things too.
    //

    ShortValue = ATL_PHY_ADVERTISE_PAUSE | ATL_PHY_ADVERTISE_ASYMMETRIC_PAUSE |
                 ATL_PHY_ADVERTISE_10_HALF | ATL_PHY_ADVERTISE_10_FULL |
                 ATL_PHY_ADVERTISE_100_HALF | ATL_PHY_ADVERTISE_100_FULL;

    Result = AtlpPerformPhyRegisterIo(Device,
                                      TRUE,
                                      FALSE,
                                      0,
                                      ATL_PHY_ADVERTISE_REGISTER,
                                      &ShortValue);

    if (Result == FALSE) {
        goto ResetPhyEnd;
    }

    ShortValue = 0;
    Result = AtlpPerformPhyRegisterIo(Device,
                                      TRUE,
                                      FALSE,
                                      0,
                                      ATL_PHY_GIGABIT_CONTROL_REGISTER,
                                      &ShortValue);

    if (Result == FALSE) {
        goto ResetPhyEnd;
    }

    ShortValue = ATL_PHY_AUTONEGOTIATE_RESTART | ATL_PHY_AUTONEGOTIATE_ENABLE;
    Result = AtlpPerformPhyRegisterIo(Device,
                                      TRUE,
                                      FALSE,
                                      0,
                                      ATL_PHY_BASIC_MODE_CONTROL_REGISTER,
                                      &ShortValue);

    if (Result == FALSE) {
        goto ResetPhyEnd;
    }

    Result = TRUE;

ResetPhyEnd:
    AtlpReleaseInterruptLock(Device, OldRunLevel);
    return TRUE;
}

BOOL
AtlpApplyChipReset (
    PATL1C_DEVICE Device
    )

/*++

Routine Description:

    This routine applies a reset to the ATL1c controller.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG Value;

    Value = ATL_READ_REGISTER32(Device, AtlRegisterMasterControl) & 0xFFFF;
    Value |= ATL_MASTER_CONTROL_OOB_DISABLE | ATL_MASTER_CONTROL_SOFT_RESET;
    ATL_WRITE_REGISTER32(Device, AtlRegisterMasterControl, Value);
    HlBusySpin(10000);
    Value = ATL_READ_REGISTER32(Device, AtlRegisterMasterControl);
    if ((Value & ATL_MASTER_CONTROL_SOFT_RESET) != 0) {
        return FALSE;
    }

    if (AtlpWaitForIdleUnit(Device, ATL_IDLE_IO_MASK) != 0) {
        return FALSE;
    }

    return TRUE;
}

BOOL
AtlpStopController (
    PATL1C_DEVICE Device
    )

/*++

Routine Description:

    This routine stops the ATL1c ethernet controller.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    BOOL Result;
    ULONG Value;

    AtlpDisableDeviceInterrupts(Device);
    Result = AtlpStopQueue(Device);
    if (Result == FALSE) {
        return FALSE;
    }

    //
    // Disable DMA.
    //

    Value = ATL_READ_REGISTER32(Device, AtlRegisterDmaControl);
    Value &= ~(ATL_DMA_CONTROL_CMB_ENABLE | ATL_DMA_CONTROL_SMB_ENABLE);
    ATL_WRITE_REGISTER32(Device, AtlRegisterDmaControl, Value);
    HlBusySpin(1000);

    //
    // Stop the MAC.
    //

    Result = AtlpStopMac(Device);
    if (Result == FALSE) {
        return FALSE;
    }

    //
    // Disable interrupts one more time in case the work item came through
    // and re-enabled them.
    //

    AtlpDisableDeviceInterrupts(Device);
    return TRUE;
}

BOOL
AtlpStopMac (
    PATL1C_DEVICE Device
    )

/*++

Routine Description:

    This routine disables the MAC.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG Value;

    Value = ATL_READ_REGISTER32(Device, AtlRegisterMacControl);
    Value &= ~(ATL_MAC_CONTROL_TRANSMIT_ENABLED |
               ATL_MAC_CONTROL_RECEIVE_ENABLED);

    ATL_WRITE_REGISTER32(Device, AtlRegisterMacControl, Value);
    if (AtlpWaitForIdleUnit(Device, ATL_IDLE_IO_MASK) != 0) {
        return FALSE;
    }

    return TRUE;
}

BOOL
AtlpStopQueue (
    PATL1C_DEVICE Device
    )

/*++

Routine Description:

    This routine disables the ethernet controller transmit and receive
    queues.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG Value;

    Value = ATL_READ_REGISTER32(Device, AtlRegisterReceiveQueueControl);
    Value &= ~ATL_RECEIVE_QUEUE_CONTROL_ENABLED;
    ATL_WRITE_REGISTER32(Device, AtlRegisterReceiveQueueControl, Value);
    Value = ATL_READ_REGISTER32(Device, AtlRegisterTransmitQueueControl);
    Value &= ~ATL_TRANSMIT_QUEUE_CONTROL_ENABLED;
    ATL_WRITE_REGISTER32(Device, AtlRegisterTransmitQueueControl, Value);
    if (AtlpWaitForIdleUnit(Device, ATL_IDLE_IO_MASK) != 0) {
        return FALSE;
    }

    return TRUE;
}

VOID
AtlpSetupReceiveFilters (
    PATL1C_DEVICE Device
    )

/*++

Routine Description:

    This routine sets up the hardware receive filters, including promiscuous
    mode and multicast setup.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    ULONG Value;

    ASSERT(KeIsQueuedLockHeld(Device->ConfigurationLock) != FALSE);

    Value = ATL_READ_REGISTER32(Device, AtlRegisterMacControl);
    Value &= ~(ATL_MAC_CONTROL_ALL_MULTICAST_ENABLE |
               ATL_MAC_CONTROL_PROMISCUOUS_MODE_ENABLE);

    Value |= ATL_MAC_CONTROL_BROADCAST_ENABLED;
    if ((Device->EnabledCapabilities &
         NET_LINK_CAPABILITY_PROMISCUOUS_MODE) != 0) {

        Value |= ATL_MAC_CONTROL_PROMISCUOUS_MODE_ENABLE;
    }

    //
    // If there were multiple addresses to receive, this would be the place
    // to set the hash bits for each one. For now, just zero them out to only
    // receive at the current station address.
    //

    ATL_WRITE_REGISTER32(Device, AtlRegisterReceiveHashTable, 0);
    ATL_WRITE_REGISTER32(Device,
                         AtlRegisterReceiveHashTable + sizeof(ULONG),
                         0);

    ATL_WRITE_REGISTER32(Device, AtlRegisterMacControl, Value);
    return;
}

KSTATUS
AtlpEnableDevice (
    PATL1C_DEVICE Device
    )

/*++

Routine Description:

    This routine brings up the ATL1c device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    ATL_DUPLEX_MODE Duplex;
    ULONGLONG LinkSpeed;
    RUNLEVEL OldRunLevel;
    BOOL Result;
    USHORT ShortValue;
    ATL_SPEED Speed;
    KSTATUS Status;
    ULONG Value;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Status = STATUS_DEVICE_IO_ERROR;

    //
    // Read the PHY status register, twice.
    //

    ShortValue = 0;
    OldRunLevel = AtlpAcquireInterruptLock(Device);
    AtlpPerformPhyRegisterIo(Device,
                             FALSE,
                             FALSE,
                             0,
                             ATL_PHY_BASIC_MODE_STATUS_REGISTER,
                             &ShortValue);

    Result = AtlpPerformPhyRegisterIo(Device,
                                      FALSE,
                                      FALSE,
                                      0,
                                      ATL_PHY_BASIC_MODE_STATUS_REGISTER,
                                      &ShortValue);

    AtlpReleaseInterruptLock(Device, OldRunLevel);
    if (Result == FALSE) {
        RtlDebugPrint("ATL1c: Failed to read Basic Mode Status Register.\n");
        goto EnableDeviceEnd;
    }

    //
    // Tear things down if the link is down.
    //

    if ((ShortValue & ATL_PHY_BASIC_MODE_STATUS_LINK_UP) == 0) {
        NetSetLinkState(Device->NetworkLink, FALSE, 0);
        Result = AtlpStopController(Device);
        if (Result == FALSE) {
            goto EnableDeviceEnd;
        }

        AtlpSetActiveStatePowerManagement(Device, AtlSpeedOff);

        //
        // Reset the descriptor rings.
        //

        KeAcquireQueuedLock(Device->TransmitLock);
        Device->LinkActive = FALSE;
        AtlpResetTransmitRing(Device);
        KeReleaseQueuedLock(Device->TransmitLock);
        KeAcquireQueuedLock(Device->ReceiveLock);
        AtlpResetReceiveRing(Device);
        KeReleaseQueuedLock(Device->ReceiveLock);

    //
    // Go on, the link is up!
    //

    } else {
        Result = AtlpGetLinkCharacteristics(Device, &Speed, &Duplex);
        if (Result == FALSE) {
            RtlDebugPrint("ATL1c: Link up, but failed to get speed/duplex "
                          "information.\n");

            goto EnableDeviceEnd;
        }

        Device->LinkActive = TRUE;
        Device->Speed = Speed;
        Device->Duplex = Duplex;
        AtlpSetActiveStatePowerManagement(Device, Speed);
        switch (Device->Speed) {
        case AtlSpeed10:
            LinkSpeed = NET_SPEED_10_MBPS;
            break;

        case AtlSpeed100:
            LinkSpeed = NET_SPEED_100_MBPS;
            break;

        case AtlSpeed1000:
            LinkSpeed = NET_SPEED_1000_MBPS;
            break;

        default:

            ASSERT(FALSE);

            LinkSpeed = 0;
            break;
        }

        NetSetLinkState(Device->NetworkLink, TRUE, LinkSpeed);
        AtlpStartQueue(Device);
        AtlpConfigureMac(Device);

        //
        // Start the MAC, it's go time.
        //

        Value = ATL_READ_REGISTER32(Device, AtlRegisterMacControl);
        Value |= ATL_MAC_CONTROL_RECEIVE_ENABLED |
                 ATL_MAC_CONTROL_TRANSMIT_ENABLED;

        ATL_WRITE_REGISTER32(Device, AtlRegisterMacControl, Value);
    }

    Status = STATUS_SUCCESS;

EnableDeviceEnd:
    return Status;
}

VOID
AtlpStartQueue (
    PATL1C_DEVICE Device
    )

/*++

Routine Description:

    This routine starts the device's transmit and receive queues.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    ULONG Value;

    Value = ATL_READ_REGISTER32(Device, AtlRegisterReceiveQueueControl);
    Value |= ATL_RECEIVE_QUEUE_CONTROL_ENABLED0;
    ATL_WRITE_REGISTER32(Device, AtlRegisterReceiveQueueControl, Value);
    Value = ATL_READ_REGISTER32(Device, AtlRegisterTransmitQueueControl);
    Value |= ATL_TRANSMIT_QUEUE_CONTROL_ENABLED;
    ATL_WRITE_REGISTER32(Device, AtlRegisterTransmitQueueControl, Value);
    return;
}

VOID
AtlpConfigureMac (
    PATL1C_DEVICE Device
    )

/*++

Routine Description:

    This routine configures the MAC after a link has been established with the
    correct speed and duplex settings.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    ULONG Value;

    Value = ATL_READ_REGISTER32(Device, AtlRegisterMacControl);
    Value &= ~(ATL_MAC_CONTROL_DUPLEX |
               ATL_MAC_CONTROL_RECEIVE_FLOW_ENABLED |
               ATL_MAC_CONTROL_TRANSMIT_FLOW_ENABLED |
               (ATL_MAC_CONTROL_SPEED_MASK << ATL_MAC_CONTROL_SPEED_SHIFT));

    switch (Device->Speed) {
    case AtlSpeedOff:
        break;

    case AtlSpeed10:
    case AtlSpeed100:
        Value |= ATL_MAC_CONTROL_SPEED_10_100 << ATL_MAC_CONTROL_SPEED_SHIFT;
        break;

    case AtlSpeed1000:
        Value |= ATL_MAC_CONTROL_SPEED_1000 << ATL_MAC_CONTROL_SPEED_SHIFT;
        break;

    default:

        ASSERT(FALSE);

        return;
    }

    if (Device->Duplex == AtlDuplexFull) {
        Value |= ATL_MAC_CONTROL_DUPLEX |
                 ATL_MAC_CONTROL_RECEIVE_FLOW_ENABLED |
                 ATL_MAC_CONTROL_TRANSMIT_FLOW_ENABLED;
    }

    ATL_WRITE_REGISTER32(Device, AtlRegisterMacControl, Value);
    return;
}

BOOL
AtlpReadPhyDebugRegister (
    PATL1C_DEVICE Device,
    USHORT Register,
    PUSHORT Data
    )

/*++

Routine Description:

    This routine reads a PHY debug register.

Arguments:

    Device - Supplies a pointer to the device.

    Register - Supplies the register to read.

    Data - Supplies a pointer where the data will be returned.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    BOOL Result;

    *Data = 0;

    //
    // Write the address register.
    //

    Result = AtlpPerformPhyRegisterIo(Device,
                                      TRUE,
                                      FALSE,
                                      0,
                                      ATL_PHY_DEBUG_ADDRESS,
                                      &Register);

    if (Result == FALSE) {
        return FALSE;
    }

    //
    // Read the data register.
    //

    Result = AtlpPerformPhyRegisterIo(Device,
                                      FALSE,
                                      FALSE,
                                      0,
                                      ATL_PHY_DEBUG_DATA,
                                      Data);

    return Result;
}

BOOL
AtlpWritePhyDebugRegister (
    PATL1C_DEVICE Device,
    USHORT Register,
    USHORT Data
    )

/*++

Routine Description:

    This routine writes to a PHY debug register.

Arguments:

    Device - Supplies a pointer to the device.

    Register - Supplies the register to write.

    Data - Supplies the data to write.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    BOOL Result;

    //
    // Write the address register.
    //

    Result = AtlpPerformPhyRegisterIo(Device,
                                      TRUE,
                                      FALSE,
                                      0,
                                      ATL_PHY_DEBUG_ADDRESS,
                                      &Register);

    if (Result == FALSE) {
        return FALSE;
    }

    //
    // Write the data register.
    //

    Result = AtlpPerformPhyRegisterIo(Device,
                                      TRUE,
                                      FALSE,
                                      0,
                                      ATL_PHY_DEBUG_DATA,
                                      &Data);

    return Result;
}

BOOL
AtlpPerformPhyRegisterIo (
    PATL1C_DEVICE Device,
    BOOL Write,
    BOOL Extension,
    UCHAR Address,
    USHORT Register,
    PUSHORT Data
    )

/*++

Routine Description:

    This routine performs a PHY register read or write using the MDIO register.

Arguments:

    Device - Supplies a pointer to the device.

    Write - Supplies TRUE if this is a write operation or FALSE if this is a
        read operation.

    Extension - Supplies a boolean indicating whether or not this access is an
        extension register.

    Address - Supplies the address of the device (usually 0).

    Register - Supplies the register to access on the device.

    Data - Supplies a pointer that for writes supplies the data to write and
        for writes contains the value to write.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    USHORT ClockSelect;
    ULONG LoopIndex;
    ULONG MdioValue;
    BOOL Result;
    ULONG Value;

    ASSERT(KeIsSpinLockHeld(&(Device->InterruptLock)) != FALSE);
    ASSERT(KeGetRunLevel() >= RunLevelDispatch);

    //
    // Set up the MDIO control register, and potentially the extension register
    // for an extended register read.
    //

    ClockSelect = ATL_MDIO_CONTROL_CLOCK_25MHZ_DIVIDE_4;
    MdioValue = ATL_MDIO_CONTROL_SPRES_PRMBL |
                ((ClockSelect & ATL_MDIO_CONTROL_CLOCK_SELECT_MASK) <<
                 ATL_MDIO_CONTROL_CLOCK_SELECT_SHIFT) |
                ATL_MDIO_CONTROL_START;

    if (Extension != FALSE) {
        MdioValue |= ATL_MDIO_CONTROL_EXTENSION_MODE;
        Value = ((Address & ATL_MDIO_EXTENSION_DEVICE_ADDRESS_MASK) <<
                 ATL_MDIO_EXTENSION_DEVICE_ADDRESS_SHIFT) |
                ((Register & ATL_MDIO_EXTENSION_REGISTER_MASK) <<
                 ATL_MDIO_EXTENSION_REGISTER_SHIFT);

        ATL_WRITE_REGISTER32(Device, AtlRegisterMdioExtension, Value);

    } else {
        MdioValue |= (Register & ATL_MDIO_CONTROL_REGISTER_MASK) <<
                     ATL_MDIO_CONTROL_REGISTER_SHIFT;
    }

    if (Write != FALSE) {
        MdioValue |= (*Data & ATL_MDIO_CONTROL_DATA_MASK) <<
                     ATL_MDIO_CONTROL_DATA_SHIFT;

    } else {
        MdioValue |= ATL_MDIO_CONTROL_READ_OPERATION;
    }

    ATL_WRITE_REGISTER32(Device, AtlRegisterMdioControl, MdioValue);

    //
    // Wait for the MDIO module to become idle again.
    //

    for (LoopIndex = 0; LoopIndex < ATL_MDIO_WAIT_LOOP_COUNT; LoopIndex += 1) {
        Value = ATL_READ_REGISTER32(Device, AtlRegisterMdioControl);
        if ((Value & (ATL_MDIO_CONTROL_BUSY | ATL_MDIO_CONTROL_START)) == 0) {
            break;
        }

        HlBusySpin(ATL_MDIO_WAIT_LOOP_DELAY);
    }

    if (LoopIndex == ATL_MDIO_WAIT_LOOP_COUNT) {
        Result = FALSE;
        goto PerformPhyRegisterIoEnd;
    }

    //
    // Read the result out if this is a read.
    //

    if (Write == FALSE) {
        Value = ATL_READ_REGISTER32(Device, AtlRegisterMdioControl);
        *Data = (USHORT)((Value >> ATL_MDIO_CONTROL_DATA_SHIFT) &
                         ATL_MDIO_CONTROL_DATA_MASK);
    }

    Result = TRUE;

PerformPhyRegisterIoEnd:
    return Result;
}

ULONG
AtlpWaitForIdleUnit (
    PATL1C_DEVICE Device,
    ULONG BitsToBecomeClear
    )

/*++

Routine Description:

    This routine attempts to wait for the device to become idle.

Arguments:

    Device - Supplies a pointer to the device.

    BitsToBecomeClear - Supplies the bitmask of bits in the idle status register
        to read until they clear.

Return Value:

    0 if all the specified bits cleared.

    Returns the bitmask of bits that are stuck on if the operation timed out.

--*/

{

    ULONG IdleRegister;
    ULONG LoopIndex;

    IdleRegister = 0;
    for (LoopIndex = 0; LoopIndex < ATL_IDLE_WAIT_LOOP_COUNT; LoopIndex += 1) {
        IdleRegister = ATL_READ_REGISTER32(Device, AtlRegisterIdleStatus);
        if ((IdleRegister & BitsToBecomeClear) == 0) {
            return 0;
        }

        HlBusySpin(ATL_IDLE_WAIT_LOOP_DELAY);
    }

    return IdleRegister;
}

BOOL
AtlpReadMacAddress (
    PATL1C_DEVICE Device
    )

/*++

Routine Description:

    This routine reads the MAC address from the device.

Arguments:

    Device - Supplies a pointer to the device. The MAC address will be set in
        the device structure.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG LoopIndex;
    ULONG OtpControl;
    ULONG TwsiControl;

    //
    // First check the current address to see if a valid MAC address is already
    // programmed in. If so, use it. It may have been set by the BIOS.
    //

    if (AtlpReadCurrentMacAddress(Device) != FALSE) {
        return TRUE;
    }

    OtpControl = ATL_READ_REGISTER32(Device, AtlRegisterOtpControl);
    if (AtlpDoesEepromExist(Device) != FALSE) {

        //
        // Enable the OTP clock if it's not already on.
        //

        if ((OtpControl & ATL_OTP_CONTROL_CLOCK_ENABLE) == 0) {
            OtpControl |= ATL_OTP_CONTROL_CLOCK_ENABLE;
            ATL_WRITE_REGISTER32(Device, AtlRegisterOtpControl, OtpControl);
            HlBusySpin(1000);
        }

        TwsiControl = ATL_READ_REGISTER32(Device, AtlRegisterTwsiControl);
        TwsiControl |= ATL_TWSI_CONTROL_SOFTWARE_LOAD_START;
        ATL_WRITE_REGISTER32(Device, AtlRegisterTwsiControl, TwsiControl);
        for (LoopIndex = 0;
             LoopIndex < ATL_TWSI_EEPROM_LOOP_COUNT;
             LoopIndex += 1) {

            HlBusySpin(ATL_TWSI_EEPROM_LOOP_DELAY);
            TwsiControl = ATL_READ_REGISTER32(Device, AtlRegisterTwsiControl);
            if ((TwsiControl & ATL_TWSI_CONTROL_SOFTWARE_LOAD_START) == 0) {
                break;
            }
        }

        if (LoopIndex == ATL_TWSI_EEPROM_LOOP_COUNT) {
            return FALSE;
        }
    }

    //
    // Disable the OTP clock.
    //

    OtpControl &= ~ATL_OTP_CONTROL_CLOCK_ENABLE;
    ATL_WRITE_REGISTER32(Device, AtlRegisterOtpControl, OtpControl);
    HlBusySpin(1000);

    //
    // Now check to see if the current address is loaded.
    //

    if (AtlpReadCurrentMacAddress(Device) != FALSE) {
        return TRUE;
    }

    return FALSE;
}

BOOL
AtlpDoesEepromExist (
    PATL1C_DEVICE Device
    )

/*++

Routine Description:

    This routine determines if there is an EEPROM attached to the ethernet
    device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    TRUE if an EEPROM is present.

    FALSE if no EEPROM is connected.

--*/

{

    ULONG Value;

    Value = ATL_READ_REGISTER32(Device, AtlRegisterTwsiDebug);
    if ((Value & ATL_TWSI_DEBUG_DEVICE_EXISTS) != 0) {
        return TRUE;
    }

    Value = ATL_READ_REGISTER32(Device, AtlRegisterMasterControl);
    if ((Value & ATL_MASTER_CONTROL_OTP_SEL) != 0) {
        return TRUE;
    }

    return FALSE;
}

BOOL
AtlpReadCurrentMacAddress (
    PATL1C_DEVICE Device
    )

/*++

Routine Description:

    This routine reads the current MAC address programmed into the device and
    if valid, saves it into the device structure.

Arguments:

    Device - Supplies a pointer to the device. If the current MAC address is
        valid, it will be saved into this structure.

Return Value:

    TRUE if the current address was valid.

    FALSE if the address was not programmed (was an invalid address).

--*/

{

    ULONG Address1;
    USHORT Address2;
    BOOL InvalidAddress;

    Address1 = ATL_READ_REGISTER32(Device, AtlRegisterMacAddress1);
    Address2 = (USHORT)ATL_READ_REGISTER32(Device, AtlRegisterMacAddress2);
    InvalidAddress = FALSE;
    if ((Address1 == MAX_ULONG) && (Address2 == MAX_USHORT)) {
        InvalidAddress = TRUE;
    }

    if ((Address1 == 0) && (Address2 == 0)) {
        InvalidAddress = TRUE;
    }

    if (InvalidAddress == FALSE) {
        Address1 = CPU_TO_NETWORK32(Address1);
        Address2 = CPU_TO_NETWORK16(Address2);
        RtlCopyMemory(Device->EepromMacAddress, &Address2, sizeof(USHORT));
        RtlCopyMemory(&(Device->EepromMacAddress[sizeof(USHORT)]),
                      &Address1,
                      sizeof(ULONG));

        return TRUE;
    }

    return FALSE;
}

VOID
AtlpDisableDeviceInterrupts (
    PATL1C_DEVICE Device
    )

/*++

Routine Description:

    This routine disables interrupt generation for the device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    ATL_WRITE_REGISTER32(Device, AtlRegisterInterruptMask, 0);
    ATL_WRITE_REGISTER32(Device,
                         AtlRegisterInterruptStatus,
                         ATL_INTERRUPT_DISABLE);

    return;
}

VOID
AtlpEnableDeviceInterrupts (
    PATL1C_DEVICE Device
    )

/*++

Routine Description:

    This routine enables interrupt generation for the device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    ATL_WRITE_REGISTER32(Device,
                         AtlRegisterInterruptStatus,
                         ATL_INTERRUPT_MASK);

    ATL_WRITE_REGISTER32(Device,
                         AtlRegisterInterruptMask,
                         Device->EnabledInterrupts);

    return;
}

VOID
AtlpSetActiveStatePowerManagement (
    PATL1C_DEVICE Device,
    ATL_SPEED Speed
    )

/*++

Routine Description:

    This routine sets the PCI Express Active State Power Management
    configuration for the device based on the link speed.

Arguments:

    Device - Supplies a pointer to the device.

    Speed - Supplies the speed of the link.

Return Value:

    None.

--*/

{

    ULONG Value;

    //
    // Read the power management register and clear out the bits that are going
    // to be set by this function.
    //

    Value = ATL_READ_REGISTER32(Device, AtlRegisterPowerManagementControl);
    Value &= ~((ATL_POWER_MANAGEMENT_CONTROL_L1_ENTRY_TIME_MASK <<
                ATL_POWER_MANAGEMENT_CONTROL_L1_ENTRY_TIME_SHIFT) |
               ATL_POWER_MANAGEMENT_CONTROL_L0S_ENABLE |
               ATL_POWER_MANAGEMENT_CONTROL_L1_ENABLE |
               ATL_POWER_MANAGEMENT_CONTROL_ASPM_MAC_CHECK);

    //
    // Enable L0s and/or L1.
    //

    if (Speed != AtlSpeedOff) {
        Value |= ATL_POWER_MANAGEMENT_CONTROL_SERDES_L1_ENABLE |
                 ATL_POWER_MANAGEMENT_CONTROL_SERDES_PLL_L1_ENABLE |
                 ATL_POWER_MANAGEMENT_CONTROL_SERDES_BUFS_RECEIVE_L1_ENABLE |
                 ATL_POWER_MANAGEMENT_CONTROL_ASPM_MAC_CHECK;

        Value &= ~(ATL_POWER_MANAGEMENT_CONTROL_SERDES_PD_EX_L1 |
                   ATL_POWER_MANAGEMENT_CONTROL_CLK_SWH_L1 |
                   ATL_POWER_MANAGEMENT_CONTROL_L0S_ENABLE |
                   ATL_POWER_MANAGEMENT_CONTROL_L1_ENABLE);

    //
    // The link is down.
    //

    } else {
        Value |= ATL_POWER_MANAGEMENT_CONTROL_CLK_SWH_L1;
        Value &= ~(ATL_POWER_MANAGEMENT_CONTROL_SERDES_L1_ENABLE |
                   ATL_POWER_MANAGEMENT_CONTROL_SERDES_PLL_L1_ENABLE |
                   ATL_POWER_MANAGEMENT_CONTROL_SERDES_BUFS_RECEIVE_L1_ENABLE |
                   ATL_POWER_MANAGEMENT_CONTROL_L0S_ENABLE);
    }

    ATL_WRITE_REGISTER32(Device, AtlRegisterPowerManagementControl, Value);
    return;
}

VOID
AtlpResetTransmitRing (
    PATL1C_DEVICE Device
    )

/*++

Routine Description:

    This routine clears out and resets the transmit descriptor ring. This
    routine assumes the transmit lock is already held (or doesn't need to be).

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    USHORT CurrentIndex;
    USHORT DescriptorIndex;

    ASSERT(KeIsQueuedLockHeld(Device->TransmitLock) != FALSE);

    //
    // Clean out and free all descriptors.
    //

    for (DescriptorIndex = 0;
         DescriptorIndex < ATL1C_TRANSMIT_DESCRIPTOR_COUNT;
         DescriptorIndex += 1) {

        if (Device->TransmitBuffer[DescriptorIndex] != NULL) {

            ASSERT(Device->TransmitDescriptor[DescriptorIndex].BufferLength !=
                                                                            0);

            NetFreeBuffer(Device->TransmitBuffer[DescriptorIndex]);
            Device->TransmitBuffer[DescriptorIndex] = NULL;
        }
    }

    //
    // Clean out the pending transmit descriptors.
    //

    RtlZeroMemory(Device->TransmitDescriptor,
                  sizeof(ATL1C_TRANSMIT_DESCRIPTOR) *
                  (ATL1C_TRANSMIT_DESCRIPTOR_COUNT + 1));

    //
    // Destroy the list of packets waiting to be sent.
    //

    NetDestroyBufferList(&(Device->TransmitPacketList));

    //
    // Reset the counters in software and hardware based on the current index.
    // The current index cannot be reset by software.
    //

    CurrentIndex = ATL_READ_REGISTER16(Device,
                                       AtlRegisterTransmitCurrentIndex);

    Device->TransmitNextToUse = CurrentIndex;
    Device->TransmitNextToClean = CurrentIndex;
    ATL_WRITE_REGISTER16(Device,
                         AtlRegisterTransmitNextIndex,
                         Device->TransmitNextToUse);

    return;
}

VOID
AtlpResetReceiveRing (
    PATL1C_DEVICE Device
    )

/*++

Routine Description:

    This routine clears out the receive descriptor ring, discarding any
    packets that had come in but not been processed. This routine does not
    acquire the receive lock, it is assumed that the receive lock is already
    held or does not need to be.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    ULONG Value;

    RtlZeroMemory(Device->ReceivedPacket,
                  sizeof(ATL1C_RECEIVED_PACKET) * ATL1C_RECEIVE_FRAME_COUNT);

    if (Device->ReceiveNextToClean == 0) {
        Value = ATL1C_RECEIVE_FRAME_COUNT - 1;

    } else {
        Value = Device->ReceiveNextToClean - 1;
    }

    ATL_WRITE_REGISTER32(Device, AtlRegisterReceiveFrameIndex, Value);
    return;
}

BOOL
AtlpGetLinkCharacteristics (
    PATL1C_DEVICE Device,
    PATL_SPEED Speed,
    PATL_DUPLEX_MODE Duplex
    )

/*++

Routine Description:

    This routine gets the link information, including link speed and duplex
    status.

Arguments:

    Device - Supplies a pointer to the device with the link to query.

    Speed - Supplies a pointer where the link speed will be returned.

    Duplex - Supplies a pointer where the link duplex status will be returned.

Return Value:

    TRUE on success.

    FALSE if the link could not be queried.

--*/

{

    RUNLEVEL OldRunLevel;
    BOOL Result;
    USHORT Value;

    OldRunLevel = AtlpAcquireInterruptLock(Device);
    Result = AtlpPerformPhyRegisterIo(Device,
                                      FALSE,
                                      FALSE,
                                      0,
                                      ATL_PHY_GIGA_PSSR_REGISTER,
                                      &Value);

    AtlpReleaseInterruptLock(Device, OldRunLevel);
    if (Result == FALSE) {
        return FALSE;
    }

    if ((Value & ATL_PHY_GIGA_PSSR_SPEED_AND_DUPLEX_RESOLVED) == 0) {
        return FALSE;
    }

    switch (Value & ATL_PHY_GIGA_PSSR_SPEED_MASK) {
    case ATL_PHY_GIGA_PSSR_SPEED_1000:
        *Speed = AtlSpeed1000;
        break;

    case ATL_PHY_GIGA_PSSR_SPEED_100:
        *Speed = AtlSpeed100;
        break;

    case ATL_PHY_GIGA_PSSR_SPEED_10:
        *Speed = AtlSpeed10;
        break;

    default:
        return FALSE;
    }

    if ((Value & ATL_PHY_GIGA_PSSR_DUPLEX) != 0) {
        *Duplex = AtlDuplexFull;

    } else {
        *Duplex = AtlDuplexHalf;
    }

    return TRUE;
}

ULONG
AtlpHashAddress (
    PUCHAR MacAddress
    )

/*++

Routine Description:

    This routine computes the value to put in a device's hash table for the
    given MAC address.

Arguments:

    MacAddress - Supplies a pointer to the six byte MAC address.

Return Value:

    Returns the hashed value.

--*/

{

    ULONG BitIndex;
    ULONG Crc32;
    ULONG Hash;

    Hash = 0;

    //
    // The hash is the CRC32, but with all the bits reversed.
    //

    Crc32 = RtlComputeCrc32(0, MacAddress, 6);
    for (BitIndex = 0;
         BitIndex < (sizeof(ULONG) * BITS_PER_BYTE);
         BitIndex += 1) {

        Hash |= ((Crc32 >> BitIndex) & 0x1) <<
                ((sizeof(ULONG) * BITS_PER_BYTE) - 1 - BitIndex);
    }

    return Hash;
}

RUNLEVEL
AtlpAcquireInterruptLock (
    PATL1C_DEVICE Device
    )

/*++

Routine Description:

    This routine acquires the interrupt lock from outside the interrupt
    handler.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Returns the original run-level.

--*/

{

    RUNLEVEL OldRunLevel;

    ASSERT(KeGetRunLevel() <= RunLevelDispatch);

    if (Device->InterruptHandle != INVALID_HANDLE) {
        OldRunLevel = IoRaiseToInterruptRunLevel(Device->InterruptHandle);

    } else {
        OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    }

    KeAcquireSpinLock(&(Device->InterruptLock));
    return OldRunLevel;
}

VOID
AtlpReleaseInterruptLock (
    PATL1C_DEVICE Device,
    RUNLEVEL OldRunLevel
    )

/*++

Routine Description:

    This routine releases the interrupt lock from outside the interrupt
    handler.

Arguments:

    Device - Supplies a pointer to the device.

    OldRunLevel - Supplies the original run-level returned by the acquire
        function.

Return Value:

    None.

--*/

{

    ASSERT(KeGetRunLevel() >= RunLevelDispatch);

    KeReleaseSpinLock(&(Device->InterruptLock));
    KeLowerRunLevel(OldRunLevel);
    return;
}

