/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

#include <minoca/kernel/driver.h>
#include "sdp.h"

//
// --------------------------------------------------------------------- Macros
//

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
SdpWaitForMmcCardToInitialize (
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
SdpAbort (
    PSD_CONTROLLER Controller,
    BOOL UseR1bResponse
    );

//
// -------------------------------------------------------------------- Globals
//

UCHAR SdFrequencyMultipliers[16] = {
    0,
    10,
    12,
    13,
    15,
    20,
    25,
    30,
    35,
    40,
    45,
    50,
    55,
    60,
    70,
    80
};

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

    Controller = MmAllocateNonPagedPool(sizeof(SD_CONTROLLER),
                                        SD_ALLOCATION_TAG);

    if (Controller == NULL) {
        return NULL;
    }

    RtlZeroMemory(Controller, sizeof(SD_CONTROLLER));
    Controller->ConsumerContext = Parameters->ConsumerContext;
    Controller->Voltages = Parameters->Voltages;
    Controller->FundamentalClock = Parameters->FundamentalClock;
    Controller->HostCapabilities = Parameters->HostCapabilities;
    Controller->OsDevice = Parameters->OsDevice;
    Controller->Flags = SD_CONTROLLER_FLAG_INSERTION_PENDING;
    RtlCopyMemory(&(Controller->FunctionTable),
                  &(Parameters->FunctionTable),
                  sizeof(SD_FUNCTION_TABLE));

    //
    // If this is a standard controller, then do some special initialization
    // steps, including filling out the missing pieces of the function table.
    //

    if (Parameters->StandardControllerBase != NULL) {
        Controller->ControllerBase = Parameters->StandardControllerBase;

        //
        // Fill in gaps that are not present in the supplied function table.
        //

        if (Controller->FunctionTable.InitializeController == NULL) {
            Controller->FunctionTable.InitializeController =
                                       SdStdFunctionTable.InitializeController;
        }

        if (Controller->FunctionTable.ResetController == NULL) {
            Controller->FunctionTable.ResetController =
                                            SdStdFunctionTable.ResetController;
        }

        if (Controller->FunctionTable.SendCommand == NULL) {
            Controller->FunctionTable.SendCommand =
                                                SdStdFunctionTable.SendCommand;
        }

        if (Controller->FunctionTable.GetSetBusWidth == NULL) {
            Controller->FunctionTable.GetSetBusWidth =
                                             SdStdFunctionTable.GetSetBusWidth;
        }

        if (Controller->FunctionTable.GetSetClockSpeed == NULL) {
            Controller->FunctionTable.GetSetClockSpeed =
                                           SdStdFunctionTable.GetSetClockSpeed;
        }

        if (Controller->FunctionTable.GetSetVoltage == NULL) {
            Controller->FunctionTable.GetSetVoltage =
                                              SdStdFunctionTable.GetSetVoltage;
        }

        if (Controller->FunctionTable.StopDataTransfer == NULL) {
            Controller->FunctionTable.StopDataTransfer =
                                           SdStdFunctionTable.StopDataTransfer;
        }

    //
    // Make sure the required functions are present.
    //

    } else {
        if ((Controller->FunctionTable.ResetController == NULL) ||
            (Controller->FunctionTable.SendCommand == NULL) ||
            (Controller->FunctionTable.GetSetBusWidth == NULL) ||
            (Controller->FunctionTable.GetSetClockSpeed == NULL)) {

            Status = STATUS_INVALID_PARAMETER;
            goto CreateControllerEnd;
        }
    }

    if (Controller->FunctionTable.MediaChangeCallback == NULL) {
        Controller->FunctionTable.MediaChangeCallback =
                                        SdStdFunctionTable.MediaChangeCallback;
    }

    Status = STATUS_SUCCESS;

CreateControllerEnd:
    if (!KSUCCESS(Status)) {
        if (Controller != NULL) {
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

    MmFreeNonPagedPool(Controller);
    return;
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
    UCHAR CardData[SD_MMC_MAX_BLOCK_SIZE];
    SD_CARD_IDENTIFICATION CardIdentification;
    BOOL CardPresent;
    ULONG ExtendedCardDataWidth;
    PSD_INITIALIZE_CONTROLLER InitializeController;
    ULONG LoopIndex;
    ULONG OldFlags;
    KSTATUS Status;

    OldFlags = RtlAtomicExchange32(&(Controller->Flags), 0);

    //
    // Compute the timeout delay in time counter ticks.
    //

    Controller->Timeout = (HlQueryTimeCounterFrequency() *
                           SD_CONTROLLER_TIMEOUT_MS) / MILLISECONDS_PER_SECOND;

    //
    // Start by checking for a card.
    //

    if (Controller->FunctionTable.GetCardDetectStatus != NULL) {
        Status = Controller->FunctionTable.GetCardDetectStatus(
                                                 Controller,
                                                 Controller->ConsumerContext,
                                                 &CardPresent);

        if ((!KSUCCESS(Status)) || (CardPresent == FALSE)) {
            goto InitializeControllerEnd;
        }
    }

    //
    // Reset the controller and wait for the reset to finish.
    //

    if (ResetController != FALSE) {
        Status = Controller->FunctionTable.ResetController(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   SD_RESET_FLAG_ALL);

        if (!KSUCCESS(Status)) {
            goto InitializeControllerEnd;
        }
    }

    InitializeController = Controller->FunctionTable.InitializeController;
    if (InitializeController != NULL) {
        Status = InitializeController(Controller,
                                      Controller->ConsumerContext,
                                      0);

        if (!KSUCCESS(Status)) {
            goto InitializeControllerEnd;
        }
    }

    if (Controller->Voltages == 0) {
        Status = STATUS_DEVICE_NOT_CONNECTED;
        goto InitializeControllerEnd;
    }

    //
    // Set the default maximum number of blocks per transfer.
    //

    Controller->MaxBlocksPerTransfer = SD_MAX_BLOCK_COUNT;
    Controller->BusWidth = 1;
    Controller->ClockSpeed = SdClock400kHz;
    Controller->CurrentVoltage = SdVoltage3V3;
    Status = SdpSetBusParameters(Controller);
    if (!KSUCCESS(Status)) {
        goto InitializeControllerEnd;
    }

    if (InitializeController != NULL) {
        Status = InitializeController(Controller,
                                      Controller->ConsumerContext,
                                      1);

        if (!KSUCCESS(Status)) {
            goto InitializeControllerEnd;
        }
    }

    //
    // Begin the initialization sequence as described in the SD specification.
    //

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

    if (SD_IS_CARD_SD(Controller)) {
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
    if (SD_IS_CARD_SD(Controller)) {
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

        if ((Controller->CardCapabilities & SD_MODE_HIGH_SPEED_52MHZ) != 0) {
            Controller->ClockSpeed = SdClock52MHz;

        } else if ((Controller->CardCapabilities & SD_MODE_HIGH_SPEED) != 0) {
            Controller->ClockSpeed = SdClock26MHz;
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

    //
    // If the old flags say there was DMA enabled, reenable it now.
    //

    if ((OldFlags & SD_CONTROLLER_FLAG_DMA_ENABLED) != 0) {
        Status = SdStandardInitializeDma(Controller);
        if (!KSUCCESS(Status)) {
            goto InitializeControllerEnd;
        }
    }

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
    KSTATUS RecoveryStatus;
    KSTATUS Status;

    if ((Controller->Flags & SD_CONTROLLER_FLAG_MEDIA_CHANGED) != 0) {
        return STATUS_MEDIA_CHANGED;

    } else if ((Controller->Flags & SD_CONTROLLER_FLAG_MEDIA_PRESENT) == 0) {
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
            RecoveryStatus = SdErrorRecovery(Controller);
            if (!KSUCCESS(RecoveryStatus)) {
                Status = RecoveryStatus;
            }

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
SdAbortTransaction (
    PSD_CONTROLLER Controller,
    BOOL UseR1bResponse
    )

/*++

Routine Description:

    This routine aborts the current SD transaction on the controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    UseR1bResponse - Supplies a boolean indicating whether to use the R1
        (FALSE) or R1b (TRUE) response for the STOP (CMD12) command.

Return Value:

    Status code.

--*/

{

    return SdpAbort(Controller, UseR1bResponse);
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

SD_API
KSTATUS
SdErrorRecovery (
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

    BOOL Inserted;
    BOOL Match;
    ULONG OldCsd[SD_MMC_CSD_WORDS];
    BOOL Removed;
    KSTATUS Status;

    SdpAbort(Controller, FALSE);
    if ((Controller->Flags & SD_CONTROLLER_FLAG_MEDIA_CHANGED) != 0) {
        return STATUS_MEDIA_CHANGED;
    }

    RtlCopyMemory(OldCsd, Controller->CardSpecificData, sizeof(OldCsd));
    RtlAtomicAnd32(&(Controller->Flags), ~SD_CONTROLLER_FLAG_MEDIA_PRESENT);
    Inserted = FALSE;
    Removed = FALSE;
    Status = SdInitializeController(Controller, TRUE);
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("SD: Card gone: %d\n", Status);
        Removed = TRUE;
    }

    Match = RtlCompareMemory(Controller->CardSpecificData,
                             OldCsd,
                             sizeof(OldCsd));

    if (Match == FALSE) {
        Inserted = TRUE;
        Removed = TRUE;
        RtlDebugPrint("SD: Media changed.\n");
        RtlAtomicOr32(&(Controller->Flags), SD_CONTROLLER_FLAG_MEDIA_CHANGED);
        RtlAtomicAnd32(&(Controller->Flags), ~SD_CONTROLLER_FLAG_MEDIA_PRESENT);
        Status = STATUS_MEDIA_CHANGED;
    }

    //
    // Call the media change callback if something happened.
    //

    if (((Removed != FALSE) || (Inserted != FALSE)) &&
        (Controller->FunctionTable.MediaChangeCallback != NULL)) {

        Controller->FunctionTable.MediaChangeCallback(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   Removed,
                                                   Inserted);
    }

    return Status;
}

SD_API
KSTATUS
SdSendBlockCount (
    PSD_CONTROLLER Controller,
    ULONG BlockCount,
    BOOL Write,
    BOOL InterruptCompletion
    )

/*++

Routine Description:

    This routine sends a CMD23 to pre-specify the block count.

Arguments:

    Controller - Supplies a pointer to the controller.

    BlockCount - Supplies the block count to set.

    Write - Supplies a boolean indicating if this is a write (TRUE) or read
        (FALSE).

    InterruptCompletion - Supplies a boolean indicating whether to poll on
        completion of the command (FALSE) or wait for a transfer done interrupt
        (TRUE).

Return Value:

    STATUS_SUCCESS if the command has been queued.

    STATUS_NOT_SUPPORTED if the card or controller does not support ACMD23.

--*/

{

    SD_COMMAND Command;
    KSTATUS Status;

    if ((Controller->CardCapabilities & SD_MODE_CMD23) == 0) {
        return STATUS_NOT_SUPPORTED;
    }

    if (BlockCount > SD_MAX_CMD23_BLOCKS) {
        BlockCount = SD_MAX_CMD23_BLOCKS;
    }

    RtlZeroMemory(&Command, sizeof(SD_COMMAND));
    Command.Command = SdCommandSetBlockCount;
    Command.ResponseType = SD_RESPONSE_R1;
    Command.CommandArgument = BlockCount;
    Command.Dma = InterruptCompletion;
    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

    return Status;
}

SD_API
KSTATUS
SdSendStop (
    PSD_CONTROLLER Controller,
    BOOL UseR1bResponse,
    BOOL InterruptCompletion
    )

/*++

Routine Description:

    This routine sends a CMD12 to stop the current transfer.

Arguments:

    Controller - Supplies a pointer to the controller.

    UseR1bResponse - Supplies a boolean indicating whether to use an R1b
        response (TRUE) or just R1 (FALSE) for more asynchronous aborts.

    InterruptCompletion - Supplies a boolean indicating whether to poll on
        completion of the command (FALSE) or wait for a transfer done interrupt
        (TRUE).

Return Value:

    Status code.

--*/

{

    SD_COMMAND Command;
    KSTATUS Status;

    RtlZeroMemory(&Command, sizeof(SD_COMMAND));
    Command.Command = SdCommandStopTransmission;
    if (UseR1bResponse != FALSE) {
        Command.ResponseType = SD_RESPONSE_R1B;

    } else {
        Command.ResponseType = SD_RESPONSE_R1;
    }

    Command.Dma = InterruptCompletion;
    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

    return Status;
}

SD_API
ULONGLONG
SdQueryTimeCounter (
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

//
// --------------------------------------------------------- Internal Functions
//

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

    SD_COMMAND Command;
    KSTATUS Status;

    //
    // If going wide, let the card know first.
    //

    if (Controller->BusWidth != 1) {
        if (SD_IS_CARD_SD(Controller)) {
            RtlZeroMemory(&Command, sizeof(SD_COMMAND));
            Command.Command = SdCommandApplicationSpecific;
            Command.ResponseType = SD_RESPONSE_R1;
            Command.CommandArgument = Controller->CardAddress << 16;
            Status = Controller->FunctionTable.SendCommand(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

            if (!KSUCCESS(Status)) {
                return Status;
            }

            Command.Command = SdCommandSetBusWidth;
            Command.ResponseType = SD_RESPONSE_R1;

            ASSERT(Controller->BusWidth == 4);

            Command.CommandArgument = 2;
            Status = Controller->FunctionTable.SendCommand(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

            if (!KSUCCESS(Status)) {
                return Status;
            }

        } else {
            Status = SdpMmcSwitch(Controller,
                                  SD_MMC_EXTENDED_CARD_DATA_BUS_WIDTH,
                                  Controller->BusWidth);

            if (!KSUCCESS(Status)) {
                return Status;
            }
        }

        HlBusySpin(2000);
    }

    Status = Controller->FunctionTable.GetSetBusWidth(
                                               Controller,
                                               Controller->ConsumerContext,
                                               TRUE);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    Status = Controller->FunctionTable.GetSetClockSpeed(
                                               Controller,
                                               Controller->ConsumerContext,
                                               TRUE);

    if (!KSUCCESS(Status)) {
        return Status;
    }

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
    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

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
        Status = Controller->FunctionTable.SendCommand(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

        if (KSUCCESS(Status)) {
            if ((Command.Response[0] & 0xFF) == (SD_COMMAND8_ARGUMENT & 0xFF)) {
                Controller->Version = SdVersion2;

            } else {
                Controller->Version = SdVersion1p0;
            }

            break;
        }
    }

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
            goto WaitForCardToInitializeEnd;
        }

        SdpGetInterfaceCondition(Controller);

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

            //
            // ACMD41 consists of CMD55+CMD41.
            //

            Command.Command = SdCommandApplicationSpecific;
            Command.ResponseType = SD_RESPONSE_R1;
            Command.CommandArgument = 0;
            Status = Controller->FunctionTable.SendCommand(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

            if (!KSUCCESS(Status)) {

                //
                // The card didn't like CMD55. This might be an MMC card. Let's
                // try the old fashioned CMD1 for MMC.
                //

                Status = SdpWaitForMmcCardToInitialize(Controller);
                goto WaitForCardToInitializeEnd;
            }

            Command.Command = SdCommandSendSdOperatingCondition;
            Command.ResponseType = SD_RESPONSE_R3;
            Command.CommandArgument = Ocr;
            if (Retry != 0) {
                if ((Controller->HostCapabilities & SD_MODE_SPI) == 0) {
                    Command.CommandArgument &=
                                  (Controller->Voltages &
                                   SD_OPERATING_CONDITION_VOLTAGE_MASK) |
                                  SD_OPERATING_CONDITION_ACCESS_MODE;

                    //
                    // Attempt to switch to 1.8V if both the card and the
                    // controller support it. In SD there is no 1.65 - 1.95
                    // bits, and the 1.8V bit is a request bit, not a bit the
                    // card advertises.
                    //

                    if ((Controller->Voltages & SD_VOLTAGE_18) != 0) {
                        Command.CommandArgument |= SD_OPERATING_CONDITION_1_8V;
                    }
                }

                if (Controller->Version == SdVersion2) {
                    Command.CommandArgument |=
                                          SD_OPERATING_CONDITION_HIGH_CAPACITY;
                }
            }

            Status = Controller->FunctionTable.SendCommand(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

            if (!KSUCCESS(Status)) {
                goto WaitForCardToInitializeEnd;
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
        Status = STATUS_NOT_READY;
        goto WaitForCardToInitializeEnd;
    }

    if ((Controller->HostCapabilities & SD_MODE_SPI) != 0) {
        Command.Command = SdCommandSpiReadOperatingCondition;
        Command.ResponseType = SD_RESPONSE_R3;
        Command.CommandArgument = 0;
        Status = Controller->FunctionTable.SendCommand(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    ASSERT((Controller->Flags & SD_CONTROLLER_FLAG_HIGH_CAPACITY) == 0);

    if ((Command.Response[0] & SD_OPERATING_CONDITION_HIGH_CAPACITY) != 0) {
        RtlAtomicOr32(&(Controller->Flags), SD_CONTROLLER_FLAG_HIGH_CAPACITY);
    }

    //
    // If the card agrees to switch to 1.8V, perform a CMD11 and switch.
    //

    if ((Command.Response[0] & SD_OPERATING_CONDITION_1_8V) != 0) {
        Command.Command = SdCommandVoltageSwitch;
        Command.ResponseType = SD_RESPONSE_R1;
        Command.CommandArgument = 0;
        Status = Controller->FunctionTable.SendCommand(
                                               Controller,
                                               Controller->ConsumerContext,
                                               &Command);

        //
        // On failure to send CMD11, reset (power cycle) the controller and
        // don't try for 1.8V again.
        //

        if (!KSUCCESS(Status)) {
            RtlDebugPrint("SD: Failed to set 1.8V CMD11: %d.\n", Status);
            Controller->FunctionTable.ResetController(
                                               Controller,
                                               Controller->ConsumerContext,
                                               SD_RESET_FLAG_ALL);

            goto WaitForCardToInitializeEnd;
        }

        //
        // The card seems to need a break in here.
        //

        HlBusySpin(2000);
        Controller->CurrentVoltage = SdVoltage1V8;
        Status = Controller->FunctionTable.GetSetVoltage(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   TRUE);

        if (!KSUCCESS(Status)) {
            RtlDebugPrint("SD: Failed to set 1.8V: %d\n", Status);
            goto WaitForCardToInitializeEnd;
        }
    }

    Status = STATUS_SUCCESS;

WaitForCardToInitializeEnd:
    return Status;
}

KSTATUS
SdpWaitForMmcCardToInitialize (
    PSD_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine sends attempts to wait for the MMC card to initialize by
    sending CMD1.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    SD_COMMAND Command;
    ULONG Ocr;
    ULONGLONG OldTimeout;
    KSTATUS Status;
    ULONGLONG Timeout;

    //
    // The BeagleBoneBlack (rev B) eMMC at least seems to need a stall,
    // otherwise the next command times out.
    //

    HlBusySpin(SD_CARD_DELAY);
    RtlZeroMemory(&Command, sizeof(SD_COMMAND));
    Timeout = HlQueryTimeCounterFrequency() * SD_CMD1_TIMEOUT;
    OldTimeout = Controller->Timeout;
    Controller->Timeout = Timeout;
    Timeout += SdQueryTimeCounter(Controller);
    Ocr = 0;
    while (TRUE) {
        Command.Command = SdCommandSendMmcOperatingCondition;
        Command.ResponseType = SD_RESPONSE_R3;
        Command.CommandArgument = Ocr;
        Command.Response[0] = 0xFFFFFFFF;
        Status = Controller->FunctionTable.SendCommand(
                                           Controller,
                                           Controller->ConsumerContext,
                                           &Command);

        if (!KSUCCESS(Status)) {
            goto WaitForMmcCardToInitializeEnd;
        }

        if (Ocr == 0) {

            //
            // If the operating condition register has never been programmed,
            // write it now and do the whole thing again. If it has been
            // successfully programmed, exit.
            //

            Ocr = Command.Response[0];
            Ocr &= (Controller->Voltages &
                    SD_OPERATING_CONDITION_VOLTAGE_MASK) |
                   SD_OPERATING_CONDITION_ACCESS_MODE;

            Ocr |= SD_OPERATING_CONDITION_HIGH_CAPACITY;
            Status = SdpResetCard(Controller);
            if (!KSUCCESS(Status)) {
                goto WaitForMmcCardToInitializeEnd;
            }

        } else if ((Command.Response[0] & SD_OPERATING_CONDITION_BUSY) != 0) {
            Controller->Version = SdMmcVersion3;
            Status = STATUS_SUCCESS;

            ASSERT((Controller->Flags & SD_CONTROLLER_FLAG_HIGH_CAPACITY) == 0);

            if ((Command.Response[0] &
                 SD_OPERATING_CONDITION_HIGH_CAPACITY) != 0) {

                RtlAtomicOr32(&(Controller->Flags),
                              SD_CONTROLLER_FLAG_HIGH_CAPACITY);
            }

            goto WaitForMmcCardToInitializeEnd;

        } else {
            if (SdQueryTimeCounter(Controller) >= Timeout) {
                break;
            }
        }
    }

    Status = STATUS_TIMEOUT;

WaitForMmcCardToInitializeEnd:
    Controller->Timeout = OldTimeout;
    return Status;
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
    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

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
    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

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
    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    if (SD_IS_CARD_SD(Controller)) {
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
    ULONG Frequency;
    ULONG FrequencyExponent;
    ULONG FrequencyMultiplierIndex;
    ULONG MmcVersion;
    KSTATUS Status;

    RtlZeroMemory(&Command, sizeof(SD_COMMAND));
    Command.Command = SdCommandSendCardSpecificData;
    Command.ResponseType = SD_RESPONSE_R2;
    Command.CommandArgument = Controller->CardAddress << 16;
    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    Status = SdpWaitForStateTransition(Controller);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    if (!SD_IS_CARD_SD(Controller)) {
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
    // Compute the clock speed. This gets clobbered completely for SD cards and
    // may get clobbered for MMC cards.
    //

    FrequencyExponent = Command.Response[0] &
                        SD_CARD_SPECIFIC_DATA_0_FREQUENCY_BASE_MASK;

    Frequency = 10000;
    while (FrequencyExponent != 0) {
        Frequency *= 10;
        FrequencyExponent -= 1;
    }

    FrequencyMultiplierIndex =
                         (Command.Response[0] >>
                          SD_CARD_SPECIFIC_DATA_0_FREQUENCY_MULTIPLIER_SHIFT) &
                          SD_CARD_SPECIFIC_DATA_0_FREQUENCY_MULTIPLIER_MASK;

    Controller->ClockSpeed = Frequency *
                             SdFrequencyMultipliers[FrequencyMultiplierIndex];

    //
    // Compute the read and write block lengths.
    //

    Controller->ReadBlockLength =
        1 << ((Command.Response[1] >>
               SD_CARD_SPECIFIC_DATA_1_READ_BLOCK_LENGTH_SHIFT) &
              SD_CARD_SPECIFIC_DATA_1_READ_BLOCK_LENGTH_MASK);

    if (SD_IS_CARD_SD(Controller)) {
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
    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

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
    if ((SD_IS_CARD_SD(Controller)) ||
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
    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

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

    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

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
    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

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

    Timeout = SdQueryTimeCounter(Controller) +
              (HlQueryTimeCounterFrequency() * SD_CONTROLLER_STATUS_TIMEOUT);

    while (TRUE) {
        Status = Controller->FunctionTable.SendCommand(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

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

            } else if ((Command.Response[0] & SD_STATUS_ERROR_MASK) != 0) {
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

        if (SdQueryTimeCounter(Controller) > Timeout) {
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

    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

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

    RetryCount = SD_CONFIGURATION_REGISTER_RETRY_COUNT;
    while (TRUE) {
        RtlZeroMemory(&Command, sizeof(SD_COMMAND));
        Command.Command = SdCommandApplicationSpecific;
        Command.ResponseType = SD_RESPONSE_R1;
        Command.CommandArgument = Controller->CardAddress << 16;
        Status = Controller->FunctionTable.SendCommand(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

        if (!KSUCCESS(Status)) {
            return Status;
        }

        Command.Command = SdCommandSendSdConfigurationRegister;
        Command.ResponseType = SD_RESPONSE_R1;
        Command.CommandArgument = 0;
        Command.BufferVirtual = ConfigurationRegister;
        Command.BufferSize = sizeof(ConfigurationRegister);
        Status = Controller->FunctionTable.SendCommand(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

        if (!KSUCCESS(Status)) {
            if (RetryCount == 0) {
                return Status;
            }

            RetryCount -= 1;
            KeDelayExecution(FALSE, FALSE, 50000);
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
    // Check for CMD23 support.
    //

    if ((Controller->Version >= SdVersion3) &&
        (ConfigurationRegister[0] & SD_CONFIGURATION_REGISTER_CMD23) != 0) {

        Controller->CardCapabilities |= SD_MODE_CMD23;
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
    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

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
    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    if ((BlockCount > 1) &&
        ((Controller->HostCapabilities & SD_MODE_AUTO_CMD12) == 0)) {

        Status = SdSendStop(Controller, TRUE, FALSE);
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
    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // SPI multiblock writes terminate with a special token, not a CMD12. Also
    // skip the CMD12 if the controller is doing it natively.
    //

    if ((BlockCount > 1) &&
        ((Controller->HostCapabilities &
          (SD_MODE_SPI | SD_MODE_AUTO_CMD12)) == 0)) {

        Status = SdSendStop(Controller, TRUE, FALSE);
        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    return Status;
}

KSTATUS
SdpAbort (
    PSD_CONTROLLER Controller,
    BOOL UseR1bResponse
    )

/*++

Routine Description:

    This routine executes an asynchronous abort for the given SD Controller.
    An asynchronous abort involves sending the abort command and then reseting
    the command and data lines.

Arguments:

    Controller - Supplies a pointer to the controller.

    UseR1bResponse - Supplies a boolean indicating whether to use the R1
        (FALSE) or R1b (TRUE) response for the STOP (CMD12) command.

Return Value:

    Status code.

--*/

{

    ULONG CardStatus;
    ULONGLONG Frequency;
    ULONG ResetFlags;
    KSTATUS Status;
    ULONGLONG Timeout;

    if (Controller->FunctionTable.StopDataTransfer != NULL) {
        Controller->FunctionTable.StopDataTransfer(Controller,
                                                   Controller->ConsumerContext);
    }

    //
    // Attempt to send the abort command until the card enters the transfer
    // state.
    //

    Frequency = HlQueryTimeCounterFrequency();
    Timeout = SdQueryTimeCounter(Controller) +
              (Frequency * SD_CONTROLLER_STATUS_TIMEOUT);

    while (TRUE) {

        //
        // Reset the command and data lines.
        //

        ResetFlags = SD_RESET_FLAG_COMMAND_LINE | SD_RESET_FLAG_DATA_LINE;
        Status = Controller->FunctionTable.ResetController(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   ResetFlags);

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

        Status = SdSendStop(Controller, UseR1bResponse, FALSE);
        if (!KSUCCESS(Status)) {
            goto AsynchronousAbortEnd;
        }

        if ((CardStatus & SD_STATUS_ERROR_MASK) != 0) {
            RtlDebugPrint("SD: Card error status 0x%08x\n", CardStatus);
        }

        if (SdQueryTimeCounter(Controller) > Timeout) {
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
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("SD: Error Recovery failed: %d\n", Status);
    }

    return Status;
}

