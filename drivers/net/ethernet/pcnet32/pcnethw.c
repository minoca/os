/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pcnethw.c

Abstract:

    This module implements the portion of the Am79C9xx PCnet driver that
    actually interacts with the hardware.

Author:

    Chris Stevens 9-Nov-2016

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/net/netdrv.h>
#include "pcnet.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum amount of packets that PCnet will keep queued before it
// starts to drop packets.
//

#define PCNET_MAX_TRANSMIT_PACKET_LIST_COUNT (PCNET_TRANSMIT_RING_LENGTH * 2)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
PcnetpLinkCheckDpc (
    PDPC Dpc
    );

KSTATUS
PcnetpInitializePhy (
    PPCNET_DEVICE Device
    );

KSTATUS
PcnetpCheckLink (
    PPCNET_DEVICE Device
    );

KSTATUS
PcnetpDetermineLinkParameters (
    PPCNET_DEVICE Device,
    PBOOL LinkUp,
    PULONGLONG Speed,
    PBOOL FullDuplex
    );

VOID
PcnetpReapReceivedDescriptors (
    PPCNET_DEVICE Device
    );

VOID
PcnetpReapTransmittedDescriptors (
    PPCNET_DEVICE Device
    );

VOID
PcnetpSendPendingPackets (
    PPCNET_DEVICE Device
    );

USHORT
PcnetpReadCsr (
    PPCNET_DEVICE Device,
    PCNET_CSR Register
    );

VOID
PcnetpWriteCsr (
    PPCNET_DEVICE Device,
    PCNET_CSR Register,
    USHORT Value
    );

USHORT
PcnetpReadBcr (
    PPCNET_DEVICE Device,
    PCNET_CSR Register
    );

VOID
PcnetpWriteBcr (
    PPCNET_DEVICE Device,
    PCNET_CSR Register,
    USHORT Value
    );

USHORT
PcnetpReadMii (
    PPCNET_DEVICE Device,
    USHORT PhyId,
    USHORT Register
    );

VOID
PcnetpWriteMii (
    PPCNET_DEVICE Device,
    USHORT PhyId,
    USHORT Register,
    USHORT Value
    );

//
// -------------------------------------------------------------------- Globals
//

BOOL PcnetDisablePacketDropping = FALSE;

//
// List the supported PCnet devices.
//

PCNET_DEVICE_INFORMATION PcnetDevices[] = {
    {PcnetAm79C970,
     0x243b,
     (PCNET_DEVICE_FLAG_AUTO_SELECT |
      PCNET_DEVICE_FLAG_AUI)},

    {PcnetAm79C970A,
     0x2621,
     (PCNET_DEVICE_FLAG_AUTO_SELECT |
      PCNET_DEVICE_FLAG_AUI |
      PCNET_DEVICE_FLAG_FULL_DUPLEX)},

    {PcnetAm79C973,
     0x2625,
     (PCNET_DEVICE_FLAG_FULL_DUPLEX |
      PCNET_DEVICE_FLAG_PHY |
      PCNET_DEVICE_FLAG_100_MBPS)},

    {PcnetAm79C975,
     0x2627,
     (PCNET_DEVICE_FLAG_FULL_DUPLEX |
      PCNET_DEVICE_FLAG_PHY |
      PCNET_DEVICE_FLAG_100_MBPS)},

    {PcnetAmInvalid, 0x0, }
};

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
PcnetSend (
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

    PPCNET_DEVICE Device;
    UINTN PacketListCount;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Device = (PPCNET_DEVICE)DeviceContext;
    KeAcquireQueuedLock(Device->TransmitListLock);
    if (Device->LinkActive == FALSE) {
        Status = STATUS_NO_NETWORK_CONNECTION;
        goto SendEnd;
    }

    //
    // If there is any room in the packet list (or dropping packets is
    // disabled), add all of the packets to the list waiting to be sent.
    //

    PacketListCount = Device->TransmitPacketList.Count;
    if ((PacketListCount < PCNET_MAX_TRANSMIT_PACKET_LIST_COUNT) ||
        (PcnetDisablePacketDropping != FALSE)) {

        NET_APPEND_PACKET_LIST(PacketList, &(Device->TransmitPacketList));
        PcnetpSendPendingPackets(Device);
        Status = STATUS_SUCCESS;

    //
    // Otherwise report that the resource is use as it is too busy to handle
    // more packets.
    //

    } else {
        Status = STATUS_RESOURCE_IN_USE;
    }

SendEnd:
    KeReleaseQueuedLock(Device->TransmitListLock);
    return Status;
}

KSTATUS
PcnetGetSetInformation (
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

    PULONG Flags;
    KSTATUS Status;

    switch (InformationType) {
    case NetLinkInformationChecksumOffload:
        if (*DataSize != sizeof(ULONG)) {
            return STATUS_INVALID_PARAMETER;
        }

        if (Set != FALSE) {
            return STATUS_NOT_SUPPORTED;
        }

        Flags = (PULONG)Data;
        *Flags = 0;
        break;

    default:
        Status = STATUS_NOT_SUPPORTED;
        break;
    }

    return Status;
}

KSTATUS
PcnetpInitializeDevice (
    PPCNET_DEVICE Device
    )

/*++

Routine Description:

    This routine initializes a PCnet32 LANCE device, performing operations that
    must run before the device structures are allocated and initialized.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    ULONG ChipId;
    PPCNET_DEVICE_INFORMATION DeviceInformation;
    ULONG Index;
    USHORT PartId;
    USHORT Style;
    USHORT Value;

    //
    // Perform a software reset of the device. This is always done with a
    // 16-bit register access.
    //

    PCNET_READ_REGISTER16(Device, PcnetWioReset);

    //
    // Check to see if the chip is in 32-bit register access mode.
    //

    Device->Registers32 = FALSE;
    Value = PcnetpReadBcr(Device, PcnetBcr18BusControl);
    if ((Value & PCNET_BCR18_DOUBLE_WORD_IO) != 0) {
        Device->Registers32 = TRUE;
        PCNET_READ_REGISTER32(Device, PcnetDwioReset);
    }

    //
    // Reading the chip ID register is only allowed if the stop bit is set.
    //

    Value = PcnetpReadCsr(Device, PcnetCsr0Status);
    if ((Value & PCNET_CSR0_STOP) == 0) {
        return STATUS_INVALID_CONFIGURATION;
    }

    //
    // Read the chip ID to determine which PCnet device is running.
    //

    ChipId = PcnetpReadCsr(Device, PcnetCsr88ChipIdLower);
    ChipId |= PcnetpReadCsr(Device, PcnetCsr89ChipIdUpper) << 16;
    PartId = (ChipId & PCNET_CHIP_ID_PART_ID_MASK) >>
             PCNET_CHIP_ID_PART_ID_SHIFT;

    DeviceInformation = &(PcnetDevices[0]);
    while (DeviceInformation->DeviceType != PcnetAmInvalid) {
        if (DeviceInformation->PartId == PartId) {
            Device->DeviceInformation = DeviceInformation;
            break;
        }

        DeviceInformation += 1;
    }

    if (DeviceInformation->DeviceType == PcnetAmInvalid) {
        RtlDebugPrint("PCNET: untested PCnet device 0x%04x, treating it like "
                      "Am79C970.\n",
                      PartId);

        Device->DeviceInformation = &(PcnetDevices[0]);
    }

    //
    // Read the MAC address. This can be done via byte access.
    //

    for (Index = 0; Index < ETHERNET_ADDRESS_SIZE; Index += 1) {
        Value = PCNET_READ_REGISTER8(Device, PcnetWioAprom + Index);
        Device->EepromMacAddress[Index] = (BYTE)Value;
    }

    //
    // Switch to 32-bit mode. Older chips like the Am79C90 only support 16-bit
    // mode. This driver could be easily adapted to run on such devices, but
    // they lack the chip ID register. It would need a way to detect the older
    // chips.
    //

    Style = (PCNET_BCR20_SOFTWARE_STYLE_PCNET_PCI <<
             PCNET_BCR20_SOFTWARE_STYLE_SHIFT) &
            PCNET_BCR20_SOFTWARE_STYLE_MASK;

    PcnetpWriteBcr(Device, PcnetBcr20SoftwareStyle, Style);
    return STATUS_SUCCESS;
}

KSTATUS
PcnetpInitializeDeviceStructures (
    PPCNET_DEVICE Device
    )

/*++

Routine Description:

    This routine performs housekeeping preparation for resetting and enabling
    an PCnet32 LANCE device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    ULONG Address;
    ULONG AllocationSize;
    PULONG BufferAddress;
    PULONG BufferFlags;
    PUSHORT BufferLength;
    PVOID Descriptor;
    PPCNET_RECEIVE_DESCRIPTOR_16 Descriptor16;
    PPCNET_RECEIVE_DESCRIPTOR_32 Descriptor32;
    ULONG DeviceFlags;
    PIO_BUFFER_FRAGMENT Fragment;
    UINTN FragmentOffset;
    ULONG FrameSize;
    ULONG Index;
    PPCNET_INITIALIZATION_BLOCK_16 InitBlock16;
    PPCNET_INITIALIZATION_BLOCK_32 InitBlock32;
    ULONG InitBlockSize;
    ULONG IoBufferFlags;
    ULONG IoBufferSize;
    PHYSICAL_ADDRESS MaxBufferAddress;
    USHORT Mode;
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONG ReceiveBufferSize;
    ULONG ReceiveDescriptorSize;
    ULONG ReceiveRingSize;
    ULONG RingAlignment;
    ULONG RingLength;
    KSTATUS Status;
    ULONG TransmitDescriptorSize;
    ULONG TransmitRingSize;
    USHORT Value;
    PVOID VirtualAddress;

    //
    // Read the software size bit to know which structures sizes to use.
    //

    Value = PcnetpReadBcr(Device, PcnetBcr20SoftwareStyle);
    if ((Value & PCNET_BCR20_SOFTWARE_SIZE_32) != 0) {
        Device->Software32 = TRUE;
        RingAlignment = PCNET_DESCRIPTOR_RING_ALIGNMENT_32;
        MaxBufferAddress = PCNET_MAX_DATA_FRAME_ADDRESS_32;
        TransmitDescriptorSize = sizeof(PCNET_TRANSMIT_DESCRIPTOR_32);
        ReceiveDescriptorSize = sizeof(PCNET_RECEIVE_DESCRIPTOR_32);
        InitBlockSize = sizeof(PCNET_INITIALIZATION_BLOCK_32);

    } else {
        Device->Software32 = FALSE;
        RingAlignment = PCNET_DESCRIPTOR_RING_ALIGNMENT_16;
        MaxBufferAddress = PCNET_MAX_DATA_FRAME_ADDRESS_16;
        TransmitDescriptorSize = sizeof(PCNET_TRANSMIT_DESCRIPTOR_16);
        ReceiveDescriptorSize = sizeof(PCNET_RECEIVE_DESCRIPTOR_16);
        InitBlockSize = sizeof(PCNET_INITIALIZATION_BLOCK_16);
    }

    //
    // Allocate the initialization block along with the transmit and receive
    // descriptor rings (which do not include the data buffers). As x86 is
    // cache coherent, there is no need to map this non-cached until the PCnet
    // runs on another architecture.
    //

    InitBlockSize = ALIGN_RANGE_UP(InitBlockSize, RingAlignment);
    ReceiveRingSize = ReceiveDescriptorSize * PCNET_RECEIVE_RING_LENGTH;
    ReceiveRingSize = ALIGN_RANGE_UP(ReceiveRingSize, RingAlignment);
    TransmitRingSize = TransmitDescriptorSize * PCNET_TRANSMIT_RING_LENGTH;

    ASSERT(Device->IoBuffer == NULL);

    IoBufferSize = InitBlockSize + ReceiveRingSize + TransmitRingSize;
    IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS;
    Device->IoBuffer = MmAllocateNonPagedIoBuffer(0,
                                                  MaxBufferAddress,
                                                  RingAlignment,
                                                  IoBufferSize,
                                                  IoBufferFlags);

    if (Device->IoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    ASSERT(Device->IoBuffer->FragmentCount == 1);
    ASSERT(Device->IoBuffer->Fragment[0].VirtualAddress != NULL);

    VirtualAddress = Device->IoBuffer->Fragment[0].VirtualAddress;
    PhysicalAddress = Device->IoBuffer->Fragment[0].PhysicalAddress;
    RtlZeroMemory(VirtualAddress, IoBufferSize);
    Device->InitializationBlock = VirtualAddress;
    VirtualAddress += InitBlockSize;
    Device->ReceiveDescriptor = VirtualAddress;
    VirtualAddress += ReceiveRingSize;
    Device->TransmitDescriptor = VirtualAddress;
    Device->ReceiveListBegin = 0;
    Device->TransmitLastReaped = PCNET_TRANSMIT_RING_LENGTH - 1;
    Device->TransmitNextToUse = 0;
    NET_INITIALIZE_PACKET_LIST(&(Device->TransmitPacketList));

    //
    // Set up the initialization block.
    //

    ASSERT((PhysicalAddress + IoBufferSize) <= MaxBufferAddress);

    DeviceFlags = Device->DeviceInformation->Flags;

    //
    // Devices with integrated PHYs do not have the auto-select bit in BCR2, so
    // they must set auto-select in the mode register (CSR15).
    //

    Mode = 0;
    if ((DeviceFlags & PCNET_DEVICE_FLAG_PHY) != 0) {
        Mode = (PCNET_MODE_PORT_SELECT_PHY << PCNET_MODE_PORT_SELECT_SHIFT) &
               PCNET_MODE_PORT_SELECT_MASK;
    }

    PhysicalAddress += InitBlockSize;
    if (Device->Software32 == FALSE) {
        InitBlock16 = Device->InitializationBlock;
        InitBlock16->Mode = Mode;
        RtlCopyMemory(InitBlock16->PhysicalAddress,
                      Device->EepromMacAddress,
                      ETHERNET_ADDRESS_SIZE);

        InitBlock16->LogicalAddress = 0;
        InitBlock16->ReceiveRingAddress = PhysicalAddress;
        RingLength = RtlCountTrailingZeros32(PCNET_RECEIVE_RING_LENGTH);
        InitBlock16->ReceiveRingAddress |=
                       (RingLength << PCNET_INIT16_RECEIVE_RING_LENGTH_SHIFT) &
                       PCNET_INIT16_RECEIVE_RING_LENGTH_MASK;

        PhysicalAddress += ReceiveRingSize;
        InitBlock16->TransmitRingAddress = PhysicalAddress;
        RingLength = RtlCountTrailingZeros32(PCNET_TRANSMIT_RING_LENGTH);
        InitBlock16->TransmitRingAddress |=
                      (RingLength << PCNET_INIT16_TRANSMIT_RING_LENGTH_SHIFT) &
                      PCNET_INIT16_TRANSMIT_RING_LENGTH_MASK;

    } else {
        InitBlock32 = Device->InitializationBlock;
        InitBlock32->Mode = Mode;
        RingLength = RtlCountTrailingZeros32(PCNET_RECEIVE_RING_LENGTH);
        InitBlock32->Mode |= (RingLength <<
                              PCNET_INIT32_RECEIVE_RING_LENGTH_SHIFT) &
                             PCNET_INIT32_RECEIVE_RING_LENGTH_MASK;

        RingLength = RtlCountTrailingZeros32(PCNET_TRANSMIT_RING_LENGTH);
        InitBlock32->Mode |= (RingLength <<
                              PCNET_INIT32_TRANSMIT_RING_LENGTH_SHIFT) &
                             PCNET_INIT32_TRANSMIT_RING_LENGTH_MASK;

        RtlCopyMemory(InitBlock32->PhysicalAddress,
                      Device->EepromMacAddress,
                      ETHERNET_ADDRESS_SIZE);

        InitBlock32->LogicalAddress = 0;
        InitBlock32->ReceiveRingAddress = PhysicalAddress;
        PhysicalAddress += ReceiveRingSize;
        InitBlock32->TransmitRingAddress = PhysicalAddress;
    }

    //
    // Allocate an array of pointers to net packet buffers that runs parallel
    // to the transmit array.
    //

    AllocationSize = sizeof(PNET_PACKET_BUFFER) * PCNET_TRANSMIT_RING_LENGTH;
    Device->TransmitPacket = MmAllocatePagedPool(AllocationSize,
                                                 PCNET_ALLOCATION_TAG);

    if (Device->TransmitPacket == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    RtlZeroMemory(Device->TransmitPacket, AllocationSize);

    //
    // Allocate a non-contiguous buffer for the receive data buffers. Again,
    // this does not need to be non-cached until the driver is ported to
    // another architecture.
    //

    FrameSize = ALIGN_RANGE_UP(PCNET_RECEIVE_FRAME_SIZE,
                               PCNET_RECEIVE_FRAME_ALIGNMENT);

    ReceiveBufferSize = PCNET_RECEIVE_RING_LENGTH * FrameSize;
    Device->ReceiveIoBuffer = MmAllocateNonPagedIoBuffer(0,
                                                         MaxBufferAddress,
                                                         0,
                                                         ReceiveBufferSize,
                                                         0);

    if (Device->ReceiveIoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    ASSERT((FrameSize & PCNET_RECEIVE_DESCRIPTOR_LENGTH_MASK) == FrameSize);

    //
    // Initialize the receive frame list.
    //

    Descriptor = Device->ReceiveDescriptor;
    Fragment = &(Device->ReceiveIoBuffer->Fragment[0]);
    FragmentOffset = 0;
    for (Index = 0; Index < PCNET_RECEIVE_RING_LENGTH; Index += 1) {
        Address = (ULONG)(Fragment->PhysicalAddress + FragmentOffset);
        if (Device->Software32 == FALSE) {
            Descriptor16 = Descriptor;
            BufferAddress = &(Descriptor16->BufferAddress);
            BufferLength = &(Descriptor16->BufferLength);
            BufferFlags = &(Descriptor16->BufferAddress);

        } else {
            Descriptor32 = Descriptor;
            BufferAddress = &(Descriptor32->BufferAddress);
            BufferLength = (PUSHORT)&(Descriptor32->BufferLength);
            BufferFlags = &(Descriptor32->BufferLength);
        }

        *BufferAddress = Address;
        *BufferLength = -FrameSize;
        RtlMemoryBarrier();
        *BufferFlags |= PCNET_RECEIVE_DESCRIPTOR_OWN;
        Descriptor += ReceiveDescriptorSize;
        FragmentOffset += FrameSize;
        if (FragmentOffset >= Fragment->Size) {
            Fragment += 1;
        }
    }

    //
    // Initialize the command and receive list locks.
    //

    Device->TransmitListLock = KeCreateQueuedLock();
    if (Device->TransmitListLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    Device->ReceiveListLock = KeCreateQueuedLock();
    if (Device->ReceiveListLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    Device->WorkItem = KeCreateWorkItem(
                              NULL,
                              WorkPriorityNormal,
                              (PWORK_ITEM_ROUTINE)PcnetpInterruptServiceWorker,
                              Device,
                              PCNET_ALLOCATION_TAG);

    if (Device->WorkItem == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    Device->LinkCheckTimer = KeCreateTimer(PCNET_ALLOCATION_TAG);
    if (Device->LinkCheckTimer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    Device->LinkCheckDpc = KeCreateDpc(PcnetpLinkCheckDpc, Device);
    if (Device->LinkCheckDpc == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    Status = STATUS_SUCCESS;

InitializeDeviceStructuresEnd:
    if (!KSUCCESS(Status)) {
        if (Device->TransmitListLock != NULL) {
            KeDestroyQueuedLock(Device->TransmitListLock);
            Device->TransmitListLock = NULL;
        }

        if (Device->ReceiveListLock != NULL) {
            KeDestroyQueuedLock(Device->ReceiveListLock);
            Device->ReceiveListLock = NULL;
        }

        if (Device->IoBuffer != NULL) {
            MmFreeIoBuffer(Device->IoBuffer);
            Device->IoBuffer = NULL;
            Device->ReceiveDescriptor = NULL;
            Device->TransmitDescriptor = NULL;
        }

        if (Device->ReceiveIoBuffer != NULL) {
            MmFreeIoBuffer(Device->ReceiveIoBuffer);
            Device->ReceiveIoBuffer = NULL;
        }

        if (Device->TransmitPacket != NULL) {
            MmFreePagedPool(Device->TransmitPacket);
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
PcnetpResetDevice (
    PPCNET_DEVICE Device
    )

/*++

Routine Description:

    This routine resets the PCnet32 device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    ULONG DeviceFlags;
    ULONGLONG Frequency;
    ULONGLONG Interval;
    PHYSICAL_ADDRESS PhysicalAddress;
    KSTATUS Status;
    ULONGLONG Timeout;
    USHORT Value;

    //
    // Reset the device.
    //

    if (Device->Registers32 == FALSE) {
        PCNET_READ_REGISTER16(Device, PcnetWioReset);

    } else {
        PCNET_READ_REGISTER32(Device, PcnetDwioReset);
    }

    //
    // Set auto-select if necessary.
    //

    DeviceFlags = Device->DeviceInformation->Flags;
    if ((DeviceFlags & PCNET_DEVICE_FLAG_AUTO_SELECT) != 0) {
        Value = PcnetpReadBcr(Device, PcnetBcr2Miscellaneous);
        Value |= PCNET_BCR2_AUTO_SELECT;
        PcnetpWriteBcr(Device, PcnetBcr2Miscellaneous, Value);
    }

    //
    // Enable full-duplex mode if the device supports it.
    //

    if ((DeviceFlags & PCNET_DEVICE_FLAG_FULL_DUPLEX) != 0) {
        Value = PcnetpReadBcr(Device, PcnetBcr9FullDuplex);
        Value |= PCNET_BCR9_FULL_DUPLEX_ENABLE;
        if ((DeviceFlags & PCNET_DEVICE_FLAG_AUI) != 0) {
            Value |= PCNET_BCR9_AUI_FULL_DUPLEX;
        }

        PcnetpWriteBcr(Device, PcnetBcr9FullDuplex, Value);
    }

    //
    // Enable auto pad to 64-bytes on transmit and auto strip of 64-byte pads
    // on receive. Also disable interrupts on transfer start.
    //

    Value = PcnetpReadCsr(Device, PcnetCsr4FeatureControl);
    Value |= PCNET_CSR4_AUTO_PAD_TRANSMIT |
             PCNET_CSR4_AUTO_STRIP_RECEIVE |
             PCNET_CSR4_TRANSMIT_START_MASK;

    PcnetpWriteCsr(Device, PcnetCsr4FeatureControl, Value);

    //
    // Set the initialization block, start initialization and then poll for the
    // initialization done interrupt.
    //

    PhysicalAddress = Device->IoBuffer->Fragment[0].PhysicalAddress;
    PcnetpWriteCsr(Device, PcnetCsr1InitBlockAddress0, (USHORT)PhysicalAddress);
    PcnetpWriteCsr(Device,
                   PcnetCsr2InitBlockAddress1,
                   (USHORT)(PhysicalAddress >> 16));

    PcnetpWriteCsr(Device, PcnetCsr0Status, PCNET_CSR0_INIT);
    Timeout = KeGetRecentTimeCounter() +
              KeConvertMicrosecondsToTimeTicks(PCNET_INITIALIZATION_TIMEOUT);

    Status = STATUS_NOT_READY;
    do {
        Value = PcnetpReadCsr(Device, PcnetCsr0Status);
        if ((Value & PCNET_CSR0_INIT_DONE) != 0) {
            Status = STATUS_SUCCESS;
            break;
        }

    } while (KeGetRecentTimeCounter() <= Timeout);

    if (!KSUCCESS(Status)) {
        goto ResetDeviceEnd;
    }

    PcnetpWriteCsr(Device, PcnetCsr0Status, PCNET_CSR0_INIT_DONE);

    //
    // Initialize the PHY.
    //

    Status = PcnetpInitializePhy(Device);
    if (!KSUCCESS(Status)){
        goto ResetDeviceEnd;
    }

    //
    // Notify the networking core of this new link now that the device is ready
    // to send and receive data, pending media being present.
    //

    if (Device->NetworkLink == NULL) {
        Status = PcnetpAddNetworkDevice(Device);
        if (!KSUCCESS(Status)) {
            goto ResetDeviceEnd;
        }
    }

    //
    // Enable interrupts and fire up the controller.
    //

    Value = PCNET_CSR0_START | PCNET_CSR0_INTERRUPT_ENABLED;
    PcnetpWriteCsr(Device, PcnetCsr0Status, Value);

    //
    // Check to see if the link is up.
    //

    PcnetpCheckLink(Device);

    //
    // Fire up the link check timer.
    //

    Frequency = HlQueryTimeCounterFrequency();
    Interval = Frequency * PCNET_LINK_CHECK_INTERVAL;
    KeQueueTimer(Device->LinkCheckTimer,
                 TimerQueueSoft,
                 0,
                 Interval,
                 0,
                 Device->LinkCheckDpc);

    Status = STATUS_SUCCESS;

ResetDeviceEnd:
    return Status;
}

INTERRUPT_STATUS
PcnetpInterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the PCnet32 interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the PCnet device
        structure.

Return Value:

    Interrupt status.

--*/

{

    PPCNET_DEVICE Device;
    INTERRUPT_STATUS InterruptStatus;
    USHORT PendingBits;

    Device = (PPCNET_DEVICE)Context;
    InterruptStatus = InterruptStatusNotClaimed;

    //
    // Read the status register, and if anything's set add it to the pending
    // bits.
    //

    PendingBits = PcnetpReadCsr(Device, PcnetCsr0Status) &
                  PCNET_CSR0_INTERRUPT_MASK;

    if (PendingBits != 0) {
        InterruptStatus = InterruptStatusClaimed;
        RtlAtomicOr32(&(Device->PendingStatusBits), PendingBits);

        //
        // Write to clear the bits that got grabbed. Since the semantics of
        // the eroro bits in this register are "write 1 to clear", any bits
        // that get set between the read and this write will just stick and
        // generate another level triggered interrupt. Unfortunately, the
        // interrupt enable register is "write 0 to clear", so it always needs
        // to get set.
        //

        PendingBits |= PCNET_CSR0_INTERRUPT_ENABLED;
        PcnetpWriteCsr(Device, PcnetCsr0Status, PendingBits);
    }

    return InterruptStatus;
}

INTERRUPT_STATUS
PcnetpInterruptServiceWorker (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine processes interrupts for the PCnet controller at low level.

Arguments:

    Parameter - Supplies an optional parameter passed in by the creator of the
        work item.

Return Value:

    Interrupt status.

--*/

{

    PPCNET_DEVICE Device;
    ULONG PendingBits;

    Device = (PPCNET_DEVICE)(Parameter);

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Clear out the pending bits.
    //

    PendingBits = RtlAtomicExchange32(&(Device->PendingStatusBits), 0);
    if (PendingBits == 0) {
        return InterruptStatusNotClaimed;
    }

    //
    // Reap the receive descriptors. A missed frame interrupt indicates that a
    // packet came in but couldn't find a descriptor. Try to alleviate the
    // pressure.
    //

    if (((PendingBits & PCNET_CSR0_RECEIVE_INTERRUPT) != 0) ||
        ((PendingBits & PCNET_CSR0_MISSED_FRAME) != 0)) {

        PcnetpReapReceivedDescriptors(Device);
    }

    //
    // If the command unit finished what it was up to, reap that memory.
    //

    if ((PendingBits & PCNET_CSR0_TRANSMIT_INTERRUPT) != 0) {
        PcnetpReapTransmittedDescriptors(Device);
    }

    //
    // If the software-only link status bit is set, the link check timer went
    // off.
    //

    if ((PendingBits & PCNET_CSR0_SOFTWARE_INTERRUPT_LINK_STATUS) != 0) {
        PcnetpCheckLink(Device);
    }

    return InterruptStatusClaimed;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
PcnetpLinkCheckDpc (
    PDPC Dpc
    )

/*++

Routine Description:

    This routine implements the PCnet DPC that is queued when a link check
    timer expires.

Arguments:

    Dpc - Supplies a pointer to the DPC that is running.

Return Value:

    None.

--*/

{

    PPCNET_DEVICE Device;
    ULONG OldPendingBits;
    KSTATUS Status;

    Device = (PPCNET_DEVICE)(Dpc->UserData);
    OldPendingBits = RtlAtomicOr32(&(Device->PendingStatusBits),
                                   PCNET_CSR0_SOFTWARE_INTERRUPT_LINK_STATUS);

    if ((OldPendingBits & PCNET_CSR0_SOFTWARE_INTERRUPT_LINK_STATUS) == 0) {
        Status = KeQueueWorkItem(Device->WorkItem);
        if (!KSUCCESS(Status)) {
            RtlAtomicAnd32(&(Device->PendingStatusBits),
                           ~PCNET_CSR0_SOFTWARE_INTERRUPT_LINK_STATUS);
        }
    }

    return;
}

KSTATUS
PcnetpInitializePhy (
    PPCNET_DEVICE Device
    )

/*++

Routine Description:

    This routine initialized the PCnet device's PHY.

Arguments:

    Device - Supplies a pointer to the device to initialize.

Return Value:

    Status code.

--*/

{

    USHORT BasicMask;
    USHORT PhyId;
    USHORT Value;

    if ((Device->DeviceInformation->Flags & PCNET_DEVICE_FLAG_PHY) == 0) {
        return STATUS_SUCCESS;
    }

    //
    // Find the PHY.
    //

    Device->PhyId = -1;
    BasicMask = MII_BASIC_STATUS_MEDIA_MASK | MII_BASIC_STATUS_EXTENDED_STATUS;
    for (PhyId = 0; PhyId < MII_PHY_COUNT; PhyId += 1) {
        Value = PcnetpReadMii(Device, PhyId, MiiRegisterBasicStatus);
        if ((Value != MAX_USHORT) && ((Value & BasicMask) != 0)) {
            Device->PhyId = PhyId;
            break;
        }
    }

    if (Device->PhyId == -1) {
        return STATUS_NO_SUCH_DEVICE;
    }

    //
    // Enabling auto-negotiation via the normal MII registers does not appear
    // to work. Make use of the PCnet's PHY control and status register.
    //

    Value = PcnetpReadBcr(Device, PcnetBcr32PhyControl);
    Value &= ~PCNET_BCR32_INIT_CLEAR_MASK;
    Value |= PCNET_BCR32_AUTO_NEGOTIATION_ENABLE;
    PcnetpWriteBcr(Device, PcnetBcr32PhyControl, Value);
    return STATUS_SUCCESS;
}

KSTATUS
PcnetpCheckLink (
    PPCNET_DEVICE Device
    )

/*++

Routine Description:

    This routine checks whether or not a PCnet device's media is still
    attached.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    ULONG DeviceFlags;
    BOOL FullDuplex;
    BOOL LinkActive;
    ULONGLONG Speed;
    KSTATUS Status;
    USHORT Value;

    //
    // If there is no PHY, then the link state needs to be taken from the BCRs.
    //

    DeviceFlags = Device->DeviceInformation->Flags;
    if ((DeviceFlags & PCNET_DEVICE_FLAG_PHY) == 0) {
        Speed = NET_SPEED_10_MBPS;
        FullDuplex = FALSE;
        if ((DeviceFlags & PCNET_DEVICE_FLAG_FULL_DUPLEX) != 0) {
            FullDuplex = TRUE;
        }

        LinkActive = FALSE;
        Value = PcnetpReadBcr(Device, PcnetBcr4LinkStatus);
        if ((Value & PCNET_BCR4_LINK_STATUS_ENABLE) != 0) {
            LinkActive = TRUE;
        }

    //
    // Otherwise read the PHY to determine the link status. The basic status
    // register must be read twice to get the most up to date data.
    //

    } else {
        Status = PcnetpDetermineLinkParameters(Device,
                                               &LinkActive,
                                               &Speed,
                                               &FullDuplex);

        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    //
    // If the link state's do not match, make some changes.
    //

    if ((Device->LinkActive != LinkActive) ||
        (Device->LinkSpeed != Speed) ||
        (Device->FullDuplex != FullDuplex)) {

        Device->LinkActive = LinkActive;
        Device->FullDuplex = FullDuplex;
        Device->LinkSpeed = Speed;
        NetSetLinkState(Device->NetworkLink, LinkActive, Speed);
    }

    return STATUS_SUCCESS;
}

KSTATUS
PcnetpDetermineLinkParameters (
    PPCNET_DEVICE Device,
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

    USHORT BasicControl;
    USHORT BasicStatus;
    USHORT BasicStatus2;
    USHORT CommonLink;
    USHORT PartnerAbility;

    *LinkUp = FALSE;
    *Speed = NET_SPEED_NONE;
    *FullDuplex = FALSE;
    BasicStatus = PcnetpReadMii(Device, Device->PhyId, MiiRegisterBasicStatus);
    BasicStatus2 = PcnetpReadMii(Device, Device->PhyId, MiiRegisterBasicStatus);
    BasicStatus |= BasicStatus2;
    if ((BasicStatus & MII_BASIC_STATUS_LINK_STATUS) == 0) {
        goto DetermineLinkParametersEnd;
    }

    BasicControl = PcnetpReadMii(Device,
                                 Device->PhyId,
                                 MiiRegisterBasicControl);

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

        CommonLink = PcnetpReadMii(Device, Device->PhyId, MiiRegisterAdvertise);
        PartnerAbility = PcnetpReadMii(Device,
                                       Device->PhyId,
                                       MiiRegisterLinkPartnerAbility);

        CommonLink &= PartnerAbility;
        if ((CommonLink & MII_ADVERTISE_100_FULL) != 0) {
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
    return STATUS_SUCCESS;
}

VOID
PcnetpReapReceivedDescriptors (
    PPCNET_DEVICE Device
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

    PVOID BufferAddress;
    PULONG BufferFlags;
    ULONG BufferFlagsMask;
    PUSHORT BufferLength;
    PULONG BufferPhysicalAddress;
    PPCNET_RECEIVE_DESCRIPTOR_16 Descriptor16;
    PPCNET_RECEIVE_DESCRIPTOR_32 Descriptor32;
    ULONG FrameSize;
    ULONG ListBegin;
    PUSHORT MessageLength;
    NET_PACKET_BUFFER Packet;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Get the base virtual address for the receive buffers. The I/O buffer may
    // not be physically contiguous, but it should be virtually contiguous.
    //

    BufferAddress = Device->ReceiveIoBuffer->Fragment[0].VirtualAddress;
    FrameSize = ALIGN_RANGE_UP(PCNET_RECEIVE_FRAME_SIZE,
                               PCNET_RECEIVE_FRAME_ALIGNMENT);

    if (Device->Software32 == FALSE) {
        BufferFlagsMask = PCNET_RECEIVE_DESCRIPTOR_FLAGS_MASK_16;

    } else {
        BufferFlagsMask = PCNET_RECEIVE_DESCRIPTOR_FLAGS_MASK_32;
    }

    //
    // Loop grabbing completed descriptors.
    //

    Packet.Flags = 0;
    KeAcquireQueuedLock(Device->ReceiveListLock);
    while (TRUE) {
        ListBegin = Device->ReceiveListBegin;
        if (Device->Software32 == FALSE) {
            Descriptor16 = Device->ReceiveDescriptor;
            Descriptor16 += ListBegin;
            BufferPhysicalAddress = &(Descriptor16->BufferAddress);
            BufferFlags = &(Descriptor16->BufferAddress);
            BufferLength = &(Descriptor16->BufferLength);
            MessageLength = &(Descriptor16->MessageLength);

        } else {
            Descriptor32 = Device->ReceiveDescriptor;
            Descriptor32 += ListBegin;
            BufferPhysicalAddress = &(Descriptor32->BufferAddress);
            BufferFlags = &(Descriptor32->BufferLength);
            BufferLength = (PUSHORT)&(Descriptor32->BufferLength);
            MessageLength = (PUSHORT)&(Descriptor32->MessageLength);
        }

        //
        // If the descriptor is still owned by the hardware, then it is not
        // ready to be reaped.
        //

        if ((*BufferFlags & PCNET_RECEIVE_DESCRIPTOR_OWN) != 0) {
            break;
        }

        //
        // The driver does not handle data chaining buffers.
        //

        ASSERT((*BufferFlags & PCNET_RECEIVE_DESCRIPTOR_START) != 0);
        ASSERT((*BufferFlags & PCNET_RECEIVE_DESCRIPTOR_END) != 0);

        //
        // If there were no errors, send it up to the core networking library
        // to process.
        //

        if ((*BufferFlags & PCNET_RECEIVE_DESCRIPTOR_ERROR) == 0) {
            Packet.Buffer = BufferAddress + (FrameSize * ListBegin);
            Packet.BufferPhysicalAddress = *BufferPhysicalAddress;
            Packet.BufferSize = *MessageLength &
                                PCNET_RECEIVE_DESCRIPTOR_LENGTH_MASK;

            Packet.DataSize = Packet.BufferSize;
            Packet.DataOffset = 0;
            Packet.FooterOffset = Packet.DataSize;
            NetProcessReceivedPacket(Device->NetworkLink, &Packet);
        }

        //
        // Set this frame up to be reused.
        //

        *BufferFlags &= ~BufferFlagsMask;
        *BufferLength = -FrameSize;
        *MessageLength = 0;
        RtlMemoryBarrier();
        *BufferFlags |= PCNET_RECEIVE_DESCRIPTOR_OWN;

        //
        // Move the beginning pointer up.
        //

        Device->ReceiveListBegin = PCNET_INCREMENT_RING_INDEX(
                                                    ListBegin,
                                                    PCNET_RECEIVE_RING_LENGTH);
    }

    KeReleaseQueuedLock(Device->ReceiveListLock);
    return;
}

VOID
PcnetpReapTransmittedDescriptors (
    PPCNET_DEVICE Device
    )

/*++

Routine Description:

    This routine cleans out any commands added to the command list that have
    been dealt with by the controller. This routine must be called at low
    level and assumes the command list lock is already held.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    PULONG BufferAddress;
    PULONG BufferFlags;
    PVOID Descriptor;
    PPCNET_TRANSMIT_DESCRIPTOR_16 Descriptor16;
    PPCNET_TRANSMIT_DESCRIPTOR_32 Descriptor32;
    BOOL DescriptorReaped;
    PULONG ErrorFlags;
    ULONG Index;

    KeAcquireQueuedLock(Device->TransmitListLock);
    DescriptorReaped = FALSE;
    while (TRUE) {

        //
        // Check to see if the next descriptor can be reaped.
        //

        Index = PCNET_INCREMENT_RING_INDEX(Device->TransmitLastReaped,
                                           PCNET_TRANSMIT_RING_LENGTH);

        //
        // Process the descriptor based on the software size.
        //

        Descriptor = Device->TransmitDescriptor;
        if (Device->Software32 == FALSE) {
            Descriptor16 = Descriptor;
            Descriptor16 += Index;
            BufferAddress = &(Descriptor16->BufferAddress);
            BufferFlags = &(Descriptor16->BufferAddress);
            ErrorFlags = &(Descriptor16->BufferLength);

        } else {
            Descriptor32 = Descriptor;
            Descriptor32 += Index;
            BufferAddress = &(Descriptor32->BufferAddress);
            BufferFlags = &(Descriptor32->BufferLength);
            ErrorFlags = &(Descriptor32->ErrorFlags);
        }

        //
        // If the buffer address was zero, then this descriptor is not in use.
        //

        if (*BufferAddress == 0) {
            break;
        }

        //
        // If the OWN bit is still set in the flags, then the hardware is still
        // working on this descriptor.
        //

        if ((*BufferFlags & PCNET_TRANSMIT_DESCRIPTOR_OWN) != 0) {
            break;
        }

        ASSERT((*BufferFlags & PCNET_TRANSMIT_DESCRIPTOR_START) != 0);
        ASSERT((*BufferFlags & PCNET_TRANSMIT_DESCRIPTOR_END) != 0);

        //
        // This descriptor is finished. Zero out the descriptor and free the
        // associated packet.
        //

        *BufferAddress = 0;
        *BufferFlags &= ~PCNET_TRANSMIT_DESCRIPTOR_FLAGS_MASK;
        *ErrorFlags &= ~PCNET_TRANSMIT_DESCRIPTOR_ERROR_FLAGS_MASK;
        NetFreeBuffer(Device->TransmitPacket[Index]);
        Device->TransmitPacket[Index] = NULL;

        //
        // Update the last reaped index to reflex that the descriptor at the
        // current index has been reaped.
        //

        Device->TransmitLastReaped = Index;
        DescriptorReaped = TRUE;
    }

    //
    // If space was freed up, send more segments.
    //

    if (DescriptorReaped != FALSE) {
        PcnetpSendPendingPackets(Device);
    }

    KeReleaseQueuedLock(Device->TransmitListLock);
    return;
}

VOID
PcnetpSendPendingPackets (
    PPCNET_DEVICE Device
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

    PULONG BufferAddress;
    PULONG BufferFlags;
    PUSHORT BufferLength;
    PPCNET_TRANSMIT_DESCRIPTOR_16 Descriptor16;
    PPCNET_TRANSMIT_DESCRIPTOR_32 Descriptor32;
    ULONG Index;
    PNET_PACKET_BUFFER Packet;
    ULONG PacketLength;
    BOOL WakeDevice;
    USHORT WakeFlags;

    //
    // Fire off as many pending packets as possible.
    //

    WakeDevice = FALSE;
    while ((NET_PACKET_LIST_EMPTY(&(Device->TransmitPacketList)) == FALSE) &&
           (Device->TransmitNextToUse != Device->TransmitLastReaped)) {

        Packet = LIST_VALUE(Device->TransmitPacketList.Head.Next,
                            NET_PACKET_BUFFER,
                            ListEntry);

        Index = Device->TransmitNextToUse;
        if (Device->Software32 == FALSE) {
            Descriptor16 = Device->TransmitDescriptor;
            Descriptor16 += Index;
            BufferAddress = &(Descriptor16->BufferAddress);
            BufferLength = (PUSHORT)&(Descriptor16->BufferLength);
            BufferFlags = &(Descriptor16->BufferAddress);

        } else {
            Descriptor32 = Device->TransmitDescriptor;
            Descriptor32 += Index;
            BufferAddress = &(Descriptor32->BufferAddress);
            BufferLength = (PUSHORT)&(Descriptor32->BufferLength);
            BufferFlags = &(Descriptor32->BufferLength);
        }

        //
        // The descriptor better be reaped and not in use.
        //

        ASSERT(*BufferAddress == 0);

        NET_REMOVE_PACKET_FROM_LIST(Packet, &(Device->TransmitPacketList));

        //
        // Fill out the descriptor.
        //

        *BufferAddress = Packet->BufferPhysicalAddress + Packet->DataOffset;
        PacketLength = Packet->FooterOffset - Packet->DataOffset;

        ASSERT(PacketLength == (USHORT)PacketLength);

        *BufferLength = -(USHORT)PacketLength;
        RtlMemoryBarrier();
        *BufferFlags |= PCNET_TRANSMIT_DESCRIPTOR_START |
                        PCNET_TRANSMIT_DESCRIPTOR_END |
                        PCNET_TRANSMIT_DESCRIPTOR_OWN;

        Device->TransmitPacket[Index] = Packet;

        //
        // Move the pointer past this entry.
        //

        Device->TransmitNextToUse = PCNET_INCREMENT_RING_INDEX(
                                                   Index,
                                                   PCNET_TRANSMIT_RING_LENGTH);

        WakeDevice = TRUE;
    }

    //
    // The interrupts enabled bit is cleared if written as zero, so it must be
    // set along with the on-demand polling bit.
    //

    if (WakeDevice != FALSE) {
        WakeFlags = PCNET_CSR0_TRANSMIT_DEMAND | PCNET_CSR0_INTERRUPT_ENABLED;
        PcnetpWriteCsr(Device, PcnetCsr0Status, WakeFlags);
    }

    return;
}

USHORT
PcnetpReadCsr (
    PPCNET_DEVICE Device,
    PCNET_CSR Register
    )

/*++

Routine Description:

    This routine reads a control and status register.

Arguments:

    Device - Supplies a pointer to the device.

    Register - Supplies the register to read.

Return Value:

    Returns the value read from the given CSR register.

--*/

{

    USHORT Result;

    if (Device->Registers32 == FALSE) {
        PCNET_WRITE_REGISTER16(Device, PcnetWioRegisterAddressPort, Register);
        Result = PCNET_READ_REGISTER16(Device, PcnetWioRegisterDataPort);

    } else {
        PCNET_WRITE_REGISTER32(Device, PcnetDwioRegisterAddressPort, Register);
        Result = PCNET_READ_REGISTER32(Device, PcnetDwioRegisterDataPort);
    }

    return Result;
}

VOID
PcnetpWriteCsr (
    PPCNET_DEVICE Device,
    PCNET_CSR Register,
    USHORT Value
    )

/*++

Routine Description:

    This routine writes a control and status register.

Arguments:

    Device - Supplies a pointer to the device.

    Register - Supplies the register to write.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

{

    if (Device->Registers32 == FALSE) {
        PCNET_WRITE_REGISTER16(Device, PcnetWioRegisterAddressPort, Register);
        PCNET_WRITE_REGISTER16(Device, PcnetWioRegisterDataPort, Value);

    } else {
        PCNET_WRITE_REGISTER32(Device, PcnetDwioRegisterAddressPort, Register);
        PCNET_WRITE_REGISTER32(Device, PcnetDwioRegisterDataPort, Value);
    }

    return;
}

USHORT
PcnetpReadBcr (
    PPCNET_DEVICE Device,
    PCNET_CSR Register
    )

/*++

Routine Description:

    This routine reads a bus control register.

Arguments:

    Device - Supplies a pointer to the device.

    Register - Supplies the register to read.

Return Value:

    Returns the value read from the given BCR register.

--*/

{

    USHORT Result;

    if (Device->Registers32 == FALSE) {
        PCNET_WRITE_REGISTER16(Device, PcnetWioRegisterAddressPort, Register);
        Result = PCNET_READ_REGISTER16(Device, PcnetWioBusDataPort);

    } else {
        PCNET_WRITE_REGISTER32(Device, PcnetDwioRegisterAddressPort, Register);
        Result = PCNET_READ_REGISTER32(Device, PcnetDwioBusDataPort);
    }

    return Result;
}

VOID
PcnetpWriteBcr (
    PPCNET_DEVICE Device,
    PCNET_CSR Register,
    USHORT Value
    )

/*++

Routine Description:

    This routine writes a bus control register.

Arguments:

    Device - Supplies a pointer to the device.

    Register - Supplies the register to write.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

{

    if (Device->Registers32 == FALSE) {
        PCNET_WRITE_REGISTER16(Device, PcnetWioRegisterAddressPort, Register);
        PCNET_WRITE_REGISTER16(Device, PcnetWioBusDataPort, Value);

    } else {
        PCNET_WRITE_REGISTER32(Device, PcnetDwioRegisterAddressPort, Register);
        PCNET_WRITE_REGISTER32(Device, PcnetDwioBusDataPort, Value);
    }

    return;
}

USHORT
PcnetpReadMii (
    PPCNET_DEVICE Device,
    USHORT PhyId,
    USHORT Register
    )

/*++

Routine Description:

    This routine reads a register from the PHY.

Arguments:

    Device - Supplies a pointer to the device.

    PhyId - Supplies the address of the PHY.

    Register - Supplies the MDIO register to read.

Return Value:

    Returns the value read from the register.

--*/

{

    USHORT Address;

    Address = (PhyId << PCNET_BCR33_PHY_ADDRESS_SHIFT) &
              PCNET_BCR33_PHY_ADDRESS_MASK;

    Address |= (Register << PCNET_BCR33_REG_ADDRESS_SHIFT) &
               PCNET_BCR33_REG_ADDRESS_MASK;

    PcnetpWriteBcr(Device, PcnetBcr33PhyAddress, Address);
    return PcnetpReadBcr(Device, PcnetBcr34PhyData);
}

VOID
PcnetpWriteMii (
    PPCNET_DEVICE Device,
    USHORT PhyId,
    USHORT Register,
    USHORT Value
    )

/*++

Routine Description:

    This routine writes a PHY register.

Arguments:

    Device - Supplies a pointer to the device.

    PhyId - Supplies the address of the PHY.

    Register - Supplies the MDIO register to write.

    Value - Supplies the value to write to the MDIO register.

Return Value:

    None.

--*/

{

    USHORT Address;

    Address = (PhyId << PCNET_BCR33_PHY_ADDRESS_SHIFT) &
              PCNET_BCR33_PHY_ADDRESS_MASK;

    Address |= (Register << PCNET_BCR33_REG_ADDRESS_SHIFT) &
               PCNET_BCR33_REG_ADDRESS_MASK;

    PcnetpWriteBcr(Device, PcnetBcr33PhyAddress, Address);
    PcnetpWriteBcr(Device, PcnetBcr34PhyData, Value);
    return;
}

