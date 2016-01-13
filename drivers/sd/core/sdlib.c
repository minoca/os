/*++

Copyright (c) 2014 Minoca Corp. All rights reserved.

Module Name:

    sdlib.c

Abstract:

    This module implements the library functionality for the SD/MMC driver.

Author:

    Evan Green 27-Feb-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/driver.h>
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

VOID
SdpInterruptServiceDpc (
    PDPC Dpc
    );

KSTATUS
SdpSetBusParameters (
    PSD_CONTROLLER Controller
    );

KSTATUS
SdpResetCard (
    PSD_CONTROLLER Controller
    );

KSTATUS
SdpGetInterfaceCondition (
    PSD_CONTROLLER Controller
    );

KSTATUS
SdpWaitForCardToInitialize (
    PSD_CONTROLLER Controller
    );

KSTATUS
SdpSetCrc (
    PSD_CONTROLLER Controller,
    BOOL Enable
    );

KSTATUS
SdpGetCardIdentification (
    PSD_CONTROLLER Controller,
    PSD_CARD_IDENTIFICATION Identification
    );

KSTATUS
SdpSetupAddressing (
    PSD_CONTROLLER Controller
    );

KSTATUS
SdpReadCardSpecificData (
    PSD_CONTROLLER Controller
    );

KSTATUS
SdpSelectCard (
    PSD_CONTROLLER Controller
    );

KSTATUS
SdpConfigureEraseGroup (
    PSD_CONTROLLER Controller
    );

KSTATUS
SdpGetExtendedCardSpecificData (
    PSD_CONTROLLER Controller,
    UCHAR Data[SD_MMC_MAX_BLOCK_SIZE]
    );

KSTATUS
SdpMmcSwitch (
    PSD_CONTROLLER Controller,
    UCHAR Index,
    UCHAR Value
    );

KSTATUS
SdpSdSwitch (
    PSD_CONTROLLER Controller,
    ULONG Mode,
    ULONG Group,
    UCHAR Value,
    ULONG Response[16]
    );

KSTATUS
SdpWaitForStateTransition (
    PSD_CONTROLLER Controller
    );

KSTATUS
SdpGetCardStatus (
    PSD_CONTROLLER Controller,
    PULONG Status
    );

KSTATUS
SdpSetSdFrequency (
    PSD_CONTROLLER Controller
    );

KSTATUS
SdpSetMmcFrequency (
    PSD_CONTROLLER Controller
    );

KSTATUS
SdpSetBlockLength (
    PSD_CONTROLLER Controller,
    ULONG BlockLength
    );

KSTATUS
SdpReadBlocksPolled (
    PSD_CONTROLLER Controller,
    ULONGLONG BlockOffset,
    ULONG BlockCount,
    PVOID BufferVirtual
    );

KSTATUS
SdpWriteBlocksPolled (
    PSD_CONTROLLER Controller,
    ULONGLONG BlockOffset,
    ULONG BlockCount,
    PVOID BufferVirtual
    );

KSTATUS
SdpSendCommand (
    PSD_CONTROLLER Controller,
    PSD_COMMAND Command
    );

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

KSTATUS
SdpResetController (
    PSD_CONTROLLER Controller,
    ULONG ResetBit
    );

KSTATUS
SdpErrorRecovery (
    PSD_CONTROLLER Controller
    );

KSTATUS
SdpSynchronousAbort (
    PSD_CONTROLLER Controller
    );

KSTATUS
SdpAsynchronousAbort (
    PSD_CONTROLLER Controller
    );

KSTATUS
SdpSendAbortCommand (
    PSD_CONTROLLER Controller
    );

VOID
SdpSetDmaInterrupts (
    PSD_CONTROLLER Controller,
    BOOL Enable
    );

ULONGLONG
SdpQueryTimeCounter (
    PSD_CONTROLLER Controller
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

SD_API
PSD_CONTROLLER
SdCreateController (
    PSD_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine creates a new SD controller object.

Arguments:

    Parameters - Supplies a pointer to the parameters to use when creating the
        controller. This can be stack allocated, as the SD library won't use
        this memory after this routine returns.

Return Value:

    Returns a pointer to the controller structure on success.

    NULL on allocation failure or if a required parameter was not filled in.

--*/

{

    PSD_CONTROLLER Controller;
    KSTATUS Status;

    if (Parameters->ControllerBase == NULL) {

        ASSERT(FALSE);

        return NULL;
    }

    Controller = MmAllocateNonPagedPool(sizeof(SD_CONTROLLER),
                                        SD_ALLOCATION_TAG);

    if (Controller == NULL) {
        return NULL;
    }

    RtlZeroMemory(Controller, sizeof(SD_CONTROLLER));
    KeInitializeSpinLock(&(Controller->InterruptLock));
    Controller->ControllerBase = Parameters->ControllerBase;
    Controller->ConsumerContext = Parameters->ConsumerContext;
    Controller->GetCardDetectStatus = Parameters->GetCardDetectStatus;
    Controller->GetWriteProtectStatus = Parameters->GetWriteProtectStatus;
    Controller->MediaChangeCallback = Parameters->MediaChangeCallback;
    Controller->Voltages = Parameters->Voltages;
    Controller->FundamentalClock = Parameters->FundamentalClock;
    Controller->HostCapabilities = Parameters->HostCapabilities;
    Controller->InterruptDpc = KeCreateDpc(SdpInterruptServiceDpc, Controller);
    if (Controller->InterruptDpc == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateControllerEnd;
    }

    Status = STATUS_SUCCESS;

CreateControllerEnd:
    if (!KSUCCESS(Status)) {
        if (Controller != NULL) {
            if (Controller->InterruptDpc != NULL) {
                KeDestroyDpc(Controller->InterruptDpc);
            }

            MmFreeNonPagedPool(Controller);
            Controller = NULL;
        }
    }

    return Controller;
}

SD_API
VOID
SdDestroyController (
    PSD_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine destroys an SD controller object.

Arguments:

    Controller - Supplies a pointer to the controller to destroy.

Return Value:

    None.

--*/

{

    if (Controller->InterruptDpc != NULL) {
        KeDestroyDpc(Controller->InterruptDpc);
    }

    MmFreeNonPagedPool(Controller);
    return;
}

SD_API
VOID
SdSetInterruptHandle (
    PSD_CONTROLLER Controller,
    HANDLE Handle
    )

/*++

Routine Description:

    This routine sets the interrupt handle of the SD controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    Handle - Supplies the interrupt handle.

Return Value:

    None.

--*/

{

    Controller->InterruptHandle = Handle;
    return;
}

SD_API
INTERRUPT_STATUS
SdInterruptService (
    PSD_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine implements the interrupt service routine for an SD controller.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Returns whether or not the SD controller caused the interrupt.

--*/

{

    ULONG InterruptStatus;
    ULONG MaskedStatus;
    ULONG OriginalPendingStatus;

    InterruptStatus = SD_READ_REGISTER(Controller, SdRegisterInterruptStatus);
    MaskedStatus = InterruptStatus & Controller->EnabledInterrupts;
    if (MaskedStatus == 0) {
        return InterruptStatusNotClaimed;
    }

    KeAcquireSpinLock(&(Controller->InterruptLock));
    OriginalPendingStatus = Controller->PendingStatusBits;
    Controller->PendingStatusBits |= MaskedStatus;
    if (OriginalPendingStatus == 0) {
        KeQueueDpc(Controller->InterruptDpc);
    }

    SD_WRITE_REGISTER(Controller, SdRegisterInterruptStatus, MaskedStatus);
    KeReleaseSpinLock(&(Controller->InterruptLock));
    return InterruptStatusClaimed;
}

SD_API
KSTATUS
SdInitializeController (
    PSD_CONTROLLER Controller,
    BOOL ResetController
    )

/*++

Routine Description:

    This routine resets and initializes the SD host controller.

Arguments:

    Controller - Supplies a pointer to the controller to initialize.

    ResetController - Supplies a boolean indicating whether or not to reset
        the controller.

Return Value:

    Status code.

--*/

{

    ULONG BusWidth;
    ULONG Capabilities;
    UCHAR CardData[SD_MMC_MAX_BLOCK_SIZE];
    SD_CARD_IDENTIFICATION CardIdentification;
    BOOL CardPresent;
    ULONG ExtendedCardDataWidth;
    ULONG HostControl;
    ULONG LoopIndex;
    KSTATUS Status;
    ULONG Value;

    RtlAtomicAnd32(&(Controller->Flags), 0);

    //
    // Start by checking for a card.
    //

    if (Controller->GetCardDetectStatus != NULL) {
        Status = Controller->GetCardDetectStatus(Controller,
                                                 Controller->ConsumerContext,
                                                 &CardPresent);

        if ((!KSUCCESS(Status)) || (CardPresent == FALSE)) {
            goto InitializeControllerEnd;
        }
    }

    //
    // Get the host controller version.
    //

    Value = SD_READ_REGISTER(Controller, SdRegisterSlotStatusVersion) >> 16;
    Controller->HostVersion = Value & SD_HOST_VERSION_MASK;

    //
    // Reset the controller and wait for the reset to finish.
    //

    if (ResetController != FALSE) {
        Status = SdpResetController(Controller, SD_CLOCK_CONTROL_RESET_ALL);
        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    Capabilities = SD_READ_REGISTER(Controller, SdRegisterCapabilities);
    if ((Capabilities & SD_CAPABILITY_ADMA2) != 0) {
        Controller->HostCapabilities |= SD_MODE_ADMA2;
    }

    //
    // Setup the voltage support if not supplied on creation.
    //

    if (Controller->Voltages == 0) {
        if ((Capabilities & SD_CAPABILITY_VOLTAGE_1V8) != 0) {
            Controller->Voltages |= SD_VOLTAGE_165_195;
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
    // Get the host control power settings from the controller voltages. Some
    // devices do not have a capabilities register.
    //

    if ((Controller->Voltages & (SD_VOLTAGE_32_33 | SD_VOLTAGE_33_34)) ==
        (SD_VOLTAGE_32_33 | SD_VOLTAGE_33_34)) {

        HostControl = SD_HOST_CONTROL_POWER_3V3;

    } else if ((Controller->Voltages & (SD_VOLTAGE_29_30 | SD_VOLTAGE_30_31)) ==
               (SD_VOLTAGE_29_30 | SD_VOLTAGE_30_31)) {

        HostControl = SD_HOST_CONTROL_POWER_3V0;

    } else if ((Controller->Voltages & SD_VOLTAGE_165_195) ==
               SD_VOLTAGE_165_195) {

        HostControl = SD_HOST_CONTROL_POWER_1V8;

    } else {
        Status = STATUS_DEVICE_NOT_CONNECTED;
        goto InitializeControllerEnd;
    }

    //
    // Set the default maximum number of blocks per transfer.
    //

    Controller->MaxBlocksPerTransfer = SD_MAX_BLOCK_COUNT;

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

    if ((Capabilities & SD_CAPABILITY_HIGH_SPEED) != 0) {
        Controller->HostCapabilities |= SD_MODE_HIGH_SPEED |
                                        SD_MODE_HIGH_SPEED_52MHZ;
    }

    SD_WRITE_REGISTER(Controller, SdRegisterHostControl, HostControl);
    Controller->BusWidth = 1;
    Controller->ClockSpeed = SdClock400kHz;
    Status = SdpSetBusParameters(Controller);
    if (!KSUCCESS(Status)) {
        goto InitializeControllerEnd;
    }

    HostControl = SD_READ_REGISTER(Controller, SdRegisterHostControl);
    HostControl |= SD_HOST_CONTROL_POWER_ENABLE;
    SD_WRITE_REGISTER(Controller, SdRegisterHostControl, HostControl);
    Value = SD_INTERRUPT_STATUS_ENABLE_DEFAULT_MASK;
    SD_WRITE_REGISTER(Controller, SdRegisterInterruptStatusEnable, Value);
    Controller->EnabledInterrupts = SD_INTERRUPT_ENABLE_DEFAULT_MASK;
    SD_WRITE_REGISTER(Controller,
                      SdRegisterInterruptSignalEnable,
                      Controller->EnabledInterrupts);

    Status = SdpWaitForCardToInitialize(Controller);
    if (!KSUCCESS(Status)) {
        goto InitializeControllerEnd;
    }

    if ((Controller->HostCapabilities & SD_MODE_SPI) != 0) {
        Status = SdpSetCrc(Controller, TRUE);
        if (!KSUCCESS(Status)) {
            goto InitializeControllerEnd;
        }
    }

    Status = SdpGetCardIdentification(Controller, &CardIdentification);
    if (!KSUCCESS(Status)) {
        goto InitializeControllerEnd;
    }

    Status = SdpSetupAddressing(Controller);
    if (!KSUCCESS(Status)) {
        goto InitializeControllerEnd;
    }

    Status = SdpReadCardSpecificData(Controller);
    if (!KSUCCESS(Status)) {
        goto InitializeControllerEnd;
    }

    Status = SdpSelectCard(Controller);
    if (!KSUCCESS(Status)) {
        goto InitializeControllerEnd;
    }

    Status = SdpConfigureEraseGroup(Controller);
    if (!KSUCCESS(Status)) {
        goto InitializeControllerEnd;
    }

    if (SD_IS_CONTROLLER_SD(Controller)) {
        Status = SdpSetSdFrequency(Controller);

    } else {
        Status = SdpSetMmcFrequency(Controller);
    }

    if (!KSUCCESS(Status)) {
        goto InitializeControllerEnd;
    }

    KeDelayExecution(FALSE, FALSE, 10000);

    //
    // Clip the card's capabilities to the host's.
    //

    Controller->CardCapabilities &= Controller->HostCapabilities;
    if (SD_IS_CONTROLLER_SD(Controller)) {
        if ((Controller->CardCapabilities & SD_MODE_4BIT) != 0) {
           Controller->BusWidth = 4;
        }

        Controller->ClockSpeed = SdClock25MHz;
        if ((Controller->CardCapabilities & SD_MODE_HIGH_SPEED) != 0) {
            Controller->ClockSpeed = SdClock50MHz;
        }

        Status = SdpSetBusParameters(Controller);
        if (!KSUCCESS(Status)) {
            goto InitializeControllerEnd;
        }

    } else {
        Status = STATUS_NOT_SUPPORTED;
        for (LoopIndex = 0; LoopIndex < 3; LoopIndex += 1) {
            if (LoopIndex == 0) {
                ExtendedCardDataWidth = SD_MMC_EXTENDED_CARD_DATA_BUS_WIDTH_8;
                BusWidth = 8;
                if ((Controller->HostCapabilities & SD_MODE_8BIT) == 0) {
                    continue;
                }

            } else if (LoopIndex == 1) {
                ExtendedCardDataWidth = SD_MMC_EXTENDED_CARD_DATA_BUS_WIDTH_4;
                BusWidth = 4;
                if ((Controller->HostCapabilities & SD_MODE_4BIT) == 0) {
                    continue;
                }

            } else {

                ASSERT(LoopIndex == 2);

                ExtendedCardDataWidth = SD_MMC_EXTENDED_CARD_DATA_BUS_WIDTH_1;
                BusWidth = 1;
            }

            Status = SdpMmcSwitch(Controller,
                                  SD_MMC_EXTENDED_CARD_DATA_BUS_WIDTH,
                                  ExtendedCardDataWidth);

            if (!KSUCCESS(Status)) {
                continue;
            }

            Controller->BusWidth = BusWidth;
            Status = SdpSetBusParameters(Controller);
            if (!KSUCCESS(Status)) {
                goto InitializeControllerEnd;
            }

            Status = SdpGetExtendedCardSpecificData(Controller, CardData);
            if (KSUCCESS(Status)) {
                if (BusWidth == 8) {
                    Controller->CardCapabilities |= SD_MODE_8BIT;

                } else if (BusWidth == 4) {
                    Controller->CardCapabilities |= SD_MODE_4BIT;
                }

                break;
            }
        }

        if (!KSUCCESS(Status)) {
            goto InitializeControllerEnd;
        }

        Controller->ClockSpeed = SdClock25MHz;
        if ((Controller->CardCapabilities & SD_MODE_HIGH_SPEED_52MHZ) != 0) {
            Controller->ClockSpeed = SdClock52MHz;

        } else if ((Controller->CardCapabilities & SD_MODE_HIGH_SPEED) != 0) {
            Controller->ClockSpeed = SdClock50MHz;
        }

        Status = SdpSetBusParameters(Controller);
        if (!KSUCCESS(Status)) {
            goto InitializeControllerEnd;
        }
    }

    for (LoopIndex = 0;
         LoopIndex < SD_SET_BLOCK_LENGTH_RETRY_COUNT;
         LoopIndex += 1) {

        Status = SdpSetBlockLength(Controller, Controller->ReadBlockLength);
        if (KSUCCESS(Status)) {
            break;
        }
    }

    if (!KSUCCESS(Status)) {
        goto InitializeControllerEnd;
    }

    RtlAtomicOr32(&(Controller->Flags), SD_CONTROLLER_FLAG_MEDIA_PRESENT);
    Status = STATUS_SUCCESS;

InitializeControllerEnd:
    return Status;
}

SD_API
KSTATUS
SdBlockIoPolled (
    PSD_CONTROLLER Controller,
    ULONGLONG BlockOffset,
    UINTN BlockCount,
    PVOID BufferVirtual,
    BOOL Write
    )

/*++

Routine Description:

    This routine performs a block I/O read or write using the CPU and not
    DMA.

Arguments:

    Controller - Supplies a pointer to the controller.

    BlockOffset - Supplies the logical block address of the I/O.

    BlockCount - Supplies the number of blocks to read or write.

    BufferVirtual - Supplies the virtual address of the I/O buffer.

    Write - Supplies a boolean indicating if this is a read operation (FALSE)
        or a write operation.

Return Value:

    Status code.

--*/

{

    UINTN BlocksDone;
    UINTN BlocksThisRound;
    KSTATUS Status;

    if ((Controller->Flags & SD_CONTROLLER_FLAG_MEDIA_PRESENT) == 0) {
        return STATUS_NO_MEDIA;
    }

    ASSERT((BlockOffset + BlockCount) * Controller->ReadBlockLength <=
           Controller->UserCapacity);

    Status = STATUS_ARGUMENT_EXPECTED;
    BlocksDone = 0;
    while (BlocksDone != BlockCount) {
        BlocksThisRound = BlockCount - BlocksDone;
        if (BlocksThisRound > Controller->MaxBlocksPerTransfer) {
            BlocksThisRound = Controller->MaxBlocksPerTransfer;
        }

        if (Write != FALSE) {
            Status = SdpWriteBlocksPolled(Controller,
                                          BlockOffset + BlocksDone,
                                          BlocksThisRound,
                                          BufferVirtual);

        } else {
            Status = SdpReadBlocksPolled(Controller,
                                         BlockOffset + BlocksDone,
                                         BlocksThisRound,
                                         BufferVirtual);
        }

        if (!KSUCCESS(Status)) {
            SdpErrorRecovery(Controller);
            break;
        }

        BlocksDone += BlocksThisRound;
        BufferVirtual += BlocksThisRound * Controller->ReadBlockLength;
    }

    return Status;
}

SD_API
KSTATUS
SdGetMediaParameters (
    PSD_CONTROLLER Controller,
    PULONGLONG BlockCount,
    PULONG BlockSize
    )

/*++

Routine Description:

    This routine returns information about the media card.

Arguments:

    Controller - Supplies a pointer to the controller.

    BlockCount - Supplies a pointer where the number of blocks in the user
        area of the medium will be returned.

    BlockSize - Supplies a pointer where the block size of the medium will be
        returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_MEDIA if there is no card in the slot.

--*/

{

    ULONG BiggestBlockSize;
    ULONG ReadBlockShift;

    if ((Controller->Flags & SD_CONTROLLER_FLAG_MEDIA_PRESENT) == 0) {
        return STATUS_NO_MEDIA;
    }

    //
    // There might be some work needed to support different read and write
    // block lengths. Investigate a bit before just ripping out this assert.
    //

    ASSERT(Controller->ReadBlockLength == Controller->WriteBlockLength);

    BiggestBlockSize = Controller->ReadBlockLength;
    if (Controller->WriteBlockLength > BiggestBlockSize) {
        BiggestBlockSize = Controller->WriteBlockLength;
    }

    ASSERT((BiggestBlockSize != 0) && (Controller->ReadBlockLength != 0));

    if (BlockSize != NULL) {
        *BlockSize = BiggestBlockSize;
    }

    if (BlockCount != NULL) {

        ASSERT(POWER_OF_2(Controller->ReadBlockLength) != FALSE);

        ReadBlockShift = RtlCountTrailingZeros32(Controller->ReadBlockLength);
        *BlockCount = Controller->UserCapacity >> ReadBlockShift;
    }

    return STATUS_SUCCESS;
}

SD_API
KSTATUS
SdInitializeDma (
    PSD_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine initializes DMA support in the host controller.

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
    ULONG Value;

    if ((Controller->Flags & SD_CONTROLLER_FLAG_MEDIA_PRESENT) == 0) {
        return STATUS_NO_MEDIA;
    }

    if ((Controller->HostCapabilities & SD_MODE_ADMA2) == 0) {
        return STATUS_NOT_SUPPORTED;
    }

    if ((Controller->HostCapabilities & SD_MODE_AUTO_CMD12) == 0) {
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Create the DMA descriptor table if not already done.
    //

    if (Controller->DmaDescriptorTable == NULL) {
        Controller->DmaDescriptorTable = MmAllocateNonPagedIoBuffer(
                                                0,
                                                MAX_ULONG,
                                                4,
                                                SD_ADMA2_DESCRIPTOR_TABLE_SIZE,
                                                TRUE,
                                                FALSE,
                                                TRUE);

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
    // Record that ADMA2 is enabled in the host controller.
    //

    RtlAtomicOr32(&(Controller->Flags), SD_CONTROLLER_FLAG_ADMA2_ENABLED);
    return STATUS_SUCCESS;
}

SD_API
VOID
SdBlockIoDma (
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

    This routine performs a block I/O read or write using ADMA2.

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
    SD_COMMAND Command;
    ULONG DescriptorCount;
    UINTN DescriptorSize;
    PSD_ADMA2_DESCRIPTOR DmaDescriptor;
    PIO_BUFFER_FRAGMENT Fragment;
    UINTN FragmentIndex;
    UINTN FragmentOffset;
    PHYSICAL_ADDRESS PhysicalAddress;
    KSTATUS Status;
    ULONG TableAddress;
    UINTN TransferSize;
    UINTN TransferSizeRemaining;

    ASSERT(BlockCount != 0);

    if ((Controller->Flags & SD_CONTROLLER_FLAG_MEDIA_PRESENT) == 0) {
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
    // Fill out the DMA descriptors.
    //

    DmaDescriptor = Controller->DmaDescriptorTable->Fragment[0].VirtualAddress;
    DescriptorCount = 0;
    TransferSizeRemaining = TransferSize;
    while ((TransferSizeRemaining != 0) &&
           (DescriptorCount < SD_ADMA2_DESCRIPTOR_COUNT - 1)) {

        ASSERT(FragmentIndex < IoBuffer->FragmentCount);

        Fragment = &(IoBuffer->Fragment[FragmentIndex]);

        //
        // This descriptor size is going to the the minimum of the total
        // remaining size, the size that can fit in a DMA descriptor, and the
        // remaining size of the fragment.
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
                                    (DescriptorSize << SD_ADMA2_LENGTH_SHIFT);

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
    TableAddress =
          (ULONG)(Controller->DmaDescriptorTable->Fragment[0].PhysicalAddress);

    SD_WRITE_REGISTER(Controller, SdRegisterAdmaAddressLow, TableAddress);
    Status = SdpSendCommand(Controller, &Command);
    if (!KSUCCESS(Status)) {
        SdpErrorRecovery(Controller);
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
SdAbortTransaction (
    PSD_CONTROLLER Controller,
    BOOL SynchronousAbort
    )

/*++

Routine Description:

    This routine aborts the current SD transaction on the controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    SynchronousAbort - Supplies a boolean indicating if an synchronous abort
        is requested or not. Note that an asynchronous abort is not actually
        asynchronous from the drivers perspective. The name is taken from
        Section 3.8 "Abort Transaction" of the SD Version 3.0 specification.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    if (SynchronousAbort == FALSE) {
        Status = SdpAsynchronousAbort(Controller);

    } else {
        Status = SdpSynchronousAbort(Controller);
    }

    return Status;
}

SD_API
VOID
SdSetCriticalMode (
    PSD_CONTROLLER Controller,
    BOOL Enable
    )

/*++

Routine Description:

    This routine sets the SD controller into and out of critical execution
    mode. Critical execution mode is necessary for crash dump scenarios in
    which timeouts must be calculated by querying the hardware time counter
    directly, as the clock is not running to update the kernel's time counter.

Arguments:

    Controller - Supplies a pointer to the controller.

    Enable - Supplies a boolean indicating if critical mode should be enabled
        or disabled.

Return Value:

    None.

--*/

{

    if (Enable != FALSE) {
        if ((Controller->Flags & SD_CONTROLLER_FLAG_CRITICAL_MODE) == 0) {
            RtlAtomicOr32(&(Controller->Flags),
                          SD_CONTROLLER_FLAG_CRITICAL_MODE);
        }

    } else {
        if ((Controller->Flags & SD_CONTROLLER_FLAG_CRITICAL_MODE) != 0) {
            RtlAtomicAnd32(&(Controller->Flags),
                           ~SD_CONTROLLER_FLAG_CRITICAL_MODE);
        }
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
SdpInterruptServiceDpc (
    PDPC Dpc
    )

/*++

Routine Description:

    This routine implements the SD DPC that is queued when an interrupt fires.

Arguments:

    Dpc - Supplies a pointer to the DPC that is running.

Return Value:

    None.

--*/

{

    PVOID CompletionContext;
    PSD_IO_COMPLETION_ROUTINE CompletionRoutine;
    PSD_CONTROLLER Controller;
    BOOL Inserted;
    RUNLEVEL OldRunLevel;
    ULONG PendingBits;
    BOOL Removed;
    KSTATUS Status;

    Controller = (PSD_CONTROLLER)(Dpc->UserData);

    //
    // Synchronize with interrupts to clear out the interrupt register.
    //

    OldRunLevel = IoRaiseToInterruptRunLevel(Controller->InterruptHandle);
    KeAcquireSpinLock(&(Controller->InterruptLock));
    PendingBits = Controller->PendingStatusBits;
    Controller->PendingStatusBits = 0;
    KeReleaseSpinLock(&(Controller->InterruptLock));
    KeLowerRunLevel(OldRunLevel);
    if (PendingBits == 0) {
        return;
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
        RtlDebugPrint("SD: Error status %x\n", PendingBits);
        SdpErrorRecovery(Controller);
        Status = STATUS_DEVICE_IO_ERROR;

    } else if ((PendingBits & SD_INTERRUPT_STATUS_TRANSFER_COMPLETE) != 0) {
        Status = STATUS_SUCCESS;
    }

    if (Controller->IoCompletionRoutine != NULL) {
        CompletionRoutine = Controller->IoCompletionRoutine;
        CompletionContext = Controller->IoCompletionContext;
        Controller->IoCompletionRoutine = NULL;
        Controller->IoCompletionContext = NULL;
        CompletionRoutine(Controller,
                          CompletionContext,
                          Controller->IoRequestSize,
                          Status);
    }

    if (((Inserted != FALSE) || (Removed != FALSE)) &&
        (Controller->MediaChangeCallback != NULL)) {

        Controller->MediaChangeCallback(Controller,
                                        Controller->ConsumerContext,
                                        Removed,
                                        Inserted);
    }

    return;
}

KSTATUS
SdpSetBusParameters (
    PSD_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine sets the bus width and clock speed.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    ULONG ClockControl;
    SD_COMMAND Command;
    ULONG Divisor;
    ULONGLONG Frequency;
    ULONG Result;
    KSTATUS Status;
    ULONGLONG Timeout;
    ULONG Value;

    Frequency = HlQueryTimeCounterFrequency();

    //
    // If going wide, let the card know first.
    //

    if ((SD_IS_CONTROLLER_SD(Controller)) && (Controller->BusWidth != 1)) {
        RtlZeroMemory(&Command, sizeof(SD_COMMAND));
        Command.Command = SdCommandApplicationSpecific;
        Command.ResponseType = SD_RESPONSE_R1;
        Command.CommandArgument = Controller->CardAddress << 16;
        Status = SdpSendCommand(Controller, &Command);
        if (!KSUCCESS(Status)) {
            return Status;
        }

        Command.Command = SdCommandSetBusWidth;
        Command.ResponseType = SD_RESPONSE_R1;

        ASSERT(Controller->BusWidth == 4);

        Command.CommandArgument = 2;
        Status = SdpSendCommand(Controller, &Command);
        if (!KSUCCESS(Status)) {
            return Status;
        }

        HlBusySpin(2000);
    }

    Value = SD_READ_REGISTER(Controller, SdRegisterHostControl);
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

    ASSERT(Controller->FundamentalClock != 0);

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
    Timeout = KeGetRecentTimeCounter() + (Frequency * SD_CONTROLLER_TIMEOUT);
    do {
        Value = SD_READ_REGISTER(Controller, SdRegisterClockControl);
        if ((Value & SD_CLOCK_CONTROL_CLOCK_STABLE) != 0) {
            Status = STATUS_SUCCESS;
            break;
        }

    } while (KeGetRecentTimeCounter() <= Timeout);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    ClockControl |= SD_CLOCK_CONTROL_SD_CLOCK_ENABLE;
    SD_WRITE_REGISTER(Controller, SdRegisterClockControl, ClockControl);
    return STATUS_SUCCESS;
}

KSTATUS
SdpResetCard (
    PSD_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine sends a reset (CMD0) command to the card.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    SD_COMMAND Command;
    KSTATUS Status;

    KeDelayExecution(FALSE, FALSE, SD_CARD_DELAY);
    RtlZeroMemory(&Command, sizeof(SD_COMMAND));
    Command.Command = SdCommandReset;
    Command.CommandArgument = 0;
    Command.ResponseType = SD_RESPONSE_NONE;
    Status = SdpSendCommand(Controller, &Command);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    KeDelayExecution(FALSE, FALSE, SD_POST_RESET_DELAY);
    return Status;
}

KSTATUS
SdpGetInterfaceCondition (
    PSD_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine sends a "Send Interface Condition" (CMD8) to the SD card.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    SD_COMMAND Command;
    KSTATUS Status;
    UINTN Try;

    Status = STATUS_DEVICE_IO_ERROR;
    for (Try = 0; Try < SD_INTERFACE_CONDITION_RETRY_COUNT; Try += 1) {
        RtlZeroMemory(&Command, sizeof(SD_COMMAND));
        Command.Command = SdCommandSendInterfaceCondition;
        Command.CommandArgument = SD_COMMAND8_ARGUMENT;
        Command.ResponseType = SD_RESPONSE_R7;
        Status = SdpSendCommand(Controller, &Command);
        if (KSUCCESS(Status)) {
            if ((Command.Response[0] & 0xFF) != (SD_COMMAND8_ARGUMENT & 0xFF)) {
                Status = STATUS_DEVICE_IO_ERROR;

            } else {
                break;
            }
        }
    }

    if (!KSUCCESS(Status)) {
        return Status;
    }

    Controller->Version = SdVersion2;
    return Status;
}

KSTATUS
SdpWaitForCardToInitialize (
    PSD_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine sends attempts to wait for the card to initialize by sending
    CMD55 (application specific command) and CMD41.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    SD_COMMAND Command;
    ULONG LoopIndex;
    ULONG Ocr;
    ULONG Retry;
    KSTATUS Status;

    RtlZeroMemory(&Command, sizeof(SD_COMMAND));
    for (LoopIndex = 0;
         LoopIndex < SD_CARD_INITIALIZE_RETRY_COUNT;
         LoopIndex += 1) {

        Status = SdpResetCard(Controller);
        if (!KSUCCESS(Status)) {
            return Status;
        }

        Status = SdpGetInterfaceCondition(Controller);
        if (!KSUCCESS(Status)) {
            return Status;
        }

        //
        // The first iteration gets the operating condition register (as no
        // voltage mask is set), the subsequent iterations attempt to set it.
        //

        Ocr = 0;
        Retry = 0;
        do {
            if (Retry == SD_CARD_OPERATING_CONDITION_RETRY_COUNT) {
                break;
            }

            Command.Command = SdCommandApplicationSpecific;
            Command.ResponseType = SD_RESPONSE_R1;
            Command.CommandArgument = 0;
            Status = SdpSendCommand(Controller, &Command);
            if (!KSUCCESS(Status)) {
                return Status;
            }

            Command.Command = SdCommandSendOperatingCondition;
            Command.ResponseType = SD_RESPONSE_R3;
            Command.CommandArgument = Ocr;
            if (Retry != 0) {
                if ((Controller->HostCapabilities & SD_MODE_SPI) == 0) {
                    Command.CommandArgument &=
                                  (Controller->Voltages &
                                   SD_OPERATING_CONDITION_VOLTAGE_MASK) |
                                  SD_OPERATING_CONDITION_ACCESS_MODE;
                }

                if (Controller->Version == SdVersion2) {
                    Command.CommandArgument |=
                                          SD_OPERATING_CONDITION_HIGH_CAPACITY;
                }
            }

            Status = SdpSendCommand(Controller, &Command);
            if (!KSUCCESS(Status)) {
                return Status;
            }

            HlBusySpin(SD_CARD_DELAY);
            Retry += 1;
            if ((Command.Response[0] & Controller->Voltages) == 0) {
                return STATUS_INVALID_CONFIGURATION;
            }

            //
            // The first iteration just gets the OCR.
            //

            if (Retry == 1) {
                Ocr = Command.Response[0];
                continue;
            }

        } while ((Command.Response[0] & SD_OPERATING_CONDITION_BUSY) == 0);

        if ((Command.Response[0] & SD_OPERATING_CONDITION_BUSY) != 0) {
            break;
        }
    }

    if (LoopIndex == SD_CARD_INITIALIZE_RETRY_COUNT) {
        return STATUS_NOT_READY;
    }

    if ((Controller->HostCapabilities & SD_MODE_SPI) != 0) {
        Command.Command = SdCommandSpiReadOperatingCondition;
        Command.ResponseType = SD_RESPONSE_R3;
        Command.CommandArgument = 0;
        Status = SdpSendCommand(Controller, &Command);
        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    ASSERT((Controller->Flags & SD_CONTROLLER_FLAG_HIGH_CAPACITY) == 0);

    if ((Command.Response[0] & SD_OPERATING_CONDITION_HIGH_CAPACITY) != 0) {
        RtlAtomicOr32(&(Controller->Flags), SD_CONTROLLER_FLAG_HIGH_CAPACITY);
    }

    return STATUS_SUCCESS;
}

KSTATUS
SdpSetCrc (
    PSD_CONTROLLER Controller,
    BOOL Enable
    )

/*++

Routine Description:

    This routine enables or disables CRCs on the card.

Arguments:

    Controller - Supplies a pointer to the controller.

    Enable - Supplies a boolean indicating whether to enable CRCs (TRUE) or
        disable CRCs (FALSE).

Return Value:

    Status code.

--*/

{

    SD_COMMAND Command;
    KSTATUS Status;

    RtlZeroMemory(&Command, sizeof(SD_COMMAND));
    Command.Command = SdCommandSpiCrcOnOff;
    Command.CommandArgument = 0;
    if (Enable != FALSE) {
        Command.CommandArgument = 1;
    }

    Command.ResponseType = SD_RESPONSE_R1;
    Status = SdpSendCommand(Controller, &Command);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    return Status;
}

KSTATUS
SdpGetCardIdentification (
    PSD_CONTROLLER Controller,
    PSD_CARD_IDENTIFICATION Identification
    )

/*++

Routine Description:

    This routine reads the card identification data from the card.

Arguments:

    Controller - Supplies a pointer to the controller.

    Identification - Supplies a pointer where the identification data will
        be returned.

Return Value:

    Status code.

--*/

{

    SD_COMMAND Command;
    KSTATUS Status;

    RtlZeroMemory(&Command, sizeof(SD_COMMAND));
    if ((Controller->HostCapabilities & SD_MODE_SPI) != 0) {
        Command.Command = SdCommandSendCardIdentification;

    } else {
        Command.Command = SdCommandAllSendCardIdentification;
    }

    Command.ResponseType = SD_RESPONSE_R2;
    Command.CommandArgument = 0;
    Status = SdpSendCommand(Controller, &Command);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    RtlCopyMemory(Identification,
                  &(Command.Response),
                  sizeof(SD_CARD_IDENTIFICATION));

    return Status;
}

KSTATUS
SdpSetupAddressing (
    PSD_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine sets up the card addressing.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    SD_COMMAND Command;
    KSTATUS Status;

    RtlZeroMemory(&Command, sizeof(SD_COMMAND));
    if ((Controller->HostCapabilities & SD_MODE_SPI) != 0) {
        return STATUS_SUCCESS;
    }

    Command.Command = SdCommandSetRelativeAddress;
    Command.ResponseType = SD_RESPONSE_R6;
    Command.CommandArgument = Controller->CardAddress << 16;
    Status = SdpSendCommand(Controller, &Command);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    if (SD_IS_CONTROLLER_SD(Controller)) {
        Controller->CardAddress = (Command.Response[0] >> 16) & 0xFFFF;
    }

    return Status;
}

KSTATUS
SdpReadCardSpecificData (
    PSD_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine reads and parses the card specific data.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    ULONGLONG CapacityBase;
    ULONG CapacityShift;
    SD_COMMAND Command;
    ULONG MmcVersion;
    KSTATUS Status;

    RtlZeroMemory(&Command, sizeof(SD_COMMAND));
    Command.Command = SdCommandSendCardSpecificData;
    Command.ResponseType = SD_RESPONSE_R2;
    Command.CommandArgument = Controller->CardAddress << 16;
    Status = SdpSendCommand(Controller, &Command);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Status = SdpWaitForStateTransition(Controller);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    if (Controller->Version == SdVersionInvalid) {
        MmcVersion = (Command.Response[0] >>
                      SD_CARD_SPECIFIC_DATA_0_MMC_VERSION_SHIFT) &
                     SD_CARD_SPECIFIC_DATA_0_MMC_VERSION_MASK;

        switch (MmcVersion) {
        case 1:
            Controller->Version = SdMmcVersion1p4;
            break;

        case 2:
            Controller->Version = SdMmcVersion2p2;
            break;

        case 3:
            Controller->Version = SdMmcVersion3;
            break;

        case 4:
            Controller->Version = SdMmcVersion4;
            break;

        default:
            Controller->Version = SdMmcVersion1p2;
            break;
        }
    }

    //
    // Compute the read and write block lengths.
    //

    Controller->ReadBlockLength =
        1 << ((Command.Response[1] >>
               SD_CARD_SPECIFIC_DATA_1_READ_BLOCK_LENGTH_SHIFT) &
              SD_CARD_SPECIFIC_DATA_1_READ_BLOCK_LENGTH_MASK);

    if (SD_IS_CONTROLLER_SD(Controller)) {
        Controller->WriteBlockLength = Controller->ReadBlockLength;

    } else {
        Controller->WriteBlockLength =
            1 << ((Command.Response[1] >>
                   SD_CARD_SPECIFIC_DATA_1_WRITE_BLOCK_LENGTH_SHIFT) &
                  SD_CARD_SPECIFIC_DATA_1_WRITE_BLOCK_LENGTH_MASK);
    }

    //
    // Compute the media size in blocks.
    //

    if ((Controller->Flags & SD_CONTROLLER_FLAG_HIGH_CAPACITY) != 0) {
        CapacityBase = ((Command.Response[1] &
                         SD_CARD_SPECIFIC_DATA_1_HIGH_CAPACITY_MASK) <<
                        SD_CARD_SPECIFIC_DATA_1_HIGH_CAPACITY_SHIFT) |
                       ((Command.Response[2] &
                         SD_CARD_SPECIFIC_DATA_2_HIGH_CAPACITY_MASK) >>
                        SD_CARD_SPECIFIC_DATA_2_HIGH_CAPACITY_SHIFT);

        CapacityShift = SD_CARD_SPECIFIC_DATA_HIGH_CAPACITY_MULTIPLIER;

    } else {
        CapacityBase = ((Command.Response[1] &
                         SD_CARD_SPECIFIC_DATA_1_CAPACITY_MASK) <<
                        SD_CARD_SPECIFIC_DATA_1_CAPACITY_SHIFT) |
                       ((Command.Response[2] &
                         SD_CARD_SPECIFIC_DATA_2_CAPACITY_MASK) >>
                        SD_CARD_SPECIFIC_DATA_2_CAPACITY_SHIFT);

        CapacityShift = (Command.Response[2] &
                         SD_CARD_SPECIFIC_DATA_2_CAPACITY_MULTIPLIER_MASK) >>
                        SD_CARD_SPECIFIC_DATA_2_CAPACITY_MULTIPLIER_SHIFT;
    }

    Controller->UserCapacity = (CapacityBase + 1) << (CapacityShift + 2);
    Controller->UserCapacity *= Controller->ReadBlockLength;
    if (Controller->ReadBlockLength > SD_MMC_MAX_BLOCK_SIZE) {
        Controller->ReadBlockLength = SD_MMC_MAX_BLOCK_SIZE;
    }

    if (Controller->WriteBlockLength > SD_MMC_MAX_BLOCK_SIZE) {
        Controller->WriteBlockLength = SD_MMC_MAX_BLOCK_SIZE;
    }

    //
    // There are currently assumptions about the block lengths both being 512.
    // This will change when MMC is supported.
    //

    ASSERT(Controller->ReadBlockLength == SD_BLOCK_SIZE);
    ASSERT(Controller->WriteBlockLength == SD_BLOCK_SIZE);

    Controller->CardSpecificData[0] = Command.Response[0];
    Controller->CardSpecificData[1] = Command.Response[1];
    Controller->CardSpecificData[2] = Command.Response[2];
    Controller->CardSpecificData[3] = Command.Response[3];
    return STATUS_SUCCESS;
}

KSTATUS
SdpSelectCard (
    PSD_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine puts the SD card into transfer mode.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    SD_COMMAND Command;
    KSTATUS Status;

    //
    // This command is not supported in SPI mode.
    //

    if ((Controller->HostCapabilities & SD_MODE_SPI) != 0) {
        return STATUS_SUCCESS;
    }

    RtlZeroMemory(&Command, sizeof(SD_COMMAND));
    Command.Command = SdCommandSelectCard;
    Command.ResponseType = SD_RESPONSE_R1;
    Command.CommandArgument = Controller->CardAddress << 16;
    Status = SdpSendCommand(Controller, &Command);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Status = SdpWaitForStateTransition(Controller);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    return Status;
}

KSTATUS
SdpConfigureEraseGroup (
    PSD_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine configures the erase group settings for the SD or MMC card.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    ULONGLONG Capacity;
    UCHAR CardData[SD_MMC_MAX_BLOCK_SIZE];
    ULONG EraseGroupMultiplier;
    ULONG EraseGroupSize;
    ULONG Offset;
    ULONG PartitionIndex;
    KSTATUS Status;

    //
    // For SD, the erase group is always one sector.
    //

    Controller->EraseGroupSize = 1;
    Controller->PartitionConfiguration = SD_MMC_PARTITION_NONE;
    if ((SD_IS_CONTROLLER_SD(Controller)) ||
        (Controller->Version < SdMmcVersion4)) {

        return STATUS_SUCCESS;
    }

    Status = SdpGetExtendedCardSpecificData(Controller, CardData);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    if (CardData[SD_MMC_EXTENDED_CARD_DATA_REVISION] >= 2) {

        //
        // The capacity is valid if it is greater than 2GB.
        //

        Capacity =
                 CardData[SD_MMC_EXTENDED_CARD_DATA_SECTOR_COUNT] |
                 (CardData[SD_MMC_EXTENDED_CARD_DATA_SECTOR_COUNT + 1] << 8) |
                 (CardData[SD_MMC_EXTENDED_CARD_DATA_SECTOR_COUNT + 2] << 16) |
                 (CardData[SD_MMC_EXTENDED_CARD_DATA_SECTOR_COUNT + 3] << 24);

        Capacity *= SD_MMC_MAX_BLOCK_SIZE;
        if (Capacity > SD_MMC_EXTENDED_SECTOR_COUNT_MINIMUM) {
            Controller->UserCapacity = Capacity;
        }
    }

    switch (CardData[SD_MMC_EXTENDED_CARD_DATA_REVISION]) {
    case 1:
        Controller->Version = SdMmcVersion4p1;
        break;

    case 2:
        Controller->Version = SdMmcVersion4p2;
        break;

    case 3:
        Controller->Version = SdMmcVersion4p3;
        break;

    case 5:
        Controller->Version = SdMmcVersion4p41;
        break;

    case 6:
        Controller->Version = SdMmcVersion4p5;
        break;

    default:
        break;
    }

    //
    // The host needs to enable the erase group def bit if the device is
    // partitioned. This is lost every time the card is reset or power cycled.
    //

    if (((CardData[SD_MMC_EXTENDED_CARD_DATA_PARTITIONING_SUPPORT] &
          SD_MMC_PARTITION_SUPPORT) != 0) &&
        ((CardData[SD_MMC_EXTENDED_CARD_DATA_PARTITIONS_ATTRIBUTE] &
          SD_MMC_PARTITION_ENHANCED_ATTRIBUTE) != 0)) {

        Status = SdpMmcSwitch(Controller,
                              SD_MMC_EXTENDED_CARD_DATA_ERASE_GROUP_DEF,
                              1);

        if (!KSUCCESS(Status)) {
            return Status;
        }

        //
        // Read out the group size from the card specific data.
        //

        Controller->EraseGroupSize =
                        CardData[SD_MMC_EXTENDED_CARD_DATA_ERASE_GROUP_SIZE] *
                        SD_MMC_MAX_BLOCK_SIZE * 1024;

    //
    // Calculate the erase group size from the card specific data.
    //

    } else {
        EraseGroupSize = (Controller->CardSpecificData[2] &
                          SD_CARD_SPECIFIC_DATA_2_ERASE_GROUP_SIZE_MASK) >>
                         SD_CARD_SPECIFIC_DATA_2_ERASE_GROUP_SIZE_SHIFT;

        EraseGroupMultiplier =
                       (Controller->CardSpecificData[2] &
                        SD_CARD_SPECIFIC_DATA_2_ERASE_GROUP_MULTIPLIER_MASK) >>
                       SD_CARD_SPECIFIC_DATA_2_ERASE_GROUP_MULTIPLIER_SHIFT;

        Controller->EraseGroupSize = (EraseGroupSize + 1) *
                                     (EraseGroupMultiplier + 1);
    }

    //
    // Store the partition information of EMMC.
    //

    if (((CardData[SD_MMC_EXTENDED_CARD_DATA_PARTITIONING_SUPPORT] &
          SD_MMC_PARTITION_SUPPORT) != 0) ||
        (CardData[SD_MMC_EXTENDED_CARD_DATA_BOOT_SIZE] != 0)) {

        Controller->PartitionConfiguration =
                   CardData[SD_MMC_EXTENDED_CARD_DATA_PARTITION_CONFIGURATION];
    }

    Controller->BootCapacity = CardData[SD_MMC_EXTENDED_CARD_DATA_BOOT_SIZE] <<
                               SD_MMC_EXTENDED_CARD_DATA_PARTITION_SHIFT;

    Controller->RpmbCapacity = CardData[SD_MMC_EXTENDED_CARD_DATA_RPMB_SIZE] <<
                               SD_MMC_EXTENDED_CARD_DATA_PARTITION_SHIFT;

    for (PartitionIndex = 0;
         PartitionIndex < SD_MMC_GENERAL_PARTITION_COUNT;
         PartitionIndex += 1) {

        Offset = SD_MMC_EXTENDED_CARD_DATA_GENERAL_PARTITION_SIZE +
                 (PartitionIndex * 3);

        Controller->GeneralPartitionCapacity[PartitionIndex] =
                                                 (CardData[Offset + 2] << 16) |
                                                 (CardData[Offset + 1] << 8) |
                                                 CardData[Offset];

        Controller->GeneralPartitionCapacity[PartitionIndex] *=
                          CardData[SD_MMC_EXTENDED_CARD_DATA_ERASE_GROUP_SIZE];

        Controller->GeneralPartitionCapacity[PartitionIndex] *=
                  CardData[SD_MMC_EXTENDED_CARD_DATA_WRITE_PROTECT_GROUP_SIZE];
    }

    return STATUS_SUCCESS;
}

KSTATUS
SdpGetExtendedCardSpecificData (
    PSD_CONTROLLER Controller,
    UCHAR Data[SD_MMC_MAX_BLOCK_SIZE]
    )

/*++

Routine Description:

    This routine gets the extended Card Specific Data from the card.

Arguments:

    Controller - Supplies a pointer to the controller.

    Data - Supplies a pointer where the data will be returned on success.

Return Value:

    Status code.

--*/

{

    SD_COMMAND Command;
    KSTATUS Status;

    RtlZeroMemory(&Command, sizeof(SD_COMMAND));
    Command.Command = SdCommandMmcSendExtendedCardSpecificData;
    Command.ResponseType = SD_RESPONSE_R1;
    Command.BufferVirtual = Data;
    Command.BufferSize = SD_MMC_MAX_BLOCK_SIZE;
    Status = SdpSendCommand(Controller, &Command);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    return Status;
}

KSTATUS
SdpMmcSwitch (
    PSD_CONTROLLER Controller,
    UCHAR Index,
    UCHAR Value
    )

/*++

Routine Description:

    This routine executes the switch command on the MMC card.

Arguments:

    Controller - Supplies a pointer to the controller.

    Index - Supplies the index of the extended card specific data to change.

    Value - Supplies the new value to set.

Return Value:

    Status code.

--*/

{

    SD_COMMAND Command;
    KSTATUS Status;

    RtlZeroMemory(&Command, sizeof(SD_COMMAND));
    Command.Command = SdCommandSwitch;
    Command.ResponseType = SD_RESPONSE_R1B;
    Command.CommandArgument = (SD_MMC_SWITCH_MODE_WRITE_BYTE <<
                               SD_MMC_SWITCH_MODE_SHIFT) |
                              (Index << SD_MMC_SWITCH_INDEX_SHIFT) |
                              (Value << SD_MMC_SWITCH_VALUE_SHIFT);

    Status = SdpSendCommand(Controller, &Command);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Status = SdpWaitForStateTransition(Controller);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    return Status;
}

KSTATUS
SdpSdSwitch (
    PSD_CONTROLLER Controller,
    ULONG Mode,
    ULONG Group,
    UCHAR Value,
    ULONG Response[16]
    )

/*++

Routine Description:

    This routine executes the switch command on the SD card.

Arguments:

    Controller - Supplies a pointer to the controller.

    Mode - Supplies the mode of the switch.

    Group - Supplies the group value.

    Value - Supplies the switch value.

    Response - Supplies a pointer where the response will be returned.

Return Value:

    Status code.

--*/

{

    SD_COMMAND Command;
    KSTATUS Status;

    RtlZeroMemory(&Command, sizeof(SD_COMMAND));
    Command.Command = SdCommandSwitch;
    Command.ResponseType = SD_RESPONSE_R1;
    Command.CommandArgument = (Mode << 31) | 0x00FFFFFF;
    Command.CommandArgument &= ~(0xF << (Group * 4));
    Command.CommandArgument |= Value << (Group * 4);
    Command.BufferVirtual = Response;
    Command.BufferSize = sizeof(ULONG) * 16;
    Status = SdpSendCommand(Controller, &Command);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    return Status;
}

KSTATUS
SdpWaitForStateTransition (
    PSD_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine attempts to wait for the card to transfer states to the point
    where it is ready for data and not in the program state.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    SD_COMMAND Command;
    KSTATUS Status;
    ULONGLONG Timeout;

    RtlZeroMemory(&Command, sizeof(SD_COMMAND));
    Command.Command = SdCommandSendStatus;
    Command.ResponseType = SD_RESPONSE_R1;
    if ((Controller->HostCapabilities & SD_MODE_SPI) == 0) {
        Command.CommandArgument = Controller->CardAddress << 16;
    }

    Timeout = SdpQueryTimeCounter(Controller) +
              (HlQueryTimeCounterFrequency() * SD_CONTROLLER_STATUS_TIMEOUT);

    while (TRUE) {
        Status = SdpSendCommand(Controller, &Command);
        if (KSUCCESS(Status)) {

            //
            // Break out if the card's all ready to go.
            //

            if (((Command.Response[0] & SD_STATUS_READY_FOR_DATA) != 0) &&
                ((Command.Response[0] & SD_STATUS_CURRENT_STATE) !=
                 SD_STATUS_STATE_PROGRAM)) {

                break;

            //
            // Complain if the card's having a bad hair day.
            //

            } else if ((Command.Response[0] & SD_STATUS_MASK) != 0) {
                RtlDebugPrint("SD: Status error 0x%x.\n", Command.Response[0]);
                return STATUS_DEVICE_IO_ERROR;
            }
        }

        //
        // If the card is long gone, then don't bother to read the status.
        //

        if ((Controller->Flags & SD_CONTROLLER_FLAG_MEDIA_PRESENT) == 0) {
            Status = STATUS_NO_MEDIA;
            break;
        }

        if (SdpQueryTimeCounter(Controller) > Timeout) {
            Status = STATUS_TIMEOUT;
            break;
        }
    }

    return Status;
}

KSTATUS
SdpGetCardStatus (
    PSD_CONTROLLER Controller,
    PULONG CardStatus
    )

/*++

Routine Description:

    This routine attempts to read the card status.

Arguments:

    Controller - Supplies a pointer to the controller.

    CardStatus - Supplies a pointer that receives the current card status on
        success.

Return Value:

    Status code.

--*/

{

    SD_COMMAND Command;
    KSTATUS Status;

    RtlZeroMemory(&Command, sizeof(SD_COMMAND));
    Command.Command = SdCommandSendStatus;
    Command.ResponseType = SD_RESPONSE_R1;
    if ((Controller->HostCapabilities & SD_MODE_SPI) == 0) {
        Command.CommandArgument = Controller->CardAddress << 16;
    }

    Status = SdpSendCommand(Controller, &Command);
    if (KSUCCESS(Status)) {
        *CardStatus = Command.Response[0];
    }

    return Status;
}

KSTATUS
SdpSetSdFrequency (
    PSD_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine sets the proper frequency for an SD card.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    SD_COMMAND Command;
    ULONG ConfigurationRegister[2];
    ULONG RetryCount;
    KSTATUS Status;
    ULONG SwitchStatus[16];
    ULONG Version;

    Controller->CardCapabilities = 0;
    if ((Controller->HostCapabilities & SD_MODE_SPI) != 0) {
        return STATUS_SUCCESS;
    }

    //
    // Read the SCR to find out if the card supports higher speeds.
    //

    RtlZeroMemory(&Command, sizeof(SD_COMMAND));
    Command.Command = SdCommandApplicationSpecific;
    Command.ResponseType = SD_RESPONSE_R1;
    Command.CommandArgument = Controller->CardAddress << 16;
    Status = SdpSendCommand(Controller, &Command);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Command.Command = SdCommandSendSdConfigurationRegister;
    Command.ResponseType = SD_RESPONSE_R1;
    Command.CommandArgument = 0;
    Command.BufferVirtual = ConfigurationRegister;
    Command.BufferSize = sizeof(ConfigurationRegister);
    RetryCount = SD_CONFIGURATION_REGISTER_RETRY_COUNT;
    while (TRUE) {
        KeDelayExecution(FALSE, FALSE, 50000);
        Status = SdpSendCommand(Controller, &Command);
        if (!KSUCCESS(Status)) {
            if (RetryCount == 0) {
                return Status;
            }

            RetryCount -= 1;
            continue;
        }

        break;
    }

    ConfigurationRegister[0] = RtlByteSwapUlong(ConfigurationRegister[0]);
    ConfigurationRegister[1] = RtlByteSwapUlong(ConfigurationRegister[1]);
    Version = (ConfigurationRegister[0] >>
               SD_CONFIGURATION_REGISTER_VERSION_SHIFT) &
               SD_CONFIGURATION_REGISTER_VERSION_MASK;

    switch (Version) {
    case 1:
        Controller->Version = SdVersion1p10;
        break;

    case 2:
        Controller->Version = SdVersion2;
        if (((ConfigurationRegister[0] >>
              SD_CONFIGURATION_REGISTER_VERSION3_SHIFT) & 0x1) != 0) {

            Controller->Version = SdVersion3;
        }

        break;

    case 0:
    default:
        Controller->Version = SdVersion1p0;
        break;
    }

    if ((ConfigurationRegister[0] & SD_CONFIGURATION_REGISTER_DATA_4BIT) != 0) {
        Controller->CardCapabilities |= SD_MODE_4BIT;
    }

    //
    // Version 1.0 doesn't support switching, so end now.
    //

    if (Controller->Version == SdVersion1p0) {
        return STATUS_SUCCESS;
    }

    RetryCount = SD_SWITCH_RETRY_COUNT;
    while (RetryCount != 0) {
        RetryCount -= 1;
        Status = SdpSdSwitch(Controller, SD_SWITCH_CHECK, 0, 1, SwitchStatus);
        if (!KSUCCESS(Status)) {
            return Status;
        }

        //
        // Wait for the high speed status to become not busy.
        //

        if ((RtlByteSwapUlong(SwitchStatus[7]) &
             SD_SWITCH_STATUS_7_HIGH_SPEED_BUSY) == 0) {

            break;
        }
    }

    //
    // Don't worry about it if high speed isn't supported by either the card or
    // the host.
    //

    if ((RtlByteSwapUlong(SwitchStatus[3]) &
         SD_SWITCH_STATUS_3_HIGH_SPEED_SUPPORTED) == 0) {

        return STATUS_SUCCESS;
    }

    if (((Controller->HostCapabilities & SD_MODE_HIGH_SPEED_52MHZ) == 0) &&
        ((Controller->HostCapabilities & SD_MODE_HIGH_SPEED) == 0)) {

        return STATUS_SUCCESS;
    }

    Status = SdpSdSwitch(Controller, SD_SWITCH_SWITCH, 0, 1, SwitchStatus);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    if ((RtlByteSwapUlong(SwitchStatus[4]) &
         SD_SWITCH_STATUS_4_HIGH_SPEED_MASK) ==
        SD_SWITCH_STATUS_4_HIGH_SPEED_VALUE) {

        Controller->CardCapabilities |= SD_MODE_HIGH_SPEED;
    }

    return STATUS_SUCCESS;
}

KSTATUS
SdpSetMmcFrequency (
    PSD_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine sets the proper frequency for an SD card.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    UCHAR CardData[SD_MMC_MAX_BLOCK_SIZE];
    UCHAR CardType;
    KSTATUS Status;

    Controller->CardCapabilities = 0;
    if ((Controller->HostCapabilities & SD_MODE_SPI) != 0) {
        return STATUS_SUCCESS;
    }

    //
    // Only version 4 supports high speed.
    //

    if (Controller->Version < SdMmcVersion4) {
        return STATUS_SUCCESS;
    }

    Status = SdpGetExtendedCardSpecificData(Controller, CardData);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    CardType = CardData[SD_MMC_EXTENDED_CARD_DATA_CARD_TYPE] &
               SD_MMC_EXTENDED_CARD_DATA_CARD_TYPE_MASK;

    Status = SdpMmcSwitch(Controller, SD_MMC_EXTENDED_CARD_DATA_HIGH_SPEED, 1);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Get the extended card data again to see if it stuck.
    //

    Status = SdpGetExtendedCardSpecificData(Controller, CardData);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    if (CardData[SD_MMC_EXTENDED_CARD_DATA_HIGH_SPEED] == 0) {
        return STATUS_SUCCESS;
    }

    Controller->CardCapabilities |= SD_MODE_HIGH_SPEED;
    if ((CardType & SD_MMC_CARD_TYPE_HIGH_SPEED_52MHZ) != 0) {
        Controller->CardCapabilities |= SD_MODE_HIGH_SPEED_52MHZ;
    }

    return STATUS_SUCCESS;
}

KSTATUS
SdpSetBlockLength (
    PSD_CONTROLLER Controller,
    ULONG BlockLength
    )

/*++

Routine Description:

    This routine sets the block length in the card.

Arguments:

    Controller - Supplies a pointer to the controller.

    BlockLength - Supplies the block length to set.

Return Value:

    Status code.

--*/

{

    SD_COMMAND Command;
    KSTATUS Status;

    RtlZeroMemory(&Command, sizeof(SD_COMMAND));
    Command.Command = SdCommandSetBlockLength;
    Command.ResponseType = SD_RESPONSE_R1;
    Command.CommandArgument = BlockLength;
    Status = SdpSendCommand(Controller, &Command);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    return Status;
}

KSTATUS
SdpReadBlocksPolled (
    PSD_CONTROLLER Controller,
    ULONGLONG BlockOffset,
    ULONG BlockCount,
    PVOID BufferVirtual
    )

/*++

Routine Description:

    This routine performs a polled block I/O read.

Arguments:

    Controller - Supplies a pointer to the controller.

    BlockOffset - Supplies the logical block address of the I/O.

    BlockCount - Supplies the number of blocks to read or write.

    BufferVirtual - Supplies the virtual address of the I/O buffer.

Return Value:

    Status code.

--*/

{

    SD_COMMAND Command;
    KSTATUS Status;

    ASSERT(BlockCount <= SD_MAX_BLOCK_COUNT);

    if (BlockCount > 1) {
        Command.Command = SdCommandReadMultipleBlocks;

    } else {
        Command.Command = SdCommandReadSingleBlock;
    }

    Command.ResponseType = SD_RESPONSE_R1;
    if ((Controller->Flags & SD_CONTROLLER_FLAG_HIGH_CAPACITY) != 0) {
        Command.CommandArgument = BlockOffset;

    } else {
        Command.CommandArgument = BlockOffset * Controller->ReadBlockLength;
    }

    Command.BufferSize = BlockCount * Controller->ReadBlockLength;
    Command.BufferVirtual = BufferVirtual;
    Command.BufferPhysical = INVALID_PHYSICAL_ADDRESS;
    Command.Write = FALSE;
    Command.Dma = FALSE;
    Status = SdpSendCommand(Controller, &Command);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    if ((BlockCount > 1) &&
        ((Controller->HostCapabilities & SD_MODE_AUTO_CMD12) == 0)) {

        Command.Command = SdCommandStopTransmission;
        Command.CommandArgument = 0;
        Command.ResponseType = SD_RESPONSE_R1B;
        Command.BufferSize = 0;
        Status = SdpSendCommand(Controller, &Command);
        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    return Status;
}

KSTATUS
SdpWriteBlocksPolled (
    PSD_CONTROLLER Controller,
    ULONGLONG BlockOffset,
    ULONG BlockCount,
    PVOID BufferVirtual
    )

/*++

Routine Description:

    This routine performs a polled block I/O write.

Arguments:

    Controller - Supplies a pointer to the controller.

    BlockOffset - Supplies the logical block address of the I/O.

    BlockCount - Supplies the number of blocks to read or write.

    BufferVirtual - Supplies the virtual address of the I/O buffer.

Return Value:

    Status code.

--*/

{

    SD_COMMAND Command;
    KSTATUS Status;

    ASSERT(BlockCount <= SD_MAX_BLOCK_COUNT);

    if (BlockCount > 1) {
        Command.Command = SdCommandWriteMultipleBlocks;

    } else {
        Command.Command = SdCommandWriteSingleBlock;
    }

    Command.ResponseType = SD_RESPONSE_R1;
    if ((Controller->Flags & SD_CONTROLLER_FLAG_HIGH_CAPACITY) != 0) {
        Command.CommandArgument = BlockOffset;

    } else {
        Command.CommandArgument = BlockOffset * Controller->ReadBlockLength;
    }

    Command.BufferSize = BlockCount * Controller->ReadBlockLength;
    Command.BufferVirtual = BufferVirtual;
    Command.BufferPhysical = INVALID_PHYSICAL_ADDRESS;
    Command.Write = TRUE;
    Command.Dma = FALSE;
    Status = SdpSendCommand(Controller, &Command);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // SPI multiblock writes terminate with a special token, not a CMD12. Also
    // skip the CMD12 if the controller is doing it natively.
    //

    if (((Controller->HostCapabilities &
          (SD_MODE_SPI | SD_MODE_AUTO_CMD12)) == 0) &&
        (BlockCount > 1)) {

        Command.Command = SdCommandStopTransmission;
        Command.CommandArgument = 0;
        Command.ResponseType = SD_RESPONSE_R1B;
        Command.BufferSize = 0;
        Status = SdpSendCommand(Controller, &Command);
        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    return Status;
}

KSTATUS
SdpSendCommand (
    PSD_CONTROLLER Controller,
    PSD_COMMAND Command
    )

/*++

Routine Description:

    This routine sends the given command to the card.

Arguments:

    Controller - Supplies a pointer to the controller.

    Command - Supplies a pointer to the command parameters.

Return Value:

    Status code.

--*/

{

    ULONG BlockCount;
    ULONG Flags;
    ULONGLONG Frequency;
    KSTATUS Status;
    ULONGLONG Timeout;
    ULONG Value;

    //
    // Set the DMA interrupts appropriately based on the command.
    //

    SdpSetDmaInterrupts(Controller, Command->Dma);

    //
    // Wait for the previous command to complete.
    //

    Frequency = HlQueryTimeCounterFrequency();
    Timeout = SdpQueryTimeCounter(Controller) +
              (Frequency * SD_CONTROLLER_TIMEOUT);

    Status = STATUS_TIMEOUT;
    do {
        Value = SD_READ_REGISTER(Controller, SdRegisterPresentState);
        if ((Value & (SD_STATE_DATA_INHIBIT | SD_STATE_COMMAND_INHIBIT)) == 0) {
            Status = STATUS_SUCCESS;
            break;
        }

    } while (SdpQueryTimeCounter(Controller) <= Timeout);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Data or commands inhibited: %x\n", Value);
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

            if ((Controller->HostCapabilities & SD_MODE_AUTO_CMD12) != 0) {
                Flags |= SD_COMMAND_AUTO_COMMAND12_ENABLE;
            }

            BlockCount = Command->BufferSize / SD_BLOCK_SIZE;

            ASSERT(BlockCount <= SD_MAX_BLOCK_COUNT);

            Value = SD_BLOCK_SIZE | (BlockCount << 16);
            SD_WRITE_REGISTER(Controller, SdRegisterBlockSizeCount, Value);

        } else {

            ASSERT(Command->BufferSize <= SD_BLOCK_SIZE);

            Value = Command->BufferSize;
            SD_WRITE_REGISTER(Controller, SdRegisterBlockSizeCount, Value);
        }

        Flags |= SD_COMMAND_DATA_PRESENT;
        if (Command->Write != FALSE) {
            Flags |= SD_COMMAND_TRANSFER_WRITE;

        } else {
            Flags |= SD_COMMAND_TRANSFER_READ;
        }

        if (Command->Dma != FALSE) {

            ASSERT((Controller->Flags &
                    SD_CONTROLLER_FLAG_ADMA2_ENABLED) != 0);

            ASSERT((Controller->Flags &
                    SD_CONTROLLER_FLAG_DMA_INTERRUPTS_ENABLED) != 0);

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

    if ((Flags & SD_COMMAND_DMA_ENABLE) != 0) {
        Status = STATUS_SUCCESS;
        goto SendCommandEnd;
    }

    //
    // Worry about waiting for the status flag if the ISR is also clearing it.
    //

    ASSERT(Controller->EnabledInterrupts == SD_INTERRUPT_ENABLE_DEFAULT_MASK);
    ASSERT((Controller->Flags &
            SD_CONTROLLER_FLAG_DMA_INTERRUPTS_ENABLED) == 0);

    Timeout = SdpQueryTimeCounter(Controller) +
              (Frequency * SD_CONTROLLER_TIMEOUT);

    Status = STATUS_TIMEOUT;
    do {
        Value = SD_READ_REGISTER(Controller, SdRegisterInterruptStatus);
        if (Value != 0) {
            Status = STATUS_SUCCESS;
            break;
        }

    } while (SdpQueryTimeCounter(Controller) <= Timeout);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Got no command complete\n");
        goto SendCommandEnd;
    }

    if ((Value & SD_INTERRUPT_STATUS_COMMAND_TIMEOUT_ERROR) != 0) {
        SdpResetController(Controller, SD_CLOCK_CONTROL_RESET_COMMAND_LINE);
        Status = STATUS_TIMEOUT;
        goto SendCommandEnd;

    } else if ((Value & SD_INTERRUPT_STATUS_ERROR_INTERRUPT) != 0) {
        RtlDebugPrint("SD: Error sending command %d: Status %x.\n",
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
    ULONGLONG Frequency;
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

    Frequency = HlQueryTimeCounterFrequency();
    while (Size != 0) {

        //
        // Get the interrupt status register.
        //

        Timeout = SdpQueryTimeCounter(Controller) +
                  (Frequency * SD_CONTROLLER_TIMEOUT);

        Status = STATUS_TIMEOUT;
        do {
            Value = SD_READ_REGISTER(Controller, SdRegisterInterruptStatus);
            if (Value != 0) {
                Status = STATUS_SUCCESS;
                break;
            }

        } while (SdpQueryTimeCounter(Controller) <= Timeout);

        if (!KSUCCESS(Status)) {
            return Status;
        }

        Mask = SD_INTERRUPT_STATUS_DATA_TIMEOUT_ERROR |
               SD_INTERRUPT_STATUS_DATA_CRC_ERROR |
               SD_INTERRUPT_STATUS_DATA_END_BIT_ERROR;

        if ((Value & Mask) != 0) {
            SdpResetController(Controller, SD_CLOCK_CONTROL_RESET_DATA_LINE);
        }

        if ((Value & SD_INTERRUPT_STATUS_ERROR_INTERRUPT) != 0) {
            RtlDebugPrint("SD: Data error on read: Status %x\n", Value);
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
    ULONGLONG Frequency;
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

    Frequency = HlQueryTimeCounterFrequency();
    while (Size != 0) {

        //
        // Get the interrupt status register.
        //

        Timeout = SdpQueryTimeCounter(Controller) +
                  (Frequency * SD_CONTROLLER_TIMEOUT);

        Status = STATUS_TIMEOUT;
        do {
            Value = SD_READ_REGISTER(Controller, SdRegisterInterruptStatus);
            if (Value != 0) {
                Status = STATUS_SUCCESS;
                break;
            }

        } while (SdpQueryTimeCounter(Controller) <= Timeout);

        if (!KSUCCESS(Status)) {
            return Status;
        }

        Mask = SD_INTERRUPT_STATUS_DATA_TIMEOUT_ERROR |
               SD_INTERRUPT_STATUS_DATA_CRC_ERROR |
               SD_INTERRUPT_STATUS_DATA_END_BIT_ERROR;

        if ((Value & Mask) != 0) {
            SdpResetController(Controller, SD_CLOCK_CONTROL_RESET_DATA_LINE);
        }

        if ((Value & SD_INTERRUPT_STATUS_ERROR_INTERRUPT) != 0) {
            RtlDebugPrint("SD : Data error on write: Status %x\n", Value);
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

KSTATUS
SdpResetController (
    PSD_CONTROLLER Controller,
    ULONG ResetBit
    )

/*++

Routine Description:

    This routine performs a soft reset of the SD controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    ResetBit - Supplies the bit to apply to the clock control register to
        cause the reset. See SD_CLOCK_CONTROL_RESET_* definitions.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_TIMEOUT if the reset bit became stuck on.

--*/

{

    ULONGLONG Frequency;
    KSTATUS Status;
    ULONGLONG Timeout;
    ULONG Value;

    Value = SD_READ_REGISTER(Controller, SdRegisterClockControl);
    SD_WRITE_REGISTER(Controller, SdRegisterClockControl, Value | ResetBit);
    Frequency = HlQueryTimeCounterFrequency();
    Timeout = SdpQueryTimeCounter(Controller) +
              (Frequency * SD_CONTROLLER_TIMEOUT);

    Status = STATUS_TIMEOUT;
    do {
        Value = SD_READ_REGISTER(Controller, SdRegisterClockControl);
        if ((Value & ResetBit) == 0) {
            Status = STATUS_SUCCESS;
            break;
        }

    } while (SdpQueryTimeCounter(Controller) <= Timeout);

    Value = SD_READ_REGISTER(Controller, SdRegisterInterruptStatus);
    SD_WRITE_REGISTER(Controller, SdRegisterInterruptStatus, Value);
    return Status;
}

KSTATUS
SdpErrorRecovery (
    PSD_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine attempts to perform recovery after an error.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    //
    // Perform an asychronous abort, which will clear any interrupts, abort the
    // command and reset the command and data lines. This will also wait until
    // the card has returned to the transfer state.
    //

    Status = SdpAsynchronousAbort(Controller);
    if (!KSUCCESS(Status)) {
        goto ErrorRecoveryEnd;
    }

ErrorRecoveryEnd:
    return Status;
}

KSTATUS
SdpSynchronousAbort (
    PSD_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine executes an synchronous abort for the given SD Controller.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    ULONGLONG Frequency;
    KSTATUS Status;
    ULONGLONG Timeout;
    ULONG Value;

    SdpSetDmaInterrupts(Controller, FALSE);
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

    Frequency = HlQueryTimeCounterFrequency();
    Timeout = SdpQueryTimeCounter(Controller) +
              (Frequency * SD_CONTROLLER_TIMEOUT);

    Status = STATUS_TIMEOUT;
    do {
        Value = SD_READ_REGISTER(Controller, SdRegisterInterruptStatus);
        if ((Value & SD_INTERRUPT_STATUS_TRANSFER_COMPLETE) != 0) {
            Status = STATUS_SUCCESS;
            break;
        }

    } while (SdpQueryTimeCounter(Controller) <= Timeout);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("SD: Stop at block gap timed out: 0x%08x\n", Value);
        goto SynchronousAbortEnd;
    }

    //
    // Clear the transfer complete.
    //

    SD_WRITE_REGISTER(Controller,
                      SdRegisterInterruptStatus,
                      SD_INTERRUPT_STATUS_TRANSFER_COMPLETE);

    //
    // Perform an asynchronous abort.
    //

    Status = SdpAsynchronousAbort(Controller);
    if (!KSUCCESS(Status)) {
        goto SynchronousAbortEnd;
    }

SynchronousAbortEnd:
    return Status;
}

KSTATUS
SdpAsynchronousAbort (
    PSD_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine executes an asynchronous abort for the given SD Controller.
    An asynchronous abort involves sending the abort command and then reseting
    the command and data lines.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    ULONG CardStatus;
    ULONGLONG Frequency;
    ULONG Reset;
    KSTATUS Status;
    ULONGLONG Timeout;

    SdpSetDmaInterrupts(Controller, FALSE);

    //
    // Attempt to send the abort command until the card enters the transfer
    // state.
    //

    Frequency = HlQueryTimeCounterFrequency();
    Timeout = SdpQueryTimeCounter(Controller) +
              (Frequency * SD_CONTROLLER_STATUS_TIMEOUT);

    while (TRUE) {
        Status = SdpSendAbortCommand(Controller);
        if (!KSUCCESS(Status)) {
            goto AsynchronousAbortEnd;
        }

        //
        // Reset the command and data lines.
        //

        Reset = SD_CLOCK_CONTROL_RESET_COMMAND_LINE |
                SD_CLOCK_CONTROL_RESET_DATA_LINE;

        Status = SdpResetController(Controller, Reset);
        if (!KSUCCESS(Status)) {
            goto AsynchronousAbortEnd;
        }

        //
        // Check the SD card's status.
        //

        Status = SdpGetCardStatus(Controller, &CardStatus);
        if (!KSUCCESS(Status)) {
            goto AsynchronousAbortEnd;
        }

        //
        // Call it good if the card is ready for data and in the transfer state.
        //

        if (((CardStatus & SD_STATUS_READY_FOR_DATA) != 0) &&
            ((CardStatus & SD_STATUS_CURRENT_STATE) ==
            SD_STATUS_STATE_TRANSFER)) {

            Status = STATUS_SUCCESS;
            break;
        }

        //
        // Exit out if the card is in an error state. The exception is if the
        // only error is an "illegal command" error. It may be that the abort
        // came in on top of the "program" state, which it is not allowed to
        // do.
        //

        if (((CardStatus & SD_STATUS_MASK) != 0) &&
            ((CardStatus & SD_STATUS_MASK) != SD_STATUS_ILLEGAL_COMMAND)) {

            RtlDebugPrint("SD: Abort status error 0x%08x.\n", CardStatus);
            Status = STATUS_DEVICE_IO_ERROR;
            break;
        }

        if (SdpQueryTimeCounter(Controller) > Timeout) {
            Status = STATUS_TIMEOUT;
            break;
        }

        //
        // If the card is long gone, then don't bother to continue.
        //

        if ((Controller->Flags & SD_CONTROLLER_FLAG_MEDIA_PRESENT) == 0) {
            Status = STATUS_NO_MEDIA;
            break;
        }
    }

AsynchronousAbortEnd:
    return Status;
}

KSTATUS
SdpSendAbortCommand (
    PSD_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine sends an abort command to the SD Controller.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    ULONG Command;
    ULONGLONG Frequency;
    KSTATUS Status;
    ULONGLONG Timeout;
    ULONG Value;

    //
    // Wait for the previous command to complete.
    //

    Frequency = HlQueryTimeCounterFrequency();
    Timeout = SdpQueryTimeCounter(Controller) +
              (Frequency * SD_CONTROLLER_TIMEOUT);

    Status = STATUS_TIMEOUT;
    do {
        Value = SD_READ_REGISTER(Controller, SdRegisterPresentState);
        if ((Value & SD_STATE_COMMAND_INHIBIT) == 0) {
            Status = STATUS_SUCCESS;
            break;
        }

    } while (SdpQueryTimeCounter(Controller) <= Timeout);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("SD: Abort commands inhibited: %x\n", Value);
        goto SendAbortCommandEnd;
    }

    //
    // Clear interrupts from the previous command.
    //

    SD_WRITE_REGISTER(Controller,
                      SdRegisterInterruptStatus,
                      SD_INTERRUPT_STATUS_ALL_MASK);

    //
    // Write the abort command. The Argument 1 register is not used for the
    // stop transmission command.
    //

    Command = (SdCommandStopTransmission << SD_COMMAND_INDEX_SHIFT) |
              SD_COMMAND_TYPE_ABORT;

    SD_WRITE_REGISTER(Controller, SdRegisterCommand, Command);

    //
    // Wait for the abort command to complete.
    //

    Timeout = SdpQueryTimeCounter(Controller) +
              (Frequency * SD_CONTROLLER_TIMEOUT);

    Status = STATUS_TIMEOUT;
    do {
        Value = SD_READ_REGISTER(Controller, SdRegisterInterruptStatus);
        if (Value != 0) {
            Status = STATUS_SUCCESS;
            break;
        }

    } while (SdpQueryTimeCounter(Controller) <= Timeout);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("SD: Abort command timed out: 0x%08x\n", Value);
        goto SendAbortCommandEnd;
    }

    //
    // Acknowledge the completion.
    //

    if ((Value & SD_INTERRUPT_STATUS_COMMAND_COMPLETE) != 0) {
        SD_WRITE_REGISTER(Controller,
                          SdRegisterInterruptStatus,
                          SD_INTERRUPT_STATUS_COMMAND_COMPLETE);
    }

    //
    // Handle any errors.
    //

    if ((Value & SD_INTERRUPT_STATUS_COMMAND_TIMEOUT_ERROR) != 0) {
        SdpResetController(Controller, SD_CLOCK_CONTROL_RESET_COMMAND_LINE);
        Status = STATUS_TIMEOUT;
        goto SendAbortCommandEnd;

    } else if ((Value & SD_INTERRUPT_STATUS_ERROR_INTERRUPT) != 0) {
        RtlDebugPrint("SD: Error sending abort: Status %x.\n", Value);
        Status = STATUS_DEVICE_IO_ERROR;
        goto SendAbortCommandEnd;
    }

    Status = STATUS_SUCCESS;

SendAbortCommandEnd:
    return Status;
}

VOID
SdpSetDmaInterrupts (
    PSD_CONTROLLER Controller,
    BOOL Enable
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

Return Value:

    None.

--*/

{

    ULONG Flags;
    ULONG Value;

    Flags = Controller->Flags;

    //
    // Enable the interrupts for transfer completion so that DMA operations
    // can complete asynchronously. Unless, of course, the DMA interrupts are
    // already enabled.
    //

    if (Enable != FALSE) {
        if ((Flags & SD_CONTROLLER_FLAG_DMA_INTERRUPTS_ENABLED) != 0) {
            return;
        }

        Controller->EnabledInterrupts |= SD_INTERRUPT_ENABLE_ERROR_MASK |
                                         SD_INTERRUPT_ENABLE_TRANSFER_COMPLETE;

        RtlMemoryBarrier();
        SD_WRITE_REGISTER(Controller,
                          SdRegisterInterruptSignalEnable,
                          Controller->EnabledInterrupts);

        Value = SD_READ_REGISTER(Controller, SdRegisterInterruptStatusEnable);

        ASSERT((Value & SD_INTERRUPT_ENABLE_TRANSFER_COMPLETE) != 0);

        Value |= SD_INTERRUPT_ENABLE_ERROR_MASK;
        SD_WRITE_REGISTER(Controller, SdRegisterInterruptStatusEnable, Value);
        RtlAtomicOr32(&(Controller->Flags),
                      SD_CONTROLLER_FLAG_DMA_INTERRUPTS_ENABLED);

    //
    // Disable the DMA interrupts so that they do not interfere with polled I/O
    // attempts to check the transfer status. Do nothing if the DMA interrupts
    // are disabled.
    //

    } else {
        if ((Flags & SD_CONTROLLER_FLAG_DMA_INTERRUPTS_ENABLED) == 0) {
            return;
        }

        Controller->EnabledInterrupts &=
                                      ~(SD_INTERRUPT_ENABLE_ERROR_MASK |
                                        SD_INTERRUPT_ENABLE_TRANSFER_COMPLETE);

        RtlMemoryBarrier();
        SD_WRITE_REGISTER(Controller,
                          SdRegisterInterruptSignalEnable,
                          Controller->EnabledInterrupts);

        Value = SD_READ_REGISTER(Controller, SdRegisterInterruptStatusEnable);
        Value &= ~SD_INTERRUPT_ENABLE_ERROR_MASK;
        SD_WRITE_REGISTER(Controller, SdRegisterInterruptStatusEnable, Value);
        RtlAtomicAnd32(&(Controller->Flags),
                       ~SD_CONTROLLER_FLAG_DMA_INTERRUPTS_ENABLED);
    }

    return;
}

ULONGLONG
SdpQueryTimeCounter (
    PSD_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine returns a snap of the time counter. Depending on the mode of
    the SD controller, this may be just a recent snap of the time counter or
    the current value in the hardware.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Returns the number of ticks that have elapsed since the system was booted,
    or a recent tick value.

--*/

{

    if ((Controller->Flags & SD_CONTROLLER_FLAG_CRITICAL_MODE) == 0) {
        return KeGetRecentTimeCounter();
    }

    return HlQueryTimeCounter();
}

