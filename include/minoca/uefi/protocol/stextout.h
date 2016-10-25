/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    stextout.h

Abstract:

    This header contains definitions for the Simple Text Out Protocol.

Author:

    Evan Green 8-Feb-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID                \
    {                                                       \
        0x387477C2, 0x69C7, 0x11D2,                         \
        {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B }   \
    }

//
// Protocol GUID defined in EFI1.1.
//

#define SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID

//
// Define's for required EFI Unicode Box Draw characters
//

#define BOXDRAW_HORIZONTAL                  0x2500
#define BOXDRAW_VERTICAL                    0x2502
#define BOXDRAW_DOWN_RIGHT                  0x250C
#define BOXDRAW_DOWN_LEFT                   0x2510
#define BOXDRAW_UP_RIGHT                    0x2514
#define BOXDRAW_UP_LEFT                     0x2518
#define BOXDRAW_VERTICAL_RIGHT              0x251c
#define BOXDRAW_VERTICAL_LEFT               0x2524
#define BOXDRAW_DOWN_HORIZONTAL             0x252C
#define BOXDRAW_UP_HORIZONTAL               0x2534
#define BOXDRAW_VERTICAL_HORIZONTAL         0x253C
#define BOXDRAW_DOUBLE_HORIZONTAL           0x2550
#define BOXDRAW_DOUBLE_VERTICAL             0x2551
#define BOXDRAW_DOWN_RIGHT_DOUBLE           0x2552
#define BOXDRAW_DOWN_DOUBLE_RIGHT           0x2553
#define BOXDRAW_DOUBLE_DOWN_RIGHT           0x2554
#define BOXDRAW_DOWN_LEFT_DOUBLE            0x2555
#define BOXDRAW_DOWN_DOUBLE_LEFT            0x2556
#define BOXDRAW_DOUBLE_DOWN_LEFT            0x2557
#define BOXDRAW_UP_RIGHT_DOUBLE             0x2558
#define BOXDRAW_UP_DOUBLE_RIGHT             0x2559
#define BOXDRAW_DOUBLE_UP_RIGHT             0x255A
#define BOXDRAW_UP_LEFT_DOUBLE              0x255B
#define BOXDRAW_UP_DOUBLE_LEFT              0x255C
#define BOXDRAW_DOUBLE_UP_LEFT              0x255D
#define BOXDRAW_VERTICAL_RIGHT_DOUBLE       0x255E
#define BOXDRAW_VERTICAL_DOUBLE_RIGHT       0x255F
#define BOXDRAW_DOUBLE_VERTICAL_RIGHT       0x2560
#define BOXDRAW_VERTICAL_LEFT_DOUBLE        0x2561
#define BOXDRAW_VERTICAL_DOUBLE_LEFT        0x2562
#define BOXDRAW_DOUBLE_VERTICAL_LEFT        0x2563
#define BOXDRAW_DOWN_HORIZONTAL_DOUBLE      0x2564
#define BOXDRAW_DOWN_DOUBLE_HORIZONTAL      0x2565
#define BOXDRAW_DOUBLE_DOWN_HORIZONTAL      0x2566
#define BOXDRAW_UP_HORIZONTAL_DOUBLE        0x2567
#define BOXDRAW_UP_DOUBLE_HORIZONTAL        0x2568
#define BOXDRAW_DOUBLE_UP_HORIZONTAL        0x2569
#define BOXDRAW_VERTICAL_HORIZONTAL_DOUBLE  0x256A
#define BOXDRAW_VERTICAL_DOUBLE_HORIZONTAL  0x256B
#define BOXDRAW_DOUBLE_VERTICAL_HORIZONTAL  0x256C

//
// EFI Required Block Elements Code Chart
//

#define BLOCKELEMENT_FULL_BLOCK   0x2588
#define BLOCKELEMENT_LIGHT_SHADE  0x2591

//
// EFI Required Geometric Shapes Code Chart
//

#define GEOMETRICSHAPE_UP_TRIANGLE    0x25B2
#define GEOMETRICSHAPE_RIGHT_TRIANGLE 0x25BA
#define GEOMETRICSHAPE_DOWN_TRIANGLE  0x25BC
#define GEOMETRICSHAPE_LEFT_TRIANGLE  0x25C4

//
// EFI Required Arrow shapes
//

#define ARROW_LEFT  0x2190
#define ARROW_UP    0x2191
#define ARROW_RIGHT 0x2192
#define ARROW_DOWN  0x2193

//
// EFI Console Colours
//

#define EFI_BLACK                 0x00
#define EFI_BLUE                  0x01
#define EFI_GREEN                 0x02
#define EFI_CYAN                  (EFI_BLUE | EFI_GREEN)
#define EFI_RED                   0x04
#define EFI_MAGENTA               (EFI_BLUE | EFI_RED)
#define EFI_BROWN                 (EFI_GREEN | EFI_RED)
#define EFI_LIGHTGRAY             (EFI_BLUE | EFI_GREEN | EFI_RED)
#define EFI_BRIGHT                0x08
#define EFI_DARKGRAY              (EFI_BRIGHT)
#define EFI_LIGHTBLUE             (EFI_BLUE | EFI_BRIGHT)
#define EFI_LIGHTGREEN            (EFI_GREEN | EFI_BRIGHT)
#define EFI_LIGHTCYAN             (EFI_CYAN | EFI_BRIGHT)
#define EFI_LIGHTRED              (EFI_RED | EFI_BRIGHT)
#define EFI_LIGHTMAGENTA          (EFI_MAGENTA | EFI_BRIGHT)
#define EFI_YELLOW                (EFI_BROWN | EFI_BRIGHT)
#define EFI_WHITE                 (EFI_BLUE | EFI_GREEN | EFI_RED | EFI_BRIGHT)

#define EFI_TEXT_ATTR(_Foreground, _Background) \
    ((_Foreground) | ((_Background) << 4))

#define EFI_BACKGROUND_BLACK      0x00
#define EFI_BACKGROUND_BLUE       0x10
#define EFI_BACKGROUND_GREEN      0x20
#define EFI_BACKGROUND_CYAN       (EFI_BACKGROUND_BLUE | EFI_BACKGROUND_GREEN)
#define EFI_BACKGROUND_RED        0x40
#define EFI_BACKGROUND_MAGENTA    (EFI_BACKGROUND_BLUE | EFI_BACKGROUND_RED)
#define EFI_BACKGROUND_BROWN      (EFI_BACKGROUND_GREEN | EFI_BACKGROUND_RED)
#define EFI_BACKGROUND_LIGHTGRAY  \
    (EFI_BACKGROUND_BLUE | EFI_BACKGROUND_GREEN | EFI_BACKGROUND_RED)

//
// We currently define attributes from 0x0 to 0x7F for color manipulations.
// To internally handle the local display characteristics for a particular
// character, Bit 7 signifies the local glyph representation for a character.
// If turned on, glyphs will be pulled from the wide glyph database and will
// display locally as a wide character (16 x 19 versus 8 x 19)
// If bit 7 is off, the narrow glyph database will be used. This does NOT
// affect information that is sent to non-local displays, such as serial or
// LAN consoles.
//

#define EFI_WIDE_ATTRIBUTE 0x80

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

//
// Create the type for backward compatibility with EFI1.1.
//

typedef EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL SIMPLE_TEXT_OUTPUT_INTERFACE;

typedef
EFI_STATUS
(EFIAPI *EFI_TEXT_RESET) (
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    BOOLEAN ExtendedVerification
    );

/*++

Routine Description:

    This routine resets the output device hardware and optionally runs
    diagnostics.

Arguments:

    This - Supplies a pointer to the protocol instance.

    ExtendedVerification - Supplies a boolean indicating if the driver should
        perform diagnostics on reset.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device is not functioning properly and could not be
    reset.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_TEXT_STRING) (
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    CHAR16 *String
    );

/*++

Routine Description:

    This routine writes a (wide) string to the output device.

Arguments:

    This - Supplies a pointer to the protocol instance.

    String - Supplies the null-terminated string to be displayed on the output
        device(s). All output devices must also support the Unicode drawing
        character codes defined in this file.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device is not functioning properly and could not be
    written to.

    EFI_UNSUPPORTED if the output device's mode indicates some of the
    characters in the string could not be rendered and were skipped.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_TEXT_TEST_STRING) (
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    CHAR16 *String
    );

/*++

Routine Description:

    This routine verifies that all characters in a string can be output to the
    target device.

Arguments:

    This - Supplies a pointer to the protocol instance.

    String - Supplies the null-terminated string to be examined for the output
        device(s).

Return Value:

    EFI_SUCCESS on success.

    EFI_UNSUPPORTED if the output device's mode indicates some of the
    characters in the string could not be rendered and were skipped.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_TEXT_QUERY_MODE) (
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    UINTN ModeNumber,
    UINTN *Columns,
    UINTN *Rows
    );

/*++

Routine Description:

    This routine requests information for an available text mode that the
    output device(s) can support.

Arguments:

    This - Supplies a pointer to the protocol instance.

    ModeNumber - Supplies the mode number to return information on.

    Columns - Supplies a pointer where the number of columns on the output
        device will be returned.

    Rows - Supplies a pointer where the number of rows on the output device
        will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_UNSUPPORTED if the mode number was not valid.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_TEXT_SET_MODE) (
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    UINTN ModeNumber
    );

/*++

Routine Description:

    This routine sets the output device to a specified mode.

Arguments:

    This - Supplies a pointer to the protocol instance.

    ModeNumber - Supplies the mode number to set.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_UNSUPPORTED if the mode number was not valid.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_TEXT_SET_ATTRIBUTE) (
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    UINTN Attribute
    );

/*++

Routine Description:

    This routine sets the background and foreground colors for the output
    string and clear screen functions.

Arguments:

    This - Supplies a pointer to the protocol instance.

    Attribute - Supplies the attributes to set. Bits 0 to 3 are the foreground
        color, and bits 4 to 6 are the background color. All other bits must
        be zero. See the EFI_TEXT_ATTR macro.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_UNSUPPORTED if the attribute requested is not defined.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_TEXT_CLEAR_SCREEN) (
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This
    );

/*++

Routine Description:

    This routine clears the output device(s) display to the currently selected
    background color.

Arguments:

    This - Supplies a pointer to the protocol instance.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_UNSUPPORTED if the output device is not in a valid text mode.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_TEXT_SET_CURSOR_POSITION) (
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    UINTN Column,
    UINTN Row
    );

/*++

Routine Description:

    This routine sets the current coordinates of the cursor position.

Arguments:

    This - Supplies a pointer to the protocol instance.

    Column - Supplies the desired column. This must be greater than or equal to
        zero and less than the number of columns as reported by the query mode
        function.

    Row - Supplies the desired row. This must be greater than or equal to zero
        and less than the number of rows as reported by the query mode function.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_UNSUPPORTED if the output device is not in a valid text mode, or the
    requested cursor position is out of range.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_TEXT_ENABLE_CURSOR) (
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    BOOLEAN Visible
    );

/*++

Routine Description:

    This routine makes the cursor visible or invisible.

Arguments:

    This - Supplies a pointer to the protocol instance.

    Visible - Supplies a boolean indicating whether to make the cursor visible
        (TRUE) or invisible (FALSE).

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_UNSUPPORTED if the output device is not in a valid text mode.

--*/

/*++

Structure Description:

    This structure defines the mode structure pointed to by the Simple Text
    Out protocol.

Members:

    MaxMode - Stores the number of modes supported by the QueryMode and SetMode
        functions.

    Mode - Stores the current text mode of the output device(s).

    Attribute - Stores the current character output attributes.

    CursorColumn - Stores the current cursor column.

    CursorRow - Stores the current cursor row.

    CursorVisible - Stores whether or not the cursor is currently visible.

--*/

typedef struct {
    INT32 MaxMode;
    INT32 Mode;
    INT32 Attribute;
    INT32 CursorColumn;
    INT32 CursorRow;
    BOOLEAN CursorVisible;
} EFI_SIMPLE_TEXT_OUTPUT_MODE;

/*++

Structure Description:

    This structure defines the Simple Text Out protocol, used to control
    text-based output devices. It is the minimu required protocol for any
    handle supplied as the ConsoleOut or StandardError device. In addition,
    the minimum supported text mode of such devices is at least 80 x 25.

Members:

    Reset - Stores a pointer to a function used to reset the output device(s).

    OutputString - Stores a pointer to a function used to print a string to the
        output.

    TestString - Stores a pointer to a function used to determine if the
        given string is fully printable.

    QueryMode - Stores a pointer to a function used to interrogate supported
        modes of the device.

    SetMode - Stores a pointer to a function used to set the current text mode.

    SetAttributes - Stores a pointer to a function used to set the text
        attributes of future printed text and clear operations.

    ClearScreen - Stores a pointer to a function used to clear the screen to
        its background color.

    SetCursorPosition - Stores a pointer to a function used to set the current
        cursor position.

    EnableCursor - Stores a pointer to a function used to make the cursor
        visible or invisible.

    Mode - Stores a pointer to the current mode settings, and maximum supported
        mode.

--*/

struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    EFI_TEXT_RESET Reset;
    EFI_TEXT_STRING OutputString;
    EFI_TEXT_TEST_STRING TestString;
    EFI_TEXT_QUERY_MODE QueryMode;
    EFI_TEXT_SET_MODE SetMode;
    EFI_TEXT_SET_ATTRIBUTE SetAttribute;
    EFI_TEXT_CLEAR_SCREEN ClearScreen;
    EFI_TEXT_SET_CURSOR_POSITION SetCursorPosition;
    EFI_TEXT_ENABLE_CURSOR EnableCursor;
    EFI_SIMPLE_TEXT_OUTPUT_MODE *Mode;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
