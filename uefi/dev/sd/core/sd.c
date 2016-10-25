/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sd.c

Abstract:

    This module implements the library functionality for the SD/MMC device.

Author:

    Evan Green 27-Feb-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
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

EFI_STATUS
EfipSdSetBusParameters (
    PEFI_SD_CONTROLLER Controller
    );

EFI_STATUS
EfipSdResetCard (
    PEFI_SD_CONTROLLER Controller
    );

EFI_STATUS
EfipSdGetInterfaceCondition (
    PEFI_SD_CONTROLLER Controller
    );

EFI_STATUS
EfipSdWaitForCardToInitialize (
    PEFI_SD_CONTROLLER Controller
    );

EFI_STATUS
EfipSdWaitForMmcCardToInitialize (
    PEFI_SD_CONTROLLER Controller
    );

EFI_STATUS
EfipSdSetCrc (
    PEFI_SD_CONTROLLER Controller,
    BOOLEAN Enable
    );

EFI_STATUS
EfipSdGetCardIdentification (
    PEFI_SD_CONTROLLER Controller,
    PSD_CARD_IDENTIFICATION Identification
    );

EFI_STATUS
EfipSdSetupAddressing (
    PEFI_SD_CONTROLLER Controller
    );

EFI_STATUS
EfipSdReadCardSpecificData (
    PEFI_SD_CONTROLLER Controller
    );

EFI_STATUS
EfipSdSelectCard (
    PEFI_SD_CONTROLLER Controller
    );

EFI_STATUS
EfipSdConfigureEraseGroup (
    PEFI_SD_CONTROLLER Controller
    );

EFI_STATUS
EfipSdGetExtendedCardSpecificData (
    PEFI_SD_CONTROLLER Controller,
    UINT8 Data[SD_MMC_MAX_BLOCK_SIZE]
    );

EFI_STATUS
EfipSdMmcSwitch (
    PEFI_SD_CONTROLLER Controller,
    UINT8 Index,
    UINT8 Value
    );

EFI_STATUS
EfipSdSdSwitch (
    PEFI_SD_CONTROLLER Controller,
    UINT32 Mode,
    UINT32 Group,
    UINT8 Value,
    UINT32 Response[16]
    );

EFI_STATUS
EfipSdWaitForStateTransition (
    PEFI_SD_CONTROLLER Controller
    );

EFI_STATUS
EfipSdGetCardStatus (
    PEFI_SD_CONTROLLER Controller,
    UINT32 *CardStatus
    );

EFI_STATUS
EfipSdSetSdFrequency (
    PEFI_SD_CONTROLLER Controller
    );

EFI_STATUS
EfipSdSetMmcFrequency (
    PEFI_SD_CONTROLLER Controller
    );

EFI_STATUS
EfipSdSetBlockLength (
    PEFI_SD_CONTROLLER Controller,
    UINT32 BlockLength
    );

EFI_STATUS
EfipSdReadBlocksPolled (
    PEFI_SD_CONTROLLER Controller,
    UINT64 BlockOffset,
    UINT32 BlockCount,
    VOID *Buffer
    );

EFI_STATUS
EfipSdWriteBlocksPolled (
    PEFI_SD_CONTROLLER Controller,
    UINT64 BlockOffset,
    UINT32 BlockCount,
    VOID *Buffer
    );

EFI_STATUS
EfipSdErrorRecovery (
    PEFI_SD_CONTROLLER Controller
    );

EFI_STATUS
EfipSdAsynchronousAbort (
    PEFI_SD_CONTROLLER Controller
    );

EFI_STATUS
EfipSdSendAbortCommand (
    PEFI_SD_CONTROLLER Controller
    );

UINT32
EfipSdByteSwap32 (
    UINT32 Input
    );

//
// -------------------------------------------------------------------- Globals
//

UINT8 EfiSdFrequencyMultipliers[16] = {
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

PEFI_SD_CONTROLLER
EfiSdCreateController (
    PEFI_SD_INITIALIZATION_BLOCK Parameters
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

    PEFI_SD_CONTROLLER Controller;
    EFI_STATUS Status;

    //
    // Either the standard controller base should be supplied or a set of
    // override functions. Not both.
    //

    if (((Parameters->StandardControllerBase == NULL) &&
         (Parameters->OverrideFunctionTable == NULL)) ||
        ((Parameters->StandardControllerBase != NULL) &&
         (Parameters->OverrideFunctionTable != NULL))) {

        return NULL;
    }

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_SD_CONTROLLER),
                             (VOID **)&Controller);

    if (EFI_ERROR(Status)) {
        goto CreateControllerEnd;
    }

    EfiSetMem(Controller, sizeof(EFI_SD_CONTROLLER), 0);
    Controller->ControllerBase = Parameters->StandardControllerBase;
    Controller->ConsumerContext = Parameters->ConsumerContext;
    Controller->GetCardDetectStatus = Parameters->GetCardDetectStatus;
    Controller->GetWriteProtectStatus = Parameters->GetWriteProtectStatus;
    Controller->Voltages = Parameters->Voltages;
    Controller->FundamentalClock = Parameters->FundamentalClock;
    Controller->HostCapabilities = Parameters->HostCapabilities;

    //
    // Either copy the override function table or the standard table.
    //

    if (Parameters->OverrideFunctionTable != NULL) {
        EfiCopyMem(&(Controller->FunctionTable),
                   Parameters->OverrideFunctionTable,
                   sizeof(SD_FUNCTION_TABLE));

    } else {
        EfiCopyMem(&(Controller->FunctionTable),
                   &EfiSdStdFunctionTable,
                   sizeof(SD_FUNCTION_TABLE));
    }

    Status = EFI_SUCCESS;

CreateControllerEnd:
    if (EFI_ERROR(Status)) {
        if (Controller != NULL) {
            EfiFreePool(Controller);
            Controller = NULL;
        }
    }

    return Controller;
}

VOID
EfiSdDestroyController (
    PEFI_SD_CONTROLLER Controller
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

    EfiFreePool(Controller);
    return;
}

EFI_STATUS
EfiSdInitializeController (
    PEFI_SD_CONTROLLER Controller,
    BOOLEAN ResetController
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

    UINT32 BusWidth;
    UINT8 CardData[SD_MMC_MAX_BLOCK_SIZE];
    SD_CARD_IDENTIFICATION CardIdentification;
    BOOLEAN CardPresent;
    UINT32 ExtendedCardDataWidth;
    UINT32 LoopIndex;
    EFI_STATUS Status;

    //
    // Start by checking for a card.
    //

    if (Controller->GetCardDetectStatus != NULL) {
        Status = Controller->GetCardDetectStatus(Controller,
                                                 Controller->ConsumerContext,
                                                 &CardPresent);

        if ((EFI_ERROR(Status)) || (CardPresent == FALSE)) {
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

        if (EFI_ERROR(Status)) {
            goto InitializeControllerEnd;
        }
    }

    Status = Controller->FunctionTable.InitializeController(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   0);

    if (EFI_ERROR(Status)) {
        goto InitializeControllerEnd;
    }

    //
    // Set the default maximum number of blocks per transfer.
    //

    Controller->MaxBlocksPerTransfer = SD_MAX_BLOCK_COUNT;
    Controller->BusWidth = 1;
    Controller->ClockSpeed = SdClock400kHz;
    Status = EfipSdSetBusParameters(Controller);
    if (EFI_ERROR(Status)) {
        goto InitializeControllerEnd;
    }

    Status = Controller->FunctionTable.InitializeController(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   1);

    if (EFI_ERROR(Status)) {
        goto InitializeControllerEnd;
    }

    //
    // Begin the initialization sequence as described in the SD specification.
    //

    Status = EfipSdWaitForCardToInitialize(Controller);
    if (EFI_ERROR(Status)) {
        goto InitializeControllerEnd;
    }

    if ((Controller->HostCapabilities & SD_MODE_SPI) != 0) {
        Status = EfipSdSetCrc(Controller, TRUE);
        if (EFI_ERROR(Status)) {
            goto InitializeControllerEnd;
        }
    }

    Status = EfipSdGetCardIdentification(Controller, &CardIdentification);
    if (EFI_ERROR(Status)) {
        goto InitializeControllerEnd;
    }

    Status = EfipSdSetupAddressing(Controller);
    if (EFI_ERROR(Status)) {
        goto InitializeControllerEnd;
    }

    Status = EfipSdReadCardSpecificData(Controller);
    if (EFI_ERROR(Status)) {
        goto InitializeControllerEnd;
    }

    Status = EfipSdSelectCard(Controller);
    if (EFI_ERROR(Status)) {
        goto InitializeControllerEnd;
    }

    Status = EfipSdConfigureEraseGroup(Controller);
    if (EFI_ERROR(Status)) {
        goto InitializeControllerEnd;
    }

    if (SD_IS_CARD_SD(Controller)) {
        Status = EfipSdSetSdFrequency(Controller);

    } else {
        Status = EfipSdSetMmcFrequency(Controller);
    }

    if (EFI_ERROR(Status)) {
        goto InitializeControllerEnd;
    }

    EfiStall(10000);

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

        Status = EfipSdSetBusParameters(Controller);
        if (EFI_ERROR(Status)) {
            goto InitializeControllerEnd;
        }

    } else {
        Status = EFI_UNSUPPORTED;
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
                ExtendedCardDataWidth = SD_MMC_EXTENDED_CARD_DATA_BUS_WIDTH_1;
                BusWidth = 1;
            }

            Status = EfipSdMmcSwitch(Controller,
                                     SD_MMC_EXTENDED_CARD_DATA_BUS_WIDTH,
                                     ExtendedCardDataWidth);

            if (EFI_ERROR(Status)) {
                continue;
            }

            Controller->BusWidth = BusWidth;
            Status = EfipSdSetBusParameters(Controller);
            if (EFI_ERROR(Status)) {
                goto InitializeControllerEnd;
            }

            Status = EfipSdGetExtendedCardSpecificData(Controller, CardData);
            if (!EFI_ERROR(Status)) {
                if (BusWidth == 8) {
                    Controller->CardCapabilities |= SD_MODE_8BIT;

                } else if (BusWidth == 4) {
                    Controller->CardCapabilities |= SD_MODE_4BIT;
                }

                break;
            }
        }

        if (EFI_ERROR(Status)) {
            goto InitializeControllerEnd;
        }

        if ((Controller->CardCapabilities & SD_MODE_HIGH_SPEED_52MHZ) != 0) {
            Controller->ClockSpeed = SdClock52MHz;

        } else if ((Controller->CardCapabilities & SD_MODE_HIGH_SPEED) != 0) {
            Controller->ClockSpeed = SdClock26MHz;
        }

        Status = EfipSdSetBusParameters(Controller);
        if (EFI_ERROR(Status)) {
            goto InitializeControllerEnd;
        }
    }

    for (LoopIndex = 0;
         LoopIndex < SD_SET_BLOCK_LENGTH_RETRY_COUNT;
         LoopIndex += 1) {

        Status = EfipSdSetBlockLength(Controller, Controller->ReadBlockLength);
        if (!EFI_ERROR(Status)) {
            break;
        }
    }

    if (EFI_ERROR(Status)) {
        goto InitializeControllerEnd;
    }

    Status = EFI_SUCCESS;

InitializeControllerEnd:
    return Status;
}

EFI_STATUS
EfiSdBlockIoPolled (
    PEFI_SD_CONTROLLER Controller,
    UINT64 BlockOffset,
    UINTN BlockCount,
    VOID *Buffer,
    BOOLEAN Write
    )

/*++

Routine Description:

    This routine performs a block I/O read or write using the CPU and not
    DMA.

Arguments:

    Controller - Supplies a pointer to the controller.

    BlockOffset - Supplies the logical block address of the I/O.

    BlockCount - Supplies the number of blocks to read or write.

    Buffer - Supplies the virtual address of the I/O buffer.

    Write - Supplies a boolean indicating if this is a read operation (FALSE)
        or a write operation.

Return Value:

    Status code.

--*/

{

    UINTN BlocksDone;
    UINTN BlocksThisRound;
    EFI_STATUS Status;
    UINT32 Try;

    Status = EFI_INVALID_PARAMETER;
    BlocksDone = 0;
    Try = 0;
    while (BlocksDone != BlockCount) {
        BlocksThisRound = BlockCount - BlocksDone;
        if (BlocksThisRound > Controller->MaxBlocksPerTransfer) {
            BlocksThisRound = Controller->MaxBlocksPerTransfer;
        }

        if (Write != FALSE) {
            Status = EfipSdWriteBlocksPolled(Controller,
                                             BlockOffset + BlocksDone,
                                             BlocksThisRound,
                                             Buffer);

        } else {
            Status = EfipSdReadBlocksPolled(Controller,
                                            BlockOffset + BlocksDone,
                                            BlocksThisRound,
                                            Buffer);
        }

        if (EFI_ERROR(Status)) {
            if (Try >= EFI_SD_IO_RETRIES) {
                break;
            }

            Status = EfipSdErrorRecovery(Controller);
            if (EFI_ERROR(Status)) {
                break;
            }

            Try += 1;
            continue;
        }

        BlocksDone += BlocksThisRound;
        Buffer += BlocksThisRound * Controller->ReadBlockLength;
    }

    return Status;
}

EFI_STATUS
EfiSdGetMediaParameters (
    PEFI_SD_CONTROLLER Controller,
    UINT64 *BlockCount,
    UINT32 *BlockSize
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

    EFI_SUCCESS on success.

    EFI_NO_MEDIA if there is no card in the slot.

--*/

{

    UINT32 BiggestBlockSize;

    BiggestBlockSize = Controller->ReadBlockLength;
    if (Controller->WriteBlockLength > BiggestBlockSize) {
        BiggestBlockSize = Controller->WriteBlockLength;
    }

    if (BiggestBlockSize == 0) {
        return EFI_NO_MEDIA;
    }

    if (BlockSize != NULL) {
        *BlockSize = BiggestBlockSize;
    }

    if (BlockCount != NULL) {
        *BlockCount = Controller->UserCapacity / Controller->ReadBlockLength;
    }

    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

EFI_STATUS
EfipSdSetBusParameters (
    PEFI_SD_CONTROLLER Controller
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

    UINT32 ClockSpeed;
    SD_COMMAND Command;
    EFI_STATUS Status;

    //
    // If going wide, let the card know first.
    //

    if (Controller->BusWidth != 1) {
        if (SD_IS_CARD_SD(Controller)) {
            EfiSetMem(&Command, sizeof(SD_COMMAND), 0);
            Command.Command = SdCommandApplicationSpecific;
            Command.ResponseType = SD_RESPONSE_R1;
            Command.CommandArgument = Controller->CardAddress << 16;
            Status = Controller->FunctionTable.SendCommand(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

            if (EFI_ERROR(Status)) {
                return Status;
            }

            Command.Command = SdCommandSetBusWidth;
            Command.ResponseType = SD_RESPONSE_R1;
            Command.CommandArgument = 2;
            Status = Controller->FunctionTable.SendCommand(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

            if (EFI_ERROR(Status)) {
                return Status;
            }

        } else {
            Status = EfipSdMmcSwitch(Controller,
                                     SD_MMC_EXTENDED_CARD_DATA_BUS_WIDTH,
                                     Controller->BusWidth);

            if (EFI_ERROR(Status)) {
                return Status;
            }
        }

        EfiStall(2000);
    }

    Status = Controller->FunctionTable.GetSetBusWidth(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   &(Controller->BusWidth),
                                                   TRUE);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    ClockSpeed = (UINT32)Controller->ClockSpeed;
    Status = Controller->FunctionTable.GetSetClockSpeed(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   &ClockSpeed,
                                                   TRUE);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS
EfipSdResetCard (
    PEFI_SD_CONTROLLER Controller
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
    EFI_STATUS Status;

    EfiStall(SD_CARD_DELAY);
    EfiSetMem(&Command, sizeof(SD_COMMAND), 0);
    Command.Command = SdCommandReset;
    Command.CommandArgument = 0;
    Command.ResponseType = SD_RESPONSE_NONE;
    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfiStall(SD_POST_RESET_DELAY);
    return Status;
}

EFI_STATUS
EfipSdGetInterfaceCondition (
    PEFI_SD_CONTROLLER Controller
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
    EFI_STATUS Status;
    UINTN Try;

    Status = EFI_DEVICE_ERROR;
    for (Try = 0; Try < SD_INTERFACE_CONDITION_RETRY_COUNT; Try += 1) {
        EfiSetMem(&Command, sizeof(SD_COMMAND), 0);
        Command.Command = SdCommandSendInterfaceCondition;
        Command.CommandArgument = SD_COMMAND8_ARGUMENT;
        Command.ResponseType = SD_RESPONSE_R7;
        Status = Controller->FunctionTable.SendCommand(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

        EfiStall(50);
        if (!EFI_ERROR(Status)) {
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

EFI_STATUS
EfipSdWaitForCardToInitialize (
    PEFI_SD_CONTROLLER Controller
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
    UINTN LoopIndex;
    UINT32 Ocr;
    UINTN Retry;
    EFI_STATUS Status;

    EfiSetMem(&Command, sizeof(SD_COMMAND), 0);
    for (LoopIndex = 0;
         LoopIndex < SD_CARD_INITIALIZE_RETRY_COUNT;
         LoopIndex += 1) {

        Status = EfipSdResetCard(Controller);
        if (EFI_ERROR(Status)) {
            goto WaitForCardToInitializeEnd;
        }

        EfipSdGetInterfaceCondition(Controller);

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

            if (EFI_ERROR(Status)) {

                //
                // The card didn't like CMD55. This might be an MMC card. Let's
                // try the old fashioned CMD1 for MMC.
                //

                Status = EfipSdWaitForMmcCardToInitialize(Controller);
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

            if (EFI_ERROR(Status)) {
                goto WaitForCardToInitializeEnd;
            }

            EfiStall(SD_CARD_DELAY);
            Retry += 1;
            if ((Command.Response[0] & Controller->Voltages) == 0) {
                return EFI_UNSUPPORTED;
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
        return EFI_NOT_READY;
    }

    if ((Controller->HostCapabilities & SD_MODE_SPI) != 0) {
        Command.Command = SdCommandSpiReadOperatingCondition;
        Command.ResponseType = SD_RESPONSE_R3;
        Command.CommandArgument = 0;
        Status = Controller->FunctionTable.SendCommand(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    Controller->HighCapacity = FALSE;
    if ((Command.Response[0] & SD_OPERATING_CONDITION_HIGH_CAPACITY) != 0) {
        Controller->HighCapacity = TRUE;
    }

    Status = EFI_SUCCESS;

WaitForCardToInitializeEnd:
    return Status;
}

EFI_STATUS
EfipSdWaitForMmcCardToInitialize (
    PEFI_SD_CONTROLLER Controller
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
    UINT32 Ocr;
    UINT32 Retry;
    EFI_STATUS Status;

    //
    // The BeagleBoneBlack (rev B) eMMC at least seems to need a stall,
    // otherwise the next command times out.
    //

    EfiStall(SD_CARD_DELAY);
    EfiSetMem(&Command, sizeof(SD_COMMAND), 0);
    Retry = 0;
    Ocr = 0;
    while (TRUE) {
        if (Retry == SD_CARD_OPERATING_CONDITION_RETRY_COUNT) {
            break;
        }

        Command.Command = SdCommandSendMmcOperatingCondition;
        Command.ResponseType = SD_RESPONSE_R3;
        Command.CommandArgument = Ocr;
        Command.Response[0] = 0xFFFFFFFF;
        Status = Controller->FunctionTable.SendCommand(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

        if (EFI_ERROR(Status)) {
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
            Status = EfipSdResetCard(Controller);
            if (EFI_ERROR(Status)) {
                goto WaitForMmcCardToInitializeEnd;
            }

        } else if ((Command.Response[0] & SD_OPERATING_CONDITION_BUSY) != 0) {
            Controller->Version = SdMmcVersion3;
            Status = EFI_SUCCESS;
            if ((Command.Response[0] &
                 SD_OPERATING_CONDITION_HIGH_CAPACITY) != 0) {

                Controller->HighCapacity = TRUE;
            }

            goto WaitForMmcCardToInitializeEnd;

        } else {
            Retry += 1;
        }

        EfiStall(SD_CARD_DELAY);
    }

    Status = EFI_NOT_READY;

WaitForMmcCardToInitializeEnd:
    return Status;
}

EFI_STATUS
EfipSdSetCrc (
    PEFI_SD_CONTROLLER Controller,
    BOOLEAN Enable
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
    EFI_STATUS Status;

    EfiSetMem(&Command, sizeof(SD_COMMAND), 0);
    Command.Command = SdCommandSpiCrcOnOff;
    Command.CommandArgument = 0;
    if (Enable != FALSE) {
        Command.CommandArgument = 1;
    }

    Command.ResponseType = SD_RESPONSE_R1;
    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    return Status;
}

EFI_STATUS
EfipSdGetCardIdentification (
    PEFI_SD_CONTROLLER Controller,
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
    EFI_STATUS Status;

    EfiSetMem(&Command, sizeof(SD_COMMAND), 0);
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

    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfiCopyMem(Identification,
               &(Command.Response),
               sizeof(SD_CARD_IDENTIFICATION));

    return Status;
}

EFI_STATUS
EfipSdSetupAddressing (
    PEFI_SD_CONTROLLER Controller
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
    EFI_STATUS Status;

    EfiSetMem(&Command, sizeof(SD_COMMAND), 0);
    if ((Controller->HostCapabilities & SD_MODE_SPI) != 0) {
        return EFI_SUCCESS;
    }

    Command.Command = SdCommandSetRelativeAddress;
    Command.ResponseType = SD_RESPONSE_R6;
    Command.CommandArgument = Controller->CardAddress << 16;
    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    if (SD_IS_CARD_SD(Controller)) {
        Controller->CardAddress = (Command.Response[0] >> 16) & 0xFFFF;
    }

    return Status;
}

EFI_STATUS
EfipSdReadCardSpecificData (
    PEFI_SD_CONTROLLER Controller
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

    UINT64 CapacityBase;
    UINT32 CapacityShift;
    SD_COMMAND Command;
    UINT32 Frequency;
    UINT32 FrequencyExponent;
    UINT32 FrequencyMultiplierIndex;
    UINT32 MmcVersion;
    EFI_STATUS Status;

    EfiSetMem(&Command, sizeof(SD_COMMAND), 0);
    Command.Command = SdCommandSendCardSpecificData;
    Command.ResponseType = SD_RESPONSE_R2;
    Command.CommandArgument = Controller->CardAddress << 16;
    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfipSdWaitForStateTransition(Controller);
    if (EFI_ERROR(Status)) {
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

    Controller->ClockSpeed =
               Frequency * EfiSdFrequencyMultipliers[FrequencyMultiplierIndex];

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

    if (Controller->HighCapacity != FALSE) {
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

    Controller->CardSpecificData[0] = Command.Response[0];
    Controller->CardSpecificData[1] = Command.Response[1];
    Controller->CardSpecificData[2] = Command.Response[2];
    Controller->CardSpecificData[3] = Command.Response[3];
    return EFI_SUCCESS;
}

EFI_STATUS
EfipSdSelectCard (
    PEFI_SD_CONTROLLER Controller
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
    EFI_STATUS Status;

    //
    // This command is not supported in SPI mode.
    //

    if ((Controller->HostCapabilities & SD_MODE_SPI) != 0) {
        return EFI_SUCCESS;
    }

    EfiSetMem(&Command, sizeof(SD_COMMAND), 0);
    Command.Command = SdCommandSelectCard;
    Command.ResponseType = SD_RESPONSE_R1;
    Command.CommandArgument = Controller->CardAddress << 16;
    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfipSdWaitForStateTransition(Controller);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    return Status;
}

EFI_STATUS
EfipSdConfigureEraseGroup (
    PEFI_SD_CONTROLLER Controller
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

    UINT64 Capacity;
    UINT8 CardData[SD_MMC_MAX_BLOCK_SIZE];
    UINT32 EraseGroupMultiplier;
    UINT32 EraseGroupSize;
    UINT32 Offset;
    UINT32 PartitionIndex;
    EFI_STATUS Status;

    //
    // For SD, the erase group is always one sector.
    //

    Controller->EraseGroupSize = 1;
    Controller->PartitionConfiguration = SD_MMC_PARTITION_NONE;
    if ((SD_IS_CARD_SD(Controller)) ||
        (Controller->Version < SdMmcVersion4)) {

        return EFI_SUCCESS;
    }

    Status = EfipSdGetExtendedCardSpecificData(Controller, CardData);
    if (EFI_ERROR(Status)) {
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

        Status = EfipSdMmcSwitch(Controller,
                                 SD_MMC_EXTENDED_CARD_DATA_ERASE_GROUP_DEF,
                                 1);

        if (EFI_ERROR(Status)) {
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

    return EFI_SUCCESS;
}

EFI_STATUS
EfipSdGetExtendedCardSpecificData (
    PEFI_SD_CONTROLLER Controller,
    UINT8 Data[SD_MMC_MAX_BLOCK_SIZE]
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
    EFI_STATUS Status;

    EfiSetMem(&Command, sizeof(SD_COMMAND), 0);
    Command.Command = SdCommandMmcSendExtendedCardSpecificData;
    Command.ResponseType = SD_RESPONSE_R1;
    Command.Buffer = Data;
    Command.BufferSize = SD_MMC_MAX_BLOCK_SIZE;
    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    return Status;
}

EFI_STATUS
EfipSdMmcSwitch (
    PEFI_SD_CONTROLLER Controller,
    UINT8 Index,
    UINT8 Value
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
    EFI_STATUS Status;

    EfiSetMem(&Command, sizeof(SD_COMMAND), 0);
    Command.Command = SdCommandSwitch;
    Command.ResponseType = SD_RESPONSE_R1B;
    Command.CommandArgument = (SD_MMC_SWITCH_MODE_WRITE_BYTE <<
                               SD_MMC_SWITCH_MODE_SHIFT) |
                              (Index << SD_MMC_SWITCH_INDEX_SHIFT) |
                              (Value << SD_MMC_SWITCH_VALUE_SHIFT);

    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfipSdWaitForStateTransition(Controller);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    return Status;
}

EFI_STATUS
EfipSdSdSwitch (
    PEFI_SD_CONTROLLER Controller,
    UINT32 Mode,
    UINT32 Group,
    UINT8 Value,
    UINT32 Response[16]
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
    EFI_STATUS Status;

    EfiSetMem(&Command, sizeof(SD_COMMAND), 0);
    Command.Command = SdCommandSwitch;
    Command.ResponseType = SD_RESPONSE_R1;
    Command.CommandArgument = (Mode << 31) | 0x00FFFFFF;
    Command.CommandArgument &= ~(0xF << (Group * 4));
    Command.CommandArgument |= Value << (Group * 4);
    Command.Buffer = Response;
    Command.BufferSize = sizeof(UINT32) * 16;
    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    return Status;
}

EFI_STATUS
EfipSdWaitForStateTransition (
    PEFI_SD_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine attempts to read the card status.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    SD_COMMAND Command;
    EFI_STATUS Status;
    UINT64 Time;
    UINT64 Timeout;

    EfiSetMem(&Command, sizeof(SD_COMMAND), 0);
    Command.Command = SdCommandSendStatus;
    Command.ResponseType = SD_RESPONSE_R1;
    if ((Controller->HostCapabilities & SD_MODE_SPI) == 0) {
        Command.CommandArgument = Controller->CardAddress << 16;
    }

    Timeout = EFI_SD_CONTROLLER_STATUS_TIMEOUT;
    Time = 0;
    while (TRUE) {
        Status = Controller->FunctionTable.SendCommand(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

        if (!EFI_ERROR(Status)) {

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
                return EFI_DEVICE_ERROR;
            }
        }

        EfiStall(50);
        Time += 50;
        if (Time > Timeout) {
            Status = EFI_TIMEOUT;
            break;
        }
    }

    return Status;
}

EFI_STATUS
EfipSdGetCardStatus (
    PEFI_SD_CONTROLLER Controller,
    UINT32 *CardStatus
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
    EFI_STATUS Status;

    EfiSetMem(&Command, sizeof(SD_COMMAND), 0);
    Command.Command = SdCommandSendStatus;
    Command.ResponseType = SD_RESPONSE_R1;
    if ((Controller->HostCapabilities & SD_MODE_SPI) == 0) {
        Command.CommandArgument = Controller->CardAddress << 16;
    }

    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

    if (!EFI_ERROR(Status)) {
        *CardStatus = Command.Response[0];
    }

    return Status;
}

EFI_STATUS
EfipSdSetSdFrequency (
    PEFI_SD_CONTROLLER Controller
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
    UINT32 ConfigurationRegister[2];
    UINT32 RetryCount;
    EFI_STATUS Status;
    UINT32 SwitchStatus[16];
    UINT32 Version;

    Controller->CardCapabilities = 0;
    if ((Controller->HostCapabilities & SD_MODE_SPI) != 0) {
        return EFI_SUCCESS;
    }

    //
    // Read the SCR to find out if the card supports higher speeds.
    //

    EfiSetMem(&Command, sizeof(SD_COMMAND), 0);
    Command.Command = SdCommandApplicationSpecific;
    Command.ResponseType = SD_RESPONSE_R1;
    Command.CommandArgument = Controller->CardAddress << 16;
    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Command.Command = SdCommandSendSdConfigurationRegister;
    Command.ResponseType = SD_RESPONSE_R1;
    Command.CommandArgument = 0;
    Command.Buffer = ConfigurationRegister;
    Command.BufferSize = sizeof(ConfigurationRegister);
    RetryCount = SD_CONFIGURATION_REGISTER_RETRY_COUNT;
    while (TRUE) {
        EfiStall(50000);
        Status = Controller->FunctionTable.SendCommand(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

        if (EFI_ERROR(Status)) {
            if (RetryCount == 0) {
                return Status;
            }

            RetryCount -= 1;
            continue;
        }

        break;
    }

    ConfigurationRegister[0] = EfipSdByteSwap32(ConfigurationRegister[0]);
    ConfigurationRegister[1] = EfipSdByteSwap32(ConfigurationRegister[1]);
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
        return EFI_SUCCESS;
    }

    RetryCount = SD_SWITCH_RETRY_COUNT;
    while (RetryCount != 0) {
        RetryCount -= 1;
        Status = EfipSdSdSwitch(Controller,
                                SD_SWITCH_CHECK,
                                0,
                                1,
                                SwitchStatus);

        if (EFI_ERROR(Status)) {
            return Status;
        }

        //
        // Wait for the high speed status to become not busy.
        //

        if ((EfipSdByteSwap32(SwitchStatus[7]) &
             SD_SWITCH_STATUS_7_HIGH_SPEED_BUSY) == 0) {

            break;
        }
    }

    //
    // Don't worry about it if high speed isn't supported by either the card or
    // the host.
    //

    if ((EfipSdByteSwap32(SwitchStatus[3]) &
         SD_SWITCH_STATUS_3_HIGH_SPEED_SUPPORTED) == 0) {

        return EFI_SUCCESS;
    }

    if (((Controller->HostCapabilities & SD_MODE_HIGH_SPEED_52MHZ) == 0) &&
        ((Controller->HostCapabilities & SD_MODE_HIGH_SPEED) == 0)) {

        return EFI_SUCCESS;
    }

    Status = EfipSdSdSwitch(Controller, SD_SWITCH_SWITCH, 0, 1, SwitchStatus);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    if ((EfipSdByteSwap32(SwitchStatus[4]) &
         SD_SWITCH_STATUS_4_HIGH_SPEED_MASK) ==
        SD_SWITCH_STATUS_4_HIGH_SPEED_VALUE) {

        Controller->CardCapabilities |= SD_MODE_HIGH_SPEED;
    }

    return EFI_SUCCESS;
}

EFI_STATUS
EfipSdSetMmcFrequency (
    PEFI_SD_CONTROLLER Controller
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

    UINT8 CardData[SD_MMC_MAX_BLOCK_SIZE];
    UINT8 CardType;
    EFI_STATUS Status;

    Controller->CardCapabilities = 0;
    if ((Controller->HostCapabilities & SD_MODE_SPI) != 0) {
        return EFI_SUCCESS;
    }

    //
    // Only version 4 supports high speed.
    //

    if (Controller->Version < SdMmcVersion4) {
        return EFI_SUCCESS;
    }

    Status = EfipSdGetExtendedCardSpecificData(Controller, CardData);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    CardType = CardData[SD_MMC_EXTENDED_CARD_DATA_CARD_TYPE] &
               SD_MMC_EXTENDED_CARD_DATA_CARD_TYPE_MASK;

    Status = EfipSdMmcSwitch(Controller,
                             SD_MMC_EXTENDED_CARD_DATA_HIGH_SPEED,
                             1);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Get the extended card data again to see if it stuck.
    //

    Status = EfipSdGetExtendedCardSpecificData(Controller, CardData);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    if (CardData[SD_MMC_EXTENDED_CARD_DATA_HIGH_SPEED] == 0) {
        return EFI_SUCCESS;
    }

    Controller->CardCapabilities |= SD_MODE_HIGH_SPEED;
    if ((CardType & SD_MMC_CARD_TYPE_HIGH_SPEED_52MHZ) != 0) {
        Controller->CardCapabilities |= SD_MODE_HIGH_SPEED_52MHZ;
    }

    return EFI_SUCCESS;
}

EFI_STATUS
EfipSdSetBlockLength (
    PEFI_SD_CONTROLLER Controller,
    UINT32 BlockLength
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
    EFI_STATUS Status;

    EfiSetMem(&Command, sizeof(SD_COMMAND), 0);
    Command.Command = SdCommandSetBlockLength;
    Command.ResponseType = SD_RESPONSE_R1;
    Command.CommandArgument = BlockLength;
    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    return Status;
}

EFI_STATUS
EfipSdReadBlocksPolled (
    PEFI_SD_CONTROLLER Controller,
    UINT64 BlockOffset,
    UINT32 BlockCount,
    VOID *Buffer
    )

/*++

Routine Description:

    This routine performs a polled block I/O read.

Arguments:

    Controller - Supplies a pointer to the controller.

    BlockOffset - Supplies the logical block address of the I/O.

    BlockCount - Supplies the number of blocks to read or write.

    Buffer - Supplies the virtual address of the I/O buffer.

Return Value:

    Status code.

--*/

{

    SD_COMMAND Command;
    EFI_STATUS Status;

    if (BlockCount > 1) {
        Command.Command = SdCommandReadMultipleBlocks;

    } else {
        Command.Command = SdCommandReadSingleBlock;
    }

    Command.ResponseType = SD_RESPONSE_R1;
    if (Controller->HighCapacity != FALSE) {
        Command.CommandArgument = BlockOffset;

    } else {
        Command.CommandArgument = BlockOffset * Controller->ReadBlockLength;
    }

    Command.BufferSize = BlockCount * Controller->ReadBlockLength;
    Command.Buffer = Buffer;
    Command.Write = FALSE;
    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    if ((BlockCount > 1) &&
        ((Controller->HostCapabilities & SD_MODE_AUTO_CMD12) == 0)) {

        Command.Command = SdCommandStopTransmission;
        Command.CommandArgument = 0;
        Command.ResponseType = SD_RESPONSE_R1B;
        Command.BufferSize = 0;
        Status = Controller->FunctionTable.SendCommand(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    return Status;
}

EFI_STATUS
EfipSdWriteBlocksPolled (
    PEFI_SD_CONTROLLER Controller,
    UINT64 BlockOffset,
    UINT32 BlockCount,
    VOID *Buffer
    )

/*++

Routine Description:

    This routine performs a polled block I/O write.

Arguments:

    Controller - Supplies a pointer to the controller.

    BlockOffset - Supplies the logical block address of the I/O.

    BlockCount - Supplies the number of blocks to read or write.

    Buffer - Supplies the virtual address of the I/O buffer.

Return Value:

    Status code.

--*/

{

    SD_COMMAND Command;
    EFI_STATUS Status;

    if (BlockCount > 1) {
        Command.Command = SdCommandWriteMultipleBlocks;

    } else {
        Command.Command = SdCommandWriteSingleBlock;
    }

    Command.ResponseType = SD_RESPONSE_R1;
    if (Controller->HighCapacity != FALSE) {
        Command.CommandArgument = BlockOffset;

    } else {
        Command.CommandArgument = BlockOffset * Controller->ReadBlockLength;
    }

    Command.BufferSize = BlockCount * Controller->ReadBlockLength;
    Command.Buffer = Buffer;
    Command.Write = TRUE;
    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

    if (EFI_ERROR(Status)) {
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
        Status = Controller->FunctionTable.SendCommand(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    return Status;
}

EFI_STATUS
EfipSdErrorRecovery (
    PEFI_SD_CONTROLLER Controller
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

    EFI_STATUS Status;

    //
    // Perform an asychronous abort, which will clear any interrupts, abort the
    // command and reset the command and data lines. This will also wait until
    // the card has returned to the transfer state.
    //

    Status = EfipSdAsynchronousAbort(Controller);
    if (EFI_ERROR(Status)) {
        EfiDebugPrint("SD: Abort failed: %x\n", Status);
    }

    Status = EfiSdInitializeController(Controller, FALSE);
    if (EFI_ERROR(Status)) {
        EfiDebugPrint("SD: Reset controller failed: %x\n", Status);
    }

    return Status;
}

EFI_STATUS
EfipSdAsynchronousAbort (
    PEFI_SD_CONTROLLER Controller
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

    UINT32 CardStatus;
    SD_COMMAND Command;
    UINT32 ResetFlags;
    EFI_STATUS Status;
    UINT64 Time;
    UINT64 Timeout;

    //
    // Attempt to send the abort command until the card enters the transfer
    // state.
    //

    Time = 0;
    Timeout = EFI_SD_CONTROLLER_STATUS_TIMEOUT;
    EfiSetMem(&Command, sizeof(SD_COMMAND), 0);
    Command.Command = SdCommandStopTransmission;
    Command.ResponseType = SD_RESPONSE_NONE;
    while (TRUE) {
        Status = Controller->FunctionTable.SendCommand(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

        if (EFI_ERROR(Status)) {
            goto AsynchronousAbortEnd;
        }

        //
        // Reset the command and data lines.
        //

        ResetFlags = SD_RESET_FLAG_COMMAND_LINE | SD_RESET_FLAG_DATA_LINE;
        Status = Controller->FunctionTable.ResetController(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   ResetFlags);

        if (EFI_ERROR(Status)) {
            goto AsynchronousAbortEnd;
        }

        //
        // Check the SD card's status.
        //

        Status = EfipSdGetCardStatus(Controller, &CardStatus);
        if (EFI_ERROR(Status)) {
            goto AsynchronousAbortEnd;
        }

        //
        // Call it good if the card is ready for data and in the transfer state.
        //

        if (((CardStatus & SD_STATUS_READY_FOR_DATA) != 0) &&
            ((CardStatus & SD_STATUS_CURRENT_STATE) ==
            SD_STATUS_STATE_TRANSFER)) {

            Status = EFI_SUCCESS;
            break;
        }

        EfiStall(50);
        Time += 50;
        if (Time > Timeout) {
            Status = EFI_TIMEOUT;
            break;
        }
    }

AsynchronousAbortEnd:
    return Status;
}

UINT32
EfipSdByteSwap32 (
    UINT32 Input
    )

/*++

Routine Description:

    This routine performs a byte-swap of a 32-bit integer, effectively changing
    its endianness.

Arguments:

    Input - Supplies the integer to byte swap.

Return Value:

    Returns the byte-swapped integer.

--*/

{

    UINT32 Result;

    Result = (Input & 0x000000FF) << 24;
    Result |= (Input & 0x0000FF00) << 8;
    Result |= (Input & 0x00FF0000) >> 8;
    Result |= ((Input & 0xFF000000) >> 24) & 0x000000FF;
    return Result;
}

