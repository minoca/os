/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sdstd.c

Abstract:

    This module implements the library functionality for the standard SD/MMC
    device.

Author:

    Chris Stevens 29-Jul-2015

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "sdp.h"

//
// --------------------------------------------------------------------- Macros
//

//
// These macros read and write SD controller registers.
//

#define SD_READ_REGISTER(_Controller, _Register) \
    HlReadRegister32((_Controller)->ControllerBase + (_Register))

#define SD_WRITE_REGISTER(_Controller, _Register, _Value) \
    HlWriteRegister32((_Controller)->ControllerBase + (_Register), (_Value))

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
SdpReadData (
    PSD_CONTROLLER Controller,
    PVOID Data,
    ULONG Size
    );

KSTATUS
SdpWriteData (
    PSD_CONTROLLER Controller,
    PVOID Data,
    ULONG Size
    );

VOID
SdpSetDmaInterrupts (
    PSD_CONTROLLER Controller,
    BOOL Enable,
    ULONG BufferSize
    );

VOID
SdpMediaChangeWorker (
    PVOID Parameter
    );

//
// -------------------------------------------------------------------- Globals
//

SD_FUNCTION_TABLE SdStdFunctionTable = {
    SdStandardInitializeController,
    SdStandardResetController,
    SdStandardSendCommand,
    SdStandardGetSetBusWidth,
    SdStandardGetSetClockSpeed,
    SdStandardGetSetVoltage,
    SdStandardStopDataTransfer,
    NULL,
    NULL,
    SdStandardMediaChangeCallback
};

//
// ------------------------------------------------------------------ Functions
//

SD_API
INTERRUPT_STATUS
SdStandardInterruptService (
    PSD_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine implements the interrupt service routine for a standard SD
    controller.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Returns whether or not the SD controller caused the interrupt.

--*/

{

    ULONG InterruptStatus;
    ULONG MaskedStatus;

    InterruptStatus = SD_READ_REGISTER(Controller, SdRegisterInterruptStatus);
    MaskedStatus = InterruptStatus & Controller->EnabledInterrupts;
    if (MaskedStatus == 0) {
        return InterruptStatusNotClaimed;
    }

    SD_WRITE_REGISTER(Controller, SdRegisterInterruptStatus, MaskedStatus);
    RtlAtomicOr32(&(Controller->PendingStatusBits), MaskedStatus);
    return InterruptStatusClaimed;
}

SD_API
INTERRUPT_STATUS
SdStandardInterruptServiceDispatch (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the interrupt handler that is called at dispatch
    level.

Arguments:

    Context - Supplies a context pointer, which in this case is a pointer to
        the SD controller.

Return Value:

    None.

--*/

{

    UINTN BytesCompleted;
    PVOID CompletionContext;
    PSD_IO_COMPLETION_ROUTINE CompletionRoutine;
    PSD_CONTROLLER Controller;
    BOOL Inserted;
    ULONG PendingBits;
    BOOL Removed;
    KSTATUS Status;

    Controller = (PSD_CONTROLLER)(Context);
    PendingBits = RtlAtomicExchange32(&(Controller->PendingStatusBits), 0);
    if (PendingBits == 0) {
        return InterruptStatusNotClaimed;
    }

    //
    // Process a media change.
    //

    Status = STATUS_DEVICE_IO_ERROR;
    Inserted = FALSE;
    Removed = FALSE;
    if ((PendingBits & SD_INTERRUPT_STATUS_CARD_REMOVAL) != 0) {
        Removed = TRUE;
        Status = STATUS_NO_MEDIA;
        RtlAtomicAnd32(&(Controller->Flags), ~SD_CONTROLLER_FLAG_MEDIA_PRESENT);
    }

    if ((PendingBits & SD_INTERRUPT_STATUS_CARD_INSERTION) != 0) {
        Inserted = TRUE;
        Status = STATUS_NO_MEDIA;
    }

    //
    // Process the I/O completion. The only other interrupt bits that are sent
    // to the DPC are the error bits and the transfer complete bit.
    //

    if ((PendingBits & SD_INTERRUPT_ENABLE_ERROR_MASK) != 0) {
        RtlDebugPrint("SD: Error status 0x%x\n", PendingBits);
        Status = STATUS_DEVICE_IO_ERROR;

    } else if ((PendingBits & SD_INTERRUPT_STATUS_TRANSFER_COMPLETE) != 0) {
        Status = STATUS_SUCCESS;
    }

    if (Controller->IoCompletionRoutine != NULL) {
        CompletionRoutine = Controller->IoCompletionRoutine;
        CompletionContext = Controller->IoCompletionContext;
        BytesCompleted = Controller->IoRequestSize;
        Controller->IoCompletionRoutine = NULL;
        Controller->IoCompletionContext = NULL;
        Controller->IoRequestSize = 0;
        CompletionRoutine(Controller,
                          CompletionContext,
                          BytesCompleted,
                          Status);
    }

    if (((Inserted != FALSE) || (Removed != FALSE)) &&
        (Controller->FunctionTable.MediaChangeCallback != NULL)) {

        Controller->FunctionTable.MediaChangeCallback(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   Removed,
                                                   Inserted);
    }

    return InterruptStatusClaimed;
}

SD_API
KSTATUS
SdStandardInitializeDma (
    PSD_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine initializes standard DMA support in the host controller.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_SUPPORTED if the host controller does not support ADMA2.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

    STATUS_NO_MEDIA if there is no card in the slot.

--*/

{

    PSD_ADMA2_DESCRIPTOR Descriptor;
    ULONG IoBufferFlags;
    ULONG Value;

    //
    // The library's DMA implementation is only supported on standard SD/MMC
    // host controllers. A standard base must be present.
    //

    if (Controller->ControllerBase == NULL) {

        ASSERT(Controller->ControllerBase != NULL);

        return STATUS_NOT_SUPPORTED;
    }

    if ((Controller->Flags & SD_CONTROLLER_FLAG_MEDIA_PRESENT) == 0) {
        return STATUS_NO_MEDIA;
    }

    if ((Controller->HostCapabilities & SD_MODE_AUTO_CMD12) == 0) {
        RtlDebugPrint("SD: No DMA because Auto CMD12 is missing.\n");
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Enable ADMA2 mode if available.
    //

    if ((Controller->HostCapabilities & SD_MODE_ADMA2) != 0) {

        //
        // Create the DMA descriptor table if not already done.
        //

        if (Controller->DmaDescriptorTable == NULL) {
            IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS |
                            IO_BUFFER_FLAG_MAP_NON_CACHED;

            Controller->DmaDescriptorTable = MmAllocateNonPagedIoBuffer(
                                                0,
                                                MAX_ULONG,
                                                4,
                                                SD_ADMA2_DESCRIPTOR_TABLE_SIZE,
                                                IoBufferFlags);

            if (Controller->DmaDescriptorTable == NULL) {
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            ASSERT(Controller->DmaDescriptorTable->FragmentCount == 1);
        }

        Descriptor = Controller->DmaDescriptorTable->Fragment[0].VirtualAddress;
        RtlZeroMemory(Descriptor, SD_ADMA2_DESCRIPTOR_TABLE_SIZE);

        //
        // Enable ADMA2 in the host control register.
        //

        Value = SD_READ_REGISTER(Controller, SdRegisterHostControl);
        Value &= ~SD_HOST_CONTROL_DMA_MODE_MASK;
        Value |= SD_HOST_CONTROL_32BIT_ADMA2;
        SD_WRITE_REGISTER(Controller, SdRegisterHostControl, Value);

        //
        // Read it to make sure the write stuck.
        //

        Value = SD_READ_REGISTER(Controller, SdRegisterHostControl);
        if ((Value & SD_HOST_CONTROL_DMA_MODE_MASK) !=
            SD_HOST_CONTROL_32BIT_ADMA2) {

            return STATUS_NOT_SUPPORTED;
        }

        //
        // ADMA requires the DMA bit to be set in the command register.
        //

        RtlAtomicOr32(&(Controller->Flags),
                      SD_CONTROLLER_FLAG_DMA_COMMAND_ENABLED);

    //
    // Enable SDMA mode if ADMA2 mode is not around.
    //

    } else if ((Controller->HostCapabilities & SD_MODE_SDMA) != 0) {
        Value = SD_READ_REGISTER(Controller, SdRegisterHostControl);
        Value &= ~SD_HOST_CONTROL_DMA_MODE_MASK;
        SD_WRITE_REGISTER(Controller, SdRegisterHostControl, Value);

        //
        // SDMA requires the DMA bit to be set in the command register.
        //

        RtlAtomicOr32(&(Controller->Flags),
                      SD_CONTROLLER_FLAG_DMA_COMMAND_ENABLED);

    //
    // Pure system DMA is the simplest form where the DMA engine reads/writes
    // the data port register. No settings need to be updated in the
    // controller's register. Fail if system DMA is not available.
    //

    } else if ((Controller->HostCapabilities & SD_MODE_SYSTEM_DMA) == 0) {
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Record that DMA is enabled in the host controller.
    //

    RtlAtomicOr32(&(Controller->Flags), SD_CONTROLLER_FLAG_DMA_ENABLED);
    return STATUS_SUCCESS;
}

SD_API
VOID
SdStandardBlockIoDma (
    PSD_CONTROLLER Controller,
    ULONGLONG BlockOffset,
    UINTN BlockCount,
    PIO_BUFFER IoBuffer,
    UINTN IoBufferOffset,
    BOOL Write,
    PSD_IO_COMPLETION_ROUTINE CompletionRoutine,
    PVOID CompletionContext
    )

/*++

Routine Description:

    This routine performs a block I/O read or write using standard ADMA2.

Arguments:

    Controller - Supplies a pointer to the controller.

    BlockOffset - Supplies the logical block address of the I/O.

    BlockCount - Supplies the number of blocks to read or write.

    IoBuffer - Supplies a pointer to the buffer containing the data to write
        or where the read data should be returned.

    IoBufferOffset - Supplies the offset from the beginning of the I/O buffer
        where this I/O should begin. This is relative to the I/O buffer's
        current offset.

    Write - Supplies a boolean indicating if this is a read operation (FALSE)
        or a write operation.

    CompletionRoutine - Supplies a pointer to a function to call when the I/O
        completes.

    CompletionContext - Supplies a context pointer to pass as a parameter to
        the completion routine.

Return Value:

    None. The status of the operation is returned when the completion routine
    is called, which may be during the execution of this function in the case
    of an early failure.

--*/

{

    ULONG BlockLength;
    PHYSICAL_ADDRESS Boundary;
    SD_COMMAND Command;
    ULONG DescriptorCount;
    UINTN DescriptorSize;
    PSD_ADMA2_DESCRIPTOR DmaDescriptor;
    PIO_BUFFER DmaDescriptorTable;
    PIO_BUFFER_FRAGMENT Fragment;
    UINTN FragmentIndex;
    UINTN FragmentOffset;
    PHYSICAL_ADDRESS PhysicalAddress;
    KSTATUS Status;
    ULONG TableAddress;
    UINTN TransferSize;
    UINTN TransferSizeRemaining;

    ASSERT(BlockCount != 0);
    ASSERT(Controller->ControllerBase != NULL);

    if ((Controller->Flags & SD_CONTROLLER_FLAG_MEDIA_CHANGED) != 0) {
        Status = STATUS_MEDIA_CHANGED;
        goto BlockIoDmaEnd;

    } else if ((Controller->Flags & SD_CONTROLLER_FLAG_MEDIA_PRESENT) == 0) {
        Status = STATUS_NO_MEDIA;
        goto BlockIoDmaEnd;
    }

    if (Write != FALSE) {
        if (BlockCount > 1) {
            Command.Command = SdCommandWriteMultipleBlocks;

        } else {
            Command.Command = SdCommandWriteSingleBlock;
        }

        BlockLength = Controller->WriteBlockLength;
        TransferSize = BlockCount * BlockLength;

        ASSERT(TransferSize != 0);

    } else {
        if (BlockCount > 1) {
            Command.Command = SdCommandReadMultipleBlocks;

        } else {
            Command.Command = SdCommandReadSingleBlock;
        }

        BlockLength = Controller->ReadBlockLength;
        TransferSize = BlockCount * BlockLength;

        ASSERT(TransferSize != 0);

    }

    //
    // Get to the correct spot in the I/O buffer.
    //

    IoBufferOffset += MmGetIoBufferCurrentOffset(IoBuffer);
    FragmentIndex = 0;
    FragmentOffset = 0;
    while (IoBufferOffset != 0) {

        ASSERT(FragmentIndex < IoBuffer->FragmentCount);

        Fragment = &(IoBuffer->Fragment[FragmentIndex]);
        if (IoBufferOffset < Fragment->Size) {
            FragmentOffset = IoBufferOffset;
            break;
        }

        IoBufferOffset -= Fragment->Size;
        FragmentIndex += 1;
    }

    //
    // Fill out the DMA descriptors for ADMA2.
    //

    TransferSizeRemaining = TransferSize;
    if ((Controller->HostCapabilities & SD_MODE_ADMA2) != 0) {
        DmaDescriptorTable = Controller->DmaDescriptorTable;
        DmaDescriptor = DmaDescriptorTable->Fragment[0].VirtualAddress;
        DescriptorCount = 0;
        while ((TransferSizeRemaining != 0) &&
               (DescriptorCount < SD_ADMA2_DESCRIPTOR_COUNT - 1)) {

            ASSERT(FragmentIndex < IoBuffer->FragmentCount);

            Fragment = &(IoBuffer->Fragment[FragmentIndex]);

            //
            // This descriptor size is going to the the minimum of the total
            // remaining size, the size that can fit in a DMA descriptor, and
            // the remaining size of the fragment.
            //

            DescriptorSize = TransferSizeRemaining;
            if (DescriptorSize > SD_ADMA2_MAX_TRANSFER_SIZE) {
                DescriptorSize = SD_ADMA2_MAX_TRANSFER_SIZE;
            }

            if (DescriptorSize > Fragment->Size - FragmentOffset) {
                DescriptorSize = Fragment->Size - FragmentOffset;
            }

            TransferSizeRemaining -= DescriptorSize;
            PhysicalAddress = Fragment->PhysicalAddress + FragmentOffset;

            //
            // Assert that the buffer is within the first 4GB.
            //

            ASSERT(((ULONG)PhysicalAddress == PhysicalAddress) &&
                   ((ULONG)(PhysicalAddress + DescriptorSize) ==
                    PhysicalAddress + DescriptorSize));

            DmaDescriptor->Address = PhysicalAddress;
            DmaDescriptor->Attributes = SD_ADMA2_VALID |
                                        SD_ADMA2_ACTION_TRANSFER |
                                        (DescriptorSize <<
                                         SD_ADMA2_LENGTH_SHIFT);

            DmaDescriptor += 1;
            DescriptorCount += 1;
            FragmentOffset += DescriptorSize;
            if (FragmentOffset >= Fragment->Size) {
                FragmentIndex += 1;
                FragmentOffset = 0;
            }
        }

        //
        // Mark the last DMA descriptor as the end of the transfer.
        //

        DmaDescriptor -= 1;
        DmaDescriptor->Attributes |= SD_ADMA2_INTERRUPT | SD_ADMA2_END;
        RtlMemoryBarrier();
        TableAddress = (ULONG)(DmaDescriptorTable->Fragment[0].PhysicalAddress);
        SD_WRITE_REGISTER(Controller, SdRegisterAdmaAddressLow, TableAddress);

    //
    // If system DMA is active, assume that the whole transfer can occur.
    //

    } else if ((Controller->HostCapabilities & SD_MODE_SYSTEM_DMA) != 0) {
        TransferSizeRemaining = 0;

    //
    // Perform a single SDMA transfer. The transfer will stop on SDMA
    // boundaries, so limit this transfer to that next boundary.
    //

    } else {
        Fragment = &(IoBuffer->Fragment[FragmentIndex]);
        PhysicalAddress = Fragment->PhysicalAddress + FragmentOffset;
        Boundary = ALIGN_RANGE_DOWN(PhysicalAddress + SD_SDMA_MAX_TRANSFER_SIZE,
                                    SD_SDMA_MAX_TRANSFER_SIZE);

        DescriptorSize = Boundary - PhysicalAddress;
        if (DescriptorSize > Fragment->Size - FragmentOffset) {
            DescriptorSize = Fragment->Size - FragmentOffset;
        }

        if (DescriptorSize > TransferSizeRemaining) {
            DescriptorSize = TransferSizeRemaining;
        }

        //
        // The physical region had better be in the first 4GB.
        //

        ASSERT(((ULONG)PhysicalAddress == PhysicalAddress) &&
               ((ULONG)(PhysicalAddress + DescriptorSize) ==
                PhysicalAddress + DescriptorSize));

        SD_WRITE_REGISTER(Controller, SdRegisterSdmaAddress, PhysicalAddress);
        TransferSizeRemaining -= DescriptorSize;
    }

    Command.ResponseType = SD_RESPONSE_R1;
    if ((Controller->Flags & SD_CONTROLLER_FLAG_HIGH_CAPACITY) != 0) {
        Command.CommandArgument = BlockOffset;

    } else {
        Command.CommandArgument = BlockOffset * BlockLength;
    }

    ASSERT((TransferSize - TransferSizeRemaining) <= MAX_ULONG);

    Command.BufferSize = (ULONG)(TransferSize - TransferSizeRemaining);
    Command.BufferVirtual = NULL;
    Command.BufferPhysical = INVALID_PHYSICAL_ADDRESS;
    Command.Write = Write;
    Command.Dma = TRUE;
    Controller->IoCompletionRoutine = CompletionRoutine;
    Controller->IoCompletionContext = CompletionContext;
    Controller->IoRequestSize = Command.BufferSize;
    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

    if (!KSUCCESS(Status)) {
        Controller->IoCompletionRoutine = NULL;
        Controller->IoCompletionContext = NULL;
        Controller->IoRequestSize = 0;
        goto BlockIoDmaEnd;
    }

    Status = STATUS_SUCCESS;

BlockIoDmaEnd:

    //
    // If this routine failed, call the completion routine back immediately.
    //

    if (!KSUCCESS(Status)) {
        CompletionRoutine(Controller, CompletionContext, 0, Status);
    }

    return;
}

SD_API
KSTATUS
SdStandardInitializeController (
    PSD_CONTROLLER Controller,
    PVOID Context,
    ULONG Phase
    )

/*++

Routine Description:

    This routine performs any controller specific initialization steps.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Phase - Supplies the phase of initialization. Phase 0 happens after the
        initial software reset and Phase 1 happens after the bus width has been
        set to 1 and the speed to 400KHz.

Return Value:

    Status code.

--*/

{

    ULONG Capabilities;
    ULONG HostControl;
    KSTATUS Status;
    ULONG Value;

    //
    // Phase 0 is an early initialization phase that happens after the
    // controller has been set. It is used to gather capabilities and set
    // certain parameters in the hardware.
    //

    if (Phase == 0) {

        //
        // Get the host controller version.
        //

        Value = SD_READ_REGISTER(Controller, SdRegisterSlotStatusVersion) >> 16;
        Controller->HostVersion = Value & SD_HOST_VERSION_MASK;

        //
        // Evaluate the capabilities and add them to the controller's host
        // capabilities that may or may not have been supplied by the main
        // driver.
        //

        Capabilities = SD_READ_REGISTER(Controller, SdRegisterCapabilities);
        if ((Capabilities & SD_CAPABILITY_ADMA2) != 0) {
            Controller->HostCapabilities |= SD_MODE_ADMA2;
        }

        if ((Capabilities & SD_CAPABILITY_SDMA) != 0) {
            Controller->HostCapabilities |= SD_MODE_SDMA;
        }

        if ((Capabilities & SD_CAPABILITY_HIGH_SPEED) != 0) {
            Controller->HostCapabilities |= SD_MODE_HIGH_SPEED |
                                            SD_MODE_HIGH_SPEED_52MHZ;
        }

        //
        // Setup the voltage support if not supplied on creation.
        //

        if (Controller->Voltages == 0) {
            if ((Capabilities & SD_CAPABILITY_VOLTAGE_1V8) != 0) {
                Controller->Voltages |= SD_VOLTAGE_165_195 | SD_VOLTAGE_18;
            }

            if ((Capabilities & SD_CAPABILITY_VOLTAGE_3V0) != 0) {
                Controller->Voltages |= SD_VOLTAGE_29_30 | SD_VOLTAGE_30_31;
            }

            if ((Capabilities & SD_CAPABILITY_VOLTAGE_3V3) != 0) {
                Controller->Voltages |= SD_VOLTAGE_32_33 | SD_VOLTAGE_33_34;
            }
        }

        if (Controller->Voltages == 0) {
            Status = STATUS_DEVICE_NOT_CONNECTED;
            goto InitializeControllerEnd;
        }

        //
        // Get the host control power settings from the controller voltages.
        // Some devices do not have a capabilities register.
        //

        if ((Controller->Voltages & (SD_VOLTAGE_32_33 | SD_VOLTAGE_33_34)) ==
            (SD_VOLTAGE_32_33 | SD_VOLTAGE_33_34)) {

            HostControl = SD_HOST_CONTROL_POWER_3V3;

        } else if ((Controller->Voltages &
                    (SD_VOLTAGE_29_30 | SD_VOLTAGE_30_31)) ==
                   (SD_VOLTAGE_29_30 | SD_VOLTAGE_30_31)) {

            HostControl = SD_HOST_CONTROL_POWER_3V0;

        } else if ((Controller->Voltages &
                    (SD_VOLTAGE_165_195 | SD_VOLTAGE_18)) != 0) {

            HostControl = SD_HOST_CONTROL_POWER_1V8;

        } else {
            Status = STATUS_DEVICE_NOT_CONNECTED;
            goto InitializeControllerEnd;
        }

        SD_WRITE_REGISTER(Controller, SdRegisterHostControl, HostControl);

        //
        // Set the base clock frequency if not supplied on creation.
        //

        if (Controller->FundamentalClock == 0) {
            if (Controller->HostVersion >= SdHostVersion3) {
                Controller->FundamentalClock =
                  ((Capabilities >> SD_CAPABILITY_BASE_CLOCK_FREQUENCY_SHIFT) &
                   SD_CAPABILITY_V3_BASE_CLOCK_FREQUENCY_MASK) * 1000000;

            } else {
                Controller->FundamentalClock =
                  ((Capabilities >> SD_CAPABILITY_BASE_CLOCK_FREQUENCY_SHIFT) &
                   SD_CAPABILITY_BASE_CLOCK_FREQUENCY_MASK) * 1000000;
            }
        }

        if (Controller->FundamentalClock == 0) {
            Status = STATUS_DEVICE_NOT_CONNECTED;
            goto InitializeControllerEnd;
        }

    //
    // Phase 1 happens right before the initialization command sequence is
    // about to begin. The clock and bus width have been program and the device
    // is just about read to go.
    //

    } else if (Phase == 1) {
        HostControl = SD_READ_REGISTER(Controller, SdRegisterHostControl);
        HostControl |= SD_HOST_CONTROL_POWER_ENABLE;
        SD_WRITE_REGISTER(Controller, SdRegisterHostControl, HostControl);
        Value = SD_INTERRUPT_STATUS_ENABLE_DEFAULT_MASK;
        SD_WRITE_REGISTER(Controller, SdRegisterInterruptStatusEnable, Value);
        Controller->EnabledInterrupts = SD_INTERRUPT_ENABLE_DEFAULT_MASK;
        SD_WRITE_REGISTER(Controller,
                          SdRegisterInterruptSignalEnable,
                          Controller->EnabledInterrupts);
    }

    Status = STATUS_SUCCESS;

InitializeControllerEnd:
    return Status;
}

SD_API
KSTATUS
SdStandardResetController (
    PSD_CONTROLLER Controller,
    PVOID Context,
    ULONG Flags
    )

/*++

Routine Description:

    This routine performs a soft reset of the SD controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Flags - Supplies a bitmask of reset flags. See SD_RESET_FLAG_* for
        definitions.

Return Value:

    Status code.

--*/

{

    ULONG ResetBits;
    KSTATUS Status;
    ULONGLONG Timeout;
    ULONG Value;

    ResetBits = 0;
    if ((Flags & SD_RESET_FLAG_ALL) != 0) {
        ResetBits |= SD_CLOCK_CONTROL_RESET_ALL;
    }

    if ((Flags & SD_RESET_FLAG_COMMAND_LINE) != 0) {
        ResetBits |= SD_CLOCK_CONTROL_RESET_COMMAND_LINE;
    }

    if ((Flags & SD_RESET_FLAG_DATA_LINE) != 0) {
        ResetBits |= SD_CLOCK_CONTROL_RESET_DATA_LINE;
    }

    Value = SD_READ_REGISTER(Controller, SdRegisterClockControl);
    SD_WRITE_REGISTER(Controller, SdRegisterClockControl, Value | ResetBits);
    Timeout = SdQueryTimeCounter(Controller) + Controller->Timeout;
    Status = STATUS_TIMEOUT;
    do {
        Value = SD_READ_REGISTER(Controller, SdRegisterClockControl);
        if ((Value & ResetBits) == 0) {
            Status = STATUS_SUCCESS;
            break;
        }

    } while (SdQueryTimeCounter(Controller) <= Timeout);

    SD_WRITE_REGISTER(Controller, SdRegisterInterruptStatusEnable, 0xFFFFFFFF);
    SD_WRITE_REGISTER(Controller, SdRegisterInterruptStatus, 0xFFFFFFFF);
    return Status;
}

SD_API
KSTATUS
SdStandardSendCommand (
    PSD_CONTROLLER Controller,
    PVOID Context,
    PSD_COMMAND Command
    )

/*++

Routine Description:

    This routine sends the given command to the card.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Command - Supplies a pointer to the command parameters.

Return Value:

    Status code.

--*/

{

    ULONG BlockCount;
    ULONG Flags;
    ULONG InhibitMask;
    KSTATUS Status;
    ULONGLONG Timeout;
    ULONG Value;

    //
    // Set the DMA interrupts appropriately based on the command.
    //

    SdpSetDmaInterrupts(Controller, Command->Dma, Command->BufferSize);

    //
    // Don't wait for the data inhibit flag if this is the abort command.
    //

    InhibitMask = SD_STATE_DATA_INHIBIT | SD_STATE_COMMAND_INHIBIT;
    if ((Command->Command == SdCommandStopTransmission) &&
        (Command->ResponseType != SD_RESPONSE_R1B)) {

        InhibitMask = SD_STATE_COMMAND_INHIBIT;
    }

    //
    // Wait for the previous command to complete.
    //

    Timeout = 0;
    Status = STATUS_TIMEOUT;
    do {
        Value = SD_READ_REGISTER(Controller, SdRegisterPresentState);
        if ((Value & InhibitMask) == 0) {
            Status = STATUS_SUCCESS;
            break;

        } else if (Timeout == 0) {
            Timeout = SdQueryTimeCounter(Controller) + Controller->Timeout;
        }

    } while (SdQueryTimeCounter(Controller) <= Timeout);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Data or commands inhibited: 0x%x\n", Value);
        goto SendCommandEnd;
    }

    //
    // Clear any interrupts from the prevoius command before proceeding.
    //

    SD_WRITE_REGISTER(Controller,
                      SdRegisterInterruptStatus,
                      SD_INTERRUPT_STATUS_ALL_MASK);

    //
    // Set up the expected response flags.
    //

    Flags = 0;
    if ((Command->ResponseType & SD_RESPONSE_PRESENT) != 0) {
        if ((Command->ResponseType & SD_RESPONSE_136_BIT) != 0) {
            Flags |= SD_COMMAND_RESPONSE_136;

        } else if ((Command->ResponseType & SD_RESPONSE_BUSY) != 0) {
            Flags |= SD_COMMAND_RESPONSE_48_BUSY;

        } else {
            Flags |= SD_COMMAND_RESPONSE_48;
        }
    }

    //
    // Set up the remainder of the command flags.
    //

    if ((Command->ResponseType & SD_RESPONSE_VALID_CRC) != 0) {
        Flags |= SD_COMMAND_CRC_CHECK_ENABLE;
    }

    if ((Command->ResponseType & SD_RESPONSE_OPCODE) != 0) {
        Flags |= SD_COMMAND_COMMAND_INDEX_CHECK_ENABLE;
    }

    //
    // If there's a data buffer, program the block count.
    //

    if (Command->BufferSize != 0) {
        if ((Command->Command == SdCommandReadMultipleBlocks) ||
            (Command->Command == SdCommandWriteMultipleBlocks)) {

            Flags |= SD_COMMAND_MULTIPLE_BLOCKS |
                     SD_COMMAND_BLOCK_COUNT_ENABLE;

            BlockCount = Command->BufferSize / SD_BLOCK_SIZE;

            ASSERT(BlockCount <= SD_MAX_BLOCK_COUNT);

            Value = SD_BLOCK_SIZE |
                    SD_SIZE_SDMA_BOUNDARY_512K |
                    (BlockCount << 16);

            SD_WRITE_REGISTER(Controller, SdRegisterBlockSizeCount, Value);

            //
            // Prefer CMD23 if the card and the host support it.
            //

            if (((Controller->CardCapabilities & SD_MODE_CMD23) != 0) &&
                (Controller->HostVersion == SdHostVersion3)) {

                Flags |= SD_COMMAND_AUTO_COMMAND23_ENABLE;
                SD_WRITE_REGISTER(Controller, SdRegisterArgument2, BlockCount);

            //
            // Fall back to auto CMD12 to explicitly stop open ended
            // reads/writes.
            //

            } else if ((Controller->HostCapabilities &
                        SD_MODE_AUTO_CMD12) != 0) {

                Flags |= SD_COMMAND_AUTO_COMMAND12_ENABLE;
            }

        } else {

            ASSERT(Command->BufferSize <= SD_BLOCK_SIZE);

            Value = Command->BufferSize | SD_SIZE_SDMA_BOUNDARY_512K;
            SD_WRITE_REGISTER(Controller, SdRegisterBlockSizeCount, Value);
        }

        Flags |= SD_COMMAND_DATA_PRESENT;
        if (Command->Write != FALSE) {
            Flags |= SD_COMMAND_TRANSFER_WRITE;

        } else {
            Flags |= SD_COMMAND_TRANSFER_READ;
        }

        if ((Controller->Flags & SD_CONTROLLER_FLAG_DMA_COMMAND_ENABLED) != 0) {
            Flags |= SD_COMMAND_DMA_ENABLE;
        }
    }

    SD_WRITE_REGISTER(Controller,
                      SdRegisterArgument1,
                      Command->CommandArgument);

    Value = (Command->Command << SD_COMMAND_INDEX_SHIFT) | Flags;
    SD_WRITE_REGISTER(Controller, SdRegisterCommand, Value);

    //
    // If this was a DMA command, just let it sail away.
    //

    if (Command->Dma != FALSE) {

        ASSERT((Controller->Flags & SD_CONTROLLER_FLAG_DMA_ENABLED) != 0);

        Status = STATUS_SUCCESS;
        goto SendCommandEnd;
    }

    //
    // Worry about waiting for the status flag if the ISR is also clearing it.
    //

    ASSERT(Controller->EnabledInterrupts == SD_INTERRUPT_ENABLE_DEFAULT_MASK);

    Timeout = 0;
    Status = STATUS_TIMEOUT;
    do {
        Value = SD_READ_REGISTER(Controller, SdRegisterInterruptStatus);
        if (Value != 0) {
            Status = STATUS_SUCCESS;
            break;

        } else if (Timeout == 0) {
            Timeout = SdQueryTimeCounter(Controller) + Controller->Timeout;
        }

    } while (SdQueryTimeCounter(Controller) <= Timeout);

    if (!KSUCCESS(Status)) {
        goto SendCommandEnd;
    }

    if ((Value & SD_INTERRUPT_STATUS_COMMAND_TIMEOUT_ERROR) != 0) {
        Controller->FunctionTable.ResetController(Controller,
                                                  Controller->ConsumerContext,
                                                  SD_RESET_FLAG_COMMAND_LINE);

        Status = STATUS_TIMEOUT;
        goto SendCommandEnd;

    } else if ((Value & SD_INTERRUPT_STATUS_ERROR_INTERRUPT) != 0) {
        RtlDebugPrint("SD: Error sending command %d: Status 0x%x.\n",
                      Command->Command,
                      Value);

        Status = STATUS_DEVICE_IO_ERROR;
        goto SendCommandEnd;
    }

    if ((Value & SD_INTERRUPT_STATUS_COMMAND_COMPLETE) != 0) {
        SD_WRITE_REGISTER(Controller,
                          SdRegisterInterruptStatus,
                          SD_INTERRUPT_STATUS_COMMAND_COMPLETE);

        //
        // Get the response if there is one.
        //

        if ((Command->ResponseType & SD_RESPONSE_PRESENT) != 0) {
            if ((Command->ResponseType & SD_RESPONSE_136_BIT) != 0) {
                Command->Response[3] = SD_READ_REGISTER(Controller,
                                                        SdRegisterResponse10);

                Command->Response[2] = SD_READ_REGISTER(Controller,
                                                        SdRegisterResponse32);

                Command->Response[1] = SD_READ_REGISTER(Controller,
                                                        SdRegisterResponse54);

                Command->Response[0] = SD_READ_REGISTER(Controller,
                                                        SdRegisterResponse76);

                if ((Controller->HostCapabilities &
                     SD_MODE_RESPONSE136_SHIFTED) != 0) {

                    Command->Response[0] =
                                         (Command->Response[0] << 8) |
                                         ((Command->Response[1] >> 24) & 0xFF);

                    Command->Response[1] =
                                         (Command->Response[1] << 8) |
                                         ((Command->Response[2] >> 24) & 0xFF);

                    Command->Response[2] =
                                         (Command->Response[2] << 8) |
                                         ((Command->Response[3] >> 24) & 0xFF);

                    Command->Response[3] = Command->Response[3] << 8;
                }

            } else {
                Command->Response[0] = SD_READ_REGISTER(Controller,
                                                        SdRegisterResponse10);
            }
        }
    }

    if (Command->BufferSize != 0) {
        if (Command->Write != FALSE) {
            Status = SdpWriteData(Controller,
                                  Command->BufferVirtual,
                                  Command->BufferSize);

        } else {
            Status = SdpReadData(Controller,
                                 Command->BufferVirtual,
                                 Command->BufferSize);
        }

        if (!KSUCCESS(Status)) {
            goto SendCommandEnd;
        }
    }

    Status = STATUS_SUCCESS;

SendCommandEnd:
    return Status;
}

SD_API
KSTATUS
SdStandardGetSetBusWidth (
    PSD_CONTROLLER Controller,
    PVOID Context,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets the controller's bus width. The bus width is
    stored in the controller structure.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Set - Supplies a boolean indicating whether the bus width should be queried
        or set.

Return Value:

    Status code.

--*/

{

    ULONG Value;

    Value = SD_READ_REGISTER(Controller, SdRegisterHostControl);
    if (Set != FALSE) {
        Value &= ~SD_HOST_CONTROL_BUS_WIDTH_MASK;
        switch (Controller->BusWidth) {
        case 1:
            Value |= SD_HOST_CONTROL_DATA_1BIT;
            break;

        case 4:
            Value |= SD_HOST_CONTROL_DATA_4BIT;
            break;

        case 8:
            Value |= SD_HOST_CONTROL_DATA_8BIT;
            break;

        default:
            RtlDebugPrint("SD: Invalid bus width %d.\n", Controller->BusWidth);

            ASSERT(FALSE);

            return STATUS_INVALID_CONFIGURATION;
        }

        SD_WRITE_REGISTER(Controller, SdRegisterHostControl, Value);

    } else {
        if ((Value & SD_HOST_CONTROL_DATA_8BIT) != 0) {
            Controller->BusWidth = 8;

        } else if ((Value & SD_HOST_CONTROL_DATA_4BIT) != 0) {
            Controller->BusWidth = 4;

        } else {
            Controller->BusWidth = 1;
        }
    }

    return STATUS_SUCCESS;
}

SD_API
KSTATUS
SdStandardGetSetClockSpeed (
    PSD_CONTROLLER Controller,
    PVOID Context,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets the controller's clock speed. The clock speed is
    stored in the controller structure.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Set - Supplies a boolean indicating whether the clock speed should be
        queried (FALSE) or set (TRUE).

Return Value:

    Status code.

--*/

{

    ULONG ClockControl;
    ULONG Divisor;
    ULONG Result;
    KSTATUS Status;
    ULONGLONG Timeout;
    ULONG Value;

    ASSERT(Controller->FundamentalClock != 0);

    //
    // Getting the clock speed is not implemented as the divisor math might not
    // work out precisely in reverse.
    //

    if (Set == FALSE) {
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Find the right divisor, the highest frequency less than the desired
    // clock. Older controllers must be a power of 2.
    //

    if (Controller->HostVersion < SdHostVersion3) {
        Result = Controller->FundamentalClock;
        Divisor = 1;
        while (Divisor < SD_V2_MAX_DIVISOR) {
            if (Result <= Controller->ClockSpeed) {
                break;
            }

            Divisor <<= 1;
            Result >>= 1;
        }

        Divisor >>= 1;

    //
    // Version 3 divisors are a multiple of 2.
    //

    } else {
        if (Controller->ClockSpeed >= Controller->FundamentalClock) {
            Divisor = 0;

        } else {
            Divisor = 2;
            while (Divisor < SD_V3_MAX_DIVISOR) {
                if ((Controller->FundamentalClock / Divisor) <=
                    Controller->ClockSpeed) {

                    break;
                }

                Divisor += 2;
            }

            Divisor >>= 1;
        }
    }

    ClockControl = SD_CLOCK_CONTROL_DEFAULT_TIMEOUT <<
                   SD_CLOCK_CONTROL_TIMEOUT_SHIFT;

    SD_WRITE_REGISTER(Controller, SdRegisterClockControl, ClockControl);
    ClockControl |= (Divisor & SD_CLOCK_CONTROL_DIVISOR_MASK) <<
                    SD_CLOCK_CONTROL_DIVISOR_SHIFT;

    ClockControl |= (Divisor & SD_CLOCK_CONTROL_DIVISOR_HIGH_MASK) >>
                    SD_CLOCK_CONTROL_DIVISOR_HIGH_SHIFT;

    ClockControl |= SD_CLOCK_CONTROL_INTERNAL_CLOCK_ENABLE;
    SD_WRITE_REGISTER(Controller, SdRegisterClockControl, ClockControl);
    SD_WRITE_REGISTER(Controller, SdRegisterClockControl, ClockControl);
    Status = STATUS_TIMEOUT;
    Timeout = SdQueryTimeCounter(Controller) + Controller->Timeout;
    do {
        Value = SD_READ_REGISTER(Controller, SdRegisterClockControl);
        if ((Value & SD_CLOCK_CONTROL_CLOCK_STABLE) != 0) {
            Status = STATUS_SUCCESS;
            break;
        }

    } while (SdQueryTimeCounter(Controller) <= Timeout);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    ClockControl |= SD_CLOCK_CONTROL_SD_CLOCK_ENABLE;
    SD_WRITE_REGISTER(Controller, SdRegisterClockControl, ClockControl);
    return STATUS_SUCCESS;
}

SD_API
KSTATUS
SdStandardGetSetVoltage (
    PSD_CONTROLLER Controller,
    PVOID Context,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets the bus voltage. The bus voltage is
    stored in the controller structure.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Set - Supplies a boolean indicating whether the bus voltage should be
        queried (FALSE) or set (TRUE).

Return Value:

    Status code.

--*/

{

    ULONG Clock;
    ULONG Host1;
    ULONG Host2;
    ULONG PresentState;

    if (Set == FALSE) {
        Host2 = SD_READ_REGISTER(Controller, SdRegisterControlStatus2);
        if ((Host2 & SD_CONTROL_STATUS2_1_8V_ENABLE) != 0) {
            Controller->CurrentVoltage = SdVoltage1V8;

        } else {
            Controller->CurrentVoltage = SdVoltage3V3;
        }

        return STATUS_SUCCESS;
    }

    //
    // Stop the clock.
    //

    Clock = SD_READ_REGISTER(Controller, SdRegisterClockControl);
    Clock &= ~SD_CLOCK_CONTROL_SD_CLOCK_ENABLE;
    SD_WRITE_REGISTER(Controller, SdRegisterClockControl, Clock);

    //
    // If it's trying to go back to 3V, then flip it, wait and go.
    //

    if (Controller->CurrentVoltage != SdVoltage1V8) {
        Host2 = SD_READ_REGISTER(Controller, SdRegisterControlStatus2);
        Host2 &= ~SD_CONTROL_STATUS2_1_8V_ENABLE;
        SD_WRITE_REGISTER(Controller, SdRegisterControlStatus2, Host2);
        HlBusySpin(10000);
        Clock |= SD_CLOCK_CONTROL_SD_CLOCK_ENABLE;
        SD_WRITE_REGISTER(Controller, SdRegisterClockControl, Clock);
        return STATUS_SUCCESS;
    }

    ASSERT((Controller->Voltages & (SD_VOLTAGE_165_195 | SD_VOLTAGE_18)) != 0);

    //
    // Check that DAT[3:0] are clear.
    //

    PresentState = SD_READ_REGISTER(Controller, SdRegisterPresentState);
    if ((PresentState & SD_STATE_DATA_LINE_LEVEL_MASK) != 0) {
        return STATUS_NOT_READY;
    }

    //
    // Set 1.8V signalling enable.
    //

    if (Controller->HostVersion > SdHostVersion2) {
        Host2 = SD_READ_REGISTER(Controller, SdRegisterControlStatus2);
        Host2 |= SD_CONTROL_STATUS2_1_8V_ENABLE;
        SD_WRITE_REGISTER(Controller, SdRegisterControlStatus2, Host2);
    }

    Host1 = SD_READ_REGISTER(Controller, SdRegisterHostControl);
    Host1 &= ~SD_HOST_CONTROL_POWER_MASK;
    Host1 |= SD_HOST_CONTROL_POWER_1V8;
    SD_WRITE_REGISTER(Controller, SdRegisterHostControl, Host1);

    //
    // Wait at least 5 milliseconds as per spec.
    //

    HlBusySpin(10000);

    //
    // Re-enable the SD clock.
    //

    Clock |= SD_CLOCK_CONTROL_SD_CLOCK_ENABLE;
    SD_WRITE_REGISTER(Controller, SdRegisterClockControl, Clock);

    //
    // Wait at least 1ms as per spec.
    //

    HlBusySpin(2000);

    //
    // Ensure that the DAT lines are all set.
    //

    PresentState = SD_READ_REGISTER(Controller, SdRegisterPresentState);
    if ((PresentState & SD_STATE_DATA_LINE_LEVEL_MASK) !=
        SD_STATE_DATA_LINE_LEVEL_MASK) {

        RtlDebugPrint("SD: DAT[3:0] didn't confirm 1.8V switch.\n");
        Host1 = SD_READ_REGISTER(Controller, SdRegisterHostControl);
        Host1 &= ~(SD_HOST_CONTROL_POWER_ENABLE | SD_HOST_CONTROL_POWER_MASK);
        Host1 |= SD_HOST_CONTROL_POWER_3V3;
        SD_WRITE_REGISTER(Controller, SdRegisterHostControl, Host1);
        return STATUS_NOT_INITIALIZED;
    }

    //
    // The voltage switch is complete and the card accepted it.
    //

    return STATUS_SUCCESS;
}

SD_API
VOID
SdStandardStopDataTransfer (
    PSD_CONTROLLER Controller,
    PVOID Context
    )

/*++

Routine Description:

    This routine stops any current data transfer on the controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

Return Value:

    None.

--*/

{

    KSTATUS Status;
    ULONGLONG Timeout;
    ULONG Value;

    SdpSetDmaInterrupts(Controller, FALSE, 0);
    SD_WRITE_REGISTER(Controller,
                      SdRegisterInterruptStatus,
                      SD_INTERRUPT_STATUS_ALL_MASK);

    //
    // Stop any current transfer at a block gap.
    //

    Value = SD_READ_REGISTER(Controller, SdRegisterHostControl);
    Value |= SD_HOST_CONTROL_STOP_AT_BLOCK_GAP;
    SD_WRITE_REGISTER(Controller, SdRegisterHostControl, Value);

    //
    // Wait for the transfer to complete.
    //

    Timeout = SdQueryTimeCounter(Controller) + Controller->Timeout;
    Status = STATUS_TIMEOUT;
    do {
        Value = SD_READ_REGISTER(Controller, SdRegisterInterruptStatus);
        if ((Value & SD_INTERRUPT_STATUS_TRANSFER_COMPLETE) != 0) {
            Status = STATUS_SUCCESS;
            break;
        }

    } while (SdQueryTimeCounter(Controller) <= Timeout);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("SD: Stop at block gap timed out: 0x%08x\n", Value);
        goto StopDataTransferEnd;
    }

    //
    // Clear the transfer complete.
    //

    SD_WRITE_REGISTER(Controller,
                      SdRegisterInterruptStatus,
                      SD_INTERRUPT_STATUS_TRANSFER_COMPLETE);

StopDataTransferEnd:
    return;
}

SD_API
VOID
SdStandardMediaChangeCallback (
    PSD_CONTROLLER Controller,
    PVOID Context,
    BOOL Removal,
    BOOL Insertion
    )

/*++

Routine Description:

    This routine is called by the SD library to notify the user of the SD
    library that media has been removed, inserted, or both. This routine is
    called from a DPC and, as a result, can get called back at dispatch level.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Removal - Supplies a boolean indicating if a removal event has occurred.

    Insertion - Supplies a boolean indicating if an insertion event has
        occurred.

Return Value:

    None.

--*/

{

    ULONG Flags;

    Flags = 0;
    if (Removal != FALSE) {
        Flags |= SD_CONTROLLER_FLAG_REMOVAL_PENDING;
    }

    if (Insertion != FALSE) {
        Flags |= SD_CONTROLLER_FLAG_INSERTION_PENDING;
    }

    //
    // If there is something pending, then create and queue a work item.
    //

    if (Flags != 0) {
        RtlAtomicOr32(&(Controller->Flags), Flags);
        KeCreateAndQueueWorkItem(NULL,
                                 WorkPriorityNormal,
                                 SdpMediaChangeWorker,
                                 Controller);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
SdpReadData (
    PSD_CONTROLLER Controller,
    PVOID Data,
    ULONG Size
    )

/*++

Routine Description:

    This routine reads polled data from the SD controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    Data - Supplies a pointer to the buffer where the data will be read into.

    Size - Supplies the size in bytes. This must be a multiple of four bytes.
        It is also assumed that the size is a multiple of the read data length.

Return Value:

    Status code.

--*/

{

    PULONG Buffer32;
    ULONG Count;
    ULONG IoIndex;
    ULONG Mask;
    KSTATUS Status;
    ULONGLONG Timeout;
    ULONG Value;

    Buffer32 = (PULONG)Data;
    Count = Size;
    if (Count > SD_BLOCK_SIZE) {
        Count = SD_BLOCK_SIZE;
    }

    Count /= sizeof(ULONG);

    ASSERT(IS_ALIGNED(Size, sizeof(ULONG)) != FALSE);

    while (Size != 0) {

        //
        // Get the interrupt status register.
        //

        Timeout = 0;
        Status = STATUS_TIMEOUT;
        do {
            Value = SD_READ_REGISTER(Controller, SdRegisterInterruptStatus);
            if (Value != 0) {
                Status = STATUS_SUCCESS;
                break;
            }

            if (Timeout == 0) {
                Timeout = SdQueryTimeCounter(Controller) + Controller->Timeout;
            }

        } while (SdQueryTimeCounter(Controller) <= Timeout);

        if (!KSUCCESS(Status)) {
            return Status;
        }

        Mask = SD_INTERRUPT_STATUS_DATA_TIMEOUT_ERROR |
               SD_INTERRUPT_STATUS_DATA_CRC_ERROR |
               SD_INTERRUPT_STATUS_DATA_END_BIT_ERROR;

        if ((Value & Mask) != 0) {
            Controller->FunctionTable.ResetController(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   SD_RESET_FLAG_DATA_LINE);
        }

        if ((Value & SD_INTERRUPT_STATUS_ERROR_INTERRUPT) != 0) {
            RtlDebugPrint("SD: Data error on read: Status 0x%x\n", Value);
            return STATUS_DEVICE_IO_ERROR;
        }

        if ((Value & SD_INTERRUPT_STATUS_BUFFER_READ_READY) != 0) {

            //
            // Acknowledge this batch of interrupts.
            //

            SD_WRITE_REGISTER(Controller, SdRegisterInterruptStatus, Value);
            Value = 0;
            for (IoIndex = 0; IoIndex < Count; IoIndex += 1) {
                *Buffer32 = SD_READ_REGISTER(Controller,
                                             SdRegisterBufferDataPort);

                Buffer32 += 1;
            }

            Size -= Count * sizeof(ULONG);
        }
    }

    Value = SD_READ_REGISTER(Controller, SdRegisterInterruptStatus);
    Mask = SD_INTERRUPT_STATUS_BUFFER_WRITE_READY |
           SD_INTERRUPT_STATUS_TRANSFER_COMPLETE;

    if ((Value & Mask) != 0) {
        SD_WRITE_REGISTER(Controller, SdRegisterInterruptStatus, Value);
    }

    return STATUS_SUCCESS;
}

KSTATUS
SdpWriteData (
    PSD_CONTROLLER Controller,
    PVOID Data,
    ULONG Size
    )

/*++

Routine Description:

    This routine writes polled data to the SD controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    Data - Supplies a pointer to the buffer containing the data to write.

    Size - Supplies the size in bytes. This must be a multiple of 4 bytes. It
        is also assumed that the size is a multiple of the write data length.

Return Value:

    Status code.

--*/

{

    PULONG Buffer32;
    ULONG Count;
    ULONG IoIndex;
    ULONG Mask;
    KSTATUS Status;
    ULONGLONG Timeout;
    ULONG Value;

    Buffer32 = (PULONG)Data;
    Count = Size;
    if (Count > SD_BLOCK_SIZE) {
        Count = SD_BLOCK_SIZE;
    }

    Count /= sizeof(ULONG);

    ASSERT(IS_ALIGNED(Size, sizeof(ULONG)) != FALSE);

    while (Size != 0) {

        //
        // Get the interrupt status register.
        //

        Timeout = 0;
        Status = STATUS_TIMEOUT;
        do {
            Value = SD_READ_REGISTER(Controller, SdRegisterInterruptStatus);
            if (Value != 0) {
                Status = STATUS_SUCCESS;
                break;
            }

            if (Timeout == 0) {
                Timeout = SdQueryTimeCounter(Controller) + Controller->Timeout;
            }

        } while (SdQueryTimeCounter(Controller) <= Timeout);

        if (!KSUCCESS(Status)) {
            return Status;
        }

        Mask = SD_INTERRUPT_STATUS_DATA_TIMEOUT_ERROR |
               SD_INTERRUPT_STATUS_DATA_CRC_ERROR |
               SD_INTERRUPT_STATUS_DATA_END_BIT_ERROR;

        if ((Value & Mask) != 0) {
            Controller->FunctionTable.ResetController(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   SD_RESET_FLAG_DATA_LINE);
        }

        if ((Value & SD_INTERRUPT_STATUS_ERROR_INTERRUPT) != 0) {
            RtlDebugPrint("SD : Data error on write: Status 0x%x\n", Value);
            return STATUS_DEVICE_IO_ERROR;
        }

        if ((Value & SD_INTERRUPT_STATUS_BUFFER_WRITE_READY) != 0) {

            //
            // Acknowledge this batch of interrupts.
            //

            SD_WRITE_REGISTER(Controller, SdRegisterInterruptStatus, Value);
            Value = 0;
            for (IoIndex = 0; IoIndex < Count; IoIndex += 1) {
                SD_WRITE_REGISTER(Controller,
                                  SdRegisterBufferDataPort,
                                  *Buffer32);

                Buffer32 += 1;
            }

            Size -= Count * sizeof(ULONG);
        }
    }

    Mask = SD_INTERRUPT_STATUS_BUFFER_READ_READY |
           SD_INTERRUPT_STATUS_TRANSFER_COMPLETE;

    if ((Value & Mask) != 0) {
        SD_WRITE_REGISTER(Controller, SdRegisterInterruptStatus, Value);
    }

    return STATUS_SUCCESS;
}

VOID
SdpSetDmaInterrupts (
    PSD_CONTROLLER Controller,
    BOOL Enable,
    ULONG BufferSize
    )

/*++

Routine Description:

    This routine enables or disables interrupts necessary to perform block I/O
    via DMA. It is assumed that the caller has synchronized disk access on this
    controller and there are currently no DMA or polled operations in flight.

Arguments:

    Controller - Supplies a pointer to the controller.

    Enable - Supplies a boolean indicating if the DMA interrupts are to be
        enabled (TRUE) or disabled (FALSE).

    BufferSize - Supplies the length of the data buffer.

Return Value:

    None.

--*/

{

    ULONG Value;

    //
    // Enable the interrupts for transfer completion so that DMA operations
    // can complete asynchronously. Unless, of course, the DMA interrupts are
    // already enabled.
    //

    if (Enable != FALSE) {
        Value = Controller->EnabledInterrupts | SD_INTERRUPT_ENABLE_ERROR_MASK;
        Value &= ~(SD_INTERRUPT_ENABLE_TRANSFER_COMPLETE |
                   SD_INTERRUPT_ENABLE_COMMAND_COMPLETE);

        if (BufferSize != 0) {
            Value |= SD_INTERRUPT_ENABLE_TRANSFER_COMPLETE;

        } else {
            Value |= SD_INTERRUPT_ENABLE_COMMAND_COMPLETE;
        }

    //
    // Disable the DMA interrupts so that they do not interfere with polled I/O
    // attempts to check the transfer status. Do nothing if the DMA interrupts
    // are disabled.
    //

    } else {
        Value = Controller->EnabledInterrupts &
                ~(SD_INTERRUPT_ENABLE_ERROR_MASK |
                  SD_INTERRUPT_ENABLE_TRANSFER_COMPLETE |
                  SD_INTERRUPT_ENABLE_COMMAND_COMPLETE);
    }

    if (Value != Controller->EnabledInterrupts) {
        Controller->EnabledInterrupts = Value;
        SD_WRITE_REGISTER(Controller,
                          SdRegisterInterruptSignalEnable,
                          Controller->EnabledInterrupts);
    }

    return;
}

VOID
SdpMediaChangeWorker (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine processes a change event from the safety of a low level work
    item.

Arguments:

    Parameter - Supplies an optional parameter passed in by the creator of the
        work item. This must be an SD controller.

Return Value:

    None.

--*/

{

    PSD_CONTROLLER Controller;
    ULONG FlagsMask;

    //
    // Notify the system of an change if either the pending flags are set.
    //

    Controller = Parameter;
    FlagsMask = SD_CONTROLLER_FLAG_INSERTION_PENDING |
                SD_CONTROLLER_FLAG_REMOVAL_PENDING;

    if ((Controller->Flags & FlagsMask) != 0) {

        ASSERT(Controller->OsDevice != NULL);

        IoNotifyDeviceTopologyChange(Controller->OsDevice);
    }

    return;
}

