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

    Chris Stevens 16-Jul-2015

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
// These macros read and write SD controller registers.
//

#define SD_READ_REGISTER(_Controller, _Register) \
    EfiReadRegister32((_Controller)->ControllerBase + (_Register))

#define SD_WRITE_REGISTER(_Controller, _Register, _Value) \
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
EfipSdInitializeController (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    UINT32 Phase
    );

EFI_STATUS
EfipSdResetController (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    UINT32 Flags
    );

EFI_STATUS
EfipSdSendCommand (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    PSD_COMMAND Command
    );

EFI_STATUS
EfipSdGetSetBusWidth (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    UINT16 *BusWidth,
    BOOLEAN Set
    );

EFI_STATUS
EfipSdGetSetClockSpeed (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    UINT32 *ClockSpeed,
    BOOLEAN Set
    );

EFI_STATUS
EfipSdReadData (
    PEFI_SD_CONTROLLER Controller,
    VOID *Data,
    UINT32 Size
    );

EFI_STATUS
EfipSdWriteData (
    PEFI_SD_CONTROLLER Controller,
    VOID *Data,
    UINT32 Size
    );

//
// -------------------------------------------------------------------- Globals
//

SD_FUNCTION_TABLE EfiSdStdFunctionTable = {
    EfipSdInitializeController,
    EfipSdResetController,
    EfipSdSendCommand,
    EfipSdGetSetBusWidth,
    EfipSdGetSetClockSpeed
};

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipSdInitializeController (
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

    UINT32 Capabilities;
    UINT32 HostControl;
    SD_HOST_VERSION HostVersion;
    EFI_STATUS Status;
    UINT32 Value;

    //
    // Phase 0 is an early initialization phase that happens after the
    // controller has been set. It is used to gather capabilities and set
    // certain parameters in the hardware.
    //

    if (Phase == 0) {
        Capabilities = SD_READ_REGISTER(Controller, SdRegisterCapabilities);
        if ((Capabilities & SD_CAPABILITY_ADMA2) != 0) {
            Controller->HostCapabilities |= SD_MODE_ADMA2;
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
            Status = EFI_DEVICE_ERROR;
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
            Status = EFI_DEVICE_ERROR;
            goto InitializeControllerEnd;
        }

        SD_WRITE_REGISTER(Controller, SdRegisterHostControl, HostControl);

        //
        // Set the base clock frequency if not supplied on creation.
        //

        if (Controller->FundamentalClock == 0) {
            Value = SD_READ_REGISTER(Controller, SdRegisterSlotStatusVersion);
            HostVersion = (Value >> 16) & SD_HOST_VERSION_MASK;
            if (HostVersion >= SdHostVersion3) {
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
            Status = EFI_DEVICE_ERROR;
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
        SD_WRITE_REGISTER(Controller, SdRegisterInterruptSignalEnable, 0);
    }

    Status = EFI_SUCCESS;

InitializeControllerEnd:
    return Status;
}

EFI_STATUS
EfipSdResetController (
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

    UINT32 ResetBits;
    EFI_STATUS Status;
    UINT64 Time;
    UINT64 Timeout;
    UINT32 Value;

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
    Timeout = EFI_SD_CONTROLLER_TIMEOUT;
    Time = 0;
    Status = EFI_TIMEOUT;
    do {
        Value = SD_READ_REGISTER(Controller, SdRegisterClockControl);
        if ((Value & ResetBits) == 0) {
            Status = EFI_SUCCESS;
            break;
        }

        EfiStall(50);
        Time += 50;

    } while (Time <= Timeout);

    SD_WRITE_REGISTER(Controller, SdRegisterInterruptStatusEnable, 0xFFFFFFFF);
    SD_WRITE_REGISTER(Controller, SdRegisterInterruptStatus, 0xFFFFFFFF);
    return Status;
}

EFI_STATUS
EfipSdSendCommand (
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

    UINT32 Flags;
    UINT32 InhibitMask;
    EFI_STATUS Status;
    UINT64 Time;
    UINT64 Timeout;
    UINT32 Value;

    Timeout = EFI_SD_CONTROLLER_TIMEOUT;
    Time = 0;
    Status = EFI_TIMEOUT;

    //
    // Don't wait for the data inhibit flag if this is the abort command.
    //

    InhibitMask = SD_STATE_DATA_INHIBIT | SD_STATE_COMMAND_INHIBIT;
    if ((Command->Command == SdCommandStopTransmission) &&
        (Command->ResponseType != SD_RESPONSE_R1B)) {

        InhibitMask = SD_STATE_COMMAND_INHIBIT;
    }

    do {
        Value = SD_READ_REGISTER(Controller, SdRegisterPresentState);
        if ((Value & InhibitMask) == 0) {
            Status = EFI_SUCCESS;
            break;
        }

        EfiStall(5);
        Time += 5;

    } while (Time <= Timeout);

    if (EFI_ERROR(Status)) {
        goto SendCommandEnd;
    }

    //
    // Clear interrupts from the previous command.
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

            Value = SD_BLOCK_SIZE |
                    ((Command->BufferSize / SD_BLOCK_SIZE) << 16);

            SD_WRITE_REGISTER(Controller, SdRegisterBlockSizeCount, Value);

        } else {
            Value = Command->BufferSize;
            SD_WRITE_REGISTER(Controller, SdRegisterBlockSizeCount, Value);
        }

        Flags |= SD_COMMAND_DATA_PRESENT;
        if (Command->Write != FALSE) {
            Flags |= SD_COMMAND_TRANSFER_WRITE;

        } else {
            Flags |= SD_COMMAND_TRANSFER_READ;
        }
    }

    SD_WRITE_REGISTER(Controller,
                      SdRegisterArgument1,
                      Command->CommandArgument);

    Value = (Command->Command << SD_COMMAND_INDEX_SHIFT) | Flags;
    SD_WRITE_REGISTER(Controller, SdRegisterCommand, Value);
    Timeout = EFI_SD_CONTROLLER_TIMEOUT;
    Time = 0;
    Status = EFI_TIMEOUT;
    do {
        Value = SD_READ_REGISTER(Controller, SdRegisterInterruptStatus);
        if (Value != 0) {
            Status = EFI_SUCCESS;
            break;
        }

        EfiStall(5);
        Time += 5;

    } while (Time <= Timeout);

    if (EFI_ERROR(Status)) {
        goto SendCommandEnd;
    }

    if ((Value & SD_INTERRUPT_STATUS_COMMAND_TIMEOUT_ERROR) != 0) {
        EfipSdResetController(Controller,
                              Controller->ConsumerContext,
                              SD_RESET_FLAG_COMMAND_LINE);

        Status = EFI_TIMEOUT;
        goto SendCommandEnd;

    } else if ((Value & SD_INTERRUPT_STATUS_ERROR_INTERRUPT) != 0) {
        Status = EFI_DEVICE_ERROR;
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
            Status = EfipSdWriteData(Controller,
                                     Command->Buffer,
                                     Command->BufferSize);

        } else {
            Status = EfipSdReadData(Controller,
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
EfipSdGetSetBusWidth (
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

    UINT32 Value;

    Value = SD_READ_REGISTER(Controller, SdRegisterHostControl);
    if (Set != FALSE) {
        Value &= ~SD_HOST_CONTROL_BUS_WIDTH_MASK;
        switch (*BusWidth) {
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
            return EFI_INVALID_PARAMETER;
        }

        SD_WRITE_REGISTER(Controller, SdRegisterHostControl, Value);

    } else {
        if ((Value & SD_HOST_CONTROL_DATA_8BIT) != 0) {
            *BusWidth = 8;

        } else if ((Value & SD_HOST_CONTROL_DATA_4BIT) != 0) {
            *BusWidth = 4;

        } else {
            *BusWidth = 1;
        }
    }

    return EFI_SUCCESS;
}

EFI_STATUS
EfipSdGetSetClockSpeed (
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

    UINT32 ClockControl;
    UINT32 Divisor;
    SD_HOST_VERSION HostVersion;
    UINT32 Result;
    EFI_STATUS Status;
    UINT64 Time;
    UINT64 Timeout;
    UINT32 Value;

    if (Controller->FundamentalClock == 0) {
        return EFI_INVALID_PARAMETER;
    }

    //
    // Getting the clock speed is not implemented as the divisor math might not
    // work out precisely in reverse.
    //

    if (Set == FALSE) {
        return EFI_UNSUPPORTED;
    }

    //
    // Find the right divisor, the highest frequency less than the desired
    // clock. Older controllers must be a power of 2.
    //

    Value = SD_READ_REGISTER(Controller, SdRegisterSlotStatusVersion) >> 16;
    HostVersion = Value & SD_HOST_VERSION_MASK;
    if (HostVersion < SdHostVersion3) {
        Result = Controller->FundamentalClock;
        Divisor = 1;
        while (Divisor < SD_V2_MAX_DIVISOR) {
            if (Result <= *ClockSpeed) {
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
        if (*ClockSpeed >= Controller->FundamentalClock) {
            Divisor = 0;

        } else {
            Divisor = 2;
            while (Divisor < SD_V3_MAX_DIVISOR) {
                if ((Controller->FundamentalClock / Divisor) <= *ClockSpeed) {
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
    Status = EFI_TIMEOUT;
    Timeout = EFI_SD_CONTROLLER_TIMEOUT;
    Time = 0;
    do {
        Value = SD_READ_REGISTER(Controller, SdRegisterClockControl);
        if ((Value & SD_CLOCK_CONTROL_CLOCK_STABLE) != 0) {
            Status = EFI_SUCCESS;
            break;
        }

        EfiStall(50);
        Time += 50;

    } while (Time <= Timeout);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    ClockControl |= SD_CLOCK_CONTROL_SD_CLOCK_ENABLE;
    SD_WRITE_REGISTER(Controller, SdRegisterClockControl, ClockControl);
    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

EFI_STATUS
EfipSdReadData (
    PEFI_SD_CONTROLLER Controller,
    VOID *Data,
    UINT32 Size
    )

/*++

Routine Description:

    This routine reads polled data from the SD controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    Data - Supplies a pointer to the buffer where the data will be read into.

    Size - Supplies the size in bytes. This must be a multiple of four bytes.

Return Value:

    Status code.

--*/

{

    UINT32 *Buffer32;
    UINT32 Count;
    UINT32 IoIndex;
    UINT32 Mask;
    EFI_STATUS Status;
    UINT64 Time;
    UINT64 Timeout;
    UINT32 Value;

    Buffer32 = (UINT32 *)Data;
    Count = Size;
    if (Count > SD_BLOCK_SIZE) {
        Count = SD_BLOCK_SIZE;
    }

    Count /= sizeof(UINT32);
    while (Size != 0) {

        //
        // Get the interrupt status register.
        //

        Time = 0;
        Timeout = EFI_SD_CONTROLLER_TIMEOUT;
        Status = EFI_TIMEOUT;
        do {
            Value = SD_READ_REGISTER(Controller, SdRegisterInterruptStatus);
            if (Value != 0) {
                Status = EFI_SUCCESS;
                break;
            }

            EfiStall(5);
            Time += 5;

        } while (Time <= Timeout);

        if (EFI_ERROR(Status)) {
            return Status;
        }

        Mask = SD_INTERRUPT_STATUS_DATA_TIMEOUT_ERROR |
               SD_INTERRUPT_STATUS_DATA_CRC_ERROR |
               SD_INTERRUPT_STATUS_DATA_END_BIT_ERROR;

        if ((Value & Mask) != 0) {
            EfipSdResetController(Controller,
                                  Controller->ConsumerContext,
                                  SD_RESET_FLAG_DATA_LINE);
        }

        if ((Value & SD_INTERRUPT_STATUS_ERROR_INTERRUPT) != 0) {
            return EFI_DEVICE_ERROR;
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

            Size -= Count * sizeof(UINT32);
        }
    }

    Value = SD_READ_REGISTER(Controller, SdRegisterInterruptStatus);
    Mask = SD_INTERRUPT_STATUS_BUFFER_WRITE_READY |
           SD_INTERRUPT_STATUS_TRANSFER_COMPLETE;

    if ((Value & Mask) != 0) {
        SD_WRITE_REGISTER(Controller, SdRegisterInterruptStatus, Value);
    }

    return EFI_SUCCESS;
}

EFI_STATUS
EfipSdWriteData (
    PEFI_SD_CONTROLLER Controller,
    VOID *Data,
    UINT32 Size
    )

/*++

Routine Description:

    This routine writes polled data to the SD controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    Data - Supplies a pointer to the buffer containing the data to write.

    Size - Supplies the size in bytes. This must be a multiple of 4 bytes.

Return Value:

    Status code.

--*/

{

    UINT32 *Buffer32;
    UINT32 Count;
    UINT32 IoIndex;
    UINT32 Mask;
    EFI_STATUS Status;
    UINT64 Time;
    UINT64 Timeout;
    UINT32 Value;

    Buffer32 = (UINT32 *)Data;
    Count = Size;
    if (Count > SD_BLOCK_SIZE) {
        Count = SD_BLOCK_SIZE;
    }

    Count /= sizeof(UINT32);
    while (Size != 0) {

        //
        // Get the interrupt status register.
        //

        Time = 0;
        Timeout = EFI_SD_CONTROLLER_TIMEOUT;
        Status = EFI_TIMEOUT;
        do {
            Value = SD_READ_REGISTER(Controller, SdRegisterInterruptStatus);
            if (Value != 0) {
                Status = EFI_SUCCESS;
                break;
            }

            Time += 5;
            EfiStall(5);

        } while (Time <= Timeout);

        if (EFI_ERROR(Status)) {
            return Status;
        }

        Mask = SD_INTERRUPT_STATUS_DATA_TIMEOUT_ERROR |
               SD_INTERRUPT_STATUS_DATA_CRC_ERROR |
               SD_INTERRUPT_STATUS_DATA_END_BIT_ERROR;

        if ((Value & Mask) != 0) {
            EfipSdResetController(Controller,
                                  Controller->ConsumerContext,
                                  SD_RESET_FLAG_DATA_LINE);
        }

        if ((Value & SD_INTERRUPT_STATUS_ERROR_INTERRUPT) != 0) {
            return EFI_DEVICE_ERROR;
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

            Size -= Count * sizeof(UINT32);
        }
    }

    Mask = SD_INTERRUPT_STATUS_BUFFER_READ_READY |
           SD_INTERRUPT_STATUS_TRANSFER_COMPLETE;

    if ((Value & Mask) != 0) {
        SD_WRITE_REGISTER(Controller, SdRegisterInterruptStatus, Value);
    }

    return EFI_SUCCESS;
}

