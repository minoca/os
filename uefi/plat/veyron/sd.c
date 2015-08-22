/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    sd.c

Abstract:

    This module implements RK3288 SD support in UEFI.

Author:

    Chris Stevens 14-Jul-2015

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include <dev/sddwc.h>
#include <minoca/uefi/protocol/blockio.h>
#include "veyronfw.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro returns a pointer to the disk I/O data given a pointer to the
// block I/O protocol instance.
//

#define EFI_SD_RK32_FROM_THIS(_BlockIo)          \
    (EFI_SD_RK32_CONTEXT *)((VOID *)(_BlockIo) - \
                            ((VOID *)(&(((EFI_SD_RK32_CONTEXT *)0)->BlockIo))))

//
// These macros read and write SD controller registers.
//

#define SD_RK32_READ_REGISTER(_Device, _Register) \
    EfiReadRegister32((_Device)->ControllerBase + (_Register))

#define SD_RK32_WRITE_REGISTER(_Device, _Register, _Value) \
    EfiWriteRegister32((_Device)->ControllerBase + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_SD_RK32_MAGIC 0x6B526453 // 'kRdS'

#define EFI_SD_RK32_BLOCK_IO_DEVICE_PATH_GUID              \
    {                                                      \
        0xCF31FAC5, 0xC24E, 0x11D2,                        \
        {0x85, 0xF3, 0x00, 0xA0, 0xC9, 0x3E, 0xA7, 0x39}   \
    }

//
// Define the amount of time to wait in microseconds for the controller to
// respond.
//

#define EFI_SD_RK32_TIMEOUT 1000000

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes the SD RK32 device context.

Members:

    Magic - Stores the magic constand EFI_SD_RK32_MAGIC.

    Handle - Stores the handle to the block I/O device.

    DevicePath - Stores a pointer to the device path.

    Controller - Stores an pointer to the controller structure.

    ControllerBase - Stores a pointer to the virtual address of the HSMMC
        registers.

    FundamentalClock - Stores the fundamental clock for the HSMMC device in
        Hertz.

    MediaPresent - Stores a boolean indicating whether or not there is a card
        in the slot.

    BlockSize - Stores the cached block size of the media.

    BlockCount - Stores the cached block count of the media.

    BlockIo - Stores the block I/O protocol.

    Media - Stores the block I/O media information.

--*/

typedef struct _EFI_SD_RK32_CONTEXT {
    UINT32 Magic;
    EFI_HANDLE Handle;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    PEFI_SD_DWC_CONTROLLER Controller;
    VOID *ControllerBase;
    UINT32 FundamentalClock;
    BOOLEAN MediaPresent;
    UINT32 BlockSize;
    UINT64 BlockCount;
    EFI_BLOCK_IO_PROTOCOL BlockIo;
    EFI_BLOCK_IO_MEDIA Media;
} EFI_SD_RK32_CONTEXT, *PEFI_SD_RK32_CONTEXT;

/*++

Structure Description:

    This structure defines the SD RK32 block I/O device path.

Members:

    DevicePath - Stores the standard vendor-specific device path.

    ControllerBase - Stores the controller number.

--*/

typedef struct _EFI_SD_RK32_BLOCK_IO_DEVICE_PATH {
    VENDOR_DEVICE_PATH DevicePath;
    UINT32 ControllerBase;
} EFI_SD_RK32_BLOCK_IO_DEVICE_PATH, *PEFI_SD_RK32_BLOCK_IO_DEVICE_PATH;

/*++

Structure Description:

    This structure defines the RK32 SD block I/O device path.

Members:

    Disk - Stores the disk device path node.

    End - Stores the end device path node.

--*/

typedef struct _EFI_SD_RK32_DEVICE_PATH {
    EFI_SD_RK32_BLOCK_IO_DEVICE_PATH Disk;
    EFI_DEVICE_PATH_PROTOCOL End;
} PACKED EFI_SD_RK32_DEVICE_PATH, *PEFI_SD_RK32_DEVICE_PATH;

//
// ----------------------------------------------- Internal Function Prototypes
//

EFIAPI
EFI_STATUS
EfipSdRk32Reset (
    EFI_BLOCK_IO_PROTOCOL *This,
    BOOLEAN ExtendedVerification
    );

EFIAPI
EFI_STATUS
EfipSdRk32ReadBlocks (
    EFI_BLOCK_IO_PROTOCOL *This,
    UINT32 MediaId,
    EFI_LBA Lba,
    UINTN BufferSize,
    VOID *Buffer
    );

EFIAPI
EFI_STATUS
EfipSdRk32WriteBlocks (
    EFI_BLOCK_IO_PROTOCOL *This,
    UINT32 MediaId,
    EFI_LBA Lba,
    UINTN BufferSize,
    VOID *Buffer
    );

EFIAPI
EFI_STATUS
EfipSdRk32FlushBlocks (
    EFI_BLOCK_IO_PROTOCOL *This
    );

EFI_STATUS
EfipSdRk32GetFundamentalClock (
    PEFI_SD_RK32_CONTEXT Device,
    UINT32 *FundamentalClock
    );

EFI_STATUS
EfipSdRk32HardResetController (
    PEFI_SD_RK32_CONTEXT Device
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the private data template.
//

EFI_SD_RK32_CONTEXT EfiSdRk32DiskTemplate = {
    EFI_SD_RK32_MAGIC,
    NULL,
    NULL,
    NULL,
    NULL,
    0,
    FALSE,
    0,
    0,
    {
        EFI_BLOCK_IO_PROTOCOL_REVISION3,
        NULL,
        EfipSdRk32Reset,
        EfipSdRk32ReadBlocks,
        EfipSdRk32WriteBlocks,
        EfipSdRk32FlushBlocks
    }
};

//
// Define the device path template.
//

EFI_SD_RK32_DEVICE_PATH EfiSdRk32DevicePathTemplate = {
    {
        {
            {
                HARDWARE_DEVICE_PATH,
                HW_VENDOR_DP,
                sizeof(EFI_SD_RK32_BLOCK_IO_DEVICE_PATH)
            },

            EFI_SD_RK32_BLOCK_IO_DEVICE_PATH_GUID,
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
EfipVeyronEnumerateSd (
    VOID
    )

/*++

Routine Description:

    This routine enumerates the SD card on the Veyron SoC.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    UINT64 BlockCount;
    UINT32 BlockSize;
    UINT32 ControllerBase;
    PEFI_SD_RK32_DEVICE_PATH DevicePath;
    PEFI_SD_RK32_CONTEXT Disk;
    EFI_SD_DWC_INITIALIZATION_BLOCK SdDwcParameters;
    EFI_STATUS Status;

    ControllerBase = RK32_SD_BASE;

    //
    // Allocate and initialize the context structure.
    //

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_SD_RK32_CONTEXT),
                             (VOID **)&Disk);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfiCopyMem(Disk, &EfiSdRk32DiskTemplate, sizeof(EFI_SD_RK32_CONTEXT));
    Disk->Handle = NULL;
    Disk->ControllerBase = (VOID *)(UINTN)ControllerBase;
    Disk->BlockIo.Media = &(Disk->Media);
    Disk->Media.RemovableMedia = TRUE;

    //
    // Create the device path.
    //

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_SD_RK32_DEVICE_PATH),
                             (VOID **)&DevicePath);

    if (EFI_ERROR(Status)) {
        goto VeyronEnumerateSdEnd;
    }

    EfiCopyMem(DevicePath,
               &EfiSdRk32DevicePathTemplate,
               sizeof(EFI_SD_RK32_DEVICE_PATH));

    DevicePath->Disk.ControllerBase = ControllerBase;
    Disk->DevicePath = (EFI_DEVICE_PATH_PROTOCOL *)DevicePath;
    Status = EfipSdRk32GetFundamentalClock(Disk, &(Disk->FundamentalClock));
    if (EFI_ERROR(Status)) {
        goto VeyronEnumerateSdEnd;
    }

    //
    // Create the SD controller.
    //

    EfiSetMem(&SdDwcParameters, sizeof(EFI_SD_DWC_INITIALIZATION_BLOCK), 0);
    SdDwcParameters.ControllerBase = Disk->ControllerBase;
    SdDwcParameters.Voltages = SD_VOLTAGE_165_195 |
                               SD_VOLTAGE_32_33 |
                               SD_VOLTAGE_33_34;

    SdDwcParameters.HostCapabilities = SD_MODE_4BIT |
                                       SD_MODE_HIGH_SPEED |
                                       SD_MODE_AUTO_CMD12;

    SdDwcParameters.FundamentalClock = Disk->FundamentalClock;
    Disk->Controller = EfiSdDwcCreateController(&SdDwcParameters);
    if (Disk->Controller == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto VeyronEnumerateSdEnd;
    }

    //
    // Reset the controller.
    //

    Status = EfipSdRk32HardResetController(Disk);
    if (EFI_ERROR(Status)) {
        goto VeyronEnumerateSdEnd;
    }

    //
    // Perform some initialization to see if the card is there.
    //

    Status = EfiSdDwcInitializeController(Disk->Controller, FALSE);
    if (!EFI_ERROR(Status)) {
        Status = EfiSdDwcGetMediaParameters(Disk->Controller,
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

    Status = EfiInstallMultipleProtocolInterfaces(&(Disk->Handle),
                                                  &EfiDevicePathProtocolGuid,
                                                  Disk->DevicePath,
                                                  &EfiBlockIoProtocolGuid,
                                                  &(Disk->BlockIo),
                                                  NULL);

VeyronEnumerateSdEnd:
    if (EFI_ERROR(Status)) {
        if (Disk != NULL) {
            if (Disk->DevicePath != NULL) {
                EfiFreePool(DevicePath);
            }

            if (Disk->Controller != NULL) {
                EfiSdDwcDestroyController(Disk->Controller);
            }

            EfiFreePool(Disk);
        }
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

EFIAPI
EFI_STATUS
EfipSdRk32Reset (
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

    PEFI_SD_RK32_CONTEXT Disk;
    EFI_STATUS Status;

    Disk = EFI_SD_RK32_FROM_THIS(This);
    Status = EfiSdDwcInitializeController(Disk->Controller, TRUE);
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
EfipSdRk32ReadBlocks (
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

    PEFI_SD_RK32_CONTEXT Disk;
    EFI_STATUS Status;

    Disk = EFI_SD_RK32_FROM_THIS(This);
    if (MediaId != Disk->Media.MediaId) {
        return EFI_MEDIA_CHANGED;
    }

    if ((Disk->MediaPresent == FALSE) || (Disk->BlockSize == 0)) {
        return EFI_NO_MEDIA;
    }

    Status = EfiSdDwcBlockIoPolled(Disk->Controller,
                                   Lba,
                                   BufferSize / Disk->BlockSize,
                                   Buffer,
                                   FALSE);

    return Status;
}

EFIAPI
EFI_STATUS
EfipSdRk32WriteBlocks (
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

    PEFI_SD_RK32_CONTEXT Disk;
    EFI_STATUS Status;

    Disk = EFI_SD_RK32_FROM_THIS(This);
    if (MediaId != Disk->Media.MediaId) {
        return EFI_MEDIA_CHANGED;
    }

    if ((Disk->MediaPresent == FALSE) || (Disk->BlockSize == 0)) {
        return EFI_NO_MEDIA;
    }

    Status = EfiSdDwcBlockIoPolled(Disk->Controller,
                                   Lba,
                                   BufferSize / Disk->BlockSize,
                                   Buffer,
                                   TRUE);

    return Status;
}

EFIAPI
EFI_STATUS
EfipSdRk32FlushBlocks (
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
EfipSdRk32GetFundamentalClock (
    PEFI_SD_RK32_CONTEXT Device,
    UINT32 *FundamentalClock
    )

/*++

Routine Description:

    This routine gets the fundamental clock frequency to use for the SD
    controller.

Arguments:

    Device - Supplies a pointer to this SD RK32 device.

    FundamentalClock - Supplies a pointer that receives the frequency of the
        fundamental clock, in Hertz.

Return Value:

    Status code.

--*/

{

    UINT32 ClockSource;
    VOID *CruBase;
    UINT32 Divisor;
    UINT32 Frequency;
    EFI_STATUS Status;
    UINT32 Value;

    CruBase = (VOID *)RK32_CRU_BASE;

    //
    // Read the current MMC0 clock source.
    //

    Value = EfiReadRegister32(CruBase + Rk32CruClockSelect11);
    ClockSource = (Value & RK32_CRU_CLOCK_SELECT11_MMC0_CLOCK_MASK) >>
                  RK32_CRU_CLOCK_SELECT11_MMC0_CLOCK_SHIFT;

    Divisor = (Value & RK32_CRU_CLOCK_SELECT11_MMC0_DIVIDER_MASK) >>
              RK32_CRU_CLOCK_SELECT11_MMC0_DIVIDER_SHIFT;

    Divisor += 1;

    //
    // Get the fundamental clock frequency base on the source.
    //

    switch (ClockSource) {
    case RK32_CRU_CLOCK_SELECT11_MMC0_CODEC_PLL:
        Status = EfipRk32GetPllClockFrequency(Rk32PllCodec, &Frequency);
        if (EFI_ERROR(Status)) {
            return Status;
        }

        break;

    case RK32_CRU_CLOCK_SELECT11_MMC0_GENERAL_PLL:
        Status = EfipRk32GetPllClockFrequency(Rk32PllGeneral, &Frequency);
        if (EFI_ERROR(Status)) {
            return Status;
        }

        break;

    case RK32_CRU_CLOCK_SELECT11_MMC0_24MHZ:
        Frequency = RK32_SDMMC_FREQUENCY_24MHZ;
        break;

    default:
        return EFI_DEVICE_ERROR;
    }

    //
    // To get the MMC0 clock speed, the clock source frequency must be divided
    // by the divisor.
    //

    *FundamentalClock = Frequency / Divisor;
    return EFI_SUCCESS;
}

EFI_STATUS
EfipSdRk32HardResetController (
    PEFI_SD_RK32_CONTEXT Device
    )

/*++

Routine Description:

    This routine resets the DesignWare SD controller and card.

Arguments:

    Device - Supplies a pointer to this SD RK32 device.

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

    SD_RK32_WRITE_REGISTER(Device, SdDwcPower, SD_DWC_POWER_DISABLE);
    SD_RK32_WRITE_REGISTER(Device, SdDwcResetN, SD_DWC_RESET_ENABLE);
    EfiStall(5000);
    SD_RK32_WRITE_REGISTER(Device, SdDwcPower, SD_DWC_POWER_ENABLE);
    SD_RK32_WRITE_REGISTER(Device, SdDwcResetN, 0);
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

    ResetMask = SD_DWC_CONTROL_FIFO_RESET |
                SD_DWC_CONTROL_CONTROLLER_RESET;

    SD_RK32_WRITE_REGISTER(Device, SdDwcControl, ResetMask);
    Status = EFI_TIMEOUT;
    Timeout = EFI_SD_RK32_TIMEOUT;
    Time = 0;
    do {
        Value = SD_RK32_READ_REGISTER(Device, SdDwcControl);
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

    SD_RK32_WRITE_REGISTER(Device,
                           SdDwcInterruptStatus,
                           SD_DWC_INTERRUPT_STATUS_ALL_MASK);

    //
    // Set 3v3 volts in the UHS register.
    //

    SD_RK32_WRITE_REGISTER(Device, SdDwcUhs, SD_DWC_UHS_VOLTAGE_3V3);

    //
    // Set the clock to 400kHz in preparation for sending CMD0 with the
    // initialization bit set.
    //

    Status = EfiSdDwcSetClockSpeed(Device->Controller, 400000);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Reset the card by sending the CMD0 reset command with the initialization
    // bit set.
    //

    Value = SD_DWC_COMMAND_START |
            SD_DWC_COMMAND_USE_HOLD_REGISTER |
            SD_DWC_COMMAND_SEND_INITIALIZATION;

    SD_RK32_WRITE_REGISTER(Device, SdDwcCommand, Value);

    //
    // Wait for the command to complete.
    //

    Status = EFI_TIMEOUT;
    Timeout = EFI_SD_RK32_TIMEOUT;
    Time = 0;
    do {
        Value = SD_RK32_READ_REGISTER(Device, SdDwcCommand);
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

    Timeout = EFI_SD_RK32_TIMEOUT;
    Time = 0;
    Status = EFI_TIMEOUT;
    do {
        Value = SD_RK32_READ_REGISTER(Device, SdDwcInterruptStatus);
        if (Value != 0) {
            if ((Value & SD_DWC_INTERRUPT_STATUS_COMMAND_DONE) != 0) {
                Status = EFI_SUCCESS;

            } else if ((Value &
                        SD_DWC_INTERRUPT_STATUS_ERROR_RESPONSE_TIMEOUT) != 0) {

                Status = EFI_NO_MEDIA;

            } else {
                Status = EFI_DEVICE_ERROR;
            }

            SD_RK32_WRITE_REGISTER(Device, SdDwcInterruptStatus, Value);
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

