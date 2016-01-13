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
#include <dev/sdrk.h>
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
// ---------------------------------------------------------------- Definitions
//

#define EFI_SD_RK32_MAGIC 0x6B526453 // 'kRdS'

#define EFI_SD_RK32_BLOCK_IO_DEVICE_PATH_GUID              \
    {                                                      \
        0xCF31FAC5, 0xC24E, 0x11D2,                        \
        {0x85, 0xF3, 0x00, 0xA0, 0xC9, 0x3E, 0xA7, 0x39}   \
    }

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
    PEFI_SD_RK_CONTROLLER Controller;
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

UINT32
EfipSdRk32GetFundamentalClock (
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
    EFI_SD_RK_INITIALIZATION_BLOCK SdRkParameters;
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
    Disk->FundamentalClock = EfipSdRk32GetFundamentalClock(Disk);

    //
    // Create the SD controller.
    //

    EfiSetMem(&SdRkParameters, sizeof(EFI_SD_RK_INITIALIZATION_BLOCK), 0);
    SdRkParameters.ControllerBase = Disk->ControllerBase;
    SdRkParameters.Voltages = SD_VOLTAGE_165_195 |
                              SD_VOLTAGE_32_33 |
                              SD_VOLTAGE_33_34;

    SdRkParameters.HostCapabilities = SD_MODE_4BIT |
                                      SD_MODE_HIGH_SPEED |
                                      SD_MODE_AUTO_CMD12;

    SdRkParameters.FundamentalClock = Disk->FundamentalClock;
    Disk->Controller = EfiSdRkCreateController(&SdRkParameters);
    if (Disk->Controller == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto VeyronEnumerateSdEnd;
    }

    //
    // Perform some initialization to see if the card is there.
    //

    Status = EfiSdRkInitializeController(Disk->Controller, TRUE, FALSE);
    if (!EFI_ERROR(Status)) {
        Status = EfiSdRkGetMediaParameters(Disk->Controller,
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
                EfiSdRkDestroyController(Disk->Controller);
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
    Status = EfiSdRkInitializeController(Disk->Controller, FALSE, TRUE);
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

    Status = EfiSdRkBlockIoPolled(Disk->Controller,
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

    Status = EfiSdRkBlockIoPolled(Disk->Controller,
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

UINT32
EfipSdRk32GetFundamentalClock (
    PEFI_SD_RK32_CONTEXT Device
    )

/*++

Routine Description:

    This routine gets the fundamental clock frequency to use for the SD
    controller.

Arguments:

    Device - Supplies a pointer to this SD RK32 device.

Return Value:

    Returns the value of the fundamental clock frequency in Hertz.

--*/

{

    UINT32 ClockSource;
    VOID *CruBase;
    UINT32 Divisor;
    UINT32 Frequency;
    UINT32 Mode;
    UINT32 Nf;
    UINT32 No;
    UINT32 Nr;
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
        Mode = EfiReadRegister32(CruBase + Rk32CruModeControl);
        Mode = (Mode & RK32_CRU_MODE_CONTROL_CODEC_PLL_MODE_MASK) >>
                RK32_CRU_MODE_CONTROL_CODEC_PLL_MODE_SHIFT;

        if (Mode == RK32_CRU_MODE_CONTROL_SLOW_MODE) {
            Frequency = RK32_CRU_PLL_SLOW_MODE_FREQUENCY;

        } else if (Mode == RK32_CRU_MODE_CONTROL_NORMAL_MODE) {

            //
            // Calculate the clock speed based on the formula described in
            // section 3.9 of the RK3288 TRM.
            //

            Value = EfiReadRegister32(CruBase + Rk32CruCodecPllControl0);
            No = (Value & RK32_CRU_CODEC_PLL_CONTROL0_CLKOD_MASK) >>
                 RK32_CRU_CODEC_PLL_CONTROL0_CLKOD_SHIFT;

            No += 1;
            Nr = (Value & RK32_CRU_CODEC_PLL_CONTROL0_CLKR_MASK) >>
                 RK32_CRU_CODEC_PLL_CONTROL0_CLKR_SHIFT;

            Nr += 1;
            Value = EfiReadRegister32(CruBase + Rk32CruCodecPllControl1);
            Nf = (Value & RK32_CRU_CODEC_PLL_CONTROL1_CLKF_MASK) >>
                 RK32_CRU_CODEC_PLL_CONTROL1_CLKF_SHIFT;

            Nf += 1;
            Frequency = RK32_CRU_PLL_COMPUTE_CLOCK_FREQUENCY(Nf, Nr, No);

        } else if (Mode == RK32_CRU_MODE_CONTROL_DEEP_SLOW_MODE) {
            Frequency = RK32_CRU_PLL_DEEP_SLOW_MODE_FREQUENCY;

        } else {
            return EFI_DEVICE_ERROR;
        }

        break;

    case RK32_CRU_CLOCK_SELECT11_MMC0_GENERAL_PLL:
        Mode = EfiReadRegister32(CruBase + Rk32CruModeControl);
        Mode = (Mode & RK32_CRU_MODE_CONTROL_GENERAL_PLL_MODE_MASK) >>
                RK32_CRU_MODE_CONTROL_GENERAL_PLL_MODE_SHIFT;

        if (Mode == RK32_CRU_MODE_CONTROL_SLOW_MODE) {
            Frequency = RK32_CRU_PLL_SLOW_MODE_FREQUENCY;

        } else if (Mode == RK32_CRU_MODE_CONTROL_NORMAL_MODE) {

            //
            // Calculate the clock speed based on the formula described in
            // section 3.9 of the RK3288 TRM.
            //

            Value = EfiReadRegister32(CruBase + Rk32CruGeneralPllControl0);
            No = (Value & RK32_CRU_GENERAL_PLL_CONTROL0_CLKOD_MASK) >>
                 RK32_CRU_GENERAL_PLL_CONTROL0_CLKOD_SHIFT;

            No += 1;
            Nr = (Value & RK32_CRU_GENERAL_PLL_CONTROL0_CLKR_MASK) >>
                 RK32_CRU_GENERAL_PLL_CONTROL0_CLKR_SHIFT;

            Nr += 1;
            Value = EfiReadRegister32(CruBase + Rk32CruGeneralPllControl1);
            Nf = (Value & RK32_CRU_GENERAL_PLL_CONTROL1_CLKF_MASK) >>
                 RK32_CRU_GENERAL_PLL_CONTROL1_CLKF_SHIFT;

            Nf += 1;
            Frequency = RK32_CRU_PLL_COMPUTE_CLOCK_FREQUENCY(Nf, Nr, No);

        } else if (Mode == RK32_CRU_MODE_CONTROL_DEEP_SLOW_MODE) {
            Frequency = RK32_CRU_PLL_DEEP_SLOW_MODE_FREQUENCY;

        } else {
            return EFI_DEVICE_ERROR;
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

    Frequency /= Divisor;
    return Frequency;
}

