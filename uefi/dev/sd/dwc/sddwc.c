/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sddwc.c

Abstract:

    This module implements the library functionality for the DesignWare SD/MMC
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
#include <dev/sddwc.h>

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
EfipSdDwcInitializeController (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    UINT32 Phase
    );

EFI_STATUS
EfipSdDwcResetController (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    UINT32 Flags
    );

EFI_STATUS
EfipSdDwcSendCommand (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    PSD_COMMAND Command
    );

EFI_STATUS
EfipSdDwcGetSetBusWidth (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    UINT16 *BusWidth,
    BOOLEAN Set
    );

EFI_STATUS
EfipSdDwcGetSetClockSpeed (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    UINT32 *ClockSpeed,
    BOOLEAN Set
    );

EFI_STATUS
EfipSdDwcReadData (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    VOID *Data,
    UINT32 Size
    );

EFI_STATUS
EfipSdDwcWriteData (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    VOID *Data,
    UINT32 Size
    );

//
// -------------------------------------------------------------------- Globals
//

SD_FUNCTION_TABLE EfiSdDwcFunctionTable = {
    EfipSdDwcInitializeController,
    EfipSdDwcResetController,
    EfipSdDwcSendCommand,
    EfipSdDwcGetSetBusWidth,
    EfipSdDwcGetSetClockSpeed
};

//
// ------------------------------------------------------------------ Functions
//

PEFI_SD_DWC_CONTROLLER
EfiSdDwcCreateController (
    PEFI_SD_DWC_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine creates a new DesignWare SD controller object.

Arguments:

    Parameters - Supplies a pointer to the parameters to use when creating the
        controller. This can be stack allocated, as the DesignWare SD device
        won't use this memory after this routine returns.

Return Value:

    Returns a pointer to the controller structure on success.

    NULL on allocation failure or if a required parameter was not filled in.

--*/

{

    PEFI_SD_DWC_CONTROLLER Controller;
    PEFI_SD_CONTROLLER SdController;
    EFI_SD_INITIALIZATION_BLOCK SdParameters;
    EFI_STATUS Status;

    if (Parameters->ControllerBase == NULL) {
        return NULL;
    }

    SdController = NULL;
    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_SD_DWC_CONTROLLER),
                             (VOID **)&Controller);

    if (EFI_ERROR(Status)) {
        goto CreateControllerEnd;
    }

    EfiSetMem(Controller, sizeof(EFI_SD_DWC_CONTROLLER), 0);
    Controller->ControllerBase = Parameters->ControllerBase;
    Controller->Voltages = Parameters->Voltages;
    Controller->HostCapabilities = Parameters->HostCapabilities;
    Controller->FundamentalClock = Parameters->FundamentalClock;
    if (Parameters->OverrideFunctionTable != NULL) {
        EfiCopyMem(&(Controller->OverrideFunctionTable),
                   Parameters->OverrideFunctionTable,
                   sizeof(SD_FUNCTION_TABLE));
    }

    Controller->OverrideContext = Parameters->OverrideContext;

    //
    // Forward this call onto the core SD library for creation.
    //

    EfiSetMem(&SdParameters, sizeof(EFI_SD_INITIALIZATION_BLOCK), 0);
    SdParameters.ConsumerContext = Controller;
    SdParameters.OverrideFunctionTable = &EfiSdDwcFunctionTable;
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
EfiSdDwcDestroyController (
    PEFI_SD_DWC_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine destroys a DesignWare SD controller object.

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
EfiSdDwcInitializeController (
    PEFI_SD_DWC_CONTROLLER Controller,
    BOOLEAN SoftReset
    )

/*++

Routine Description:

    This routine resets and initializes the DesignWare SD host controller.

Arguments:

    Controller - Supplies a pointer to the controller to initialize.

    SoftReset - Supplies a boolean indicating whether or not to perform a soft
        reset on the controller.

Return Value:

    Status code.

--*/

{

    EFI_STATUS Status;

    Status = EfiSdInitializeController(Controller->SdController, SoftReset);
    if (EFI_ERROR(Status)) {
        goto InitializeControllerEnd;
    }

InitializeControllerEnd:
    return Status;
}

EFI_STATUS
EfiSdDwcBlockIoPolled (
    PEFI_SD_DWC_CONTROLLER Controller,
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
EfiSdDwcGetMediaParameters (
    PEFI_SD_DWC_CONTROLLER Controller,
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
EfiSdDwcSetClockSpeed (
    PEFI_SD_DWC_CONTROLLER DwcController,
    UINT32 ClockSpeed
    )

/*++

Routine Description:

    This routine sets the controller's clock speed.

Arguments:

    DwcController - Supplies a pointer to this DesignWare SD controller.

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

    if (DwcController->FundamentalClock == 0) {
        return EFI_INVALID_PARAMETER;
    }

    //
    // Wait for the card to not be busy.
    //

    Timeout = EFI_SD_DWC_CONTROLLER_TIMEOUT;
    Time = 0;
    Status = EFI_TIMEOUT;
    do {
        Value = SD_DWC_READ_REGISTER(DwcController, SdDwcStatus);
        if ((Value & SD_DWC_STATUS_DATA_BUSY) == 0) {
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

    SD_DWC_WRITE_REGISTER(DwcController, SdDwcClockEnable, 0);

    //
    // Send the command to indicate that the clock enable register is being
    // updated.
    //

    Value = SD_DWC_COMMAND_START |
            SD_DWC_COMMAND_UPDATE_CLOCK_REGISTERS |
            SD_DWC_COMMAND_WAIT_PREVIOUS_DATA_COMPLETE;

    SD_DWC_WRITE_REGISTER(DwcController, SdDwcCommand, Value);
    Timeout = EFI_SD_DWC_CONTROLLER_TIMEOUT;
    Time = 0;
    Status = EFI_TIMEOUT;
    do {
        Value = SD_DWC_READ_REGISTER(DwcController, SdDwcCommand);
        if ((Value & SD_DWC_COMMAND_START) == 0) {
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

    if (ClockSpeed >= DwcController->FundamentalClock) {
        Divisor = 0;

    } else {
        Divisor = 2;
        while (Divisor < SD_DWC_MAX_DIVISOR) {
            if ((DwcController->FundamentalClock / Divisor) <= ClockSpeed) {
                break;
            }

            Divisor += 2;
        }

        Divisor >>= 1;
    }

    SD_DWC_WRITE_REGISTER(DwcController, SdDwcClockDivider, Divisor);
    SD_DWC_WRITE_REGISTER(DwcController,
                          SdDwcClockSource,
                          SD_DWC_CLOCK_SOURCE_DIVIDER_0);

    //
    // Send the command to indicate that the clock source and divider are is
    // being updated.
    //

    Value = SD_DWC_COMMAND_START |
            SD_DWC_COMMAND_UPDATE_CLOCK_REGISTERS |
            SD_DWC_COMMAND_WAIT_PREVIOUS_DATA_COMPLETE;

    SD_DWC_WRITE_REGISTER(DwcController, SdDwcCommand, Value);
    Timeout = EFI_SD_DWC_CONTROLLER_TIMEOUT;
    Time = 0;
    Status = EFI_TIMEOUT;
    do {
        Value = SD_DWC_READ_REGISTER(DwcController, SdDwcCommand);
        if ((Value & SD_DWC_COMMAND_START) == 0) {
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

    SD_DWC_WRITE_REGISTER(DwcController,
                          SdDwcClockEnable,
                          (SD_DWC_CLOCK_ENABLE_LOW_POWER |
                           SD_DWC_CLOCK_ENABLE_ON));

    //
    // Send the command to indicate that the clock is enable register being
    // updated.
    //

    Value = SD_DWC_COMMAND_START |
            SD_DWC_COMMAND_UPDATE_CLOCK_REGISTERS |
            SD_DWC_COMMAND_WAIT_PREVIOUS_DATA_COMPLETE;

    SD_DWC_WRITE_REGISTER(DwcController, SdDwcCommand, Value);
    Timeout = EFI_SD_DWC_CONTROLLER_TIMEOUT;
    Time = 0;
    Status = EFI_TIMEOUT;
    do {
        Value = SD_DWC_READ_REGISTER(DwcController, SdDwcCommand);
        if ((Value & SD_DWC_COMMAND_START) == 0) {
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
EfipSdDwcInitializeController (
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

    PEFI_SD_DWC_CONTROLLER DwcController;
    UINT32 Mask;
    EFI_STATUS Status;
    UINT32 Value;
    UINT32 Voltage;

    DwcController = (PEFI_SD_DWC_CONTROLLER)Context;
    if (DwcController->OverrideFunctionTable.InitializeController != NULL) {
        Status = DwcController->OverrideFunctionTable.InitializeController(
                                                Controller,
                                                DwcController->OverrideContext,
                                                Phase);

        return Status;
    }

    //
    // Phase 0 is an early initialization phase that happens after the
    // controller has been set. It is used to gather capabilities and set
    // certain parameters in the hardware.
    //

    if (Phase == 0) {
        Mask = SD_DWC_CONTROL_FIFO_RESET | SD_DWC_CONTROL_CONTROLLER_RESET;
        SD_DWC_WRITE_REGISTER(DwcController, SdDwcControl, Mask);
        do {
            Value = SD_DWC_READ_REGISTER(DwcController, SdDwcControl);

        } while ((Value & Mask) != 0);

        //
        // Set the default burst length.
        //

        Value = (SD_DWC_BUS_MODE_BURST_LENGTH_16 <<
                 SD_DWC_BUS_MODE_BURST_LENGTH_SHIFT) |
                SD_DWC_BUS_MODE_FIXED_BURST;

        SD_DWC_WRITE_REGISTER(DwcController, SdDwcBusMode, Value);

        //
        // Set the default FIFO threshold.
        //

        SD_DWC_WRITE_REGISTER(DwcController,
                              SdDwcFifoThreshold,
                              SD_DWC_FIFO_THRESHOLD_DEFAULT);

        //
        // Set the default timeout.
        //

        SD_DWC_WRITE_REGISTER(DwcController,
                              SdDwcTimeout,
                              SD_DWC_TIMEOUT_DEFAULT);

        //
        // Set the voltages based on the supported values supplied when the
        // controller was created.
        //

        Voltage = SD_DWC_READ_REGISTER(DwcController, SdDwcUhs);
        Voltage &= ~SD_DWC_UHS_VOLTAGE_MASK;
        if ((DwcController->Voltages &
             (SD_VOLTAGE_32_33 | SD_VOLTAGE_33_34)) ==
            (SD_VOLTAGE_32_33 | SD_VOLTAGE_33_34)) {

            Voltage |= SD_DWC_UHS_VOLTAGE_3V3;

        } else if ((DwcController->Voltages &
                    (SD_VOLTAGE_165_195 | SD_VOLTAGE_18)) != 0) {

            Voltage |= SD_DWC_UHS_VOLTAGE_1V8;

        } else {
            Status = EFI_DEVICE_ERROR;
            goto InitializeControllerEnd;
        }

        SD_DWC_WRITE_REGISTER(DwcController, SdDwcUhs, Voltage);

    //
    // Phase 1 happens right before the initialization command sequence is
    // about to begin. The clock and bus width have been program and the device
    // is just about read to go.
    //

    } else if (Phase == 1) {

        //
        // Turn on the power.
        //

        SD_DWC_WRITE_REGISTER(DwcController, SdDwcPower, SD_DWC_POWER_ENABLE);

        //
        // Set the interrupt mask, clear any pending state, and enable the
        // interrupts.
        //

        SD_DWC_WRITE_REGISTER(DwcController, SdDwcInterruptMask, 0);
        SD_DWC_WRITE_REGISTER(DwcController,
                              SdDwcInterruptStatus,
                              SD_DWC_INTERRUPT_STATUS_ALL_MASK);

        Value = SD_DWC_READ_REGISTER(DwcController, SdDwcControl);
        Value |= SD_DWC_CONTROL_INTERRUPT_ENABLE;
        SD_DWC_WRITE_REGISTER(DwcController, SdDwcControl, Value);
    }

    Status = EFI_SUCCESS;

InitializeControllerEnd:
    return Status;
}

EFI_STATUS
EfipSdDwcResetController (
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

    PEFI_SD_DWC_CONTROLLER DwcController;
    UINT32 ResetMask;
    EFI_STATUS Status;
    UINT64 Time;
    UINT64 Timeout;
    UINT32 Value;

    DwcController = (PEFI_SD_DWC_CONTROLLER)Context;
    if (DwcController->OverrideFunctionTable.ResetController != NULL) {
        Status = DwcController->OverrideFunctionTable.ResetController(
                                                Controller,
                                                DwcController->OverrideContext,
                                                Flags);

        return Status;
    }

    //
    // Always reset the FIFO, but only reset the whole controller if the all
    // flag was specified.
    //

    ResetMask = SD_DWC_CONTROL_FIFO_RESET;
    if ((Flags & SD_RESET_FLAG_ALL) != 0) {
        ResetMask |= SD_DWC_CONTROL_CONTROLLER_RESET;
    }

    SD_DWC_WRITE_REGISTER(DwcController, SdDwcControl, ResetMask);
    Status = EFI_TIMEOUT;
    Timeout = EFI_SD_DWC_CONTROLLER_TIMEOUT;
    Time = 0;
    do {
        Value = SD_DWC_READ_REGISTER(DwcController, SdDwcControl);
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

    return EFI_SUCCESS;
}

EFI_STATUS
EfipSdDwcSendCommand (
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
    PEFI_SD_DWC_CONTROLLER DwcController;
    UINT32 Flags;
    EFI_STATUS Status;
    UINT64 Time;
    UINT64 Timeout;
    UINT32 Value;

    DwcController = (PEFI_SD_DWC_CONTROLLER)Context;
    if (DwcController->OverrideFunctionTable.SendCommand != NULL) {
        Status = DwcController->OverrideFunctionTable.SendCommand(
                                                Controller,
                                                DwcController->OverrideContext,
                                                Command);

        return Status;
    }

    //
    // Wait for the last command to complete.
    //

    Value = SD_DWC_READ_REGISTER(DwcController, SdDwcStatus);
    if ((Value & SD_DWC_STATUS_FIFO_EMPTY) == 0) {
        Value = SD_DWC_READ_REGISTER(DwcController, SdDwcControl);
        Value |= SD_DWC_CONTROL_FIFO_RESET;
        SD_DWC_WRITE_REGISTER(DwcController, SdDwcControl, Value);
        Timeout = EFI_SD_DWC_CONTROLLER_TIMEOUT;
        Time = 0;
        Status = EFI_TIMEOUT;
        do {
            Value = SD_DWC_READ_REGISTER(DwcController, SdDwcControl);
            if ((Value & SD_DWC_CONTROL_FIFO_RESET) == 0) {
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

    SD_DWC_WRITE_REGISTER(DwcController,
                          SdDwcInterruptStatus,
                          SD_DWC_INTERRUPT_STATUS_ALL_MASK);

    //
    // Set up the response flags.
    //

    Flags = SD_DWC_COMMAND_WAIT_PREVIOUS_DATA_COMPLETE;
    if (Command->Command == SdCommandReset) {
        Flags |= SD_DWC_COMMAND_SEND_INITIALIZATION;
    }

    if ((Command->ResponseType & SD_RESPONSE_PRESENT) != 0) {
        if ((Command->ResponseType & SD_RESPONSE_136_BIT) != 0) {
            Flags |= SD_DWC_COMMAND_LONG_RESPONSE;
        }

        Flags |= SD_DWC_COMMAND_RESPONSE_EXPECTED;
    }

    //
    // Set up the remainder of the command flags.
    //

    if ((Command->ResponseType & SD_RESPONSE_VALID_CRC) != 0) {
        Flags |= SD_DWC_COMMAND_CHECK_RESPONSE_CRC;
    }

    //
    // If there's a data buffer, program the block count.
    //

    if (Command->BufferSize != 0) {
        Flags |= SD_DWC_COMMAND_DATA_EXPECTED;
        if (Command->Write != FALSE) {
            Flags |= SD_DWC_COMMAND_WRITE;

        } else {
            Flags |= SD_DWC_COMMAND_READ;
        }

        //
        // If reading or writing multiple blocks, the block size register
        // should be set to the default block size and the byte count should be
        // a multiple of the block size.
        //

        if ((Command->Command == SdCommandReadMultipleBlocks) ||
            (Command->Command == SdCommandWriteMultipleBlocks)) {

            if ((DwcController->HostCapabilities & SD_MODE_AUTO_CMD12) != 0) {
                Flags |= SD_DWC_COMMAND_SEND_AUTO_STOP;
            }

            SD_DWC_WRITE_REGISTER(DwcController,
                                  SdDwcBlockSize,
                                  SD_DWC_BLOCK_SIZE);

            SD_DWC_WRITE_REGISTER(DwcController,
                                  SdDwcByteCount,
                                  Command->BufferSize);

        //
        // Otherwise set the block size to total number of bytes to be
        // processed.
        //

        } else {
            SD_DWC_WRITE_REGISTER(DwcController,
                                  SdDwcBlockSize,
                                  Command->BufferSize);

            SD_DWC_WRITE_REGISTER(DwcController,
                                  SdDwcByteCount,
                                  Command->BufferSize);
        }
    }

    SD_DWC_WRITE_REGISTER(DwcController, SdDwcTimeout, 0xFFFFFFFF);

    //
    // Write the command argument.
    //

    SD_DWC_WRITE_REGISTER(DwcController,
                          SdDwcCommandArgument,
                          Command->CommandArgument);

    //
    // Set the command and wait for it to be accepted.
    //

    CommandValue = (Command->Command << SD_DWC_COMMAND_INDEX_SHIFT) &
                   SD_DWC_COMMAND_INDEX_MASK;

    CommandValue |= SD_DWC_COMMAND_START |
                    SD_DWC_COMMAND_USE_HOLD_REGISTER |
                    Flags;

    SD_DWC_WRITE_REGISTER(DwcController, SdDwcCommand, CommandValue);
    Timeout = EFI_SD_DWC_CONTROLLER_TIMEOUT;
    Time = 0;
    Status = EFI_TIMEOUT;
    do {
        Value = SD_DWC_READ_REGISTER(DwcController, SdDwcCommand);
        if ((Value & SD_DWC_COMMAND_START) == 0) {
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

    Timeout = EFI_SD_DWC_CONTROLLER_TIMEOUT;
    Time = 0;
    Status = EFI_TIMEOUT;
    do {
        Value = SD_DWC_READ_REGISTER(DwcController, SdDwcInterruptStatus);
        if ((Value & SD_DWC_INTERRUPT_STATUS_COMMAND_DONE) != 0) {
            Status = EFI_SUCCESS;
            break;
        }

        EfiStall(50);
        Time += 50;

    } while (Time <= Timeout);

    if (EFI_ERROR(Status)) {
        goto SendCommandEnd;
    }

    if ((Value & SD_DWC_INTERRUPT_STATUS_ERROR_RESPONSE_TIMEOUT) != 0) {
        SD_DWC_WRITE_REGISTER(DwcController,
                              SdDwcInterruptStatus,
                              SD_DWC_INTERRUPT_STATUS_ALL_MASK);

        EfipSdDwcResetController(Controller,
                                 DwcController,
                                 SD_RESET_FLAG_COMMAND_LINE);

        Status = EFI_TIMEOUT;
        goto SendCommandEnd;

    } else if ((Value & SD_DWC_INTERRUPT_STATUS_COMMAND_ERROR_MASK) != 0) {
        SD_DWC_WRITE_REGISTER(DwcController,
                              SdDwcInterruptStatus,
                              SD_DWC_INTERRUPT_STATUS_ALL_MASK);

        Status = EFI_DEVICE_ERROR;
        goto SendCommandEnd;
    }

    //
    // Acknowledge the completed command.
    //

    SD_DWC_WRITE_REGISTER(DwcController,
                          SdDwcInterruptStatus,
                          SD_DWC_INTERRUPT_STATUS_COMMAND_DONE);

    //
    // Get the response if there is one.
    //

    if ((Command->ResponseType & SD_RESPONSE_PRESENT) != 0) {
        if ((Command->ResponseType & SD_RESPONSE_136_BIT) != 0) {
            Command->Response[3] = SD_DWC_READ_REGISTER(DwcController,
                                                        SdDwcResponse0);

            Command->Response[2] = SD_DWC_READ_REGISTER(DwcController,
                                                        SdDwcResponse1);

            Command->Response[1] = SD_DWC_READ_REGISTER(DwcController,
                                                        SdDwcResponse2);

            Command->Response[0] = SD_DWC_READ_REGISTER(DwcController,
                                                        SdDwcResponse3);

            if ((DwcController->HostCapabilities &
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
            Command->Response[0] = SD_DWC_READ_REGISTER(DwcController,
                                                        SdDwcResponse0);
        }
    }

    //
    // Read/write the data.
    //

    if (Command->BufferSize != 0) {
        if (Command->Write != FALSE) {
            Status = EfipSdDwcWriteData(Controller,
                                        Context,
                                        Command->Buffer,
                                        Command->BufferSize);

        } else {
            Status = EfipSdDwcReadData(Controller,
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
EfipSdDwcGetSetBusWidth (
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

    PEFI_SD_DWC_CONTROLLER DwcController;
    EFI_STATUS Status;
    UINT32 Value;

    DwcController = (PEFI_SD_DWC_CONTROLLER)Context;
    if (DwcController->OverrideFunctionTable.GetSetBusWidth != NULL) {
        Status = DwcController->OverrideFunctionTable.GetSetBusWidth(
                                                Controller,
                                                DwcController->OverrideContext,
                                                BusWidth,
                                                Set);

        return Status;
    }

    if (Set != FALSE) {
        switch (*BusWidth) {
        case 1:
            Value = SD_DWC_CARD_TYPE_1_BIT_WIDTH;
            break;

        case 4:
            Value = SD_DWC_CARD_TYPE_4_BIT_WIDTH;
            break;

        case 8:
            Value = SD_DWC_CARD_TYPE_8_BIT_WIDTH;
            break;

        default:
            return EFI_INVALID_PARAMETER;
        }

        SD_DWC_WRITE_REGISTER(DwcController, SdDwcCardType, Value);

    } else {
        Value = SD_DWC_READ_REGISTER(DwcController, SdDwcCardType);
        if ((Value & SD_DWC_CARD_TYPE_8_BIT_WIDTH) != 0) {
            *BusWidth = 8;

        } else if ((Value & SD_DWC_CARD_TYPE_4_BIT_WIDTH) != 0) {
            *BusWidth = 4;

        } else {
            *BusWidth = 1;
        }
    }

    return EFI_SUCCESS;
}

EFI_STATUS
EfipSdDwcGetSetClockSpeed (
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

    PEFI_SD_DWC_CONTROLLER DwcController;
    EFI_STATUS Status;

    DwcController = (PEFI_SD_DWC_CONTROLLER)Context;
    if (DwcController->OverrideFunctionTable.GetSetClockSpeed != NULL) {
        Status = DwcController->OverrideFunctionTable.GetSetClockSpeed(
                                                Controller,
                                                DwcController->OverrideContext,
                                                ClockSpeed,
                                                Set);

        return Status;
    }

    if (DwcController->FundamentalClock == 0) {
        return EFI_INVALID_PARAMETER;
    }

    //
    // Getting the clock speed is not implemented as the divisor math might not
    // work out precisely in reverse.
    //

    if (Set == FALSE) {
        return EFI_UNSUPPORTED;
    }

    return EfiSdDwcSetClockSpeed(DwcController, *ClockSpeed);
}

//
// --------------------------------------------------------- Internal Functions
//

EFI_STATUS
EfipSdDwcReadData (
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
    PEFI_SD_DWC_CONTROLLER DwcController;
    UINT32 Interrupts;
    UINT32 IoIndex;
    EFI_STATUS Status;
    UINT64 Time;
    UINT64 Timeout;
    UINT32 Value;

    DwcController = (PEFI_SD_DWC_CONTROLLER)Context;
    DataTransferOver = FALSE;
    Buffer32 = (UINT32 *)Data;
    Size /= sizeof(UINT32);
    while (Size != 0) {

        //
        // Get the interrupt status register.
        //

        Time = 0;
        Timeout = EFI_SD_DWC_CONTROLLER_TIMEOUT;
        Status = EFI_TIMEOUT;
        do {
            Interrupts = SD_DWC_READ_REGISTER(DwcController,
                                              SdDwcInterruptStatus);

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

        if ((Interrupts & SD_DWC_INTERRUPT_STATUS_DATA_ERROR_MASK) != 0) {
            EfipSdDwcResetController(Controller,
                                     Context,
                                     SD_RESET_FLAG_DATA_LINE);

            return EFI_DEVICE_ERROR;
        }

        //
        // Check for received data status. If data is ready, the status
        // register holds the number of 32-bit elements to be read.
        //

        DataReadyMask = SD_DWC_INTERRUPT_STATUS_RECEIVE_FIFO_DATA_REQUEST;
        if ((Interrupts & DataReadyMask) != 0) {
            Count = SD_DWC_READ_REGISTER(DwcController, SdDwcStatus);
            Count = (Count & SD_DWC_STATUS_FIFO_COUNT_MASK) >>
                    SD_DWC_STATUS_FIFO_COUNT_SHIFT;

            for (IoIndex = 0; IoIndex < Count; IoIndex += 1) {
                *Buffer32 = SD_DWC_READ_REGISTER(DwcController, SdDwcFifoBase);
                Buffer32 += 1;
            }

            Size -= Count;
            SD_DWC_WRITE_REGISTER(DwcController,
                                  SdDwcInterruptStatus,
                                  DataReadyMask);
        }

        //
        // Check for the transfer over bit. If it is set, then read the rest of
        // the bytes from the FIFO.
        //

        if ((Interrupts & SD_DWC_INTERRUPT_STATUS_DATA_TRANSFER_OVER) != 0) {
            for (IoIndex = 0; IoIndex < Size; IoIndex += 1) {
                *Buffer32 = SD_DWC_READ_REGISTER(DwcController, SdDwcFifoBase);
                Buffer32 += 1;
            }

            SD_DWC_WRITE_REGISTER(DwcController,
                                  SdDwcInterruptStatus,
                                  SD_DWC_INTERRUPT_STATUS_DATA_TRANSFER_OVER);

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
        Timeout = EFI_SD_DWC_CONTROLLER_TIMEOUT;
        Status = EFI_TIMEOUT;
        do {
            Interrupts = SD_DWC_READ_REGISTER(DwcController,
                                              SdDwcInterruptStatus);

            if ((Interrupts &
                 SD_DWC_INTERRUPT_STATUS_DATA_TRANSFER_OVER) != 0) {

                Status = EFI_SUCCESS;
                break;
            }

            EfiStall(50);
            Time += 50;

        } while (Time <= Timeout);

        if (EFI_ERROR(Status)) {
            return Status;
        }

        SD_DWC_WRITE_REGISTER(DwcController,
                              SdDwcInterruptStatus,
                              SD_DWC_INTERRUPT_STATUS_DATA_TRANSFER_OVER);
    }

    //
    // Wait until the state machine and data stop being busy.
    //

    Timeout = EFI_SD_DWC_CONTROLLER_TIMEOUT;
    Time = 0;
    Status = EFI_TIMEOUT;
    BusyMask = SD_DWC_STATUS_DATA_STATE_MACHINE_BUSY |
               SD_DWC_STATUS_DATA_BUSY;

    do {
        Value = SD_DWC_READ_REGISTER(DwcController, SdDwcStatus);
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
EfipSdDwcWriteData (
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
    PEFI_SD_DWC_CONTROLLER DwcController;
    UINT32 Interrupts;
    UINT32 IoIndex;
    EFI_STATUS Status;
    UINT64 Time;
    UINT64 Timeout;
    UINT32 Value;

    DwcController = (PEFI_SD_DWC_CONTROLLER)Context;
    DataTransferOver = FALSE;
    Buffer32 = (UINT32 *)Data;
    Size /= sizeof(UINT32);
    while (Size != 0) {

        //
        // Get the interrupt status register.
        //

        Time = 0;
        Timeout = EFI_SD_DWC_CONTROLLER_TIMEOUT;
        Status = EFI_TIMEOUT;
        do {
            Interrupts = SD_DWC_READ_REGISTER(DwcController,
                                              SdDwcInterruptStatus);

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

        if ((Interrupts & SD_DWC_INTERRUPT_STATUS_DATA_ERROR_MASK) != 0) {
            EfipSdDwcResetController(Controller,
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

        DataRequestMask = SD_DWC_INTERRUPT_STATUS_TRANSMIT_FIFO_DATA_REQUEST;
        if ((Interrupts & DataRequestMask) != 0) {
            Count = SD_DWC_READ_REGISTER(DwcController, SdDwcStatus);
            Count = (Count & SD_DWC_STATUS_FIFO_COUNT_MASK) >>
                    SD_DWC_STATUS_FIFO_COUNT_SHIFT;

            Count = (SD_DWC_FIFO_DEPTH / sizeof(UINT32)) - Count;
            for (IoIndex = 0; IoIndex < Count; IoIndex += 1) {
                SD_DWC_WRITE_REGISTER(DwcController, SdDwcFifoBase, *Buffer32);
                Buffer32 += 1;
            }

            Size -= Count;
            SD_DWC_WRITE_REGISTER(DwcController,
                                  SdDwcInterruptStatus,
                                  DataRequestMask);
        }

        //
        // Check for the transfer over bit. If it is set, then exit.
        //

        if ((Interrupts & SD_DWC_INTERRUPT_STATUS_DATA_TRANSFER_OVER) != 0) {
            SD_DWC_WRITE_REGISTER(DwcController,
                                  SdDwcInterruptStatus,
                                  SD_DWC_INTERRUPT_STATUS_DATA_TRANSFER_OVER);

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
        Timeout = EFI_SD_DWC_CONTROLLER_TIMEOUT;
        Time = 0;
        Status = EFI_TIMEOUT;
        do {
            Interrupts = SD_DWC_READ_REGISTER(DwcController,
                                             SdDwcInterruptStatus);

            if ((Interrupts &
                 SD_DWC_INTERRUPT_STATUS_DATA_TRANSFER_OVER) != 0) {

                Status = EFI_SUCCESS;
                break;
            }

            EfiStall(50);
            Time += 50;

        } while (Time <= Timeout);

        if (EFI_ERROR(Status)) {
            return Status;
        }

        SD_DWC_WRITE_REGISTER(DwcController,
                              SdDwcInterruptStatus,
                              SD_DWC_INTERRUPT_STATUS_DATA_TRANSFER_OVER);
    }

    //
    // Wait until the state machine and data stop being busy.
    //

    Timeout = EFI_SD_DWC_CONTROLLER_TIMEOUT;
    Time = 0;
    Status = EFI_TIMEOUT;
    BusyMask = SD_DWC_STATUS_DATA_STATE_MACHINE_BUSY |
               SD_DWC_STATUS_DATA_BUSY;

    do {
        Value = SD_DWC_READ_REGISTER(DwcController, SdDwcStatus);
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

