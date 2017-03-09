/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    smsc95hw.c

Abstract:

    This module implements device support for the SMSC95xx family of USB
    Ethernet Controllers.

Author:

    Evan Green 7-Nov-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/net/netdrv.h>
#include <minoca/usb/usb.h>
#include "smsc95.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum number of bulk out transfers that are allowed to be
// submitted to USB at one time.
//

#define SM95_MAX_BULK_OUT_TRANSFER_COUNT 64

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines an SM95xx bulk out transfer. These transfers are
    allocated on demand and recycled when complete.

Members:

    ListEntry - Stores a pointer to the next and previous bulk out transfer
        on the devices free transfer list.

    Device - Stores a pointer to the SM95 device that owns the transfer.

    UsbTransfer - Stores a pointer to the USB transfer that belongs to this
        SM95 transfer for the duration of its existence.

    Packet - Stores a pointer to the network packet buffer whose data is being
        sent by the USB transfer.

--*/

typedef struct _SM95_BULK_OUT_TRANSFER {
    LIST_ENTRY ListEntry;
    PSM95_DEVICE Device;
    PUSB_TRANSFER UsbTransfer;
    PNET_PACKET_BUFFER Packet;
} SM95_BULK_OUT_TRANSFER, *PSM95_BULK_OUT_TRANSFER;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
Sm95pTransmitPacketCompletion (
    PUSB_TRANSFER Transfer
    );

KSTATUS
Sm95pEnableMac (
    PSM95_DEVICE Device
    );

KSTATUS
Sm95pUpdateFilterMode (
    PSM95_DEVICE Device
    );

KSTATUS
Sm95pSetupChecksumOffloading (
    PSM95_DEVICE Device,
    BOOL EnableTransmitChecksumOffload,
    BOOL EnableReceiveChecksumOffload
    );

KSTATUS
Sm95pSetMacAddress (
    PSM95_DEVICE Device,
    BYTE Address[ETHERNET_ADDRESS_SIZE]
    );

KSTATUS
Sm95pReadMacAddress (
    PSM95_DEVICE Device
    );

KSTATUS
Sm95pInitializePhy (
    PSM95_DEVICE Device
    );

KSTATUS
Sm95pRestartNway (
    PSM95_DEVICE Device
    );

KSTATUS
Sm95pReadEeprom (
    PSM95_DEVICE Device,
    ULONG Offset,
    ULONG Length,
    PBYTE Data
    );

KSTATUS
Sm95pWaitForEeprom (
    PSM95_DEVICE Device,
    BOOL ObserveEepromTimeout
    );

KSTATUS
Sm95pWriteMdio (
    PSM95_DEVICE Device,
    USHORT PhyId,
    USHORT Index,
    ULONG Data
    );

KSTATUS
Sm95pReadMdio (
    PSM95_DEVICE Device,
    USHORT PhyId,
    USHORT Index,
    PULONG Data
    );

KSTATUS
Sm95pWaitForPhy (
    PSM95_DEVICE Device
    );

KSTATUS
Sm95pWriteRegister (
    PSM95_DEVICE Device,
    USHORT Register,
    ULONG Data
    );

KSTATUS
Sm95pReadRegister (
    PSM95_DEVICE Device,
    USHORT Register,
    PULONG Data
    );

KSTATUS
Sm95pSubmitBulkInTransfers (
    PSM95_DEVICE Device
    );

VOID
Sm95pCancelBulkInTransfers (
    PSM95_DEVICE Device
    );

PSM95_BULK_OUT_TRANSFER
Sm95pAllocateBulkOutTransfer (
    PSM95_DEVICE Device
    );

VOID
Sm95pFreeBulkOutTransfer (
    PSM95_BULK_OUT_TRANSFER Transfer
    );

//
// -------------------------------------------------------------------- Globals
//

BOOL Sm95DisablePacketDropping = FALSE;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
Sm95Send (
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

    ULONG DataSize;
    PSM95_DEVICE Device;
    PULONG Header;
    PNET_PACKET_BUFFER Packet;
    PSM95_BULK_OUT_TRANSFER Sm95Transfer;
    KSTATUS Status;
    PUSB_TRANSFER UsbTransfer;

    Device = (PSM95_DEVICE)DeviceContext;

    //
    // If there are more bulk out transfers in transit that allowed, drop all
    // of these packets.
    //

    if ((Device->BulkOutTransferCount >= SM95_MAX_BULK_OUT_TRANSFER_COUNT) &&
        (Sm95DisablePacketDropping == FALSE)) {

        return STATUS_RESOURCE_IN_USE;
    }

    //
    // Otherwise submit all the packets. This may stretch over the maximum
    // number of bulk out transfers, but it's a flexible line.
    //

    while (NET_PACKET_LIST_EMPTY(PacketList) == FALSE) {
        Packet = LIST_VALUE(PacketList->Head.Next,
                            NET_PACKET_BUFFER,
                            ListEntry);

        NET_REMOVE_PACKET_FROM_LIST(Packet, PacketList);

        ASSERT(IS_ALIGNED(Packet->BufferSize, MmGetIoBufferAlignment()) !=
               FALSE);

        ASSERT(IS_ALIGNED((UINTN)Packet->Buffer,
                          MmGetIoBufferAlignment()) != FALSE);

        ASSERT(IS_ALIGNED((UINTN)Packet->BufferPhysicalAddress,
                          MmGetIoBufferAlignment()) != FALSE);

        //
        // There might be legitimate reasons for this assert to be spurious,
        // but most likely this assert fired because something in the
        // networking stack failed to properly allocate the required header
        // space. Go figure out who allocated this packet.
        //

        ASSERT(Packet->DataOffset == SM95_TRANSMIT_HEADER_SIZE);

        DataSize = Packet->FooterOffset - Packet->DataOffset;
        Packet->DataOffset -= SM95_TRANSMIT_HEADER_SIZE;
        Header = Packet->Buffer;
        *Header = DataSize |
                  SM95_TRANSMIT_FLAG_FIRST_SEGMENT |
                  SM95_TRANSMIT_FLAG_LAST_SEGMENT;

        *(Header + 1) = DataSize;

        //
        // Allocate a transfer for this packet. All packets need to be dealt
        // with, so if the allocation or submission fails then free the buffer.
        //

        Sm95Transfer = Sm95pAllocateBulkOutTransfer(Device);
        if (Sm95Transfer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            RtlDebugPrint("SM95: Failed to allocate transfer.\n");
            NetFreeBuffer(Packet);
            break;
        }

        Sm95Transfer->Packet = Packet;
        UsbTransfer = Sm95Transfer->UsbTransfer;
        UsbTransfer->Length = Packet->FooterOffset;
        UsbTransfer->BufferActualLength = Packet->BufferSize;
        UsbTransfer->Buffer = Header;
        UsbTransfer->BufferPhysicalAddress = Packet->BufferPhysicalAddress;
        RtlAtomicAdd32(&(Device->BulkOutTransferCount), 1);
        Status = UsbSubmitTransfer(UsbTransfer);
        if (!KSUCCESS(Status)) {
            RtlDebugPrint("SM95: Failed to submit transmit packet: %d\n",
                          Status);

            Sm95Transfer->Packet = NULL;
            Sm95pFreeBulkOutTransfer(Sm95Transfer);
            NetFreeBuffer(Packet);
            RtlAtomicAdd32(&(Device->BulkOutTransferCount), -1);
            break;
        }
    }

    return Status;
}

KSTATUS
Sm95GetSetInformation (
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
    PSM95_DEVICE Device;
    PULONG Flags;
    ULONG NewCapabilities;
    ULONG OriginalCapabilities;
    KSTATUS Status;

    Status = STATUS_SUCCESS;
    Device = (PSM95_DEVICE)DeviceContext;
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
        NewCapabilities = Device->EnabledCapabilities;
        if (*BooleanOption != FALSE) {
            NewCapabilities |= NET_LINK_CAPABILITY_PROMISCUOUS_MODE;

        } else {
            NewCapabilities &= ~NET_LINK_CAPABILITY_PROMISCUOUS_MODE;
        }

        if ((NewCapabilities ^ Device->EnabledCapabilities) != 0) {
            OriginalCapabilities = Device->EnabledCapabilities;
            Device->EnabledCapabilities = NewCapabilities;
            Status = Sm95pUpdateFilterMode(Device);
            if (!KSUCCESS(Status)) {
                Device->EnabledCapabilities = OriginalCapabilities;
            }
        }

        KeReleaseQueuedLock(Device->ConfigurationLock);
        break;

    default:
        Status = STATUS_NOT_SUPPORTED;
        break;
    }

    return Status;
}

VOID
Sm95InterruptTransferCompletion (
    PUSB_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine is called when the interrupt transfer returns. It processes
    the notification from the device.

Arguments:

    Transfer - Supplies a pointer to the transfer that completed.

Return Value:

    None.

--*/

{

    PSM95_DEVICE Device;
    BOOL OriginalLinkUp;
    ULONG Source;
    KSTATUS Status;
    ULONG Value;

    Device = Transfer->UserData;

    ASSERT(Transfer == Device->InterruptTransfer);

    //
    // If the transfer failed, don't bother with the data. If it was cancelled,
    // just exit immediately. The device was likely removed.
    //

    if (!KSUCCESS(Transfer->Status)) {
        if (Transfer->Status == STATUS_OPERATION_CANCELLED) {
            return;
        }

        goto InterruptTransferCompletionEnd;
    }

    if (Transfer->LengthTransferred != sizeof(ULONG)) {
        RtlDebugPrint("SM95: Got weird interrupt transfer of size %d.\n",
                      Transfer->LengthTransferred);

        goto InterruptTransferCompletionEnd;
    }

    RtlCopyMemory(&Value, Transfer->Buffer, sizeof(ULONG));
    if ((Value & SM95_INTERRUPT_STATUS_PHY) != 0) {

        //
        // Read the interrupt status to clear it from the PHY.
        //

        Status = Sm95pReadMdio(Device,
                               Device->PhyId,
                               Sm95PhyRegisterInterruptSource,
                               &Source);

        if (!KSUCCESS(Status)) {
            goto InterruptTransferCompletionEnd;
        }

        //
        // Read the status register to find out what happened to the link. Read
        // the register twice as the link status bit is sticky.
        //

        Status = Sm95pReadMdio(Device,
                               Device->PhyId,
                               MiiRegisterBasicStatus,
                               &Value);

        if (!KSUCCESS(Status)) {
            goto InterruptTransferCompletionEnd;
        }

        Status = Sm95pReadMdio(Device,
                               Device->PhyId,
                               MiiRegisterBasicStatus,
                               &Value);

        if (!KSUCCESS(Status)) {
            goto InterruptTransferCompletionEnd;
        }

        if ((Value & MII_BASIC_STATUS_LINK_STATUS) != 0) {
            if ((Value & MII_BASIC_STATUS_AUTONEGOTIATE_COMPLETE) != 0) {

                //
                // Get the current link state.
                //

                NetGetLinkState(Device->NetworkLink, &OriginalLinkUp, NULL);

                //
                // TODO: Get the real device speed when generic MII support is
                // added.
                //

                NetSetLinkState(Device->NetworkLink, TRUE, NET_SPEED_100_MBPS);

                //
                // Submit the bulk IN transfer if the original state was down.
                //

                if (OriginalLinkUp == FALSE) {
                    Status = Sm95pSubmitBulkInTransfers(Device);
                    if (!KSUCCESS(Status)) {
                        goto InterruptTransferCompletionEnd;
                    }
                }
            }

        } else {
            NetSetLinkState(Device->NetworkLink, FALSE, 0);

            //
            // Try to cancel the bulk IN transfer. If the transfer has also
            // completed, it may be waiting to run, in which case it is too
            // late to cancel. That's OK as it will check the link state and
            // see that it should not re-submit. Make sure that the cancel
            // routine does not wait for the transfer to reach the inactive
            // state as the transfer could be sitting on the completed transfer
            // queue behind this transfer.
            //

            Sm95pCancelBulkInTransfers(Device);
        }
    }

    //
    // Write the interrupt status register to clear the interrupts.
    //

    Status = Sm95pWriteRegister(Device,
                                Sm95RegisterInterruptStatus,
                                SM95_INTERRUPT_MASK);

    if (!KSUCCESS(Status)) {
        goto InterruptTransferCompletionEnd;
    }

InterruptTransferCompletionEnd:

    //
    // Resubmit the transfer.
    //

    Status = UsbSubmitTransfer(Transfer);
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("SM95: Failed to resubmit interrupt transfer: %d.\n",
                      Status);
    }

    return;
}

VOID
Sm95BulkInTransferCompletion (
    PUSB_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine is called when the bulk in transfer returns. It processes
    the notification from the device.

Arguments:

    Transfer - Supplies a pointer to the transfer that completed.

Return Value:

    None.

--*/

{

    PUCHAR Data;
    PSM95_DEVICE Device;
    PULONG Header;
    ULONG Length;
    BOOL LinkUp;
    NET_PACKET_BUFFER Packet;
    ULONG PacketLength;
    PHYSICAL_ADDRESS PhysicalAddress;
    KSTATUS Status;

    Device = Transfer->UserData;
    Status = STATUS_SUCCESS;

    //
    // If the transfer failed, don't bother with the data.
    //

    if (!KSUCCESS(Transfer->Status)) {

        //
        // If the transfer stalled, attempt to clear the HALT feature from the
        // endpoint.
        //

        if (Transfer->Error == UsbErrorTransferStalled) {
            Status = UsbClearFeature(Device->UsbCoreHandle,
                                     USB_SETUP_REQUEST_ENDPOINT_RECIPIENT,
                                     USB_FEATURE_ENDPOINT_HALT,
                                     Device->BulkInEndpoint);
        }

        goto BulkInTransferCompletionEnd;
    }

    Data = Transfer->Buffer;
    PhysicalAddress = Transfer->BufferPhysicalAddress;
    Length = Transfer->LengthTransferred;
    Packet.IoBuffer = NULL;
    Packet.Flags = 0;
    while (Length > 0) {
        if (Length < sizeof(ULONG)) {
            RtlDebugPrint("SM95: Received odd sized data (%d).\n", Length);
            break;
        }

        Header = (PULONG)Data;

        ASSERT(((UINTN)Header & 0x3) == 0);

        if ((*Header & SM95_RECEIVE_FLAG_ERROR_SUMMARY) != 0) {
            RtlDebugPrint("SM95: Receive error summary 0x%x\n", *Header);
            break;
        }

        PacketLength = (*Header & SM95_RECEIVE_FRAME_LENGTH_MASK) >>
                       SM95_RECEIVE_FRAME_LENGTH_SHIFT;

        if (PacketLength > Length - sizeof(ULONG)) {
            RtlDebugPrint("SM95: Got packet purported to be size %d, but "
                          "only %d bytes remaining in the transfer.\n",
                          PacketLength,
                          Length - sizeof(ULONG));

            break;
        }

        Packet.Buffer = Data + sizeof(ULONG) + SM95_RECEIVE_DATA_OFFSET;
        Packet.BufferPhysicalAddress = PhysicalAddress +
                                       sizeof(ULONG) +
                                       SM95_RECEIVE_DATA_OFFSET;

        Packet.BufferSize = PacketLength - sizeof(ULONG);
        Packet.DataSize = Packet.BufferSize;
        Packet.DataOffset = 0;
        Packet.FooterOffset = Packet.DataSize;
        NetProcessReceivedPacket(Device->NetworkLink, &Packet);

        //
        // Advance to the next packet, adding an extra 4 and aligning the total
        // offset to 4.
        //

        PacketLength += sizeof(ULONG) + SM95_RECEIVE_DATA_OFFSET;
        PacketLength = ALIGN_RANGE_UP(PacketLength, sizeof(ULONG));
        if (PacketLength >= Length) {
            break;
        }

        Length -= PacketLength;
        Data += PacketLength;
        PhysicalAddress += PacketLength;
    }

BulkInTransferCompletionEnd:

    //
    // If the link is still up and everything went smashingly above, resubmit
    // the transfer and around it goes.
    //

    NetGetLinkState(Device->NetworkLink, &LinkUp, NULL);
    if (KSUCCESS(Status) && (LinkUp != FALSE)) {
        Status = UsbSubmitTransfer(Transfer);
        if (!KSUCCESS(Status)) {
            RtlDebugPrint("SM95: Failed to resubmit bulk IN transfer.\n");
        }
    }

    return;
}

KSTATUS
Sm95pInitialize (
    PSM95_DEVICE Device
    )

/*++

Routine Description:

    This routine initializes and enables the SMSC95xx device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    ULONGLONG CurrentTime;
    ULONG ReadValue;
    USB_DEVICE_SPEED Speed;
    KSTATUS Status;
    ULONGLONG Timeout;
    ULONGLONG TimeoutTicks;
    ULONG Value;

    TimeoutTicks = HlQueryTimeCounterFrequency() * SM95_DEVICE_TIMEOUT;

    //
    // The device's PHY is at a fixed address.
    //

    Device->PhyId = SM95_PHY_ID;

    //
    // Perform a reset of the device.
    //

    Value = SM95_HARDWARE_CONFIG_LITE_RESET;
    Status = Sm95pWriteRegister(Device, Sm95RegisterHardwareConfig, Value);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    CurrentTime = KeGetRecentTimeCounter();
    Timeout = CurrentTime + TimeoutTicks;
    do {
        Status = Sm95pReadRegister(Device,
                                   Sm95RegisterHardwareConfig,
                                   &ReadValue);

        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }

        if ((ReadValue & SM95_HARDWARE_CONFIG_LITE_RESET) == 0) {
            break;
        }

        CurrentTime = KeGetRecentTimeCounter();

    } while (CurrentTime <= Timeout);

    if (CurrentTime > Timeout) {
        Status = STATUS_TIMEOUT;
        goto InitializeEnd;
    }

    //
    // Also reset the PHY.
    //

    Value = SM95_POWER_CONTROL_PHY_RESET;
    Status = Sm95pWriteRegister(Device, Sm95RegisterPowerControl, Value);
    CurrentTime = KeGetRecentTimeCounter();
    Timeout = CurrentTime + TimeoutTicks;
    do {
        Status = Sm95pReadRegister(Device,
                                   Sm95RegisterPowerControl,
                                   &ReadValue);

        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }

        if ((ReadValue & SM95_POWER_CONTROL_PHY_RESET) == 0) {
            break;
        }

        CurrentTime = KeGetRecentTimeCounter();

    } while (CurrentTime <= Timeout);

    if (CurrentTime > Timeout) {
        Status = STATUS_TIMEOUT;
        goto InitializeEnd;
    }

    //
    // Read the MAC address from the EEPROM and program it into the device.
    // If there was no EEPROM, generate a random MAC address.
    //

    Status = Sm95pReadMacAddress(Device);
    if (Status == STATUS_INVALID_ADDRESS) {
        NetCreateEthernetAddress(Device->MacAddress);

    } else if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    Status = Sm95pSetMacAddress(Device, Device->MacAddress);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    //
    // Enable BIR.
    //

    Status = Sm95pReadRegister(Device, Sm95RegisterHardwareConfig, &ReadValue);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    Value = ReadValue | SM95_HARDWARE_CONFIG_BULK_IN_EMPTY_RESPONSE;
    Status = Sm95pWriteRegister(Device, Sm95RegisterHardwareConfig, Value);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    //
    // Set up the burst capability.
    //

    Status = UsbGetDeviceSpeed(Device->UsbCoreHandle, &Speed);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    if (Speed == UsbDeviceSpeedHigh) {
        Value = SM95_HIGH_SPEED_BURST_SIZE / SM95_HIGH_SPEED_TRANSFER_SIZE;

    } else {

        ASSERT(Speed == UsbDeviceSpeedFull);

        Value = SM95_FULL_SPEED_BURST_SIZE / SM95_FULL_SPEED_TRANSFER_SIZE;
    }

    Status = Sm95pWriteRegister(Device, Sm95RegisterBurstCapability, Value);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    //
    // Set the bulk IN delay.
    //

    Value = SM95_DEFAULT_BULK_IN_DELAY;
    Status = Sm95pWriteRegister(Device, Sm95RegisterBulkInDelay, Value);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    //
    // Enable MEF and BCE.
    //

    Status = Sm95pReadRegister(Device, Sm95RegisterHardwareConfig, &Value);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    Value |= SM95_HARDWARE_CONFIG_MULTIPLE_ETHERNET_FRAMES |
             SM95_HARDWARE_CONFIG_BURST_CAP_ENABLED;

    Value &= ~SM95_HARDWARE_CONFIG_RX_DATA_OFFSET_MASK;
    Value |= SM95_RECEIVE_DATA_OFFSET <<
             SM95_HARDWARE_CONFIG_RX_DATA_OFFSET_SHIFT;

    Status = Sm95pWriteRegister(Device, Sm95RegisterHardwareConfig, Value);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    //
    // Clear all interrupts.
    //

    Value = SM95_INTERRUPT_MASK;
    Status = Sm95pWriteRegister(Device, Sm95RegisterInterruptStatus, Value);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    //
    // Configure the GPIO pins as LED outputs.
    //

    Value = SM95_LED_GPIO_CONFIG_SPEED_LED | SM95_LED_GPIO_CONFIG_LINK_LED |
            SM95_LED_GPIO_CONFIG_FULL_DUPLEX_LED;

    Status = Sm95pWriteRegister(Device, Sm95RegisterLedGpioConfig, Value);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    //
    // Initialize transmit parameters.
    //

    Status = Sm95pWriteRegister(Device, Sm95RegisterFlowControl, 0);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    Value = SM95_AUTO_FLOW_CONTROL_DEFAULT;
    Status = Sm95pWriteRegister(Device, Sm95RegisterAutoFlowControl, Value);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    Status = Sm95pReadRegister(Device,
                               Sm95RegisterMacControl,
                               &(Device->MacControl));

    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    //
    // Initialize receive parameters.
    //

    Value = SM95_VLAN_8021Q;
    Status = Sm95pWriteRegister(Device, Sm95RegisterVlan1, Value);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    //
    // Disable checksum offload engines.
    //

    Status = Sm95pSetupChecksumOffloading(Device, FALSE, FALSE);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    Status = Sm95pInitializePhy(Device);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    //
    // Enable PHY interrupts.
    //

    Status = Sm95pReadRegister(Device,
                               Sm95RegisterInterruptEndpointControl,
                               &Value);

    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    Value |= SM95_INTERRUPT_ENDPOINT_CONTROL_PHY_INTERRUPTS;
    Status = Sm95pWriteRegister(Device,
                                Sm95RegisterInterruptEndpointControl,
                                Value);

    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    Status = Sm95pEnableMac(Device);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    Status = Sm95pUpdateFilterMode(Device);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    //
    // Do an initial read of the MII status and report the link as up if it
    // started connected. Read the register twice as the link status bit is
    // sticky.
    //

    Status = Sm95pReadMdio(Device,
                           Device->PhyId,
                           MiiRegisterBasicStatus,
                           &Value);

    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    Status = Sm95pReadMdio(Device,
                           Device->PhyId,
                           MiiRegisterBasicStatus,
                           &Value);

    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    //
    // Notify the networking core of this new link now that the device is ready
    // to send and receive data, pending media being present.
    //

    Status = Sm95pAddNetworkDevice(Device);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

    if (((Value & MII_BASIC_STATUS_LINK_STATUS) != 0) &&
        ((Value & MII_BASIC_STATUS_AUTONEGOTIATE_COMPLETE) != 0)) {

        //
        // TODO: Get the real device speed when generic MII support is added.
        //

        NetSetLinkState(Device->NetworkLink, TRUE, NET_SPEED_100_MBPS);

        //
        // Submit the bulk IN transfer.
        //

        Status = Sm95pSubmitBulkInTransfers(Device);
        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }
    }

    //
    // Submit the interrupt transfer.
    //

    Status = UsbSubmitTransfer(Device->InterruptTransfer);
    if (!KSUCCESS(Status)) {
        goto InitializeEnd;
    }

InitializeEnd:
    return Status;
}

VOID
Sm95pDestroyBulkOutTransfers (
    PSM95_DEVICE Device
    )

/*++

Routine Description:

    This routine destroys the SMSC95xx device's bulk out tranfers.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    PSM95_BULK_OUT_TRANSFER Sm95Transfer;

    while (LIST_EMPTY(&(Device->BulkOutFreeTransferList)) == FALSE) {
        Sm95Transfer = LIST_VALUE(Device->BulkOutFreeTransferList.Next,
                                 SM95_BULK_OUT_TRANSFER,
                                 ListEntry);

        ASSERT(Sm95Transfer->Packet == NULL);

        LIST_REMOVE(&(Sm95Transfer->ListEntry));
        UsbDestroyTransfer(Sm95Transfer->UsbTransfer);
        MmFreePagedPool(Sm95Transfer);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
Sm95pTransmitPacketCompletion (
    PUSB_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine is called when an asynchronous I/O request completes with
    success, failure, or is cancelled.

Arguments:

    Transfer - Supplies a pointer to the transfer that completed.

Return Value:

    None.

--*/

{

    PSM95_BULK_OUT_TRANSFER Sm95Transfer;

    Sm95Transfer = Transfer->UserData;
    RtlAtomicAdd32(&(Sm95Transfer->Device->BulkOutTransferCount), -1);
    NetFreeBuffer(Sm95Transfer->Packet);
    Sm95Transfer->Packet = NULL;
    Sm95pFreeBulkOutTransfer(Sm95Transfer);
    return;
}

KSTATUS
Sm95pEnableMac (
    PSM95_DEVICE Device
    )

/*++

Routine Description:

    This routine enables transmitting and receiving of data from the wild.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    //
    // Disable multicast for now.
    //

    Device->MacControl &= ~(SM95_MAC_CONTROL_PROMISCUOUS |
                            SM95_MAC_CONTROL_MULTICAST_PAS |
                            SM95_MAC_CONTROL_HP_FILTER |
                            SM95_MAC_CONTROL_RECEIVE_ALL |
                            SM95_MAC_CONTROL_RECEIVE_OWN);

    //
    // Enable transmit and receive at the MAC.
    //

    Device->MacControl |= SM95_MAC_CONTROL_FULL_DUPLEX|
                          SM95_MAC_CONTROL_ENABLE_TRANSMIT |
                          SM95_MAC_CONTROL_ENABLE_RECEIVE;

    Status = Sm95pWriteRegister(Device,
                                Sm95RegisterMacControl,
                                Device->MacControl);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Enable transmit at the SCSRs.
    //

    Status = Sm95pWriteRegister(Device,
                                Sm95RegisterTransmitControl,
                                SM95_TRANSMIT_CONTROL_ENABLE);

    return Status;
}

KSTATUS
Sm95pUpdateFilterMode (
    PSM95_DEVICE Device
    )

/*++

Routine Description:

    This routine updates an SMSC95xx device's filter mode based on the
    currently enabled capabilities.

Arguments:

    Device - Supplies a pointer to the SMSC95xx device.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    if ((Device->EnabledCapabilities &
         NET_LINK_CAPABILITY_PROMISCUOUS_MODE) != 0) {

        Device->MacControl |= SM95_MAC_CONTROL_PROMISCUOUS;

    } else {
        Device->MacControl &= ~SM95_MAC_CONTROL_PROMISCUOUS;
    }

    Status = Sm95pWriteRegister(Device,
                                Sm95RegisterMacControl,
                                Device->MacControl);

    return Status;
}

KSTATUS
Sm95pSetupChecksumOffloading (
    PSM95_DEVICE Device,
    BOOL EnableTransmitChecksumOffload,
    BOOL EnableReceiveChecksumOffload
    )

/*++

Routine Description:

    This routine enables or disables the checksum offload engines for transmit
    and receive packets.

Arguments:

    Device - Supplies a pointer to the device.

    EnableTransmitChecksumOffload - Supplies a boolean indicating whether or
        not checksum offloading should be enabled for outgoing packets.

    EnableReceiveChecksumOffload - Supplies a boolean indicating whether or not
        checksum offloading should be enabled for incoming packets.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;
    ULONG Value;

    Status = Sm95pReadRegister(Device,
                               Sm95RegisterChecksumOffloadControl,
                               &Value);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    Value &= ~(SM95_CHECKSUM_CONTROL_TRANSMIT_ENABLE |
               SM95_CHECKSUM_CONTROL_RECEIVE_ENABLE);

    if (EnableTransmitChecksumOffload != FALSE) {
        Value |= SM95_CHECKSUM_CONTROL_TRANSMIT_ENABLE;
    }

    if (EnableReceiveChecksumOffload != FALSE) {
        Value |= SM95_CHECKSUM_CONTROL_RECEIVE_ENABLE;
    }

    Status = Sm95pWriteRegister(Device,
                                Sm95RegisterChecksumOffloadControl,
                                Value);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    return Status;
}

KSTATUS
Sm95pSetMacAddress (
    PSM95_DEVICE Device,
    BYTE Address[ETHERNET_ADDRESS_SIZE]
    )

/*++

Routine Description:

    This routine sets the individual physical address for the given device.

Arguments:

    Device - Supplies a pointer to the device.

    Address - Supplies the new address to set.

Return Value:

    Status code.

--*/

{

    ULONG AddressHigh;
    ULONG AddressLow;
    KSTATUS Status;

    RtlCopyMemory(&AddressLow, Address, sizeof(ULONG));
    AddressHigh = 0;
    RtlCopyMemory(&AddressHigh, &(Address[sizeof(ULONG)]), sizeof(USHORT));
    Status = Sm95pWriteRegister(Device, Sm95RegisterMacAddressLow, AddressLow);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Status = Sm95pWriteRegister(Device,
                                Sm95RegisterMacAddressHigh,
                                AddressHigh);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    return STATUS_SUCCESS;
}

KSTATUS
Sm95pReadMacAddress (
    PSM95_DEVICE Device
    )

/*++

Routine Description:

    This routine reads the MAC address out of the EEPROM on the SMSC95xx. The
    MAC address will be stored in the device structure.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = Sm95pReadEeprom(Device,
                             SM95_EEPROM_MAC_ADDRESS,
                             sizeof(Device->MacAddress),
                             Device->MacAddress);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    if (NetIsEthernetAddressValid(Device->MacAddress) == FALSE) {
        return STATUS_INVALID_ADDRESS;
    }

    return STATUS_SUCCESS;
}

KSTATUS
Sm95pInitializePhy (
    PSM95_DEVICE Device
    )

/*++

Routine Description:

    This routine initializes the PHY on the SMSC95xx.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;
    ULONG Value;

    Status = Sm95pWriteMdio(Device,
                            Device->PhyId,
                            MiiRegisterBasicControl,
                            MII_BASIC_CONTROL_RESET);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Wait for the reset to complete.
    //

    do {
        Status = Sm95pReadMdio(Device,
                               Device->PhyId,
                               MiiRegisterBasicControl,
                               &Value);

        if (!KSUCCESS(Status)) {
            return Status;
        }

    } while ((Value & MII_BASIC_CONTROL_RESET) != 0);

    //
    // Advertise all modes and pause capabilities.
    //

    Value = MII_ADVERTISE_ALL | MII_ADVERTISE_CSMA | MII_ADVERTISE_PAUSE |
            MII_ADVERTISE_PAUSE_ASYMMETRIC;

    Status = Sm95pWriteMdio(Device, Device->PhyId, MiiRegisterAdvertise, Value);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Read the interrupt status register to clear the bits.
    //

    Status = Sm95pReadMdio(Device,
                           Device->PhyId,
                           Sm95PhyRegisterInterruptSource,
                           &Value);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Write the interrupt mask.
    //

    Value = SM95_PHY_INTERRUPT_AUTONEGOTIATION_COMPLETE |
            SM95_PHY_INTERRUPT_LINK_DOWN;

    Status = Sm95pWriteMdio(Device,
                            Device->PhyId,
                            Sm95PhyRegisterInterruptMask,
                            Value);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Restart auto-negotiation.
    //

    Status = Sm95pReadMdio(Device,
                            Device->PhyId,
                            MiiRegisterBasicControl,
                            &Value);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    Value |= MII_BASIC_CONTROL_RESTART_AUTONEGOTIATION;
    Status = Sm95pWriteMdio(Device,
                            Device->PhyId,
                            MiiRegisterBasicControl,
                            Value);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    return Status;
}

KSTATUS
Sm95pRestartNway (
    PSM95_DEVICE Device
    )

/*++

Routine Description:

    This routine restarts N-Way (autonegotiation) for the device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;
    ULONG Value;

    //
    // Read the control register, and restart autonegotiation if it's enabled.
    //

    Status = Sm95pReadMdio(Device,
                           Device->PhyId,
                           MiiRegisterBasicControl,
                           &Value);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    if ((Value & MII_BASIC_CONTROL_ENABLE_AUTONEGOTIATION) != 0) {
        Value |= MII_BASIC_CONTROL_RESTART_AUTONEGOTIATION;
        Status = Sm95pWriteMdio(Device,
                                Device->PhyId,
                                MiiRegisterBasicControl,
                                Value);

    } else {
        Status = STATUS_INVALID_CONFIGURATION;
    }

    return Status;
}

KSTATUS
Sm95pReadEeprom (
    PSM95_DEVICE Device,
    ULONG Offset,
    ULONG Length,
    PBYTE Data
    )

/*++

Routine Description:

    This routine reads from the EEPROM on the SMSC95xx device.

Arguments:

    Device - Supplies a pointer to the device.

    Offset - Supplies the offset in bytes from the beginning of the EEPROM to
        read from.

    Length - Supplies the number of bytes to read.

    Data - Supplies a pointer where the EEPROM data will be returned on success.

Return Value:

    Status code.

--*/

{

    ULONG ByteIndex;
    ULONG Command;
    KSTATUS Status;
    ULONG Value;

    Status = Sm95pWaitForEeprom(Device, FALSE);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Read bytes from the EEPROM one at a time.
    //

    for (ByteIndex = 0; ByteIndex < Length; ByteIndex += 1) {

        //
        // Set up the command register to read the EEPROM at the specified
        // offset.
        //

        ASSERT(Offset + ByteIndex <= SM95_EEPROM_COMMAND_ADDRESS_MASK);

        Command = SM95_EEPROM_COMMAND_BUSY |
                  ((Offset + ByteIndex) & SM95_EEPROM_COMMAND_ADDRESS_MASK);

        Status = Sm95pWriteRegister(Device, Sm95RegisterEepromCommand, Command);
        if (!KSUCCESS(Status)) {
            return Status;
        }

        //
        // Wait for the EEPROM to accept the command.
        //

        Status = Sm95pWaitForEeprom(Device, TRUE);
        if (!KSUCCESS(Status)) {
            return Status;
        }

        //
        // Read the spoils out of the data register.
        //

        Status = Sm95pReadRegister(Device, Sm95RegisterEepromData, &Value);
        if (!KSUCCESS(Status)) {
            return Status;
        }

        Data[ByteIndex] = (BYTE)Value;
    }

    return STATUS_SUCCESS;
}

KSTATUS
Sm95pWaitForEeprom (
    PSM95_DEVICE Device,
    BOOL ObserveEepromTimeout
    )

/*++

Routine Description:

    This routine waits for the EEPROM to finish or time out.

Arguments:

    Device - Supplies a pointer to the device.

    ObserveEepromTimeout - Supplies a boolean indicating if the EEPROM timeout
        bit should be checked as well as the busy bit (TRUE) or if only the
        busy bit should be waited on (FALSE).

Return Value:

    Status code.

--*/

{

    KSTATUS Status;
    ULONGLONG Timeout;
    ULONG Value;

    Timeout = KeGetRecentTimeCounter() +
              (HlQueryTimeCounterFrequency() * SM95_EEPROM_TIMEOUT);

    do {
        Status = Sm95pReadRegister(Device, Sm95RegisterEepromCommand, &Value);
        if (!KSUCCESS(Status)) {
            return Status;
        }

        if ((ObserveEepromTimeout != FALSE) &&
            ((Value & SM95_EEPROM_COMMAND_TIMEOUT) != 0)) {

            break;
        }

        if ((Value & SM95_EEPROM_COMMAND_BUSY) == 0) {
            return STATUS_SUCCESS;
        }

    } while (KeGetRecentTimeCounter() <= Timeout);

    return STATUS_TIMEOUT;
}

KSTATUS
Sm95pWriteMdio (
    PSM95_DEVICE Device,
    USHORT PhyId,
    USHORT Index,
    ULONG Data
    )

/*++

Routine Description:

    This routine performs an MDIO register write.

Arguments:

    Device - Supplies a pointer to the device.

    PhyId - Supplies the device ID to write to.

    Index - Supplies the register to write.

    Data - Supplies the value to write.

Return Value:

    Status code.

--*/

{

    ULONG Address;
    KSTATUS Status;

    Status = Sm95pWaitForPhy(Device);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Write the data contents first.
    //

    Status = Sm95pWriteRegister(Device, Sm95RegisterMiiData, Data);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Write the address into the address register to execute the write.
    //

    Address = (PhyId << SM95_MII_ADDRESS_PHY_ID_SHIFT) |
              (Index << SM95_MII_ADDRESS_INDEX_SHIFT) |
              SM95_MII_ADDRESS_WRITE;

    Status = Sm95pWriteRegister(Device, Sm95RegisterMiiAddress, Address);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    return Status;
}

KSTATUS
Sm95pReadMdio (
    PSM95_DEVICE Device,
    USHORT PhyId,
    USHORT Index,
    PULONG Data
    )

/*++

Routine Description:

    This routine performs an MDIO register read.

Arguments:

    Device - Supplies a pointer to the device.

    PhyId - Supplies the device ID to read from.

    Index - Supplies the register to read.

    Data - Supplies a pointer where the register contents will be returned on
        success.

Return Value:

    Status code.

--*/

{

    ULONG Address;
    KSTATUS Status;
    ULONG Value;

    Status = Sm95pWaitForPhy(Device);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Write the address into the address register.
    //

    Address = (PhyId << SM95_MII_ADDRESS_PHY_ID_SHIFT) |
              (Index << SM95_MII_ADDRESS_INDEX_SHIFT);

    Status = Sm95pWriteRegister(Device, Sm95RegisterMiiAddress, Address);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Status = Sm95pWaitForPhy(Device);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Read the requested data out of the data register.
    //

    Status = Sm95pReadRegister(Device, Sm95RegisterMiiData, &Value);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    *Data = Value & 0x0000FFFF;
    return Status;
}

KSTATUS
Sm95pWaitForPhy (
    PSM95_DEVICE Device
    )

/*++

Routine Description:

    This routine waits until the PHY is not busy.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;
    ULONGLONG Timeout;
    ULONG Value;

    Timeout = KeGetRecentTimeCounter() +
              (HlQueryTimeCounterFrequency() * SM95_DEVICE_TIMEOUT);

    do {
        Status = Sm95pReadRegister(Device, Sm95RegisterMiiAddress, &Value);
        if (!KSUCCESS(Status)) {
            return Status;
        }

        if ((Value & SM95_MII_ADDRESS_BUSY) == 0) {
            return STATUS_SUCCESS;
        }

    } while (KeGetRecentTimeCounter() <= Timeout);

    return STATUS_TIMEOUT;
}

KSTATUS
Sm95pWriteRegister (
    PSM95_DEVICE Device,
    USHORT Register,
    ULONG Data
    )

/*++

Routine Description:

    This routine performs a register write to the SMSC95xx device.

Arguments:

    Device - Supplies a pointer to the device.

    Register - Supplies the register number to write to.

    Data - Supplies the value to write.

Return Value:

    Status code.

--*/

{

    PUSB_TRANSFER ControlTransfer;
    PUSB_SETUP_PACKET Setup;
    KSTATUS Status;

    ControlTransfer = Device->ControlTransfer;
    Setup = ControlTransfer->Buffer;
    Setup->RequestType = USB_SETUP_REQUEST_TO_DEVICE |
                         USB_SETUP_REQUEST_VENDOR |
                         USB_SETUP_REQUEST_DEVICE_RECIPIENT;

    Setup->Request = SM95_VENDOR_REQUEST_WRITE_REGISTER;
    Setup->Value = 0;
    Setup->Index = Register;
    Setup->Length = sizeof(ULONG);
    RtlCopyMemory(Setup + 1, &Data, sizeof(ULONG));
    ControlTransfer->Direction = UsbTransferDirectionOut;
    ControlTransfer->Length = sizeof(USB_SETUP_PACKET) + sizeof(ULONG);
    Status = UsbSubmitSynchronousTransfer(ControlTransfer);
    return Status;
}

KSTATUS
Sm95pReadRegister (
    PSM95_DEVICE Device,
    USHORT Register,
    PULONG Data
    )

/*++

Routine Description:

    This routine performs a register read from the SMSC95xx device.

Arguments:

    Device - Supplies a pointer to the device.

    Register - Supplies the register number to read from.

    Data - Supplies a pointer where the register contents will be returned on
        success.

Return Value:

    Status code.

--*/

{

    PUSB_TRANSFER ControlTransfer;
    PUSB_SETUP_PACKET Setup;
    KSTATUS Status;

    ControlTransfer = Device->ControlTransfer;
    Setup = ControlTransfer->Buffer;
    Setup->RequestType = USB_SETUP_REQUEST_TO_HOST |
                         USB_SETUP_REQUEST_VENDOR |
                         USB_SETUP_REQUEST_DEVICE_RECIPIENT;

    Setup->Request = SM95_VENDOR_REQUEST_READ_REGISTER;
    Setup->Value = 0;
    Setup->Index = Register;
    Setup->Length = sizeof(ULONG);
    ControlTransfer->Direction = UsbTransferDirectionIn;
    ControlTransfer->Length = sizeof(USB_SETUP_PACKET) + sizeof(ULONG);
    Status = UsbSubmitSynchronousTransfer(ControlTransfer);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    RtlCopyMemory(Data, Setup + 1, sizeof(ULONG));
    return Status;
}

KSTATUS
Sm95pSubmitBulkInTransfers (
    PSM95_DEVICE Device
    )

/*++

Routine Description:

    This routine submits all the bulk IN transfers allocated for the device.

Arguments:

    Device - Supplies a pointer to an SM95 device.

Return Value:

    Status code.

--*/

{

    ULONG Index;
    KSTATUS Status;

    for (Index = 0; Index < SM95_BULK_IN_TRANSFER_COUNT; Index += 1) {
        Status = UsbSubmitTransfer(Device->BulkInTransfer[Index]);
        if (!KSUCCESS(Status)) {
            break;
        }
    }

    return Status;
}

VOID
Sm95pCancelBulkInTransfers (
    PSM95_DEVICE Device
    )

/*++

Routine Description:

    This routine attempts to cancel all the bulk IN transfers for the device.

Arguments:

    Device - Supplies a pointer to an SM95 device.

Return Value:

    None.

--*/

{

    ULONG Index;

    for (Index = 0; Index < SM95_BULK_IN_TRANSFER_COUNT; Index += 1) {
        UsbCancelTransfer(Device->BulkInTransfer[Index], FALSE);
    }

    return;
}

PSM95_BULK_OUT_TRANSFER
Sm95pAllocateBulkOutTransfer (
    PSM95_DEVICE Device
    )

/*++

Routine Description:

    This routine allocates an SM95 bulk OUT transfer. If there are no free bulk
    OUT transfers ready to go, it will create a new transfer.

Arguments:

    Device - Supplies a pointer to the SM95 device in need of a new transfer.

Return Value:

    Returns a pointer to the allocated SM95 bulk OUT transfer on success or
    NULL on failure.

--*/

{

    PSM95_BULK_OUT_TRANSFER Sm95Transfer;
    PUSB_TRANSFER UsbTransfer;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Loop attempting to use the most recently released existing transfer, but
    // allocate a new transfer if none are available.
    //

    Sm95Transfer = NULL;
    while (Sm95Transfer == NULL) {
        if (LIST_EMPTY(&(Device->BulkOutFreeTransferList)) != FALSE) {
            Sm95Transfer = MmAllocatePagedPool(sizeof(SM95_BULK_OUT_TRANSFER),
                                               SM95_ALLOCATION_TAG);

            if (Sm95Transfer == NULL) {
                goto AllocateBulkOutTransferEnd;
            }

            UsbTransfer = UsbAllocateTransfer(Device->UsbCoreHandle,
                                              Device->BulkOutEndpoint,
                                              SM95_MAX_PACKET_SIZE,
                                              0);

            if (UsbTransfer == NULL) {
                MmFreePagedPool(Sm95Transfer);
                Sm95Transfer = NULL;
                goto AllocateBulkOutTransferEnd;
            }

            UsbTransfer->Direction = UsbTransferDirectionOut;
            UsbTransfer->CallbackRoutine = Sm95pTransmitPacketCompletion;
            UsbTransfer->UserData = Sm95Transfer;
            Sm95Transfer->Device = Device;
            Sm95Transfer->UsbTransfer = UsbTransfer;
            Sm95Transfer->Packet = NULL;

        } else {
            KeAcquireQueuedLock(Device->BulkOutListLock);
            if (LIST_EMPTY(&(Device->BulkOutFreeTransferList)) == FALSE) {
                Sm95Transfer = LIST_VALUE(Device->BulkOutFreeTransferList.Next,
                                          SM95_BULK_OUT_TRANSFER,
                                          ListEntry);

                LIST_REMOVE(&(Sm95Transfer->ListEntry));
            }

            KeReleaseQueuedLock(Device->BulkOutListLock);
        }
    }

AllocateBulkOutTransferEnd:
    return Sm95Transfer;
}

VOID
Sm95pFreeBulkOutTransfer (
    PSM95_BULK_OUT_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine releases an SM95 bulk OUT transfer for recycling.

Arguments:

    Transfer - Supplies a pointer to the SM95 transfer to be recycled.

Return Value:

    None.

--*/

{

    PSM95_DEVICE Device;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Insert it onto the head of the list so it stays hot.
    //

    Device = Transfer->Device;
    KeAcquireQueuedLock(Device->BulkOutListLock);
    INSERT_AFTER(&(Transfer->ListEntry), &(Device->BulkOutFreeTransferList));
    KeReleaseQueuedLock(Device->BulkOutListLock);
    return;
}

