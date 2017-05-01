/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rtl81hw.c

Abstract:

    This module implements device support for the Realtek RTL81xx family of
    Ethernet controllers.

Author:

    Chris Stevens 20-Jun-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/net/netdrv.h>
#include "rtl81.h"

//
// --------------------------------------------------------------------- Macros
//

//
// These checksum macros can be used to determine if the IP, UDP, or TCP
// checksum failure bits are set in a receive descriptors command value.
//

#define RTL81_RECEIVE_IP_CHECKSUM_FAILURE(_Command) \
    ((_Command & RTL81_RECEIVE_DESCRIPTOR_COMMAND_IP_CHECKSUM_FAILURE) != 0)

#define RTL81_RECEIVE_UDP_CHECKSUM_FAILURE(_Command) \
    ((_Command & RTL81_RECEIVE_DESCRIPTOR_COMMAND_UDP_CHECKSUM_FAILURE) != 0)

#define RTL81_RECEIVE_TCP_CHECKSUM_FAILURE(_Command) \
    ((_Command & RTL81_RECEIVE_DESCRIPTOR_COMMAND_TCP_CHECKSUM_FAILURE) != 0)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum number of pending packets that will be saved before the
// driver starts to drop packets for legacy chips. Such chips only have 4
// descriptors, but save a fair amount of packets to be sent.
//

#define RTL81_MAX_TRANSMIT_PACKET_LIST_COUNT_LEGACY 64

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
Rtl81pInitializePhy (
    PRTL81_DEVICE Device
    );

VOID
Rtl81pCheckLinkState (
    PRTL81_DEVICE Device
    );

VOID
Rtl81pReapTransmitDescriptors (
    PRTL81_DEVICE Device
    );

VOID
Rtl81pSendPendingPackets (
    PRTL81_DEVICE Device
    );

VOID
Rtl81pSendPacketsLegacy (
    PRTL81_DEVICE Device
    );

VOID
Rtl81pSendPacketsDefault (
    PRTL81_DEVICE Device
    );

VOID
Rtl81pReapReceivedFrames (
    PRTL81_DEVICE Device
    );

VOID
Rtl81pReapReceivedFramesLegacy (
    PRTL81_DEVICE Device
    );

VOID
Rtl81pReapReceivedFramesDefault (
    PRTL81_DEVICE Device
    );

VOID
Rtl81pUpdateFilterMode (
    PRTL81_DEVICE Device
    );

KSTATUS
Rtl81pReadMacAddress (
    PRTL81_DEVICE Device
    );

KSTATUS
Rtl81pReadMdio (
    PRTL81_DEVICE Device,
    RTL81_MII_REGISTER Register,
    PULONG Data
    );

KSTATUS
Rtl81pWriteMdio (
    PRTL81_DEVICE Device,
    RTL81_MII_REGISTER Register,
    ULONG Data
    );

//
// -------------------------------------------------------------------- Globals
//

BOOL Rtl81DisablePacketDropping = FALSE;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
Rtl81Send (
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

    PRTL81_DEVICE Device;
    UINTN PacketListCount;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Device = (PRTL81_DEVICE)DeviceContext;
    KeAcquireQueuedLock(Device->TransmitLock);

    //
    // If there is any room in the packet list (or dropping packets is
    // disabled), add all of the packets to the list waiting to be sent.
    //

    PacketListCount = Device->TransmitPacketList.Count;
    if ((PacketListCount < Device->MaxTransmitPacketListCount) ||
        (Rtl81DisablePacketDropping != FALSE)) {

        NET_APPEND_PACKET_LIST(PacketList, &(Device->TransmitPacketList));
        Rtl81pSendPendingPackets(Device);
        Status = STATUS_SUCCESS;

    //
    // Otherwise report that the resource is use as it is too busy to handle
    // more packets.
    //

    } else {
        Status = STATUS_RESOURCE_IN_USE;
    }

    KeReleaseQueuedLock(Device->TransmitLock);
    return Status;
}

KSTATUS
Rtl81GetSetInformation (
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
    PRTL81_DEVICE Device;
    ULONG EnabledCapabilities;
    KSTATUS Status;
    ULONG SupportedCapabilities;
    USHORT Value;

    Device = (PRTL81_DEVICE)DeviceContext;
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
        // checksum bits.
        //

        *Capabilities &= NET_LINK_CAPABILITY_CHECKSUM_MASK;

        //
        // Not all RTL81xx devices support checksum offloading. Make sure the
        // supplied capabilities are supported.
        //

        SupportedCapabilities = Device->SupportedCapabilities &
                                NET_LINK_CAPABILITY_CHECKSUM_MASK;

        if ((*Capabilities & ~SupportedCapabilities) != 0) {
            Status = STATUS_NOT_SUPPORTED;
            break;
        }

        //
        // Synchronize updates to the enabled capabilities field and the
        // reprogramming of the hardware register. It would be bad if the field
        // said checksum offloading was enabled, but the hardware had it
        // disabled. Future calls to enable it would fail.
        //

        KeAcquireQueuedLock(Device->ConfigurationLock);

        //
        // If it is a set, figure out what is changing. There is nothing to do
        // if the change is in the transmit flags. Netcore requests transmit
        // offloads on a per-packet basis and there is not a global shut off on
        // RTL81xx devices. Requests to enable or disable receive checksum
        // offloading, however, need to modify the command 2 register.
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

            Value = RTL81_READ_REGISTER16(Device, Rtl81RegisterCommand2);
            if ((*Capabilities &
                 NET_LINK_CAPABILITY_CHECKSUM_RECEIVE_MASK) != 0) {

                Value |= RTL81_COMMAND_2_REGISTER_RECEIVE_CHECKSUM_OFFLOAD;
                *Capabilities |= NET_LINK_CAPABILITY_CHECKSUM_RECEIVE_MASK;

            //
            // If all receive capabilities are off, turn receive checksum
            // offloadng off.
            //

            } else if ((*Capabilities &
                         NET_LINK_CAPABILITY_CHECKSUM_RECEIVE_MASK) == 0) {

                Value &= ~RTL81_COMMAND_2_REGISTER_RECEIVE_CHECKSUM_OFFLOAD;
            }

            RTL81_WRITE_REGISTER16(Device, Rtl81RegisterCommand2, Value);
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
            Rtl81pUpdateFilterMode(Device);
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
Rtl81pInitializeDeviceStructures (
    PRTL81_DEVICE Device
    )

/*++

Routine Description:

    This routine performs housekeeping preparation for resetting and enabling
    an RTL81xx device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    UINTN AllocationSize;
    ULONG Capabilities;
    PRTL81_DEFAULT_DATA DefaultData;
    PRTL81_RECEIVE_DESCRIPTOR Descriptor;
    ULONG Flags;
    ULONG Index;
    PIO_BUFFER IoBuffer;
    ULONG IoBufferFlags;
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONGLONG Size;
    KSTATUS Status;
    ULONG Version;

    ASSERT(Device->TransmitLock == NULL);

    Device->TransmitLock = KeCreateQueuedLock();
    if (Device->TransmitLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    ASSERT(Device->ReceiveLock == NULL);

    Device->ReceiveLock = KeCreateQueuedLock();
    if (Device->ReceiveLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    ASSERT(Device->ConfigurationLock == NULL);

    Device->ConfigurationLock = KeCreateQueuedLock();
    if (Device->ConfigurationLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    NET_INITIALIZE_PACKET_LIST(&(Device->TransmitPacketList));

    //
    // The range of different RTL81xx devices use various register sets and
    // descriptor modes, among other scattered properties. Determine the card
    // type now and initialize the flags.
    //

    Flags = 0;
    Version = RTL81_READ_REGISTER32(Device, Rtl81RegisterTransmitConfiguration);
    Version &= RTL81_TRANSMIT_CONFIGURATION_HARDWARE_VERSION_MASK;
    switch (Version) {
    case RTL81_HARDWARE_VERSION_8101:
    case RTL81_HARDWARE_VERSION_8130:
    case RTL81_HARDWARE_VERSION_8139:
    case RTL81_HARDWARE_VERSION_8139A:
    case RTL81_HARDWARE_VERSION_8139AG:
    case RTL81_HARDWARE_VERSION_8139B:
    case RTL81_HARDWARE_VERSION_8139C:
        Flags = RTL81_FLAG_TRANSMIT_MODE_LEGACY |
                RTL81_FLAG_REGISTER_SET_LEGACY;

        break;

    case RTL81_HARDWARE_VERSION_8139CPLUS:
        Flags = RTL81_FLAG_REGISTER_SET_LEGACY |
                RTL81_FLAG_RECEIVE_COMMAND_LEGACY |
                RTL81_FLAG_DESCRIPTOR_LIMIT_64 |
                RTL81_FLAG_MULTI_SEGMENT_SUPPORT |
                RTL81_FLAG_CHECKSUM_OFFLOAD_DEFAULT;

        break;

    case RTL81_HARDWARE_VERSION_8102EL:
    case RTL81_HARDWARE_VERSION_8168E_VL:
        Flags = RTL81_FLAG_CHECKSUM_OFFLOAD_VLAN;
        break;

    default:
        RtlDebugPrint("RTL81: Untested hardware version 0x%08x.\n", Version);
        break;
    }

    Device->Flags = Flags;

    //
    // All RTL81xx devices support promiscuous mode, but do not enable it by
    // default.
    //

    Device->SupportedCapabilities |= NET_LINK_CAPABILITY_PROMISCUOUS_MODE;

    //
    // Both checksum versions support the same features. So start with checksum
    // offloading enabled for transmit and receive.
    //

    if ((Device->Flags & RTL81_FLAG_CHECKSUM_OFFLOAD_MASK) != 0) {
        Capabilities = NET_LINK_CAPABILITY_TRANSMIT_IP_CHECKSUM_OFFLOAD |
                       NET_LINK_CAPABILITY_TRANSMIT_UDP_CHECKSUM_OFFLOAD |
                       NET_LINK_CAPABILITY_TRANSMIT_TCP_CHECKSUM_OFFLOAD |
                       NET_LINK_CAPABILITY_RECEIVE_IP_CHECKSUM_OFFLOAD |
                       NET_LINK_CAPABILITY_RECEIVE_UDP_CHECKSUM_OFFLOAD |
                       NET_LINK_CAPABILITY_RECEIVE_TCP_CHECKSUM_OFFLOAD;

        Device->SupportedCapabilities |= Capabilities;
        Device->EnabledCapabilities |= Capabilities;
    }

    //
    // Set up the common transmit and receive interrupt status bits.
    //

    Device->TransmitInterruptMask = RTL81_INTERRUPT_TRANSMIT_OK |
                                    RTL81_INTERRUPT_TRANSMIT_ERROR;

    Device->ReceiveInterruptMask = RTL81_INTERRUPT_RECEIVE_OK |
                                   RTL81_INTERRUPT_RECEIVE_ERROR;

    //
    // The legacy devices have different transmit and receive data
    // requirements, so separate the initialization structures based on the
    // flags.
    //

    if ((Flags & RTL81_FLAG_TRANSMIT_MODE_LEGACY) != 0) {
        IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS;
        IoBuffer = MmAllocateNonPagedIoBuffer(
                                         0,
                                         MAX_ULONG,
                                         RTL81_RECEIVE_RING_BUFFER_ALIGNMENT,
                                         RTL81_RECEIVE_RING_BUFFER_PADDED_SIZE,
                                         IoBufferFlags);

        if (IoBuffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializeDeviceStructuresEnd;
        }

        ASSERT(IoBuffer->FragmentCount == 1);
        ASSERT(Device->U.LegacyData.ReceiveIoBuffer == NULL);

        Device->U.LegacyData.ReceiveIoBuffer = IoBuffer;

        ASSERT(Device->U.LegacyData.TransmitNextToUse == 0);
        ASSERT(Device->U.LegacyData.TransmitNextToClean == 0);

        Device->MaxTransmitPacketListCount =
                                   RTL81_MAX_TRANSMIT_PACKET_LIST_COUNT_LEGACY;

    } else {
        DefaultData = &(Device->U.DefaultData);
        if ((Flags & RTL81_FLAG_DESCRIPTOR_LIMIT_64) != 0) {
            DefaultData->TransmitDescriptorCount =
                                       RTL81_TRANSMIT_DESCRIPTOR_COUNT_LIMITED;

            DefaultData->ReceiveDescriptorCount =
                                        RTL81_RECEIVE_DESCRIPTOR_COUNT_LIMITED;

        } else {
            DefaultData->TransmitDescriptorCount =
                                       RTL81_TRANSMIT_DESCRIPTOR_COUNT_DEFAULT;

            DefaultData->ReceiveDescriptorCount =
                                        RTL81_RECEIVE_DESCRIPTOR_COUNT_DEFAULT;
        }

        Device->MaxTransmitPacketListCount =
                                      DefaultData->TransmitDescriptorCount * 2;

        AllocationSize = (DefaultData->TransmitDescriptorCount *
                          sizeof(RTL81_TRANSMIT_DESCRIPTOR)) +
                         (DefaultData->TransmitDescriptorCount *
                          sizeof(PNET_PACKET_BUFFER)) +
                         (DefaultData->ReceiveDescriptorCount *
                          sizeof(RTL81_RECEIVE_DESCRIPTOR)) +
                         (DefaultData->ReceiveDescriptorCount *
                          RTL81_RECEIVE_BUFFER_DATA_SIZE);

        IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS;
        IoBuffer = MmAllocateNonPagedIoBuffer(0,
                                              MAX_ULONGLONG,
                                              RTL81_DESCRIPTOR_ALIGNMENT,
                                              AllocationSize,
                                              IoBufferFlags);

        if (IoBuffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializeDeviceStructuresEnd;
        }

        ASSERT(IoBuffer->FragmentCount == 1);

        //
        // Zero out everything except the receive packet data buffers.
        //

        Size = AllocationSize -
               (DefaultData->ReceiveDescriptorCount *
                RTL81_RECEIVE_BUFFER_DATA_SIZE);

        RtlZeroMemory(IoBuffer->Fragment[0].VirtualAddress, Size);

        //
        // Carve up the buffer, giving each array its piece.
        //

        PhysicalAddress = IoBuffer->Fragment[0].PhysicalAddress;
        DefaultData->TransmitDescriptor = IoBuffer->Fragment[0].VirtualAddress;

        ASSERT(IS_ALIGNED(PhysicalAddress, RTL81_DESCRIPTOR_ALIGNMENT));

        PhysicalAddress += (DefaultData->TransmitDescriptorCount *
                            sizeof(RTL81_TRANSMIT_DESCRIPTOR));

        DefaultData->TransmitBuffer =
                  (PNET_PACKET_BUFFER *)(DefaultData->TransmitDescriptor +
                                         DefaultData->TransmitDescriptorCount);

        PhysicalAddress += (DefaultData->TransmitDescriptorCount *
                            sizeof(PNET_PACKET_BUFFER));

        DefaultData->ReceiveDescriptor =
             (PRTL81_RECEIVE_DESCRIPTOR)(DefaultData->TransmitBuffer +
                                         DefaultData->TransmitDescriptorCount);

        ASSERT(IS_ALIGNED(PhysicalAddress, RTL81_DESCRIPTOR_ALIGNMENT));

        PhysicalAddress += (DefaultData->ReceiveDescriptorCount *
                            sizeof(RTL81_RECEIVE_DESCRIPTOR));

        DefaultData->ReceivePacketData =
                                  (PVOID)(DefaultData->ReceiveDescriptor +
                                          DefaultData->ReceiveDescriptorCount);

        ASSERT(Device->U.DefaultData.DescriptorIoBuffer == NULL);

        DefaultData->DescriptorIoBuffer = IoBuffer;

        ASSERT(DefaultData->TransmitNextToUse == 0);
        ASSERT(DefaultData->TransmitNextToClean == 0);
        ASSERT(DefaultData->ReceiveNextToReap == 0);

        //
        // Initialize the receive descriptors so that they are marked as owned
        // by the hardware and have the correct physical address and size in
        // place.
        //

        ASSERT(RTL81_RECEIVE_BUFFER_DATA_SIZE <=
               (RTL81_RECEIVE_DESCRIPTOR_COMMAND_SIZE_MASK >>
                RTL81_RECEIVE_DESCRIPTOR_COMMAND_SIZE_SHIFT));

        Descriptor = NULL;
        for (Index = 0;
             Index < DefaultData->ReceiveDescriptorCount;
             Index += 1) {

            Descriptor = &(DefaultData->ReceiveDescriptor[Index]);
            Descriptor->Command = RTL81_RECEIVE_DESCRIPTOR_DEFAULT_COMMAND;
            Descriptor->PhysicalAddress = PhysicalAddress;
            PhysicalAddress += RTL81_RECEIVE_BUFFER_DATA_SIZE;
        }

        //
        // Mark the last descriptor so that the hardware knows this is the end.
        //

        Descriptor->Command |= RTL81_RECEIVE_DESCRIPTOR_COMMAND_END_OF_RING;

        //
        // Add device specific transmit and receive mask bits.
        //

        Device->TransmitInterruptMask |= RTL81_INTERRUPT_TRANSMIT_UNAVAILABLE;
        Device->ReceiveInterruptMask |= RTL81_INTERRUPT_RECEIVE_OVERFLOW;
    }

    Status = STATUS_SUCCESS;

InitializeDeviceStructuresEnd:
    return Status;
}

VOID
Rtl81pDestroyDeviceStructures (
    PRTL81_DEVICE Device
    )

/*++

Routine Description:

    This routine performs destroy any device structures allocated for the
    RTL81xx device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    if (Device->TransmitLock != NULL) {
        KeDestroyQueuedLock(Device->TransmitLock);
    }

    if (Device->ReceiveLock != NULL) {
        KeDestroyQueuedLock(Device->ReceiveLock);
    }

    if (Device->ConfigurationLock != NULL) {
        KeDestroyQueuedLock(Device->ConfigurationLock);
    }

    if (Device->InterruptHandle != INVALID_HANDLE){
        IoDisconnectInterrupt(Device->InterruptHandle);
    }

    if ((Device->Flags & RTL81_FLAG_TRANSMIT_MODE_LEGACY) != 0) {
        if (Device->U.LegacyData.ReceiveIoBuffer != NULL) {
            MmFreeIoBuffer(Device->U.LegacyData.ReceiveIoBuffer);
        }

    } else {
        if (Device->U.DefaultData.DescriptorIoBuffer != NULL) {
            MmFreeIoBuffer(Device->U.DefaultData.DescriptorIoBuffer);
        }
    }

    return;
}

KSTATUS
Rtl81pInitialize (
    PRTL81_DEVICE Device
    )

/*++

Routine Description:

    This routine initializes and enables the RTL81xx device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    USHORT Command2;
    BYTE CommandRegister;
    BYTE CommandRegisterMask;
    ULONGLONG CurrentTime;
    PIO_BUFFER_FRAGMENT Fragment;
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONG ReceiveBufferStart;
    ULONG ReceiveConfiguration;
    KSTATUS Status;
    ULONGLONG Timeout;
    ULONGLONG TimeoutTicks;

    TimeoutTicks = HlQueryTimeCounterFrequency() * RTL81_DEVICE_TIMEOUT;

    //
    // Execute a software reset on the devices.
    //

    RTL81_WRITE_REGISTER8(Device,
                          Rtl81RegisterCommand,
                          RTL81_COMMAND_REGISTER_RESET);

    CurrentTime = KeGetRecentTimeCounter();
    Timeout = CurrentTime + TimeoutTicks;
    do {
        CommandRegister = RTL81_READ_REGISTER8(Device, Rtl81RegisterCommand);
        if ((CommandRegister & RTL81_COMMAND_REGISTER_RESET) == 0) {
            break;
        }

        CurrentTime = KeGetRecentTimeCounter();

    } while (CurrentTime <= Timeout);

    if (CurrentTime > Timeout) {
        Status = STATUS_TIMEOUT;
        goto InitializeEnd;
    }

    //
    // Get the MAC address out of the EEPROM.
    //

    Status = Rtl81pReadMacAddress(Device);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    //
    // Reset the PHY and start auto-negotiation.
    //

    Status = Rtl81pInitializePhy(Device);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    //
    // Disable all interrupts.
    //

    RTL81_WRITE_REGISTER16(Device, Rtl81RegisterInterruptMask, 0);

    //
    // Initialize the transmit and receive buffers based on the device type.
    //

    if ((Device->Flags & RTL81_FLAG_TRANSMIT_MODE_LEGACY) != 0) {
        Fragment = &(Device->U.LegacyData.ReceiveIoBuffer->Fragment[0]);

        ASSERT(Fragment->PhysicalAddress == (ULONG)Fragment->PhysicalAddress);

        ReceiveBufferStart = (ULONG)Fragment->PhysicalAddress;;
        RTL81_WRITE_REGISTER32(Device,
                               Rtl81RegisterReceiveBufferStart,
                               ReceiveBufferStart);

    } else {

        //
        // Enable transmit and receive in the second command register. Also
        // enable checksum offload on receive if set.
        //

        Command2 = RTL81_COMMAND_2_REGISTER_DEFAULT;
        if ((Device->EnabledCapabilities &
            NET_LINK_CAPABILITY_CHECKSUM_RECEIVE_MASK) != 0) {

            Command2 |= RTL81_COMMAND_2_REGISTER_RECEIVE_CHECKSUM_OFFLOAD;
        }

        RTL81_WRITE_REGISTER16(Device, Rtl81RegisterCommand2, Command2);
        Fragment = &(Device->U.DefaultData.DescriptorIoBuffer->Fragment[0]);
        PhysicalAddress = Fragment->PhysicalAddress;
        RTL81_WRITE_REGISTER32(Device,
                               Rtl81RegisterTransmitDescriptorBaseLow,
                               (ULONG)PhysicalAddress);

        RTL81_WRITE_REGISTER32(Device,
                               Rtl81RegisterTransmitDescriptorBaseHigh,
                               (ULONG)(PhysicalAddress >> 32));

        PhysicalAddress += (Device->U.DefaultData.TransmitDescriptorCount *
                            sizeof(RTL81_TRANSMIT_DESCRIPTOR)) +
                           (Device->U.DefaultData.TransmitDescriptorCount *
                            sizeof(PNET_PACKET_BUFFER));

        RTL81_WRITE_REGISTER32(Device,
                               Rtl81RegisterReceiveDescriptorBaseLow,
                               (ULONG)PhysicalAddress);

        RTL81_WRITE_REGISTER32(Device,
                               Rtl81RegisterReceiveDescriptorBaseHigh,
                               (ULONG)(PhysicalAddress >> 32));
    }

    //
    // Enable transmit and receive.
    //

    CommandRegisterMask = RTL81_COMMAND_REGISTER_RECEIVE_ENABLE |
                          RTL81_COMMAND_REGISTER_TRANSMIT_ENABLE;

    RTL81_WRITE_REGISTER8(Device, Rtl81RegisterCommand, CommandRegisterMask);
    CurrentTime = KeGetRecentTimeCounter();
    Timeout = CurrentTime + TimeoutTicks;
    do {
        CommandRegister = RTL81_READ_REGISTER8(Device, Rtl81RegisterCommand);
        if ((CommandRegister & CommandRegisterMask) == CommandRegisterMask) {
            break;
        }

        CurrentTime = KeGetRecentTimeCounter();

    } while (CurrentTime <= Timeout);

    if (CurrentTime > Timeout) {
        Status = STATUS_TIMEOUT;
        goto InitializeEnd;
    }

    //
    // Configure the transmit options. This must happen after transmit has been
    // enabled.
    //

    RTL81_WRITE_REGISTER32(Device,
                           Rtl81RegisterTransmitConfiguration,
                           RTL81_TRANSMIT_CONFIGURATION_DEFAULT_OPTIONS);

    //
    // Configure extra transmit registers for the devices using the newer
    // register set.
    //

    if ((Device->Flags & RTL81_FLAG_TRANSMIT_MODE_LEGACY) == 0) {
        RTL81_WRITE_REGISTER16(Device,
                               Rtl81RegisterEarlyTransmitThreshold,
                               RTL81_EARLY_TRANSMIT_THRESHOLD_DEFAULT);
    }

    //
    // Configure the receive options. This must happend after receive has been
    // enabled. Extra bits are needed for the RTL8139 chip.
    //

    ReceiveConfiguration = RTL81_RECEIVE_CONFIGURATION_DEFAULT_OPTIONS;
    if ((Device->Flags & RTL81_FLAG_TRANSMIT_MODE_LEGACY) != 0) {

        //
        // Note that the no-wrap bit has no effect when using a 64K buffer.
        //

        ReceiveConfiguration |=
                        (RTL81_RECEIVE_CONFIGURATION_DEFAULT_EARLY_THRESHOLD <<
                         RTL81_RECEIVE_CONFIGURATION_EARLY_TRESHOLD_SHIFT);
    }

    Device->ReceiveConfiguration = ReceiveConfiguration;
    RTL81_WRITE_REGISTER32(Device,
                           Rtl81RegisterReceiveConfiguration,
                           ReceiveConfiguration);

    //
    // Set the initial reception filtering, which will be based on the
    // currently enabled capabilities.
    //

    Rtl81pUpdateFilterMode(Device);

    //
    // Configure extra receive registers for non RTL8139 devices.
    //

    if ((Device->Flags & RTL81_FLAG_REGISTER_SET_LEGACY) == 0) {
        RTL81_WRITE_REGISTER16(Device,
                               Rtl81RegisterReceiveMaxPacketSize,
                               RTL81_RECEIVE_BUFFER_DATA_SIZE);
    }

    //
    // Notify the networking core of this new link now that the device is ready
    // to send and receive data, pending media being present.
    //

    Status = Rtl81pAddNetworkDevice(Device);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    //
    // Check to see if this link is up.
    //

    Rtl81pCheckLinkState(Device);

    //
    // Clear any pending interrupts and then enable the desired interrupts.
    //

    RTL81_WRITE_REGISTER16(Device,
                           Rtl81RegisterInterruptStatus,
                           RTL81_DEFAULT_INTERRUPT_MASK);

    RTL81_WRITE_REGISTER16(Device,
                           Rtl81RegisterInterruptMask,
                           RTL81_DEFAULT_INTERRUPT_MASK);

    Status = STATUS_SUCCESS;

InitializeEnd:
    return Status;
}

INTERRUPT_STATUS
Rtl81pInterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the RTL81xx interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the e100 device
        structure.

Return Value:

    Interrupt status.

--*/

{

    PRTL81_DEVICE Device;
    USHORT PendingBits;

    Device = (PRTL81_DEVICE)Context;

    //
    // Read the status register, and if nothing is set then return immediately.
    //

    PendingBits = RTL81_READ_REGISTER16(Device, Rtl81RegisterInterruptStatus);
    PendingBits &= RTL81_DEFAULT_INTERRUPT_MASK;
    if (PendingBits == 0) {
        return InterruptStatusNotClaimed;
    }

    //
    // The RTL81xx devices that use MSIs require interrupts to be disabled and
    // enabled after each interrupt, otherwise the interrupts eventually stop
    // firing. That said, disable and enable the interrupts even if MSIs are
    // not in use.
    //

    RTL81_WRITE_REGISTER16(Device, Rtl81RegisterInterruptMask, 0);
    RTL81_WRITE_REGISTER16(Device, Rtl81RegisterInterruptStatus, PendingBits);
    RTL81_WRITE_REGISTER16(Device,
                           Rtl81RegisterInterruptMask,
                           RTL81_DEFAULT_INTERRUPT_MASK);

    RtlAtomicOr32(&(Device->PendingInterrupts), PendingBits);
    return InterruptStatusClaimed;
}

INTERRUPT_STATUS
Rtl81pInterruptServiceWorker (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine processes interrupts for the RTL81xx controller at low level.

Arguments:

    Parameter - Supplies an optional parameter passed in by the creator of the
        work item.

Return Value:

    Interrupt status.

--*/

{

    PRTL81_DEVICE Device;
    ULONG PendingBits;

    Device = (PRTL81_DEVICE)(Parameter);

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Clear out the pending bits.
    //

    PendingBits = RtlAtomicExchange32(&(Device->PendingInterrupts), 0);
    if (PendingBits == 0) {
        return InterruptStatusNotClaimed;
    }

    //
    // Check to see if the link has changed.
    //

    if ((PendingBits & RTL81_INTERRUPT_LINK_CHANGE) != 0) {
        Rtl81pCheckLinkState(Device);
    }

    //
    // Communicate to the debugger if there were any receive errors.
    //

    if (((PendingBits & RTL81_INTERRUPT_RECEIVE_FIFO_OVERFLOW) != 0) ||
        ((PendingBits & RTL81_INTERRUPT_RECEIVE_ERROR) != 0) ||
        ((PendingBits & RTL81_INTERRUPT_RECEIVE_OVERFLOW) != 0)) {

        RtlDebugPrint("RTL81xx: Receive packet error 0x%x.\n", PendingBits);
    }

    //
    // If a packet was received, process it.
    //

    if ((PendingBits & Device->ReceiveInterruptMask) != 0) {
        Rtl81pReapReceivedFrames(Device);
    }

    //
    // If there was a transmit error or a successful transmit, then go through
    // and reap the packets.
    //

    if ((PendingBits & Device->TransmitInterruptMask) != 0) {
        Rtl81pReapTransmitDescriptors(Device);
    }

    return InterruptStatusClaimed;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
Rtl81pInitializePhy (
    PRTL81_DEVICE Device
    )

/*++

Routine Description:

    This routine initializes the PHY on the RTL81xx.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    USHORT BasicControl;
    ULONGLONG CurrentTime;
    KSTATUS Status;
    ULONGLONG Timeout;
    ULONGLONG TimeoutTicks;
    ULONG Value;

    TimeoutTicks = HlQueryTimeCounterFrequency() * RTL81_DEVICE_TIMEOUT;

    //
    // The RTL8139 based chips access the PHY through the basic mode registers.
    // Complete reset and auto-negotiation using those registers.
    //

    if ((Device->Flags & RTL81_FLAG_REGISTER_SET_LEGACY) != 0) {
        RTL81_WRITE_REGISTER16(Device,
                               Rtl81RegisterBasicModeControl,
                               RTL81_BASIC_MODE_CONTROL_INITIAL_VALUE);

        //
        // According the the RealTek RTL8139C+ datasheet, the reset bit is
        // supposed to be self-clearing. QEMU, however, does not clear the bit.
        // Ignore timeout failures.
        //

        CurrentTime = KeGetRecentTimeCounter();
        Timeout = CurrentTime + TimeoutTicks;
        do {
            BasicControl = RTL81_READ_REGISTER16(Device,
                                                 Rtl81RegisterBasicModeControl);

            if ((BasicControl & RTL81_BASIC_MODE_CONTROL_RESET) == 0) {
                break;
            }

            CurrentTime = KeGetRecentTimeCounter();

        } while (CurrentTime <= Timeout);

    //
    // RTL8168 and above access the PHY through the MII registers. Reset the
    // PHY and then start auto-negotiation.
    //

    } else {
        Status = Rtl81pWriteMdio(Device,
                                 Rtl81MiiRegisterBasicControl,
                                 RTL81_MII_BASIC_CONTROL_RESET);

        if (!KSUCCESS(Status)) {
            goto InitializePhyEnd;
        }

        CurrentTime = KeGetRecentTimeCounter();
        Timeout = CurrentTime + TimeoutTicks;
        do {
            Status = Rtl81pReadMdio(Device,
                                    Rtl81MiiRegisterBasicControl,
                                    &Value);

            if (!KSUCCESS(Status)) {
                goto InitializePhyEnd;
            }

            BasicControl = (USHORT)Value;
            if ((BasicControl & RTL81_MII_BASIC_CONTROL_RESET) == 0) {
                break;
            }

            CurrentTime = KeGetRecentTimeCounter();

        } while (CurrentTime <= Timeout);

        if (CurrentTime > Timeout) {
            Status = STATUS_TIMEOUT;
            goto InitializePhyEnd;
        }

        Status = Rtl81pWriteMdio(Device,
                                 Rtl81MiiRegisterAdvertise,
                                 RTL81_MII_ADVERTISE_ALL);

        if (!KSUCCESS(Status)) {
            goto InitializePhyEnd;
        }

        //
        // The gigabit control register needs to be read, modified, and
        // written as not all bits are advertisement related.
        //

        Status = Rtl81pReadMdio(Device, Rtl81MiiRegisterGigabitControl, &Value);
        if (!KSUCCESS(Status)) {
            goto InitializePhyEnd;
        }

        Value |= (RTL81_MII_GIGABIT_CONTROL_ADVERTISE_1000_FULL |
                  RTL81_MII_GIGABIT_CONTROL_ADVERTISE_1000_HALF);

        Status = Rtl81pWriteMdio(Device, Rtl81MiiRegisterGigabitControl, Value);
        if (!KSUCCESS(Status)) {
            goto InitializePhyEnd;
        }

        Value = RTL81_MII_BASIC_CONTROL_ENABLE_AUTONEGOTIATION |
                RTL81_MII_BASIC_CONTROL_RESTART_AUTONEGOTIATION;

        Status = Rtl81pWriteMdio(Device, Rtl81MiiRegisterBasicControl, Value);
        if (!KSUCCESS(Status)) {
            goto InitializePhyEnd;
        }
    }

    Status = STATUS_SUCCESS;

InitializePhyEnd:
    return Status;
}

VOID
Rtl81pCheckLinkState (
    PRTL81_DEVICE Device
    )

/*++

Routine Description:

    This routine checks the state of the link and will notify the networking
    core if the link is up or down.

Arguments:

    Device - Supplies a pointer to the RTL81xx device.

Return Value:

    None.

--*/

{

    ULONGLONG LinkSpeed;
    BYTE MediaStatus;
    BYTE PhyStatus;

    //
    // The RTL8139 based devices use the media status register.
    //

    if ((Device->Flags & RTL81_FLAG_REGISTER_SET_LEGACY) != 0) {
        MediaStatus = RTL81_READ_REGISTER8(Device, Rtl81RegisterMediaStatus);
        if ((MediaStatus & RTL81_MEDIA_STATUS_SPEED_10) != 0) {
            LinkSpeed = NET_SPEED_10_MBPS;

        } else {
            LinkSpeed = NET_SPEED_100_MBPS;
        }

        if ((MediaStatus & RTL81_MEDIA_STATUS_LINK_DOWN) == 0) {
            NetSetLinkState(Device->NetworkLink, TRUE, LinkSpeed);

        } else {
            NetSetLinkState(Device->NetworkLink, FALSE, LinkSpeed);
        }

    //
    // Otherwise the PHY status register is used.
    //

    } else {
        PhyStatus = RTL81_READ_REGISTER8(Device, Rtl81RegisterPhyStatus);
        if ((PhyStatus & RTL81_PHY_STATUS_SPEED_10) != 0) {
            LinkSpeed = NET_SPEED_10_MBPS;

        } else if ((PhyStatus & RTL81_PHY_STATUS_SPEED_100) != 0) {
            LinkSpeed = NET_SPEED_100_MBPS;

        } else if ((PhyStatus & RTL81_PHY_STATUS_SPEED_1000) != 0) {
            LinkSpeed = NET_SPEED_1000_MBPS;

        } else {

            ASSERT((PhyStatus & RTL81_PHY_STATUS_LINK_UP) == 0);

            LinkSpeed = 0;
        }

        if ((PhyStatus & RTL81_PHY_STATUS_LINK_UP) != 0) {
            NetSetLinkState(Device->NetworkLink, TRUE, LinkSpeed);

        } else {
            NetSetLinkState(Device->NetworkLink, FALSE, LinkSpeed);
        }
    }

    return;
}

VOID
Rtl81pReapTransmitDescriptors (
    PRTL81_DEVICE Device
    )

/*++

Routine Description:

    This routine attempts to reap any transmit descriptors that completed or
    experienced an error. It will send along more data if it released any
    descriptors.

Arguments:

    Device - Supplies a pointer to the RTL81xx device.

Return Value:

    None.

--*/

{

    ULONG Command;
    PRTL81_DEFAULT_DATA DefaultData;
    PRTL81_TRANSMIT_DESCRIPTOR Descriptor;
    BOOL DescriptorReaped;
    LIST_ENTRY DestroyList;
    PRTL81_LEGACY_DATA LegacyData;
    USHORT NextToClean;
    PNET_PACKET_BUFFER Packet;
    RTL81_REGISTER Register;
    ULONG TransmitStatus;

    DescriptorReaped = FALSE;
    INITIALIZE_LIST_HEAD(&DestroyList);
    KeAcquireQueuedLock(Device->TransmitLock);

    //
    // Check all descriptors between the next to clean and the next to use. If
    // the two values are equal and an interrupt came in, it likely means that
    // all descriptors are eligble for reaping, rather than none. Split this
    // logic based on the device type.
    //

    if ((Device->Flags & RTL81_FLAG_TRANSMIT_MODE_LEGACY) != 0) {
        LegacyData = &(Device->U.LegacyData);
        do {
            NextToClean = LegacyData->TransmitNextToClean;

            //
            // If the next descriptor to clean is not in use, then skip
            // cleaning entirely.
            //

            if (LegacyData->ActiveTransmitPackets[NextToClean] == NULL) {
                break;
            }

            Register = Rtl81RegisterTransmitStatus0 +
                       (NextToClean * sizeof(ULONG));

            TransmitStatus = RTL81_READ_REGISTER32(Device, Register);

            //
            // If the transmission was not aborted, completed or underrun, then
            // the descriptor cannot be reaped. Break out of the loop as the
            // descriptors are serviced in round-robin style.
            //

            if (((TransmitStatus & RTL81_TRANSMIT_STATUS_ABORT) == 0) &&
                ((TransmitStatus & RTL81_TRANSMIT_STATUS_OK) == 0) &&
                ((TransmitStatus & RTL81_TRANSMIT_STATUS_FIFO_UNDERRUN) == 0)) {

                break;
            }

            DescriptorReaped = TRUE;
            Packet = LegacyData->ActiveTransmitPackets[NextToClean];
            INSERT_BEFORE(&(Packet->ListEntry), &DestroyList);
            LegacyData->ActiveTransmitPackets[NextToClean] = NULL;

            //
            // Advance to clean the next descriptor.
            //

            LegacyData->TransmitNextToClean += 1;
            if (LegacyData->TransmitNextToClean ==
                RTL81_TRANSMIT_DESCRIPTOR_COUNT_LEGACY) {

                LegacyData->TransmitNextToClean = 0;
            }

        } while (LegacyData->TransmitNextToClean !=
                 LegacyData->TransmitNextToUse);

    } else {
        DefaultData = &(Device->U.DefaultData);
        do {
            NextToClean = DefaultData->TransmitNextToClean;

            //
            // If the next descriptor to clean is not in use, then skip
            // cleaning entirely.
            //

            if (DefaultData->TransmitBuffer[NextToClean] == NULL) {
                break;
            }

            Descriptor = &(DefaultData->TransmitDescriptor[NextToClean]);
            Command = Descriptor->Command;

            //
            // If the hardware still owns the descriptor, then it cannot be
            // reclaimed. Skip the rest of the cleaning.
            //

            if ((Command & RTL81_TRANSMIT_DESCRIPTOR_COMMAND_OWN) != 0) {
                break;
            }

            DescriptorReaped = TRUE;
            Packet = DefaultData->TransmitBuffer[NextToClean];
            INSERT_BEFORE(&(Packet->ListEntry), &DestroyList);
            DefaultData->TransmitBuffer[NextToClean] = NULL;

            //
            // Advance to clean the next descriptor.
            //

            DefaultData->TransmitNextToClean += 1;
            if (DefaultData->TransmitNextToClean ==
                DefaultData->TransmitDescriptorCount) {

                DefaultData->TransmitNextToClean = 0;
            }

        } while (DefaultData->TransmitNextToClean !=
                 DefaultData->TransmitNextToUse);

        //
        // If there are still packets waiting to be sent, then flush them
        // through in case the hardware went idle.
        //

        if (DefaultData->TransmitNextToClean !=
            DefaultData->TransmitNextToUse) {

            if ((Device->Flags & RTL81_FLAG_REGISTER_SET_LEGACY) != 0) {
                RTL81_WRITE_REGISTER8(Device,
                                      Rtl81RegisterTransmitPriorityPolling2,
                                      RTL81_TRANSMIT_PRIORITY_POLLING_NORMAL);

            } else {
                RTL81_WRITE_REGISTER8(Device,
                                      Rtl81RegisterTransmitPriorityPolling1,
                                      RTL81_TRANSMIT_PRIORITY_POLLING_NORMAL);
            }
        }
    }

    //
    // If a descriptor was reaped, then try to pump more packets through.
    //

    if (DescriptorReaped != FALSE) {
        Rtl81pSendPendingPackets(Device);
    }

    KeReleaseQueuedLock(Device->TransmitLock);

    //
    // Destroy any reaped buffers.
    //

    while (LIST_EMPTY(&DestroyList) == FALSE) {
        Packet = LIST_VALUE(DestroyList.Next, NET_PACKET_BUFFER, ListEntry);
        LIST_REMOVE(&(Packet->ListEntry));
        NetFreeBuffer(Packet);
    }

    return;
}

VOID
Rtl81pSendPendingPackets (
    PRTL81_DEVICE Device
    )

/*++

Routine Description:

    This routine sends any pending packets as long as there are free
    descriptors available. This routine assumes that the device's lock is held.

Arguments:

    Device - Supplies a pointer to the RTL81xx device.

Return Value:

    None.

--*/

{

    if ((Device->Flags & RTL81_FLAG_TRANSMIT_MODE_LEGACY) != 0) {
        Rtl81pSendPacketsLegacy(Device);

    } else {
        Rtl81pSendPacketsDefault(Device);
    }

    return;
}

VOID
Rtl81pSendPacketsLegacy (
    PRTL81_DEVICE Device
    )

/*++

Routine Description:

    This routine sends any pending packets for a legacy transmit device as long
    as there are free descriptors available. This routine assumes that the
    device's lock is held.

Arguments:

    Device - Supplies a pointer to the RTL81xx device.

Return Value:

    None.

--*/

{

    PRTL81_LEGACY_DATA LegacyData;
    USHORT NextToUse;
    ULONG Offset;
    PNET_PACKET_BUFFER Packet;
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONG Size;

    ASSERT(KeIsQueuedLockHeld(Device->TransmitLock) != FALSE);
    ASSERT((Device->Flags & RTL81_FLAG_TRANSMIT_MODE_LEGACY) != 0);

    //
    // Iterate over the list of pending transmit packets allocating them to
    // free descriptors, if there are any.
    //

    LegacyData = &(Device->U.LegacyData);
    while (NET_PACKET_LIST_EMPTY(&(Device->TransmitPacketList)) == FALSE) {

        //
        // Get the next descriptor to use. If it is not available, then
        // exit. Otherwise increment the next to use index.
        //

        NextToUse = LegacyData->TransmitNextToUse;
        if (LegacyData->ActiveTransmitPackets[NextToUse] != NULL) {
            break;
        }

        LegacyData->TransmitNextToUse += 1;
        if (LegacyData->TransmitNextToUse ==
            RTL81_TRANSMIT_DESCRIPTOR_COUNT_LEGACY) {

            LegacyData->TransmitNextToUse = 0;
        }

        Packet = LIST_VALUE(Device->TransmitPacketList.Head.Next,
                            NET_PACKET_BUFFER,
                            ListEntry);

        NET_REMOVE_PACKET_FROM_LIST(Packet, &(Device->TransmitPacketList));

        //
        // Remember the packet so that it can be released once it is
        // successfully sent and then begin the transmit process. Setting
        // the size in the status register also sets the OWN bit to 0,
        // triggering the start of the transmission. Thus, the physical
        // address must be programmed first.
        //

        ASSERT(LegacyData->ActiveTransmitPackets[NextToUse] == NULL);

        LegacyData->ActiveTransmitPackets[NextToUse] = Packet;
        Offset = NextToUse * sizeof(ULONG);
        PhysicalAddress = Packet->BufferPhysicalAddress + Packet->DataOffset;

        ASSERT(PhysicalAddress == (ULONG)PhysicalAddress);

        RTL81_WRITE_REGISTER32(Device,
                               (Rtl81RegisterTransmitAddress0 + Offset),
                               (ULONG)PhysicalAddress);

        Size = Packet->FooterOffset - Packet->DataOffset;

        ASSERT(Size <= RTL81_MAX_TRANSMIT_PACKET_SIZE);

        //
        // The RTL8139C does not automatically pad runt packets (less than
        // 64 bytes). The buffer should have been zero'd as this driver
        // registered a minimum packet length with net core. Adjust the size,
        // leaving space for the hardware to fill in the CRC.
        //

        if (Size < (RTL81_MINIMUM_PACKET_LENGTH - sizeof(ULONG))) {

            ASSERT(Packet->BufferSize >= RTL81_MINIMUM_PACKET_LENGTH);

            Size = RTL81_MINIMUM_PACKET_LENGTH - sizeof(ULONG);
        }

        RTL81_WRITE_REGISTER32(Device,
                               (Rtl81RegisterTransmitStatus0 + Offset),
                               Size);
    }

    return;
}

VOID
Rtl81pSendPacketsDefault (
    PRTL81_DEVICE Device
    )

/*++

Routine Description:

    This routine sends any pending packets for a default transmit device as
    long as there are free descriptors available. This routine assumes that the
    device's lock is held.

Arguments:

    Device - Supplies a pointer to the RTL81xx device.

Return Value:

    None.

--*/

{

    ULONG Command;
    PRTL81_DEFAULT_DATA DefaultData;
    PRTL81_TRANSMIT_DESCRIPTOR Descriptor;
    ULONG Flags;
    USHORT NextToUse;
    PNET_PACKET_BUFFER Packet;
    BOOL PacketSubmitted;
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONG Size;
    ULONG VlanTag;

    ASSERT(KeIsQueuedLockHeld(Device->TransmitLock) != FALSE);
    ASSERT((Device->Flags & RTL81_FLAG_TRANSMIT_MODE_LEGACY) == 0);

    //
    // Iterate over the list of pending transmit packets allocating them to
    // free descriptors, if there are any.
    //

    DefaultData = &(Device->U.DefaultData);
    PacketSubmitted = FALSE;
    while (NET_PACKET_LIST_EMPTY(&(Device->TransmitPacketList)) == FALSE) {

        //
        // Get the next descriptor to use. If it is not available, then
        // exit. Otherwise increment the next to use index.
        //

        NextToUse = DefaultData->TransmitNextToUse;
        if (DefaultData->TransmitBuffer[NextToUse] != NULL) {
            break;
        }

        DefaultData->TransmitNextToUse += 1;
        if (DefaultData->TransmitNextToUse ==
            DefaultData->TransmitDescriptorCount) {

            DefaultData->TransmitNextToUse = 0;
        }

        Packet = LIST_VALUE(Device->TransmitPacketList.Head.Next,
                            NET_PACKET_BUFFER,
                            ListEntry);

        NET_REMOVE_PACKET_FROM_LIST(Packet, &(Device->TransmitPacketList));

        //
        // Remember the packet so that it can be released once it is
        // successfully sent and then begin the transmit process.
        //

        ASSERT(DefaultData->TransmitBuffer[NextToUse] == NULL);

        DefaultData->TransmitBuffer[NextToUse] = Packet;

        //
        // Program the descriptor with the packets data.
        //

        Size = Packet->FooterOffset - Packet->DataOffset;

        ASSERT(Size <= RTL81_MAX_TRANSMIT_PACKET_SIZE);

        Command = RTL81_TRANSMIT_DESCRIPTOR_COMMAND_OWN |
                  RTL81_TRANSMIT_DESCRIPTOR_COMMAND_FIRST_SEGMENT |
                  RTL81_TRANSMIT_DESCRIPTOR_COMMAND_LAST_SEGMENT |
                  ((Size << RTL81_TRANSMIT_DESCRIPTOR_COMMAND_SIZE_SHIFT) &
                   RTL81_TRANSMIT_DESCRIPTOR_COMMAND_SIZE_MASK);

        //
        // See if any checksum offloads were requested.
        //

        VlanTag = 0;
        if ((Device->Flags & RTL81_FLAG_CHECKSUM_OFFLOAD_DEFAULT) != 0) {
            Flags = Packet->Flags;
            if ((Flags & NET_PACKET_FLAG_IP_CHECKSUM_OFFLOAD) != 0) {
                Command |=
                         RTL81_TRANSMIT_DESCRIPTOR_COMMAND_IP_CHECKSUM_OFFLOAD;
            }

            if ((Flags & NET_PACKET_FLAG_UDP_CHECKSUM_OFFLOAD) != 0) {
                Command |=
                        RTL81_TRANSMIT_DESCRIPTOR_COMMAND_UDP_CHECKSUM_OFFLOAD;

            } else if ((Flags & NET_PACKET_FLAG_TCP_CHECKSUM_OFFLOAD) != 0) {
                Command |=
                        RTL81_TRANSMIT_DESCRIPTOR_COMMAND_TCP_CHECKSUM_OFFLOAD;
            }

        } else if ((Device->Flags & RTL81_FLAG_CHECKSUM_OFFLOAD_VLAN) != 0) {
            Flags = Packet->Flags;
            if ((Flags & NET_PACKET_FLAG_IP_CHECKSUM_OFFLOAD) != 0) {
                VlanTag |= RTL81_TRANSMIT_DESCRIPTOR_VLAN_IP_CHECKSUM_OFFLOAD;
            }

            if ((Flags & NET_PACKET_FLAG_UDP_CHECKSUM_OFFLOAD) != 0) {
                VlanTag |= RTL81_TRANSMIT_DESCRIPTOR_VLAN_UDP_CHECKSUM_OFFLOAD;

            } else if ((Flags & NET_PACKET_FLAG_TCP_CHECKSUM_OFFLOAD) != 0) {
                VlanTag |= RTL81_TRANSMIT_DESCRIPTOR_VLAN_TCP_CHECKSUM_OFFLOAD;
            }
        }

        if (NextToUse == (DefaultData->TransmitDescriptorCount - 1)) {
            Command |= RTL81_TRANSMIT_DESCRIPTOR_COMMAND_END_OF_RING;
        }

        PhysicalAddress = Packet->BufferPhysicalAddress +
                          Packet->DataOffset;

        Descriptor = &(DefaultData->TransmitDescriptor[NextToUse]);
        Descriptor->VlanTag = VlanTag;
        Descriptor->PhysicalAddress = PhysicalAddress;
        RtlMemoryBarrier();
        Descriptor->Command = Command;
        PacketSubmitted = TRUE;
    }

    //
    // If a packet was submitted by setting its state in a descriptor, then
    // poke the hardware to let it know to check the transmit queue.
    //

    if (PacketSubmitted != FALSE) {
        if ((Device->Flags & RTL81_FLAG_REGISTER_SET_LEGACY) != 0) {
            RTL81_WRITE_REGISTER8(Device,
                                  Rtl81RegisterTransmitPriorityPolling2,
                                  RTL81_TRANSMIT_PRIORITY_POLLING_NORMAL);

        } else {
            RTL81_WRITE_REGISTER8(Device,
                                  Rtl81RegisterTransmitPriorityPolling1,
                                  RTL81_TRANSMIT_PRIORITY_POLLING_NORMAL);
        }
    }

    return;
}

VOID
Rtl81pReapReceivedFrames (
    PRTL81_DEVICE Device
    )

/*++

Routine Description:

    This routine reaps received frames from RTL81xx hardware and notifies the
    core networking driver about a packet's arrival.

Arguments:

    Device - Supplies a pointer to the RTL81xx device.

Return Value:

    None.

--*/

{

    KeAcquireQueuedLock(Device->ReceiveLock);

    //
    // Handle the reaping based on the device type. RTL8139 is different than
    // everything else.
    //

    if ((Device->Flags & RTL81_FLAG_TRANSMIT_MODE_LEGACY) != 0) {
        Rtl81pReapReceivedFramesLegacy(Device);

    } else {
        Rtl81pReapReceivedFramesDefault(Device);
    }

    KeReleaseQueuedLock(Device->ReceiveLock);
    return;
}

VOID
Rtl81pReapReceivedFramesLegacy (
    PRTL81_DEVICE Device
    )

/*++

Routine Description:

    This routine reaps received frames from the legacy RTL receive ring buffer
    and notifies the core networking driver about a packet's arrival.

Arguments:

    Device - Supplies a pointer to the RTL81xx device.

Return Value:

    None.

--*/

{

    USHORT AlignedOffset;
    USHORT BytesReaped;
    BYTE CommandRegister;
    ULONG CurrentOffset;
    BYTE EarlyStatus;
    USHORT EndOffset;
    PIO_BUFFER_FRAGMENT Fragment;
    PRTL81_PACKET_HEADER Header;
    PRTL81_LEGACY_DATA LegacyData;
    USHORT MaxBytesToReap;
    NET_PACKET_BUFFER Packet;
    USHORT PacketLength;
    PHYSICAL_ADDRESS PhysicalAddress;
    USHORT ReadPacketAddress;
    PVOID VirtualAddress;
    USHORT WrapLength;
    USHORT WrapOffset;

    ASSERT((Device->Flags & RTL81_FLAG_TRANSMIT_MODE_LEGACY) != 0);

    LegacyData = &(Device->U.LegacyData);
    Packet.IoBuffer = NULL;
    Packet.ListEntry.Next = NULL;
    Packet.Flags = 0;

    //
    // Get the current read offset and the hardware's write offset.
    //

    CurrentOffset = RTL81_READ_REGISTER16(Device,
                                          Rtl81RegisterReadPacketAddress);

    CurrentOffset += RTL81_RECEIVE_OFFSET_ADJUSTMENT;
    if (CurrentOffset >= RTL81_MAXIMUM_RECEIVE_RING_BUFFER_OFFSET) {
        CurrentOffset -= RTL81_MAXIMUM_RECEIVE_RING_BUFFER_OFFSET;
    }

    EndOffset = RTL81_READ_REGISTER16(Device,
                                      Rtl81RegisterReceiveBufferCurrent);

    ASSERT(EndOffset < RTL81_MAXIMUM_RECEIVE_RING_BUFFER_OFFSET);

    //
    // Figure how many good bytes are available to process, accounting for
    // the wrap around.
    //

    if (EndOffset > CurrentOffset) {
        MaxBytesToReap = EndOffset - CurrentOffset;

    } else {
        MaxBytesToReap = (RTL81_MAXIMUM_RECEIVE_RING_BUFFER_OFFSET -
                          CurrentOffset) +
                         EndOffset;
    }

    Fragment = &(LegacyData->ReceiveIoBuffer->Fragment[0]);
    VirtualAddress = Fragment->VirtualAddress;
    PhysicalAddress = Fragment->PhysicalAddress;

    //
    // Loop until the buffer is empty according to the command register or
    // until the maximum bytes have been reaped.
    //

    BytesReaped = 0;
    CommandRegister = RTL81_READ_REGISTER8(Device, Rtl81RegisterCommand);
    while ((CommandRegister & RTL81_COMMAND_REGISTER_BUFFER_EMPTY) == 0) {
        Header = (PRTL81_PACKET_HEADER)(VirtualAddress + CurrentOffset);

        //
        // If the packet is early or there was an error, break out of the loop.
        //

        if (((Header->Status & RTL81_RECEIVE_PACKET_STATUS_OK) == 0) ||
            ((Header->Status & RTL81_RECEIVE_PACKET_ERROR_MASK) != 0) ||
            (Header->Length > RTL81_MAXIMUM_PACKET_LENGTH) ||
            (Header->Length < RTL81_MINIMUM_PACKET_LENGTH)) {

            EarlyStatus = RTL81_READ_REGISTER8(Device,
                                               Rtl81RegisterEarlyReceiveStatus);

            if ((EarlyStatus & RTL81_EARLY_RECEIVE_STATUS_OK) != 0) {
                break;
            }

            CommandRegister = RTL81_READ_REGISTER8(Device,
                                                   Rtl81RegisterCommand);

            CommandRegister &= ~RTL81_COMMAND_REGISTER_RECEIVE_ENABLE;
            RTL81_WRITE_REGISTER8(Device,
                                  Rtl81RegisterCommand,
                                  CommandRegister);

            CommandRegister |= RTL81_COMMAND_REGISTER_RECEIVE_ENABLE;
            RTL81_WRITE_REGISTER8(Device,
                                  Rtl81RegisterCommand,
                                  CommandRegister);

            //
            // Updates to the devices receive configuration field and changes
            // to the register must be synchronized.
            //

            KeAcquireQueuedLock(Device->ConfigurationLock);
            RTL81_WRITE_REGISTER32(Device,
                                   Rtl81RegisterReceiveConfiguration,
                                   Device->ReceiveConfiguration);

            KeReleaseQueuedLock(Device->ConfigurationLock);
            ReadPacketAddress = RTL81_MAXIMUM_RECEIVE_RING_BUFFER_OFFSET -
                                RTL81_RECEIVE_OFFSET_ADJUSTMENT;

            RTL81_WRITE_REGISTER16(Device,
                                   Rtl81RegisterReadPacketAddress,
                                   ReadPacketAddress);

            break;
        }

        //
        // The header indicated a valid packet, try to count these as reaped
        // bytes. If these bytes extend beyond the pre-calculated total, exit
        // the loop now, they packet likely isn't ready.
        //

        BytesReaped += sizeof(RTL81_PACKET_HEADER) + Header->Length;
        if (BytesReaped > MaxBytesToReap) {
            break;
        }

        //
        // Create a network buffer packet to send to the networking core. Get
        // the offset of the actual data by skipping over the header. Wrap
        // around to zero if the current offset went beyond the end.
        //

        CurrentOffset += sizeof(RTL81_PACKET_HEADER);
        if (CurrentOffset >= RTL81_MAXIMUM_RECEIVE_RING_BUFFER_OFFSET) {
            CurrentOffset -= RTL81_MAXIMUM_RECEIVE_RING_BUFFER_OFFSET;
        }

        //
        // Remove the size of the CRC from the length.
        //

        PacketLength = Header->Length - RTL81_RECEIVE_CRC_LENGTH;

        //
        // Extra space was left at the end of the receive I/O buffer to handle
        // wrapping, so copy any wrapped data to the end of the buffer. Move
        // the current offset forward, accounting for the CRC.
        //

        ASSERT(CurrentOffset < RTL81_MAXIMUM_RECEIVE_RING_BUFFER_OFFSET);

        Packet.Buffer = VirtualAddress + CurrentOffset;
        Packet.BufferPhysicalAddress = PhysicalAddress + CurrentOffset;
        WrapOffset = RTL81_MAXIMUM_RECEIVE_RING_BUFFER_OFFSET - CurrentOffset;
        if (PacketLength > WrapOffset) {
            WrapLength = PacketLength - WrapOffset;
            RtlCopyMemory(Packet.Buffer + WrapOffset,
                          VirtualAddress,
                          WrapLength);

            CurrentOffset = WrapLength + RTL81_RECEIVE_CRC_LENGTH;

        } else {
            CurrentOffset += PacketLength + RTL81_RECEIVE_CRC_LENGTH;
        }

        if (CurrentOffset >= RTL81_MAXIMUM_RECEIVE_RING_BUFFER_OFFSET) {
            CurrentOffset -= RTL81_MAXIMUM_RECEIVE_RING_BUFFER_OFFSET;
        }

        //
        // Remove the size of the CRC from the length.
        //

        Packet.BufferSize = PacketLength;
        Packet.DataSize = PacketLength;
        Packet.DataOffset = 0;
        Packet.FooterOffset = PacketLength;
        NetProcessReceivedPacket(Device->NetworkLink, &Packet);

        //
        // Move past this packet. The current offset is set to the end of the
        // CRC. Just align it up and then notify the hardware. Count these as
        // bytes reaped.
        //

        AlignedOffset = ALIGN_RANGE_UP(CurrentOffset,
                                       RTL81_RECEIVE_RING_BUFFER_ALIGNMENT);

        if (AlignedOffset >= RTL81_MAXIMUM_RECEIVE_RING_BUFFER_OFFSET) {
            AlignedOffset = 0;
            BytesReaped += RTL81_MAXIMUM_RECEIVE_RING_BUFFER_OFFSET -
                           CurrentOffset;

        } else {
            BytesReaped += AlignedOffset - CurrentOffset;
        }

        CurrentOffset = AlignedOffset;
        if (CurrentOffset >= RTL81_MAXIMUM_RECEIVE_RING_BUFFER_OFFSET) {
            CurrentOffset -= RTL81_MAXIMUM_RECEIVE_RING_BUFFER_OFFSET;
        }

        //
        // Don't update the current offset with the adjustment as the next
        // packet should be sitting at the offset before the adjustment.
        //

        ReadPacketAddress = CurrentOffset - RTL81_RECEIVE_OFFSET_ADJUSTMENT;
        RTL81_WRITE_REGISTER16(Device,
                               Rtl81RegisterReadPacketAddress,
                               ReadPacketAddress);

        //
        // Update the command register status now that a packet has been
        // processed. The buffer may be empty.
        //

        CommandRegister = RTL81_READ_REGISTER8(Device, Rtl81RegisterCommand);
    }

    return;
}

VOID
Rtl81pReapReceivedFramesDefault (
    PRTL81_DEVICE Device
    )

/*++

Routine Description:

    This routine reaps received frames from receive descriptors of the newer
    RTL8139C+, RTL8168, and similar chips. It then notifies the core networking
    driver about a packet's arrival.

Arguments:

    Device - Supplies a pointer to the RTL81xx device.

Return Value:

    None.

--*/

{

    ULONG Command;
    PRTL81_DEFAULT_DATA DefaultData;
    PRTL81_RECEIVE_DESCRIPTOR Descriptor;
    ULONG Flags;
    USHORT NextToReap;
    NET_PACKET_BUFFER Packet;
    ULONG Protocol;
    ULONG SegmentFlags;
    ULONG Size;
    ULONG VlanTag;

    ASSERT((Device->Flags & RTL81_FLAG_TRANSMIT_MODE_LEGACY) == 0);

    Packet.IoBuffer = NULL;
    Packet.ListEntry.Next = NULL;
    Descriptor = NULL;
    DefaultData = &(Device->U.DefaultData);
    SegmentFlags = RTL81_RECEIVE_DESCRIPTOR_COMMAND_FIRST_SEGMENT |
                   RTL81_RECEIVE_DESCRIPTOR_COMMAND_LAST_SEGMENT;

    while (TRUE) {

        //
        // If this isn't the first time around, advance the next to index and
        // reset the current descriptor.
        //

        if (Descriptor != NULL) {
            Command = RTL81_RECEIVE_DESCRIPTOR_DEFAULT_COMMAND;
            DefaultData->ReceiveNextToReap += 1;
            if (DefaultData->ReceiveNextToReap ==
                DefaultData->ReceiveDescriptorCount) {

                Command |= RTL81_RECEIVE_DESCRIPTOR_COMMAND_END_OF_RING;
                DefaultData->ReceiveNextToReap = 0;
            }

            Descriptor->Command = Command;
        }

        //
        // Try to harvest the packet in te next descriptor.
        //

        NextToReap = DefaultData->ReceiveNextToReap;
        Descriptor = &(DefaultData->ReceiveDescriptor[NextToReap]);

        //
        // If the descriptor is still in use by the hardware, then stop.
        //

        Command = Descriptor->Command;
        if ((Command & RTL81_RECEIVE_DESCRIPTOR_COMMAND_OWN) != 0) {
            break;
        }

        //
        // Rtl8168C and above do not support multi-segment packets. Discard
        // such packets.
        //

        if (((Device->Flags & RTL81_FLAG_MULTI_SEGMENT_SUPPORT) == 0) &&
            ((Command & SegmentFlags) != SegmentFlags)) {

            continue;
        }

        //
        // This is a valid packet that needs to be reaped. Only single
        // packets are supported.
        //

        ASSERT((Command & SegmentFlags) == SegmentFlags);

        //
        // The command bits differ between the RTL8139C+ and newer chips.
        // Handle that now.
        //

        if ((Device->Flags & RTL81_FLAG_RECEIVE_COMMAND_LEGACY) != 0) {
            Size = (Command & RTL81_RECEIVE_DESCRIPTOR_COMMAND_SIZE_MASK) >>
                   RTL81_RECEIVE_DESCRIPTOR_COMMAND_SIZE_SHIFT;

        } else {
            Size = (Command &
                    RTL81_RECEIVE_DESCRIPTOR_COMMAND_LARGE_SIZE_MASK) >>
                   RTL81_RECEIVE_DESCRIPTOR_COMMAND_LARGE_SIZE_SHIFT;

            //
            // With the size and top four bits out of the way, modify the
            // command variable so that the values match those of the older
            // model.
            //

            Command >>= RTL81_RECEIVE_DESCRIPTOR_COMMAND_SHIFT;
        }

        //
        // Skip the packet if any error flags are set.
        //

        if ((Command & RTL81_RECEIVE_DESCRIPTOR_COMMAND_ERROR_SUMMARY) != 0) {
            continue;
        }

        //
        // Collect the checksum flags, passing the packet to the networking
        // core even if the checksum failed.
        //

        Flags = 0;
        Protocol = (Command & RTL81_RECEIVE_DESCRIPTOR_COMMAND_PROTOCOL_MASK) >>
                   RTL81_RECEIVE_DESCRIPTOR_COMMAND_PROTOCOL_SHIFT;

        if (Protocol != 0) {
            VlanTag = Descriptor->VlanTag;
            if (((Device->Flags & RTL81_FLAG_CHECKSUM_OFFLOAD_VLAN) == 0) ||
                ((VlanTag & RTL81_RECEIVE_DESCRIPTOR_VLAN_IP4) != 0)) {

                Flags |= NET_PACKET_FLAG_IP_CHECKSUM_OFFLOAD;
                if (RTL81_RECEIVE_IP_CHECKSUM_FAILURE(Command) != FALSE) {
                    Flags |= NET_PACKET_FLAG_IP_CHECKSUM_FAILED;
                }
            }

            if (Protocol == RTL81_RECEIVE_DESCRIPTOR_COMMAND_PROTOCOL_UDP_IP) {
                Flags |= NET_PACKET_FLAG_UDP_CHECKSUM_OFFLOAD;
                if (RTL81_RECEIVE_UDP_CHECKSUM_FAILURE(Command) != FALSE) {
                    Flags |= NET_PACKET_FLAG_UDP_CHECKSUM_FAILED;
                }

            } else if (Protocol ==
                       RTL81_RECEIVE_DESCRIPTOR_COMMAND_PROTOCOL_TCP_IP) {

                Flags |= NET_PACKET_FLAG_TCP_CHECKSUM_OFFLOAD;
                if (RTL81_RECEIVE_TCP_CHECKSUM_FAILURE(Command) != FALSE) {
                    Flags |= NET_PACKET_FLAG_TCP_CHECKSUM_FAILED;
                }
            }
        }

        Packet.Buffer = DefaultData->ReceivePacketData +
                        (NextToReap * RTL81_RECEIVE_BUFFER_DATA_SIZE);

        Packet.BufferPhysicalAddress = Descriptor->PhysicalAddress;
        Packet.Flags = Flags;

        //
        // Discard the CRC from the size.
        //

        Size -= RTL81_RECEIVE_CRC_LENGTH;
        Packet.BufferSize = Size;
        Packet.DataSize = Size;
        Packet.DataOffset = 0;
        Packet.FooterOffset = Size;
        NetProcessReceivedPacket(Device->NetworkLink, &Packet);
    }

    return;
}

VOID
Rtl81pUpdateFilterMode (
    PRTL81_DEVICE Device
    )

/*++

Routine Description:

    This routine updates an RTL81xx device's filter mode based on the currently
    enabled capabilities.

Arguments:

    Device - Supplies a pointer to the RTL81xx device.

Return Value:

    None.

--*/

{

    ULONG Configuration;
    ULONG Multicast[2];

    Configuration = RTL81_READ_REGISTER32(Device,
                                          Rtl81RegisterReceiveConfiguration);

    //
    // Broadcast packets and packets whose destination MAC address matches the
    // local address are always accepted.
    //

    Configuration |= RTL81_RECEIVE_CONFIGURATION_ACCEPT_BROADCAST_PACKETS |
                     RTL81_RECEIVE_CONFIGURATION_ACCEPT_PHYSICAL_MATCH_PACKETS;

    if ((Device->EnabledCapabilities &
         NET_LINK_CAPABILITY_PROMISCUOUS_MODE) != 0) {

        Configuration |=
                       RTL81_RECEIVE_CONFIGURATION_ACCEPT_MULTICAST_PACKETS |
                       RTL81_RECEIVE_CONFIGURATION_ACCEPT_ALL_PHYSICAL_PACKETS;

        Multicast[0] = 0xFFFFFFFF;
        Multicast[1] = 0xFFFFFFFF;

    } else {
        Configuration &=
                    ~(RTL81_RECEIVE_CONFIGURATION_ACCEPT_MULTICAST_PACKETS |
                      RTL81_RECEIVE_CONFIGURATION_ACCEPT_ALL_PHYSICAL_PACKETS);

        Multicast[0] = 0;
        Multicast[1] = 0;
    }

    Device->ReceiveConfiguration = Configuration;
    RTL81_WRITE_REGISTER32(Device,
                           Rtl81RegisterReceiveConfiguration,
                           Configuration);

    RTL81_WRITE_REGISTER32(Device,
                           Rtl81RegisterMulticast0,
                           Multicast[0]);

    RTL81_WRITE_REGISTER32(Device,
                           Rtl81RegisterMulticast4,
                           Multicast[1]);

    return;
}

KSTATUS
Rtl81pReadMacAddress (
    PRTL81_DEVICE Device
    )

/*++

Routine Description:

    This routine reads the MAC address out of the EEPROM on the RTL81xx. The
    MAC address will be stored in the device structure.

Arguments:

    Device - Supplies a pointer to the RTL81xx device.

Return Value:

    Status code.

--*/

{

    RTL81_REGISTER AddressRegister;
    ULONG Index;
    USHORT MacValue;

    //
    // The MAC address is in the individual address registers. There is one
    // byte in each but two can be read at a time as they are sequential
    // registers.
    //

    AddressRegister = Rtl81RegisterId0;
    for (Index = 0; Index < sizeof(Device->MacAddress); Index += 2) {
        MacValue = RTL81_READ_REGISTER16(Device, AddressRegister);
        Device->MacAddress[Index] = (BYTE)MacValue;
        Device->MacAddress[Index + 1] = (BYTE)(MacValue >> BITS_PER_BYTE);
        AddressRegister += 2;
    }

    //
    // Check to determine if this is a valid MAC address.
    //

    if (NetIsEthernetAddressValid(Device->MacAddress) == FALSE) {
        return STATUS_INVALID_ADDRESS;
    }

    return STATUS_SUCCESS;
}

KSTATUS
Rtl81pReadMdio (
    PRTL81_DEVICE Device,
    RTL81_MII_REGISTER Register,
    PULONG Data
    )

/*++

Routine Description:

    This routine performs an MDIO register read.

Arguments:

    Device - Supplies a pointer to the device.

    Register - Supplies the register to read.

    Data - Supplies a pointer where the register contents will be returned on
        success.

Return Value:

    Status code.

--*/

{

    ULONGLONG CurrentTime;
    ULONG RegisterValue;
    ULONGLONG Timeout;
    ULONGLONG TimeoutTicks;

    TimeoutTicks = HlQueryTimeCounterFrequency() * RTL81_DEVICE_TIMEOUT;

    ASSERT(Register < Rtl81MiiRegisterMax);

    RegisterValue = RTL81_MII_ACCESS_READ |
                    ((Register << RTL81_MII_ACCESS_REGISTER_SHIFT) &
                     RTL81_MII_ACCESS_REGISTER_MASK);

    RTL81_WRITE_REGISTER32(Device, Rtl81RegisterMiiAccess, RegisterValue);
    CurrentTime = KeGetRecentTimeCounter();
    Timeout = CurrentTime + TimeoutTicks;
    do {
        KeDelayExecution(FALSE, FALSE, 100);
        RegisterValue = RTL81_READ_REGISTER32(Device, Rtl81RegisterMiiAccess);
        if ((RegisterValue & RTL81_MII_ACCESS_COMPLETE_MASK) ==
            RTL81_MII_ACCESS_READ_COMPLETE) {

            *Data = (RegisterValue & RTL81_MII_ACCESS_DATA_MASK) >>
                    RTL81_MII_ACCESS_DATA_SHIFT;

            KeDelayExecution(FALSE, FALSE, 20);
            break;
        }

        CurrentTime = KeGetRecentTimeCounter();

    } while (CurrentTime <= Timeout);

    if (CurrentTime > Timeout) {
        return STATUS_TIMEOUT;
    }

    return STATUS_SUCCESS;
}

KSTATUS
Rtl81pWriteMdio (
    PRTL81_DEVICE Device,
    RTL81_MII_REGISTER Register,
    ULONG Data
    )

/*++

Routine Description:

    This routine performs an MDIO register write.

Arguments:

    Device - Supplies a pointer to the device.

    Register - Supplies the register to read.

    Data - Supplies the data to write to the register.

Return Value:

    Status code.

--*/

{

    ULONGLONG CurrentTime;
    ULONG RegisterValue;
    ULONGLONG Timeout;
    ULONGLONG TimeoutTicks;

    TimeoutTicks = HlQueryTimeCounterFrequency() * RTL81_DEVICE_TIMEOUT;

    ASSERT(Register < Rtl81MiiRegisterMax);
    ASSERT((Data & ~RTL81_MII_ACCESS_DATA_MASK) == 0);

    RegisterValue = RTL81_MII_ACCESS_WRITE |
                    ((Register << RTL81_MII_ACCESS_REGISTER_SHIFT) &
                     RTL81_MII_ACCESS_REGISTER_MASK) |
                    Data;

    RTL81_WRITE_REGISTER32(Device, Rtl81RegisterMiiAccess, RegisterValue);
    CurrentTime = KeGetRecentTimeCounter();
    Timeout = CurrentTime + TimeoutTicks;
    do {
        KeDelayExecution(FALSE, FALSE, 100);
        RegisterValue = RTL81_READ_REGISTER32(Device, Rtl81RegisterMiiAccess);
        if ((RegisterValue & RTL81_MII_ACCESS_COMPLETE_MASK) ==
            RTL81_MII_ACCESS_WRITE_COMPLETE) {

            KeDelayExecution(FALSE, FALSE, 20);
            break;
        }

        CurrentTime = KeGetRecentTimeCounter();

    } while (CurrentTime <= Timeout);

    if (CurrentTime > Timeout) {
        return STATUS_TIMEOUT;
    }

    return STATUS_SUCCESS;
}

