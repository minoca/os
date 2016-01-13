/*++

Copyright (c) 2015 Minoca Corp. All rights reserved.

Module Name:

    sdrk.c

Abstract:

    This module implements the library functionality for the Rockchip SD/MMC
    device.

Author:

    Chris Stevens 16-Jul-2015

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "sdrkp.h"

//
// --------------------------------------------------------------------- Macros
//

//
// These macros read and write SD Rockchip controller registers.
//

#define SD_RK_READ_REGISTER(_Controller, _Register) \
    EfiReadRegister32((_Controller)->ControllerBase + (_Register))

#define SD_RK_WRITE_REGISTER(_Controller, _Register, _Value) \
    EfiWriteRegister32((_Controller)->ControllerBase + (_Register), (_Value))

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
EfipSdRkInitializeController (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    UINT32 Phase
    );

EFI_STATUS
EfipSdRkResetController (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    UINT32 Flags
    );

EFI_STATUS
EfipSdRkSendCommand (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    PSD_COMMAND Command
    );

EFI_STATUS
EfipSdRkGetSetBusWidth (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    UINT16 *BusWidth,
    BOOLEAN Set
    );

EFI_STATUS
EfipSdRkGetSetClockSpeed (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    UINT32 *ClockSpeed,
    BOOLEAN Set
    );

EFI_STATUS
EfipSdRkReadData (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    VOID *Data,
    UINT32 Size
    );

EFI_STATUS
EfipSdRkWriteData (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    VOID *Data,
    UINT32 Size
    );

EFI_STATUS
EfipSdRkHardResetController (
    PEFI_SD_RK_CONTROLLER RkController
    );

EFI_STATUS
EfipSdRkSetClockSpeed (
    PEFI_SD_RK_CONTROLLER RkController,
    UINT32 ClockSpeed
    );

//
// -------------------------------------------------------------------- Globals
//

SD_FUNCTION_TABLE EfiSdRkFunctionTable = {
    EfipSdRkInitializeController,
    EfipSdRkResetController,
    EfipSdRkSendCommand,
    EfipSdRkGetSetBusWidth,
    EfipSdRkGetSetClockSpeed
};

//
// ------------------------------------------------------------------ Functions
//

PEFI_SD_RK_CONTROLLER
EfiSdRkCreateController (
    PEFI_SD_RK_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine creates a new SD Rockchip controller object.

Arguments:

    Parameters - Supplies a pointer to the parameters to use when creating the
        controller. This can be stack allocated, as the Rockchip SD device
        won't use this memory after this routine returns.

Return Value:

    Returns a pointer to the controller structure on success.

    NULL on allocation failure or if a required parameter was not filled in.

--*/

{

    PEFI_SD_RK_CONTROLLER Controller;
    PEFI_SD_CONTROLLER SdController;
    EFI_SD_INITIALIZATION_BLOCK SdParameters;
    EFI_STATUS Status;

    if (Parameters->ControllerBase == NULL) {
        return NULL;
    }

    SdController = NULL;
    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_SD_RK_CONTROLLER),
                             (VOID **)&Controller);

    if (EFI_ERROR(Status)) {
        goto CreateControllerEnd;
    }

    EfiSetMem(Controller, sizeof(EFI_SD_RK_CONTROLLER), 0);
    Controller->ControllerBase = Parameters->ControllerBase;
    Controller->Voltages = Parameters->Voltages;
    Controller->HostCapabilities = Parameters->HostCapabilities;
    Controller->FundamentalClock = Parameters->FundamentalClock;

    //
    // Forward this call onto the core SD library for creation.
    //

    EfiSetMem(&SdParameters, sizeof(EFI_SD_INITIALIZATION_BLOCK), 0);
    SdParameters.ConsumerContext = Controller;
    SdParameters.OverrideFunctionTable = &EfiSdRkFunctionTable;
    SdParameters.Voltages = Parameters->Voltages;
    SdParameters.FundamentalClock = Parameters->FundamentalClock;
    SdParameters.HostCapabilities = Parameters->HostCapabilities;
    SdController = EfiSdCreateController(&SdParameters);
    if (SdController == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto CreateControllerEnd;
    }

    Controller->SdController = SdController;
    Status = EFI_SUCCESS;

CreateControllerEnd:
    if (EFI_ERROR(Status)) {
        if (SdController != NULL) {
            EfiSdDestroyController(SdController);
        }

        if (Controller != NULL) {
            EfiFreePool(Controller);
            Controller = NULL;
        }
    }

    return Controller;
}

VOID
EfiSdRkDestroyController (
    PEFI_SD_RK_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine destroys an SD Rockchip controller object.

Arguments:

    Controller - Supplies a pointer to the controller to destroy.

Return Value:

    None.

--*/

{

    EfiSdDestroyController(Controller->SdController);
    EfiFreePool(Controller);
    return;
}


EFI_STATUS
EfiSdRkInitializeController (
    PEFI_SD_RK_CONTROLLER Controller,
    BOOLEAN HardReset,
    BOOLEAN SoftReset
    )

/*++

Routine Description:

    This routine resets and initializes the SD Rockchip host controller.

Arguments:

    Controller - Supplies a pointer to the controller to initialize.

    HardReset - Supplies a boolean indicating whether or not to perform a
        hardware reset on the controller.

    SoftReset - Supplies a boolean indicating whether or not to perform a soft
        reset on the controller.

Return Value:

    Status code.

--*/

{

    EFI_STATUS Status;

    if (HardReset != FALSE) {
        Status = EfipSdRkHardResetController(Controller);
        if (EFI_ERROR(Status)) {
            goto InitializeControllerEnd;
        }
    }

    Status = EfiSdInitializeController(Controller->SdController, SoftReset);
    if (EFI_ERROR(Status)) {
        goto InitializeControllerEnd;
    }

InitializeControllerEnd:
    return Status;
}

EFI_STATUS
EfiSdRkBlockIoPolled (
    PEFI_SD_RK_CONTROLLER Controller,
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

    EFI_STATUS Status;

    Status = EfiSdBlockIoPolled(Controller->SdController,
                                BlockOffset,
                                BlockCount,
                                Buffer,
                                Write);

    return Status;
}

EFI_STATUS
EfiSdRkGetMediaParameters (
    PEFI_SD_RK_CONTROLLER Controller,
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

    EFI_STATUS Status;

    Status = EfiSdGetMediaParameters(Controller->SdController,
                                     BlockCount,
                                     BlockSize);

    return Status;
}

EFI_STATUS
EfipSdRkInitializeController (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    UINT32 Phase
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

    PEFI_SD_RK_CONTROLLER RkController;
    EFI_STATUS Status;
    UINT32 Value;
    UINT32 Voltage;

    RkController = (PEFI_SD_RK_CONTROLLER)Context;

    //
    // Phase 0 is an early initialization phase that happens after the
    // controller has been set. It is used to gather capabilities and set
    // certain parameters in the hardware.
    //

    if (Phase == 0) {

        //
        // Set the default burst length.
        //

        Value = (RK32_SD_BUS_MODE_BURST_LENGTH_16 <<
                 RK32_SD_BUS_MODE_BURST_LENGTH_SHIFT) |
                RK32_SD_BUS_MODE_FIXED_BURST;

        SD_RK_WRITE_REGISTER(RkController, Rk32SdBusMode, Value);

        //
        // Set the default FIFO threshold.
        //

        SD_RK_WRITE_REGISTER(RkController,
                             Rk32SdFifoThreshold,
                             RK32_SD_FIFO_THRESHOLD_DEFAULT);

        //
        // Set the default timeout.
        //

        SD_RK_WRITE_REGISTER(RkController,
                             Rk32SdTimeout,
                             RK32_SD_TIMEOUT_DEFAULT);

        //
        // Set the voltages based on the supported values supplied when the
        // controller was created.
        //

        Voltage = SD_RK_READ_REGISTER(RkController, Rk32SdUhs);
        Voltage &= ~RK32_SD_UHS_VOLTAGE_MASK;
        if ((RkController->Voltages & (SD_VOLTAGE_32_33 | SD_VOLTAGE_33_34)) ==
            (SD_VOLTAGE_32_33 | SD_VOLTAGE_33_34)) {

            Voltage |= RK32_SD_UHS_VOLTAGE_3V3;

        } else if ((RkController->Voltages & SD_VOLTAGE_165_195) ==
                   SD_VOLTAGE_165_195) {

            Voltage |= RK32_SD_UHS_VOLTAGE_1V8;

        } else {
            Status = EFI_DEVICE_ERROR;
            goto InitializeControllerEnd;
        }

        SD_RK_WRITE_REGISTER(RkController, Rk32SdUhs, Voltage);

    //
    // Phase 1 happens right before the initialization command sequence is
    // about to begin. The clock and bus width have been program and the device
    // is just about read to go.
    //

    } else if (Phase == 1) {

        //
        // Turn on the power.
        //

        SD_RK_WRITE_REGISTER(RkController, Rk32SdPower, RK32_SD_POWER_ENABLE);

        //
        // Set the interrupt mask, clear any pending state, and enable the
        // interrupts.
        //

        SD_RK_WRITE_REGISTER(RkController,
                             Rk32SdInterruptMask,
                             RK32_SD_INTERRUPT_MASK_DEFAULT);

        SD_RK_WRITE_REGISTER(RkController,
                             Rk32SdInterruptStatus,
                             RK32_SD_INTERRUPT_STATUS_ALL_MASK);

        Value = SD_RK_READ_REGISTER(RkController, Rk32SdControl);
        Value |= RK32_SD_CONTROL_INTERRUPT_ENABLE;
        SD_RK_WRITE_REGISTER(RkController, Rk32SdControl, Value);
    }

    Status = EFI_SUCCESS;

InitializeControllerEnd:
    return Status;
}

EFI_STATUS
EfipSdRkResetController (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    UINT32 Flags
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

    UINT32 ResetMask;
    PEFI_SD_RK_CONTROLLER RkController;
    EFI_STATUS Status;
    UINT64 Time;
    UINT64 Timeout;
    UINT32 Value;

    //
    // Always reset the DMA and FIFO.
    //

    RkController = (PEFI_SD_RK_CONTROLLER)Context;
    ResetMask = RK32_SD_CONTROL_FIFO_RESET | RK32_SD_CONTROL_DMA_RESET;
    SD_RK_WRITE_REGISTER(RkController, Rk32SdControl, ResetMask);
    Status = EFI_TIMEOUT;
    Timeout = EFI_SD_RK_CONTROLLER_TIMEOUT;
    Time = 0;
    do {
        Value = SD_RK_READ_REGISTER(RkController, Rk32SdControl);
        if ((Value & ResetMask) == 0) {
            Status = EFI_SUCCESS;
            break;
        }

        EfiStall(50);
        Time += 50;

    } while (Time <= Timeout);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Don't go any further unless a full software reset was requested.
    //

    if ((Flags & SD_RESET_FLAG_ALL) == 0) {
        return EFI_SUCCESS;
    }

    Value = SD_RK_READ_REGISTER(RkController, Rk32SdBusMode);
    Value |= RK32_SD_BUS_MODE_SOFTWARE_RESET;
    SD_RK_WRITE_REGISTER(RkController, Rk32SdBusMode, Value);
    Status = EFI_TIMEOUT;
    Timeout = EFI_SD_RK_CONTROLLER_TIMEOUT;
    Time = 0;
    do {
        Value = SD_RK_READ_REGISTER(RkController, Rk32SdBusMode);
        if ((Value & RK32_SD_BUS_MODE_SOFTWARE_RESET) == 0) {
            Status = EFI_SUCCESS;
            break;
        }

        EfiStall(50);
        Time += 50;

    } while (Time < Timeout);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS
EfipSdRkSendCommand (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
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

    UINT32 CommandValue;
    UINT32 Flags;
    PEFI_SD_RK_CONTROLLER RkController;
    EFI_STATUS Status;
    UINT64 Time;
    UINT64 Timeout;
    UINT32 Value;

    RkController = (PEFI_SD_RK_CONTROLLER)Context;

    //
    // Wait for the last command to complete.
    //

    Value = SD_RK_READ_REGISTER(RkController, Rk32SdStatus);
    if ((Value & RK32_SD_STATUS_FIFO_EMPTY) == 0) {
        Value = SD_RK_READ_REGISTER(RkController, Rk32SdControl);
        Value |= RK32_SD_CONTROL_FIFO_RESET;
        SD_RK_WRITE_REGISTER(RkController, Rk32SdControl, Value);
        Timeout = EFI_SD_RK_CONTROLLER_TIMEOUT;
        Time = 0;
        Status = EFI_TIMEOUT;
        do {
            Value = SD_RK_READ_REGISTER(RkController, Rk32SdControl);
            if ((Value & RK32_SD_CONTROL_FIFO_RESET) == 0) {
                Status = EFI_SUCCESS;
                break;
            }

            EfiStall(50);
            Time += 50;

        } while (Time <= Timeout);

        if (EFI_ERROR(Status)) {
            goto SendCommandEnd;
        }
    }

    //
    // Clear any old interrupt status.
    //

    SD_RK_WRITE_REGISTER(RkController,
                         Rk32SdInterruptStatus,
                         RK32_SD_INTERRUPT_STATUS_ALL_MASK);

    //
    // Set up the response flags.
    //

    Flags = RK32_SD_COMMAND_WAIT_PREVIOUS_DATA_COMPLETE;
    if ((Command->ResponseType & SD_RESPONSE_PRESENT) != 0) {
        if ((Command->ResponseType & SD_RESPONSE_136_BIT) != 0) {
            Flags |= RK32_SD_COMMAND_LONG_RESPONSE;
        }

        Flags |= RK32_SD_COMMAND_RESPONSE_EXPECTED;
    }

    //
    // Set up the remainder of the command flags.
    //

    if ((Command->ResponseType & SD_RESPONSE_VALID_CRC) != 0) {
        Flags |= RK32_SD_COMMAND_CHECK_RESPONSE_CRC;
    }

    //
    // If there's a data buffer, program the block count.
    //

    if (Command->BufferSize != 0) {
        Flags |= RK32_SD_COMMAND_DATA_EXPECTED;
        if (Command->Write != FALSE) {
            Flags |= RK32_SD_COMMAND_WRITE;

        } else {
            Flags |= RK32_SD_COMMAND_READ;
        }

        //
        // If reading or writing multiple blocks, the block size register
        // should be set to the default block size and the byte count should be
        // a multiple of the block size.
        //

        if ((Command->Command == SdCommandReadMultipleBlocks) ||
            (Command->Command == SdCommandWriteMultipleBlocks)) {

            if ((RkController->HostCapabilities & SD_MODE_AUTO_CMD12) != 0) {
                Flags |= RK32_SD_COMMAND_SEND_AUTO_STOP;
            }

            SD_RK_WRITE_REGISTER(RkController,
                                 Rk32SdBlockSize,
                                 SD_RK_BLOCK_SIZE);

            SD_RK_WRITE_REGISTER(RkController,
                                 Rk32SdByteCount,
                                 Command->BufferSize);

        //
        // Otherwise set the block size to total number of bytes to be
        // processed.
        //

        } else {
            SD_RK_WRITE_REGISTER(RkController,
                                 Rk32SdBlockSize,
                                 Command->BufferSize);

            SD_RK_WRITE_REGISTER(RkController,
                                 Rk32SdByteCount,
                                 Command->BufferSize);
        }
    }

    //
    // Write the command argument.
    //

    SD_RK_WRITE_REGISTER(RkController,
                         Rk32SdCommandArgument,
                         Command->CommandArgument);

    //
    // Set the command and wait for it to be accepted.
    //

    CommandValue = (Command->Command << RK32_SD_COMMAND_INDEX_SHIFT) &
                   RK32_SD_COMMAND_INDEX_MASK;

    CommandValue |= RK32_SD_COMMAND_START |
                    RK32_SD_COMMAND_USE_HOLD_REGISTER |
                    Flags;

    SD_RK_WRITE_REGISTER(RkController, Rk32SdCommand, CommandValue);
    Timeout = EFI_SD_RK_CONTROLLER_TIMEOUT;
    Time = 0;
    Status = EFI_TIMEOUT;
    do {
        Value = SD_RK_READ_REGISTER(RkController, Rk32SdCommand);
        if ((Value & RK32_SD_COMMAND_START) == 0) {
            Status = EFI_SUCCESS;
            break;
        }

        EfiStall(50);
        Time += 50;

    } while (Time <= Timeout);

    if (EFI_ERROR(Status)) {
        goto SendCommandEnd;
    }

    //
    // Check the interrupt status.
    //

    Timeout = EFI_SD_RK_CONTROLLER_TIMEOUT;
    Time = 0;
    Status = EFI_TIMEOUT;
    do {
        Value = SD_RK_READ_REGISTER(RkController, Rk32SdInterruptStatus);
        if ((Value & RK32_SD_INTERRUPT_STATUS_COMMAND_DONE) != 0) {
            Status = EFI_SUCCESS;
            break;
        }

        EfiStall(50);
        Time += 50;

    } while (Time <= Timeout);

    if (EFI_ERROR(Status)) {
        goto SendCommandEnd;
    }

    if ((Value & RK32_SD_INTERRUPT_STATUS_ERROR_RESPONSE_TIMEOUT) != 0) {
        SD_RK_WRITE_REGISTER(RkController,
                             Rk32SdInterruptStatus,
                             RK32_SD_INTERRUPT_STATUS_ALL_MASK);

        EfipSdRkResetController(Controller,
                                RkController,
                                SD_RESET_FLAG_COMMAND_LINE);

        Status = EFI_TIMEOUT;
        goto SendCommandEnd;

    } else if ((Value & RK32_SD_INTERRUPT_STATUS_COMMAND_ERROR_MASK) != 0) {
        SD_RK_WRITE_REGISTER(RkController,
                             Rk32SdInterruptStatus,
                             RK32_SD_INTERRUPT_STATUS_ALL_MASK);

        Status = EFI_DEVICE_ERROR;
        goto SendCommandEnd;
    }

    //
    // Acknowledge the completed command.
    //

    SD_RK_WRITE_REGISTER(RkController,
                         Rk32SdInterruptStatus,
                         RK32_SD_INTERRUPT_STATUS_COMMAND_DONE);

    //
    // Get the response if there is one.
    //

    if ((Command->ResponseType & SD_RESPONSE_PRESENT) != 0) {
        if ((Command->ResponseType & SD_RESPONSE_136_BIT) != 0) {
            Command->Response[3] = SD_RK_READ_REGISTER(RkController,
                                                       Rk32SdResponse0);

            Command->Response[2] = SD_RK_READ_REGISTER(RkController,
                                                       Rk32SdResponse1);

            Command->Response[1] = SD_RK_READ_REGISTER(RkController,
                                                       Rk32SdResponse2);

            Command->Response[0] = SD_RK_READ_REGISTER(RkController,
                                                       Rk32SdResponse3);

            if ((RkController->HostCapabilities &
                 SD_MODE_RESPONSE136_SHIFTED) != 0) {

                Command->Response[0] = (Command->Response[0] << 8) |
                                       ((Command->Response[1] >> 24) & 0xFF);

                Command->Response[1] = (Command->Response[1] << 8) |
                                       ((Command->Response[2] >> 24) & 0xFF);

                Command->Response[2] = (Command->Response[2] << 8) |
                                       ((Command->Response[3] >> 24) & 0xFF);

                Command->Response[3] = Command->Response[3] << 8;
            }

        } else {
            Command->Response[0] = SD_RK_READ_REGISTER(RkController,
                                                       Rk32SdResponse0);
        }
    }

    //
    // Read/write the data.
    //

    if (Command->BufferSize != 0) {
        if (Command->Write != FALSE) {
            Status = EfipSdRkWriteData(Controller,
                                       Context,
                                       Command->Buffer,
                                       Command->BufferSize);

        } else {
            Status = EfipSdRkReadData(Controller,
                                      Context,
                                      Command->Buffer,
                                      Command->BufferSize);
        }

        if (EFI_ERROR(Status)) {
            goto SendCommandEnd;
        }
    }

    Status = EFI_SUCCESS;

SendCommandEnd:
    return Status;
}

EFI_STATUS
EfipSdRkGetSetBusWidth (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    UINT16 *BusWidth,
    BOOLEAN Set
    )

/*++

Routine Description:

    This routine gets or sets the controller's bus width.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    BusWidth - Supplies a pointer that receives bus width information on get
        and contains bus width information on set.

    Set - Supplies a boolean indicating whether the bus width should be queried
        or set.

Return Value:

    Status code.

--*/

{

    PEFI_SD_RK_CONTROLLER RkController;
    UINT32 Value;

    RkController = (PEFI_SD_RK_CONTROLLER)Context;
    if (Set != FALSE) {
        switch (*BusWidth) {
        case 1:
            Value = RK32_SD_CARD_TYPE_1_BIT_WIDTH;
            break;

        case 4:
            Value = RK32_SD_CARD_TYPE_4_BIT_WIDTH;
            break;

        case 8:
            Value = RK32_SD_CARD_TYPE_8_BIT_WIDTH;
            break;

        default:
            return EFI_INVALID_PARAMETER;
        }

        SD_RK_WRITE_REGISTER(RkController, Rk32SdCardType, Value);

    } else {
        Value = SD_RK_READ_REGISTER(RkController, Rk32SdCardType);
        if ((Value & RK32_SD_CARD_TYPE_8_BIT_WIDTH) != 0) {
            *BusWidth = 8;

        } else if ((Value & RK32_SD_CARD_TYPE_4_BIT_WIDTH) != 0) {
            *BusWidth = 4;

        } else {
            *BusWidth = 1;
        }
    }

    return EFI_SUCCESS;
}

EFI_STATUS
EfipSdRkGetSetClockSpeed (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    UINT32 *ClockSpeed,
    BOOLEAN Set
    )

/*++

Routine Description:

    This routine gets or sets the controller's clock speed.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    ClockSpeed - Supplies a pointer that receives the current clock speed on
        get and contains the desired clock speed on set.

    Set - Supplies a boolean indicating whether the bus width should be queried
        or set.

Return Value:

    Status code.

--*/

{

    PEFI_SD_RK_CONTROLLER RkController;

    RkController = (PEFI_SD_RK_CONTROLLER)Context;
    if (RkController->FundamentalClock == 0) {
        return EFI_INVALID_PARAMETER;
    }

    //
    // Getting the clock speed is not implemented as the divisor math might not
    // work out precisely in reverse.
    //

    if (Set == FALSE) {
        return EFI_UNSUPPORTED;
    }

    return EfipSdRkSetClockSpeed(RkController, *ClockSpeed);
}

//
// --------------------------------------------------------- Internal Functions
//

EFI_STATUS
EfipSdRkReadData (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    VOID *Data,
    UINT32 Size
    )

/*++

Routine Description:

    This routine reads polled data from the SD controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Data - Supplies a pointer to the buffer where the data will be read into.

    Size - Supplies the size in bytes. This must be a multiple of four bytes.

Return Value:

    Status code.

--*/

{

    UINT32 *Buffer32;
    UINT32 BusyMask;
    UINT32 Count;
    UINT32 DataReadyMask;
    BOOLEAN DataTransferOver;
    UINT32 Interrupts;
    UINT32 IoIndex;
    PEFI_SD_RK_CONTROLLER RkController;
    EFI_STATUS Status;
    UINT64 Time;
    UINT64 Timeout;
    UINT32 Value;

    RkController = (PEFI_SD_RK_CONTROLLER)Context;
    DataTransferOver = FALSE;
    Buffer32 = (UINT32 *)Data;
    Size /= sizeof(UINT32);
    while (Size != 0) {

        //
        // Get the interrupt status register.
        //

        Time = 0;
        Timeout = EFI_SD_RK_CONTROLLER_TIMEOUT;
        Status = EFI_TIMEOUT;
        do {
            Interrupts = SD_RK_READ_REGISTER(RkController,
                                             Rk32SdInterruptStatus);

            if (Interrupts != 0) {
                Status = EFI_SUCCESS;
                break;
            }

            EfiStall(50);
            Time += 50;

        } while (Time <= Timeout);

        if (EFI_ERROR(Status)) {
            return Status;
        }

        //
        // Reset the controller if any error bits are set.
        //

        if ((Interrupts & RK32_SD_INTERRUPT_STATUS_DATA_ERROR_MASK) != 0) {
            EfipSdRkResetController(Controller,
                                    Context,
                                    SD_RESET_FLAG_DATA_LINE);

            return EFI_DEVICE_ERROR;
        }

        //
        // Check for received data status. If data is ready, the status
        // register holds the number of 32-bit elements to be read.
        //

        DataReadyMask = RK32_SD_INTERRUPT_STATUS_RECEIVE_FIFO_DATA_REQUEST;
        if ((Interrupts & DataReadyMask) != 0) {
            Count = SD_RK_READ_REGISTER(RkController, Rk32SdStatus);
            Count = (Count & RK32_SD_STATUS_FIFO_COUNT_MASK) >>
                    RK32_SD_STATUS_FIFO_COUNT_SHIFT;

            for (IoIndex = 0; IoIndex < Count; IoIndex += 1) {
                *Buffer32 = SD_RK_READ_REGISTER(RkController, Rk32SdFifoBase);
                Buffer32 += 1;
            }

            Size -= Count;
            SD_RK_WRITE_REGISTER(RkController,
                                 Rk32SdInterruptStatus,
                                 DataReadyMask);
        }

        //
        // Check for the transfer over bit. If it is set, then read the rest of
        // the bytes from the FIFO.
        //

        if ((Interrupts & RK32_SD_INTERRUPT_STATUS_DATA_TRANSFER_OVER) != 0) {
            for (IoIndex = 0; IoIndex < Size; IoIndex += 1) {
                *Buffer32 = SD_RK_READ_REGISTER(RkController, Rk32SdFifoBase);
                Buffer32 += 1;
            }

            SD_RK_WRITE_REGISTER(RkController,
                                 Rk32SdInterruptStatus,
                                 RK32_SD_INTERRUPT_STATUS_DATA_TRANSFER_OVER);

            Size = 0;
            DataTransferOver = TRUE;
            break;
        }
    }

    //
    // If the data transfer over interrupt has not yet been seen, wait for it
    // to be asserted.
    //

    if (DataTransferOver == FALSE) {
        Time = 0;
        Timeout = EFI_SD_RK_CONTROLLER_TIMEOUT;
        Status = EFI_TIMEOUT;
        do {
            Interrupts = SD_RK_READ_REGISTER(RkController,
                                             Rk32SdInterruptStatus);

            if ((Interrupts &
                 RK32_SD_INTERRUPT_STATUS_DATA_TRANSFER_OVER) != 0) {

                Status = EFI_SUCCESS;
                break;
            }

            EfiStall(50);
            Time += 50;

        } while (Time <= Timeout);

        if (EFI_ERROR(Status)) {
            return Status;
        }

        SD_RK_WRITE_REGISTER(RkController,
                             Rk32SdInterruptStatus,
                             RK32_SD_INTERRUPT_STATUS_DATA_TRANSFER_OVER);
    }

    //
    // Wait until the state machine and data stop being busy.
    //

    Timeout = EFI_SD_RK_CONTROLLER_TIMEOUT;
    Time = 0;
    Status = EFI_TIMEOUT;
    BusyMask = RK32_SD_STATUS_DATA_STATE_MACHINE_BUSY |
               RK32_SD_STATUS_DATA_BUSY;

    do {
        Value = SD_RK_READ_REGISTER(RkController, Rk32SdStatus);
        if ((Value & BusyMask) == 0) {
            Status = EFI_SUCCESS;
            break;
        }

        EfiStall(50);
        Time += 50;

    } while (Time <= Timeout);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS
EfipSdRkWriteData (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    VOID *Data,
    UINT32 Size
    )

/*++

Routine Description:

    This routine writes polled data to the SD controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Data - Supplies a pointer to the buffer containing the data to write.

    Size - Supplies the size in bytes. This must be a multiple of 4 bytes.

Return Value:

    Status code.

--*/

{

    UINT32 *Buffer32;
    UINT32 BusyMask;
    UINT32 Count;
    UINT32 DataRequestMask;
    BOOLEAN DataTransferOver;
    UINT32 Interrupts;
    UINT32 IoIndex;
    PEFI_SD_RK_CONTROLLER RkController;
    EFI_STATUS Status;
    UINT64 Time;
    UINT64 Timeout;
    UINT32 Value;

    RkController = (PEFI_SD_RK_CONTROLLER)Context;
    DataTransferOver = FALSE;
    Buffer32 = (UINT32 *)Data;
    Size /= sizeof(UINT32);
    while (Size != 0) {

        //
        // Get the interrupt status register.
        //

        Time = 0;
        Timeout = EFI_SD_RK_CONTROLLER_TIMEOUT;
        Status = EFI_TIMEOUT;
        do {
            Interrupts = SD_RK_READ_REGISTER(RkController,
                                             Rk32SdInterruptStatus);

            if (Interrupts != 0) {
                Status = EFI_SUCCESS;
                break;
            }

            EfiStall(50);
            Time += 50;

        } while (Time <= Timeout);

        if (EFI_ERROR(Status)) {
            return Status;
        }

        //
        // Reset the controller if any error bits are set.
        //

        if ((Interrupts & RK32_SD_INTERRUPT_STATUS_DATA_ERROR_MASK) != 0) {
            EfipSdRkResetController(Controller,
                                    Context,
                                    SD_RESET_FLAG_DATA_LINE);

            return EFI_DEVICE_ERROR;
        }

        //
        // If the controller is ready for data to be written, the number of
        // 4-byte elements consumed in the FIFO is stored in the status
        // register. The available bytes is the total FIFO size minus that
        // amount.
        //

        DataRequestMask = RK32_SD_INTERRUPT_STATUS_TRANSMIT_FIFO_DATA_REQUEST;
        if ((Interrupts & DataRequestMask) != 0) {
            Count = SD_RK_READ_REGISTER(RkController, Rk32SdStatus);
            Count = (Count & RK32_SD_STATUS_FIFO_COUNT_MASK) >>
                    RK32_SD_STATUS_FIFO_COUNT_SHIFT;

            Count = (RK32_SD_FIFO_DEPTH / sizeof(UINT32)) - Count;
            for (IoIndex = 0; IoIndex < Count; IoIndex += 1) {
                SD_RK_WRITE_REGISTER(RkController, Rk32SdFifoBase, *Buffer32);
                Buffer32 += 1;
            }

            Size -= Count;
            SD_RK_WRITE_REGISTER(RkController,
                                 Rk32SdInterruptStatus,
                                 DataRequestMask);
        }

        //
        // Check for the transfer over bit. If it is set, then exit.
        //

        if ((Interrupts & RK32_SD_INTERRUPT_STATUS_DATA_TRANSFER_OVER) != 0) {
            SD_RK_WRITE_REGISTER(RkController,
                                 Rk32SdInterruptStatus,
                                 RK32_SD_INTERRUPT_STATUS_DATA_TRANSFER_OVER);

            Size = 0;
            DataTransferOver = TRUE;
            break;
        }
    }

    //
    // If the data transfer over interrupt has not yet been seen, wait for it
    // to be asserted.
    //

    if (DataTransferOver == FALSE) {
        Timeout = EFI_SD_RK_CONTROLLER_TIMEOUT;
        Time = 0;
        Status = EFI_TIMEOUT;
        do {
            Interrupts = SD_RK_READ_REGISTER(RkController,
                                             Rk32SdInterruptStatus);

            if ((Interrupts &
                 RK32_SD_INTERRUPT_STATUS_DATA_TRANSFER_OVER) != 0) {

                Status = EFI_SUCCESS;
                break;
            }

            EfiStall(50);
            Time += 50;

        } while (Time <= Timeout);

        if (EFI_ERROR(Status)) {
            return Status;
        }

        SD_RK_WRITE_REGISTER(RkController,
                             Rk32SdInterruptStatus,
                             RK32_SD_INTERRUPT_STATUS_DATA_TRANSFER_OVER);
    }

    //
    // Wait until the state machine and data stop being busy.
    //

    Timeout = EFI_SD_RK_CONTROLLER_TIMEOUT;
    Time = 0;
    Status = EFI_TIMEOUT;
    BusyMask = RK32_SD_STATUS_DATA_STATE_MACHINE_BUSY |
               RK32_SD_STATUS_DATA_BUSY;

    do {
        Value = SD_RK_READ_REGISTER(RkController, Rk32SdStatus);
        if ((Value & BusyMask) == 0) {
            Status = EFI_SUCCESS;
            break;
        }

        EfiStall(50);
        Time += 50;

    } while (Time <= Timeout);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS
EfipSdRkHardResetController (
    PEFI_SD_RK_CONTROLLER RkController
    )

/*++

Routine Description:

    This routine resets the RK SD controller and card.

Arguments:

    RkController - Supplies a pointer to this SD RK32 device.

Return Value:

    Status code.

--*/

{

    VOID *CruBase;
    VOID *GrfBase;
    UINT32 ResetMask;
    EFI_STATUS Status;
    UINT64 Time;
    UINT64 Timeout;
    UINT32 Value;

    //
    // First perform a hardware reset on the SD card.
    //

    SD_RK_WRITE_REGISTER(RkController, Rk32SdPower, RK32_SD_POWER_DISABLE);
    SD_RK_WRITE_REGISTER(RkController, Rk32SdResetN, RK32_SD_RESET_ENABLE);
    EfiStall(5000);
    SD_RK_WRITE_REGISTER(RkController, Rk32SdPower, RK32_SD_POWER_ENABLE);
    SD_RK_WRITE_REGISTER(RkController, Rk32SdResetN, 0);
    EfiStall(1000);

    //
    // Reset the SD/MMC.
    //

    CruBase = (VOID *)RK32_CRU_BASE;
    Value = RK32_CRU_SOFT_RESET8_MMC0 << RK32_CRU_SOFT_RESET8_PROTECT_SHIFT;
    Value |= RK32_CRU_SOFT_RESET8_MMC0;
    EfiWriteRegister32(CruBase + Rk32CruSoftReset8, Value);
    EfiStall(100);
    Value &= ~RK32_CRU_SOFT_RESET8_MMC0;
    EfiWriteRegister32(CruBase + Rk32CruSoftReset8, Value);

    //
    // Reset the IOMUX to the correct value for SD/MMC.
    //

    GrfBase = (VOID *)RK32_GRF_BASE;
    Value = RK32_GRF_GPIO6C_IOMUX_VALUE;
    EfiWriteRegister32(GrfBase + Rk32GrfGpio6cIomux, Value);

    //
    // Perform a complete controller reset and wait for it to complete.
    //

    ResetMask = RK32_SD_CONTROL_FIFO_RESET |
                RK32_SD_CONTROL_CONTROLLER_RESET;

    SD_RK_WRITE_REGISTER(RkController, Rk32SdControl, ResetMask);
    Status = EFI_TIMEOUT;
    Timeout = EFI_SD_RK_CONTROLLER_TIMEOUT;
    Time = 0;
    do {
        Value = SD_RK_READ_REGISTER(RkController, Rk32SdControl);
        if ((Value & ResetMask) == 0) {
            Status = EFI_SUCCESS;
            break;
        }

        EfiStall(50);
        Time += 50;

    } while (Time <= Timeout);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Clear interrupts.
    //

    SD_RK_WRITE_REGISTER(RkController,
                         Rk32SdInterruptStatus,
                         RK32_SD_INTERRUPT_STATUS_ALL_MASK);

    //
    // Set 3v3 volts in the UHS register.
    //

    SD_RK_WRITE_REGISTER(RkController, Rk32SdUhs, RK32_SD_UHS_VOLTAGE_3V3);

    //
    // Set the clock to 400kHz in preparation for sending CMD0 with the
    // initialization bit set.
    //

    Status = EfipSdRkSetClockSpeed(RkController, 400000);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Reset the card by sending the CMD0 reset command with the initialization
    // bit set.
    //

    Value = RK32_SD_COMMAND_START |
            RK32_SD_COMMAND_USE_HOLD_REGISTER |
            RK32_SD_COMMAND_SEND_INITIALIZATION;

    SD_RK_WRITE_REGISTER(RkController, Rk32SdCommand, Value);

    //
    // Wait for the command to complete.
    //

    Status = EFI_TIMEOUT;
    Timeout = EFI_SD_RK_CONTROLLER_TIMEOUT;
    Time = 0;
    do {
        Value = SD_RK_READ_REGISTER(RkController, Rk32SdCommand);
        if ((Value & RK32_SD_COMMAND_START) == 0) {
            Status = EFI_SUCCESS;
            break;
        }

        EfiStall(50);
        Time += 50;

    } while (Time <= Timeout);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Timeout = EFI_SD_RK_CONTROLLER_TIMEOUT;
    Time = 0;
    Status = EFI_TIMEOUT;
    do {
        Value = SD_RK_READ_REGISTER(RkController, Rk32SdInterruptStatus);
        if (Value != 0) {
            if ((Value & RK32_SD_INTERRUPT_STATUS_COMMAND_DONE) != 0) {
                Status = EFI_SUCCESS;

            } else if ((Value &
                        RK32_SD_INTERRUPT_STATUS_ERROR_RESPONSE_TIMEOUT) != 0) {

                Status = EFI_NO_MEDIA;

            } else {
                Status = EFI_DEVICE_ERROR;
            }

            SD_RK_WRITE_REGISTER(RkController, Rk32SdInterruptStatus, Value);
            break;
        }

        EfiStall(50);
        Time += 50;

    } while (Time <= Timeout);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS
EfipSdRkSetClockSpeed (
    PEFI_SD_RK_CONTROLLER RkController,
    UINT32 ClockSpeed
    )

/*++

Routine Description:

    This routine sets the controller's clock speed.

Arguments:

    RkController - Supplies a pointer to this SD Rockchip controller.

    ClockSpeed - Supplies the desired clock speed in Hertz.

Return Value:

    Status code.

--*/

{

    UINT32 Divisor;
    EFI_STATUS Status;
    UINT64 Time;
    UINT64 Timeout;
    UINT32 Value;

    if (RkController->FundamentalClock == 0) {
        return EFI_INVALID_PARAMETER;
    }

    //
    // Wait for the card to not be busy.
    //

    Timeout = EFI_SD_RK_CONTROLLER_TIMEOUT;
    Time = 0;
    Status = EFI_TIMEOUT;
    do {
        Value = SD_RK_READ_REGISTER(RkController, Rk32SdStatus);
        if ((Value & RK32_SD_STATUS_DATA_BUSY) == 0) {
            Status = EFI_SUCCESS;
            break;
        }

        EfiStall(50);
        Time += 50;

    } while (Time <= Timeout);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Disable all clocks.
    //

    SD_RK_WRITE_REGISTER(RkController, Rk32SdClockEnable, 0);

    //
    // Send the command to indicate that the clock enable register is being
    // updated.
    //

    Value = RK32_SD_COMMAND_START |
            RK32_SD_COMMAND_UPDATE_CLOCK_REGISTERS |
            RK32_SD_COMMAND_WAIT_PREVIOUS_DATA_COMPLETE;

    SD_RK_WRITE_REGISTER(RkController, Rk32SdCommand, Value);
    Timeout = EFI_SD_RK_CONTROLLER_TIMEOUT;
    Time = 0;
    Status = EFI_TIMEOUT;
    do {
        Value = SD_RK_READ_REGISTER(RkController, Rk32SdCommand);
        if ((Value & RK32_SD_COMMAND_START) == 0) {
            Status = EFI_SUCCESS;
            break;
        }

        EfiStall(50);
        Time += 50;

    } while (Time <= Timeout);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Get the appropriate divisor without going over the desired clock speed.
    //

    if (ClockSpeed >= RkController->FundamentalClock) {
        Divisor = 0;

    } else {
        Divisor = 2;
        while (Divisor < RK32_SD_MAX_DIVISOR) {
            if ((RkController->FundamentalClock / Divisor) <= ClockSpeed) {
                break;
            }

            Divisor += 2;
        }

        Divisor >>= 1;
    }

    SD_RK_WRITE_REGISTER(RkController, Rk32SdClockDivider, Divisor);
    SD_RK_WRITE_REGISTER(RkController,
                         Rk32SdClockSource,
                         RK32_SD_CLOCK_SOURCE_DIVIDER_0);

    //
    // Send the command to indicate that the clock source and divider are is
    // being updated.
    //

    Value = RK32_SD_COMMAND_START |
            RK32_SD_COMMAND_UPDATE_CLOCK_REGISTERS |
            RK32_SD_COMMAND_WAIT_PREVIOUS_DATA_COMPLETE;

    SD_RK_WRITE_REGISTER(RkController, Rk32SdCommand, Value);
    Timeout = EFI_SD_RK_CONTROLLER_TIMEOUT;
    Time = 0;
    Status = EFI_TIMEOUT;
    do {
        Value = SD_RK_READ_REGISTER(RkController, Rk32SdCommand);
        if ((Value & RK32_SD_COMMAND_START) == 0) {
            Status = EFI_SUCCESS;
            break;
        }

        EfiStall(50);
        Time += 50;

    } while (Time <= Timeout);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Enable the clocks in lower power mode.
    //

    SD_RK_WRITE_REGISTER(RkController,
                         Rk32SdClockEnable,
                         (RK32_SD_CLOCK_ENABLE_LOW_POWER |
                          RK32_SD_CLOCK_ENABLE_ON));

    //
    // Send the command to indicate that the clock is enable register being
    // updated.
    //

    Value = RK32_SD_COMMAND_START |
            RK32_SD_COMMAND_UPDATE_CLOCK_REGISTERS |
            RK32_SD_COMMAND_WAIT_PREVIOUS_DATA_COMPLETE;

    SD_RK_WRITE_REGISTER(RkController, Rk32SdCommand, Value);
    Timeout = EFI_SD_RK_CONTROLLER_TIMEOUT;
    Time = 0;
    Status = EFI_TIMEOUT;
    do {
        Value = SD_RK_READ_REGISTER(RkController, Rk32SdCommand);
        if ((Value & RK32_SD_COMMAND_START) == 0) {
            Status = EFI_SUCCESS;
            break;
        }

        EfiStall(50);
        Time += 50;

    } while (Time <= Timeout);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

