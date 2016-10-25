/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    graphout.h

Abstract:

    This header contains definitions for the UEFI Graphics Output Protocol
    (sometimes called GOP).

Author:

    Evan Green 8-Feb-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID                   \
    {                                                       \
        0x9042A9DE, 0x23DC, 0x4A38,                         \
        {0x96, 0xFB, 0x7A, 0xDE, 0xD0, 0x80, 0x51, 0x6A }   \
    }

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Enumeration Description:

    This enumeration describes the pixel formats supported in UEFI.

Values:

    PixelRedGreenBlueReserved8BitPerColor - Indicates a pixel is 32-bits and
        byte zero represents red, byte one represents green, byte two
        represents blue, and byte three is reserved. This is the definition
        for the physical frame buffer. The byte values for the red, green, and
        blue components represent the color intensity. This color intensity
        value range from a minimum intensity of 0 to maximum intensity of 255.

    PixelBlueGreenRedReserved8BitPerColor - Indicates a pixel is 32-bits and
        byte zero represents blue, byte one represents green, byte two
        represents red, and byte three is reserved. This is the definition
        for the physical frame buffer. The byte values for the red, green, and
        blue components represent the color intensity. This color intensity
        value range from a minimum intensity of 0 to maximum intensity of 255.

    PixelBitMask - Indicates the pixel definition of the physical frame buffer.

    PixelBltOnly - Indicates a physical frame buffer is not supported.

    PixelFormatMax - Indicates the first invalid value, used for boundary
        checks.

--*/

typedef enum {
    PixelRedGreenBlueReserved8BitPerColor,
    PixelBlueGreenRedReserved8BitPerColor,
    PixelBitMask,
    PixelBltOnly,
    PixelFormatMax
} EFI_GRAPHICS_PIXEL_FORMAT;

/*++

Enumeration Description:

    This enumeration describes the possible actions for Blit operations.

Values:

    EfiBltVideoFill - Indicates to write data from the BltBuffer pixel (0, 0)
        directly to every pixel of the video display rectangle (DestinationX,
        DestinationY) (DestinationX + Width, DestinationY + Height). Only one
        pixel will be used from the BltBuffer. Delta is NOT used.

    EfiBltVideoToBltBuffer - Indicates to read data from the video display
        rectangle (SourceX, SourceY) (SourceX + Width, SourceY + Height) and
        place it in the BltBuffer rectangle (DestinationX, DestinationY)
        (DestinationX + Width, DestinationY + Height). If DestinationX or
        DestinationY is not zero then Delta must be set to the length in bytes
        of a row in the BltBuffer.

    EfiBltBufferToVideo - Indicates to write data from the BltBuffer rectangle
        (SourceX, SourceY) (SourceX + Width, SourceY + Height) directly to the
        video display rectangle (DestinationX, DestinationY)
        (DestinationX + Width, DestinationY + Height). If SourceX or SourceY is
        not zero then Delta must be set to the length in bytes of a row in the
        BltBuffer.

    EfiBltVideoToVideo - Indicates to Copy from the video display rectangle
        (SourceX, SourceY) (SourceX + Width, SourceY + Height) to the video
        display rectangle (DestinationX, DestinationY) (DestinationX + Width,
        DestinationY + Height). The BltBuffer and Delta are not used in this
        mode.

    EfiGraphicsOutputBltOperationMax - Indicates the boundary of the valid
        values, used for bounds checking.

--*/

typedef enum {
    EfiBltVideoFill,
    EfiBltVideoToBltBuffer,
    EfiBltBufferToVideo,
    EfiBltVideoToVideo,
    EfiGraphicsOutputBltOperationMax
} EFI_GRAPHICS_OUTPUT_BLT_OPERATION;

typedef struct _EFI_GRAPHICS_OUTPUT_PROTOCOL EFI_GRAPHICS_OUTPUT_PROTOCOL;

/*++

Structure Description:

    This structure describes the pixel bitmap, indicating which bits are used
    for which color channels.

Members:

    RedMask - Stores the mask of bits used for red.

    GreenMask - Stores the mask of bits used for green.

    BlueMask - Stores the mask of bits used for blue.

    ReservedMask - Stores the mask of bits reserved in the pixel format.

--*/

typedef struct {
    UINT32 RedMask;
    UINT32 GreenMask;
    UINT32 BlueMask;
    UINT32 ReservedMask;
} EFI_PIXEL_BITMASK;

/*++

Structure Description:

    This structure describes graphics mode information.

Members:

    Version - Stores the version of this data structure.

    HorizontalResolution - Stores the size of the video screen in pixels in the
        X direction.

    VerticalResolution - Store the size of the video screen in pixels in the
        Y direction.

    PixelFormat - Stores the physical format of the pixel. A value of
        PixelBltOnly implies that a linear frame buffer is not available for
        this mode.

    PixelInformation - Stores the mask of bits being used for each color
        channel. This is only valid if the pixel format is set to
        PixelFormatBitMask.

    PixelsPerScanLine - Stores the number of pixels per video memory line.

--*/

typedef struct {
    UINT32 Version;
    UINT32 HorizontalResolution;
    UINT32 VerticalResolution;
    EFI_GRAPHICS_PIXEL_FORMAT PixelFormat;
    EFI_PIXEL_BITMASK PixelInformation;
    UINT32 PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

/*++

Structure Description:

    This structure describes the pixel format for a BitBlt operation.

Members:

    Blue - Stores the blue channel.

    Green - Stores the green channel.

    Red - Stores the red channel.

    Reserved - Stores the reserved channel.

--*/

typedef struct {
    UINT8 Blue;
    UINT8 Green;
    UINT8 Red;
    UINT8 Reserved;
} EFI_GRAPHICS_OUTPUT_BLT_PIXEL;

/*++

Union Description:

    This union creates the common storage container for a BitBlt pixel and a
    raw device pixel.

Members:

    Pixel - Stores the BitBlt version of the pixel.

    Raw - Stores the raw format pixel.

--*/

typedef union {
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pixel;
    UINT32 Raw;
} EFI_GRAPHICS_OUTPUT_BLT_PIXEL_UNION;

typedef
EFI_STATUS
(EFIAPI *EFI_GRAPHICS_OUTPUT_PROTOCOL_QUERY_MODE) (
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    UINT32 ModeNumber,
    UINTN *SizeOfInfo,
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info
    );

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

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if a hardware error occurred trying to retrieve the video
    mode.

    EFI_INVALID_PARAMETER if the mode number is not valid.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_GRAPHICS_OUTPUT_PROTOCOL_SET_MODE) (
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    UINT32 ModeNumber
    );

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

typedef
EFI_STATUS
(EFIAPI *EFI_GRAPHICS_OUTPUT_PROTOCOL_BLT) (
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BltBuffer,
    EFI_GRAPHICS_OUTPUT_BLT_OPERATION BltOperation,
    UINTN SourceX,
    UINTN SourceY,
    UINTN DestinationX,
    UINTN DestinationY,
    UINTN Width,
    UINTN Height,
    UINTN Delta
    );

/*++

Routine Description:

    This routine performs a Blt (copy) operation of pixels on the graphics
    screen. Blt stands for Block Transfer for those not up on their video lingo.

Arguments:

    This - Supplies a pointer to the protocol instance.

    BltBuffer - Supplies an optional pointer to the data to transfer to the
        graphics screen. The size must be at least width * height *
        sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL).

    BltOperation - Supplies the operation to perform when copying the buffer to
        the screen.

    SourceX - Supplies the X coordinate of the source of the operation.

    SourceY - Supplies the Y coordinate of the source of the operation.

    DestinationX - Supplies the X coordinate of the destination of the
        operation.

    DestinationY - Supplies the Y coordinate of the destination of the
        operation.

    Width - Supplies the width of the rectangle in pixels.

    Height - Supplies the height of the rectangle in pixels.

    Delta - Supplies an optional number of bytes in a row of the given buffer.
        If a delta of zero is used, the entire buffer is being operated on.
        This is not used for EfiBltVideoFill or EfiBltVideoToVideo operations.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the operation was not valid.

    EFI_DEVICE_ERROR if a hardware error occurred and the request could not be
    completed.

--*/

/*++

Structure Description:

    This structure describes a graphics mode.

Members:

    MaxMode - Stores the maximum number of modes supported by the query and set
        mode functions, exclusive (this is one beyond the last supported mode
        number).

    Mode - Stores the current mode number. Valid mode numbers are between 0 and
        the max mode minus one.

    Info - Stores a pointer to a read-only version of the mode information.

    SizeOfInfo - Stores the size of the information structure in bytes.

    FrameBufferBase - Stores the base physical address of the graphics linear
        frame buffer. The first pixel here represents the upper left pixel of
        the display.

    FrameBufferSize - Stores the size of the frame buffer needed to support the
        active mode as defined by PixelsPerScanLine * VerticalResolution *
        PixelElementSize.

--*/

typedef struct {
    UINT32 MaxMode;
    UINT32 Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN SizeOfInfo;
    EFI_PHYSICAL_ADDRESS FrameBufferBase;
    UINTN FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

/*++

Structure Description:

    This structure describes the Graphics Output Protocol. It provides a basic
    abstraction to set video modes and copy pixels to and from the graphics
    controller's frame buffer. The linear address of the hardware frame buffer
    is also exposed so software can write directly to the video hardware (yay).

Members:

    QueryMode - Stores a pointer to a function used to query information about
        the supported video modes.

    SetMode - Stores a pointer to a function used to set the current mode.

    Blt - Stores a pointer to a function used to copy graphics data to the
        screen.

    Mode - Stores a pointer to the mode information.

--*/

struct _EFI_GRAPHICS_OUTPUT_PROTOCOL {
    EFI_GRAPHICS_OUTPUT_PROTOCOL_QUERY_MODE QueryMode;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_SET_MODE SetMode;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_BLT Blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
