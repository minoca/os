/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    basevid.h

Abstract:

    This header contains definitions for the base video library, which can
    print text onto a frame buffer.

Author:

    Evan Green 30-Jan-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

#define BASE_VIDEO_ATTRIBUTES(_ForegroundColor, _BackgroundColor) \
    ((_ForegroundColor) | ((_BackgroundColor) << BASE_VIDEO_BACKGROUND_SHIFT))

//
// This macro creates a base video color from red, green and blue components.
// Valid values are between 0 and 255.
//

#define BASE_VIDEO_COLOR_RGB(_Red, _Green, _Blue)               \
    ((((ULONG)(_Red) << 16) & 0x00FF0000) |                     \
     (((ULONG)(_Green) << 8) & 0x0000FF00) |                    \
     ((ULONG)(_Blue) & 0x000000FF))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the attributes.
//

#define BASE_VIDEO_BACKGROUND_SHIFT 4
#define BASE_VIDEO_COLOR_MASK       0x0F
#define BASE_VIDEO_FOREGROUND_BOLD  0x0100
#define BASE_VIDEO_BACKGROUND_BOLD  0x0200
#define BASE_VIDEO_NEGATIVE         0x0400
#define BASE_VIDEO_CURSOR           0x0800

//
// Define base video font flags.
//

//
// Set this flag if the data is rotated by 90 degrees. That is, the first byte
// contains the first column of data, rather than the first rows. This is done
// to save space (as a 5x7 character can be listed as 5 bytes rather than 7).
// The maximum sized font allowing rotation is 8x8.
//

#define BASE_VIDEO_FONT_ROTATED 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

typedef ULONG BASE_VIDEO_COLOR, *PBASE_VIDEO_COLOR;

typedef enum _ANSI_COLOR {
    AnsiColorDefault,
    AnsiColorBlack,
    AnsiColorRed,
    AnsiColorGreen,
    AnsiColorYellow,
    AnsiColorBlue,
    AnsiColorMagenta,
    AnsiColorCyan,
    AnsiColorWhite,
    AnsiColorCount
} ANSI_COLOR, *PANSI_COLOR;

typedef enum _BASE_VIDEO_MODE {
    BaseVideoInvalidMode,
    BaseVideoModeFrameBuffer,
    BaseVideoModeBiosText,
} BASE_VIDEO_MODE, *PBASE_VIDEO_MODE;

/*++

Structure Description:

    This structure defines a base video console color pallette.

Members:

    AnsiColor - Stores the array of colors to use for each of the ANSI colors.
        The color in the default slot is used for the foreground only.

    BoldAnsiColor - Stores the array of colors to use for each of the ANSI
        colors when the bold attribute is on. The color in the default slot is
        used for the foreground only.

    DefaultBackground - Stores the default background color to use.

    DefaultBoldBackground - Stores the default bold background color to use.

    CursorText - Stores the text color to use when the cursor is over it.

    CursorBackground - Stores the background color to use for the cursor.

--*/

typedef struct _BASE_VIDEO_PALETTE {
    BASE_VIDEO_COLOR AnsiColor[AnsiColorCount];
    BASE_VIDEO_COLOR BoldAnsiColor[AnsiColorCount];
    BASE_VIDEO_COLOR DefaultBackground;
    BASE_VIDEO_COLOR DefaultBoldBackground;
    BASE_VIDEO_COLOR CursorText;
    BASE_VIDEO_COLOR CursorBackground;
} BASE_VIDEO_PALETTE, *PBASE_VIDEO_PALETTE;

/*++

Structure Description:

    This structure defines a basic base video color palette, for those that
    don't feel like redefining all the colors. Default values will be used for
    colors that are represented in the full color palette structure but not
    this one.

Members:

    DefaultForeground - Stores the default foreground color.

    DefaultBoldForeground - Stores the default bold foreground color.

    DefaultBackground - Stores the default background color.

    DefaultBoldBackground - Stores the default bold background color.

    CursorText - Stores the text color to use when the cursor is over it.

    CursorBackground - Stores the background color to use for the cursor.

--*/

typedef struct _BASE_VIDEO_PARTIAL_PALETTE {
    BASE_VIDEO_COLOR DefaultForeground;
    BASE_VIDEO_COLOR DefaultBoldForeground;
    BASE_VIDEO_COLOR DefaultBackground;
    BASE_VIDEO_COLOR DefaultBoldBackground;
    BASE_VIDEO_COLOR CursorText;
    BASE_VIDEO_COLOR CursorBackground;
} BASE_VIDEO_PARTIAL_PALETTE, *PBASE_VIDEO_PARTIAL_PALETTE;

/*++

Structure Description:

    This structure defines a single character cell in the base video library.

Members:

    Attributes - Stores the character attributes.

    Character - Stores the character.

--*/

typedef struct _BASE_VIDEO_CHARACTER_DATA {
    USHORT Attributes;
    USHORT Character;
} BASE_VIDEO_CHARACTER_DATA, *PBASE_VIDEO_CHARACTER_DATA;

/*++

Structure Description:

    This union defines a single character cell in the base video library.

Members:

    AsUint32 - Accesses the data as a single 32-bit value.

    Data - Access the data members.

--*/

typedef union _BASE_VIDEO_CHARACTER {
    ULONG AsUint32;
    BASE_VIDEO_CHARACTER_DATA Data;
} BASE_VIDEO_CHARACTER, *PBASE_VIDEO_CHARACTER;

/*++

Structure Description:

    This structure defines a base video font.

Members:

    GlyphCount - Stores the number of glyphs in the data.

    FirstAsciiCode - Stores the ASCII code of the first glyph. Usually this is
        a space (0x20).

    GlyphBytesWidth - Stores the number of bytes of data in a character row.
        For rotated fonts, this is the number of bytes in a character column.

    GlyphWidth - Stores the width of the glyph data, in pixels.

    CellWidth - Stores the width of a character cell in pixels.

    GlyphHeight - Stores the height of a glyph, in pixels.

    CellHeight - Stores the height of a character cell, in pixels.

    Flags - Stores a bitfield of flags. See BASE_VIDEO_FONT_* definitions.

    Data - Stores a pointer to the font data itself.

--*/

typedef struct _BASE_VIDEO_FONT {
    UCHAR GlyphCount;
    UCHAR FirstAsciiCode;
    UCHAR GlyphBytesWidth;
    UCHAR GlyphWidth;
    UCHAR CellWidth;
    UCHAR GlyphHeight;
    UCHAR CellHeight;
    ULONG Flags;
    const UCHAR *Data;
} BASE_VIDEO_FONT, *PBASE_VIDEO_FONT;

/*++

Structure Description:

    This structure stores the context for a base video frame buffer.

Members:

    Mode - Stores the mode of the frame buffer.

    FrameBuffer - Stores the pointer to the linear frame buffer itself.

    Width - Stores the width of the visible area of the frame buffer in pixels.
        For text mode frame buffers, this is the screen width in character
        columns.

    Height - Stores the height of the frame buffer in pixels. For text mode
        frame buffers, this is the screen height in character rows.

    BitsPerPixel - Stores the number of bits in a pixel.

    PixelsPerScanLine - Stores the number of pixels in a line, both visible
        and invisible.

    RedMask - Stores the set of bits that represent the red channel in each
        pixel.

    GreenMask - Stores the set of bits that represent the green channel in
        each pixel.

    BlueMask - Stores the set of bits that represent the blue channel in each
        pixel.

    Palette - Stores the current palette, with colors in "idealized" form.

    PhysicalPalette - Stores the current palette, with colors in actual device
        pixel form.

    Font - Stores a pointer to the font information.

    Columns - Stores the number of text columns in the frame buffer.

    Rows - Stores the number of rows in the frame buffer.

--*/

typedef struct _BASE_VIDEO_CONTEXT {
    BASE_VIDEO_MODE Mode;
    PVOID FrameBuffer;
    ULONG Width;
    ULONG Height;
    ULONG BitsPerPixel;
    ULONG PixelsPerScanLine;
    ULONG RedMask;
    ULONG GreenMask;
    ULONG BlueMask;
    BASE_VIDEO_PALETTE Palette;
    BASE_VIDEO_PALETTE PhysicalPalette;
    PBASE_VIDEO_FONT Font;
    ULONG Columns;
    ULONG Rows;
} BASE_VIDEO_CONTEXT, *PBASE_VIDEO_CONTEXT;

//
// -------------------------------------------------------------------- Globals
//

//
// Define the different fonts that can be used.
//

extern BASE_VIDEO_FONT VidFontVga8x16;
extern BASE_VIDEO_FONT VidFontVga9x16;
extern BASE_VIDEO_FONT VidFontVerite8x16;
extern BASE_VIDEO_FONT VidFontPs2Thin48x16;
extern BASE_VIDEO_FONT VidFontIso8x16;
extern BASE_VIDEO_FONT VidFont6x8;
extern BASE_VIDEO_FONT VidFont4x6;

//
// Store a pointer to the default font to use when initializing new video
// contexts.
//

extern PBASE_VIDEO_FONT VidDefaultFont;

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
VidInitialize (
    PBASE_VIDEO_CONTEXT Context,
    PSYSTEM_RESOURCE_FRAME_BUFFER FrameBuffer
    );

/*++

Routine Description:

    This routine initializes the base video library.

Arguments:

    Context - Supplies a pointer to the video context to initialize.

    FrameBuffer - Supplies a pointer to the frame buffer parameters.

Return Value:

    Status code.

--*/

VOID
VidClearScreen (
    PBASE_VIDEO_CONTEXT Context,
    ULONG MinimumX,
    ULONG MinimumY,
    ULONG MaximumX,
    ULONG MaximumY
    );

/*++

Routine Description:

    This routine clears a region of the screen, filling it with the default
    fill character. If no frame buffer is present, this is a no-op.

Arguments:

    Context - Supplies a pointer to the initialized base video context.

    MinimumX - Supplies the minimum X coordinate of the rectangle to clear,
        inclusive.

    MinimumY - Supplies the minimum Y coordinate of the rectangle to clear,
        inclusive.

    MaximumX - Supplies the maximum X coordinate of the rectangle to clear,
        exclusive.

    MaximumY - Supplies the maximum Y coordinate of the rectangle to clear,
        exclusive.

Return Value:

    None.

--*/

VOID
VidPrintString (
    PBASE_VIDEO_CONTEXT Context,
    ULONG XCoordinate,
    ULONG YCoordinate,
    PCSTR String
    );

/*++

Routine Description:

    This routine prints a null-terminated string to the screen at the
    specified location.

Arguments:

    Context - Supplies a pointer to the initialized base video context.

    XCoordinate - Supplies the X coordinate of the location on the screen
        to write to.

    YCoordinate - Supplies the Y cooordinate of the location on the screen
        to write to.

    String - Supplies the string to print.

Return Value:

    None.

--*/

VOID
VidPrintHexInteger (
    PBASE_VIDEO_CONTEXT Context,
    ULONG XCoordinate,
    ULONG YCoordinate,
    ULONG Number
    );

/*++

Routine Description:

    This routine prints an integer to the screen in the specified location.

Arguments:

    Context - Supplies a pointer to the initialized base video context.

    XCoordinate - Supplies the X coordinate of the location on the screen
        to write to.

    YCoordinate - Supplies the Y cooordinate of the location on the screen
        to write to.

    Number - Supplies the signed integer to print.

Return Value:

    None.

--*/

VOID
VidPrintInteger (
    PBASE_VIDEO_CONTEXT Context,
    ULONG XCoordinate,
    ULONG YCoordinate,
    LONG Number
    );

/*++

Routine Description:

    This routine prints an integer to the screen in the specified location.

Arguments:

    Context - Supplies a pointer to the initialized base video context.

    XCoordinate - Supplies the X coordinate of the location on the screen
        to write to.

    YCoordinate - Supplies the Y cooordinate of the location on the screen
        to write to.

    Number - Supplies the signed integer to print.

Return Value:

    None.

--*/

VOID
VidPrintCharacters (
    PBASE_VIDEO_CONTEXT Context,
    ULONG XCoordinate,
    ULONG YCoordinate,
    PBASE_VIDEO_CHARACTER Characters,
    ULONG Count
    );

/*++

Routine Description:

    This routine prints a set of characters.

Arguments:

    Context - Supplies a pointer to the initialized base video context.

    XCoordinate - Supplies the X coordinate of the location on the screen
        to write to.

    YCoordinate - Supplies the Y cooordinate of the location on the screen
        to write to.

    Characters - Supplies a pointer to the array of characters to write.

    Count - Supplies the number of characters in the array.

Return Value:

    None.

--*/

VOID
VidSetPalette (
    PBASE_VIDEO_CONTEXT Context,
    PBASE_VIDEO_PALETTE Palette,
    PBASE_VIDEO_PALETTE OldPalette
    );

/*++

Routine Description:

    This routine sets the current video palette. It is the caller's
    responsibility to synchronize both with printing and clearing the screen.

Arguments:

    Context - Supplies a pointer to the initialized base video context.

    Palette - Supplies a pointer to the palette to set. This memory will be
        copied.

    OldPalette - Supplies an optional pointer where the old palette data will
        be returned.

Return Value:

    None.

--*/

VOID
VidSetPartialPalette (
    PBASE_VIDEO_CONTEXT Context,
    PBASE_VIDEO_PARTIAL_PALETTE PartialPalette
    );

/*++

Routine Description:

    This routine sets the current video palette. It is the caller's
    responsibility to synchronize both with printing and clearing the screen.

Arguments:

    Context - Supplies a pointer to the initialized base video context.

    PartialPalette - Supplies a pointer to the palette to set. This memory will
        be copied. Values in the palette not specified here will be left
        unchanged.

Return Value:

    None.

--*/

VOID
VidGetPalette (
    PBASE_VIDEO_CONTEXT Context,
    PBASE_VIDEO_PALETTE Palette
    );

/*++

Routine Description:

    This routine gets the current video palette. It is the caller's
    responsibility to synchronize with anyone else that might be changing the
    palette.

Arguments:

    Context - Supplies a pointer to the initialized base video context.

    Palette - Supplies a pointer where the palette will be returned.

Return Value:

    None.

--*/

