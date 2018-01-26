/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sd.c

Abstract:

    This module implements BCM2709 SD support in UEFI.

Author:

    Chris Stevens 5-Jan-2015

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include <dev/sd.h>
#include <dev/bcm2709.h>
#include <minoca/uefi/protocol/blockio.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro returns a pointer to the disk I/O data given a pointer to the
// block I/O protocol instance.
//

#define EFI_SD_BCM2709_FROM_THIS(_BlockIo)                 \
        (EFI_SD_BCM2709_CONTEXT *)((VOID *)(_BlockIo) -    \
                        ((VOID *)(&(((EFI_SD_BCM2709_CONTEXT *)0)->BlockIo))))

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_SD_BCM2709_MAGIC 0x32426453 // '2BdS'

#define EFI_SD_BCM2709_BLOCK_IO_DEVICE_PATH_GUID         \
    {                                                    \
        0xFCA216DE, 0x950E, 0x11E4,                      \
        {0xBD, 0x11, 0x04, 0x01, 0x0F, 0xDD, 0x74, 0x01} \
    }

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the data necessary to enable the eMMC.

Members:

    Header - Stores a header that defines the total size of the messages being
        sent to and received from the mailbox.

    DeviceState - Stores a request to set the state for a particular device.

    EndTag - Stores the tag to denote the end of the mailbox message.

--*/

typedef struct _EFI_SD_BCM2709_ENABLE_EMMC {
    BCM2709_MAILBOX_HEADER Header;
    BCM2709_MAILBOX_DEVICE_STATE DeviceState;
    UINT32 EndTag;
} EFI_SD_BCM2709_ENABLE_EMMC, *PEFI_SD_BCM2709_ENABLE_EMMC;

/*++

Structure Description:

    This structure defines the data necessary to get the eMMC clock rate in Hz.

Members:

    Header - Stores a header that defines the total size of the messages being
        sent to and received from the mailbox.

    ClockRate - Stores a request to get the rate for a particular clock.

    EndTag - Stores the tag to denote the end of the mailbox message.

--*/

typedef struct _EFI_SD_BCM2709_GET_EMMC_CLOCK {
    BCM2709_MAILBOX_HEADER Header;
    BCM2709_MAILBOX_GET_CLOCK_RATE ClockRate;
    UINT32 EndTag;
} EFI_SD_BCM2709_GET_EMMC_CLOCK, *PEFI_SD_BCM2709_GET_EMMC_CLOCK;

/*++

Structure Description:

    This structure describes the SD BCM2709 device context.

Members:

    Magic - Stores the magic constand EFI_SD_BCM2709_MAGIC.

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

typedef struct _EFI_SD_BCM2709_CONTEXT {
    UINT32 Magic;
    EFI_HANDLE Handle;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    PEFI_SD_CONTROLLER Controller;
    BOOLEAN MediaPresent;
    UINT32 BlockSize;
    UINT64 BlockCount;
    EFI_BLOCK_IO_PROTOCOL BlockIo;
    EFI_BLOCK_IO_MEDIA Media;
} EFI_SD_BCM2709_CONTEXT, *PEFI_SD_BCM2709_CONTEXT;

/*++

Structure Description:

    This structure defines the SD BCM2709 block I/O device path.

Members:

    DevicePath - Stores the standard vendor-specific device path.

    ControllerBase - Stores the controller number.

--*/

typedef struct _EFI_SD_BCM2709_BLOCK_IO_DEVICE_PATH {
    VENDOR_DEVICE_PATH DevicePath;
    UINT32 ControllerBase;
} EFI_SD_BCM2709_BLOCK_IO_DEVICE_PATH, *PEFI_SD_BCM2709_BLOCK_IO_DEVICE_PATH;

/*++

Structure Description:

    This structure defines the BCM2709 SD block I/O device path.

Members:

    Disk - Stores the disk device path node.

    End - Stores the end device path node.

--*/

#pragma pack(push, 1)

typedef struct _EFI_SD_BCM2709_DEVICE_PATH {
    EFI_SD_BCM2709_BLOCK_IO_DEVICE_PATH Disk;
    EFI_DEVICE_PATH_PROTOCOL End;
} PACKED EFI_SD_BCM2709_DEVICE_PATH, *PEFI_SD_BCM2709_DEVICE_PATH;

#pragma pack(pop)

//
// ----------------------------------------------- Internal Function Prototypes
//

EFIAPI
EFI_STATUS
EfipSdBcm2709Reset (
    EFI_BLOCK_IO_PROTOCOL *This,
    BOOLEAN ExtendedVerification
    );

EFIAPI
EFI_STATUS
EfipSdBcm2709ReadBlocks (
    EFI_BLOCK_IO_PROTOCOL *This,
    UINT32 MediaId,
    EFI_LBA Lba,
    UINTN BufferSize,
    VOID *Buffer
    );

EFIAPI
EFI_STATUS
EfipSdBcm2709WriteBlocks (
    EFI_BLOCK_IO_PROTOCOL *This,
    UINT32 MediaId,
    EFI_LBA Lba,
    UINTN BufferSize,
    VOID *Buffer
    );

EFIAPI
EFI_STATUS
EfipSdBcm2709FlushBlocks (
    EFI_BLOCK_IO_PROTOCOL *This
    );

EFI_STATUS
EfipSdBcm2709ResetController (
    PEFI_SD_BCM2709_CONTEXT Device
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the private data template.
//

EFI_SD_BCM2709_CONTEXT EfiSdBcm2709DiskTemplate = {
    EFI_SD_BCM2709_MAGIC,
    NULL,
    NULL,
    NULL,
    FALSE,
    0,
    0,
    {
        EFI_BLOCK_IO_PROTOCOL_REVISION3,
        NULL,
        EfipSdBcm2709Reset,
        EfipSdBcm2709ReadBlocks,
        EfipSdBcm2709WriteBlocks,
        EfipSdBcm2709FlushBlocks
    },

    {0},
};

//
// Define the device path template.
//

EFI_SD_BCM2709_DEVICE_PATH EfiSdBcm2709DevicePathTemplate = {
    {
        {
            {
                HARDWARE_DEVICE_PATH,
                HW_VENDOR_DP,
                sizeof(EFI_SD_BCM2709_BLOCK_IO_DEVICE_PATH)
            },

            EFI_SD_BCM2709_BLOCK_IO_DEVICE_PATH_GUID,
        },

        BCM2709_EMMC_OFFSET
    },

    {
        END_DEVICE_PATH_TYPE,
        END_ENTIRE_DEVICE_PATH_SUBTYPE,
        END_DEVICE_PATH_LENGTH
    }
};

//
// Define a template for the command to enable the eMMC power.
//

EFI_SD_BCM2709_ENABLE_EMMC EfiBcm2709EmmcPowerCommand = {
    {
        sizeof(EFI_SD_BCM2709_ENABLE_EMMC),
        0
    },

    {
        {
            BCM2709_MAILBOX_TAG_SET_POWER_STATE,
            sizeof(UINT32) + sizeof(UINT32),
            sizeof(UINT32) + sizeof(UINT32)
        },

        BCM2709_MAILBOX_DEVICE_SDHCI,
        BCM2709_MAILBOX_POWER_STATE_ON
    },

    0
};

//
// Define a template for the command to get the eMMC clock rate.
//

EFI_SD_BCM2709_GET_EMMC_CLOCK EfiBcm2709EmmcGetClockRateCommand = {
    {
        sizeof(EFI_SD_BCM2709_GET_EMMC_CLOCK),
        0
    },

    {
        {
            BCM2709_MAILBOX_TAG_GET_CLOCK_RATE,
            sizeof(UINT32) + sizeof(UINT32),
            sizeof(UINT32)
        },

        BCM2709_MAILBOX_CLOCK_ID_EMMC,
        0
    },

    0
};

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipBcm2709EnumerateSd (
    VOID
    )

/*++

Routine Description:

    This routine enumerates the SD card on the BCM2709.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    UINT64 BlockCount;
    UINT32 BlockSize;
    PEFI_SD_BCM2709_DEVICE_PATH DevicePath;
    PEFI_SD_BCM2709_CONTEXT Disk;
    UINT32 ExpectedLength;
    UINT32 Frequency;
    EFI_SD_BCM2709_GET_EMMC_CLOCK GetClockRate;
    UINT32 Length;
    EFI_SD_INITIALIZATION_BLOCK SdParameters;
    EFI_STATUS Status;

    //
    // The BCM2709 device library must be initialized to enumerate SD.
    //

    if (EfiBcm2709Initialized == FALSE) {
        return EFI_NOT_READY;
    }

    //
    // Allocate and initialize the context structure.
    //

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_SD_BCM2709_CONTEXT),
                             (VOID **)&Disk);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfiCopyMem(Disk, &EfiSdBcm2709DiskTemplate, sizeof(EFI_SD_BCM2709_CONTEXT));
    Disk->BlockIo.Media = &(Disk->Media);
    Disk->Media.RemovableMedia = TRUE;

    //
    // Create the device path.
    //

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_SD_BCM2709_DEVICE_PATH),
                             (VOID **)&DevicePath);

    if (EFI_ERROR(Status)) {
        goto Bcm2709EnumerateSdEnd;
    }

    //
    // Update the device path template now that the BCM2709 device has a base
    // address. The global array only had the offset.
    //

    EfiSdBcm2709DevicePathTemplate.Disk.ControllerBase =
                                                      (UINTN)BCM2709_EMMC_BASE;

    EfiCopyMem(DevicePath,
               &EfiSdBcm2709DevicePathTemplate,
               sizeof(EFI_SD_BCM2709_DEVICE_PATH));

    Disk->DevicePath = (EFI_DEVICE_PATH_PROTOCOL *)DevicePath;

    //
    // Initialize the eMMC's power.
    //

    Status = EfipBcm2709MailboxSendCommand(BCM2709_MAILBOX_PROPERTIES_CHANNEL,
                                           &EfiBcm2709EmmcPowerCommand,
                                           sizeof(EFI_SD_BCM2709_ENABLE_EMMC),
                                           TRUE);

    if (EFI_ERROR(Status)) {
        goto Bcm2709EnumerateSdEnd;
    }

    //
    // Get the eMMC's clock frequency.
    //

    EfiCopyMem(&GetClockRate,
               &EfiBcm2709EmmcGetClockRateCommand,
               sizeof(EFI_SD_BCM2709_GET_EMMC_CLOCK));

    Status = EfipBcm2709MailboxSendCommand(
                                        BCM2709_MAILBOX_PROPERTIES_CHANNEL,
                                        &GetClockRate,
                                        sizeof(EFI_SD_BCM2709_GET_EMMC_CLOCK),
                                        FALSE);

    if (EFI_ERROR(Status)) {
        goto Bcm2709EnumerateSdEnd;
    }

    Length = GetClockRate.ClockRate.TagHeader.Length;
    ExpectedLength = sizeof(BCM2709_MAILBOX_GET_CLOCK_RATE) -
                     sizeof(BCM2709_MAILBOX_TAG);

    if (BCM2709_MAILBOX_CHECK_TAG_LENGTH(Length, ExpectedLength) == FALSE) {
        Status = EFI_DEVICE_ERROR;
        goto Bcm2709EnumerateSdEnd;
    }

    Frequency = GetClockRate.ClockRate.Rate;

    //
    // Create the SD controller.
    //

    EfiSetMem(&SdParameters, sizeof(EFI_SD_INITIALIZATION_BLOCK), 0);
    SdParameters.StandardControllerBase =
                                       (VOID *)DevicePath->Disk.ControllerBase;

    SdParameters.Voltages = SD_VOLTAGE_32_33 | SD_VOLTAGE_33_34;
    SdParameters.HostCapabilities = SD_MODE_4BIT |
                                    SD_MODE_RESPONSE136_SHIFTED |
                                    SD_MODE_HIGH_SPEED |
                                    SD_MODE_HIGH_SPEED_52MHZ |
                                    SD_MODE_AUTO_CMD12;

    SdParameters.FundamentalClock = Frequency;
    Disk->Controller = EfiSdCreateController(&SdParameters);
    if (Disk->Controller == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Bcm2709EnumerateSdEnd;
    }

    //
    // Perform some initialization to see if the card is there.
    //

    Status = EfiSdInitializeController(Disk->Controller, TRUE);
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

    Status = EfiInstallMultipleProtocolInterfaces(&(Disk->Handle),
                                                  &EfiDevicePathProtocolGuid,
                                                  Disk->DevicePath,
                                                  &EfiBlockIoProtocolGuid,
                                                  &(Disk->BlockIo),
                                                  NULL);

Bcm2709EnumerateSdEnd:
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

//
// --------------------------------------------------------- Internal Functions
//

EFIAPI
EFI_STATUS
EfipSdBcm2709Reset (
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

    PEFI_SD_BCM2709_CONTEXT Disk;
    EFI_STATUS Status;

    Disk = EFI_SD_BCM2709_FROM_THIS(This);
    Status = EfiSdInitializeController(Disk->Controller, TRUE);
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
EfipSdBcm2709ReadBlocks (
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

    PEFI_SD_BCM2709_CONTEXT Disk;
    EFI_STATUS Status;

    Disk = EFI_SD_BCM2709_FROM_THIS(This);
    if (MediaId != Disk->Media.MediaId) {
        return EFI_MEDIA_CHANGED;
    }

    if ((Disk->MediaPresent == FALSE) || (Disk->BlockSize == 0)) {
        return EFI_NO_MEDIA;
    }

    Status = EfiSdBlockIoPolled(Disk->Controller,
                                Lba,
                                BufferSize / Disk->BlockSize,
                                Buffer,
                                FALSE);

    return Status;
}

EFIAPI
EFI_STATUS
EfipSdBcm2709WriteBlocks (
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

    PEFI_SD_BCM2709_CONTEXT Disk;
    EFI_STATUS Status;

    Disk = EFI_SD_BCM2709_FROM_THIS(This);
    if (MediaId != Disk->Media.MediaId) {
        return EFI_MEDIA_CHANGED;
    }

    if ((Disk->MediaPresent == FALSE) || (Disk->BlockSize == 0)) {
        return EFI_NO_MEDIA;
    }

    Status = EfiSdBlockIoPolled(Disk->Controller,
                                Lba,
                                BufferSize / Disk->BlockSize,
                                Buffer,
                                TRUE);

    return Status;
}

EFIAPI
EFI_STATUS
EfipSdBcm2709FlushBlocks (
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

