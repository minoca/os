/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

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

#define RTL_API DLLEXPORT

#include <minoca/driver.h>
#include <minoca/bootload.h>
#include <minoca/basevid.h>

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
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
VidpConvertPalette (
    PBASE_VIDEO_PALETTE Palette,
    PBASE_VIDEO_PALETTE PhysicalPalette
    );

VOID
VidpPrintCharacter (
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
// Store a pointer to the frame buffer base address.
//

PVOID VidFrameBuffer = NULL;

//
// Store the width and height of the frame buffer, in pixels.
//

ULONG VidFrameBufferWidth = 0;
ULONG VidFrameBufferHeight = 0;
ULONG VidFrameBufferPixelsPerScanLine = 0;

//
// Store the number of bits in a pixel.
//

ULONG VidFrameBufferBitsPerPixel = 0;

//
// Store the pixel format in color masks.
//

ULONG VidRedMask = 0;
ULONG VidGreenMask = 0;
ULONG VidBlueMask = 0;

//
// Store the current palette, initialized with some default values.
//

BASE_VIDEO_PALETTE VidPalette = {
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
// Store the translated palette, which conforms to the device's actual pixel
// format.
//

BASE_VIDEO_PALETTE VidPhysicalPalette;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
VidInitialize (
    PSYSTEM_RESOURCE_FRAME_BUFFER FrameBuffer
    )

/*++

Routine Description:

    This routine initializes the base video library.

Arguments:

    FrameBuffer - Supplies a pointer to the frame buffer parameters.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    VidFrameBuffer = FrameBuffer->Header.VirtualAddress;
    VidFrameBufferWidth = FrameBuffer->Width;
    VidFrameBufferHeight = FrameBuffer->Height;
    VidFrameBufferPixelsPerScanLine = FrameBuffer->PixelsPerScanLine;
    VidFrameBufferBitsPerPixel = FrameBuffer->BitsPerPixel;
    VidRedMask = FrameBuffer->RedMask;
    VidGreenMask = FrameBuffer->GreenMask;
    VidBlueMask = FrameBuffer->BlueMask;

    ASSERT((VidFrameBuffer != NULL) &&
           (VidFrameBufferWidth != 0) &&
           (VidFrameBufferHeight != 0) &&
           (VidFrameBufferPixelsPerScanLine >= VidFrameBufferWidth) &&
           (VidFrameBufferBitsPerPixel != 0) &&
           (VidRedMask != 0) &&
           (VidGreenMask != 0) &&
           (VidBlueMask != 0));

    VidpConvertPalette(&VidPalette, &VidPhysicalPalette);
    Status = STATUS_SUCCESS;
    return Status;
}

VOID
VidClearScreen (
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

    ULONG Color;
    ULONG HorizontalIndex;
    PULONG Pixel;
    PUSHORT Pixel16;
    PUCHAR Pixel8;
    ULONG VerticalIndex;

    if (VidFrameBuffer == NULL) {
        return;
    }

    //
    // If either minimum value is off the screen, exit.
    //

    if (MinimumX >= VidFrameBufferWidth) {
        return;
    }

    if (MinimumY >= VidFrameBufferHeight) {
        return;
    }

    //
    // Truncate the maximum values.
    //

    if (MaximumX > VidFrameBufferWidth) {
        MaximumX = VidFrameBufferWidth;
    }

    if (MaximumY > VidFrameBufferHeight) {
        MaximumY = VidFrameBufferHeight;
    }

    Color = VidPhysicalPalette.DefaultBackground;

    //
    // Switch on the bits per pixel outside the hot inner loop.
    //

    switch (VidFrameBufferBitsPerPixel) {
    case 8:
        for (VerticalIndex = MinimumY;
             VerticalIndex < MaximumY;
             VerticalIndex += 1) {

            Pixel = (PULONG)(VidFrameBuffer +
                             (((VerticalIndex *
                                VidFrameBufferPixelsPerScanLine) +
                                MinimumX) *
                              (VidFrameBufferBitsPerPixel / BITS_PER_BYTE)));

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

            Pixel = (PULONG)(VidFrameBuffer +
                             (((VerticalIndex *
                                VidFrameBufferPixelsPerScanLine) +
                               MinimumX) *
                              (VidFrameBufferBitsPerPixel / BITS_PER_BYTE)));

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

            Pixel = (PULONG)(VidFrameBuffer +
                             (((VerticalIndex *
                                VidFrameBufferPixelsPerScanLine) +
                               MinimumX) *
                              (VidFrameBufferBitsPerPixel / BITS_PER_BYTE)));

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

            Pixel = (PULONG)(VidFrameBuffer +
                             (((VerticalIndex *
                                VidFrameBufferPixelsPerScanLine) +
                               MinimumX) *
                              (VidFrameBufferBitsPerPixel / BITS_PER_BYTE)));

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
    ULONG XCoordinate,
    ULONG YCoordinate,
    PSTR String
    )

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

{

    BASE_VIDEO_CHARACTER Character;

    if (VidFrameBuffer == NULL) {
        return;
    }

    Character.AsUint32 = 0;
    while (*String != '\0') {
        Character.Data.Character = *String;
        VidpPrintCharacter(XCoordinate, YCoordinate, &Character);
        XCoordinate += 1;
        if (XCoordinate >= (VidFrameBufferWidth / BASE_VIDEO_CHARACTER_WIDTH)) {
            XCoordinate = 0;
            YCoordinate += 1;
        }

        if (YCoordinate >=
            (VidFrameBufferHeight / BASE_VIDEO_CHARACTER_HEIGHT)) {

            YCoordinate = 0;
        }

        String += 1;
    }

    return;
}

VOID
VidPrintHexInteger (
    ULONG XCoordinate,
    ULONG YCoordinate,
    ULONG Number
    )

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

{

    CHAR StringBuffer[30];

    if (VidFrameBuffer == NULL) {
        return;
    }

    VidpConvertIntegerToString(Number, StringBuffer, 16);
    VidPrintString(XCoordinate, YCoordinate, StringBuffer);
    return;
}

VOID
VidPrintInteger (
    ULONG XCoordinate,
    ULONG YCoordinate,
    LONG Number
    )

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

{

    CHAR StringBuffer[30];

    if (VidFrameBuffer == NULL) {
        return;
    }

    VidpConvertIntegerToString(Number, StringBuffer, 10);
    VidPrintString(XCoordinate, YCoordinate, StringBuffer);
    return;
}

VOID
VidPrintCharacters (
    ULONG XCoordinate,
    ULONG YCoordinate,
    PBASE_VIDEO_CHARACTER Characters,
    ULONG Count
    )

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

{

    ULONG Index;

    for (Index = 0; Index < Count; Index += 1) {
        VidpPrintCharacter(XCoordinate, YCoordinate, Characters + Index);
        XCoordinate += 1;
        if (XCoordinate >= (VidFrameBufferWidth / BASE_VIDEO_CHARACTER_WIDTH)) {
            XCoordinate = 0;
            YCoordinate += 1;
        }

        if (YCoordinate >=
            (VidFrameBufferHeight / BASE_VIDEO_CHARACTER_HEIGHT)) {

            YCoordinate = 0;
        }
    }

    return;
}

VOID
VidSetPalette (
    PBASE_VIDEO_PALETTE Palette,
    PBASE_VIDEO_PALETTE OldPalette
    )

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

{

    if (OldPalette != NULL) {
        RtlCopyMemory(OldPalette, &VidPalette, sizeof(BASE_VIDEO_PALETTE));
    }

    RtlCopyMemory(&VidPalette, Palette, sizeof(BASE_VIDEO_PALETTE));
    VidpConvertPalette(&VidPalette, &VidPhysicalPalette);
    return;
}

VOID
VidSetPartialPalette (
    PBASE_VIDEO_PARTIAL_PALETTE PartialPalette
    )

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

{

    BASE_VIDEO_PALETTE Palette;

    VidGetPalette(&Palette);
    Palette.AnsiColor[AnsiColorDefault] = PartialPalette->DefaultForeground;
    Palette.BoldAnsiColor[AnsiColorDefault] =
                                         PartialPalette->DefaultBoldForeground;

    Palette.DefaultBackground = PartialPalette->DefaultBackground;
    Palette.DefaultBoldBackground = PartialPalette->DefaultBoldBackground;
    Palette.CursorText = PartialPalette->CursorText;
    Palette.CursorBackground = PartialPalette->CursorBackground;
    VidSetPalette(&Palette, NULL);
    return;
}

VOID
VidGetPalette (
    PBASE_VIDEO_PALETTE Palette
    )

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

{

    RtlCopyMemory(Palette, &VidPalette, sizeof(BASE_VIDEO_PALETTE));
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
VidpConvertPalette (
    PBASE_VIDEO_PALETTE Palette,
    PBASE_VIDEO_PALETTE PhysicalPalette
    )

/*++

Routine Description:

    This routine converts the given palette into a physical palette that uses
    the native pixel format.

Arguments:

    Palette - Supplies the natural palette to convert.

    PhysicalPalette - Supplies a pointer where the palette in native pixel
        format will be returned.

Return Value:

    None.

--*/

{

    ULONG BlueBit;
    ULONG ColorIndex;
    ULONG GreenBit;
    ULONG RedBit;
    COLOR_TRANSLATION Translation;

    ASSERT((VidRedMask != 0) && (VidGreenMask != 0) && (VidBlueMask != 0));

    //
    // Handle some common cases.
    //

    if ((VidRedMask == 0x00FF0000) && (VidGreenMask == 0x0000FF00) &&
        (VidBlueMask == 0x000000FF)) {

        RtlCopyMemory(PhysicalPalette, Palette, sizeof(BASE_VIDEO_PALETTE));
        return;

    } else if ((VidRedMask == 0x000000FF) && (VidGreenMask == 0x0000FF00) &&
               (VidBlueMask == 0x00FF0000)) {

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

    Translation.RedMask = VidRedMask;
    Translation.GreenMask = VidGreenMask;
    Translation.BlueMask = VidBlueMask;
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

    return;
}

VOID
VidpPrintCharacter (
    ULONG XCoordinate,
    ULONG YCoordinate,
    PBASE_VIDEO_CHARACTER Character
    )

/*++

Routine Description:

    This routine prints a character to the screen in the specified location.

Arguments:

    XCoordinate - Supplies the X coordinate of the location on the screen
        to write to.

    YCoordinate - Supplies the Y cooordinate of the location on the screen
        to write to.

    Character - Supplies a pointer to the character to print.

Return Value:

    None.

--*/

{

    USHORT Attributes;
    ANSI_COLOR BackgroundAnsiColor;
    ULONG ColorOff;
    ULONG ColorOn;
    PUSHORT Destination16;
    PULONG Destination32;
    PBYTE Destination8;
    ANSI_COLOR ForegroundAnsiColor;
    ULONG HorizontalIndex;
    BYTE Source;
    ULONG SourceIndex;
    ULONG SwapColor;
    ULONG VerticalIndex;
    ULONG YPixel;

    //
    // Get the colors to use.
    //

    ColorOn = VidPhysicalPalette.AnsiColor[AnsiColorDefault];
    ColorOff = VidPhysicalPalette.DefaultBackground;
    if (Character->Data.Attributes != 0) {
        Attributes = Character->Data.Attributes;
        if ((Attributes & BASE_VIDEO_CURSOR) != 0) {
            ColorOn = VidPhysicalPalette.CursorText;
            ColorOff = VidPhysicalPalette.CursorBackground;

        } else {
            BackgroundAnsiColor = (Attributes >> BASE_VIDEO_BACKGROUND_SHIFT) &
                                  BASE_VIDEO_COLOR_MASK;

            ForegroundAnsiColor = Attributes & BASE_VIDEO_COLOR_MASK;
            ColorOn = VidPhysicalPalette.AnsiColor[ForegroundAnsiColor];
            if ((Attributes & BASE_VIDEO_FOREGROUND_BOLD) != 0) {
                ColorOn = VidPhysicalPalette.BoldAnsiColor[ForegroundAnsiColor];
            }

            if (BackgroundAnsiColor != AnsiColorDefault) {
                ColorOff = VidPhysicalPalette.AnsiColor[BackgroundAnsiColor];
            }

            if ((Attributes & BASE_VIDEO_BACKGROUND_BOLD) != 0) {
                ColorOff =
                         VidPhysicalPalette.BoldAnsiColor[BackgroundAnsiColor];

                if (BackgroundAnsiColor == AnsiColorDefault) {
                    ColorOff = VidPhysicalPalette.DefaultBoldBackground;
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
    // Separate write loops for different pixel widths does mean more code,
    // but it skips conditionals in the inner loops, which are very hot.
    //

    switch (VidFrameBufferBitsPerPixel) {
    case 8:
        for (VerticalIndex = 0;
             VerticalIndex < BASE_VIDEO_CHARACTER_HEIGHT;
             VerticalIndex += 1) {

            SourceIndex = (VerticalIndex * 256) +
                          (Character->Data.Character & 0xFF);

            Source = VidFontData[SourceIndex];
            YPixel = ((YCoordinate * BASE_VIDEO_CHARACTER_HEIGHT) +
                      VerticalIndex) * VidFrameBufferPixelsPerScanLine;

            Destination8 = VidFrameBuffer +
                           ((YPixel +
                             (XCoordinate * BASE_VIDEO_CHARACTER_WIDTH)) *
                            (VidFrameBufferBitsPerPixel / BITS_PER_BYTE));

            for (HorizontalIndex = 0;
                 HorizontalIndex < BASE_VIDEO_CHARACTER_WIDTH;
                 HorizontalIndex += 1) {

                if ((Source & (1 << HorizontalIndex)) != 0) {
                    *Destination8 = (UCHAR)ColorOn;

                } else {
                    *Destination8 = (UCHAR)ColorOff;
                }

                Destination8 += 1;
            }
        }

        break;

    case 16:
        for (VerticalIndex = 0;
             VerticalIndex < BASE_VIDEO_CHARACTER_HEIGHT;
             VerticalIndex += 1) {

            SourceIndex = (VerticalIndex * 256) +
                          (Character->Data.Character & 0xFF);

            Source = VidFontData[SourceIndex];
            YPixel = ((YCoordinate * BASE_VIDEO_CHARACTER_HEIGHT) +
                      VerticalIndex) * VidFrameBufferPixelsPerScanLine;

            Destination16 = VidFrameBuffer +
                            ((YPixel +
                              (XCoordinate * BASE_VIDEO_CHARACTER_WIDTH)) *
                             (VidFrameBufferBitsPerPixel / BITS_PER_BYTE));

            for (HorizontalIndex = 0;
                 HorizontalIndex < BASE_VIDEO_CHARACTER_WIDTH;
                 HorizontalIndex += 1) {

                if ((Source & (1 << HorizontalIndex)) != 0) {
                    *Destination16 = (USHORT)ColorOn;

                } else {
                    *Destination16 = (USHORT)ColorOff;
                }

                Destination16 += 1;
            }
        }

        break;

    case 24:
        for (VerticalIndex = 0;
             VerticalIndex < BASE_VIDEO_CHARACTER_HEIGHT;
             VerticalIndex += 1) {

            SourceIndex = (VerticalIndex * 256) +
                          (Character->Data.Character & 0xFF);

            Source = VidFontData[SourceIndex];
            YPixel = ((YCoordinate * BASE_VIDEO_CHARACTER_HEIGHT) +
                      VerticalIndex) * VidFrameBufferPixelsPerScanLine;

            Destination8 = VidFrameBuffer +
                           ((YPixel +
                             (XCoordinate * BASE_VIDEO_CHARACTER_WIDTH)) *
                            (VidFrameBufferBitsPerPixel / BITS_PER_BYTE));

            for (HorizontalIndex = 0;
                 HorizontalIndex < BASE_VIDEO_CHARACTER_WIDTH;
                 HorizontalIndex += 1) {

                if ((Source & (1 << HorizontalIndex)) != 0) {
                    *Destination8 = (UCHAR)ColorOn;
                    *(Destination8 + 1) = (UCHAR)(ColorOn >> 8);
                    *(Destination8 + 2) = (UCHAR)(ColorOn >> 16);

                } else {
                    *Destination8 = (UCHAR)ColorOff;
                    *(Destination8 + 1) = (UCHAR)(ColorOff >> 8);
                    *(Destination8 + 2) = (UCHAR)(ColorOff >> 16);
                }

                Destination8 += 3;
            }
        }

        break;

    case 32:
        for (VerticalIndex = 0;
             VerticalIndex < BASE_VIDEO_CHARACTER_HEIGHT;
             VerticalIndex += 1) {

            SourceIndex = (VerticalIndex * 256) +
                          (Character->Data.Character & 0xFF);

            Source = VidFontData[SourceIndex];
            YPixel = ((YCoordinate * BASE_VIDEO_CHARACTER_HEIGHT) +
                      VerticalIndex) * VidFrameBufferPixelsPerScanLine;

            Destination32 = VidFrameBuffer +
                            ((YPixel +
                              (XCoordinate * BASE_VIDEO_CHARACTER_WIDTH)) *
                             (VidFrameBufferBitsPerPixel / BITS_PER_BYTE));

            for (HorizontalIndex = 0;
                 HorizontalIndex < BASE_VIDEO_CHARACTER_WIDTH;
                 HorizontalIndex += 1) {

                if ((Source & (1 << HorizontalIndex)) != 0) {
                    *Destination32 = ColorOn;

                } else {
                    *Destination32 = ColorOff;
                }

                Destination32 += 1;
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

