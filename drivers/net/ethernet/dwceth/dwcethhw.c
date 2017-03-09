/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dwcethhw.c

Abstract:

    This module implements the actual hardware support for the DesignWare
    Ethernet controller.

Author:

    Evan Green 5-Dec-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/net/netdrv.h>
#include <minoca/net/mii.h>
#include "dwceth.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Borrow an unused bit in the status register for the software link check.
//

#define DWE_STATUS_LINK_CHECK (1 << 11)

//
// Define the maximum amount of packets that DWE will keep queued before it
// starts to drop packets.
//

#define DWE_MAX_TRANSMIT_PACKET_LIST_COUNT (DWE_TRANSMIT_DESCRIPTOR_COUNT * 2)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
DwepLinkCheckDpc (
    PDPC Dpc
    );

KSTATUS
DwepInitializePhy (
    PDWE_DEVICE Device
    );

VOID
DwepReadMacAddress (
    PDWE_DEVICE Device
    );

VOID
DwepReapCompletedTransmitDescriptors (
    PDWE_DEVICE Device
    );

VOID
DwepSendPendingPackets (
    PDWE_DEVICE Device
    );

VOID
DwepReapReceivedFrames (
    PDWE_DEVICE Device
    );

KSTATUS
DwepCheckLink (
    PDWE_DEVICE Device
    );

KSTATUS
DwepDetermineLinkParameters (
    PDWE_DEVICE Device,
    PBOOL LinkUp,
    PULONGLONG Speed,
    PBOOL FullDuplex
    );

VOID
DwepUpdateFilterMode (
    PDWE_DEVICE Device
    );

KSTATUS
DwepReadMii (
    PDWE_DEVICE Device,
    ULONG Phy,
    ULONG Register,
    PULONG Result
    );

KSTATUS
DwepWriteMii (
    PDWE_DEVICE Device,
    ULONG Phy,
    ULONG Register,
    ULONG Value
    );

//
// -------------------------------------------------------------------- Globals
//

BOOL DweDisablePacketDropping = FALSE;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
DweSend (
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

    PDWE_DEVICE Device;
    UINTN PacketListCount;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Device = (PDWE_DEVICE)DeviceContext;
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
    if ((PacketListCount < DWE_MAX_TRANSMIT_PACKET_LIST_COUNT) ||
        (DweDisablePacketDropping != FALSE)) {

        NET_APPEND_PACKET_LIST(PacketList, &(Device->TransmitPacketList));
        DwepSendPendingPackets(Device);
        Status = STATUS_SUCCESS;

    //
    // Otherwise report that the resource is use as it is too busy to handle
    // more packets.
    //

    } else {
        Device->DroppedTxPackets += PacketList->Count;
        RtlDebugPrint("DWE: Dropped %d packets.\n", Device->DroppedTxPackets);
        Status = STATUS_RESOURCE_IN_USE;
    }

SendEnd:
    KeReleaseQueuedLock(Device->TransmitLock);
    return Status;
}

KSTATUS
DweGetSetInformation (
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
    PULONG Capabilities;
    ULONG ChangedCapabilities;
    PDWE_DEVICE Device;
    ULONG EnabledCapabilities;
    KSTATUS Status;
    ULONG SupportedCapabilities;
    ULONG Value;

    Device = DeviceContext;
    switch (InformationType) {
    case NetLinkInformationChecksumOffload:
        if (*DataSize != sizeof(ULONG)) {
            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        //
        // If the request is a get, just return the device's current checksum
        // capabilities.
        //

        Status = STATUS_SUCCESS;
        Capabilities = (PULONG)Data;
        if (Set == FALSE) {
            *Capabilities = Device->EnabledCapabilities &
                            NET_LINK_CAPABILITY_CHECKSUM_MASK;

            break;
        }

        //
        // Scrub the capabilities in case the caller supplied more than the
        // checksum bits and make sure all of the supplied capabilities are
        // supported.
        //

        *Capabilities &= NET_LINK_CAPABILITY_CHECKSUM_MASK;
        SupportedCapabilities = Device->SupportedCapabilities &
                                NET_LINK_CAPABILITY_CHECKSUM_MASK;

        if ((*Capabilities & ~SupportedCapabilities) != 0) {
            Status = STATUS_NOT_SUPPORTED;
            break;
        }

        //
        // Synchronize updates to the enabled capabilities field and the
        // reprogramming of the hardware register.
        //

        KeAcquireQueuedLock(Device->ConfigurationLock);

        //
        // If it is a set, figure out what is changing. There is nothing to do
        // if the change is in the transmit flags. Netcore requests transmit
        // offloads on a per-packet basis and there is not a global shut off on
        // DesignWare Ethernet devices. Requests to enable or disable receive
        // checksum change the MAC configuration.
        //

        ChangedCapabilities = (*Capabilities ^ Device->EnabledCapabilities) &
                              NET_LINK_CAPABILITY_CHECKSUM_MASK;

        if ((ChangedCapabilities &
             NET_LINK_CAPABILITY_CHECKSUM_RECEIVE_MASK) != 0) {

            //
            // If any of the receive checksum capapbilities are set, then
            // offloading must remain on for all protocols. There is no
            // granularity.
            //

            Value = DWE_READ(Device, DweRegisterMacConfiguration);
            if ((*Capabilities &
                 NET_LINK_CAPABILITY_CHECKSUM_RECEIVE_MASK) != 0) {

                Value |= DWE_MAC_CONFIGURATION_CHECKSUM_OFFLOAD;
                *Capabilities |= NET_LINK_CAPABILITY_CHECKSUM_RECEIVE_MASK;

            //
            // If all receive capabilities are off, turn receive checksum
            // offloadng off.
            //

            } else if ((*Capabilities &
                        NET_LINK_CAPABILITY_CHECKSUM_RECEIVE_MASK) == 0) {

                Value &= ~DWE_MAC_CONFIGURATION_CHECKSUM_OFFLOAD;
            }

            DWE_WRITE(Device, DweRegisterMacConfiguration, Value);
        }

        //
        // Update the checksum flags.
        //

        Device->EnabledCapabilities &= ~NET_LINK_CAPABILITY_CHECKSUM_MASK;
        Device->EnabledCapabilities |= *Capabilities;
        KeReleaseQueuedLock(Device->ConfigurationLock);
        break;

    case NetLinkInformationPromiscuousMode:
        if (*DataSize != sizeof(ULONG)) {
            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        Status = STATUS_SUCCESS;
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
        EnabledCapabilities = Device->EnabledCapabilities;
        if (*BooleanOption != FALSE) {
            EnabledCapabilities |= NET_LINK_CAPABILITY_PROMISCUOUS_MODE;

        } else {
            EnabledCapabilities &= ~NET_LINK_CAPABILITY_PROMISCUOUS_MODE;
        }

        if ((EnabledCapabilities ^ Device->EnabledCapabilities) != 0) {
            Device->EnabledCapabilities = EnabledCapabilities;
            DwepUpdateFilterMode(Device);
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
DwepInitializeDeviceStructures (
    PDWE_DEVICE Device
    )

/*++

Routine Description:

    This routine creates the data structures needed for a DesignWare Ethernet
    controller.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    ULONG Capabilities;
    ULONG CommandIndex;
    PDWE_DESCRIPTOR Descriptor;
    ULONG DescriptorPhysical;
    ULONG DescriptorSize;
    ULONG FrameIndex;
    ULONG IoBufferFlags;
    ULONG NextDescriptorPhysical;
    ULONG ReceiveFrameData;
    ULONG ReceiveSize;
    KSTATUS Status;

    //
    // Initialize the transmit and receive list locks.
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
    // By default, IP, UDP, and TCP checksum features are enabled.
    //

    Capabilities = NET_LINK_CAPABILITY_TRANSMIT_IP_CHECKSUM_OFFLOAD |
                   NET_LINK_CAPABILITY_TRANSMIT_UDP_CHECKSUM_OFFLOAD |
                   NET_LINK_CAPABILITY_TRANSMIT_TCP_CHECKSUM_OFFLOAD |
                   NET_LINK_CAPABILITY_RECEIVE_IP_CHECKSUM_OFFLOAD |
                   NET_LINK_CAPABILITY_RECEIVE_UDP_CHECKSUM_OFFLOAD |
                   NET_LINK_CAPABILITY_RECEIVE_TCP_CHECKSUM_OFFLOAD;

    Device->SupportedCapabilities |= Capabilities;
    Device->EnabledCapabilities |= Capabilities;

    //
    // Promiscuous mode is supported, but not enabled.
    //

    Device->SupportedCapabilities |= NET_LINK_CAPABILITY_PROMISCUOUS_MODE;

    //
    // Allocate the receive buffers. This is allocated as non-write though and
    // cacheable, which means software must be careful when the frame is
    // first received (and do an invalidate), and when setting up the
    // link pointers, but after the receive is complete it's normal memory.
    //

    ReceiveSize = DWE_RECEIVE_FRAME_DATA_SIZE * DWE_RECEIVE_FRAME_COUNT;

    ASSERT(Device->ReceiveDataIoBuffer == NULL);

    IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS;
    Device->ReceiveDataIoBuffer = MmAllocateNonPagedIoBuffer(0,
                                                             MAX_ULONG,
                                                             16,
                                                             ReceiveSize,
                                                             IoBufferFlags);

    if (Device->ReceiveDataIoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    ASSERT(Device->ReceiveDataIoBuffer->FragmentCount == 1);
    ASSERT(Device->ReceiveDataIoBuffer->Fragment[0].VirtualAddress != NULL);

    Device->ReceiveData =
                       Device->ReceiveDataIoBuffer->Fragment[0].VirtualAddress;

    //
    // Allocate both the transmit and the receive descriptors. This is
    // allocated non-cached as they are shared with the hardware.
    //

    DescriptorSize = (DWE_TRANSMIT_DESCRIPTOR_COUNT + DWE_RECEIVE_FRAME_COUNT) *
                     sizeof(DWE_DESCRIPTOR);

    ASSERT(Device->DescriptorIoBuffer == NULL);

    IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS;
    Device->DescriptorIoBuffer = MmAllocateNonPagedIoBuffer(0,
                                                            MAX_ULONG,
                                                            16,
                                                            DescriptorSize,
                                                            IoBufferFlags);

    if (Device->DescriptorIoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    ASSERT(Device->DescriptorIoBuffer->FragmentCount == 1);
    ASSERT(Device->DescriptorIoBuffer->Fragment[0].VirtualAddress != NULL);

    Device->TransmitDescriptors =
                        Device->DescriptorIoBuffer->Fragment[0].VirtualAddress;

    Device->ReceiveDescriptors = Device->TransmitDescriptors +
                                 DWE_TRANSMIT_DESCRIPTOR_COUNT;

    NET_INITIALIZE_PACKET_LIST(&(Device->TransmitPacketList));
    Device->TransmitBegin = 0;
    Device->TransmitEnd = 0;
    Device->ReceiveBegin = 0;
    RtlZeroMemory(Device->TransmitDescriptors, DescriptorSize);

    //
    // Allocate an array of pointers to net packet buffers that runs parallel
    // to the transmit array.
    //

    AllocationSize = sizeof(PNET_PACKET_BUFFER) * DWE_TRANSMIT_DESCRIPTOR_COUNT;
    Device->TransmitPacket = MmAllocateNonPagedPool(AllocationSize,
                                                    DWE_ALLOCATION_TAG);

    if (Device->TransmitPacket == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    RtlZeroMemory(Device->TransmitPacket, AllocationSize);

    ASSERT(Device->WorkItem == NULL);

    Device->WorkItem = KeCreateWorkItem(
                                NULL,
                                WorkPriorityNormal,
                                (PWORK_ITEM_ROUTINE)DwepInterruptServiceWorker,
                                Device,
                                DWE_ALLOCATION_TAG);

    if (Device->WorkItem == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    ASSERT(Device->LinkCheckTimer == NULL);

    Device->LinkCheckTimer = KeCreateTimer(DWE_ALLOCATION_TAG);
    if (Device->LinkCheckTimer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    Device->LinkCheckDpc = KeCreateDpc(DwepLinkCheckDpc, Device);
    if (Device->LinkCheckDpc == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    //
    // Initialize the receive frame list in chained mode.
    //

    DescriptorPhysical =
              (ULONG)(Device->DescriptorIoBuffer->Fragment[0].PhysicalAddress);

    DescriptorPhysical += sizeof(DWE_DESCRIPTOR) *
                          DWE_TRANSMIT_DESCRIPTOR_COUNT;

    NextDescriptorPhysical = DescriptorPhysical + sizeof(DWE_DESCRIPTOR);
    ReceiveFrameData =
             (ULONG)(Device->ReceiveDataIoBuffer->Fragment[0].PhysicalAddress);

    for (FrameIndex = 0;
         FrameIndex < DWE_RECEIVE_FRAME_COUNT;
         FrameIndex += 1) {

        Descriptor = &(Device->ReceiveDescriptors[FrameIndex]);
        Descriptor->Control = DWE_RX_STATUS_DMA_OWNED;
        Descriptor->BufferSize =
                              DWE_BUFFER_SIZE(DWE_RECEIVE_FRAME_DATA_SIZE, 0) |
                              DWE_RX_SIZE_CHAINED;

        Descriptor->Address1 = ReceiveFrameData;
        ReceiveFrameData += DWE_RECEIVE_FRAME_DATA_SIZE;
        if (FrameIndex == DWE_RECEIVE_FRAME_COUNT - 1) {
            Descriptor->Address2OrNextDescriptor = DescriptorPhysical;

        } else {
            Descriptor->Address2OrNextDescriptor = NextDescriptorPhysical;
        }

        NextDescriptorPhysical += sizeof(DWE_DESCRIPTOR);
    }

    //
    // Initialize the transmit descriptor list in chained mode. The "DMA owned"
    // bit is clear on all descriptors, so the controller doesn't try to
    // transmit them.
    //

    DescriptorPhysical =
              (ULONG)(Device->DescriptorIoBuffer->Fragment[0].PhysicalAddress);

    NextDescriptorPhysical = DescriptorPhysical + sizeof(DWE_DESCRIPTOR);
    for (CommandIndex = 0;
         CommandIndex < DWE_TRANSMIT_DESCRIPTOR_COUNT;
         CommandIndex += 1) {

        Descriptor = &(Device->TransmitDescriptors[CommandIndex]);
        Descriptor->Control = DWE_TX_CONTROL_CHAINED;

        //
        // Loop the last command back around to the first.
        //

        if (CommandIndex == DWE_TRANSMIT_DESCRIPTOR_COUNT - 1) {
            Descriptor->Address2OrNextDescriptor = DescriptorPhysical;

        //
        // Point this link at the next command.
        //

        } else {
            Descriptor->Address2OrNextDescriptor = NextDescriptorPhysical;
        }

        NextDescriptorPhysical += sizeof(DWE_DESCRIPTOR);
    }

    Status = STATUS_SUCCESS;

InitializeDeviceStructuresEnd:
    if (!KSUCCESS(Status)) {
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

        if (Device->ReceiveDataIoBuffer != NULL) {
            MmFreeIoBuffer(Device->ReceiveDataIoBuffer);
            Device->ReceiveDataIoBuffer = NULL;
            Device->ReceiveData = NULL;
        }

        if (Device->DescriptorIoBuffer != NULL) {
            MmFreeIoBuffer(Device->DescriptorIoBuffer);
            Device->DescriptorIoBuffer = NULL;
            Device->TransmitDescriptors = NULL;
            Device->ReceiveDescriptors = NULL;
        }

        if (Device->TransmitPacket != NULL) {
            MmFreeNonPagedPool(Device->TransmitPacket);
            Device->TransmitPacket = NULL;
        }

        if (Device->WorkItem != NULL) {
            KeDestroyWorkItem(Device->WorkItem);
            Device->WorkItem = NULL;
        }

        if (Device->LinkCheckTimer != NULL) {
            KeDestroyTimer(Device->LinkCheckTimer);
            Device->LinkCheckTimer = NULL;
        }

        if (Device->LinkCheckDpc != NULL) {
            KeDestroyDpc(Device->LinkCheckDpc);
            Device->LinkCheckDpc = NULL;
        }
    }

    return Status;
}

KSTATUS
DwepResetDevice (
    PDWE_DEVICE Device
    )

/*++

Routine Description:

    This routine resets the DesignWare Ethernet device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    ULONG DescriptorBase;
    ULONGLONG Frequency;
    KSTATUS Status;
    ULONGLONG Timeout;
    ULONG Value;

    //
    // Read the MAC address before resetting the device to get a MAC address
    // that might have been assigned by the firmware.
    //

    DwepReadMacAddress(Device);

    //
    // Perform a software reset, and wait for it to finish.
    //

    Value = DWE_READ(Device, DweRegisterBusMode);
    Value |= DWE_BUS_MODE_SOFTWARE_RESET;
    DWE_WRITE(Device, DweRegisterBusMode, Value);
    Frequency = HlQueryTimeCounterFrequency();
    Timeout = KeGetRecentTimeCounter() + Frequency;
    do {
        Value = DWE_READ(Device, DweRegisterBusMode);
        if ((Value & DWE_BUS_MODE_SOFTWARE_RESET) == 0) {
            break;
        }

        KeYield();

    } while (KeGetRecentTimeCounter() <= Timeout);

    if ((Value & DWE_BUS_MODE_SOFTWARE_RESET) != 0) {
        RtlDebugPrint("DWE: Cannot reset device.\n");
        return STATUS_DEVICE_IO_ERROR;
    }

    Value |= DWE_BUS_MODE_LARGE_DESCRIPTORS |
             DWE_BUS_MODE_8X_BURST_LENGTHS |
             (DWE_BUS_MODE_TX_BURST_LENGTH <<
              DWE_BUS_MODE_TX_BURST_LENGTH_SHIFT);

    DWE_WRITE(Device, DweRegisterBusMode, Value);

    //
    // Halt any DMA.
    //

    Value = DWE_READ(Device, DweRegisterOperationMode);
    Value &= ~(DWE_OPERATION_MODE_START_RECEIVE |
               DWE_OPERATION_MODE_START_TRANSMIT);

    DWE_WRITE(Device, DweRegisterOperationMode, Value);

    //
    // Write the descriptor base addresses.
    //

    DescriptorBase =
              (ULONG)(Device->DescriptorIoBuffer->Fragment[0].PhysicalAddress);

    DWE_WRITE(Device, DweRegisterTransmitDescriptorListAddress, DescriptorBase);
    DescriptorBase += sizeof(DWE_DESCRIPTOR) * DWE_TRANSMIT_DESCRIPTOR_COUNT;
    DWE_WRITE(Device, DweRegisterReceiveDescriptorListAddress, DescriptorBase);

    //
    // Set the MAC address.
    //

    RtlCopyMemory(&Value, Device->MacAddress, sizeof(ULONG));
    DWE_WRITE(Device, DWE_MAC_ADDRESS_LOW(0), Value);
    Value = 0;
    RtlCopyMemory(&Value, &(Device->MacAddress[sizeof(ULONG)]), sizeof(USHORT));
    DWE_WRITE(Device, DWE_MAC_ADDRESS_HIGH(0), Value);

    //
    // Set the initial filter mode.
    //

    DwepUpdateFilterMode(Device);

    //
    // Set up DMA.
    //

    Value = DWE_READ(Device, DweRegisterOperationMode);
    Value |= DWE_OPERATION_MODE_TX_STORE_AND_FORWARD |
             DWE_OPERATION_MODE_OPERATE_ON_SECOND_FRAME |
             DWE_OPERATION_MODE_FORWARD_UNDERSIZED_GOOD_FRAMES |
             DWE_OPERATION_MODE_RX_THRESHOLD_32;

    Value &= ~DWE_OPERATION_MODE_RX_STORE_AND_FORWARD;
    DWE_WRITE(Device, DweRegisterOperationMode, Value);
    DWE_WRITE(Device, DweRegisterInterruptEnable, DWE_INTERRUPT_ENABLE_DEFAULT);

    //
    // Disable interrupts that indicate when the counters get halfway or all
    // the way towards overflowing.
    //

    Value = DWE_RECEIVE_INTERRUPT_MASK;
    DWE_WRITE(Device, DweRegisterMmcReceiveInterruptMask, Value);
    Value = DWE_TRANSMIT_INTERRUPT_MASK;
    DWE_WRITE(Device, DweRegisterMmcTransmitInterruptMask, Value);
    Value = DWE_RECEIVE_CHECKSUM_INTERRUPT_MASK;
    DWE_WRITE(Device, DweRegisterReceiveChecksumOffloadInterruptMask, Value);

    //
    // Fire up DMA.
    //

    Value = DWE_READ(Device, DweRegisterOperationMode);
    Value |= DWE_OPERATION_MODE_START_TRANSMIT |
             DWE_OPERATION_MODE_START_RECEIVE;

    DWE_WRITE(Device, DweRegisterOperationMode, Value);

    //
    // Enable data flow.
    //

    Value = DWE_READ(Device, DweRegisterMacConfiguration);
    Value |= DWE_MAC_CONFIGURATION_JABBER_DISABLE |
             DWE_MAC_CONFIGURATION_AUTO_PAD_CRC_STRIPPING |
             DWE_MAC_CONFIGURATION_BURST_ENABLE |
             DWE_MAC_CONFIGURATION_TRANSMITTER_ENABLE |
             DWE_MAC_CONFIGURATION_RECEIVER_ENABLE;

    if ((Device->EnabledCapabilities &
         NET_LINK_CAPABILITY_CHECKSUM_RECEIVE_MASK) != 0) {

        Value |= DWE_MAC_CONFIGURATION_CHECKSUM_OFFLOAD;

    } else {
        Value &= ~DWE_MAC_CONFIGURATION_CHECKSUM_OFFLOAD;
    }

    DWE_WRITE(Device, DweRegisterMacConfiguration, Value);
    Status = DwepInitializePhy(Device);
    if (!KSUCCESS(Status)) {
        goto ResetDeviceEnd;
    }

    //
    // Notify the networking core of this new link now that the device is ready
    // to send and receive data, pending media being present.
    //

    if (Device->NetworkLink == NULL) {
        Status = DwepAddNetworkDevice(Device);
        if (!KSUCCESS(Status)) {
            goto ResetDeviceEnd;
        }
    }

    //
    // Determine whether or not there is media connected, and what speed it is.
    //

    Status = DwepCheckLink(Device);
    if (!KSUCCESS(Status)) {
        goto ResetDeviceEnd;
    }

    //
    // Fire up the link check timer.
    //

    Device->LinkCheckInterval = Frequency * DWE_LINK_CHECK_INTERVAL;
    KeQueueTimer(Device->LinkCheckTimer,
                 TimerQueueSoft,
                 0,
                 Device->LinkCheckInterval,
                 0,
                 Device->LinkCheckDpc);

ResetDeviceEnd:
    return Status;
}

INTERRUPT_STATUS
DwepInterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the DesignWare Ethernet interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the e100 device
        structure.

Return Value:

    Interrupt status.

--*/

{

    PDWE_DEVICE Device;
    INTERRUPT_STATUS InterruptStatus;
    ULONG PendingBits;

    Device = (PDWE_DEVICE)Context;
    InterruptStatus = InterruptStatusNotClaimed;

    //
    // Read the status register, and if anything's set add it to the pending
    // bits.
    //

    PendingBits = DWE_READ(Device, DweRegisterStatus);
    if (PendingBits != 0) {
        InterruptStatus = InterruptStatusClaimed;
        RtlAtomicOr32(&(Device->PendingStatusBits), PendingBits);

        //
        // Read the more detailed status register, and clear the bits.
        //

        DWE_WRITE(Device, DweRegisterStatus, PendingBits);
        if ((PendingBits & DWE_STATUS_ERROR_MASK) != 0) {
            RtlDebugPrint("DWE Error: 0x%08x\n", PendingBits);
        }
    }

    return InterruptStatus;
}

INTERRUPT_STATUS
DwepInterruptServiceWorker (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine processes interrupts for the DesignWare Ethernet controller at
    low level.

Arguments:

    Parameter - Supplies an optional parameter passed in by the creator of the
        work item.

Return Value:

    Interrupt status.

--*/

{

    ULONGLONG CurrentTime;
    PDWE_DEVICE Device;
    ULONG PendingBits;

    Device = (PDWE_DEVICE)(Parameter);

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Clear out the pending bits.
    //

    PendingBits = RtlAtomicExchange32(&(Device->PendingStatusBits), 0);
    if (PendingBits == 0) {
        return InterruptStatusNotClaimed;
    }

    if ((PendingBits & DWE_STATUS_RECEIVE_INTERRUPT) != 0) {
        DwepReapReceivedFrames(Device);
    }

    //
    // If the command unit finished what it was up to, reap that memory.
    //

    if ((PendingBits & DWE_STATUS_TRANSMIT_INTERRUPT) != 0) {
        DwepReapCompletedTransmitDescriptors(Device);
    }

    if ((PendingBits & DWE_STATUS_LINK_CHECK) != 0) {
        CurrentTime = KeGetRecentTimeCounter();
        Device->NextLinkCheck = CurrentTime + Device->LinkCheckInterval;
        DwepCheckLink(Device);
    }

    return InterruptStatusClaimed;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
DwepLinkCheckDpc (
    PDPC Dpc
    )

/*++

Routine Description:

    This routine implements the DesignWare Ethernet DPC that is queued when an
    interrupt fires.

Arguments:

    Dpc - Supplies a pointer to the DPC that is running.

Return Value:

    None.

--*/

{

    PDWE_DEVICE Device;
    ULONG OldPendingStatus;
    KSTATUS Status;

    Device = (PDWE_DEVICE)(Dpc->UserData);
    OldPendingStatus = RtlAtomicOr32(&(Device->PendingStatusBits),
                                     DWE_STATUS_LINK_CHECK);

    if ((OldPendingStatus & DWE_STATUS_LINK_CHECK) == 0) {
        Status = KeQueueWorkItem(Device->WorkItem);
        if (!KSUCCESS(Status)) {
            RtlAtomicAnd32(&(Device->PendingStatusBits),
                           ~DWE_STATUS_LINK_CHECK);
        }
    }

    return;
}

KSTATUS
DwepInitializePhy (
    PDWE_DEVICE Device
    )

/*++

Routine Description:

    This routine initializes the PHY on the DesignWare Ethernet Controller.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    ULONG BasicStatus;
    ULONG PhyId;
    KSTATUS Status;
    ULONG Value;

    //
    // Find the PHY.
    //

    Device->PhyId = (ULONG)-1;
    for (PhyId = 0; PhyId < MII_PHY_COUNT; PhyId += 1) {
        BasicStatus = 0;
        Status = DwepReadMii(Device,
                             PhyId,
                             MiiRegisterBasicStatus,
                             &BasicStatus);

        //
        // If the register presents at least one of the connection
        // possibilities, then assume its valid.
        //

        if ((KSUCCESS(Status)) && (BasicStatus != 0) &&
            (BasicStatus != MAX_USHORT) &&
            ((BasicStatus &
              (MII_BASIC_STATUS_MEDIA_MASK |
               MII_BASIC_STATUS_EXTENDED_STATUS)) != 0)) {

            Device->PhyId = PhyId;
            break;
        }
    }

    //
    // If no PHY was found, fail to start.
    //

    if (Device->PhyId == (ULONG)-1) {
        return STATUS_NO_SUCH_DEVICE;
    }

    //
    // TODO: This should be in generic MII code.
    //

    Status = DwepWriteMii(Device,
                          Device->PhyId,
                          MiiRegisterBasicControl,
                          MII_BASIC_CONTROL_RESET);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    Value = MII_ADVERTISE_ALL | MII_ADVERTISE_CSMA | MII_ADVERTISE_PAUSE |
            MII_ADVERTISE_PAUSE_ASYMMETRIC;

    Status = DwepWriteMii(Device, Device->PhyId, MiiRegisterAdvertise, Value);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    return Status;
}

VOID
DwepReadMacAddress (
    PDWE_DEVICE Device
    )

/*++

Routine Description:

    This routine reads the current MAC address out of the DesignWare Ethernet
    controller.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    ULONG AddressHigh;
    ULONG AddressLow;

    if (Device->MacAddressAssigned != FALSE) {
        return;
    }

    AddressLow = DWE_READ(Device, DWE_MAC_ADDRESS_LOW(0));
    AddressHigh = DWE_READ(Device, DWE_MAC_ADDRESS_HIGH(0)) & 0x0000FFFF;
    if ((AddressLow != 0xFFFFFFFF) || (AddressHigh != 0x0000FFFF)) {
        RtlCopyMemory(Device->MacAddress, &AddressLow, sizeof(ULONG));
        RtlCopyMemory(&(Device->MacAddress[sizeof(ULONG)]),
                      &AddressHigh,
                      sizeof(USHORT));

    } else {
        NetCreateEthernetAddress(Device->MacAddress);
    }

    Device->MacAddressAssigned = TRUE;
    return;
}

VOID
DwepReapCompletedTransmitDescriptors (
    PDWE_DEVICE Device
    )

/*++

Routine Description:

    This routine cleans out any transmit descriptors completed by the hardware.
    This routine must be called at low level and assumes the command list lock
    is already held.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    UINTN Begin;
    PDWE_DESCRIPTOR Descriptor;
    BOOL DescriptorReaped;

    DescriptorReaped = FALSE;
    KeAcquireQueuedLock(Device->TransmitLock);
    while (TRUE) {
        Begin = Device->TransmitBegin;
        Descriptor = &(Device->TransmitDescriptors[Begin]);

        //
        // If the buffer size word is zeroed, that's the indication that this
        // descriptor has already been cleaned out.
        //

        if (Descriptor->BufferSize == 0) {
            break;
        }

        //
        // If the command, whatever it may be, is not complete, then this is
        // an active entry, so stop reaping.
        //

        if ((Descriptor->Control & DWE_TX_CONTROL_DMA_OWNED) != 0) {
            break;
        }

        if ((Descriptor->Control & DWE_TX_CONTROL_ERROR_MASK) != 0) {
            RtlDebugPrint("DWE: TX Error 0x%x\n", Descriptor->Control);
        }

        //
        // Free up the packet and mark the descriptor as free for use by
        // zeroing out the control.
        //

        NetFreeBuffer(Device->TransmitPacket[Begin]);
        Device->TransmitPacket[Begin] = NULL;
        Descriptor->BufferSize = 0;
        DescriptorReaped = TRUE;

        //
        // Move the beginning of the list forward.
        //

        if (Begin == DWE_TRANSMIT_DESCRIPTOR_COUNT - 1) {
            Device->TransmitBegin = 0;

        } else {
            Device->TransmitBegin = Begin + 1;
        }
    }

    if (DescriptorReaped != FALSE) {
        DwepSendPendingPackets(Device);
    }

    KeReleaseQueuedLock(Device->TransmitLock);
    return;
}

VOID
DwepSendPendingPackets (
    PDWE_DEVICE Device
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

    PHYSICAL_ADDRESS BufferPhysical;
    ULONG Control;
    PDWE_DESCRIPTOR Descriptor;
    ULONG DescriptorIndex;
    PNET_PACKET_BUFFER Packet;
    BOOL PacketSent;
    ULONG PacketSize;

    //
    // Send as many packets as possible.
    //

    PacketSent = FALSE;
    while (NET_PACKET_LIST_EMPTY(&(Device->TransmitPacketList)) == FALSE) {
        Packet = LIST_VALUE(Device->TransmitPacketList.Head.Next,
                            NET_PACKET_BUFFER,
                            ListEntry);

        DescriptorIndex = Device->TransmitEnd;
        Descriptor = &(Device->TransmitDescriptors[DescriptorIndex]);
        if (Descriptor->BufferSize != 0) {
            break;
        }

        NET_REMOVE_PACKET_FROM_LIST(Packet, &(Device->TransmitPacketList));

        //
        // Success, a free descriptor. Let's fill it out!
        //

        Control = DWE_TX_CONTROL_CHAINED |
                  DWE_TX_CONTROL_FIRST_SEGMENT |
                  DWE_TX_CONTROL_LAST_SEGMENT |
                  DWE_TX_CONTROL_INTERRUPT_ON_COMPLETE |
                  DWE_TX_CONTROL_CHECKSUM_NONE |
                  DWE_TX_CONTROL_DMA_OWNED;

        if ((Packet->Flags & NET_PACKET_FLAG_IP_CHECKSUM_OFFLOAD) != 0) {
            if ((Packet->Flags &
                 (NET_PACKET_FLAG_TCP_CHECKSUM_OFFLOAD |
                  NET_PACKET_FLAG_UDP_CHECKSUM_OFFLOAD)) != 0) {

                Control |= DWE_TX_CONTROL_CHECKSUM_PSEUDOHEADER;

            } else {
                Control |= DWE_TX_CONTROL_CHECKSUM_IP_HEADER;
            }
        }

        //
        // Fill out the transfer buffer pointer and size.
        //

        PacketSize = Packet->FooterOffset - Packet->DataOffset;
        Descriptor->BufferSize = DWE_BUFFER_SIZE(PacketSize, 0);
        BufferPhysical = Packet->BufferPhysicalAddress + Packet->DataOffset;

        ASSERT(BufferPhysical == (ULONG)BufferPhysical);

        Descriptor->Address1 = BufferPhysical;
        Device->TransmitPacket[DescriptorIndex] = Packet;

        //
        // Use a register write to write the new control value in, making
        // it live in the hardware.
        //

        HlWriteRegister32(&(Descriptor->Control), Control);

        //
        // Move the pointer past this entry.
        //

        if (DescriptorIndex == DWE_TRANSMIT_DESCRIPTOR_COUNT - 1) {
            Device->TransmitEnd = 0;

        } else {
            Device->TransmitEnd = DescriptorIndex + 1;
        }

        PacketSent = TRUE;
    }

    //
    // Write the transmit poll demand register to make the hardware take a look
    // at the transmit queue again.
    //

    if (PacketSent != FALSE) {
        DWE_WRITE(Device, DweRegisterTransmitPollDemand, 1);
    }

    return;
}

VOID
DwepReapReceivedFrames (
    PDWE_DEVICE Device
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

    UINTN Begin;
    PDWE_DESCRIPTOR Descriptor;
    ULONG ExtendedStatus;
    NET_PACKET_BUFFER Packet;
    ULONG PayloadType;
    ULONG ReceivePhysical;
    PVOID ReceiveVirtual;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Loop grabbing completed frames.
    //

    Packet.Flags = 0;
    KeAcquireQueuedLock(Device->ReceiveLock);
    ReceivePhysical =
             (ULONG)(Device->ReceiveDataIoBuffer->Fragment[0].PhysicalAddress);

    ReceiveVirtual = Device->ReceiveDataIoBuffer->Fragment[0].VirtualAddress;
    while (TRUE) {
        Begin = Device->ReceiveBegin;
        Descriptor = &(Device->ReceiveDescriptors[Begin]);

        //
        // If the frame is not complete, then this is the end of packets that
        // need to be reaped.
        //

        if ((Descriptor->Control & DWE_RX_STATUS_DMA_OWNED) != 0) {
            break;
        }

        //
        // If the frame came through alright, send it up to the core networking
        // library to process.
        //

        if ((Descriptor->Control & DWE_RX_STATUS_ERROR_MASK) == 0) {
            Packet.Buffer = ReceiveVirtual +
                            (Begin * DWE_RECEIVE_FRAME_DATA_SIZE);

            Packet.BufferPhysicalAddress =
                       ReceivePhysical + (Begin * DWE_RECEIVE_FRAME_DATA_SIZE);

            Packet.BufferSize = (Descriptor->Control >>
                                 DWE_RX_STATUS_FRAME_LENGTH_SHIFT) &
                                DWE_RX_STATUS_FRAME_LENGTH_MASK;

            Packet.DataSize = Packet.BufferSize;
            Packet.DataOffset = 0;
            Packet.FooterOffset = Packet.DataSize;
            Packet.Flags = 0;

            //
            // If the extended status bits are set, figure out if checksum
            // offloading occurred.
            //

            if ((Descriptor->Control & DWE_RX_STATUS_EXTENDED_STATUS) != 0) {
                ExtendedStatus = Descriptor->ExtendedStatus;

                //
                // If an IP header error occurred, leave it at that.
                //

                if ((ExtendedStatus &
                     DWE_RX_STATUS2_IP_HEADER_ERROR) != 0) {

                     Packet.Flags |= NET_PACKET_FLAG_IP_CHECKSUM_OFFLOAD |
                                     NET_PACKET_FLAG_IP_CHECKSUM_FAILED;

                //
                // If the checksum was not bypassed, then the IP header
                // checksum was valid.
                //

                } else if ((ExtendedStatus &
                            DWE_RX_STATUS2_IP_CHECKSUM_BYPASSED) == 0) {

                    Packet.Flags |= NET_PACKET_FLAG_IP_CHECKSUM_OFFLOAD;
                    PayloadType = ExtendedStatus &
                                  DWE_RX_STATUS2_IP_PAYLOAD_TYPE_MASK;

                    //
                    // Handle a TCP packet.
                    //

                    if (PayloadType == DWE_RX_STATUS2_IP_PAYLOAD_TCP) {
                        Packet.Flags |= NET_PACKET_FLAG_TCP_CHECKSUM_OFFLOAD;
                        if ((ExtendedStatus &
                             DWE_RX_STATUS2_IP_PAYLOAD_ERROR) != 0) {

                            Packet.Flags |= NET_PACKET_FLAG_TCP_CHECKSUM_FAILED;
                        }

                    //
                    // Handle a UDP packet.
                    //

                    } else if (PayloadType == DWE_RX_STATUS2_IP_PAYLOAD_UDP) {
                        Packet.Flags |= NET_PACKET_FLAG_UDP_CHECKSUM_OFFLOAD;
                        if ((ExtendedStatus &
                             DWE_RX_STATUS2_IP_PAYLOAD_ERROR) != 0) {

                            Packet.Flags |= NET_PACKET_FLAG_UDP_CHECKSUM_FAILED;
                        }
                    }
                }
            }

            NetProcessReceivedPacket(Device->NetworkLink, &Packet);

        } else {
            RtlDebugPrint("DWE: RX Error 0x%08x\n", Descriptor->Control);
        }

        //
        // Set this frame up to be reused, it will be the new end of the list.
        //

        HlWriteRegister32(&(Descriptor->Control), DWE_RX_STATUS_DMA_OWNED);

        //
        // Move the beginning pointer up.
        //

        if (Begin == DWE_RECEIVE_FRAME_COUNT - 1) {
            Device->ReceiveBegin = 0;

        } else {
            Device->ReceiveBegin = Begin + 1;
        }
    }

    KeReleaseQueuedLock(Device->ReceiveLock);
    return;
}

KSTATUS
DwepCheckLink (
    PDWE_DEVICE Device
    )

/*++

Routine Description:

    This routine checks to see if the media is connected and at what speed.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    BOOL FullDuplex;
    BOOL LinkUp;
    ULONGLONG Speed;
    KSTATUS Status;
    ULONG Value;

    Status = DwepDetermineLinkParameters(Device, &LinkUp, &Speed, &FullDuplex);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    if ((Device->LinkActive != LinkUp) ||
        (Device->LinkSpeed != Speed) ||
        (Device->FullDuplex != FullDuplex)) {

        //
        // Synchronize access to the MAC configuration register. It is also
        // accessed when setting device information.
        //

        KeAcquireQueuedLock(Device->ConfigurationLock);
        Value = DWE_READ(Device, DweRegisterMacConfiguration);
        if (Speed == NET_SPEED_1000_MBPS) {
            Value &= ~(DWE_MAC_CONFIGURATION_RMII_SPEED_100 |
                       DWE_MAC_CONFIGURATION_RMII_NOT_GIGABIT);

        } else if (Speed == NET_SPEED_100_MBPS) {
            Value |= DWE_MAC_CONFIGURATION_RMII_SPEED_100 |
                     DWE_MAC_CONFIGURATION_RMII_NOT_GIGABIT;

        } else if (Speed == NET_SPEED_10_MBPS) {
            Value &= ~DWE_MAC_CONFIGURATION_RMII_SPEED_100;
            Value |= DWE_MAC_CONFIGURATION_RMII_NOT_GIGABIT;
        }

        Value &= ~DWE_MAC_CONFIGURATION_DUPLEX_MODE;
        if (FullDuplex != FALSE) {
            Value |= DWE_MAC_CONFIGURATION_DUPLEX_MODE;
        }

        DWE_WRITE(Device, DweRegisterMacConfiguration, Value);
        KeReleaseQueuedLock(Device->ConfigurationLock);
        Device->LinkActive = LinkUp;
        Device->LinkSpeed = Speed;
        Device->FullDuplex = FullDuplex;
        NetSetLinkState(Device->NetworkLink, LinkUp, Speed);
    }

    return STATUS_SUCCESS;
}

KSTATUS
DwepDetermineLinkParameters (
    PDWE_DEVICE Device,
    PBOOL LinkUp,
    PULONGLONG Speed,
    PBOOL FullDuplex
    )

/*++

Routine Description:

    This routine reads the link parameters out of the PHY.

Arguments:

    Device - Supplies a pointer to the device.

    LinkUp - Supplies a pointer where a boolean will be returned indicating if
        the media is connected or not.

    Speed - Supplies a pointer where the link speed will be returned if
        connected.

    FullDuplex - Supplies a pointer where a boolean will be returned indicating
        if the connection is full duplex (TRUE) or half duplex (FALSE).

Return Value:

    Status code.

--*/

{

    ULONG BasicControl;
    ULONG BasicStatus;
    ULONG BasicStatus2;
    ULONG CommonLink;
    ULONG GigabitControl;
    ULONG GigabitStatus;
    BOOL HasGigabit;
    ULONG PartnerAbility;
    KSTATUS Status;

    *LinkUp = FALSE;
    *Speed = NET_SPEED_NONE;
    *FullDuplex = FALSE;
    HasGigabit = FALSE;
    Status = DwepReadMii(Device,
                         Device->PhyId,
                         MiiRegisterBasicStatus,
                         &BasicStatus);

    if (!KSUCCESS(Status)) {
        goto DetermineLinkParametersEnd;
    }

    Status = DwepReadMii(Device,
                         Device->PhyId,
                         MiiRegisterBasicStatus,
                         &BasicStatus2);

    if (!KSUCCESS(Status)) {
        goto DetermineLinkParametersEnd;
    }

    BasicStatus |= BasicStatus2;
    if ((BasicStatus & MII_BASIC_STATUS_LINK_STATUS) == 0) {
        goto DetermineLinkParametersEnd;
    }

    Status = DwepReadMii(Device,
                         Device->PhyId,
                         MiiRegisterBasicControl,
                         &BasicControl);

    if (!KSUCCESS(Status)) {
        goto DetermineLinkParametersEnd;
    }

    if ((BasicControl & MII_BASIC_CONTROL_ISOLATE) != 0) {
        goto DetermineLinkParametersEnd;
    }

    if ((BasicControl & MII_BASIC_CONTROL_LOOPBACK) != 0) {
        RtlDebugPrint("MII Loopback enabled!\n");
    }

    //
    // The link status bit is set, so media is connected. Determine what type.
    //

    *LinkUp = TRUE;
    if ((BasicControl & MII_BASIC_CONTROL_ENABLE_AUTONEGOTIATION) != 0) {
        if ((BasicStatus & MII_BASIC_STATUS_AUTONEGOTIATE_COMPLETE) == 0) {
            *LinkUp = FALSE;
            goto DetermineLinkParametersEnd;
        }

        //
        // Take the common set of the advertised abilities and the partner's
        // abilities.
        //

        Status = DwepReadMii(Device,
                             Device->PhyId,
                             MiiRegisterAdvertise,
                             &CommonLink);

        if (!KSUCCESS(Status)) {
            goto DetermineLinkParametersEnd;
        }

        Status = DwepReadMii(Device,
                             Device->PhyId,
                             MiiRegisterLinkPartnerAbility,
                             &PartnerAbility);

        if (!KSUCCESS(Status)) {
            goto DetermineLinkParametersEnd;
        }

        CommonLink &= PartnerAbility;
        GigabitStatus = 0;
        GigabitControl = 0;
        if (HasGigabit != FALSE) {
            Status = DwepReadMii(Device,
                                 Device->PhyId,
                                 MiiRegisterGigabitStatus,
                                 &GigabitStatus);

            if (!KSUCCESS(Status)) {
                goto DetermineLinkParametersEnd;
            }

            Status = DwepReadMii(Device,
                                 Device->PhyId,
                                 MiiRegisterGigabitControl,
                                 &GigabitControl);

            if (!KSUCCESS(Status)) {
                goto DetermineLinkParametersEnd;
            }
        }

        if (((GigabitControl & MII_GIGABIT_CONTROL_ADVERTISE_1000_FULL) != 0) &&
            ((GigabitStatus & MII_GIGABIT_STATUS_PARTNER_1000_FULL) != 0)) {

            *Speed = NET_SPEED_1000_MBPS;
            *FullDuplex = TRUE;

        } else if (((GigabitControl &
                     MII_GIGABIT_CONTROL_ADVERTISE_1000_HALF) != 0) &&
                   ((GigabitStatus &
                     MII_GIGABIT_STATUS_PARTNER_1000_HALF) != 0)) {

            *Speed = NET_SPEED_1000_MBPS;
            *FullDuplex = TRUE;

        } else if ((CommonLink & MII_ADVERTISE_100_FULL) != 0) {
            *Speed = NET_SPEED_100_MBPS;
            *FullDuplex = TRUE;

        } else if ((CommonLink & MII_ADVERTISE_100_BASE4) != 0) {
            *Speed = NET_SPEED_100_MBPS;
            *FullDuplex = TRUE;

        } else if ((CommonLink & MII_ADVERTISE_100_HALF) != 0) {
            *Speed = NET_SPEED_100_MBPS;
            *FullDuplex = FALSE;

        } else if ((CommonLink & MII_ADVERTISE_10_FULL) != 0) {
            *Speed = NET_SPEED_10_MBPS;
            *FullDuplex = TRUE;

        } else if ((CommonLink & MII_ADVERTISE_10_HALF) != 0) {
            *Speed = NET_SPEED_10_MBPS;
            *FullDuplex = FALSE;

        } else {
            *LinkUp = FALSE;
        }
    }

DetermineLinkParametersEnd:
    return Status;
}

VOID
DwepUpdateFilterMode (
    PDWE_DEVICE Device
    )

/*++

Routine Description:

    This routine updates a DesignWare Ethernet device's filter mode based on
    the currently enabled capabilities.

Arguments:

    Device - Supplies a pointer to the DWE device.

Return Value:

    None.

--*/

{

    ULONG Value;

    Value = DWE_MAC_FRAME_FILTER_HASH_MULTICAST;
    if ((Device->EnabledCapabilities &
         NET_LINK_CAPABILITY_PROMISCUOUS_MODE) != 0) {

        Value |= DWE_MAC_FRAME_FILTER_PROMISCUOUS;
    }

    DWE_WRITE(Device, DweRegisterMacFrameFilter, Value);
    return;
}

KSTATUS
DwepReadMii (
    PDWE_DEVICE Device,
    ULONG Phy,
    ULONG Register,
    PULONG Result
    )

/*++

Routine Description:

    This routine reads a register from the PHY.

Arguments:

    Device - Supplies a pointer to the device.

    Phy - Supplies the address of the PHY.

    Register - Supplies the register to read.

    Result - Supplies a pointer where the result will be returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_DEVICE_IO_ERROR if the device could not be read.

--*/

{

    USHORT Mii;
    ULONGLONG Timeout;

    Mii = ((Phy & DWE_GMII_ADDRESS_DEVICE_MASK) <<
           DWE_GMII_ADDRESS_DEVICE_SHIFT) |
          ((Register & DWE_GMII_ADDRESS_REGISTER_MASK) <<
           DWE_GMII_ADDRESS_REGISTER_SHIFT) |
          (DWE_MII_CLOCK_VALUE << DWE_GMII_ADDRESS_CLOCK_RANGE_SHIFT) |
          DWE_GMII_ADDRESS_BUSY;

    DWE_WRITE(Device, DweRegisterGmiiAddress, Mii);
    Timeout = KeGetRecentTimeCounter() +
              (HlQueryTimeCounterFrequency() * DWE_MII_TIMEOUT);

    do {
        Mii = DWE_READ(Device, DweRegisterGmiiAddress);
        if ((Mii & DWE_GMII_ADDRESS_BUSY) == 0) {
            break;
        }

        KeYield();

    } while (KeGetRecentTimeCounter() <= Timeout);

    if ((Mii & DWE_GMII_ADDRESS_BUSY) != 0) {
        return STATUS_DEVICE_IO_ERROR;
    }

    *Result = DWE_READ(Device, DweRegisterGmiiData);
    return STATUS_SUCCESS;
}

KSTATUS
DwepWriteMii (
    PDWE_DEVICE Device,
    ULONG Phy,
    ULONG Register,
    ULONG Value
    )

/*++

Routine Description:

    This routine writes a register to the PHY.

Arguments:

    Device - Supplies a pointer to the device.

    Phy - Supplies the address of the PHY.

    Register - Supplies the register to read.

    Value - Supplies the value to write.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_DEVICE_IO_ERROR if the operation timed out.

--*/

{

    USHORT Mii;
    ULONGLONG Timeout;

    Mii = ((Phy & DWE_GMII_ADDRESS_DEVICE_MASK) <<
           DWE_GMII_ADDRESS_DEVICE_SHIFT) |
          ((Register & DWE_GMII_ADDRESS_REGISTER_MASK) <<
           DWE_GMII_ADDRESS_REGISTER_SHIFT) |
          (DWE_MII_CLOCK_VALUE << DWE_GMII_ADDRESS_CLOCK_RANGE_SHIFT) |
          DWE_GMII_ADDRESS_WRITE | DWE_GMII_ADDRESS_BUSY;

    DWE_WRITE(Device, DweRegisterGmiiData, Value);
    DWE_WRITE(Device, DweRegisterGmiiAddress, Mii);
    Timeout = KeGetRecentTimeCounter() +
              (HlQueryTimeCounterFrequency() * DWE_MII_TIMEOUT);

    do {
        Mii = DWE_READ(Device, DweRegisterGmiiAddress);
        if ((Mii & DWE_GMII_ADDRESS_BUSY) == 0) {
            break;
        }

        KeYield();

    } while (KeGetRecentTimeCounter() <= Timeout);

    if ((Mii & DWE_GMII_ADDRESS_BUSY) != 0) {
        return STATUS_DEVICE_IO_ERROR;
    }

    return STATUS_SUCCESS;
}

