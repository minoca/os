/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    am3ethhw.c

Abstract:

    This module implements the actual hardware support for the TI AM335x CPSW
    Ethernet controller.

Author:

    Evan Green 20-Mar-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/net/netdrv.h>
#include <minoca/net/mii.h>
#include "am3eth.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum amount of packets that AM3 Ethernet will keep queued
// before it starts to drop packets.
//

#define A3E_MAX_TRANSMIT_PACKET_LIST_COUNT (A3E_TRANSMIT_DESCRIPTOR_COUNT * 2)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
A3epLinkCheckDpc (
    PDPC Dpc
    );

KSTATUS
A3epInitializePhy (
    PA3E_DEVICE Device,
    ULONG Port
    );

VOID
A3epReadMacAddress (
    PA3E_DEVICE Device
    );

VOID
A3epSendPendingPackets (
    PA3E_DEVICE Device
    );

VOID
A3epReapCompletedTransmitDescriptors (
    PA3E_DEVICE Device
    );

VOID
A3epReapReceivedFrames (
    PA3E_DEVICE Device
    );

KSTATUS
A3epCheckLink (
    PA3E_DEVICE Device
    );

KSTATUS
A3epDetermineLinkParameters (
    PA3E_DEVICE Device,
    PBOOL LinkUp,
    PULONGLONG Speed,
    PBOOL FullDuplex
    );

VOID
A3epUpdateFilterMode (
    PA3E_DEVICE Device
    );

KSTATUS
A3epReadPhy (
    PA3E_DEVICE Device,
    ULONG Phy,
    ULONG Register,
    PULONG Result
    );

KSTATUS
A3epWritePhy (
    PA3E_DEVICE Device,
    ULONG Phy,
    ULONG Register,
    ULONG RegisterValue
    );

KSTATUS
A3epWriteAndWait (
    PA3E_DEVICE Device,
    ULONG Register,
    ULONG Value
    );

VOID
A3epAleSetPortState (
    PA3E_DEVICE Device,
    ULONG Port,
    ULONG State
    );

VOID
A3epConfigurePortToHostVlan (
    PA3E_DEVICE Device,
    ULONG Port,
    UCHAR MacAddress[ETHERNET_ADDRESS_SIZE]
    );

ULONG
A3epAleGetFreeEntry (
    PA3E_DEVICE Device
    );

VOID
A3epAleReadEntry (
    PA3E_DEVICE Device,
    ULONG TableIndex,
    ULONG Entry[A3E_ALE_ENTRY_WORDS]
    );

VOID
A3epAleWriteEntry (
    PA3E_DEVICE Device,
    ULONG TableIndex,
    ULONG Entry[A3E_ALE_ENTRY_WORDS]
    );

//
// -------------------------------------------------------------------- Globals
//

BOOL A3eDisablePacketDropping = FALSE;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
A3eSend (
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

    PA3E_DEVICE Device;
    UINTN PacketListCount;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Device = (PA3E_DEVICE)DeviceContext;
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
    if ((PacketListCount < A3E_MAX_TRANSMIT_PACKET_LIST_COUNT) ||
        (A3eDisablePacketDropping != FALSE)) {

        NET_APPEND_PACKET_LIST(PacketList, &(Device->TransmitPacketList));
        A3epSendPendingPackets(Device);
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
A3eGetSetInformation (
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
    PA3E_DEVICE Device;
    KSTATUS Status;

    Device = (PA3E_DEVICE)DeviceContext;
    switch (InformationType) {
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
        Capabilities = Device->EnabledCapabilities;
        if (*BooleanOption != FALSE) {
            Capabilities |= NET_LINK_CAPABILITY_PROMISCUOUS_MODE;

        } else {
            Capabilities &= ~NET_LINK_CAPABILITY_PROMISCUOUS_MODE;
        }

        if ((Capabilities ^ Device->EnabledCapabilities) != 0) {
            Device->EnabledCapabilities = Capabilities;
            A3epUpdateFilterMode(Device);
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
A3epInitializeDeviceStructures (
    PA3E_DEVICE Device
    )

/*++

Routine Description:

    This routine creates the data structures needed for an AM335x CPSW
    Ethernet controller.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    PA3E_DESCRIPTOR Descriptor;
    ULONG FrameIndex;
    ULONG IoBufferFlags;
    ULONG NextDescriptorPhysical;
    ULONG ReceiveFrameData;
    ULONG ReceiveFrameDataSize;
    ULONG ReceiveSize;
    KSTATUS Status;

    KeInitializeSpinLock(&(Device->InterruptLock));
    NET_INITIALIZE_PACKET_LIST(&(Device->TransmitPacketList));

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

    Device->DataAlignment = MmGetIoBufferAlignment();

    //
    // Allocate the receive buffers. This is allocated as non-write though and
    // cacheable, which means software must be careful when the frame is
    // first received (and do an invalidate), and when setting up the
    // link pointers, but after the receive is complete it's normal memory.
    //

    ReceiveFrameDataSize = ALIGN_RANGE_UP(A3E_RECEIVE_FRAME_DATA_SIZE,
                                          Device->DataAlignment);

    ReceiveSize = ReceiveFrameDataSize * A3E_RECEIVE_FRAME_COUNT;

    ASSERT(Device->ReceiveDataIoBuffer == NULL);

    IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS;
    Device->ReceiveDataIoBuffer = MmAllocateNonPagedIoBuffer(0,
                                                             MAX_ULONG,
                                                             0,
                                                             ReceiveSize,
                                                             IoBufferFlags);

    if (Device->ReceiveDataIoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    ASSERT(Device->ReceiveDataIoBuffer->FragmentCount == 1);
    ASSERT(Device->ReceiveDataIoBuffer->Fragment[0].VirtualAddress != NULL);

    Device->ReceiveFrameDataSize = ReceiveFrameDataSize;

    //
    // There's 8 kilobytes of RAM in there, use it for descriptors.
    //

    Device->TransmitDescriptors = Device->ControllerBase + A3E_CPPI_RAM_OFFSET;
    Device->TransmitDescriptorsPhysical = Device->ControllerBasePhysical +
                                          A3E_CPPI_RAM_OFFSET;

    RtlZeroMemory(Device->TransmitDescriptors, A3E_TRANSMIT_DESCRIPTORS_SIZE);
    Device->ReceiveDescriptors = Device->TransmitDescriptors +
                                 A3E_TRANSMIT_DESCRIPTOR_COUNT;

    Device->ReceiveDescriptorsPhysical = Device->TransmitDescriptorsPhysical +
                                         A3E_TRANSMIT_DESCRIPTORS_SIZE;

    Device->TransmitBegin = 0;
    Device->TransmitEnd = 0;
    Device->ReceiveBegin = 0;

    //
    // Allocate an array of pointers to net packet buffer pointers that runs
    // parallel to the transmit array.
    //

    AllocationSize = sizeof(PNET_PACKET_BUFFER) * A3E_TRANSMIT_DESCRIPTOR_COUNT;
    Device->TransmitPacket = MmAllocateNonPagedPool(AllocationSize,
                                                    A3E_ALLOCATION_TAG);

    if (Device->TransmitPacket == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    RtlZeroMemory(Device->TransmitPacket, AllocationSize);

    //
    // Create the various kernel objects used for synchronization and service.
    //

    ASSERT(Device->WorkItem == NULL);

    Device->WorkItem = KeCreateWorkItem(
                                NULL,
                                WorkPriorityNormal,
                                (PWORK_ITEM_ROUTINE)A3epInterruptServiceWorker,
                                Device,
                                A3E_ALLOCATION_TAG);

    if (Device->WorkItem == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    ASSERT(Device->LinkCheckTimer == NULL);

    Device->LinkCheckTimer = KeCreateTimer(A3E_ALLOCATION_TAG);
    if (Device->LinkCheckTimer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    Device->LinkCheckDpc = KeCreateDpc(A3epLinkCheckDpc, Device);
    if (Device->LinkCheckDpc == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    //
    // Initialize the receive frame list as a train of packets connected to
    // each other, but that do not circle back.
    //

    NextDescriptorPhysical = Device->ReceiveDescriptorsPhysical +
                             sizeof(A3E_DESCRIPTOR);

    ReceiveFrameData =
             (ULONG)(Device->ReceiveDataIoBuffer->Fragment[0].PhysicalAddress);

    for (FrameIndex = 0;
         FrameIndex < A3E_RECEIVE_FRAME_COUNT;
         FrameIndex += 1) {

        Descriptor = &(Device->ReceiveDescriptors[FrameIndex]);
        if (FrameIndex == A3E_RECEIVE_FRAME_COUNT - 1) {
            Descriptor->NextDescriptor = A3E_DESCRIPTOR_NEXT_NULL;

        } else {
            Descriptor->NextDescriptor = NextDescriptorPhysical;
        }

        Descriptor->Buffer = ReceiveFrameData;
        Descriptor->BufferLengthOffset = ReceiveFrameDataSize;
        Descriptor->PacketLengthFlags = A3E_DESCRIPTOR_HARDWARE_OWNED;
        ReceiveFrameData += ReceiveFrameDataSize;
        NextDescriptorPhysical += sizeof(A3E_DESCRIPTOR);
    }

    //
    // Promiscuous mode is supported by not enabled by default.
    //

    Device->SupportedCapabilities |= NET_LINK_CAPABILITY_PROMISCUOUS_MODE;
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
A3epResetDevice (
    PA3E_DEVICE Device
    )

/*++

Routine Description:

    This routine resets the TI CPSW Ethernet device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    UINTN Channel;
    ULONG Divisor;
    ULONGLONG Frequency;
    ULONG Port;
    KSTATUS Status;
    ULONG Value;

    Port = 1;
    Device->PhyId = 0;

    //
    // Read the MAC address before resetting the device to get a MAC address
    // that might have been assigned by the firmware.
    //

    A3epReadMacAddress(Device);

    //
    // Perform software resets of the various submodules.
    //

    Status = A3epWriteAndWait(Device,
                              A3E_SS_OFFSET + A3eSsSoftReset,
                              A3E_SS_SOFT_RESET_SOFT_RESET);

    if (!KSUCCESS(Status)) {
        goto ResetDeviceEnd;
    }

    Status = A3epWriteAndWait(Device,
                              A3E_WR_OFFSET + A3eWrSoftReset,
                              A3E_WR_SOFT_RESET_SOFT_RESET);

    if (!KSUCCESS(Status)) {
        goto ResetDeviceEnd;
    }

    Status = A3epWriteAndWait(Device,
                              A3E_SL1_OFFSET + A3eSlSoftReset,
                              A3E_SL_SOFT_RESET_SOFT_RESET);

    if (!KSUCCESS(Status)) {
        goto ResetDeviceEnd;
    }

    Status = A3epWriteAndWait(Device,
                              A3E_SL2_OFFSET + A3eSlSoftReset,
                              A3E_SL_SOFT_RESET_SOFT_RESET);

    if (!KSUCCESS(Status)) {
        goto ResetDeviceEnd;
    }

    Status = A3epWriteAndWait(Device,
                              A3E_CPDMA_OFFSET + A3eDmaSoftReset,
                              A3E_CPDMA_DMA_SOFT_RESET_SOFT_RESET);

    if (!KSUCCESS(Status)) {
        goto ResetDeviceEnd;
    }

    //
    // Reset all the descriptor pointers.
    //

    for (Channel = 0; Channel < A3E_CPDMA_CHANNEL_COUNT; Channel += 1) {
        A3E_DMA_WRITE(Device,
                      A3E_CPDMA_CHANNEL(A3eDmaTxHeadDescriptorPointer, Channel),
                      0);

        A3E_DMA_WRITE(Device,
                      A3E_CPDMA_CHANNEL(A3eDmaRxHeadDescriptorPointer, Channel),
                      0);

        A3E_DMA_WRITE(Device,
                      A3E_CPDMA_CHANNEL(A3eDmaTxCompletionPointer, Channel),
                      0);

        A3E_DMA_WRITE(Device,
                      A3E_CPDMA_CHANNEL(A3eDmaRxCompletionPointer, Channel),
                      0);
    }

    //
    // Initialize MDIO, including the divisor rate.
    //

    Divisor = (A3E_MDIO_FREQUENCY_INPUT / A3E_MDIO_FREQUENCY_OUTPUT) - 1;
    Value = (Divisor & A3E_MDIO_CONTROL_DIVISOR_MASK) |
            A3E_MDIO_CONTROL_ENABLE |
            A3E_MDIO_CONTROL_PREAMBLE |
            A3E_MDIO_CONTROL_FAULTENB;

    A3E_MDIO_WRITE(Device, A3eMdioControl, Value);
    HlBusySpin(1000);

    //
    // Initialize the Address Lookup Engine.
    //

    Value = A3E_ALE_CONTROL_CLEAR_TABLE | A3E_ALE_CONTROL_ENABLE_ALE;
    A3E_ALE_WRITE(Device, A3eAleControl, Value);
    A3epAleSetPortState(Device, 0, A3E_ALE_PORT_STATE_FORWARD);
    A3epAleSetPortState(Device, 1, A3E_ALE_PORT_STATE_FORWARD);
    A3epAleSetPortState(Device, 2, A3E_ALE_PORT_STATE_FORWARD);

    //
    // Make sure the filter is mode correct, based on the current capabilities.
    //

    A3epUpdateFilterMode(Device);

    //
    // Enter dual MAC mode. To drop any packets that are not VLAN-tagged, set
    // the "VLAN aware bit in the ALE control register.
    //

    Value = A3E_PORT_READ(Device, 0, A3ePortTxInControl);
    Value &= ~A3E_PORT_TX_IN_CONTROL_TX_IN_SELECT;
    Value |= A3E_PORT_TX_IN_CONTROL_TX_IN_DUAL_MAC;
    A3E_PORT_WRITE(Device, 0, A3ePortTxInControl, Value);

    //
    // Enable statistics.
    //

    Value = A3E_SS_STATISTICS_PORT_ENABLE_PORT0_STATISTICS_ENABLE |
            A3E_SS_STATISTICS_PORT_ENABLE_PORT1_STATISTICS_ENABLE |
            A3E_SS_STATISTICS_PORT_ENABLE_PORT2_STATISTICS_ENABLE;

    A3E_SS_WRITE(Device, A3eSsStatisticsPortEnable, Value);

    //
    // Set the head of the receive list for channel 0.
    //

    Value = A3E_RX_DESCRIPTOR(Device, Device->ReceiveBegin);
    A3E_DMA_WRITE(Device,
                  A3E_CPDMA_CHANNEL(A3eDmaRxHeadDescriptorPointer, 0),
                  Value);

    //
    // Set the assigned MAC address.
    //

    Value = Device->MacAddress[4] | (Device->MacAddress[5] << 8);
    A3E_PORT_WRITE(Device, Port, A3ePortSourceAddressLow, Value);
    Value = Device->MacAddress[0] |
            (Device->MacAddress[1] << 8) |
            (Device->MacAddress[2] << 16) |
            (Device->MacAddress[3] << 24);

    A3E_PORT_WRITE(Device, Port, A3ePortSourceAddressHigh, Value);

    //
    // Acknowledge any previous pending interrupts.
    //

    A3E_DMA_WRITE(Device, A3eDmaCpDmaEoiVector, A3E_CPDMA_EOI_TX_PULSE);
    A3E_DMA_WRITE(Device, A3eDmaCpDmaEoiVector, A3E_CPDMA_EOI_RX_PULSE);

    //
    // Enable transmit and receive.
    //

    A3E_DMA_WRITE(Device, A3eDmaTxControl, A3E_CPDMA_TX_CONTROL_ENABLE);
    A3E_DMA_WRITE(Device, A3eDmaRxControl, A3E_CPDMA_RX_CONTROL_ENABLE);

    //
    // Enable interrupts for channel 0 and control core 0.
    //

    A3E_DMA_WRITE(Device, A3eDmaTxInterruptMaskSet, A3E_CPDMA_CHANNEL_MASK(0));
    A3E_WR_WRITE(Device,
                 A3E_WR_CORE(A3eWrCoreTxInterruptEnable, 0),
                 A3E_WR_CHANNEL_MASK(0));

    A3E_DMA_WRITE(Device, A3eDmaRxInterruptMaskSet, A3E_CPDMA_CHANNEL_MASK(0));
    A3E_WR_WRITE(Device,
                 A3E_WR_CORE(A3eWrCoreRxInterruptEnable, 0),
                 A3E_WR_CHANNEL_MASK(0));

    //
    // Configure VLAN, setting one VLAN ID between ports 0 and 1, and a
    // different VLAN ID between ports 0 and 2. Use the port number itself (1)
    // as the VLAN ID.
    //

    Value = Port |
            (0 << A3E_PORT_VLAN_PORT_CFI_SHIFT) |
            (0 << A3E_PORT_VLAN_PORT_PRIORITY_SHIFT);

    A3E_PORT_WRITE(Device, Port, A3ePortPortVlan, Value);
    A3epConfigurePortToHostVlan(Device, Port, Device->MacAddress);

    //
    // Fire up the PHY.
    //

    Status = A3epInitializePhy(Device, Port);
    if (!KSUCCESS(Status)) {
        goto ResetDeviceEnd;
    }

    //
    // Notify the networking core of this new link now that the device is ready
    // to send and receive data, pending media being present.
    //

    if (Device->NetworkLink == NULL) {
        Status = A3epAddNetworkDevice(Device);
        if (!KSUCCESS(Status)) {
            goto ResetDeviceEnd;
        }
    }

    //
    // Determine whether or not there is media connected, and what speed it is.
    //

    Status = A3epCheckLink(Device);
    if (!KSUCCESS(Status)) {
        goto ResetDeviceEnd;
    }

    //
    // Fire up the link check timer.
    //

    Frequency = HlQueryTimeCounterFrequency();
    Device->LinkCheckInterval = Frequency * A3E_LINK_CHECK_INTERVAL;
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
A3epTxInterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the TI CPSW Ethernet transmit interrupt service
    routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the e100 device
        structure.

Return Value:

    Interrupt status.

--*/

{

    ULONG CurrentPointer;
    PA3E_DEVICE Device;
    INTERRUPT_STATUS InterruptStatus;
    RUNLEVEL OldRunLevel;
    ULONG PendingBits;

    Device = (PA3E_DEVICE)Context;
    InterruptStatus = InterruptStatusNotClaimed;

    //
    // Read the status register, and if anything's set add it to the pending
    // bits.
    //

    PendingBits = A3E_DMA_READ(Device, A3eDmaTxInterruptStatusMasked);
    if (PendingBits != 0) {
        InterruptStatus = InterruptStatusClaimed;
        RtlAtomicOr32(&(Device->PendingStatusBits),
                      A3E_PENDING_TRANSMIT_INTERRUPT);

        //
        // Since this interrupt synchronizes with another interrupt, raise to
        // a priority that is the maximum of the two.
        //

        OldRunLevel = KeRaiseRunLevel(Device->InterruptRunLevel);
        KeAcquireSpinLock(&(Device->InterruptLock));

        //
        // The controller demands that the current descriptor pointer is
        // acknowledged before deasserting the interrupt, because they imagine
        // processing the descriptors directly in the ISR. Just read and write
        // back the value to silence it.
        //

        CurrentPointer = A3E_DMA_READ(
                              Device,
                              A3E_CPDMA_CHANNEL(A3eDmaTxCompletionPointer, 0));

        A3E_DMA_WRITE(Device,
                      A3E_CPDMA_CHANNEL(A3eDmaTxCompletionPointer, 0),
                      CurrentPointer);

        //
        // Also write the EOI register.
        //

        A3E_DMA_WRITE(Device, A3eDmaCpDmaEoiVector, A3E_CPDMA_EOI_TX_PULSE);
        KeReleaseSpinLock(&(Device->InterruptLock));
        KeLowerRunLevel(OldRunLevel);
    }

    return InterruptStatus;
}

INTERRUPT_STATUS
A3epRxInterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the TI CPSW Ethernet receive interrupt service
    routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the e100 device
        structure.

Return Value:

    Interrupt status.

--*/

{

    ULONG CurrentPointer;
    PA3E_DEVICE Device;
    INTERRUPT_STATUS InterruptStatus;
    RUNLEVEL OldRunLevel;
    ULONG PendingBits;

    Device = (PA3E_DEVICE)Context;
    InterruptStatus = InterruptStatusNotClaimed;

    //
    // Read the status register, and if anything's set add it to the pending
    // bits.
    //

    PendingBits = A3E_DMA_READ(Device, A3eDmaRxInterruptStatusMasked);
    if (PendingBits != 0) {
        InterruptStatus = InterruptStatusClaimed;
        RtlAtomicOr32(&(Device->PendingStatusBits),
                      A3E_PENDING_RECEIVE_INTERRUPT);

        //
        // Since this interrupt synchronizes with another interrupt, raise to
        // a priority that is the maximum of the two.
        //

        OldRunLevel = KeRaiseRunLevel(Device->InterruptRunLevel);
        KeAcquireSpinLock(&(Device->InterruptLock));

        //
        // The controller demands that the current descriptor pointer is
        // acknowledged before deasserting the interrupt, because they imagine
        // processing the descriptors directly in the ISR. Just read and write
        // back the value to silence it.
        //

        CurrentPointer = A3E_DMA_READ(
                              Device,
                              A3E_CPDMA_CHANNEL(A3eDmaRxCompletionPointer, 0));

        A3E_DMA_WRITE(Device,
                      A3E_CPDMA_CHANNEL(A3eDmaRxCompletionPointer, 0),
                      CurrentPointer);

        //
        // Also write the EOI register.
        //

        A3E_DMA_WRITE(Device, A3eDmaCpDmaEoiVector, A3E_CPDMA_EOI_RX_PULSE);
        KeReleaseSpinLock(&(Device->InterruptLock));
        KeLowerRunLevel(OldRunLevel);
    }

    return InterruptStatus;
}

INTERRUPT_STATUS
A3epInterruptServiceWorker (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine processes interrupts for the TI CPSW Ethernet controller at
    low level.

Arguments:

    Parameter - Supplies an optional parameter passed in by the creator of the
        work item.

Return Value:

    None.

--*/

{

    ULONGLONG CurrentTime;
    PA3E_DEVICE Device;
    ULONG PendingBits;

    Device = (PA3E_DEVICE)(Parameter);

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Clear out the pending bits.
    //

    PendingBits = RtlAtomicExchange32(&(Device->PendingStatusBits), 0);
    if (PendingBits == 0) {
        return InterruptStatusNotClaimed;
    }

    if ((PendingBits & A3E_PENDING_RECEIVE_INTERRUPT) != 0) {
        A3epReapReceivedFrames(Device);
    }

    //
    // If the command unit finished what it was up to, reap that memory.
    //

    if ((PendingBits & A3E_PENDING_TRANSMIT_INTERRUPT) != 0) {
        A3epReapCompletedTransmitDescriptors(Device);
    }

    if ((PendingBits & A3E_PENDING_LINK_CHECK_TIMER) != 0) {
        CurrentTime = KeGetRecentTimeCounter();
        Device->NextLinkCheck = CurrentTime + Device->LinkCheckInterval;
        A3epCheckLink(Device);
    }

    return InterruptStatusClaimed;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
A3epLinkCheckDpc (
    PDPC Dpc
    )

/*++

Routine Description:

    This routine implements the TI CPSW Ethernet DPC that is queued when a
    link check timer expires.

Arguments:

    Dpc - Supplies a pointer to the DPC that is running.

Return Value:

    None.

--*/

{

    PA3E_DEVICE Device;
    ULONG OldPendingBits;
    KSTATUS Status;

    Device = (PA3E_DEVICE)(Dpc->UserData);
    OldPendingBits = RtlAtomicOr32(&(Device->PendingStatusBits),
                                   A3E_PENDING_LINK_CHECK_TIMER);

    if ((OldPendingBits & A3E_PENDING_LINK_CHECK_TIMER) == 0) {
        Status = KeQueueWorkItem(Device->WorkItem);
        if (!KSUCCESS(Status)) {
            RtlAtomicAnd32(&(Device->PendingStatusBits),
                           ~A3E_PENDING_LINK_CHECK_TIMER);
        }
    }

    return;
}

KSTATUS
A3epInitializePhy (
    PA3E_DEVICE Device,
    ULONG Port
    )

/*++

Routine Description:

    This routine initializes the PHY on the TI CPSW Ethernet Controller.

Arguments:

    Device - Supplies a pointer to the device.

    Port - Supplies the port number to use, 1 or 2.

Return Value:

    Status code.

--*/

{

    ULONG Advertise;
    ULONG Alive;
    ULONG BasicControl;
    USHORT GigabitAdvertise;
    KSTATUS Status;
    ULONG Value;

    //
    // If using the second port, then members like the PHY ID need to be
    // duplicated per port.
    //

    ASSERT(Port == 1);

    Alive = A3E_MDIO_READ(Device, A3eMdioAlive);
    if ((Alive & (1 << Device->PhyId)) == 0) {
        RtlDebugPrint("A3E: PHY not alive.\n");
        return STATUS_NOT_READY;
    }

    GigabitAdvertise = 0;
    Status = A3epReadPhy(Device,
                         Device->PhyId,
                         MiiRegisterBasicControl,
                         &BasicControl);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    BasicControl |= MII_BASIC_CONTROL_ENABLE_AUTONEGOTIATION;
    if (Device->GigabitCapable != FALSE) {
        BasicControl |= MII_BASIC_CONTROL_SPEED_1000;
    }

    Status = A3epWritePhy(Device,
                          Device->PhyId,
                          MiiRegisterBasicControl,
                          BasicControl);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    Status = A3epReadPhy(Device,
                         Device->PhyId,
                         MiiRegisterBasicControl,
                         &BasicControl);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Write the autonegotiation capabilities.
    //

    Advertise = MII_ADVERTISE_100_FULL | MII_ADVERTISE_100_HALF |
                MII_ADVERTISE_10_FULL | MII_ADVERTISE_10_HALF;

    Status = A3epWritePhy(Device,
                          Device->PhyId,
                          MiiRegisterAdvertise,
                          Advertise);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Write autonegotiation gigabit capabilities.
    //

    if (Device->GigabitCapable != FALSE) {
        GigabitAdvertise = MII_GIGABIT_CONTROL_ADVERTISE_1000_FULL;
        Status = A3epWritePhy(Device,
                              Device->PhyId,
                              MiiRegisterGigabitControl,
                              GigabitAdvertise);

        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    //
    // Restart autonegotiation.
    //

    BasicControl |= MII_BASIC_CONTROL_RESTART_AUTONEGOTIATION;
    Status = A3epWritePhy(Device,
                          Device->PhyId,
                          MiiRegisterBasicControl,
                          BasicControl);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Set the EDPWRDOWN (energy detect power down) bit in the LAN8710 for
    // better power management (otherwise there are large 15-20mA spikes
    // every 2-3 milliseconds).
    //

    A3epWritePhy(Device,
                 Device->PhyId,
                 PHY_LAN8710_MODE,
                 PHY_LAN8710_MODE_ENERGY_DETECT_POWER_DOWN);

    //
    // Enable RGMII for the sliver.
    //

    Value = A3E_SL1_READ(Device, A3eSlMacControl);
    Value |= A3E_SL_MAC_CONTROL_GMII_ENABLE |
             A3E_SL_MAC_CONTROL_IFCTL_A |
             A3E_SL_MAC_CONTROL_IFCTL_B;

    A3E_SL1_WRITE(Device, A3eSlMacControl, Value);
    return Status;
}

VOID
A3epReadMacAddress (
    PA3E_DEVICE Device
    )

/*++

Routine Description:

    This routine reads the current MAC address out of the TI CPSW Ethernet
    controller.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    UCHAR Address[ETHERNET_ADDRESS_SIZE];
    ULONG AddressHigh;
    ULONG AddressLow;

    if (Device->MacAddressAssigned != FALSE) {
        return;
    }

    //
    // This reads the MAC address in reversed.
    //

    AddressLow = A3E_PORT_READ(Device, 1, A3ePortSourceAddressLow) & 0x0000FFFF;
    Address[4] = AddressLow & 0xFF;
    Address[5] = (AddressLow >> 8) & 0xFF;
    AddressHigh = A3E_PORT_READ(Device, 1, A3ePortSourceAddressHigh);
    Address[0] = AddressHigh & 0xFF;
    Address[1] = (AddressHigh >> 8) & 0xFF;
    Address[2] = (AddressHigh >> 16) & 0xFF;
    Address[3] = (AddressHigh >> 24) & 0xFF;
    if (NetIsEthernetAddressValid(Address) == FALSE) {
        NetCreateEthernetAddress(Device->MacAddress);

    } else {
        RtlCopyMemory(Device->MacAddress, Address, ETHERNET_ADDRESS_SIZE);
    }

    Device->MacAddressAssigned = TRUE;
    return;
}

VOID
A3epSendPendingPackets (
    PA3E_DEVICE Device
    )

/*++

Routine Description:

    This routine sends as many packets as can fit in the hardware descriptor
    buffer. This routine assumes the command list lock is already held.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    ULONG BufferDescriptorAddress;
    UINTN BufferSize;
    PA3E_DESCRIPTOR Descriptor;
    ULONG DescriptorIndex;
    ULONG Flags;
    ULONG HeadDescriptor;
    PNET_PACKET_BUFFER Packet;
    ULONG PacketLength;
    ULONG Port;
    PA3E_DESCRIPTOR PreviousDescriptor;
    ULONG PreviousIndex;

    Port = 1;
    HeadDescriptor = 0;
    while (NET_PACKET_LIST_EMPTY(&(Device->TransmitPacketList)) == FALSE) {
        Packet = LIST_VALUE(Device->TransmitPacketList.Head.Next,
                            NET_PACKET_BUFFER,
                            ListEntry);

        //
        // If the transmit packet array is not NULL, this descriptor is either
        // active or not yet reaped. Wait for more entries.
        //

        DescriptorIndex = Device->TransmitEnd;
        if (Device->TransmitPacket[DescriptorIndex] != NULL) {
            break;
        }

        NET_REMOVE_PACKET_FROM_LIST(Packet, &(Device->TransmitPacketList));

        //
        // If the packet is less than the allowed minimum packet size, then pad
        // it. The buffer should be big enough to handle it and should have
        // already initialized the padding to zero. The hardware adds the 4
        // byte CRC, so do not include that in the padding.
        //

        PacketLength = Packet->FooterOffset - Packet->DataOffset;
        if (PacketLength < (A3E_TRANSMIT_MINIMUM_PACKET_SIZE - sizeof(ULONG))) {

            ASSERT(Packet->BufferSize >= A3E_TRANSMIT_MINIMUM_PACKET_SIZE);

            PacketLength = A3E_TRANSMIT_MINIMUM_PACKET_SIZE - sizeof(ULONG);
        }

        BufferSize = PacketLength + Packet->DataOffset;
        BufferSize = ALIGN_RANGE_UP(BufferSize, Device->DataAlignment);
        MmFlushBufferForDataOut(Packet->Buffer, BufferSize);

        //
        // Success, a free descriptor. Let's fill it out!
        //

        ASSERT((PacketLength & ~A3E_DESCRIPTOR_BUFFER_LENGTH_MASK) == 0);

        Descriptor = &(Device->TransmitDescriptors[DescriptorIndex]);
        Descriptor->NextDescriptor = A3E_DESCRIPTOR_NEXT_NULL;
        Descriptor->Buffer = Packet->BufferPhysicalAddress + Packet->DataOffset;
        Descriptor->BufferLengthOffset = PacketLength;

        ASSERT((PacketLength & ~A3E_DESCRIPTOR_TX_PACKET_LENGTH_MASK) == 0);

        Descriptor->PacketLengthFlags =
                                     PacketLength |
                                     A3E_DESCRIPTOR_START_OF_PACKET |
                                     A3E_DESCRIPTOR_END_OF_PACKET |
                                     A3E_DESCRIPTOR_HARDWARE_OWNED |
                                     A3E_DESCRIPTOR_TX_TO_PORT_ENABLE |
                                     (Port << A3E_DESCRIPTOR_TX_TO_PORT_SHIFT);

        //
        // Calculate the physical address of the descriptor, and set it as the
        // next pointer of the previous descriptor. If this is the first packet
        // being sent, then this is setting the next pointer for a descriptor
        // that was never queued, but it's harmless.
        //

        if (DescriptorIndex == 0) {
            PreviousIndex = A3E_TRANSMIT_DESCRIPTOR_COUNT - 1;

        } else {
            PreviousIndex = DescriptorIndex - 1;
        }

        PreviousDescriptor = &(Device->TransmitDescriptors[PreviousIndex]);
        BufferDescriptorAddress = Device->TransmitDescriptorsPhysical +
                                  (DescriptorIndex * sizeof(A3E_DESCRIPTOR));

        //
        // Use the register write function to ensure the compiler does this in
        // a single write (and not something goofy like byte by byte). This
        // routine also serves as a full memory barrier.
        //

        HlWriteRegister32(&(PreviousDescriptor->NextDescriptor),
                          BufferDescriptorAddress);

        Flags = PreviousDescriptor->PacketLengthFlags;
        if ((Device->TransmitPacket[PreviousIndex] == NULL) ||
            ((Flags & A3E_DESCRIPTOR_END_OF_QUEUE) != 0)) {

            //
            // Clear the end of queue bit so that reaping the previous
            // descriptor does not cause a second reprogramming of the hardware.
            //

            Flags &= ~A3E_DESCRIPTOR_END_OF_QUEUE;
            PreviousDescriptor->PacketLengthFlags = Flags;

            //
            // This condition should only be detected once.
            //

            ASSERT(HeadDescriptor == 0);

            HeadDescriptor = BufferDescriptorAddress;
        }

        Device->TransmitPacket[DescriptorIndex] = Packet;

        //
        // Advance the index.
        //

        DescriptorIndex += 1;
        if (DescriptorIndex == A3E_TRANSMIT_DESCRIPTOR_COUNT) {
            DescriptorIndex = 0;
        }

        Device->TransmitEnd = DescriptorIndex;
    }

    //
    // If the device went idle before or during the queuing of this packet,
    // poke it to start transmission.
    //

    if (HeadDescriptor != 0) {
        A3E_DMA_WRITE(Device,
                      A3E_CPDMA_CHANNEL(A3eDmaTxHeadDescriptorPointer, 0),
                      HeadDescriptor);
    }

    return;
}

VOID
A3epReapCompletedTransmitDescriptors (
    PA3E_DEVICE Device
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

    ULONG Channel;
    PA3E_DESCRIPTOR Descriptor;
    ULONG Flags;
    ULONG HeadDescriptor;
    BOOL PacketReaped;
    ULONG PreviousFlags;
    ULONG ReapIndex;

    PacketReaped = FALSE;
    PreviousFlags = 0;
    HeadDescriptor = 0;
    KeAcquireQueuedLock(Device->TransmitLock);
    ReapIndex = Device->TransmitBegin;
    while (TRUE) {

        //
        // If there is no packet for this index, then the descriptor has
        // already been reaped.
        //

        if (Device->TransmitPacket[ReapIndex] == NULL) {
            break;
        }

        //
        // If the descriptor is still owned by the hardware, then it is not
        // complete. The hardware, however, may have gone idle if the last
        // descriptor marked the end of the queue. Poke the hardware with the
        // current descriptor if necessary.
        //

        Descriptor = &(Device->TransmitDescriptors[ReapIndex]);
        Flags = Descriptor->PacketLengthFlags;
        if ((Flags & A3E_DESCRIPTOR_HARDWARE_OWNED) != 0) {
            if ((PreviousFlags & A3E_DESCRIPTOR_END_OF_QUEUE) != 0) {
                HeadDescriptor = Device->TransmitDescriptorsPhysical +
                                 (ReapIndex * sizeof(A3E_DESCRIPTOR));

                Channel = A3E_CPDMA_CHANNEL(A3eDmaTxHeadDescriptorPointer, 0);
                A3E_DMA_WRITE(Device, Channel, HeadDescriptor);
            }

            break;
        }

        //
        // Free up the packet and mark the descriptor as free for use by
        // zeroing out the control.
        //

        NetFreeBuffer(Device->TransmitPacket[ReapIndex]);
        Device->TransmitPacket[ReapIndex] = NULL;
        PacketReaped = TRUE;
        PreviousFlags = Flags;

        //
        // Move the beginning of the list forward.
        //

        ReapIndex += 1;
        if (ReapIndex == A3E_TRANSMIT_DESCRIPTOR_COUNT) {
            ReapIndex = 0;
        }

        Device->TransmitBegin = ReapIndex;
    }

    //
    // If at least one packet was reaped, attempt to pump more packets through.
    //

    if (PacketReaped != FALSE) {
        A3epSendPendingPackets(Device);
    }

    KeReleaseQueuedLock(Device->TransmitLock);
    return;
}

VOID
A3epReapReceivedFrames (
    PA3E_DEVICE Device
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
    PA3E_DESCRIPTOR Descriptor;
    ULONG DescriptorPhysical;
    ULONG Flags;
    NET_PACKET_BUFFER Packet;
    ULONG PacketSize;
    PA3E_DESCRIPTOR PreviousDescriptor;
    ULONG PreviousIndex;
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

        Flags = Descriptor->PacketLengthFlags;
        if ((Flags & A3E_DESCRIPTOR_HARDWARE_OWNED) != 0) {
            break;
        }

        //
        // If the frame came through alright, send it up to the core networking
        // library to process.
        //

        if ((Flags & A3E_DESCRIPTOR_RX_PACKET_ERROR_MASK) == 0) {
            Packet.IoBuffer = NULL;
            Packet.Buffer = ReceiveVirtual +
                            (Begin * Device->ReceiveFrameDataSize);

            Packet.BufferPhysicalAddress =
                      ReceivePhysical + (Begin * Device->ReceiveFrameDataSize);

            ASSERT(((Flags & A3E_DESCRIPTOR_START_OF_PACKET) != 0) &&
                   ((Flags & A3E_DESCRIPTOR_END_OF_PACKET) != 0));

            Packet.BufferSize = Device->ReceiveFrameDataSize;
            Packet.DataSize = Descriptor->BufferLengthOffset &
                              A3E_DESCRIPTOR_BUFFER_LENGTH_MASK;

            Packet.DataOffset = Descriptor->BufferLengthOffset >>
                                A3E_DESCRIPTOR_BUFFER_OFFSET_SHIFT;

            Packet.FooterOffset = Packet.DataSize;
            Packet.Flags = 0;
            PacketSize = Packet.DataSize;
            PacketSize = ALIGN_RANGE_UP(PacketSize, Device->DataAlignment);
            MmFlushBufferForDataIn(Packet.Buffer, PacketSize);
            NetProcessReceivedPacket(Device->NetworkLink, &Packet);

        } else {
            RtlDebugPrint("A3E: RX Error 0x%04x\n", Flags);
        }

        //
        // Set this frame up to be reused, it will be the new end of the list.
        //

        Descriptor->NextDescriptor = A3E_DESCRIPTOR_NEXT_NULL;
        Descriptor->BufferLengthOffset = Device->ReceiveFrameDataSize;
        Descriptor->PacketLengthFlags = A3E_DESCRIPTOR_HARDWARE_OWNED;
        if (Begin == 0) {
            PreviousIndex = A3E_RECEIVE_FRAME_COUNT - 1;

        } else {
            PreviousIndex = Begin - 1;
        }

        //
        // Set the next pointer first, then if the hardware idled out first,
        // restart it.
        //

        PreviousDescriptor = &(Device->ReceiveDescriptors[PreviousIndex]);
        DescriptorPhysical = Device->ReceiveDescriptorsPhysical +
                             (Begin * sizeof(A3E_DESCRIPTOR));

        PreviousDescriptor->NextDescriptor = DescriptorPhysical;
        if ((PreviousDescriptor->PacketLengthFlags &
             A3E_DESCRIPTOR_END_OF_QUEUE) != 0) {

            A3E_DMA_WRITE(Device,
                          A3E_CPDMA_CHANNEL(A3eDmaRxHeadDescriptorPointer, 0),
                          DescriptorPhysical);
        }

        //
        // Move the beginning pointer up.
        //

        Begin += 1;
        if (Begin == A3E_RECEIVE_FRAME_COUNT) {
            Begin = 0;
        }

        Device->ReceiveBegin = Begin;
    }

    KeReleaseQueuedLock(Device->ReceiveLock);
    return;
}

KSTATUS
A3epCheckLink (
    PA3E_DEVICE Device
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

    Status = A3epDetermineLinkParameters(Device, &LinkUp, &Speed, &FullDuplex);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    if ((Device->LinkActive != LinkUp) ||
        (Device->LinkSpeed != Speed) ||
        (Device->FullDuplex != FullDuplex)) {

        //
        // A todo if you will: If port 2 is fired up, this macro will need to
        // switch between 1/2.
        //

        Value = A3E_SL1_READ(Device, A3eSlMacControl);
        Value &= ~(A3E_SL_MAC_CONTROL_GIGABIT |
                   A3E_SL_MAC_CONTROL_FULL_DUPLEX |
                   A3E_SL_MAC_CONTROL_EXT_IN);

        if (Speed == NET_SPEED_1000_MBPS) {
            Value |= A3E_SL_MAC_CONTROL_GIGABIT;

        } else if (Speed == NET_SPEED_10_MBPS) {
            Value |= A3E_SL_MAC_CONTROL_EXT_IN;
        }

        if (FullDuplex != FALSE) {
            Value |= A3E_SL_MAC_CONTROL_FULL_DUPLEX;
        }

        A3E_SL1_WRITE(Device, A3eSlMacControl, Value);
        Device->LinkActive = LinkUp;
        Device->LinkSpeed = Speed;
        Device->FullDuplex = FullDuplex;
        NetSetLinkState(Device->NetworkLink, LinkUp, Speed);
    }

    return STATUS_SUCCESS;
}

KSTATUS
A3epDetermineLinkParameters (
    PA3E_DEVICE Device,
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
    ULONG Mode;
    ULONG PartnerAbility;
    KSTATUS Status;

    *LinkUp = FALSE;
    *Speed = NET_SPEED_NONE;
    *FullDuplex = FALSE;

    //
    // The energy power down mode is a little flaky. If there is no link,
    // disable and re-enable it, which will kick it into detecting a link.
    //

    if (Device->LinkActive == FALSE) {
        Status = A3epReadPhy(Device,
                             Device->PhyId,
                             PHY_LAN8710_MODE,
                             &Mode);

        if (!KSUCCESS(Status)) {
            goto DetermineLinkParametersEnd;
        }

        Mode &= ~PHY_LAN8710_MODE_ENERGY_DETECT_POWER_DOWN;
        A3epWritePhy(Device, Device->PhyId, PHY_LAN8710_MODE, Mode);
        KeDelayExecution(FALSE, FALSE, 64 * MICROSECONDS_PER_MILLISECOND);
        Mode |= PHY_LAN8710_MODE_ENERGY_DETECT_POWER_DOWN;
        A3epWritePhy(Device, Device->PhyId, PHY_LAN8710_MODE, Mode);
    }

    HasGigabit = Device->GigabitCapable;
    Status = A3epReadPhy(Device,
                         Device->PhyId,
                         MiiRegisterBasicStatus,
                         &BasicStatus);

    if (!KSUCCESS(Status)) {
        goto DetermineLinkParametersEnd;
    }

    Status = A3epReadPhy(Device,
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

    Status = A3epReadPhy(Device,
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

        Status = A3epReadPhy(Device,
                             Device->PhyId,
                             MiiRegisterAdvertise,
                             &CommonLink);

        if (!KSUCCESS(Status)) {
            goto DetermineLinkParametersEnd;
        }

        Status = A3epReadPhy(Device,
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
            Status = A3epReadPhy(Device,
                                 Device->PhyId,
                                 MiiRegisterGigabitStatus,
                                 &GigabitStatus);

            if (!KSUCCESS(Status)) {
                goto DetermineLinkParametersEnd;
            }

            Status = A3epReadPhy(Device,
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
A3epUpdateFilterMode (
    PA3E_DEVICE Device
    )

/*++

Routine Description:

    This routine updates an device's filter mode based on the currently enabled
    capabilities.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    ULONG Value;

    Value = A3E_ALE_READ(Device, A3eAleControl);
    if ((Device->EnabledCapabilities &
         NET_LINK_CAPABILITY_PROMISCUOUS_MODE) != 0) {

        Value |= A3E_ALE_CONTROL_BYPASS;

    } else {
        Value &= ~A3E_ALE_CONTROL_BYPASS;
    }

    A3E_ALE_WRITE(Device, A3eAleControl, Value);
    return;
}

KSTATUS
A3epReadPhy (
    PA3E_DEVICE Device,
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

    ULONGLONG Timeout;
    ULONG Value;

    Timeout = KeGetRecentTimeCounter() +
              (HlQueryTimeCounterFrequency() * A3E_PHY_TIMEOUT);

    //
    // Wait for any previous activity to finish.
    //

    do {
        Value = A3E_MDIO_READ(Device, A3eMdioUserAccess0);
        if ((Value & A3E_MDIO_USERACCESS0_GO) == 0) {
            break;
        }

    } while (KeGetRecentTimeCounter() <= Timeout);

    if ((Value & A3E_MDIO_USERACCESS0_GO) != 0) {
        return STATUS_DEVICE_IO_ERROR;
    }

    //
    // Write the request.
    //

    Value = ((Register & A3E_PHY_REGISTER_MASK) << A3E_PHY_REGISTER_SHIFT) |
            ((Phy & A3E_PHY_ADDRESS_MASK) << A3E_PHY_ADDRESS_SHIFT) |
            A3E_MDIO_USERACCESS0_READ | A3E_MDIO_USERACCESS0_GO;

    A3E_MDIO_WRITE(Device, A3eMdioUserAccess0, Value);

    //
    // Wait for the command to complete.
    //

    do {
        Value = A3E_MDIO_READ(Device, A3eMdioUserAccess0);
        if ((Value & A3E_MDIO_USERACCESS0_GO) == 0) {
            break;
        }

    } while (KeGetRecentTimeCounter() <= Timeout);

    if ((Value & A3E_MDIO_USERACCESS0_GO) != 0) {
        return STATUS_DEVICE_IO_ERROR;
    }

    if ((Value & A3E_MDIO_USERACCESS0_ACK) == 0) {
        return STATUS_DEVICE_IO_ERROR;
    }

    *Result = Value & A3E_PHY_DATA_MASK;
    return STATUS_SUCCESS;
}

KSTATUS
A3epWritePhy (
    PA3E_DEVICE Device,
    ULONG Phy,
    ULONG Register,
    ULONG RegisterValue
    )

/*++

Routine Description:

    This routine writes a register to the PHY.

Arguments:

    Device - Supplies a pointer to the device.

    Phy - Supplies the address of the PHY.

    Register - Supplies the register to read.

    RegisterValue - Supplies the value to write.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_DEVICE_IO_ERROR if the operation timed out.

--*/

{

    ULONGLONG Timeout;
    ULONG Value;

    Timeout = KeGetRecentTimeCounter() +
              (HlQueryTimeCounterFrequency() * A3E_PHY_TIMEOUT);

    //
    // Wait for any previous activity to finish.
    //

    do {
        Value = A3E_MDIO_READ(Device, A3eMdioUserAccess0);
        if ((Value & A3E_MDIO_USERACCESS0_GO) == 0) {
            break;
        }

    } while (KeGetRecentTimeCounter() <= Timeout);

    if ((Value & A3E_MDIO_USERACCESS0_GO) != 0) {
        return STATUS_DEVICE_IO_ERROR;
    }

    //
    // Write the request.
    //

    Value = ((Register & A3E_PHY_REGISTER_MASK) << A3E_PHY_REGISTER_SHIFT) |
            ((Phy & A3E_PHY_ADDRESS_MASK) << A3E_PHY_ADDRESS_SHIFT) |
            A3E_MDIO_USERACCESS0_WRITE | A3E_MDIO_USERACCESS0_GO |
            RegisterValue;

    A3E_MDIO_WRITE(Device, A3eMdioUserAccess0, Value);

    //
    // Wait for the command to complete.
    //

    do {
        Value = A3E_MDIO_READ(Device, A3eMdioUserAccess0);
        if ((Value & A3E_MDIO_USERACCESS0_GO) == 0) {
            break;
        }

    } while (KeGetRecentTimeCounter() <= Timeout);

    if ((Value & A3E_MDIO_USERACCESS0_GO) != 0) {
        return STATUS_DEVICE_IO_ERROR;
    }

    return STATUS_SUCCESS;
}

KSTATUS
A3epWriteAndWait (
    PA3E_DEVICE Device,
    ULONG Register,
    ULONG Value
    )

/*++

Routine Description:

    This routine writes the given value to a register, and then waits for the
    bits written to clear. It's used by soft reset of the different modules.

Arguments:

    Device - Supplies a pointer to the device.

    Register - Supplies the register to write and poll.

    Value - Supplies the value to write.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_TIMEOUT if the operation timed out.

--*/

{

    ULONGLONG Frequency;
    ULONG NewValue;
    ULONGLONG Timeout;

    Frequency = HlQueryTimeCounterFrequency();
    Timeout = KeGetRecentTimeCounter() + Frequency;
    A3E_WRITE(Device, Register, Value);
    do {
        NewValue = A3E_READ(Device, Register);
        if ((NewValue & Value) == 0) {
            break;
        }

        KeYield();

    } while (KeGetRecentTimeCounter() <= Timeout);

    if ((NewValue & Value) != 0) {
        RtlDebugPrint("A3E: Cannot reset device.\n");
        return STATUS_TIMEOUT;
    }

    return STATUS_SUCCESS;
}

VOID
A3epAleSetPortState (
    PA3E_DEVICE Device,
    ULONG Port,
    ULONG State
    )

/*++

Routine Description:

    This routine sets the port state for the given Address Lookup Engine type.

Arguments:

    Device - Supplies a pointer to the device.

    Port - Supplies the port index, 0 through 5.

    State - Supplies the state to set. See A3E_ALE_PORT_STATE_* definitions.

Return Value:

    None.

--*/

{

    ULONG Register;
    ULONG Value;

    Register = A3E_ALE_PORT_CONTROL(Port);
    Value = A3E_ALE_READ(Device, Register);
    Value &= ~A3E_ALE_PORT_CONTROL_STATE_MASK;
    Value |= State;
    A3E_ALE_WRITE(Device, Register, Value);
    return;
}

VOID
A3epConfigurePortToHostVlan (
    PA3E_DEVICE Device,
    ULONG Port,
    UCHAR MacAddress[ETHERNET_ADDRESS_SIZE]
    )

/*++

Routine Description:

    This routine configures the VLAN and VLAN/Unicast entries in the Address
    Lookup Engine for Dual MAC mode.

Arguments:

    Device - Supplies a pointer to the device.

    Port - Supplies the port number to set up. Valid values are 1 and 2.

    MacAddress - Supplies the MAC address to use.

Return Value:

    None.

--*/

{

    ULONG AleIndex;
    ULONG AleUcastEntry[A3E_ALE_ENTRY_WORDS];
    ULONG AleVlanEntry[A3E_ALE_ENTRY_WORDS];
    PUCHAR Bytes;
    ULONG Index;

    RtlZeroMemory(AleUcastEntry, sizeof(AleUcastEntry));
    RtlZeroMemory(AleVlanEntry, sizeof(AleVlanEntry));
    AleIndex = A3epAleGetFreeEntry(Device);
    if (AleIndex == A3E_MAX_ALE_ENTRIES) {
        return;
    }

    Bytes = (PUCHAR)AleVlanEntry;
    Bytes[A3E_ALE_VLAN_ENTRY_MEMBER_LIST_INDEX] =
                                A3E_HOST_PORT_MASK | A3E_SLAVE_PORT_MASK(Port);

    Bytes[A3E_ALE_VLAN_ENTRY_ID_BIT0_BIT7_INDEX] = Port;
    Bytes[A3E_ALE_VLAN_ENTRY_TYPE_ID_BIT8_BIT11_INDEX] =
                                                       A3E_ALE_ENTRY_TYPE_VLAN;

    Bytes[A3E_ALE_VLAN_ENTRY_FRC_UNTAG_EGR_INDEX] =
                                A3E_HOST_PORT_MASK | A3E_SLAVE_PORT_MASK(Port);

    A3epAleWriteEntry(Device, AleIndex, AleVlanEntry);

    //
    // Set up the VLAN/unicast entry.
    //

    AleIndex = A3epAleGetFreeEntry(Device);
    if (AleIndex == A3E_MAX_ALE_ENTRIES) {
        return;
    }

    Bytes = (PUCHAR)AleUcastEntry;
    for (Index = 0; Index < ETHERNET_ADDRESS_SIZE; Index += 1) {
        Bytes[Index] = MacAddress[ETHERNET_ADDRESS_SIZE - Index - 1];
    }

    Bytes[A3E_ALE_VLANUCAST_ENTRY_ID_BIT0_BIT7_INDEX] = Port;
    Bytes[A3E_ALE_VLANUCAST_ENTRY_TYPE_ID_BIT8_BIT11_INDEX] =
                                                  A3E_ALE_ENTRY_TYPE_VLANUCAST;

    A3epAleWriteEntry(Device, AleIndex, AleVlanEntry);
    return;
}

ULONG
A3epAleGetFreeEntry (
    PA3E_DEVICE Device
    )

/*++

Routine Description:

    This routine attempts to find a free entry in the Address Lookup Engine
    table.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Returns the index of a free entry on success.

    A3E_MAX_ALE_ENTRIES if all entries are occupied.

--*/

{

    ULONG AleEntry[A3E_ALE_ENTRY_WORDS];
    PUCHAR Bytes;
    ULONG Index;

    for (Index = 0; Index < A3E_MAX_ALE_ENTRIES; Index += 1) {
        A3epAleReadEntry(Device, Index, AleEntry);
        Bytes = (PUCHAR)AleEntry;
        if ((Bytes[A3E_ALE_ENTRY_TYPE_INDEX] & A3E_ALE_ENTRY_TYPE_MASK) ==
            A3E_ALE_ENTRY_TYPE_FREE) {

            return Index;
        }
    }

    return Index;
}

VOID
A3epAleReadEntry (
    PA3E_DEVICE Device,
    ULONG TableIndex,
    ULONG Entry[A3E_ALE_ENTRY_WORDS]
    )

/*++

Routine Description:

    This routine reads an Address Lookup Engine entry.

Arguments:

    Device - Supplies a pointer to the device.

    TableIndex - Supplies the index of the entry to look up.

    Entry - Supplies the array where the entry will be returned.

Return Value:

    None.

--*/

{

    UINTN WordIndex;

    A3E_ALE_WRITE(Device, A3eAleTableControl, TableIndex);
    for (WordIndex = 0; WordIndex < A3E_ALE_ENTRY_WORDS; WordIndex += 1) {
        Entry[WordIndex] = A3E_ALE_READ(Device, A3E_ALE_TABLE(WordIndex));
    }

    return;
}

VOID
A3epAleWriteEntry (
    PA3E_DEVICE Device,
    ULONG TableIndex,
    ULONG Entry[A3E_ALE_ENTRY_WORDS]
    )

/*++

Routine Description:

    This routine writes an Address Lookup Engine entry.

Arguments:

    Device - Supplies a pointer to the device.

    TableIndex - Supplies the index of the entry to set.

    Entry - Supplies the entry value to set.

Return Value:

    None.

--*/

{

    UINTN WordIndex;

    for (WordIndex = 0; WordIndex < A3E_ALE_ENTRY_WORDS; WordIndex += 1) {
        A3E_ALE_WRITE(Device, A3E_ALE_TABLE(WordIndex), Entry[WordIndex]);
    }

    A3E_ALE_WRITE(Device,
                  A3eAleTableControl,
                  TableIndex | A3E_ALE_TABLE_CONTROL_WRITE);

    return;
}

