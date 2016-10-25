/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    video.c

Abstract:

    This module implements support for frame buffer video display in UEFI. This
    frame buffer is passed on and used by the kernel.

Author:

    Evan Green 12-Feb-2014

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/uefi/uefi.h>
#include <minoca/uefi/protocol/graphout.h>
#include "firmware.h"
#include "bootlib.h"
#include "efisup.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum number of output devices that any machine should have.
//

#define MAX_GRAPHICS_OUT_HANDLES 20

//
// Define the number of video strategies.
//

#define BO_EFI_VIDEO_STRATEGIES_COUNT \
    (sizeof(BoEfiVideoStrategies) / sizeof(BoEfiVideoStrategies[0]))

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the possible strategies when choosing a video mode.
//

typedef enum _BOOT_VIDEO_STRATEGY {
    BootVideoStrategyInvalid,
    BootVideoStrategyUseFirmwareMode,
    BootVideoStrategyUseFirmwareModeMin1024x768,
    BootVideoStrategyUseLowestResolution,
    BootVideoStrategyUseHighestResolution,
    BootVideoStrategyMax1024x768,
    BootVideoStrategyMax1024x600,
    BootVideoStrategySpecificValues
} BOOT_VIDEO_STRATEGY, *PBOOT_VIDEO_STRATEGY;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
BopEfiConfigureFrameBuffer (
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Graphics
    );

KSTATUS
BopEfiFindBestVideoMode (
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Graphics,
    PUINTN BestModeNumber,
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **BestMode,
    PUINTN BestModeSize,
    PBOOL MustFree
    );

EFI_STATUS
BopEfiGraphicsOutputQueryMode (
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    UINT32 ModeNumber,
    UINTN *SizeOfInfo,
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info
    );

EFI_STATUS
BopEfiGraphicsOutputSetMode (
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    UINT32 ModeNumber
    );

//
// -------------------------------------------------------------------- Globals
//

USHORT BoEfiDesiredVideoResolutionX;
USHORT BoEfiDesiredVideoResolutionY;

//
// Define the order in which the boot video strategies will be applied.
//

BOOT_VIDEO_STRATEGY BoEfiVideoStrategies[] = {
    BootVideoStrategySpecificValues,
    BootVideoStrategyUseFirmwareModeMin1024x768,
    BootVideoStrategyMax1024x768,
    BootVideoStrategyUseFirmwareMode,
    BootVideoStrategyUseLowestResolution,
};

//
// Define the video parameters.
//

USHORT BoEfiVideoResolutionX;
USHORT BoEfiVideoResolutionY;
USHORT BoEfiVideoPixelsPerScanLine;
USHORT BoEfiVideoBitsPerPixel;
ULONG BoEfiVideoRedMask;
ULONG BoEfiVideoGreenMask;
ULONG BoEfiVideoBlueMask;
PHYSICAL_ADDRESS BoEfiFrameBufferAddress;
ULONGLONG BoEfiFrameBufferSize;

//
// ------------------------------------------------------------------ Functions
//

VOID
BopEfiInitializeVideo (
    VOID
    )

/*++

Routine Description:

    This routine initializes UEFI video services. Failure here is not fatal.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINTN BufferSize;
    EFI_STATUS EfiStatus;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Graphics;
    EFI_HANDLE Handle;
    EFI_HANDLE HandleArray[MAX_GRAPHICS_OUT_HANDLES];
    UINTN HandleCount;
    UINTN HandleIndex;
    KSTATUS Status;

    BufferSize = sizeof(HandleArray);
    Graphics = NULL;
    RtlZeroMemory(HandleArray, BufferSize);

    //
    // Request all handles that respond to the graphics output protocol.
    //

    EfiStatus = BopEfiLocateHandle(ByProtocol,
                                   &BoEfiGraphicsOutputProtocolGuid,
                                   NULL,
                                   &BufferSize,
                                   HandleArray);

    if (EFI_ERROR(EfiStatus)) {
        goto EfiInitializeVideoEnd;
    }

    //
    // Loop through all the handles trying to find one to be the official frame
    // buffer.
    //

    Status = STATUS_NO_SUCH_DEVICE;
    HandleCount = BufferSize / sizeof(EFI_HANDLE);
    for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex += 1) {
        Handle = HandleArray[HandleIndex];
        EfiStatus = BopEfiOpenProtocol(Handle,
                                       &BoEfiGraphicsOutputProtocolGuid,
                                       (VOID **)&Graphics,
                                       BoEfiImageHandle,
                                       NULL,
                                       EFI_OPEN_PROTOCOL_GET_PROTOCOL);

        if (EFI_ERROR(EfiStatus)) {
            Status = BopEfiStatusToKStatus(EfiStatus);
            continue;
        }

        //
        // Configure the video and close the protocol. If the configuration
        // succeeded, then that's it.
        //

        Status = BopEfiConfigureFrameBuffer(Graphics);
        BopEfiCloseProtocol(Handle,
                            &BoEfiGraphicsOutputProtocolGuid,
                            BoEfiImageHandle,
                            NULL);

        if (KSUCCESS(Status)) {
            break;
        }
    }

    if (!KSUCCESS(Status)) {
        goto EfiInitializeVideoEnd;
    }

EfiInitializeVideoEnd:
    return;
}

KSTATUS
BopEfiGetVideoInformation (
    PULONG ResolutionX,
    PULONG ResolutionY,
    PULONG PixelsPerScanLine,
    PULONG BitsPerPixel,
    PULONG RedMask,
    PULONG GreenMask,
    PULONG BlueMask,
    PPHYSICAL_ADDRESS FrameBufferBase,
    PULONGLONG FrameBufferSize
    )

/*++

Routine Description:

    This routine returns information about the video frame buffer.

Arguments:

    ResolutionX - Supplies a pointer where the horizontal resolution in pixels
        will be returned on success.

    ResolutionY - Supplies a pointer where the vertical resolution in pixels
        will be returned on success.

    PixelsPerScanLine - Supplies a pointer where the number of pixels per scan
        line will be returned on success.

    BitsPerPixel - Supplies a pointer where the number of bits per pixel will
        be returned on success.

    RedMask - Supplies a pointer where the mask of bits corresponding to the
        red channel will be returned on success. It is assumed this will be a
        contiguous chunk of bits.

    GreenMask - Supplies a pointer where the mask of bits corresponding to the
        green channel will be returned on success. It is assumed this will be a
        contiguous chunk of bits.

    BlueMask - Supplies a pointer where the mask of bits corresponding to the
        blue channel will be returned on success. It is assumed this will be a
        contiguous chunk of bits.

    FrameBufferBase - Supplies a pointer where the physical base address of the
        frame buffer will be returned on success.

    FrameBufferSize - Supplies a pointer where the size of the frame buffer in
        bytes will be returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_CONFIGURED if video services could not be initialized.

--*/

{

    if (BoEfiVideoResolutionX == 0) {
        return STATUS_NOT_CONFIGURED;
    }

    *ResolutionX = BoEfiVideoResolutionX;
    *ResolutionY = BoEfiVideoResolutionY;
    *PixelsPerScanLine = BoEfiVideoPixelsPerScanLine;
    *BitsPerPixel = BoEfiVideoBitsPerPixel;
    *RedMask = BoEfiVideoRedMask;
    *GreenMask = BoEfiVideoGreenMask;
    *BlueMask = BoEfiVideoBlueMask;
    *FrameBufferBase = BoEfiFrameBufferAddress;
    *FrameBufferSize = BoEfiFrameBufferSize;
    return STATUS_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
BopEfiConfigureFrameBuffer (
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Graphics
    )

/*++

Routine Description:

    This routine attempts to configure a graphics output device.

Arguments:

    Graphics - Supplies a pointer to the open graphics output protocol
        interface.

Return Value:

    Status code.

--*/

{

    ULONG BitsPerPixel;
    ULONG CombinedMask;
    EFI_STATUS EfiStatus;
    BOOL FreeMode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Mode;
    UINTN ModeIndex;
    UINTN ModeSize;
    KSTATUS Status;

    Mode = NULL;
    FreeMode = FALSE;

    //
    // Figure out the best video mode this device has got based on internal
    // policy.
    //

    Status = BopEfiFindBestVideoMode(Graphics,
                                     &ModeIndex,
                                     &Mode,
                                     &ModeSize,
                                     &FreeMode);

    if (!KSUCCESS(Status)) {
        goto EfiConfigureFrameBufferEnd;
    }

    //
    // Try to set the desired video mode. If the mode is already set, then
    // don't bother doing anything.
    //

    if ((Graphics->Mode == NULL) || (Graphics->Mode->Mode != ModeIndex)) {
        EfiStatus = BopEfiGraphicsOutputSetMode(Graphics, ModeIndex);
        if (EFI_ERROR(EfiStatus)) {
            Status = BopEfiStatusToKStatus(EfiStatus);
            goto EfiConfigureFrameBufferEnd;
        }
    }

    //
    // Set the globals with the video parameters.
    //

    BoEfiVideoResolutionX = Mode->HorizontalResolution;
    BoEfiVideoResolutionY = Mode->VerticalResolution;
    BoEfiVideoPixelsPerScanLine = Mode->PixelsPerScanLine;
    switch (Mode->PixelFormat) {
    case PixelRedGreenBlueReserved8BitPerColor:
        BoEfiVideoBitsPerPixel = 32;
        BoEfiVideoRedMask = 0x000000FF;
        BoEfiVideoGreenMask = 0x0000FF00;
        BoEfiVideoBlueMask = 0x00FF0000;
        break;

    case PixelBlueGreenRedReserved8BitPerColor:
        BoEfiVideoBitsPerPixel = 32;
        BoEfiVideoRedMask = 0x00FF0000;
        BoEfiVideoGreenMask = 0x0000FF00;
        BoEfiVideoBlueMask = 0x000000FF;
        break;

    case PixelBitMask:
        BoEfiVideoRedMask = Mode->PixelInformation.RedMask;
        BoEfiVideoGreenMask = Mode->PixelInformation.GreenMask;
        BoEfiVideoBlueMask = Mode->PixelInformation.BlueMask;
        CombinedMask = Mode->PixelInformation.RedMask |
                       Mode->PixelInformation.GreenMask |
                       Mode->PixelInformation.BlueMask |
                       Mode->PixelInformation.ReservedMask;

        ASSERT(CombinedMask != 0);

        BitsPerPixel = 32;
        while ((BitsPerPixel != 0) &&
               ((CombinedMask & (1 << (BitsPerPixel - 1))) == 0)) {

            BitsPerPixel -= 1;
        }

        BoEfiVideoBitsPerPixel = BitsPerPixel;
        break;

    //
    // The "find best mode" function should have weeded out any unsupported
    // pixel formats.
    //

    default:

        ASSERT(FALSE);

        Status = STATUS_INVALID_CONFIGURATION;
        goto EfiConfigureFrameBufferEnd;
    }

    BoEfiFrameBufferAddress = Graphics->Mode->FrameBufferBase;
    BoEfiFrameBufferSize = Graphics->Mode->FrameBufferSize;
    Status = STATUS_SUCCESS;

EfiConfigureFrameBufferEnd:
    if (FreeMode != FALSE) {

        ASSERT(Mode != NULL);

        BopEfiFreePool(Mode);
    }

    return Status;
}

KSTATUS
BopEfiFindBestVideoMode (
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Graphics,
    PUINTN BestModeNumber,
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **BestMode,
    PUINTN BestModeSize,
    PBOOL MustFree
    )

/*++

Routine Description:

    This routine determines the best video mode to use.

Arguments:

    Graphics - Supplies a pointer to the graphics device protocol instance.

    BestModeNumber - Supplies a pointer where the best mode number will be
        returned on success.

    BestMode - Supplies a pointer that returns a pointer to the best mode on
        success. This may or may not be allocated from pool.

    BestModeSize - Supplies a pointer where the size of the best mode buffer
        in bytes will be returned.

    MustFree - Supplies a pointer to a boolean indicating if the caller must
        free the buffer from EFI pool (not the loader pool). If FALSE, then
        this memory can be abandoned by the caller when finished.

Return Value:

    EFI status code.

--*/

{

    ULONG BitsPerPixel;
    ULONG ChosenBitsPerPixel;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *ChosenMode;
    UINTN ChosenModeIndex;
    UINTN ChosenModeSize;
    ULONG CombinedMask;
    ULONG DesiredX;
    ULONG DesiredY;
    EFI_STATUS EfiStatus;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Mode;
    ULONG ModeIndex;
    UINTN ModeSize;
    KSTATUS Status;
    BOOT_VIDEO_STRATEGY Strategy;
    ULONG StrategyIndex;

    ChosenBitsPerPixel = 0;
    ChosenMode = NULL;
    ChosenModeIndex = 0;
    ChosenModeSize = 0;
    Mode = NULL;
    *MustFree = FALSE;

    //
    // Try each of the strategies in order until a suitable mode is found or
    // there are no more strategies.
    //

    Status = STATUS_NOT_SUPPORTED;
    for (StrategyIndex = 0;
         StrategyIndex < BO_EFI_VIDEO_STRATEGIES_COUNT;
         StrategyIndex += 1) {

        Strategy = BoEfiVideoStrategies[StrategyIndex];
        switch (Strategy) {

        //
        // Only use the firmware mode if it is supported.
        //

        case BootVideoStrategyUseFirmwareModeMin1024x768:
        case BootVideoStrategyUseFirmwareMode:
            Mode = Graphics->Mode->Info;
            ModeSize = Graphics->Mode->SizeOfInfo;
            ModeIndex = Graphics->Mode->Mode;

            //
            // Skip it if the size is wonky.
            //

            if (ModeSize < sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION)) {
                Mode = NULL;
                continue;
            }

            //
            // Skip it if the pixel format is not supported.
            //

            if ((Mode->PixelFormat != PixelRedGreenBlueReserved8BitPerColor) &&
                (Mode->PixelFormat != PixelBlueGreenRedReserved8BitPerColor) &&
                (Mode->PixelFormat != PixelBitMask)) {

                Mode = NULL;
                continue;
            }

            //
            // Skip the firmware mode if it must be at least 1024x768 and it is
            // not up to the challenge.
            //

            if ((Strategy == BootVideoStrategyUseFirmwareModeMin1024x768) &&
                ((Mode->HorizontalResolution < 1024) ||
                 (Mode->VerticalResolution < 768))) {

                Mode = NULL;
                continue;
            }

            *MustFree = FALSE;
            ChosenMode = Mode;
            ChosenModeSize = ModeSize;
            ChosenModeIndex = ModeIndex;
            Mode = NULL;
            Status = STATUS_SUCCESS;
            goto EfiFindBestVideoModeEnd;

        case BootVideoStrategyUseLowestResolution:
        case BootVideoStrategyUseHighestResolution:
            DesiredX = -1;
            DesiredY = -1;
            break;

        case BootVideoStrategyMax1024x768:
            DesiredX = 1024;
            DesiredY = 768;
            break;

        case BootVideoStrategyMax1024x600:
            DesiredX = 1024;
            DesiredY = 600;
            break;

        case BootVideoStrategySpecificValues:
            if ((BoEfiDesiredVideoResolutionX == 0) ||
                (BoEfiDesiredVideoResolutionY == 0)) {

                continue;
            }

            DesiredX = BoEfiDesiredVideoResolutionX;
            DesiredY = BoEfiDesiredVideoResolutionY;
            break;

        default:

            ASSERT(FALSE);

            Status = STATUS_INVALID_CONFIGURATION;
            goto EfiFindBestVideoModeEnd;
        }

        *MustFree = TRUE;
        for (ModeIndex = 0;
             ModeIndex < Graphics->Mode->MaxMode;
             ModeIndex += 1) {

            //
            // Get information about this mode.
            //

            EfiStatus = BopEfiGraphicsOutputQueryMode(Graphics,
                                                      ModeIndex,
                                                      &ModeSize,
                                                      &Mode);

            if (EFI_ERROR(EfiStatus)) {
                Status = BopEfiStatusToKStatus(EfiStatus);
                goto EfiFindBestVideoModeEnd;
            }

            //
            // Skip it if the size is wonky.
            //

            if (ModeSize < sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION)) {
                BopEfiFreePool(Mode);
                Mode = NULL;
                continue;
            }

            //
            // Skip it if the pixel format is not supported.
            //

            if ((Mode->PixelFormat != PixelRedGreenBlueReserved8BitPerColor) &&
                (Mode->PixelFormat != PixelBlueGreenRedReserved8BitPerColor) &&
                (Mode->PixelFormat != PixelBitMask)) {

                BopEfiFreePool(Mode);
                Mode = NULL;
                continue;
            }

            //
            // If the strategy is looking for specific values, skip anything
            // that does not match the desired resolution.
            //

            if (Strategy == BootVideoStrategySpecificValues) {
                if ((Mode->HorizontalResolution != DesiredX) ||
                    (Mode->VerticalResolution != DesiredY)) {

                    BopEfiFreePool(Mode);
                    Mode = NULL;
                    continue;
                }

            //
            // Otherwise skip it if the resolution is bigger than the desired
            // resolution.
            //

            } else {
                if ((Mode->HorizontalResolution > DesiredX) ||
                    (Mode->VerticalResolution > DesiredY)) {

                    BopEfiFreePool(Mode);
                    Mode = NULL;
                    continue;
                }
            }

            //
            // Figure out the depth of this mode.
            //

            BitsPerPixel = 32;
            switch (Mode->PixelFormat) {
            case PixelRedGreenBlueReserved8BitPerColor:
            case PixelBlueGreenRedReserved8BitPerColor:
                break;

            case PixelBitMask:
                CombinedMask = Mode->PixelInformation.RedMask |
                               Mode->PixelInformation.GreenMask |
                               Mode->PixelInformation.BlueMask |
                               Mode->PixelInformation.ReservedMask;

                ASSERT(CombinedMask != 0);

                while ((BitsPerPixel != 0) &&
                       ((CombinedMask & (1 << (BitsPerPixel - 1))) == 0)) {

                    BitsPerPixel -= 1;
                }

                break;

            //
            // The conditional above should have skipped this.
            //

            default:

                ASSERT(FALSE);

                break;
            }

            //
            // Check it against the favorite mode if there is one.
            //

            if (ChosenMode != NULL) {

                //
                // If trying to pick the lowest resolution, skip it if it is
                // higher than the chosen mode.
                //

                if (Strategy == BootVideoStrategyUseLowestResolution) {
                    if ((Mode->HorizontalResolution >
                         ChosenMode->HorizontalResolution) ||
                        (Mode->VerticalResolution >
                         ChosenMode->VerticalResolution)) {

                        BopEfiFreePool(Mode);
                        Mode = NULL;
                        continue;
                    }

                //
                // Otherwise, skip it if the resolution is any smaller than the
                // chosen mode.
                //

                } else {
                    if ((Mode->HorizontalResolution <
                         ChosenMode->HorizontalResolution) ||
                        (Mode->VerticalResolution <
                         ChosenMode->VerticalResolution)) {

                        BopEfiFreePool(Mode);
                        Mode = NULL;
                        continue;
                    }
                }

                //
                // Okay, so it's the same resolution or smaller/bigger
                // depending on the strategy. Skip it if it's the same
                // resolution, but has a worse depth.
                //

                if ((Mode->HorizontalResolution ==
                     ChosenMode->HorizontalResolution) &&
                    (Mode->VerticalResolution ==
                     ChosenMode->VerticalResolution) &&
                    (BitsPerPixel < ChosenBitsPerPixel)) {

                    BopEfiFreePool(Mode);
                    Mode = NULL;
                    continue;
                }

                //
                // This mode is better than the chosen mode. Free the previous
                // chosen mode, it sucks.
                //

                BopEfiFreePool(ChosenMode);
            }

            //
            // If the strategy is to choose the lowest mode, update the desired
            // X and Y to immediately skip larger resolutions.
            //

            if (Strategy == BootVideoStrategyUseLowestResolution) {
                DesiredX = Mode->HorizontalResolution;
                DesiredY = Mode->VerticalResolution;
            }

            ChosenMode = Mode;
            ChosenModeIndex = ModeIndex;
            ChosenModeSize = ModeSize;
            ChosenBitsPerPixel = BitsPerPixel;
            Mode = NULL;
        }

        if (ChosenMode != NULL) {
            Status = STATUS_SUCCESS;
            break;
        }

        Mode = NULL;
    }

EfiFindBestVideoModeEnd:
    if (!KSUCCESS(Status)) {
        *MustFree = FALSE;
        if (ChosenMode != NULL) {
            BopEfiFreePool(ChosenMode);
            ChosenMode = NULL;
        }

        ChosenModeIndex = -1;
        ChosenModeSize = 0;
    }

    if (Mode != NULL) {
        BopEfiFreePool(Mode);
    }

    *BestMode = ChosenMode;
    *BestModeNumber = ChosenModeIndex;
    *BestModeSize = ChosenModeSize;
    return Status;
}

EFI_STATUS
BopEfiGraphicsOutputQueryMode (
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    UINT32 ModeNumber,
    UINTN *SizeOfInfo,
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info
    )

/*++

Routine Description:

    This routine returns information about available graphics modes that the
    graphics device and set of active video output devices support.

Arguments:

    This - Supplies a pointer to the protocol instance.

    ModeNumber - Supplies the mode number to return information about.

    SizeOfInfo - Supplies a pointer that on input contains the size in bytes of
        the information buffer.

    Info - Supplies a pointer where a callee-allocated buffer will be returned
        containing information about the mode. The caller is responsible for
        calling FreePool to free this data.

Return Value:

    EFI status code.

--*/

{

    EFI_STATUS Status;

    BopEfiRestoreFirmwareContext();
    Status = This->QueryMode(This, ModeNumber, SizeOfInfo, Info);
    BopEfiRestoreApplicationContext();
    return Status;
}

EFI_STATUS
BopEfiGraphicsOutputSetMode (
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    UINT32 ModeNumber
    )

/*++

Routine Description:

    This routine sets the video device into the specified mode and clears the
    visible portions of the output display to black.

Arguments:

    This - Supplies a pointer to the protocol instance.

    ModeNumber - Supplies the mode number to set.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if a hardware error occurred trying to set the video mode.

    EFI_UNSUPPORTED if the mode number is not supported by this device.

--*/

{

    EFI_STATUS Status;

    BopEfiRestoreFirmwareContext();
    Status = This->SetMode(This, ModeNumber);
    BopEfiRestoreApplicationContext();
    return Status;
}

