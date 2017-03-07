/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fb.h

Abstract:

    This header contains definitions for a basic video framebuffer.

Author:

    Evan Green 3-Mar-2017

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the magic value used to identify frame buffer structures.
//

#define FRAME_BUFFER_MAGIC 0x6D617246

//
// Define the size of the frame buffer identifier.
//

#define FRAME_BUFFER_ID_LENGTH 32

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the possible frame buffer IOCTL numbers.
//

typedef enum _FRAME_BUFFER_CONTROL {
    FrameBufferGetMode = 0x4600,
    FrameBufferSetMode = 0x4601,
    FrameBufferGetInfo = 0x4602
} FRAME_BUFFER_CONTROL, *PFRAME_BUFFER_CONTROL;

//
// Define frame buffer types.
//

typedef enum _FRAME_BUFFER_TYPE {
    FrameBufferTypeInvalid,
    FrameBufferTypeLinear,
    FrameBufferTypeText
} FRAME_BUFFER_TYPE, *PFRAME_BUFFER_TYPE;

/*++

Structure Description:

    This structure stores information about a frame buffer device, responded to
    by the FrameBufferGetInfo control command.

Members:

    Magic - Stores the constant value FRAME_BUFFER_MAGIC, used to identify
        that this really is a frame buffer information structure.

    Identifier - Stores a potentially non-null terminated string containing an
        identifier of the device.

    Type - Stores the frame buffer type. See the FRAME_BUFFER_TYPE enum.

    Address - Stores the physical address of the frame buffer.

    Length - Stores the length in bytes of the frame buffer.

    PanStepX - Stores the granularity of hardware panning in the X direction.
        This is 0 if the hardware does not support panning.

    PanStepY - Stores the granularity of hardware panning in the Y directory.
        Set to 0 if the hardware does not support panning.

    WrapStepY - Stores whether or not the hardware supports vertical wrapping.

    LineLength - Stores the length of a line in bytes, including extra bytes
        at the end of the visual line.

    RegisterAddress - Stores the physical address of the device registers, or
        0 if no access to the hardware registers is provided.

    RegisterLength - Stores the length of the registers region.

--*/

typedef struct _FRAME_BUFFER_INFO {
    ULONG Magic;
    CHAR Identifier[FRAME_BUFFER_ID_LENGTH];
    ULONG Type;
    ULONGLONG Address;
    ULONGLONG Length;
    USHORT PanStepX;
    USHORT PanStepY;
    USHORT WrapStepY;
    ULONG LineLength;
    ULONGLONG RegisterAddress;
    ULONGLONG RegisterLength;
} FRAME_BUFFER_INFO, *PFRAME_BUFFER_INFO;

/*++

Structure Description:

    This structure stores potentially programmable information about the
    frame buffer's configuration.

Members:

    Magic - Stores the constant value FRAME_BUFFER_MAGIC, used to identify
        that this really is a frame buffer information structure.

    ResolutionX - Stores the visible resolution in the horizontal dimension.

    ResolutionY - Stores the visible resolution in the vertical dimension.

    VirtualResolutionX - Stores the virtual resolution in the horizontal
        dimension, navigated by hardware panning.

    VirtualResolutionY - Stores the virtual resolution in the vertical
        dimension, navigated by hardware panning.

    OffsetX - Stores the horizontal offset from the virtual region to the
        visible region.

    OffsetY - Stores the vertical offset from the virtual region to the
        visible region.

    BitsPerPixel - Stores the width of a pixel in bits.

    RedMask - Stores the mask of which bits in a pixel correspond to red.

    GreenMask - Stores the mask of which bits in a pixel correspond to green.

    BlueMask - Stores the mask of which bits in a pixel correspond to blue.

    AlphaMask - Stores the mask of which bits in a pixel correspond to
        transparency.

    PixelClock - Stores the pixel clock period in picoseconds.

    LeftMargin - Stores the number of pixel clocks between the sync to the
        picture.

    RightMargin - Stores the number of pixel clocks between the end of the
        picture and the sync.

    TopMargin - Stores the number of pixel clocks between the end of the sync
        and the start of the picture.

    BottomMargin - Stores the number of pixel clock between the end of the
        picture and the start of the sync.

    HorizontalSync - Stores the length of the horizontal sync in pixel clocks.

    VerticalSync - Stores the length of the vertical sync in pixel clocks.

    Rotate - Stores the angle of rotation counterclockwise.

--*/

typedef struct _FRAME_BUFFER_MODE {
    ULONG Magic;
    ULONG ResolutionX;
    ULONG ResolutionY;
    ULONG VirtualResolutionX;
    ULONG VirtualResolutionY;
    ULONG OffsetX;
    ULONG OffsetY;
    ULONG BitsPerPixel;
    ULONG RedMask;
    ULONG GreenMask;
    ULONG BlueMask;
    ULONG AlphaMask;
    ULONG PixelClock;
    ULONG LeftMargin;
    ULONG RightMargin;
    ULONG TopMargin;
    ULONG BottomMargin;
    ULONG HorizontalSync;
    ULONG VerticalSync;
    ULONG Rotate;
} FRAME_BUFFER_MODE, *PFRAME_BUFFER_MODE;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
