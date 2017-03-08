/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    textvid.c

Abstract:

    This module implements a fake firmware interface for ARM systems that have
    no firmware services layer.

Author:

    Evan Green 30-Jan-2013

Environment:

    Boot and Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "basevidp.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This routine converts a color value from software form into its physical
// form.
//

#define TRANSLATE_COLOR(_Value, _Translation)                                  \
    (((SHIFT_COLOR(_Value, _Translation.RedShift) & _Translation.RedMask)) |   \
     ((SHIFT_COLOR(_Value, _Translation.GreenShift) &                          \
       _Translation.GreenMask)) |                                              \
     ((SHIFT_COLOR(_Value, _Translation.BlueShift) & _Translation.BlueMask)))

//
// This routine shifts a value left by the given amount (right if the shift is
// negative).
//

#define SHIFT_COLOR(_Value, _Shift) \
    (((_Shift) >= 0) ? ((_Value) << (_Shift)) : ((_Value) >> -(_Shift)))

//
// This macro just flips red and blue.
//

#define SWIZZLE_RED_BLUE(_Value)                                          \
    ((((_Value) & 0x000000FF) << 16) | (((_Value) & 0x00FF0000) >> 16) |  \
     ((_Value) & 0x0000FF00))

//
// This macro converts a foreground and background color into attributes
// (upper 8 of 16 bits) for BIOS text mode.
//

#define BIOS_TEXT_ATTRIBUTES(_Foreground, _Background) \
    ((((_Foreground) & 0xF) << 8) | (((_Background) & 0x7) << 12))

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
VidpConvertPalette (
    PBASE_VIDEO_CONTEXT Context,
    PBASE_VIDEO_PALETTE Palette,
    PBASE_VIDEO_PALETTE PhysicalPalette
    );

VOID
VidpPrintCharacter (
    PBASE_VIDEO_CONTEXT Context,
    ULONG XCoordinate,
    ULONG YCoordinate,
    PBASE_VIDEO_CHARACTER Character
    );

VOID
VidpConvertIntegerToString (
    LONG Integer,
    PSTR String,
    ULONG Base
    );

ULONG
VidpFindHighestBitSet (
    ULONG Value
    );

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _COLOR_TRANSLATION {
    LONG RedShift;
    ULONG RedMask;
    LONG GreenShift;
    ULONG GreenMask;
    LONG BlueShift;
    ULONG BlueMask;
} COLOR_TRANSLATION, *PCOLOR_TRANSLATION;

//
// -------------------------------------------------------------------- Globals
//

//
// Store the default palette to use.
//

BASE_VIDEO_PALETTE VidDefaultPalette = {
    {
        BASE_VIDEO_COLOR_RGB(255, 240, 165),
        BASE_VIDEO_COLOR_RGB(0, 0, 0),
        BASE_VIDEO_COLOR_RGB(134, 37, 23),
        BASE_VIDEO_COLOR_RGB(37, 188, 36),
        BASE_VIDEO_COLOR_RGB(173, 173, 39),
        BASE_VIDEO_COLOR_RGB(50, 27, 184),
        BASE_VIDEO_COLOR_RGB(134, 30, 134),
        BASE_VIDEO_COLOR_RGB(47, 204, 197),
        BASE_VIDEO_COLOR_RGB(203, 204, 206),
    },

    {
        BASE_VIDEO_COLOR_RGB(255, 255, 170),
        BASE_VIDEO_COLOR_RGB(142, 145, 149),
        BASE_VIDEO_COLOR_RGB(255, 120, 100),
        BASE_VIDEO_COLOR_RGB(49, 231, 34),
        BASE_VIDEO_COLOR_RGB(234, 236, 35),
        BASE_VIDEO_COLOR_RGB(70, 160, 255),
        BASE_VIDEO_COLOR_RGB(240, 100, 240),
        BASE_VIDEO_COLOR_RGB(20, 240, 240),
        BASE_VIDEO_COLOR_RGB(233, 235, 237),
    },

    BASE_VIDEO_COLOR_RGB(19, 119, 61),
    BASE_VIDEO_COLOR_RGB(19, 119, 61),
    BASE_VIDEO_COLOR_RGB(255, 240, 165),
    BASE_VIDEO_COLOR_RGB(142, 40, 0),
};

//
// Store a pointer to the default font to use.
//

PBASE_VIDEO_FONT VidDefaultFont = &VidFontPs2Thin48x16;

//
// Store the conversion between ANSI colors and BIOS text attribute numbers.
//

const UCHAR VidTextModeColors[AnsiColorCount] = {7, 0, 4, 2, 6, 1, 5, 3, 7};

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
VidInitialize (
    PBASE_VIDEO_CONTEXT Context,
    PSYSTEM_RESOURCE_FRAME_BUFFER FrameBuffer
    )

/*++

Routine Description:

    This routine initializes the base video library.

Arguments:

    Context - Supplies a pointer to the video context to initialize.

    FrameBuffer - Supplies a pointer to the frame buffer parameters.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Context->Mode = FrameBuffer->Mode;
    Context->FrameBuffer = FrameBuffer->Header.VirtualAddress;
    Context->Width = FrameBuffer->Width;
    Context->Height = FrameBuffer->Height;
    Context->PixelsPerScanLine = FrameBuffer->PixelsPerScanLine;
    Context->BitsPerPixel = FrameBuffer->BitsPerPixel;
    Context->RedMask = FrameBuffer->RedMask;
    Context->GreenMask = FrameBuffer->GreenMask;
    Context->BlueMask = FrameBuffer->BlueMask;

    ASSERT((Context->Mode != BaseVideoInvalidMode) &&
           (Context->FrameBuffer != NULL) &&
           (Context->Width != 0) &&
           (Context->Height != 0) &&
           (Context->PixelsPerScanLine >= Context->Width) &&
           (Context->BitsPerPixel != 0));

    ASSERT((Context->Mode != BaseVideoModeFrameBuffer) ||
           ((Context->RedMask != 0) &&
            (Context->GreenMask != 0) &&
            (Context->BlueMask != 0)));

    RtlCopyMemory(&(Context->Palette),
                  &VidDefaultPalette,
                  sizeof(BASE_VIDEO_PALETTE));

    //
    // The default palette is pretty unreadable when reduced down to 8 bits.
    // Set the background to black as a compromise, giving things kind of an
    // old school CRT look.
    //

    if (Context->BitsPerPixel <= 8) {
        Context->Palette.DefaultBackground = BASE_VIDEO_COLOR_RGB(0, 0, 0);
        Context->Palette.DefaultBoldBackground =
                                            Context->Palette.DefaultBackground;
    }

    VidpConvertPalette(Context,
                       &(Context->Palette),
                       &Context->PhysicalPalette);

    Context->Font = VidDefaultFont;

    ASSERT(Context->Font != NULL);

    if (Context->Mode == BaseVideoModeBiosText) {
        Context->Columns = Context->Width;
        Context->Rows = Context->Height;

    } else {
        Context->Columns = Context->Width / Context->Font->CellWidth;
        Context->Rows = Context->Height / Context->Font->CellHeight;
    }

    Status = STATUS_SUCCESS;
    return Status;
}

VOID
VidClearScreen (
    PBASE_VIDEO_CONTEXT Context,
    ULONG MinimumX,
    ULONG MinimumY,
    ULONG MaximumX,
    ULONG MaximumY
    )

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

{

    BASE_VIDEO_CHARACTER Character;
    ULONG Color;
    ULONG HorizontalIndex;
    PULONG Pixel;
    PUSHORT Pixel16;
    PUCHAR Pixel8;
    ULONG VerticalIndex;

    if (Context->FrameBuffer == NULL) {
        return;
    }

    //
    // If either minimum value is off the screen, exit.
    //

    if (MinimumX >= Context->Width) {
        return;
    }

    if (MinimumY >= Context->Height) {
        return;
    }

    //
    // Truncate the maximum values.
    //

    if (MaximumX > Context->Width) {
        MaximumX = Context->Width;
    }

    if (MaximumY > Context->Height) {
        MaximumY = Context->Height;
    }

    //
    // Handle text mode by running around printing spaces.
    //

    if (Context->Mode == BaseVideoModeBiosText) {
        Character.Data.Character = ' ';
        Character.Data.Attributes = 0;
        for (VerticalIndex = MinimumY;
             VerticalIndex < MaximumY;
             VerticalIndex += 1) {

            for (HorizontalIndex = MinimumX;
                 HorizontalIndex < MaximumX;
                 HorizontalIndex += 1) {

                VidpPrintCharacter(Context,
                                   HorizontalIndex,
                                   VerticalIndex,
                                   &Character);
            }
        }

        return;
    }

    Color = Context->PhysicalPalette.DefaultBackground;

    //
    // Switch on the bits per pixel outside the hot inner loop.
    //

    switch (Context->BitsPerPixel) {
    case 8:
        for (VerticalIndex = MinimumY;
             VerticalIndex < MaximumY;
             VerticalIndex += 1) {

            Pixel = (PULONG)(Context->FrameBuffer +
                             (((VerticalIndex *
                                Context->PixelsPerScanLine) +
                                MinimumX) *
                              (Context->BitsPerPixel / BITS_PER_BYTE)));

            Pixel8 = (PUCHAR)Pixel;
            for (HorizontalIndex = MinimumX;
                 HorizontalIndex < MaximumX;
                 HorizontalIndex += 1) {

                *Pixel8 = (UCHAR)Color;
                Pixel8 += 1;
            }
        }

        break;

    case 16:
        for (VerticalIndex = MinimumY;
             VerticalIndex < MaximumY;
             VerticalIndex += 1) {

            Pixel = (PULONG)(Context->FrameBuffer +
                             (((VerticalIndex *
                                Context->PixelsPerScanLine) +
                               MinimumX) *
                              (Context->BitsPerPixel / BITS_PER_BYTE)));

            Pixel16 = (PUSHORT)Pixel;
            for (HorizontalIndex = MinimumX;
                 HorizontalIndex < MaximumX;
                 HorizontalIndex += 1) {

                *Pixel16 = (USHORT)Color;
                Pixel16 += 1;
            }
        }

        break;

    case 24:
        for (VerticalIndex = MinimumY;
             VerticalIndex < MaximumY;
             VerticalIndex += 1) {

            Pixel = (PULONG)(Context->FrameBuffer +
                             (((VerticalIndex *
                                Context->PixelsPerScanLine) +
                               MinimumX) *
                              (Context->BitsPerPixel / BITS_PER_BYTE)));

            Pixel8 = (PUCHAR)Pixel;
            for (HorizontalIndex = MinimumX;
                 HorizontalIndex < MaximumX;
                 HorizontalIndex += 1) {

                *Pixel8 = (UCHAR)Color;
                *(Pixel8 + 1) = (UCHAR)(Color >> 8);
                *(Pixel8 + 2) = (UCHAR)(Color >> 16);
                Pixel8 += 3;
            }
        }

        break;

    case 32:
        for (VerticalIndex = MinimumY;
             VerticalIndex < MaximumY;
             VerticalIndex += 1) {

            Pixel = (PULONG)(Context->FrameBuffer +
                             (((VerticalIndex *
                                Context->PixelsPerScanLine) +
                               MinimumX) *
                              (Context->BitsPerPixel / BITS_PER_BYTE)));

            for (HorizontalIndex = MinimumX;
                 HorizontalIndex < MaximumX;
                 HorizontalIndex += 1) {

                *Pixel = Color;
                Pixel += 1;
            }
        }

        break;

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

VOID
VidPrintString (
    PBASE_VIDEO_CONTEXT Context,
    ULONG XCoordinate,
    ULONG YCoordinate,
    PCSTR String
    )

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

{

    BASE_VIDEO_CHARACTER Character;
    ULONG Columns;
    ULONG Rows;

    if (Context->FrameBuffer == NULL) {
        return;
    }

    Columns = Context->Columns;
    Rows = Context->Rows;
    Character.AsUint32 = 0;
    while (*String != '\0') {
        Character.Data.Character = *String;
        VidpPrintCharacter(Context, XCoordinate, YCoordinate, &Character);
        XCoordinate += 1;
        if (XCoordinate >= Columns) {
            XCoordinate = 0;
            YCoordinate += 1;
        }

        if (YCoordinate >= Rows) {
            YCoordinate = 0;
        }

        String += 1;
    }

    return;
}

VOID
VidPrintHexInteger (
    PBASE_VIDEO_CONTEXT Context,
    ULONG XCoordinate,
    ULONG YCoordinate,
    ULONG Number
    )

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

{

    CHAR StringBuffer[30];

    VidpConvertIntegerToString(Number, StringBuffer, 16);
    VidPrintString(Context, XCoordinate, YCoordinate, StringBuffer);
    return;
}

VOID
VidPrintInteger (
    PBASE_VIDEO_CONTEXT Context,
    ULONG XCoordinate,
    ULONG YCoordinate,
    LONG Number
    )

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

{

    CHAR StringBuffer[30];

    VidpConvertIntegerToString(Number, StringBuffer, 10);
    VidPrintString(Context, XCoordinate, YCoordinate, StringBuffer);
    return;
}

VOID
VidPrintCharacters (
    PBASE_VIDEO_CONTEXT Context,
    ULONG XCoordinate,
    ULONG YCoordinate,
    PBASE_VIDEO_CHARACTER Characters,
    ULONG Count
    )

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

{

    ULONG Columns;
    ULONG Index;
    ULONG Rows;

    Columns = Context->Columns;
    Rows = Context->Rows;
    for (Index = 0; Index < Count; Index += 1) {
        VidpPrintCharacter(Context,
                           XCoordinate,
                           YCoordinate,
                           Characters + Index);

        XCoordinate += 1;
        if (XCoordinate >= Columns) {
            XCoordinate = 0;
            YCoordinate += 1;
        }

        if (YCoordinate >= Rows) {
            YCoordinate = 0;
        }
    }

    return;
}

VOID
VidSetPalette (
    PBASE_VIDEO_CONTEXT Context,
    PBASE_VIDEO_PALETTE Palette,
    PBASE_VIDEO_PALETTE OldPalette
    )

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

{

    if (OldPalette != NULL) {
        RtlCopyMemory(OldPalette,
                      &(Context->Palette),
                      sizeof(BASE_VIDEO_PALETTE));
    }

    RtlCopyMemory(&(Context->Palette), Palette, sizeof(BASE_VIDEO_PALETTE));
    VidpConvertPalette(Context,
                       &(Context->Palette),
                       &(Context->PhysicalPalette));

    return;
}

VOID
VidSetPartialPalette (
    PBASE_VIDEO_CONTEXT Context,
    PBASE_VIDEO_PARTIAL_PALETTE PartialPalette
    )

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

{

    BASE_VIDEO_PALETTE Palette;

    VidGetPalette(Context, &Palette);
    Palette.AnsiColor[AnsiColorDefault] = PartialPalette->DefaultForeground;
    Palette.BoldAnsiColor[AnsiColorDefault] =
                                         PartialPalette->DefaultBoldForeground;

    Palette.DefaultBackground = PartialPalette->DefaultBackground;
    Palette.DefaultBoldBackground = PartialPalette->DefaultBoldBackground;
    Palette.CursorText = PartialPalette->CursorText;
    Palette.CursorBackground = PartialPalette->CursorBackground;
    VidSetPalette(Context, &Palette, NULL);
    return;
}

VOID
VidGetPalette (
    PBASE_VIDEO_CONTEXT Context,
    PBASE_VIDEO_PALETTE Palette
    )

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

{

    RtlCopyMemory(Palette, &(Context->Palette), sizeof(BASE_VIDEO_PALETTE));
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
VidpConvertPalette (
    PBASE_VIDEO_CONTEXT Context,
    PBASE_VIDEO_PALETTE Palette,
    PBASE_VIDEO_PALETTE PhysicalPalette
    )

/*++

Routine Description:

    This routine converts the given palette into a physical palette that uses
    the native pixel format.

Arguments:

    Context - Supplies a pointer to the initialized base video context.

    Palette - Supplies the natural palette to convert.

    PhysicalPalette - Supplies a pointer where the palette in native pixel
        format will be returned.

Return Value:

    None.

--*/

{

    ULONG BlueBit;
    ULONG BlueMask;
    ULONG ColorIndex;
    ULONG GreenBit;
    ULONG GreenMask;
    ULONG RedBit;
    ULONG RedMask;
    COLOR_TRANSLATION Translation;

    if (Context->Mode == BaseVideoModeBiosText) {
        for (ColorIndex = 0; ColorIndex < AnsiColorCount; ColorIndex += 1) {
            PhysicalPalette->AnsiColor[ColorIndex] =
                                                 VidTextModeColors[ColorIndex];

            PhysicalPalette->BoldAnsiColor[ColorIndex] =
                                             VidTextModeColors[ColorIndex] + 8;
        }

        PhysicalPalette->DefaultBackground = VidTextModeColors[AnsiColorBlack];
        PhysicalPalette->DefaultBoldBackground =
                                            VidTextModeColors[AnsiColorWhite];

        PhysicalPalette->CursorText = PhysicalPalette->DefaultBackground;
        PhysicalPalette->CursorBackground =
                                  PhysicalPalette->AnsiColor[AnsiColorDefault];

        goto ConvertPaletteEnd;
    }

    ASSERT(Context->Mode == BaseVideoModeFrameBuffer);

    RedMask = Context->RedMask;
    GreenMask = Context->GreenMask;
    BlueMask = Context->BlueMask;

    ASSERT((RedMask != 0) && (GreenMask != 0) && (BlueMask != 0));

    //
    // Handle some common cases.
    //

    if ((RedMask == 0x00FF0000) && (GreenMask == 0x0000FF00) &&
        (BlueMask == 0x000000FF)) {

        RtlCopyMemory(PhysicalPalette, Palette, sizeof(BASE_VIDEO_PALETTE));
        return;

    } else if ((RedMask == 0x000000FF) && (GreenMask == 0x0000FF00) &&
               (BlueMask == 0x00FF0000)) {

        for (ColorIndex = 0; ColorIndex < AnsiColorCount; ColorIndex += 1) {
            PhysicalPalette->AnsiColor[ColorIndex] =
                              SWIZZLE_RED_BLUE(Palette->AnsiColor[ColorIndex]);

            PhysicalPalette->BoldAnsiColor[ColorIndex] =
                          SWIZZLE_RED_BLUE(Palette->BoldAnsiColor[ColorIndex]);
        }

        PhysicalPalette->DefaultBackground =
                                  SWIZZLE_RED_BLUE(Palette->DefaultBackground);

        PhysicalPalette->DefaultBoldBackground =
                              SWIZZLE_RED_BLUE(Palette->DefaultBoldBackground);

        PhysicalPalette->CursorText = SWIZZLE_RED_BLUE(Palette->CursorText);
        PhysicalPalette->CursorBackground =
                                   SWIZZLE_RED_BLUE(Palette->CursorBackground);

        return;
    }

    //
    // Create the translation shift and mask.
    //

    Translation.RedMask = RedMask;
    Translation.GreenMask = GreenMask;
    Translation.BlueMask = BlueMask;
    RedBit = VidpFindHighestBitSet(Translation.RedMask) + 1;
    GreenBit = VidpFindHighestBitSet(Translation.GreenMask) + 1;
    BlueBit = VidpFindHighestBitSet(Translation.BlueMask) + 1;
    Translation.RedShift = RedBit - (3 * BITS_PER_BYTE);
    Translation.GreenShift = GreenBit - (2 * BITS_PER_BYTE);
    Translation.BlueShift = BlueBit - BITS_PER_BYTE;

    //
    // Translate all the colors in the palette.
    //

    for (ColorIndex = 0; ColorIndex < AnsiColorCount; ColorIndex += 1) {
        PhysicalPalette->AnsiColor[ColorIndex] =
                  TRANSLATE_COLOR(Palette->AnsiColor[ColorIndex], Translation);

        PhysicalPalette->BoldAnsiColor[ColorIndex] =
              TRANSLATE_COLOR(Palette->BoldAnsiColor[ColorIndex], Translation);
    }

    PhysicalPalette->DefaultBackground =
                      TRANSLATE_COLOR(Palette->DefaultBackground, Translation);

    PhysicalPalette->DefaultBoldBackground =
                  TRANSLATE_COLOR(Palette->DefaultBoldBackground, Translation);

    PhysicalPalette->CursorText =
                             TRANSLATE_COLOR(Palette->CursorText, Translation);

    PhysicalPalette->CursorBackground =
                       TRANSLATE_COLOR(Palette->CursorBackground, Translation);

ConvertPaletteEnd:
    return;
}

VOID
VidpPrintCharacter (
    PBASE_VIDEO_CONTEXT Context,
    ULONG XCoordinate,
    ULONG YCoordinate,
    PBASE_VIDEO_CHARACTER Character
    )

/*++

Routine Description:

    This routine prints a character to the screen in the specified location.

Arguments:

    Context - Supplies a pointer to the initialized base video context.

    XCoordinate - Supplies the column to write to.

    YCoordinate - Supplies the row to write to.

    Character - Supplies a pointer to the character to print.

Return Value:

    None.

--*/

{

    USHORT Attributes;
    ANSI_COLOR BackgroundAnsiColor;
    ULONG BitIndex;
    ULONG ByteIndex;
    ULONG ColorOff;
    ULONG ColorOn;
    ULONG ColumnIndex;
    PUCHAR Data;
    PUSHORT Destination16;
    PULONG Destination32;
    PBYTE Destination8;
    PBASE_VIDEO_FONT Font;
    ANSI_COLOR ForegroundAnsiColor;
    ULONG HorizontalIndex;
    ULONG LineSize;
    PVOID LineStart;
    UCHAR RotateBuffer[8];
    ULONG RowIndex;
    BYTE Source;
    ULONG SourceIndex;
    ULONG SwapColor;
    ULONG VerticalIndex;
    ULONG YPixel;

    //
    // Get the colors to use.
    //

    ColorOn = Context->PhysicalPalette.AnsiColor[AnsiColorDefault];
    ColorOff = Context->PhysicalPalette.DefaultBackground;
    if (Character->Data.Attributes != 0) {
        Attributes = Character->Data.Attributes;
        if ((Attributes & BASE_VIDEO_CURSOR) != 0) {
            ColorOn = Context->PhysicalPalette.CursorText;
            ColorOff = Context->PhysicalPalette.CursorBackground;

        } else {
            BackgroundAnsiColor = (Attributes >> BASE_VIDEO_BACKGROUND_SHIFT) &
                                  BASE_VIDEO_COLOR_MASK;

            ForegroundAnsiColor = Attributes & BASE_VIDEO_COLOR_MASK;
            ColorOn = Context->PhysicalPalette.AnsiColor[ForegroundAnsiColor];
            if ((Attributes & BASE_VIDEO_FOREGROUND_BOLD) != 0) {
                ColorOn =
                    Context->PhysicalPalette.BoldAnsiColor[ForegroundAnsiColor];
            }

            if (BackgroundAnsiColor != AnsiColorDefault) {
                ColorOff =
                       Context->PhysicalPalette.AnsiColor[BackgroundAnsiColor];
            }

            if ((Attributes & BASE_VIDEO_BACKGROUND_BOLD) != 0) {
                ColorOff =
                    Context->PhysicalPalette.BoldAnsiColor[BackgroundAnsiColor];

                if (BackgroundAnsiColor == AnsiColorDefault) {
                    ColorOff = Context->PhysicalPalette.DefaultBoldBackground;
                }
            }

            if ((Attributes & BASE_VIDEO_NEGATIVE) != 0) {
                SwapColor = ColorOn;
                ColorOn = ColorOff;
                ColorOff = SwapColor;
            }
        }
    }

    //
    // Handle text mode differently.
    //

    if (Context->Mode == BaseVideoModeBiosText) {
        Destination16 = Context->FrameBuffer;
        Destination16 += (YCoordinate * Context->Width) + XCoordinate;
        *Destination16 = BIOS_TEXT_ATTRIBUTES(ColorOn, ColorOff) |
                         (UCHAR)(Character->Data.Character);

        return;
    }

    //
    // Get the glyph data for that character.
    //

    Font = Context->Font;
    if ((Character->Data.Character < Font->FirstAsciiCode) ||
        (Character->Data.Character >=
         Font->FirstAsciiCode + Font->GlyphCount)) {

        ASSERT(Font->FirstAsciiCode <= ' ');

        SourceIndex = ' ' - Font->FirstAsciiCode;

    } else {
        SourceIndex = Character->Data.Character - Font->FirstAsciiCode;
    }

    //
    // Rotate the character if needed. For those wondering, this code takes
    // about 185 bytes on x86, and the rotated data storage saves about 192
    // bytes for the 5x7 and 4x6 fonts each.
    //

    if ((Font->Flags & BASE_VIDEO_FONT_ROTATED) != 0) {
        SourceIndex *= Font->GlyphBytesWidth * Font->GlyphWidth;
        Data = (PUCHAR)&(Font->Data[SourceIndex]);

        ASSERT((Font->GlyphWidth < sizeof(RotateBuffer)) &&
               (Font->GlyphHeight < sizeof(RotateBuffer)) &&
               (sizeof(RotateBuffer) <= BITS_PER_BYTE));

        //
        // The normal data format runs horizontally. Build it a horizontal
        // row at a time (assuming there will be left than 8).
        //

        for (RowIndex = 0; RowIndex < Font->GlyphHeight; RowIndex += 1) {
            Source = 0;

            //
            // The row's data is spread out since the data is stored a column
            // at a time. It's always the same bit (row) for each column.
            //

            for (ColumnIndex = 0;
                 ColumnIndex < Font->GlyphWidth;
                 ColumnIndex += 1) {

                BitIndex = RowIndex;
                if ((Data[ColumnIndex] & (1 << BitIndex)) != 0) {
                    Source |= 1 << (BITS_PER_BYTE - 1 - ColumnIndex);
                }
            }

            RotateBuffer[RowIndex] = Source;
        }

        Data = RotateBuffer;

    } else {
        SourceIndex *= Font->GlyphBytesWidth * Font->GlyphHeight;
        Data = (PUCHAR)&(Font->Data[SourceIndex]);
    }

    //
    // Compute the starting address on the frame buffer.
    //

    YPixel = (YCoordinate * Font->CellHeight) * Context->PixelsPerScanLine;
    LineStart = Context->FrameBuffer +
                ((YPixel + (XCoordinate * Font->CellWidth)) *
                 (Context->BitsPerPixel / BITS_PER_BYTE));

    LineSize = Context->PixelsPerScanLine *
               (Context->BitsPerPixel / BITS_PER_BYTE);

    //
    // Separate write loops for different pixel widths does mean more code,
    // but it skips conditionals in the inner loops, which are very hot.
    //

    switch (Context->BitsPerPixel) {
    case 8:
        for (VerticalIndex = 0;
             VerticalIndex < Font->GlyphHeight;
             VerticalIndex += 1) {

            Destination8 = LineStart;
            HorizontalIndex = 0;
            for (ByteIndex = 0;
                 ByteIndex < Font->GlyphBytesWidth;
                 ByteIndex += 1) {

                Source = *Data;
                BitIndex = 0;
                while ((BitIndex < BITS_PER_BYTE) &&
                       (HorizontalIndex < Font->GlyphWidth)) {

                    if ((Source & 0x80) != 0) {
                        *Destination8 = (UCHAR)ColorOn;

                    } else {
                        *Destination8 = (UCHAR)ColorOff;
                    }

                    Destination8 += 1;
                    HorizontalIndex += 1;
                    Source <<= 1;
                    BitIndex += 1;
                }

                Data += 1;
            }

            while (HorizontalIndex < Font->CellWidth) {
                *Destination8 = (UCHAR)ColorOff;
                Destination8 += 1;
                HorizontalIndex += 1;
            }

            LineStart += LineSize;
        }

        while (VerticalIndex < Font->CellHeight) {
            Destination8 = LineStart;
            for (HorizontalIndex = 0;
                 HorizontalIndex < Font->CellWidth;
                 HorizontalIndex += 1) {

                *Destination8 = (UCHAR)ColorOff;
                Destination8 += 1;
            }

            LineStart += LineSize;
            VerticalIndex += 1;
        }

        break;

    case 16:
        for (VerticalIndex = 0;
             VerticalIndex < Font->GlyphHeight;
             VerticalIndex += 1) {

            Destination16 = LineStart;
            HorizontalIndex = 0;
            for (ByteIndex = 0;
                 ByteIndex < Font->GlyphBytesWidth;
                 ByteIndex += 1) {

                Source = *Data;
                BitIndex = 0;
                while ((BitIndex < BITS_PER_BYTE) &&
                       (HorizontalIndex < Font->GlyphWidth)) {

                    if ((Source & 0x80) != 0) {
                        *Destination16 = (USHORT)ColorOn;

                    } else {
                        *Destination16 = (USHORT)ColorOff;
                    }

                    Destination16 += 1;
                    HorizontalIndex += 1;
                    Source <<= 1;
                    BitIndex += 1;
                }

                Data += 1;
            }

            while (HorizontalIndex < Font->CellWidth) {
                *Destination16 = (USHORT)ColorOff;
                Destination16 += 1;
                HorizontalIndex += 1;
            }

            LineStart += LineSize;
        }

        while (VerticalIndex < Font->CellHeight) {
            Destination16 = LineStart;
            for (HorizontalIndex = 0;
                 HorizontalIndex < Font->CellWidth;
                 HorizontalIndex += 1) {

                *Destination16 = (USHORT)ColorOff;
                Destination16 += 1;
            }

            LineStart += LineSize;
            VerticalIndex += 1;
        }

        break;

    case 24:
        for (VerticalIndex = 0;
             VerticalIndex < Font->GlyphHeight;
             VerticalIndex += 1) {

            Destination8 = LineStart;
            HorizontalIndex = 0;
            for (ByteIndex = 0;
                 ByteIndex < Font->GlyphBytesWidth;
                 ByteIndex += 1) {

                Source = *Data;
                BitIndex = 0;
                while ((BitIndex < BITS_PER_BYTE) &&
                       (HorizontalIndex < Font->GlyphWidth)) {

                    if ((Source & 0x80) != 0) {
                        *Destination8 = (UCHAR)ColorOn;
                        *(Destination8 + 1) = (UCHAR)(ColorOn >> 8);
                        *(Destination8 + 2) = (UCHAR)(ColorOn >> 16);

                    } else {
                        *Destination8 = (UCHAR)ColorOff;
                        *(Destination8 + 1) = (UCHAR)(ColorOff >> 8);
                        *(Destination8 + 2) = (UCHAR)(ColorOff >> 16);
                    }

                    Destination8 += 3;
                    HorizontalIndex += 1;
                    Source <<= 1;
                    BitIndex += 1;
                }

                Data += 1;
            }

            while (HorizontalIndex < Font->CellWidth) {
                *Destination8 = (UCHAR)ColorOff;
                *(Destination8 + 1) = (UCHAR)(ColorOff >> 8);
                *(Destination8 + 2) = (UCHAR)(ColorOff >> 16);
                Destination8 += 3;
                HorizontalIndex += 1;
            }

            LineStart += LineSize;
        }

        while (VerticalIndex < Font->CellHeight) {
            Destination8 = LineStart;
            for (HorizontalIndex = 0;
                 HorizontalIndex < Font->CellWidth;
                 HorizontalIndex += 1) {

                *Destination8 = (UCHAR)ColorOff;
                *(Destination8 + 1) = (UCHAR)(ColorOff >> 8);
                *(Destination8 + 2) = (UCHAR)(ColorOff >> 16);
                Destination8 += 3;
            }

            LineStart += LineSize;
            VerticalIndex += 1;
        }

        break;

    case 32:
        for (VerticalIndex = 0;
             VerticalIndex < Font->GlyphHeight;
             VerticalIndex += 1) {

            Destination32 = LineStart;
            HorizontalIndex = 0;
            for (ByteIndex = 0;
                 ByteIndex < Font->GlyphBytesWidth;
                 ByteIndex += 1) {

                Source = *Data;
                BitIndex = 0;
                while ((BitIndex < BITS_PER_BYTE) &&
                       (HorizontalIndex < Font->GlyphWidth)) {

                    if ((Source & 0x80) != 0) {
                        *Destination32 = ColorOn;

                    } else {
                        *Destination32 = ColorOff;
                    }

                    Destination32 += 1;
                    HorizontalIndex += 1;
                    Source <<= 1;
                    BitIndex += 1;
                }

                Data += 1;
            }

            while (HorizontalIndex < Font->CellWidth) {
                *Destination32 = ColorOff;
                Destination32 += 1;
                HorizontalIndex += 1;
            }

            LineStart += LineSize;
        }

        while (VerticalIndex < Font->CellHeight) {
            Destination32 = LineStart;
            for (HorizontalIndex = 0;
                 HorizontalIndex < Font->CellWidth;
                 HorizontalIndex += 1) {

                *Destination32 = ColorOff;
                Destination32 += 1;
            }

            LineStart += LineSize;
            VerticalIndex += 1;
        }

        break;

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

VOID
VidpConvertIntegerToString (
    LONG Integer,
    PSTR String,
    ULONG Base
    )

/*++

Routine Description:

    This routine converts an integer to a string.

Arguments:

    Integer - Supplies the signed integer to convert to string format.

    String - Supplies a pointer to the allocated buffer where the string
        will be written.

    Base - Supplies the base of the number to print, from 2 to 16.

Return Value:

    None.

--*/

{

    ULONG CurrentPosition;
    BOOL Positive;

    CurrentPosition = 0;
    Positive = TRUE;

    //
    // Parameter checking.
    //

    if (String == NULL) {
        return;
    }

    if ((Base < 2) || (Base > 16)) {
        *String = '\0';
        return;
    }

    //
    // Only deal with positive numbers, but remember if the number was negative.
    //

    if ((Integer < 0) && (Base == 10)) {
        Positive = FALSE;
        Integer = -Integer;
    }

    //
    // Loop over the integer, getting the least significant digit each
    // iteration. Note that this causes the string to come out backwards, which
    // is why the string is reversed before it is returned.
    //

    do {
        String[CurrentPosition] = (Integer % Base) + '0';
        if (String[CurrentPosition] > '9') {
            String[CurrentPosition] = (Integer % Base) - 10 + 'A';
        }

        CurrentPosition += 1;
        Integer = Integer / Base;

    } while (Integer > 0);

    //
    // Print out the negative sign at the end.
    //

    if (Positive == FALSE) {
        String[CurrentPosition] = '-';
        CurrentPosition += 1;
    }

    //
    // Pad spaces to at least 8.
    //

    while (CurrentPosition < 8) {
        String[CurrentPosition] = ' ';
        CurrentPosition += 1;
    }

    //
    // Null terminate and reverse the string.
    //

    String[CurrentPosition] = '\0';
    RtlStringReverse(String, String + CurrentPosition);
    return;
}

ULONG
VidpFindHighestBitSet (
    ULONG Value
    )

/*++

Routine Description:

    This routine finds the highest bit set in the given 32-bit integer.

Arguments:

    Value - Supplies the value to find the highest bit number of.

Return Value:

    Returns the index of the highest bit set.

--*/

{

    ULONG Index;

    Index = (sizeof(ULONG) * BITS_PER_BYTE) - 1;
    while (TRUE) {
        if ((Value & (1 << Index)) != 0) {
            return Index;
        }

        if (Index == 0) {
            break;
        }

        Index -= 1;
    }

    return Index;
}

