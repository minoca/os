/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sd.c

Abstract:

    This module implements BeagleBone Black SD support in UEFI.

Author:

    Evan Green 20-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include <dev/sd.h>
#include <minoca/uefi/protocol/blockio.h>
#include <minoca/soc/am335x.h>
#include <dev/tirom.h>
#include "bbonefw.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro returns a pointer to the disk I/O data given a pointer to the
// block I/O protocol instance.
//

#define EFI_SD_AM335_FROM_THIS(_BlockIo)                 \
        (EFI_SD_AM335_CONTEXT *)((VOID *)(_BlockIo) -    \
                        ((VOID *)(&(((EFI_SD_AM335_CONTEXT *)0)->BlockIo))))

//
// These macros read and write SD controller registers.
//

#define SD_AM335_READ_REGISTER(_Device, _Register) \
    *(volatile UINT32 *)((_Device)->ControllerBase + (_Register))

#define SD_AM335_WRITE_REGISTER(_Device, _Register, _Value) \
    *(volatile UINT32 *)((_Device)->ControllerBase + (_Register)) = (_Value)

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_SD_AM335_MAGIC 0x33416453 // '3AdS'

#define EFI_SD_AM335_BLOCK_IO_DEVICE_PATH_GUID              \
    {                                                       \
        0xCF31FAC5, 0xC24E, 0x11D2,                         \
        {0x85, 0xF3, 0x00, 0xA0, 0xC9, 0x3E, 0xA7, 0x40}    \
    }

//
// Define the offset into the HSMMC block where the SD standard registers
// start.
//

#define SD_AM335_CONTROLLER_SD_REGISTER_OFFSET 0x200

//
// Define the fundamental frequency of the HSMMC clock. An initial divisor of
// 0x80 (divide by 256) gets a base frequency of 375000, just under the 400kHz
// limit.
//

#define SD_AM335_FUNDAMENTAL_CLOCK_SPEED 96000000
#define SD_AM335_INITIAL_DIVISOR 0x80

#define SD_AM335_SYSCONFIG_REGISTER 0x110
#define SD_AM335_SYSSTATUS_REGISTER 0x114
#define SD_AM335_CON_REGISTER 0x12C

//
// Sysconfig register definitions
//

#define SD_AM335_SYSCONFIG_SOFT_RESET 0x00000002

//
// Sysstatus register definitions
//

#define SD_AM335_SYSSTATUS_RESET_DONE 0x00000001

//
// Con (control) register definitions.
//

#define SD_AM335_CON_INIT           (1 << 1)
#define SD_AM335_CON_DEBOUNCE_MASK  (0x3 << 9)
#define SD_AM335_CON_DMA_MASTER     (1 << 20)

//
// Define the AM335 SD timeout in microseconds.
//

#define EFI_SD_AM335_TIMEOUT 1000000

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes the SD AM335x device context.

Members:

    Magic - Stores the magic constand EFI_SD_AM335_MAGIC.

    Handle - Stores the handle to the block I/O device.

    DevicePath - Stores a pointer to the device path.

    Controller - Stores an pointer to the controller structure.

    ControllerBase - Stores a pointer to the virtual address of the HSMMC
        registers.

    MediaPresent - Stores a boolean indicating whether or not there is a card
        in the slot.

    BlockSize - Stores the cached block size of the media.

    BlockCount - Stores the cached block count of the media.

    BlockIo - Stores the block I/O protocol.

    Media - Stores the block I/O media information.

--*/

typedef struct _EFI_SD_AM335_CONTEXT {
    UINT32 Magic;
    EFI_HANDLE Handle;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    PEFI_SD_CONTROLLER Controller;
    VOID *ControllerBase;
    BOOLEAN MediaPresent;
    UINT32 BlockSize;
    UINT64 BlockCount;
    EFI_BLOCK_IO_PROTOCOL BlockIo;
    EFI_BLOCK_IO_MEDIA Media;
} EFI_SD_AM335_CONTEXT, *PEFI_SD_AM335_CONTEXT;

/*++

Structure Description:

    This structure defines the SD AM335x block I/O device path.

Members:

    DevicePath - Stores the standard vendor-specific device path.

    ControllerBase - Stores the controller number.

--*/

typedef struct _EFI_SD_AM335_BLOCK_IO_DEVICE_PATH {
    VENDOR_DEVICE_PATH DevicePath;
    UINT32 ControllerBase;
} EFI_SD_AM335_BLOCK_IO_DEVICE_PATH, *PEFI_SD_AM335_BLOCK_IO_DEVICE_PATH;

/*++

Structure Description:

    This structure defines the AM335x SD block I/O device path.

Members:

    Disk - Stores the disk device path node.

    End - Stores the end device path node.

--*/

#pragma pack(push, 1)

typedef struct _EFI_SD_AM335_DEVICE_PATH {
    EFI_SD_AM335_BLOCK_IO_DEVICE_PATH Disk;
    EFI_DEVICE_PATH_PROTOCOL End;
} PACKED EFI_SD_AM335_DEVICE_PATH, *PEFI_SD_AM335_DEVICE_PATH;

#pragma pack(pop)

//
// ----------------------------------------------- Internal Function Prototypes
//

EFI_STATUS
EfipBeagleBoneEnumerateSdController (
    UINT32 ControllerBase,
    BOOLEAN RemovableMedia
    );

EFIAPI
EFI_STATUS
EfipSdAm335Reset (
    EFI_BLOCK_IO_PROTOCOL *This,
    BOOLEAN ExtendedVerification
    );

EFIAPI
EFI_STATUS
EfipSdAm335ReadBlocks (
    EFI_BLOCK_IO_PROTOCOL *This,
    UINT32 MediaId,
    EFI_LBA Lba,
    UINTN BufferSize,
    VOID *Buffer
    );

EFIAPI
EFI_STATUS
EfipSdAm335WriteBlocks (
    EFI_BLOCK_IO_PROTOCOL *This,
    UINT32 MediaId,
    EFI_LBA Lba,
    UINTN BufferSize,
    VOID *Buffer
    );

EFIAPI
EFI_STATUS
EfipSdAm335FlushBlocks (
    EFI_BLOCK_IO_PROTOCOL *This
    );

EFI_STATUS
EfipSdAm335ResetController (
    PEFI_SD_AM335_CONTEXT Device
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the private data template.
//

EFI_SD_AM335_CONTEXT EfiSdAm335DiskTemplate = {
    EFI_SD_AM335_MAGIC,
    NULL,
    NULL,
    NULL,
    NULL,
    FALSE,
    0,
    0,
    {
        EFI_BLOCK_IO_PROTOCOL_REVISION3,
        NULL,
        EfipSdAm335Reset,
        EfipSdAm335ReadBlocks,
        EfipSdAm335WriteBlocks,
        EfipSdAm335FlushBlocks
    }
};

//
// Define the device path template.
//

EFI_SD_AM335_DEVICE_PATH EfiSdAm335DevicePathTemplate = {
    {
        {
            {
                HARDWARE_DEVICE_PATH,
                HW_VENDOR_DP,
                sizeof(EFI_SD_AM335_BLOCK_IO_DEVICE_PATH)
            },

            EFI_SD_AM335_BLOCK_IO_DEVICE_PATH_GUID,
        },

        0xFFFFFFFF
    },

    {
        END_DEVICE_PATH_TYPE,
        END_ENTIRE_DEVICE_PATH_SUBTYPE,
        END_DEVICE_PATH_LENGTH
    }
};

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipBeagleBoneEnumerateStorage (
    VOID
    )

/*++

Routine Description:

    This routine enumerates the SD card and eMMC on the BeagleBone.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    BOOLEAN PeripheralBoot;
    EFI_STATUS Status;

    Status = EfipBeagleBoneEnumerateSdController(AM335_HSMMC_0_BASE, TRUE);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    PeripheralBoot = FALSE;
    if (EfiBootDeviceCode != AM335_ROM_DEVICE_MMCSD1) {
        PeripheralBoot = TRUE;
    }

    //
    // Only enumerate eMMC if the firmware was not loaded from SD. Enumerating
    // eMMC will cause NV variables to be loaded from there, which will specify
    // a BootOrder of eMMC first. The user likely didn't go to all the trouble
    // of booting via SD only to have this firmware launch the eMMC boot option.
    //

    if (PeripheralBoot == FALSE) {
        Status = EfipBeagleBoneEnumerateSdController(AM335_HSMMC_1_BASE, FALSE);
        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

EFI_STATUS
EfipBeagleBoneEnumerateSdController (
    UINT32 ControllerBase,
    BOOLEAN RemovableMedia
    )

/*++

Routine Description:

    This routine enumerates the an SD or eMMC controller on the BeagleBone.

Arguments:

    ControllerBase - Supplies the physical address of the controller to
        enumerate.

    RemovableMedia - Supplies a boolean whether or not the controller is
        connected to removable media.

Return Value:

    EFI status code.

--*/

{

    UINT64 BlockCount;
    UINT32 BlockSize;
    PEFI_SD_AM335_DEVICE_PATH DevicePath;
    PEFI_SD_AM335_CONTEXT Disk;
    EFI_SD_INITIALIZATION_BLOCK SdParameters;
    EFI_STATUS Status;

    //
    // Allocate and initialize the context structure.
    //

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_SD_AM335_CONTEXT),
                             (VOID **)&Disk);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfiCopyMem(Disk, &EfiSdAm335DiskTemplate, sizeof(EFI_SD_AM335_CONTEXT));
    Disk->Handle = NULL;
    Disk->ControllerBase = (VOID *)(UINTN)ControllerBase;
    Disk->BlockIo.Media = &(Disk->Media);
    Disk->Media.RemovableMedia = RemovableMedia;

    //
    // Create the device path.
    //

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_SD_AM335_DEVICE_PATH),
                             (VOID **)&DevicePath);

    if (EFI_ERROR(Status)) {
        goto BeagleBoneEnumerateSdEnd;
    }

    EfiCopyMem(DevicePath,
               &EfiSdAm335DevicePathTemplate,
               sizeof(EFI_SD_AM335_DEVICE_PATH));

    DevicePath->Disk.ControllerBase = ControllerBase;
    Disk->DevicePath = (EFI_DEVICE_PATH_PROTOCOL *)DevicePath;

    //
    // Create the SD controller.
    //

    EfiSetMem(&SdParameters, sizeof(EFI_SD_INITIALIZATION_BLOCK), 0);
    SdParameters.StandardControllerBase =
                 Disk->ControllerBase + SD_AM335_CONTROLLER_SD_REGISTER_OFFSET;

    SdParameters.Voltages = SD_VOLTAGE_29_30 | SD_VOLTAGE_30_31;
    SdParameters.HostCapabilities = SD_MODE_4BIT |
                                    SD_MODE_HIGH_SPEED |
                                    SD_MODE_AUTO_CMD12;

    SdParameters.FundamentalClock = SD_AM335_FUNDAMENTAL_CLOCK_SPEED;
    Disk->Controller = EfiSdCreateController(&SdParameters);
    if (Disk->Controller == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto BeagleBoneEnumerateSdEnd;
    }

    //
    // Perform some initialization to see if the card is there.
    //

    Status = EfipSdAm335ResetController(Disk);
    if (!EFI_ERROR(Status)) {
        Status = EfiSdInitializeController(Disk->Controller, FALSE);
        if (!EFI_ERROR(Status)) {
            Status = EfiSdGetMediaParameters(Disk->Controller,
                                             &BlockCount,
                                             &BlockSize);

            if (!EFI_ERROR(Status)) {
                Disk->MediaPresent = TRUE;
                Disk->BlockSize = BlockSize;
                Disk->BlockCount = BlockCount;
                Disk->Media.MediaPresent = TRUE;
                Disk->Media.BlockSize = BlockSize;
                Disk->Media.LastBlock = BlockCount - 1;
            }
        }
    }

    Status = EfiInstallMultipleProtocolInterfaces(&(Disk->Handle),
                                                  &EfiDevicePathProtocolGuid,
                                                  Disk->DevicePath,
                                                  &EfiBlockIoProtocolGuid,
                                                  &(Disk->BlockIo),
                                                  NULL);

BeagleBoneEnumerateSdEnd:
    if (EFI_ERROR(Status)) {
        if (Disk != NULL) {
            if (Disk->DevicePath != NULL) {
                EfiFreePool(DevicePath);
            }

            if (Disk->Controller != NULL) {
                EfiSdDestroyController(Disk->Controller);
            }

            EfiFreePool(Disk);
        }
    }

    return Status;
}

EFIAPI
EFI_STATUS
EfipSdAm335Reset (
    EFI_BLOCK_IO_PROTOCOL *This,
    BOOLEAN ExtendedVerification
    )

/*++

Routine Description:

    This routine resets the block device.

Arguments:

    This - Supplies a pointer to the protocol instance.

    ExtendedVerification - Supplies a boolean indicating whether or not the
        driver should perform diagnostics on reset.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

--*/

{

    PEFI_SD_AM335_CONTEXT Disk;
    EFI_STATUS Status;

    Disk = EFI_SD_AM335_FROM_THIS(This);
    Status = EfipSdAm335ResetController(Disk);
    if (!EFI_ERROR(Status)) {
        Status = EfiSdInitializeController(Disk->Controller, FALSE);
    }

    if (EFI_ERROR(Status)) {
        Disk->MediaPresent = FALSE;
        Disk->Media.MediaPresent = FALSE;

    } else {
        Disk->Media.MediaId += 1;
        Disk->Media.MediaPresent = TRUE;
        Disk->MediaPresent = TRUE;
    }

    return Status;
}

EFIAPI
EFI_STATUS
EfipSdAm335ReadBlocks (
    EFI_BLOCK_IO_PROTOCOL *This,
    UINT32 MediaId,
    EFI_LBA Lba,
    UINTN BufferSize,
    VOID *Buffer
    )

/*++

Routine Description:

    This routine performs a block I/O read from the device.

Arguments:

    This - Supplies a pointer to the protocol instance.

    MediaId - Supplies the media identifier, which changes each time the media
        is replaced.

    Lba - Supplies the logical block address of the read.

    BufferSize - Supplies the size of the buffer in bytes.

    Buffer - Supplies the buffer where the read data will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_NO_MEDIA if there is no media in the device.

    EFI_MEDIA_CHANGED if the media ID does not match the current device.

    EFI_BAD_BUFFER_SIZE if the buffer was not a multiple of the device block
    size.

    EFI_INVALID_PARAMETER if the read request contains LBAs that are not valid,
    or the buffer is not properly aligned.

--*/

{

    PEFI_SD_AM335_CONTEXT Disk;
    EFI_STATUS Status;

    Disk = EFI_SD_AM335_FROM_THIS(This);
    if (MediaId != Disk->Media.MediaId) {
        return EFI_MEDIA_CHANGED;
    }

    if ((Disk->MediaPresent == FALSE) || (Disk->BlockSize == 0)) {
        return EFI_NO_MEDIA;
    }

    EfipBeagleBoneBlackSetLeds(3);
    Status = EfiSdBlockIoPolled(Disk->Controller,
                                Lba,
                                BufferSize / Disk->BlockSize,
                                Buffer,
                                FALSE);

    EfipBeagleBoneBlackSetLeds(1);
    return Status;
}

EFIAPI
EFI_STATUS
EfipSdAm335WriteBlocks (
    EFI_BLOCK_IO_PROTOCOL *This,
    UINT32 MediaId,
    EFI_LBA Lba,
    UINTN BufferSize,
    VOID *Buffer
    )

/*++

Routine Description:

    This routine performs a block I/O write to the device.

Arguments:

    This - Supplies a pointer to the protocol instance.

    MediaId - Supplies the media identifier, which changes each time the media
        is replaced.

    Lba - Supplies the logical block address of the write.

    BufferSize - Supplies the size of the buffer in bytes.

    Buffer - Supplies the buffer containing the data to write.

Return Value:

    EFI_SUCCESS on success.

    EFI_WRITE_PROTECTED if the device cannot be written to.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_NO_MEDIA if there is no media in the device.

    EFI_MEDIA_CHANGED if the media ID does not match the current device.

    EFI_BAD_BUFFER_SIZE if the buffer was not a multiple of the device block
    size.

    EFI_INVALID_PARAMETER if the read request contains LBAs that are not valid,
    or the buffer is not properly aligned.

--*/

{

    PEFI_SD_AM335_CONTEXT Disk;
    EFI_STATUS Status;

    Disk = EFI_SD_AM335_FROM_THIS(This);
    if (MediaId != Disk->Media.MediaId) {
        return EFI_MEDIA_CHANGED;
    }

    if ((Disk->MediaPresent == FALSE) || (Disk->BlockSize == 0)) {
        return EFI_NO_MEDIA;
    }

    EfipBeagleBoneBlackSetLeds(3);
    Status = EfiSdBlockIoPolled(Disk->Controller,
                                Lba,
                                BufferSize / Disk->BlockSize,
                                Buffer,
                                TRUE);

    EfipBeagleBoneBlackSetLeds(1);
    return Status;
}

EFIAPI
EFI_STATUS
EfipSdAm335FlushBlocks (
    EFI_BLOCK_IO_PROTOCOL *This
    )

/*++

Routine Description:

    This routine flushes the block device.

Arguments:

    This - Supplies a pointer to the protocol instance.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_NO_MEDIA if there is no media in the device.

--*/

{

    return EFI_SUCCESS;
}

EFI_STATUS
EfipSdAm335ResetController (
    PEFI_SD_AM335_CONTEXT Device
    )

/*++

Routine Description:

    This routine resets the OMAP4 SD controller and card.

Arguments:

    Device - Supplies a pointer to this SD OMAP4 device.

Return Value:

    Status code.

--*/

{

    UINT32 ClockControl;
    UINT32 Divisor;
    UINT32 Register;
    EFI_STATUS Status;
    UINT64 Time;
    UINT64 Timeout;
    UINT32 Value;

    //
    // Perform a soft reset on the HSMMC part.
    //

    SD_AM335_WRITE_REGISTER(Device,
                            SD_AM335_SYSCONFIG_REGISTER,
                            SD_AM335_SYSCONFIG_SOFT_RESET);

    Status = EFI_TIMEOUT;
    Time = 0;
    Timeout = EFI_SD_AM335_TIMEOUT;
    do {
        if ((SD_AM335_READ_REGISTER(Device, SD_AM335_SYSSTATUS_REGISTER) &
             SD_AM335_SYSSTATUS_RESET_DONE) != 0) {

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
    // Perform a reset on the SD controller.
    //

    Register = SD_AM335_CONTROLLER_SD_REGISTER_OFFSET + SdRegisterClockControl;
    Value = SD_AM335_READ_REGISTER(Device, Register);
    Value |= SD_CLOCK_CONTROL_RESET_ALL;
    Status = EFI_TIMEOUT;
    Time = 0;
    Timeout = EFI_SD_AM335_TIMEOUT;
    do {
        if ((SD_AM335_READ_REGISTER(Device, Register) &
             SD_CLOCK_CONTROL_RESET_ALL) == 0) {

            Status = EFI_SUCCESS;
            break;
        }

        EfiStall(50);
        Time += 50;

    } while (Time <= Timeout);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Register = SD_AM335_CONTROLLER_SD_REGISTER_OFFSET +
               SdRegisterInterruptStatus;

    SD_AM335_WRITE_REGISTER(Device, Register, 0xFFFFFFFF);

    //
    // Set up the host control register for 3 Volts.
    //

    Register = SD_AM335_CONTROLLER_SD_REGISTER_OFFSET + SdRegisterHostControl;
    Value = SD_HOST_CONTROL_POWER_3V0;
    SD_AM335_WRITE_REGISTER(Device, Register, Value);

    //
    // Add the 3.0V and 1.8V capabilities to the capability register.
    //

    Register = SD_AM335_CONTROLLER_SD_REGISTER_OFFSET + SdRegisterCapabilities;
    Value = SD_AM335_READ_REGISTER(Device, Register);
    Value |= SD_CAPABILITY_VOLTAGE_3V0 | SD_CAPABILITY_VOLTAGE_1V8;
    SD_AM335_WRITE_REGISTER(Device, Register, Value);

    //
    // Initialize the HSMMC control register.
    //

    Register = SD_AM335_CON_REGISTER;
    Value = SD_AM335_READ_REGISTER(Device, Register) &
            SD_AM335_CON_DEBOUNCE_MASK;

    SD_AM335_WRITE_REGISTER(Device, Register, Value);

    //
    // Set up the clock control register for 400kHz in preparation for sending
    // CMD0 with INIT held.
    //

    Register = SD_AM335_CONTROLLER_SD_REGISTER_OFFSET + SdRegisterClockControl;
    ClockControl = SD_CLOCK_CONTROL_DEFAULT_TIMEOUT <<
                   SD_CLOCK_CONTROL_TIMEOUT_SHIFT;

    SD_AM335_WRITE_REGISTER(Device, Register, ClockControl);
    Divisor = SD_AM335_INITIAL_DIVISOR;
    ClockControl |= (Divisor & SD_CLOCK_CONTROL_DIVISOR_MASK) <<
                    SD_CLOCK_CONTROL_DIVISOR_SHIFT;

    ClockControl |= (Divisor & SD_CLOCK_CONTROL_DIVISOR_HIGH_MASK) >>
                    SD_CLOCK_CONTROL_DIVISOR_HIGH_SHIFT;

    ClockControl |= SD_CLOCK_CONTROL_INTERNAL_CLOCK_ENABLE;
    SD_AM335_WRITE_REGISTER(Device, Register, ClockControl);
    Status = EFI_TIMEOUT;
    Time = 0;
    Timeout = EFI_SD_AM335_TIMEOUT;
    do {
        Value = SD_AM335_READ_REGISTER(Device, Register);
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
    SD_AM335_WRITE_REGISTER(Device, Register, ClockControl);
    SD_AM335_WRITE_REGISTER(Device, Register, ClockControl);
    Register = SD_AM335_CONTROLLER_SD_REGISTER_OFFSET + SdRegisterHostControl;
    Value = SD_AM335_READ_REGISTER(Device, Register);
    Value |= SD_HOST_CONTROL_POWER_ENABLE;
    SD_AM335_WRITE_REGISTER(Device, Register, Value);
    Register = SD_AM335_CONTROLLER_SD_REGISTER_OFFSET +
               SdRegisterInterruptStatusEnable;

    Value = SD_INTERRUPT_STATUS_ENABLE_DEFAULT_MASK;
    SD_AM335_WRITE_REGISTER(Device, Register, Value);

    //
    // Reset the card by setting the init flag and issuing the card reset (go
    // idle, command 0) command.
    //

    Register = SD_AM335_CON_REGISTER;
    Value = SD_AM335_READ_REGISTER(Device, Register) | SD_AM335_CON_INIT |
            SD_AM335_CON_DMA_MASTER;

    SD_AM335_WRITE_REGISTER(Device, Register, Value);

    //
    // Write a 0 to the command register to issue the command.
    //

    Register = SD_AM335_CONTROLLER_SD_REGISTER_OFFSET + SdRegisterCommand;
    SD_AM335_WRITE_REGISTER(Device, Register, 0);

    //
    // Wait for the command to complete.
    //

    Register = SD_AM335_CONTROLLER_SD_REGISTER_OFFSET +
               SdRegisterInterruptStatus;

    Status = EFI_TIMEOUT;
    Time = 0;
    Timeout = EFI_SD_AM335_TIMEOUT;
    do {
        Value = SD_AM335_READ_REGISTER(Device, Register);
        if (Value != 0) {
            if ((Value & SD_INTERRUPT_STATUS_COMMAND_COMPLETE) != 0) {
                Status = EFI_SUCCESS;

            } else if ((Value &
                        SD_INTERRUPT_STATUS_COMMAND_TIMEOUT_ERROR) != 0) {

                Status = EFI_NO_MEDIA;

            } else {
                Status = EFI_DEVICE_ERROR;
            }

            SD_AM335_WRITE_REGISTER(Device, Register, Value);
            break;
        }

        EfiStall(50);
        Time += 50;

    } while (Time <= Timeout);

    //
    // Disable the INIT line.
    //

    Register = SD_AM335_CON_REGISTER;
    Value = SD_AM335_READ_REGISTER(Device, Register) & (~SD_AM335_CON_INIT);
    SD_AM335_WRITE_REGISTER(Device, Register, Value);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EFI_SUCCESS;
    return Status;
}

