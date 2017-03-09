/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    e1000hw.c

Abstract:

    This module implements the portion of the e1000 driver that actually
    interacts with the hardware.

Author:

    Evan Green 8-Nov-2016

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/net/netdrv.h>
#include "e1000.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _E1000_PHY_ENTRY {
    ULONG PhyId;
    E1000_PHY_TYPE PhyType;
} E1000_PHY_ENTRY, *PE1000_PHY_ENTRY;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
E1000pSetupCopperLink (
    PE1000_DEVICE Device
    );

KSTATUS
E1000pSetupSerdesLink (
    PE1000_DEVICE Device
    );

VOID
E1000pCheckLink (
    PE1000_DEVICE Device
    );

VOID
E1000pResetPhyHardware (
    PE1000_DEVICE Device
    );

KSTATUS
E1000pDetectPhy (
    PE1000_DEVICE Device
    );

KSTATUS
E1000pReadPhy (
    PE1000_DEVICE Device,
    ULONG Address,
    PUSHORT Data
    );

KSTATUS
E1000pWritePhy (
    PE1000_DEVICE Device,
    ULONG Address,
    USHORT Data
    );

KSTATUS
E1000pPerformPhyIo (
    PE1000_DEVICE Device,
    ULONG Address,
    PUSHORT Data,
    BOOL Write
    );

VOID
E1000pMdiShiftOut (
    PE1000_DEVICE Device,
    ULONG Data,
    ULONG BitCount
    );

USHORT
E1000pMdiShiftIn (
    PE1000_DEVICE Device
    );

KSTATUS
E1000pReadDeviceMacAddress (
    PE1000_DEVICE Device
    );

KSTATUS
E1000pDetermineEepromCharacteristics (
    PE1000_DEVICE Device
    );

KSTATUS
E1000pReadEeprom (
    PE1000_DEVICE Device,
    USHORT RegisterOffset,
    ULONG WordCount,
    PUSHORT Value
    );

KSTATUS
E1000pEepromAcquire (
    PE1000_DEVICE Device
    );

VOID
E1000pEepromRelease (
    PE1000_DEVICE Device
    );

VOID
E1000pEepromStandby (
    PE1000_DEVICE Device
    );

BOOL
E1000pEepromSpiReady (
    PE1000_DEVICE Device
    );

VOID
E1000pEepromShiftOut (
    PE1000_DEVICE Device,
    USHORT Value,
    USHORT BitCount
    );

USHORT
E1000pEepromShiftIn (
    PE1000_DEVICE Device,
    USHORT BitCount
    );

VOID
E1000pDetermineMediaType (
    PE1000_DEVICE Device
    );

VOID
E1000pSetReceiveAddress (
    PE1000_DEVICE Device,
    PUCHAR Address,
    ULONG Index
    );

KSTATUS
E1000pFillRxDescriptors (
    PE1000_DEVICE Device
    );

VOID
E1000pReapTxDescriptors (
    PE1000_DEVICE Device
    );

VOID
E1000pReapReceivedFrames (
    PE1000_DEVICE Device
    );

VOID
E1000pSendPendingPackets (
    PE1000_DEVICE Device
    );

VOID
E1000pUpdateFilterMode (
    PE1000_DEVICE Device
    );

//
// -------------------------------------------------------------------- Globals
//

BOOL E1000DisablePacketDropping = FALSE;

E1000_PHY_ENTRY E1000PhyEntries[] = {
    {0x01410C30, E1000PhyM88},
    {0x01410C50, E1000PhyM88},
    {0x01410CC0, E1000PhyM88},
    {0x01410C20, E1000PhyM88},
    {0x02A80380, E1000PhyIgp2},
    {0x01410CA0, E1000PhyGg82563},
    {0x02A80390, E1000PhyIgp3},
    {0x02A80330, E1000PhyIfe},
    {0x02A80320, E1000PhyIfe},
    {0x02A80310, E1000PhyIfe},
    {0x01410CB0, E1000PhyBm},
    {0x01410CB1, E1000PhyBm},
    {0x004DD040, E1000Phy82578},
    {0x01540050, E1000Phy82577},
    {0x01540090, E1000Phy82579},
    {0x015400A0, E1000PhyI217},
    {0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
E1000Send (
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

    PE1000_DEVICE Device;
    UINTN PacketListCount;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Device = (PE1000_DEVICE)DeviceContext;
    KeAcquireQueuedLock(Device->TxListLock);
    if (Device->LinkSpeed == 0) {
        Status = STATUS_NO_NETWORK_CONNECTION;
        goto SendEnd;
    }

    //
    // If there is any room in the packet list (or dropping packets is
    // disabled), add all of the packets to the list waiting to be sent.
    //

    PacketListCount = Device->TxPacketList.Count;
    if ((PacketListCount < E1000_MAX_TRANSMIT_PACKET_LIST_COUNT) ||
        (E1000DisablePacketDropping != FALSE)) {

        NET_APPEND_PACKET_LIST(PacketList, &(Device->TxPacketList));
        E1000pSendPendingPackets(Device);
        Status = STATUS_SUCCESS;

    //
    // Otherwise report that the resource is use as it is too busy to handle
    // more packets.
    //

    } else {
        Status = STATUS_RESOURCE_IN_USE;
    }

SendEnd:
    KeReleaseQueuedLock(Device->TxListLock);
    return Status;
}

KSTATUS
E1000GetSetInformation (
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
    PE1000_DEVICE Device;
    PULONG Flags;
    KSTATUS Status;

    Device = (PE1000_DEVICE)DeviceContext;
    switch (InformationType) {
    case NetLinkInformationChecksumOffload:
        if (*DataSize != sizeof(ULONG)) {
            return STATUS_INVALID_PARAMETER;
        }

        if (Set != FALSE) {
            return STATUS_NOT_SUPPORTED;
        }

        Flags = (PULONG)Data;
        *Flags = Device->EnabledCapabilities &
                 NET_LINK_CAPABILITY_CHECKSUM_MASK;

        Status = STATUS_SUCCESS;
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
        Capabilities = Device->EnabledCapabilities;
        if (*BooleanOption != FALSE) {
            Capabilities |= NET_LINK_CAPABILITY_PROMISCUOUS_MODE;

        } else {
            Capabilities &= ~NET_LINK_CAPABILITY_PROMISCUOUS_MODE;
        }

        if ((Capabilities ^ Device->EnabledCapabilities) != 0) {
            Device->EnabledCapabilities = Capabilities;
            E1000pUpdateFilterMode(Device);
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
E1000pInitializeDeviceStructures (
    PE1000_DEVICE Device
    )

/*++

Routine Description:

    This routine performs housekeeping preparation for resetting and enabling
    an E1000 device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    ULONG Capabilities;
    ULONG ReceiveSize;
    KSTATUS Status;
    ULONG TxDescriptorSize;

    //
    // IP, UDP, and TCP checksum offloading are enabled by default.
    //

    Capabilities = NET_LINK_CAPABILITY_RECEIVE_IP_CHECKSUM_OFFLOAD |
                   NET_LINK_CAPABILITY_RECEIVE_TCP_CHECKSUM_OFFLOAD |
                   NET_LINK_CAPABILITY_RECEIVE_UDP_CHECKSUM_OFFLOAD;

    Device->SupportedCapabilities |= Capabilities;
    Device->EnabledCapabilities |= Capabilities;

    //
    // Promiscuous filtering mode is supported, but not enabled by default.
    //

    Device->SupportedCapabilities |= NET_LINK_CAPABILITY_PROMISCUOUS_MODE;

    //
    // Initialize the transmit and receive list locks.
    //

    Device->TxListLock = KeCreateQueuedLock();
    if (Device->TxListLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    Device->RxListLock = KeCreateQueuedLock();
    if (Device->RxListLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    Device->ConfigurationLock = KeCreateQueuedLock();
    if (Device->ConfigurationLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    //
    // Allocate the receive buffers, including space for the descriptors and
    // space for the data.
    //

    ASSERT(MmPageSize() >= sizeof(E1000_RX_DESCRIPTOR) * E1000_RX_RING_SIZE);

    ReceiveSize = sizeof(E1000_RX_DESCRIPTOR) * E1000_RX_RING_SIZE;

    ASSERT(Device->RxIoBuffer == NULL);

    Device->RxIoBuffer = MmAllocateNonPagedIoBuffer(0,
                                                    MAX_ULONG,
                                                    16,
                                                    ReceiveSize,
                                                    0);

    if (Device->RxIoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    ASSERT(Device->RxIoBuffer->Fragment[0].VirtualAddress != NULL);

    Device->RxDescriptors = Device->RxIoBuffer->Fragment[0].VirtualAddress;
    Device->RxListBegin = 0;

    //
    // Allocate the transmit descriptors (which don't include the data to
    // transmit).
    //

    TxDescriptorSize = sizeof(E1000_TX_DESCRIPTOR) * E1000_TX_RING_SIZE;

    ASSERT(MmPageSize() >= TxDescriptorSize);
    ASSERT(Device->TxIoBuffer == NULL);

    Device->TxIoBuffer = MmAllocateNonPagedIoBuffer(0,
                                                    MAX_ULONG,
                                                    16,
                                                    TxDescriptorSize,
                                                    0);

    if (Device->TxIoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    ASSERT(Device->TxIoBuffer->FragmentCount == 1);
    ASSERT(Device->TxIoBuffer->Fragment[0].VirtualAddress != NULL);

    Device->TxDescriptors = Device->TxIoBuffer->Fragment[0].VirtualAddress;
    Device->TxNextReap = 0;
    Device->TxNextToUse = 0;
    RtlZeroMemory(Device->TxDescriptors, TxDescriptorSize);
    NET_INITIALIZE_PACKET_LIST(&(Device->TxPacketList));

    //
    // Allocate an array of pointers to net packet buffers that runs parallel
    // to the transmit and receive arrays.
    //

    AllocationSize = sizeof(PNET_PACKET_BUFFER) *
                     (E1000_TX_RING_SIZE + E1000_RX_RING_SIZE);

    Device->TxPacket = MmAllocatePagedPool(AllocationSize,
                                           E1000_ALLOCATION_TAG);

    if (Device->TxPacket == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    RtlZeroMemory(Device->TxPacket, AllocationSize);
    Device->RxPackets = Device->TxPacket + E1000_TX_RING_SIZE;

    //
    // Initialize the receive frame list.
    //

    RtlZeroMemory(Device->RxDescriptors,
                  sizeof(E1000_RX_DESCRIPTOR) * E1000_RX_RING_SIZE);

    //
    // Disable all interrupts.
    //

    E1000_WRITE(Device, E1000InterruptMaskClear, 0xFFFFFFFF);
    Status = STATUS_SUCCESS;

InitializeDeviceStructuresEnd:
    if (!KSUCCESS(Status)) {
        if (Device->TxListLock != NULL) {
            KeDestroyQueuedLock(Device->TxListLock);
            Device->TxListLock = NULL;
        }

        if (Device->RxListLock != NULL) {
            KeDestroyQueuedLock(Device->RxListLock);
            Device->RxListLock = NULL;
        }

        if (Device->ConfigurationLock != NULL) {
            KeDestroyQueuedLock(Device->ConfigurationLock);
            Device->ConfigurationLock = NULL;
        }

        if (Device->RxIoBuffer != NULL) {
            MmFreeIoBuffer(Device->RxIoBuffer);
            Device->RxIoBuffer = NULL;
            Device->RxDescriptors = NULL;
        }

        if (Device->TxIoBuffer != NULL) {
            MmFreeIoBuffer(Device->TxIoBuffer);
            Device->TxIoBuffer = NULL;
            Device->TxDescriptors = NULL;
        }

        if (Device->TxPacket != NULL) {
            MmFreePagedPool(Device->TxPacket);
            Device->TxPacket = NULL;
        }
    }

    return Status;
}

KSTATUS
E1000pResetDevice (
    PE1000_DEVICE Device
    )

/*++

Routine Description:

    This routine resets the E1000 device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    ULONG Control;
    ULONG ExtendedControl;
    ULONG Index;
    ULONG Management;
    UCHAR NullAddress[ETHERNET_ADDRESS_SIZE];
    ULONG RxChecksumControl;
    ULONG RxControl;
    KSTATUS Status;
    ULONG TxControl;

    E1000_WRITE(Device, E1000InterruptMaskClear, 0xFFFFFFFF);

    //
    // Destroy any old packets lying around.
    //

    for (Index = 0; Index < E1000_TX_RING_SIZE; Index += 1) {
        if (Device->TxPacket[Index] != NULL) {
            NetFreeBuffer(Device->TxPacket[Index]);
            Device->TxPacket[Index] = NULL;
        }
    }

    Status = E1000pDetermineEepromCharacteristics(Device);
    if (!KSUCCESS(Status)) {
        goto ResetDeviceEnd;
    }

    E1000pDetermineMediaType(Device);

    //
    // Perform a complete device reset. Start by disabling interrupts.
    //

    E1000_WRITE(Device, E1000RxControl, 0);
    E1000_WRITE(Device, E1000TxControl, E1000_TX_CONTROL_PAD_SHORT_PACKETS);
    HlBusySpin(10000);
    Control = E1000_READ(Device, E1000DeviceControl);
    E1000_WRITE(Device,
                E1000DeviceControl,
                Control | E1000_DEVICE_CONTROL_RESET);

    HlBusySpin(20000);
    switch (Device->MacType) {
    case E1000Mac82543:
        ExtendedControl = E1000_READ(Device, E1000ExtendedDeviceControl);
        ExtendedControl |= E1000_EXTENDED_CONTROL_EEPROM_RESET;
        E1000_WRITE(Device, E1000ExtendedDeviceControl, ExtendedControl);
        HlBusySpin(2000);
        break;

    default:
        break;
    }

    //
    // Let the firmware know the driver is loaded.
    //

    ExtendedControl = E1000_READ(Device, E1000ExtendedDeviceControl);
    ExtendedControl |= E1000_EXTENDED_CONTROL_DRIVER_LOADED;
    E1000_WRITE(Device, E1000ExtendedDeviceControl, ExtendedControl);
    Management = E1000_READ(Device, E1000ManagementControl);
    Management &= ~E1000_MANAGEMENT_ARP_REQUEST_FILTERING;
    E1000_WRITE(Device, E1000ManagementControl, Management);

    //
    // Mask off and remove all interrupts again as requested by the spec.
    //

    E1000_WRITE(Device, E1000InterruptMaskClear, 0xFFFFFFFF);
    E1000_READ(Device, E1000InterruptCauseRead);

    //
    // Read the MAC address out of the EEPROM.
    //

    Status = E1000pReadDeviceMacAddress(Device);
    if (!KSUCCESS(Status)) {
        goto ResetDeviceEnd;
    }

    //
    // Notify the networking core of this new link now that the device is ready
    // to send and receive data, pending media being present. Though the device
    // wants to interrupt, the interrupt is not yet connected here.
    //

    if (Device->NetworkLink == NULL) {
        Status = E1000pAddNetworkDevice(Device);
        if (!KSUCCESS(Status)) {
            goto ResetDeviceEnd;
        }
    }

    Status = E1000pFillRxDescriptors(Device);
    if (!KSUCCESS(Status)) {
        goto ResetDeviceEnd;
    }

    //
    // Set up the MAC address filter.
    //

    E1000pSetReceiveAddress(Device, Device->EepromMacAddress, 0);
    RtlZeroMemory(NullAddress, ETHERNET_ADDRESS_SIZE);
    for (Index = 1; Index < E1000_RECEIVE_ADDRESSES; Index += 1) {
        E1000pSetReceiveAddress(Device, NullAddress, Index);
    }

    //
    // Set up the multicast filter.
    //

    for (Index = 0; Index < E1000_MULTICAST_TABLE_SIZE; Index += 1) {
        E1000_WRITE_ARRAY(Device, E1000MulticastTable, Index, 0);
    }

    if (Device->MediaType == E1000MediaCopper) {
        Status = E1000pSetupCopperLink(Device);

    } else {
        Status = E1000pSetupSerdesLink(Device);
    }

    if (!KSUCCESS(Status)) {
        goto ResetDeviceEnd;
    }

    E1000_WRITE(Device, E1000FlowControlType, E1000_FLOW_CONTROL_TYPE);
    E1000_WRITE(Device,
                E1000FlowControlAddressHigh,
                E1000_FLOW_CONTROL_ADDRESS_HIGH);

    E1000_WRITE(Device,
                E1000FlowControlAddressLow,
                E1000_FLOW_CONTROL_ADDRESS_LOW);

    E1000_WRITE(Device,
                E1000FlowControlTransmitTimerValue,
                E1000_FLOW_CONTROL_PAUSE_TIME);

    //
    // The link is set up, finish up other initialization.
    //

    E1000_WRITE(Device, E1000VlanEthertype, E1000_VLAN_ETHERTYPE);

    //
    // Initialize transmit.
    //

    E1000_WRITE(Device,
                E1000TxDescriptorLength0,
                sizeof(E1000_TX_DESCRIPTOR) * E1000_TX_RING_SIZE);

    E1000_WRITE(Device,
                E1000TxDescriptorBaseHigh0,
                Device->TxIoBuffer->Fragment[0].PhysicalAddress >> 32);

    E1000_WRITE(Device,
                E1000TxDescriptorBaseLow0,
                (ULONG)(Device->TxIoBuffer->Fragment[0].PhysicalAddress));

    E1000_WRITE(Device, E1000TxDescriptorTail0, 0);
    E1000_WRITE(Device, E1000TxDescriptorHead0, 0);
    E1000_WRITE(Device, E1000TxIpg, E1000_TX_IPG_VALUE);
    E1000_WRITE(Device, E1000TxInterruptDelayValue, E1000_TX_INTERRUPT_DELAY);
    E1000_WRITE(Device,
                E1000TxAbsoluteInterruptDelayValue,
                E1000_TX_INTERRUPT_ABSOLUTE_DELAY);

    TxControl = E1000_READ(Device, E1000TxControl);
    TxControl |= E1000_TX_CONTROL_ENABLE |
                 E1000_TX_CONTROL_PAD_SHORT_PACKETS |
                 E1000_TX_CONTROL_RETRANSMIT_LATE_COLLISION;

    E1000_WRITE(Device, E1000TxControl, TxControl);
    if (Device->MacType == E1000MacI354) {
        E1000_WRITE(Device,
                    E1000TxDescriptorControl0,
                    E1000_TXD_CONTROL_DEFAULT_VALUE_I354);

    } else {
        E1000_WRITE(Device,
                    E1000TxDescriptorControl0,
                    E1000_TXD_CONTROL_DEFAULT_VALUE);
    }

    //
    // Initialize receive. On a reset this could compete with capability change
    // requests. Synchronize it.
    //

    KeAcquireQueuedLock(Device->ConfigurationLock);
    RxControl = E1000_READ(Device, E1000RxControl);
    RxControl &= ~(E1000_RX_CONTROL_MULTICAST_OFFSET_MASK |
                   E1000_RX_CONTROL_BUFFER_SIZE_MASK |
                   E1000_RX_CONTROL_LONG_PACKET_ENABLE |
                   E1000_RX_CONTROL_BUFFER_SIZE_EXTENSION |
                   E1000_RX_CONTROL_MULTICAST_PROMISCUOUS |
                   E1000_RX_CONTROL_UNICAST_PROMISCUOUS |
                   E1000_RX_CONTROL_ENABLE);

    RxControl |= E1000_RX_CONTROL_BROADCAST_ACCEPT |
                 E1000_RX_CONTROL_BUFFER_SIZE_2K;

    E1000_WRITE(Device, E1000RxControl, RxControl);
    E1000pUpdateFilterMode(Device);
    E1000_WRITE(Device, E1000RxInterruptDelayTimer, E1000_RX_INTERRUPT_DELAY);
    E1000_WRITE(Device,
                E1000RxInterruptAbsoluteDelayTimer,
                E1000_RX_ABSOLUTE_INTERRUPT_DELAY);

    E1000_WRITE(Device,
                E1000RxDescriptorLength0,
                sizeof(E1000_RX_DESCRIPTOR) * E1000_RX_RING_SIZE);

    E1000_WRITE(Device,
                E1000RxDescriptorBaseHigh0,
                Device->RxIoBuffer->Fragment[0].PhysicalAddress >> 32);

    E1000_WRITE(Device,
                E1000RxDescriptorBaseLow0,
                (ULONG)(Device->RxIoBuffer->Fragment[0].PhysicalAddress));

    E1000_WRITE(Device, E1000RxDescriptorTail0, E1000_RX_RING_SIZE - 1);
    E1000_WRITE(Device, E1000RxDescriptorHead0, 0);
    RxChecksumControl = E1000_RX_CHECKSUM_START | E1000_RX_CHECKSUM_IP_OFFLOAD |
                        E1000_RX_CHECKSUM_TCP_UDP_OFFLOAD |
                        E1000_RX_CHECKSUM_IPV6_OFFLOAD;

    E1000_WRITE(Device, E1000RxChecksumControl, RxChecksumControl);
    if (Device->MacType == E1000MacI354) {
        E1000_WRITE(Device,
                    E1000RxDescriptorControl0,
                    E1000_RXD_CONTROL_DEFAULT_VALUE_I354);

    } else {
        E1000_WRITE(Device,
                    E1000RxDescriptorControl0,
                    E1000_RXD_CONTROL_DEFAULT_VALUE);
    }

    //
    // Write the tail again after enabling the ring to kick it into gear.
    //

    E1000_WRITE(Device, E1000RxDescriptorTail0, E1000_RX_RING_SIZE - 1);

    //
    // Enable receive globally.
    //

    RxControl |= E1000_RX_CONTROL_ENABLE;
    E1000_WRITE(Device, E1000RxControl, RxControl);
    KeReleaseQueuedLock(Device->ConfigurationLock);
    Status = STATUS_SUCCESS;

ResetDeviceEnd:
    return Status;
}

VOID
E1000pEnableInterrupts (
    PE1000_DEVICE Device
    )

/*++

Routine Description:

    This routine enables interrupts on the E1000 device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    //
    // Enable interrupts.
    //

    E1000_WRITE(Device, E1000InterruptMaskSet, E1000_INTERRUPT_ENABLE_MASK);

    //
    // Fire off a link status change interrupt to determine the link parameters.
    //

    E1000_WRITE(Device,
                E1000InterruptCauseSet,
                E1000_INTERRUPT_LINK_STATUS_CHANGE);

    return;
}

INTERRUPT_STATUS
E1000pInterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the e1000 interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the e1000 device
        structure.

Return Value:

    Interrupt status.

--*/

{

    PE1000_DEVICE Device;
    USHORT PendingBits;

    Device = (PE1000_DEVICE)Context;
    PendingBits = E1000_READ(Device, E1000InterruptCauseRead);
    if (PendingBits == 0) {
        return InterruptStatusNotClaimed;
    }

    if (PendingBits != 0) {
        RtlAtomicOr32(&(Device->PendingStatusBits), PendingBits);
        E1000_WRITE(Device, E1000InterruptMaskClear, PendingBits);
    }

    return InterruptStatusClaimed;
}

INTERRUPT_STATUS
E1000pInterruptServiceWorker (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine processes interrupts for the e1000 controller at low level.

Arguments:

    Parameter - Supplies an optional parameter passed in by the creator of the
        work item.

Return Value:

    Interrupt status.

--*/

{

    PE1000_DEVICE Device;
    ULONG PendingBits;

    Device = (PE1000_DEVICE)(Parameter);

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Clear out the pending bits.
    //

    PendingBits = RtlAtomicExchange32(&(Device->PendingStatusBits), 0);
    if (PendingBits == 0) {
        return InterruptStatusNotClaimed;
    }

    if ((PendingBits &
         (E1000_INTERRUPT_RX_OVERRUN | E1000_INTERRUPT_SMALL_RX_PACKET |
          E1000_INTERRUPT_RX_SEQUENCE_ERROR)) != 0) {

        RtlDebugPrint("E1000: Error Interrupts 0x%08x\n", PendingBits);
    }

    //
    // Handle link status changes.
    //

    if ((PendingBits & E1000_INTERRUPT_LINK_STATUS_CHANGE) != 0) {
        E1000pCheckLink(Device);
    }

    //
    // Process new receive frames.
    //

    E1000pReapReceivedFrames(Device);

    //
    // If the command unit finished what it was up to, reap that memory.
    //

    if ((PendingBits & E1000_INTERRUPT_TX_DESCRIPTOR_WRITTEN_BACK) != 0) {
        E1000pReapTxDescriptors(Device);
    }

    //
    // Re-enable interrupts now that they've been serviced.
    //

    E1000_WRITE(Device, E1000InterruptMaskSet, PendingBits);
    return InterruptStatusClaimed;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
E1000pSetupCopperLink (
    PE1000_DEVICE Device
    )

/*++

Routine Description:

    This routine sets up a copper-based link.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    USHORT AutoNegotiate;
    ULONG Control;
    USHORT GigabitControl;
    USHORT PhyControl;
    KSTATUS Status;
    ULONG TxControl;

    Control = E1000_READ(Device, E1000DeviceControl);
    Control |= E1000_DEVICE_CONTROL_SET_LINK_UP;
    if (Device->MacType != E1000Mac82543) {
        Control &= ~(E1000_DEVICE_CONTROL_FORCE_SPEED |
                     E1000_DEVICE_CONTROL_FORCE_DUPLEX);

        E1000_WRITE(Device, E1000DeviceControl, Control);

    } else {
        Control |= E1000_DEVICE_CONTROL_FORCE_SPEED |
                   E1000_DEVICE_CONTROL_FORCE_DUPLEX;

        E1000_WRITE(Device, E1000DeviceControl, Control);
        E1000pResetPhyHardware(Device);
    }

    Status = E1000pDetectPhy(Device);
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("E1000: Unable to detect PHY.\n");
        return Status;
    }

    //
    // Pre-config is done, set up auto-negotiation.
    //

    Status = E1000pReadPhy(Device,
                           E1000_PHY_AUTONEGOTIATE_ADVERTISEMENT,
                           &AutoNegotiate);

    if (!KSUCCESS(Status)) {
        goto SetupCopperLinkEnd;
    }

    Status = E1000pReadPhy(Device,
                           E1000_PHY_1000T_CONTROL,
                           &GigabitControl);

    if (!KSUCCESS(Status)) {
        goto SetupCopperLinkEnd;
    }

    AutoNegotiate |= E1000_AUTONEGOTIATE_ADVERTISE_10_HALF |
                     E1000_AUTONEGOTIATE_ADVERTISE_10_FULL |
                     E1000_AUTONEGOTIATE_ADVERTISE_100_HALF |
                     E1000_AUTONEGOTIATE_ADVERTISE_100_FULL;

    GigabitControl |= E1000_1000T_CONTROL_ADVERTISE_1000_FULL;

    //
    // Write the autonegotiate parameters.
    //

    Status = E1000pWritePhy(Device,
                            E1000_PHY_AUTONEGOTIATE_ADVERTISEMENT,
                            AutoNegotiate);

    if (!KSUCCESS(Status)) {
        goto SetupCopperLinkEnd;
    }

    Status = E1000pWritePhy(Device,
                            E1000_PHY_1000T_CONTROL,
                            GigabitControl);

    if (!KSUCCESS(Status)) {
        goto SetupCopperLinkEnd;
    }

    //
    // Start autonegotiation.
    //

    Status = E1000pReadPhy(Device, E1000_PHY_CONTROL, &PhyControl);
    if (!KSUCCESS(Status)) {
        goto SetupCopperLinkEnd;
    }

    PhyControl &= ~E1000_PHY_CONTROL_POWER_DOWN;
    PhyControl |= E1000_PHY_CONTROL_RESTART_AUTO_NEGOTIATION |
                  E1000_PHY_CONTROL_AUTO_NEGOTIATE_ENABLE;

    Status = E1000pWritePhy(Device, E1000_PHY_CONTROL, PhyControl);
    if (!KSUCCESS(Status)) {
        goto SetupCopperLinkEnd;
    }

    TxControl = E1000_READ(Device, E1000TxControl);
    TxControl &= ~E1000_TX_CONTROL_COLLISION_DISTANCE_MASK;
    TxControl |= E1000_TX_CONTROL_DEFAULT_COLLISION_DISTANCE <<
                 E1000_TX_CONTROL_COLLISION_DISTANCE_SHIFT;

    E1000_WRITE(Device, E1000TxControl, TxControl);

SetupCopperLinkEnd:
    return Status;
}

KSTATUS
E1000pSetupSerdesLink (
    PE1000_DEVICE Device
    )

/*++

Routine Description:

    This routine sets up a fiber serdes link.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    BOOL Autonegotiate;
    ULONG Control;
    ULONG ControlEx;
    ULONG PcsControl;
    ULONG TxConfiguration;
    ULONG TxControl;
    ULONG Value;

    //
    // Configure the collision distance.
    //

    TxControl = E1000_READ(Device, E1000TxControl);
    TxControl &= ~E1000_TX_CONTROL_COLLISION_DISTANCE_MASK;
    TxControl |= E1000_TX_CONTROL_DEFAULT_COLLISION_DISTANCE <<
                 E1000_TX_CONTROL_COLLISION_DISTANCE_SHIFT;

    E1000_WRITE(Device, E1000TxControl, TxControl);

    //
    // Set up flow control and enable autonegotiation.
    //

    Control = E1000_READ(Device, E1000DeviceControl);
    Control &= ~E1000_DEVICE_CONTROL_LINK_RESET;
    Control |= E1000_DEVICE_CONTROL_SET_LINK_UP |
               E1000_DEVICE_CONTROL_SPEED_1000 |
               E1000_DEVICE_CONTROL_FORCE_SPEED |
               E1000_DEVICE_CONTROL_FORCE_DUPLEX |
               E1000_DEVICE_CONTROL_DUPLEX;

    PcsControl = E1000_READ(Device, E1000PcsControl);
    PcsControl |= E1000_PCS_CONTROL_FORCED_SPEED_1000 |
                  E1000_PCS_CONTROL_FORCED_DUPLEX_FULL;

    PcsControl &= ~(E1000_PCS_CONTROL_FORCED_LINK_VALUE |
                    E1000_PCS_CONTROL_AUTONEGOTIATE_ENABLE |
                    E1000_PCS_CONTROL_FORCE_SPEED_DUPLEX |
                    E1000_PCS_CONTROL_FORCE_LINK);

    ControlEx = E1000_READ(Device, E1000ExtendedDeviceControl);
    Autonegotiate = TRUE;
    TxConfiguration = E1000_TX_CONFIGURATION_FULL_DUPLEX |
                      E1000_TX_CONFIGURATION_PAUSE_MASK |
                      E1000_TX_CONFIGURATION_AUTONEGOTIATE_ENABLE;

    if ((ControlEx & E1000_EXTENDED_CONTROL_LINK_MASK) ==
        E1000_EXTENDED_CONTROL_LINK_1000BASE_KX) {

        Autonegotiate = FALSE;
        TxConfiguration &= ~E1000_TX_CONFIGURATION_AUTONEGOTIATE_ENABLE;
        PcsControl |= E1000_PCS_CONTROL_FORCE_FLOW_CONTROL;
    }

    E1000_WRITE(Device, E1000TxConfigurationWord, TxConfiguration);
    if (Autonegotiate != FALSE) {
        PcsControl |= E1000_PCS_CONTROL_AUTONEGOTIATE_ENABLE |
                      E1000_PCS_CONTROL_AUTONEGOTIATE_RESTART;

        PcsControl &= ~E1000_PCS_CONTROL_FORCE_FLOW_CONTROL;
    }

    //
    // Configure PCS and power things up.
    //

    E1000_WRITE(Device, E1000PcsControl, PcsControl);
    Value = E1000_READ(Device, E1000PcsConfiguration);
    Value |= E1000_PCS_CONFIGURATION_PCS_ENABLE;
    E1000_WRITE(Device, E1000PcsConfiguration, Value);
    ControlEx = E1000_READ(Device, E1000ExtendedDeviceControl);
    ControlEx &= ~E1000_EXTENDED_CONTROL_SDP7_DATA;
    E1000_WRITE(Device, E1000ExtendedDeviceControl, ControlEx);

    //
    // Take the link out of reset.
    //

    E1000_WRITE(Device, E1000DeviceControl, Control);
    return STATUS_SUCCESS;
}

VOID
E1000pCheckLink (
    PE1000_DEVICE Device
    )

/*++

Routine Description:

    This routine checks on the link to see if it has come up or gone down.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    ULONG LinkStatus;
    BOOL LinkUp;
    USHORT PhyStatus;
    ULONGLONG Speed;
    KSTATUS Status;

    LinkUp = FALSE;
    Speed = 0;

    //
    // For copper links, ask the PHY.
    //

    if (Device->MediaType == E1000MediaCopper) {
        Status = E1000pReadPhy(Device, E1000_PHY_STATUS, &PhyStatus);
        if (!KSUCCESS(Status)) {
            goto CheckLinkEnd;
        }

        Status = E1000pReadPhy(Device, E1000_PHY_STATUS, &PhyStatus);
        if (!KSUCCESS(Status)) {
            goto CheckLinkEnd;
        }

        if ((PhyStatus & E1000_PHY_STATUS_LINK) != 0) {
            LinkUp = TRUE;
        }

        LinkStatus = E1000_READ(Device, E1000DeviceStatus);

    //
    // Internal serdes link check.
    //

    } else {
        LinkStatus = E1000_READ(Device, E1000DeviceStatus);
        if ((LinkStatus & E1000_DEVICE_STATUS_LINK_UP) != 0) {
            LinkUp = TRUE;
        }
    }

    if (LinkUp != FALSE) {
        if ((LinkStatus & E1000_DEVICE_STATUS_SPEED_1000) != 0) {
            Speed = NET_SPEED_1000_MBPS;

        } else if ((LinkStatus & E1000_DEVICE_STATUS_SPEED_100) != 0) {
            Speed = NET_SPEED_100_MBPS;

        } else {
            Speed = NET_SPEED_10_MBPS;
        }

        if (Device->MacType == E1000MacI354) {
            if (((LinkStatus & E1000_DEVICE_STATUS_2500_CAPABLE) != 0) &&
                ((LinkStatus & E1000_DEVICE_STATUS_SPEED_2500))) {

                Speed = NET_SPEED_2500_MBPS;
            }
        }

        if (Device->LinkSpeed != Speed) {
            Device->LinkSpeed = Speed;
            NetSetLinkState(Device->NetworkLink, TRUE, Speed);
        }

    } else {
        if (Device->LinkSpeed != 0) {
            NetSetLinkState(Device->NetworkLink, FALSE, 0);
            Device->LinkSpeed = 0;
        }
    }

CheckLinkEnd:
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("E1000: Check link failed: %d\n", Status);
    }

    return;
}

VOID
E1000pResetPhyHardware (
    PE1000_DEVICE Device
    )

/*++

Routine Description:

    This routine issues a reset to the PHY.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    ULONG Control;
    ULONG ExtendedControl;

    if (Device->MacType == E1000Mac82543) {
        ExtendedControl = E1000_READ(Device, E1000ExtendedDeviceControl);
        ExtendedControl |= E1000_EXTENDED_CONTROL_SDP4_DIRECTION;
        ExtendedControl &= ~E1000_EXTENDED_CONTROL_SDP4_DATA;
        E1000_WRITE(Device, E1000ExtendedDeviceControl, ExtendedControl);
        HlBusySpin(10000);
        ExtendedControl |= E1000_EXTENDED_CONTROL_SDP4_DATA;
        E1000_WRITE(Device, E1000ExtendedDeviceControl, ExtendedControl);

    } else {
        Control = E1000_READ(Device, E1000DeviceControl);
        Control |= E1000_DEVICE_CONTROL_PHY_RESET;
        E1000_WRITE(Device, E1000DeviceControl, Control);
        HlBusySpin(10000);
        Control &= ~E1000_DEVICE_CONTROL_PHY_RESET;
        E1000_WRITE(Device, E1000DeviceControl, Control);
    }

    HlBusySpin(10000);
    return;
}

KSTATUS
E1000pDetectPhy (
    PE1000_DEVICE Device
    )

/*++

Routine Description:

    This routine locates the PHY.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    PE1000_PHY_ENTRY PhyEntry;
    USHORT PhyIdHigh;
    USHORT PhyIdLow;
    KSTATUS Status;

    if (Device->PhyId != 0) {
        return STATUS_SUCCESS;
    }

    Status = E1000pReadPhy(Device, E1000_PHY_ID1, &PhyIdHigh);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Device->PhyId = PhyIdHigh << 16;
    HlBusySpin(20);
    Status = E1000pReadPhy(Device, E1000_PHY_ID2, &PhyIdLow);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Device->PhyId |= PhyIdLow & E1000_PHY_REVISION_MASK;
    Device->PhyRevision = PhyIdLow & (~E1000_PHY_REVISION_MASK);
    PhyEntry = &(E1000PhyEntries[0]);
    Device->PhyType = E1000PhyUnknown;
    while (PhyEntry->PhyId != 0) {
        if (PhyEntry->PhyId == Device->PhyId) {
            Device->PhyType = PhyEntry->PhyType;
            break;
        }

        PhyEntry += 1;
    }

    return STATUS_SUCCESS;
}

KSTATUS
E1000pReadPhy (
    PE1000_DEVICE Device,
    ULONG Address,
    PUSHORT Data
    )

/*++

Routine Description:

    This routine reads from a PHY register.

Arguments:

    Device - Supplies a pointer to the device.

    Address - Supplies the PHY register to read.

    Data - Supplies a pointer where the data will be returned on success.

Return Value:

    Status code.

--*/

{

    USHORT AddressShort;
    KSTATUS Status;

    if ((Device->PhyType == E1000PhyIgp) &&
        (Address > E1000_PHY_MAX_MULTI_PAGE_REGISTER)) {

        AddressShort = Address;
        Status = E1000pPerformPhyIo(Device,
                                    E1000_IGP1_PHY_PAGE_SELECT,
                                    &AddressShort,
                                    TRUE);

        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    Status = E1000pPerformPhyIo(Device,
                                Address & E1000_PHY_REGISTER_ADDRESS,
                                Data,
                                FALSE);

    return Status;
}

KSTATUS
E1000pWritePhy (
    PE1000_DEVICE Device,
    ULONG Address,
    USHORT Data
    )

/*++

Routine Description:

    This routine writes to a PHY register.

Arguments:

    Device - Supplies a pointer to the device.

    Address - Supplies the PHY register to write.

    Data - Supplies the data to write.

Return Value:

    Status code.

--*/

{

    USHORT AddressShort;
    KSTATUS Status;

    if ((Device->PhyType == E1000PhyIgp) &&
        (Address > E1000_PHY_MAX_MULTI_PAGE_REGISTER)) {

        AddressShort = Address;
        Status = E1000pPerformPhyIo(Device,
                                    E1000_IGP1_PHY_PAGE_SELECT,
                                    &AddressShort,
                                    TRUE);

        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    Status = E1000pPerformPhyIo(Device,
                                Address & E1000_PHY_REGISTER_ADDRESS,
                                &Data,
                                TRUE);

    return Status;
}

KSTATUS
E1000pPerformPhyIo (
    PE1000_DEVICE Device,
    ULONG Address,
    PUSHORT Data,
    BOOL Write
    )

/*++

Routine Description:

    This routine performs a low level read from the PHY.

Arguments:

    Device - Supplies a pointer to the device.

    Address - Supplies the PHY register to read.

    Data - Supplies a pointer that for writes contains the data to write. For
        reads, supplies a pointer where the read data will be returned.

    Write - Supplies a boolean indicating if this is a read or write.

Return Value:

    Status code.

--*/

{

    ULONG MdiControl;
    ULONG PhyAddress;
    ULONGLONG Time;
    ULONGLONG Timeout;

    PhyAddress = 1;

    ASSERT(Address <= E1000_PHY_REGISTER_ADDRESS);

    if (Device->MacType == E1000Mac82543) {

        //
        // Send a preamble, which is 32 consecutive 1 bits. Then shift the
        // command out, and the data in.
        //

        E1000pMdiShiftOut(Device, E1000_PHY_PREAMBLE, E1000_PHY_PREAMBLE_SIZE);
        if (Write != FALSE) {
            MdiControl = E1000_PHY_TURNAROUND | (Address << 2) |
                         (PhyAddress << 7) | (E1000_PHY_OP_WRITE << 12) |
                         (E1000_PHY_SOF << 14);

            MdiControl = (MdiControl << 16) | *Data;
            E1000pMdiShiftOut(Device, MdiControl, 32);

        } else {
            MdiControl = Address | (PhyAddress << 5) |
                         (E1000_PHY_OP_READ << 10) | (E1000_PHY_SOF << 12);

            E1000pMdiShiftOut(Device, MdiControl, 14);
            *Data = E1000pMdiShiftIn(Device);
        }

    //
    // Use the MDI control register to access the PHY.
    //

    } else {
        MdiControl = (Address << E1000_MDI_CONTROL_REGISTER_SHIFT) |
                     (PhyAddress << E1000_MDI_CONTROL_PHY_ADDRESS_SHIFT);

        if (Write != FALSE) {
            MdiControl |= E1000_PHY_OP_WRITE << E1000_MDI_CONTROL_PHY_OP_SHIFT;
            MdiControl |= *Data;

        } else {
            MdiControl |= E1000_PHY_OP_READ << E1000_MDI_CONTROL_PHY_OP_SHIFT;
        }

        E1000_WRITE(Device, E1000MdiControl, MdiControl);
        Time = HlQueryTimeCounter();
        Timeout = Time + HlQueryTimeCounterFrequency();
        while (Time <= Timeout) {
            HlBusySpin(50);
            MdiControl = E1000_READ(Device, E1000MdiControl);
            if ((MdiControl & E1000_MDI_CONTROL_READY) != 0) {
                break;
            }

            Time = HlQueryTimeCounter();
        }

        if ((MdiControl & E1000_MDI_CONTROL_READY) == 0) {
            RtlDebugPrint("E1000: PHY access failure.\n");
            return STATUS_TIMEOUT;
        }

        if ((MdiControl & E1000_MDI_CONTROL_ERROR) != 0) {
            RtlDebugPrint("E1000: PHY access error.\n");
            return STATUS_DEVICE_IO_ERROR;
        }

        *Data = (USHORT)MdiControl;
    }

    return STATUS_SUCCESS;
}

VOID
E1000pMdiShiftOut (
    PE1000_DEVICE Device,
    ULONG Data,
    ULONG BitCount
    )

/*++

Routine Description:

    This routine shifts data out the software defined pins.

Arguments:

    Device - Supplies a pointer to the device.

    Data - Supplies the bits to bang out.

    BitCount - Supplies the number of bits to send.

Return Value:

    None.

--*/

{

    ULONG Control;
    ULONG Mask;

    Mask = 1 << (BitCount - 1);
    Control = E1000_READ(Device, E1000DeviceControl);
    Control |= E1000_DEVICE_CONTROL_MDIO_DIRECTION |
               E1000_DEVICE_CONTROL_MDC_DIRECTION;

    while (Mask != 0) {
        Control &= ~E1000_DEVICE_CONTROL_MDIO;
        if ((Data & Mask) != 0) {
            Control |= E1000_DEVICE_CONTROL_MDIO;
        }

        E1000_WRITE(Device, E1000DeviceControl, Control);
        HlBusySpin(10);
        Control |= E1000_DEVICE_CONTROL_MDC;
        E1000_WRITE(Device, E1000DeviceControl, Control);
        HlBusySpin(10);
        Control &= ~E1000_DEVICE_CONTROL_MDC;
        E1000_WRITE(Device, E1000DeviceControl, Control);
        HlBusySpin(10);
        Mask >>= 1;
    }

    return;
}

USHORT
E1000pMdiShiftIn (
    PE1000_DEVICE Device
    )

/*++

Routine Description:

    This routine shifts data in from the software defined pins.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Returns the 16 bit value shifted in.

--*/

{

    ULONG Bit;
    ULONG Control;
    USHORT Data;

    Control = E1000_READ(Device, E1000DeviceControl);
    Control &= ~(E1000_DEVICE_CONTROL_MDIO_DIRECTION |
                 E1000_DEVICE_CONTROL_MDIO);

    E1000_WRITE(Device, E1000DeviceControl, Control);

    //
    // Send and up-down clock pulse before reading in the data. The first clock
    // occurred when the last bit of the register address was clocked out. This
    // pulse accounts for the turnaround bits.
    //

    Control |= E1000_DEVICE_CONTROL_MDC;
    E1000_WRITE(Device, E1000DeviceControl, Control);
    HlBusySpin(10);
    Control &= ~E1000_DEVICE_CONTROL_MDC;
    E1000_WRITE(Device, E1000DeviceControl, Control);
    HlBusySpin(10);
    Data = 0;
    for (Bit = 0; Bit < 16; Bit += 1) {
        Data <<= 1;
        Control |= E1000_DEVICE_CONTROL_MDC;
        E1000_WRITE(Device, E1000DeviceControl, Control);
        HlBusySpin(10);
        Control = E1000_READ(Device, E1000DeviceControl);
        if ((Control & E1000_DEVICE_CONTROL_MDIO) != 0) {
            Data |= 0x1;
        }

        Control &= ~E1000_DEVICE_CONTROL_MDC;
        E1000_WRITE(Device, E1000DeviceControl, Control);
        HlBusySpin(10);
    }

    Control |= E1000_DEVICE_CONTROL_MDC;
    E1000_WRITE(Device, E1000DeviceControl, Control);
    HlBusySpin(10);
    Control &= ~E1000_DEVICE_CONTROL_MDC;
    E1000_WRITE(Device, E1000DeviceControl, Control);
    HlBusySpin(10);
    return Data;
}

KSTATUS
E1000pReadDeviceMacAddress (
    PE1000_DEVICE Device
    )

/*++

Routine Description:

    This routine reads the device's MAC address out of the EEPROM.

Arguments:

    Device - Supplies a pointer to the device. The resulting MAC address will
        be stored in here.

Return Value:

    Status code.

--*/

{

    ULONG ByteIndex;
    ULONG MacHigh;
    ULONG MacLow;
    USHORT Register;
    KSTATUS Status;
    USHORT Value;

    //
    // See if there's already a MAC address in there, and use that if there is.
    //

    MacLow = E1000_READ_ARRAY(Device, E1000RxAddressLow, 0);
    MacHigh = E1000_READ_ARRAY(Device, E1000RxAddressHigh, 0);
    if ((MacHigh & E1000_RECEIVE_ADDRESS_HIGH_VALID) != 0) {
        RtlCopyMemory(&(Device->EepromMacAddress[0]), &MacLow, sizeof(ULONG));
        RtlCopyMemory(&(Device->EepromMacAddress[4]), &MacHigh, sizeof(USHORT));
        return STATUS_SUCCESS;
    }

    //
    // Read from the EEPROM.
    //

    Value = 0;
    for (ByteIndex = 0;
         ByteIndex < sizeof(Device->EepromMacAddress);
         ByteIndex += sizeof(USHORT)) {

        Register = ByteIndex >> 1;
        Status = E1000pReadEeprom(Device, Register, 1, &Value);
        if (!KSUCCESS(Status)) {
            return Status;
        }

        Device->EepromMacAddress[ByteIndex] = (BYTE)Value;
        Device->EepromMacAddress[ByteIndex + 1] =
                                               (BYTE)(Value >> BITS_PER_BYTE);

        Register += 1;
    }

    return Status;
}

KSTATUS
E1000pDetermineEepromCharacteristics (
    PE1000_DEVICE Device
    )

/*++

Routine Description:

    This routine gets information about the EEPROM on the given device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    PE1000_EEPROM_INFO Eeprom;
    ULONG EepromControl;

    Eeprom = &(Device->EepromInfo);
    EepromControl = E1000_READ(Device, E1000EepromControl);
    Eeprom->Type = E1000EepromMicrowire;
    switch (Device->MacType) {
    case E1000Mac82540:
    case E1000Mac82545:
    case E1000Mac82574:
    case E1000MacI350:
    case E1000MacI354:
        Eeprom->OpcodeBits = 3;
        Eeprom->Delay = 50;
        if ((EepromControl & E1000_EEPROM_CONTROL_NM_SIZE) != 0) {
            Eeprom->WordSize = 256;
            Eeprom->AddressBits = 8;

        } else {
            Eeprom->WordSize = 64;
            Eeprom->AddressBits = 6;
        }

        break;

    case E1000Mac82543:
        Eeprom->OpcodeBits = 3;
        Eeprom->WordSize = 64;
        Eeprom->AddressBits = 6;
        Eeprom->Delay = 50;
        break;

    default:

        ASSERT(FALSE);

        return STATUS_INVALID_CONFIGURATION;
    }

    return STATUS_SUCCESS;
}

KSTATUS
E1000pReadEeprom (
    PE1000_DEVICE Device,
    USHORT RegisterOffset,
    ULONG WordCount,
    PUSHORT Value
    )

/*++

Routine Description:

    This routine reads from the E1000 EEPROM.

Arguments:

    Device - Supplies a pointer to the device.

    RegisterOffset - Supplies the EEPROM register to read.

    WordCount - Supplies the number of words to read.

    Value - Supplies a pointer to a value that for write operations contains the
        value to write. For read operations, supplies a pointer where the
        read value will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_READY if the number of address bits could not be determined.

--*/

{

    UCHAR Opcode;
    KSTATUS Status;
    USHORT Word;
    ULONG WordIndex;

    ASSERT(Device->EepromInfo.AddressBits != 0);

    Status = E1000pEepromAcquire(Device);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    if (Device->EepromInfo.Type == E1000EepromMicrowire) {
        for (WordIndex = 0; WordIndex < WordCount; WordIndex += 1) {
            E1000pEepromShiftOut(Device,
                                 E1000_EEPROM_MICROWIRE_READ,
                                 Device->EepromInfo.OpcodeBits);

            E1000pEepromShiftOut(Device,
                                 RegisterOffset + WordIndex,
                                 Device->EepromInfo.AddressBits);

            Value[WordIndex] = E1000pEepromShiftIn(Device, 16);
        }

        E1000pEepromStandby(Device);

    } else if (Device->EepromInfo.Type == E1000EepromSpi) {
        if (!E1000pEepromSpiReady(Device)) {
            Status = STATUS_NOT_READY;
            goto ReadEepromEnd;
        }

        E1000pEepromStandby(Device);
        Opcode = E1000_EEPROM_SPI_READ;
        if ((Device->EepromInfo.AddressBits >= 8) && (RegisterOffset >= 0x80)) {
            Opcode |= E1000_EEPROM_SPI_ADDRESS8;
        }

        E1000pEepromShiftOut(Device, Opcode, Device->EepromInfo.OpcodeBits);
        E1000pEepromShiftOut(Device,
                             RegisterOffset * 2,
                             Device->EepromInfo.AddressBits);

        for (WordIndex = 0; WordIndex < WordCount; WordIndex += 1) {
            Word = E1000pEepromShiftIn(Device, 16);
            Value[WordIndex] = (Word >> BITS_PER_BYTE) |
                               (Word << BITS_PER_BYTE);
        }
    }

    Status = STATUS_SUCCESS;

ReadEepromEnd:
    E1000pEepromRelease(Device);
    return Status;
}

KSTATUS
E1000pEepromAcquire (
    PE1000_DEVICE Device
    )

/*++

Routine Description:

    This routine acquires the EEPROM for exclusive use.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    ULONG Control;
    ULONGLONG Time;
    ULONGLONG Timeout;

    Control = E1000_READ(Device, E1000EepromControl);
    if (Device->MacType != E1000Mac82543) {
        Control |= E1000_EEPROM_CONTROL_REQUEST_ACCESS;
        E1000_WRITE(Device, E1000EepromControl, Control);
        Time = HlQueryTimeCounter();
        Timeout = Time + HlQueryTimeCounterFrequency();
        while (((Control & E1000_EEPROM_CONTROL_GRANT_ACCESS) == 0) &&
               (Time <= Timeout)) {

            Control = E1000_READ(Device, E1000EepromControl);
            Time = HlQueryTimeCounter();
        }

        if ((Control & E1000_EEPROM_CONTROL_GRANT_ACCESS) == 0) {
            Control &= ~E1000_EEPROM_CONTROL_REQUEST_ACCESS;
            E1000_WRITE(Device, E1000EepromControl, Control);
            RtlDebugPrint("E1000: EEPROM acquire timeout.\n");
            return STATUS_TIMEOUT;
        }
    }

    if (Device->EepromInfo.Type == E1000EepromMicrowire) {
        Control &= ~(E1000_EEPROM_CONTROL_DATA_INPUT |
                     E1000_EEPROM_CONTROL_CLOCK_INPUT);

        E1000_WRITE(Device, E1000EepromControl, Control);
        Control |= E1000_EEPROM_CONTROL_CHIP_SELECT;
        E1000_WRITE(Device, E1000EepromControl, Control);

    } else if (Device->EepromInfo.Type == E1000EepromSpi) {
        Control &= ~(E1000_EEPROM_CONTROL_DATA_INPUT |
                     E1000_EEPROM_CONTROL_CLOCK_INPUT);

        E1000_WRITE(Device, E1000EepromControl, Control);
        HlBusySpin(1000);
    }

    return STATUS_SUCCESS;
}

VOID
E1000pEepromRelease (
    PE1000_DEVICE Device
    )

/*++

Routine Description:

    This routine releases the EEPROM.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    ULONG Control;

    Control = E1000_READ(Device, E1000EepromControl);
    if (Device->EepromInfo.Type == E1000EepromMicrowire) {
        Control &= ~(E1000_EEPROM_CONTROL_DATA_INPUT |
                     E1000_EEPROM_CONTROL_CHIP_SELECT);

        E1000_WRITE(Device, E1000EepromControl, Control);

        //
        // Clock out one more rising edge and falling edge.
        //

        Control |= E1000_EEPROM_CONTROL_CLOCK_INPUT;
        E1000_WRITE(Device, E1000EepromControl, Control);
        HlBusySpin(Device->EepromInfo.Delay);
        Control &= ~E1000_EEPROM_CONTROL_CLOCK_INPUT;
        E1000_WRITE(Device, E1000EepromControl, Control);
        HlBusySpin(Device->EepromInfo.Delay);

    } else if (Device->EepromInfo.Type == E1000EepromSpi) {
        Control &= ~E1000_EEPROM_CONTROL_CLOCK_INPUT;
        Control |= E1000_EEPROM_CONTROL_CHIP_SELECT;
        E1000_WRITE(Device, E1000EepromControl, Control);
        HlBusySpin(Device->EepromInfo.Delay);
    }

    if (Device->MacType != E1000Mac82543) {
        Control &= ~E1000_EEPROM_CONTROL_REQUEST_ACCESS;
        E1000_WRITE(Device, E1000EepromControl, Control);
    }

    return;
}

VOID
E1000pEepromStandby (
    PE1000_DEVICE Device
    )

/*++

Routine Description:

    This routine returns the EEPROM to a standby state.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    ULONG Control;

    Control = E1000_READ(Device, E1000EepromControl);
    if (Device->EepromInfo.Type == E1000EepromMicrowire) {
        Control &= ~(E1000_EEPROM_CONTROL_DATA_INPUT |
                     E1000_EEPROM_CONTROL_CHIP_SELECT);

        E1000_WRITE(Device, E1000EepromControl, Control);

        //
        // Clock out one more rising edge, enable chip select, then clock a
        // falling edge.
        //

        Control |= E1000_EEPROM_CONTROL_CLOCK_INPUT;
        E1000_WRITE(Device, E1000EepromControl, Control);
        HlBusySpin(Device->EepromInfo.Delay);
        Control |= E1000_EEPROM_CONTROL_CHIP_SELECT;
        E1000_WRITE(Device, E1000EepromControl, Control);
        HlBusySpin(Device->EepromInfo.Delay);
        Control &= ~E1000_EEPROM_CONTROL_CLOCK_INPUT;
        E1000_WRITE(Device, E1000EepromControl, Control);
        HlBusySpin(Device->EepromInfo.Delay);

    } else if (Device->EepromInfo.Type == E1000EepromSpi) {
        Control |= E1000_EEPROM_CONTROL_CHIP_SELECT;
        E1000_WRITE(Device, E1000EepromControl, Control);
        HlBusySpin(Device->EepromInfo.Delay);
        Control &= ~E1000_EEPROM_CONTROL_CHIP_SELECT;
        E1000_WRITE(Device, E1000EepromControl, Control);
        HlBusySpin(Device->EepromInfo.Delay);
    }

    return;
}

BOOL
E1000pEepromSpiReady (
    PE1000_DEVICE Device
    )

/*++

Routine Description:

    This routine determines if the given SPI-based EEPROM is ready for commands.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    TRUE if the EEPROM is ready.

    FALSE if the EEPROM is not ready or not responding.

--*/

{

    UCHAR SpiStatus;
    ULONGLONG Time;
    ULONGLONG Timeout;

    Time = HlQueryTimeCounter();
    Timeout = Time + HlQueryTimeCounterFrequency();

    //
    // Read the status register until the least significant bit is cleared.
    //

    while (Time <= Timeout) {
        Time = HlQueryTimeCounter();
        E1000pEepromShiftOut(Device,
                             E1000_EEPROM_SPI_READ_STATUS,
                             Device->EepromInfo.OpcodeBits);

        SpiStatus = E1000pEepromShiftIn(Device, 8);
        if ((SpiStatus & E1000_EEPROM_SPI_STATUS_BUSY) == 0) {
            break;
        }

        HlBusySpin(5000);
        E1000pEepromStandby(Device);
    }

    if (Time > Timeout) {
        return FALSE;
    }

    return TRUE;
}

VOID
E1000pEepromShiftOut (
    PE1000_DEVICE Device,
    USHORT Value,
    USHORT BitCount
    )

/*++

Routine Description:

    This routine shifts out EEPROM data.

Arguments:

    Device - Supplies a pointer to the device.

    Value - Supplies the value to write.

    BitCount - Supplies the number of bytes to write.

Return Value:

    None.

--*/

{

    ULONG Control;
    ULONG Mask;

    Control = E1000_READ(Device, E1000EepromControl);
    Mask = 1 << (BitCount - 1);
    if (Device->EepromInfo.Type == E1000EepromMicrowire) {
        Control &= ~E1000_EEPROM_CONTROL_DATA_OUTPUT;

    } else if (Device->EepromInfo.Type == E1000EepromSpi) {
        Control |= E1000_EEPROM_CONTROL_DATA_OUTPUT;
    }

    while (Mask != 0) {
        Control &= ~E1000_EEPROM_CONTROL_DATA_INPUT;
        if ((Value & Mask) != 0) {
            Control |= E1000_EEPROM_CONTROL_DATA_INPUT;
        }

        E1000_WRITE(Device, E1000EepromControl, Control);
        HlBusySpin(Device->EepromInfo.Delay);

        //
        // Pulse out a rising edge and falling edge to the clock.
        //

        Control |= E1000_EEPROM_CONTROL_CLOCK_INPUT;
        E1000_WRITE(Device, E1000EepromControl, Control);
        HlBusySpin(Device->EepromInfo.Delay);
        Control &= ~E1000_EEPROM_CONTROL_CLOCK_INPUT;
        E1000_WRITE(Device, E1000EepromControl, Control);
        HlBusySpin(Device->EepromInfo.Delay);

        //
        // Clock out the next bit.
        //

        Mask >>= 1;
    }

    Control &= ~E1000_EEPROM_CONTROL_DATA_INPUT;
    E1000_WRITE(Device, E1000EepromControl, Control);
    return;
}

USHORT
E1000pEepromShiftIn (
    PE1000_DEVICE Device,
    USHORT BitCount
    )

/*++

Routine Description:

    This routine shifts in EEPROM data.

Arguments:

    Device - Supplies a pointer to the device.

    BitCount - Supplies the number of bytes to read.

Return Value:

    Returns the read value.

--*/

{

    ULONG Bit;
    ULONG Control;
    USHORT Data;

    Control = E1000_READ(Device, E1000EepromControl);
    Control &= ~(E1000_EEPROM_CONTROL_DATA_OUTPUT |
                 E1000_EEPROM_CONTROL_DATA_INPUT);

    Data = 0;
    for (Bit = 0; Bit < BitCount; Bit += 1) {
        Data <<= 1;
        Control |= E1000_EEPROM_CONTROL_CLOCK_INPUT;
        E1000_WRITE(Device, E1000EepromControl, Control);
        HlBusySpin(Device->EepromInfo.Delay);
        Control = E1000_READ(Device, E1000EepromControl);
        Control &= ~E1000_EEPROM_CONTROL_DATA_INPUT;
        if ((Control & E1000_EEPROM_CONTROL_DATA_OUTPUT) != 0) {
            Data |= 0x1;
        }

        Control &= ~E1000_EEPROM_CONTROL_CLOCK_INPUT;
        E1000_WRITE(Device, E1000EepromControl, Control);
        HlBusySpin(Device->EepromInfo.Delay);
    }

    Control &= ~E1000_EEPROM_CONTROL_DATA_INPUT;
    E1000_WRITE(Device, E1000EepromControl, Control);
    return Data;
}

VOID
E1000pDetermineMediaType (
    PE1000_DEVICE Device
    )

/*++

Routine Description:

    This routine determines the type of media connected to this controller.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    ULONG ControlEx;

    ControlEx = E1000_READ(Device, E1000ExtendedDeviceControl);
    switch (ControlEx & E1000_EXTENDED_CONTROL_LINK_MASK) {
    case E1000_EXTENDED_CONTROL_LINK_1000BASE_KX:
    case E1000_EXTENDED_CONTROL_LINK_SERDES:
        Device->MediaType = E1000MediaInternalSerdes;
        break;

    default:
        Device->MediaType = E1000MediaCopper;
        break;
    }

    return;
}

VOID
E1000pSetReceiveAddress (
    PE1000_DEVICE Device,
    PUCHAR Address,
    ULONG Index
    )

/*++

Routine Description:

    This routine sets a receive address.

Arguments:

    Device - Supplies a pointer to the device.

    Address - Supplies a pointer to the 6 byte MAC address to set.

    Index - Supplies the address index to set.

Return Value:

    None.

--*/

{

    ULONG High;
    ULONG Low;

    Low = Address[0] | (Address[1] << 8) | (Address[2] << 16) |
          (Address[3] << 24);

    High = Address[4] | (Address[5] << 8) | E1000_RECEIVE_ADDRESS_HIGH_VALID;
    E1000_WRITE_ARRAY(Device, E1000RxAddressLow, Index << 1, Low);
    E1000_WRITE_ARRAY(Device, E1000RxAddressHigh, Index << 1, High);
    return;
}

KSTATUS
E1000pFillRxDescriptors (
    PE1000_DEVICE Device
    )

/*++

Routine Description:

    This routine fills up and initializes any receive descriptors.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    PNET_PACKET_BUFFER Buffer;
    ULONG Index;
    PE1000_RX_DESCRIPTOR RxDescriptor;
    KSTATUS Status;

    for (Index = 0; Index < E1000_RX_RING_SIZE; Index += 1) {
        if (Device->RxPackets[Index] != NULL) {
            continue;
        }

        Status = NetAllocateBuffer(0,
                                   E1000_RX_DATA_SIZE,
                                   0,
                                   Device->NetworkLink,
                                   0,
                                   &Buffer);

        if (!KSUCCESS(Status)) {
            return Status;
        }

        Device->RxPackets[Index] = Buffer;
        RxDescriptor = &(Device->RxDescriptors[Index]);
        RxDescriptor->Address = Buffer->BufferPhysicalAddress;
        RxDescriptor->Status = 0;
        RxDescriptor->Length = 0;
    }

    return STATUS_SUCCESS;
}

VOID
E1000pReapTxDescriptors (
    PE1000_DEVICE Device
    )

/*++

Routine Description:

    This routine reaps any transmit descriptors that the hardware is done with.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    ULONG Head;
    ULONG Index;
    ULONG ReapCount;
    ULONG ReapIndex;

    KeAcquireQueuedLock(Device->TxListLock);
    Head = E1000_READ(Device, E1000TxDescriptorHead0);
    ReapIndex = Device->TxNextReap;

    //
    // If the current head is beyond its previous location, then the number of
    // packets the hardware is done with is just the difference.
    //

    if (Head >= ReapIndex) {
        ReapCount = Head - ReapIndex;

    //
    // If the head wrapped, then the number of packets is from the previous
    // index to the end, plus however far the head got after wrapping.
    //

    } else {
        ReapCount = Head + (E1000_TX_RING_SIZE - ReapIndex);
    }

    //
    // Free the specified number of packets.
    //

    if (ReapCount != 0) {
        for (Index = 0; Index < ReapCount; Index += 1) {
            NetFreeBuffer(Device->TxPacket[ReapIndex]);
            Device->TxPacket[ReapIndex] = NULL;
            ReapIndex += 1;
            if (ReapIndex == E1000_TX_RING_SIZE) {
                ReapIndex = 0;
            }
        }

        Device->TxNextReap = Head;
        E1000pSendPendingPackets(Device);
    }

    KeReleaseQueuedLock(Device->TxListLock);
    return;
}

VOID
E1000pReapReceivedFrames (
    PE1000_DEVICE Device
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

    PE1000_RX_DESCRIPTOR Descriptor;
    ULONG DescriptorIndex;
    ULONG Flags;
    ULONG NewTail;
    PNET_PACKET_BUFFER Packet;

    KeAcquireQueuedLock(Device->RxListLock);
    DescriptorIndex = Device->RxListBegin;
    Descriptor = &(Device->RxDescriptors[DescriptorIndex]);
    while ((Descriptor->Status & E1000_RX_STATUS_DONE) != 0) {

        //
        // Handling packets that spawn multiple descriptors is not currently
        // supported.
        //

        ASSERT((Descriptor->Status & E1000_RX_STATUS_END_OF_PACKET) != 0);

        if (Descriptor->Errors != 0) {
            RtlDebugPrint("E1000: RX Packet Error %02x\n", Descriptor->Errors);
        }

        Packet = Device->RxPackets[DescriptorIndex];

        ASSERT(Packet->BufferPhysicalAddress == Descriptor->Address);

        Packet->DataSize = Descriptor->Length;
        Packet->DataOffset = 0;
        Packet->FooterOffset = Packet->DataSize;

        //
        // Determine the checksum offload flags, if the hardware computed them.
        //

        Flags = 0;
        if ((Descriptor->Status & E1000_RX_STATUS_IGNORE_CHECKSUM) == 0) {
            if ((Descriptor->Status & E1000_RX_STATUS_IP4_CHECKSUM) != 0) {
                if ((Descriptor->Errors & E1000_RX_ERROR_IP_CHECKSUM) != 0) {
                    Flags |= NET_PACKET_FLAG_IP_CHECKSUM_FAILED;

                } else {
                    Flags |= NET_PACKET_FLAG_IP_CHECKSUM_OFFLOAD;
                }
            }

            if ((Descriptor->Status & E1000_RX_STATUS_TCP_CHECKSUM) != 0) {
                if ((Descriptor->Errors &
                     E1000_RX_ERROR_TCP_UDP_CHECKSUM) != 0) {

                    Flags |= NET_PACKET_FLAG_TCP_CHECKSUM_FAILED;

                } else {
                    Flags |= NET_PACKET_FLAG_TCP_CHECKSUM_OFFLOAD;
                }
            }

            if ((Descriptor->Status & E1000_RX_STATUS_UDP_CHECKSUM) != 0) {
                if ((Descriptor->Errors &
                     E1000_RX_ERROR_TCP_UDP_CHECKSUM) != 0) {

                    Flags |= NET_PACKET_FLAG_UDP_CHECKSUM_FAILED;

                } else {
                    Flags |= NET_PACKET_FLAG_UDP_CHECKSUM_OFFLOAD;
                }
            }
        }

        Packet->Flags = Flags;
        NetProcessReceivedPacket(Device->NetworkLink, Packet);
        Descriptor->Status = 0;
        DescriptorIndex += 1;
        if (DescriptorIndex == E1000_RX_RING_SIZE) {
            DescriptorIndex = 0;
        }

        Descriptor = &(Device->RxDescriptors[DescriptorIndex]);
    }

    //
    // Write the new tail if there is one.
    //

    if (DescriptorIndex != Device->RxListBegin) {
        Device->RxListBegin = DescriptorIndex;
        if (DescriptorIndex == 0) {
            NewTail = E1000_RX_RING_SIZE - 1;

        } else {
            NewTail = DescriptorIndex - 1;
        }

        RtlMemoryBarrier();
        E1000_WRITE(Device, E1000RxDescriptorTail0, NewTail);
    }

    KeReleaseQueuedLock(Device->RxListLock);
    return;
}

VOID
E1000pSendPendingPackets (
    PE1000_DEVICE Device
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

    PE1000_TX_DESCRIPTOR Descriptor;
    PNET_PACKET_BUFFER Packet;
    ULONG Space;

    if (NET_PACKET_LIST_EMPTY(&(Device->TxPacketList))) {
        return;
    }

    //
    // In the non-wrapped case, all the descriptors after "next to use" are
    // free, plus all the ones before "next reap". Subtract one because the
    // queue can never be completely full, otherwise it would look empty.
    //

    if (Device->TxNextToUse >= Device->TxNextReap) {
        Space = E1000_TX_RING_SIZE - Device->TxNextToUse +
                Device->TxNextReap - 1;

    //
    // In the wrapped case, the head is catching up to a slow tail. Use the
    // rest of the space, minus one so as not to completely catch up.
    //

    } else {
        Space = Device->TxNextReap - Device->TxNextToUse - 1;
    }

    //
    // Avoid bumping the tail pointer if there's no room.
    //

    if (Space == 0) {
        return;
    }

    while ((NET_PACKET_LIST_EMPTY(&(Device->TxPacketList)) == FALSE) &&
           (Space != 0)) {

        Packet = LIST_VALUE(Device->TxPacketList.Head.Next,
                            NET_PACKET_BUFFER,
                            ListEntry);

        NET_REMOVE_PACKET_FROM_LIST(Packet, &(Device->TxPacketList));
        Descriptor = &(Device->TxDescriptors[Device->TxNextToUse]);
        Descriptor->Address = Packet->BufferPhysicalAddress +
                              Packet->DataOffset;

        Descriptor->Length = Packet->FooterOffset - Packet->DataOffset;
        Descriptor->Command = E1000_TX_COMMAND_INTERRUPT_DELAY |
                              E1000_TX_COMMAND_REPORT_STATUS |
                              E1000_TX_COMMAND_CRC |
                              E1000_TX_COMMAND_END;

        Descriptor->Status = 0;
        Device->TxPacket[Device->TxNextToUse] = Packet;

        //
        // Advance the descriptor, and account for the space.
        //

        Device->TxNextToUse += 1;
        if (Device->TxNextToUse == E1000_TX_RING_SIZE) {
            Device->TxNextToUse = 0;
        }

        Space -= 1;
    }

    E1000_WRITE(Device, E1000TxDescriptorTail0, Device->TxNextToUse);
    return;
}

VOID
E1000pUpdateFilterMode (
    PE1000_DEVICE Device
    )

/*++

Routine Description:

    This routine updates the device's receive filter mode based on the
    currently enabled capabilities.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    ULONG RxControl;

    ASSERT(KeIsQueuedLockHeld(Device->ConfigurationLock) != FALSE);

    RxControl = E1000_READ(Device, E1000RxControl);
    if ((Device->EnabledCapabilities &
         NET_LINK_CAPABILITY_PROMISCUOUS_MODE) != 0) {

        RxControl |= E1000_RX_CONTROL_MULTICAST_PROMISCUOUS |
                     E1000_RX_CONTROL_UNICAST_PROMISCUOUS;

    } else {
        RxControl &= ~(E1000_RX_CONTROL_MULTICAST_PROMISCUOUS |
                       E1000_RX_CONTROL_UNICAST_PROMISCUOUS);
    }

    E1000_WRITE(Device, E1000RxControl, RxControl);
    return;
}

