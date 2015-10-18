/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    e100hw.c

Abstract:

    This module implements the portion of the e100 driver that actually
    interacts with the hardware.

Author:

    Evan Green 4-Apr-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/driver.h>
#include <minoca/net/netdrv.h>
#include "e100.h"

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
E100pReadDeviceMacAddress (
    PE100_DEVICE Device
    );

KSTATUS
E100pPerformEepromIo (
    PE100_DEVICE Device,
    USHORT RegisterOffset,
    PUSHORT Value,
    BOOL Write
    );

KSTATUS
E100pDetermineEepromAddressWidth (
    PE100_DEVICE Device
    );

VOID
E100pReapCompletedCommands (
    PE100_DEVICE Device
    );

VOID
E100pReapReceivedFrames (
    PE100_DEVICE Device
    );

VOID
E100pSendPendingPackets (
    PE100_DEVICE Device
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
E100Send (
    PVOID DriverContext,
    PLIST_ENTRY PacketListHead
    )

/*++

Routine Description:

    This routine sends data through the network.

Arguments:

    DriverContext - Supplies a pointer to the driver context associated with the
        link down which this data is to be sent.

    PacketListHead - Supplies a pointer to the head of a list of network
        packets to send. Data these packets may be modified by this routine,
        but must not be used once this routine returns.

Return Value:

    Status code. It is assumed that either all packets are submitted (if
    success is returned) or none of the packets were submitted (if a failing
    status is returned).

--*/

{

    PE100_DEVICE Device;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Device = (PE100_DEVICE)DriverContext;
    KeAcquireQueuedLock(Device->CommandListLock);
    if (Device->LinkActive == FALSE) {
        Status = STATUS_NO_NETWORK_CONNECTION;
        goto SendEnd;
    }

    //
    // Add these packets onto the end of the list of outgoing packets.
    //

    APPEND_LIST(PacketListHead, &(Device->TransmitPacketList));

    //
    // Enqueue as many as possible now.
    //

    E100pSendPendingPackets(Device);
    Status = STATUS_SUCCESS;

SendEnd:
    KeReleaseQueuedLock(Device->CommandListLock);
    return Status;
}

KSTATUS
E100GetSetInformation (
    PVOID DriverContext,
    NET_LINK_INFORMATION_TYPE InformationType,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets the network device layer's link information.

Arguments:

    DriverContext - Supplies a pointer to the driver context associated with the
        link for which information is being set or queried.

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
E100pInitializeDeviceStructures (
    PE100_DEVICE Device
    )

/*++

Routine Description:

    This routine performs housekeeping preparation for resetting and enabling
    an E100 device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    PE100_COMMAND Command;
    ULONG CommandIndex;
    ULONG CommandSize;
    PE100_RECEIVE_FRAME Frame;
    ULONG FrameBasePhysical;
    ULONG FrameIndex;
    ULONG IoBufferFlags;
    ULONG NextFrameAddress;
    ULONG ReceiveSize;
    KSTATUS Status;

    //
    // Initialize the command and receive list locks.
    //

    Device->CommandListLock = KeCreateQueuedLock();
    if (Device->CommandListLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    Device->ReceiveListLock = KeCreateQueuedLock();
    if (Device->ReceiveListLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    //
    // Allocate the receive buffers. This is allocated as non-write though and
    // cacheable, which means software must be careful when the frame is
    // first received (and do an invalidate), and when setting up the
    // link pointers, but after the receive is complete it's normal memory.
    //

    ReceiveSize = sizeof(E100_RECEIVE_FRAME) * E100_RECEIVE_FRAME_COUNT;

    ASSERT(Device->ReceiveFrameIoBuffer == NULL);

    IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS;
    Device->ReceiveFrameIoBuffer = MmAllocateNonPagedIoBuffer(0,
                                                              MAX_ULONG,
                                                              16,
                                                              ReceiveSize,
                                                              IoBufferFlags);

    if (Device->ReceiveFrameIoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    ASSERT(Device->ReceiveFrameIoBuffer->FragmentCount == 1);
    ASSERT(Device->ReceiveFrameIoBuffer->Fragment[0].VirtualAddress != NULL);

    Device->ReceiveFrame =
                      Device->ReceiveFrameIoBuffer->Fragment[0].VirtualAddress;

    Device->ReceiveListBegin = 0;

    //
    // Allocate the command blocks (which don't include the data to transmit).
    // This memory is allocated non-cached since every write and read
    // essentially interacts with the hardware, and the data to transmit isn't
    // included.
    //

    CommandSize = sizeof(E100_COMMAND) * E100_COMMAND_RING_COUNT;

    ASSERT(Device->CommandIoBuffer == NULL);

    IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS;
    Device->CommandIoBuffer = MmAllocateNonPagedIoBuffer(0,
                                                         MAX_ULONG,
                                                         16,
                                                         CommandSize,
                                                         IoBufferFlags);

    if (Device->CommandIoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    ASSERT(Device->CommandIoBuffer->FragmentCount == 1);
    ASSERT(Device->CommandIoBuffer->Fragment[0].VirtualAddress != NULL);

    Device->Command = Device->CommandIoBuffer->Fragment[0].VirtualAddress;
    Device->CommandListBegin = 0;
    Device->CommandListEnd = 1;
    RtlZeroMemory(Device->Command, CommandSize);
    INITIALIZE_LIST_HEAD(&(Device->TransmitPacketList));

    //
    // Allocate an array of pointers to net packet buffers that runs parallel
    // to the command array.
    //

    AllocationSize = sizeof(PNET_PACKET_BUFFER) * E100_COMMAND_RING_COUNT;
    Device->CommandPacket = MmAllocatePagedPool(AllocationSize,
                                                E100_ALLOCATION_TAG);

    if (Device->CommandPacket == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    RtlZeroMemory(Device->CommandPacket, AllocationSize);

    ASSERT(Device->LinkCheckTimer == NULL);

    Device->LinkCheckTimer = KeCreateTimer(E100_ALLOCATION_TAG);
    if (Device->LinkCheckTimer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    //
    // Initialize the receive frame list.
    //

    FrameBasePhysical =
            (ULONG)(Device->ReceiveFrameIoBuffer->Fragment[0].PhysicalAddress);

    NextFrameAddress = FrameBasePhysical + sizeof(E100_RECEIVE_FRAME);
    for (FrameIndex = 0;
         FrameIndex < E100_RECEIVE_FRAME_COUNT;
         FrameIndex += 1) {

        Frame = &(Device->ReceiveFrame[FrameIndex]);
        Frame->Status = 0;
        if (FrameIndex == E100_RECEIVE_FRAME_COUNT - 1) {
            Frame->Status |= E100_RECEIVE_COMMAND_SUSPEND;
            Frame->NextFrame = FrameBasePhysical;

        } else {
            Frame->NextFrame = NextFrameAddress;
        }

        NextFrameAddress += sizeof(E100_RECEIVE_FRAME);
        Frame->Sizes = RECEIVE_FRAME_DATA_SIZE <<
                       E100_RECEIVE_SIZE_BUFFER_SIZE_SHIFT;
    }

    //
    // Initialize the ring of commands.
    //

    FrameBasePhysical =
                 (ULONG)(Device->CommandIoBuffer->Fragment[0].PhysicalAddress);

    NextFrameAddress = FrameBasePhysical + sizeof(E100_COMMAND);
    for (CommandIndex = 0;
         CommandIndex < E100_COMMAND_RING_COUNT;
         CommandIndex += 1) {

        Command = &(Device->Command[CommandIndex]);
        Command->Command = 0;

        //
        // Loop the last command back around to the first: a real ring!
        //

        if (CommandIndex == E100_COMMAND_RING_COUNT - 1) {
            Command->NextCommand = FrameBasePhysical;

        //
        // Point this link at the next command.
        //

        } else {
            Command->NextCommand = NextFrameAddress;
            NextFrameAddress += sizeof(E100_COMMAND);
        }
    }

    //
    // Set the first command to be a no-op that suspends the command unit.
    //

    Command = &(Device->Command[0]);
    Command->Command = E100_COMMAND_SUSPEND | E100_COMMAND_NOP;
    Status = STATUS_SUCCESS;

InitializeDeviceStructuresEnd:
    if (!KSUCCESS(Status)) {
        if (Device->CommandListLock != NULL) {
            KeDestroyQueuedLock(Device->CommandListLock);
            Device->CommandListLock = NULL;
        }

        if (Device->ReceiveListLock != NULL) {
            KeDestroyQueuedLock(Device->ReceiveListLock);
            Device->ReceiveListLock = NULL;
        }

        if (Device->ReceiveFrameIoBuffer != NULL) {
            MmFreeIoBuffer(Device->ReceiveFrameIoBuffer);
            Device->ReceiveFrameIoBuffer = NULL;
            Device->ReceiveFrame = NULL;
        }

        if (Device->CommandIoBuffer != NULL) {
            MmFreeIoBuffer(Device->CommandIoBuffer);
            Device->CommandIoBuffer = NULL;
            Device->Command = NULL;
        }

        if (Device->CommandPacket != NULL) {
            MmFreePagedPool(Device->CommandPacket);
            Device->CommandPacket = NULL;
        }

        if (Device->LinkCheckTimer != NULL) {
            KeDestroyTimer(Device->LinkCheckTimer);
            Device->LinkCheckTimer = NULL;
        }
    }

    return Status;
}

KSTATUS
E100pResetDevice (
    PE100_DEVICE Device
    )

/*++

Routine Description:

    This routine resets the E100 device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    PE100_COMMAND Command;
    ULONG CommandIndex;
    UCHAR GeneralStatus;
    ULONGLONG LinkSpeed;
    PE100_COMMAND PreviousCommand;
    ULONG PreviousCommandIndex;
    KSTATUS Status;
    ULONGLONG Timeout;
    ULONG Value;

    //
    // Perform a complete device reset.
    //

    E100_WRITE_REGISTER32(Device, E100RegisterPort, E100_PORT_RESET);
    HlBusySpin(E100_PORT_RESET_DELAY_MICROSECONDS);

    //
    // Read the MAC address out of the EEPROM.
    //

    Status = E100pReadDeviceMacAddress(Device);
    if (!KSUCCESS(Status)) {
        goto ResetDeviceEnd;
    }

    //
    // Create a network device object now that the device has been fired up
    // enough to read the network address out of it.
    //

    if (Device->NetworkLink == NULL) {
        Status = E100pCreateNetworkDevice(Device);
        if (!KSUCCESS(Status)) {
            goto ResetDeviceEnd;
        }
    }

    //
    // Destroy any old packets lying around.
    //

    for (CommandIndex = 0;
         CommandIndex < E100_COMMAND_RING_COUNT;
         CommandIndex += 1) {

        if (Device->CommandPacket[CommandIndex] != NULL) {
            NetFreeBuffer(Device->CommandPacket[CommandIndex]);
            Device->CommandPacket[CommandIndex] = NULL;
        }
    }

    //
    // Set up the first command to set the individual address.
    //

    Command = &(Device->Command[Device->CommandListEnd]);
    PreviousCommandIndex = E100_DECREMENT_RING_INDEX(Device->CommandListEnd,
                                                     E100_COMMAND_RING_COUNT);

    PreviousCommand = &(Device->Command[PreviousCommandIndex]);
    Device->CommandListEnd = E100_INCREMENT_RING_INDEX(Device->CommandListEnd,
                                                       E100_COMMAND_RING_COUNT);

    RtlCopyMemory(&(Command->U.SetAddress),
                  &(Device->EepromMacAddress[0]),
                  ETHERNET_ADDRESS_SIZE);

    Command->Command = E100_COMMAND_SUSPEND |
                       (E100CommandSetIndividualAddress <<
                        E100_COMMAND_BLOCK_COMMAND_SHIFT);

    PreviousCommand->Command &= ~E100_COMMAND_SUSPEND;

    //
    // Set the command unit base and start the command unit.
    //

    E100_WRITE_REGISTER32(Device, E100RegisterPointer, 0);
    E100_WRITE_COMMAND_REGISTER(Device, E100_COMMAND_UNIT_LOAD_BASE);
    do {
        Value = E100_READ_COMMAND_REGISTER(Device);

    } while ((Value & E100_COMMAND_UNIT_COMMAND_MASK) != 0);

    Value = Device->CommandIoBuffer->Fragment[0].PhysicalAddress;
    E100_WRITE_REGISTER32(Device, E100RegisterPointer, Value);
    E100_WRITE_COMMAND_REGISTER(Device, E100_COMMAND_UNIT_START);
    do {
        Value = E100_READ_COMMAND_REGISTER(Device);

    } while ((Value & E100_COMMAND_UNIT_COMMAND_MASK) != 0);

    //
    // Set the receive unit base and start the receive unit.
    //

    E100_WRITE_REGISTER32(Device, E100RegisterPointer, 0);
    E100_WRITE_COMMAND_REGISTER(Device, E100_COMMAND_RECEIVE_LOAD_BASE);
    do {
        Value = E100_READ_COMMAND_REGISTER(Device);

    } while ((Value & E100_COMMAND_RECEIVE_COMMAND_MASK) != 0);

    Value = Device->ReceiveFrameIoBuffer->Fragment[0].PhysicalAddress;
    E100_WRITE_REGISTER32(Device, E100RegisterPointer, Value);
    E100_WRITE_COMMAND_REGISTER(Device, E100_COMMAND_RECEIVE_START);
    do {
        Value = E100_READ_COMMAND_REGISTER(Device);

    } while ((Value & E100_COMMAND_RECEIVE_COMMAND_MASK) != 0);

    //
    // Check to see how everything is doing. The status register may take a
    // little while to transition from idle to ready.
    //

    Timeout = KeGetRecentTimeCounter() +
              KeConvertMicrosecondsToTimeTicks(E100_READY_TIMEOUT);

    Status = STATUS_NOT_READY;
    do {
        Value = E100_READ_STATUS_REGISTER(Device);
        if ((Value & E100_STATUS_RECEIVE_UNIT_STATUS_MASK) ==
            E100_STATUS_RECEIVE_UNIT_READY) {

            Status = STATUS_SUCCESS;
            break;

        } else if ((Value & E100_STATUS_RECEIVE_UNIT_STATUS_MASK) !=
                   E100_STATUS_RECEIVE_UNIT_IDLE) {

            break;
        }

    } while (KeGetRecentTimeCounter() <= Timeout);

    if (!KSUCCESS(Status)) {
        goto ResetDeviceEnd;
    }

    //
    // Figure out if the link is up, and report on it if so.
    // TODO: The link state should be checked periodically, rather than just
    // once at the beginning.
    //

    Status = NetStartLink(Device->NetworkLink);
    if (!KSUCCESS(Status)) {
        goto ResetDeviceEnd;
    }

    GeneralStatus = E100_READ_REGISTER8(Device, E100RegisterGeneralStatus);
    if ((GeneralStatus & E100_CONTROL_STATUS_LINK_UP) != 0) {
        LinkSpeed = NET_SPEED_10_MBPS;
        if ((GeneralStatus & E100_CONTROL_STATUS_100_MBPS) != 0) {
            LinkSpeed = NET_SPEED_100_MBPS;
        }

        Device->LinkActive = TRUE;
        NetSetLinkState(Device->NetworkLink, TRUE, LinkSpeed);

    } else {
        Device->LinkActive = FALSE;
        NetSetLinkState(Device->NetworkLink, FALSE, 0);
    }

    Status = STATUS_SUCCESS;

ResetDeviceEnd:
    return Status;
}

INTERRUPT_STATUS
E100pInterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the e100 interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the e100 device
        structure.

Return Value:

    Interrupt status.

--*/

{

    PE100_DEVICE Device;
    INTERRUPT_STATUS InterruptStatus;
    USHORT PendingBits;

    Device = (PE100_DEVICE)Context;
    InterruptStatus = InterruptStatusNotClaimed;

    //
    // Read the status register, and if anything's set add it to the pending
    // bits.
    //

    PendingBits = E100_READ_STATUS_REGISTER(Device) &
                  E100_STATUS_INTERRUPT_MASK;

    if (PendingBits != 0) {
        InterruptStatus = InterruptStatusClaimed;
        RtlAtomicOr32(&(Device->PendingStatusBits), PendingBits);

        //
        // Write to clear the bits that got grabbed. Since the semantics of this
        // register are "write 1 to clear", any bits that get set between the
        // read and this write will just stick and generate another level
        // triggered interrupt.
        //

        E100_WRITE_REGISTER8(Device,
                             E100RegisterAcknowledge,
                             PendingBits >> BITS_PER_BYTE);
    }

    return InterruptStatus;
}

INTERRUPT_STATUS
E100pInterruptServiceWorker (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine processes interrupts for the e100 controller at low level.

Arguments:

    Parameter - Supplies an optional parameter passed in by the creator of the
        work item.

Return Value:

    Interrupt status.

--*/

{

    PE100_DEVICE Device;
    ULONG PendingBits;
    ULONG ProcessFramesMask;

    Device = (PE100_DEVICE)(Parameter);

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Clear out the pending bits.
    //

    PendingBits = RtlAtomicExchange32(&(Device->PendingStatusBits), 0);
    if (PendingBits == 0) {
        return InterruptStatusNotClaimed;
    }

    //
    // Handle the receive unit leaving the ready state and new frames
    // coming in.
    //

    ProcessFramesMask = E100_STATUS_RECEIVE_NOT_READY |
                        E100_STATUS_FRAME_RECEIVED;

    if ((PendingBits & ProcessFramesMask) != 0) {
        E100pReapReceivedFrames(Device);
    }

    //
    // If the command unit finished what it was up to, reap that memory.
    //

    if ((PendingBits &
         (E100_STATUS_COMMAND_NOT_ACTIVE |
          E100_STATUS_COMMAND_COMPLETE)) != 0) {

        KeAcquireQueuedLock(Device->CommandListLock);
        E100pReapCompletedCommands(Device);
        KeReleaseQueuedLock(Device->CommandListLock);
    }

    return InterruptStatusClaimed;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
E100pReadDeviceMacAddress (
    PE100_DEVICE Device
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
    USHORT Register;
    KSTATUS Status;
    USHORT Value;

    Register = E100_EEPROM_INDIVIDUAL_ADDRESS_OFFSET;
    Value = 0;
    for (ByteIndex = 0;
         ByteIndex < sizeof(Device->EepromMacAddress);
         ByteIndex += sizeof(USHORT)) {

        Status = E100pPerformEepromIo(Device, Register, &Value, FALSE);
        if (!KSUCCESS(Status)) {
            return Status;
        }

        //
        // The bytes come out of the EEPROM a little backwards. If the MAC
        // address started with 00:AA:..., the first read out of the EEPROM
        // would have 00 in byte 0, and AA in byte 1. Maybe that's not
        // backwards at all.
        //

        Device->EepromMacAddress[ByteIndex] = (BYTE)Value;
        Device->EepromMacAddress[ByteIndex + 1] =
                                               (BYTE)(Value >> BITS_PER_BYTE);

        Register += 1;
    }

    return Status;
}

KSTATUS
E100pPerformEepromIo (
    PE100_DEVICE Device,
    USHORT RegisterOffset,
    PUSHORT Value,
    BOOL Write
    )

/*++

Routine Description:

    This routine performs an I/O operation with the e100's attached EEPROM.

Arguments:

    Device - Supplies a pointer to the device.

    RegisterOffset - Supplies the EEPROM register to read.

    Value - Supplies a pointer to a value that for write operations contains the
        value to write. For read operations, supplies a pointer where the
        read value will be returned.

    Write - Supplies a boolean indicating whether to write to the EEPROM (TRUE)
        or read from the EEPROM (FALSE).

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_READY if the number of address bits could not be determined.

--*/

{

    ULONG BitCount;
    ULONG BitIndex;
    ULONG Mask;
    ULONG OpcodeShift;
    ULONG OutValue;
    USHORT ReadRegister;
    USHORT ReadValue;
    ULONG Register;
    KSTATUS Status;

    //
    // Determine the address width of the EEPROM if needed.
    //

    if (Device->EepromAddressBits == 0) {
        Status = E100pDetermineEepromAddressWidth(Device);
        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    ASSERT(Device->EepromAddressBits != 0);

    //
    // Build the bitfield to send, which looks like: Opcode, Address, Value.
    // The opcode is 3 bits, address is variable (probably 6 or 8), and the
    // value is 16 bits.
    //

    OpcodeShift = (sizeof(USHORT) * BITS_PER_BYTE) + Device->EepromAddressBits;
    if (Write != FALSE) {
        OutValue = E100_EEPROM_OPCODE_WRITE << OpcodeShift;
        OutValue |= *Value;

    } else {
        OutValue = E100_EEPROM_OPCODE_READ << OpcodeShift;
    }

    OutValue |= RegisterOffset << (sizeof(USHORT) * BITS_PER_BYTE);

    //
    // Activate the EEPROM.
    //

    Register = E100_EEPROM_CHIP_SELECT;
    E100_WRITE_REGISTER16(Device, E100RegisterEepromControl, Register);

    //
    // Write out the value, one bit at a time.
    //

    ReadValue = 0;
    BitCount = OpcodeShift + E100_EEPROM_OPCODE_LENGTH;
    for (BitIndex = 0; BitIndex < BitCount; BitIndex += 1) {
        Mask = 1 << (BitCount - BitIndex - 1);
        if ((OutValue & Mask) != 0) {
            Register |= E100_EEPROM_DATA_IN;

        } else {
            Register &= ~E100_EEPROM_DATA_IN;
        }

        //
        // Write the data in bit out to the EEPROM.
        //

        E100_WRITE_REGISTER16(Device, E100RegisterEepromControl, Register);

        //
        // Set the clock high and wait the appropriate amount of time.
        //

        E100_WRITE_REGISTER16(Device,
                              E100RegisterEepromControl,
                              Register | E100_EEPROM_CLOCK);

        HlBusySpin(E100_EEPROM_DELAY_MICROSECONDS);

        //
        // Set the clock low and wait again.
        //

        E100_WRITE_REGISTER16(Device, E100RegisterEepromControl, Register);
        HlBusySpin(E100_EEPROM_DELAY_MICROSECONDS);

        //
        // Read the bit in and save it. Since this field is a short, the higher
        // bits (like the address and opcode) that don't make sense to read
        // will just drop off the big end.
        //

        ReadRegister = E100_READ_REGISTER16(Device, E100RegisterEepromControl);
        if ((ReadRegister & E100_EEPROM_DATA_OUT) != 0) {
            ReadValue |= (USHORT)Mask;
        }
    }

    //
    // Disable the EEPROM.
    //

    E100_WRITE_REGISTER16(Device, E100RegisterEepromControl, 0);
    if (Write == FALSE) {
        *Value = ReadValue;
    }

    return STATUS_SUCCESS;
}

KSTATUS
E100pDetermineEepromAddressWidth (
    PE100_DEVICE Device
    )

/*++

Routine Description:

    This routine determines how many address bits there are on the EEPROM
    attached to the e100 device. This is needed to be able to successfully
    read from and write to the EEPROM. Common results are 6 and 8 (for 64 and
    256 word EEPROMs).

Arguments:

    Device - Supplies a pointer to the device. The address width will be
        stored in here.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_READY if the number of address bits could not be determined.

--*/

{

    ULONG BitIndex;
    ULONG Mask;
    USHORT ReadRegister;
    USHORT Register;
    KSTATUS Status;
    ULONG WriteValue;

    Status = STATUS_SUCCESS;
    WriteValue = E100_EEPROM_OPCODE_READ << (32 - E100_EEPROM_OPCODE_LENGTH);

    //
    // Activate the EEPROM.
    //

    Register = E100_EEPROM_CHIP_SELECT;
    E100_WRITE_REGISTER16(Device, E100RegisterEepromControl, Register);

    //
    // Write out the opcode and address bits, and watch for the EEPROM to start
    // sending the dummy zero.
    //

    for (BitIndex = 0;
         BitIndex < sizeof(ULONG) * BITS_PER_BYTE;
         BitIndex += 1) {

        Mask = 1 << (31 - BitIndex);
        if ((WriteValue & Mask) != 0) {
            Register |= E100_EEPROM_DATA_IN;

        } else {
            Register &= ~E100_EEPROM_DATA_IN;
        }

        //
        // Write the data in bit out to the EEPROM.
        //

        E100_WRITE_REGISTER16(Device, E100RegisterEepromControl, Register);

        //
        // Set the clock high and wait the appropriate amount of time.
        //

        E100_WRITE_REGISTER16(Device,
                              E100RegisterEepromControl,
                              Register | E100_EEPROM_CLOCK);

        HlBusySpin(E100_EEPROM_DELAY_MICROSECONDS);

        //
        // Set the clock low and wait again.
        //

        E100_WRITE_REGISTER16(Device, E100RegisterEepromControl, Register);
        HlBusySpin(E100_EEPROM_DELAY_MICROSECONDS);

        //
        // If the opcode has already gone by, then start watching for the
        // dummy 0 bit coming out of the EEPROM.
        //

        ReadRegister = E100_READ_REGISTER16(Device, E100RegisterEepromControl);
        if ((BitIndex >= E100_EEPROM_OPCODE_LENGTH) &&
            ((ReadRegister & E100_EEPROM_DATA_OUT) == 0)) {

            break;
        }
    }

    if (BitIndex == sizeof(ULONG) * BITS_PER_BYTE) {
        Status = STATUS_NOT_READY;

    } else if (BitIndex == E100_EEPROM_OPCODE_LENGTH) {
        Status = STATUS_UNSUCCESSFUL;

    } else {
        Device->EepromAddressBits = BitIndex - E100_EEPROM_OPCODE_LENGTH + 1;
    }

    //
    // Don't leave the EEPROM hanging, read the 16 bit word that was requested.
    //

    Register = E100_EEPROM_CHIP_SELECT;
    for (BitIndex = 0;
         BitIndex < sizeof(USHORT) * BITS_PER_BYTE;
         BitIndex += 1) {

        //
        // Set the clock high and wait the appropriate amount of time.
        //

        E100_WRITE_REGISTER16(Device,
                              E100RegisterEepromControl,
                              Register | E100_EEPROM_CLOCK);

        HlBusySpin(E100_EEPROM_DELAY_MICROSECONDS);

        //
        // Set the clock low and wait again.
        //

        E100_WRITE_REGISTER16(Device, E100RegisterEepromControl, Register);
        HlBusySpin(E100_EEPROM_DELAY_MICROSECONDS);

        //
        // Read the data out, but ignore it.
        //

        ReadRegister = E100_READ_REGISTER16(Device, E100RegisterEepromControl);
    }

    //
    // Disable the EEPROM.
    //

    E100_WRITE_REGISTER16(Device, E100RegisterEepromControl, 0);
    return Status;
}

VOID
E100pReapCompletedCommands (
    PE100_DEVICE Device
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

    PE100_COMMAND Command;
    ULONG CommandIndex;
    BOOL CommandReaped;

    ASSERT(KeIsQueuedLockHeld(Device->CommandListLock) != FALSE);

    CommandReaped = FALSE;
    while (TRUE) {
        CommandIndex = Device->CommandListBegin;
        Command = &(Device->Command[CommandIndex]);

        //
        // If the command word is zeroed, that's the mark that there are no
        // more commands on the list.
        //

        if (Command->Command == 0) {
            break;
        }

        //
        // If the command, whatever it may be, is not complete, then this is
        // an active entry, so stop reaping.
        //

        if ((Command->Command & E100_COMMAND_MASK_COMMAND_COMPLETE) == 0) {
            break;
        }

        //
        // If it's a transmit command and it's complete, go free the transmit
        // buffer.
        //

        if ((Command->Command & E100_COMMAND_BLOCK_COMMAND_MASK) ==
            (E100CommandTransmit << E100_COMMAND_BLOCK_COMMAND_SHIFT)) {

            NetFreeBuffer(Device->CommandPacket[CommandIndex]);
            Device->CommandPacket[CommandIndex] = NULL;
        }

        //
        // Zero out the command, this one's finished.
        //

        Command->Command = 0;

        //
        // Move the beginning of the list forward.
        //

        Device->CommandListBegin =
              E100_INCREMENT_RING_INDEX(CommandIndex, E100_COMMAND_RING_COUNT);

        CommandReaped = TRUE;
    }

    //
    // If space was freed up, send more segments.
    //

    if ((CommandReaped != FALSE) &&
        (LIST_EMPTY(&(Device->TransmitPacketList)) == FALSE)) {

        E100pSendPendingPackets(Device);
    }

    return;
}

VOID
E100pReapReceivedFrames (
    PE100_DEVICE Device
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

    PE100_RECEIVE_FRAME Frame;
    PE100_RECEIVE_FRAME LastFrame;
    ULONG ListBegin;
    ULONG ListEnd;
    NET_PACKET_BUFFER Packet;
    ULONG ReceivePhysicalAddress;
    USHORT ReceiveStatus;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Loop grabbing completed frames.
    //

    Packet.Flags = 0;
    KeAcquireQueuedLock(Device->ReceiveListLock);
    ReceivePhysicalAddress =
            (ULONG)(Device->ReceiveFrameIoBuffer->Fragment[0].PhysicalAddress);

    while (TRUE) {
        ListBegin = Device->ReceiveListBegin;
        Frame = &(Device->ReceiveFrame[ListBegin]);

        //
        // If the frame is not complete, then this is the end of packets that
        // need to be reaped.
        //

        if ((Frame->Status & E100_RECEIVE_COMPLETE) == 0) {
            break;
        }

        //
        // If the frame came through alright, send it up to the core networking
        // library to process.
        //

        if ((Frame->Status & E100_RECEIVE_OK) != 0) {
            Packet.Buffer = (PVOID)(&(Frame->ReceiveFrame));
            Packet.BufferPhysicalAddress = ReceivePhysicalAddress +
                                      (ListBegin * sizeof(E100_RECEIVE_FRAME));

            Packet.BufferSize = Frame->Sizes &
                                E100_RECEIVE_SIZE_ACTUAL_COUNT_MASK;

            Packet.DataSize = Packet.BufferSize;
            Packet.DataOffset = 0;
            Packet.FooterOffset = Packet.DataSize;
            NetProcessReceivedPacket(Device->NetworkLink, &Packet);
        }

        //
        // Set this frame up to be reused, it will be the new end of the list.
        //

        Frame->Status = E100_RECEIVE_COMMAND_SUSPEND;
        Frame->Sizes = RECEIVE_FRAME_DATA_SIZE <<
                       E100_RECEIVE_SIZE_BUFFER_SIZE_SHIFT;

        //
        // Clear the end of list bit in the previous final frame. The atomic
        // AND also acts as a full memory barrier.
        //

        ListEnd = E100_DECREMENT_RING_INDEX(ListBegin,
                                            E100_RECEIVE_FRAME_COUNT);

        LastFrame = &(Device->ReceiveFrame[ListEnd]);
        RtlAtomicAnd32(&(LastFrame->Status), ~E100_RECEIVE_COMMAND_SUSPEND);

        //
        // Move the beginning pointer up.
        //

        Device->ReceiveListBegin = E100_INCREMENT_RING_INDEX(
                                                      ListBegin,
                                                      E100_RECEIVE_FRAME_COUNT);
    }

    //
    // Resume the receive unit if it's not active.
    //

    ReceiveStatus = E100_READ_STATUS_REGISTER(Device) &
                    E100_STATUS_RECEIVE_UNIT_STATUS_MASK;

    if (ReceiveStatus != E100_STATUS_RECEIVE_UNIT_READY) {

        ASSERT(ReceiveStatus == E100_STATUS_RECEIVE_UNIT_SUSPENDED);

        E100_WRITE_COMMAND_REGISTER(Device, E100_COMMAND_RECEIVE_RESUME);
    }

    KeReleaseQueuedLock(Device->ReceiveListLock);
    return;
}

VOID
E100pSendPendingPackets (
    PE100_DEVICE Device
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
    PE100_COMMAND Command;
    ULONG CommandIndex;
    PNET_PACKET_BUFFER Packet;
    ULONG PreviousCommandIndex;

    while (LIST_EMPTY(&(Device->TransmitPacketList)) == FALSE) {
        Packet = LIST_VALUE(Device->TransmitPacketList.Next,
                            NET_PACKET_BUFFER,
                            ListEntry);

        CommandIndex = Device->CommandListEnd;
        Command = &(Device->Command[CommandIndex]);

        //
        // If the command isn't zero, this is an active or unreaped entry.
        // Wait for some entries to free up, and try again.
        //

        if (Command->Command != 0) {
            return;
        }

        LIST_REMOVE(&(Packet->ListEntry));

        //
        // Success, a free command entry. Let's fill it out!
        //

        Command->Command = (E100CommandTransmit <<
                            E100_COMMAND_BLOCK_COMMAND_SHIFT) |
                           E100_COMMAND_SUSPEND |
                           E100_COMMAND_TRANSMIT_FLEXIBLE_MODE;

        //
        // Calculate the physical address of the transmit buffer descriptor
        // "array" (in quotes because there's only one element in it).
        //

        BufferDescriptorAddress =
                     Device->CommandIoBuffer->Fragment[0].PhysicalAddress +
                     (CommandIndex * sizeof(E100_COMMAND)) +
                     FIELD_OFFSET(E100_COMMAND, U.Transmit.BufferAddress);

        Command->U.Transmit.DescriptorAddress = BufferDescriptorAddress;
        Command->U.Transmit.DescriptorProperties =
                      (1 << E100_TRANSMIT_BUFFER_DESCRIPTOR_COUNT_SHIFT) |
                      E100_TRANSMIT_THRESHOLD;

        //
        // Fill out the transfer buffer descriptor array with the one
        // data entry it points to.
        //

        Command->U.Transmit.BufferAddress = Packet->BufferPhysicalAddress +
                                            Packet->DataOffset;

        Command->U.Transmit.BufferProperties =
                              (Packet->FooterOffset - Packet->DataOffset) |
                              E100_TRANSMIT_BUFFER_END_OF_LIST;

        //
        // Also save the virtual address of this packet. This is not used
        // by hardware, but helps the reaping function know how to free the
        // buffer once it's fully processed by the hardware.
        //

        Command->U.Transmit.BufferVirtual = Packet->Buffer +
                                            Packet->DataOffset;

        Device->CommandPacket[CommandIndex] = Packet;

        //
        // Now that this command is set up, clear the suspend bit on the
        // previous command so the hardware access this new packet. This
        // atomic access also acts as a memory barrier, ensuring this packet
        // is all set up in memory.
        //

        PreviousCommandIndex = E100_DECREMENT_RING_INDEX(
                                                  CommandIndex,
                                                  E100_COMMAND_RING_COUNT);

        RtlAtomicAnd32(&(Device->Command[PreviousCommandIndex].Command),
                       ~E100_COMMAND_SUSPEND);

        //
        // Move the pointer past this entry.
        //

        Device->CommandListEnd = E100_INCREMENT_RING_INDEX(
                                                  CommandIndex,
                                                  E100_COMMAND_RING_COUNT);
    }

    //
    // If the device is suspended at this point (after adding all these great
    // commands), wake it up.
    //

    if ((E100_READ_STATUS_REGISTER(Device) &
         E100_STATUS_COMMAND_UNIT_STATUS_MASK) ==
        E100_STATUS_COMMAND_UNIT_SUSPENDED) {

        E100_WRITE_COMMAND_REGISTER(Device, E100_COMMAND_UNIT_RESUME);
    }

    return;
}

