/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

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
// Define the height and width of a character with the base video font.
//

#define BASE_VIDEO_CHARACTER_WIDTH 8
#define BASE_VIDEO_CHARACTER_HEIGHT 16

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

//
// -------------------------------------------------------------------- Globals
//

//
// The font data is stored as one very wide and fat 1 bit-per-pixel bitmap,
// starting at the upper left corner and scanning right. At a width of 8
// pixels per character (this is an 8x16 font), this means that these first few
// bytes are each the top line of a character. The last few bytes are the
// bottom lines of the last few characters. There are 256 characters.
//
//

extern BYTE VidFontData[];

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
VidInitialize (
    PSYSTEM_RESOURCE_FRAME_BUFFER FrameBuffer
    );

/*++

Routine Description:

    This routine initializes the base video library.

Arguments:

    FrameBuffer - Supplies a pointer to the frame buffer parameters.

Return Value:

    None.

--*/

VOID
VidClearScreen (
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
    ULONG XCoordinate,
    ULONG YCoordinate,
    PSTR String
    );

/*++

Routine Description:

    This routine prints a null-terminated string to the screen at the
    specified location. If no frame buffer is available, this output is
    redirected to the debugger.

Arguments:

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
    ULONG XCoordinate,
    ULONG YCoordinate,
    ULONG Number
    );

/*++

Routine Description:

    This routine prints an integer to the screen in the specified location.
    If no frame buffer is available, this output is redirected to the debugger.

Arguments:

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
    ULONG XCoordinate,
    ULONG YCoordinate,
    LONG Number
    );

/*++

Routine Description:

    This routine prints an integer to the screen in the specified location. If
    no frame buffer is available, this output is redirected to the debugger.

Arguments:

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
    ULONG XCoordinate,
    ULONG YCoordinate,
    PBASE_VIDEO_CHARACTER Characters,
    ULONG Count
    );

/*++

Routine Description:

    This routine prints a set of characters.

Arguments:

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
    PBASE_VIDEO_PALETTE Palette,
    PBASE_VIDEO_PALETTE OldPalette
    );

/*++

Routine Description:

    This routine sets the current video palette. It is the caller's
    responsibility to synchronize both with printing and clearing the screen.

Arguments:

    Palette - Supplies a pointer to the palette to set. This memory will be
        copied.

    OldPalette - Supplies an optional pointer where the old palette data will
        be returned.

Return Value:

    None.

--*/

VOID
VidSetPartialPalette (
    PBASE_VIDEO_PARTIAL_PALETTE PartialPalette
    );

/*++

Routine Description:

    This routine sets the current video palette. It is the caller's
    responsibility to synchronize both with printing and clearing the screen.

Arguments:

    PartialPalette - Supplies a pointer to the palette to set. This memory will
        be copied. Values in the palette not specified here will be left
        unchanged.

Return Value:

    None.

--*/

VOID
VidGetPalette (
    PBASE_VIDEO_PALETTE Palette
    );

/*++

Routine Description:

    This routine gets the current video palette. It is the caller's
    responsibility to synchronize with anyone else that might be changing the
    palette.

Arguments:

    Palette - Supplies a pointer where the palette will be returned.

Return Value:

    None.

--*/

