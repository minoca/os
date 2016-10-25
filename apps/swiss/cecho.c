/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    cecho.c

Abstract:

    This module implements the color echo application.

Author:

    Evan Green 1-May-2014

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include "swlib.h"
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

#define CECHO_USAGE \
    "usage: cecho [-f <color>] [-b <color>] [-neE]\n"                          \
    "The cecho utility echoes command line parameters in color. Options are:\n"\
    "  -f <color> -- Print with the given foreground color.\n"                 \
    "  -b <color> -- Print with the given background color.\n"                 \
    "  -n -- Do not print a newline at the end.\n"                             \
    "  -e -- Enable escape processing.\n"                                      \
    "  -E -- Disable escape processing.\n"                                     \
    "Use the -- argument to disable argument processing of all subsequent \n"  \
    "parameters. The color type can be one of:\n"                              \
    "  d -- Default color\n"                                                   \
    "  k -- Black\n"                                                           \
    "  r -- Dark red\n"                                                        \
    "  g -- Dark green\n"                                                      \
    "  y -- Dark yellow\n"                                                     \
    "  b -- Dark blue\n"                                                       \
    "  m -- Dark magenta\n"                                                    \
    "  c -- Dark cyan\n"                                                       \
    "  a -- Dark gray\n"                                                       \
    "  D -- Bold default\n"                                                    \
    "  A -- gray\n"                                                            \
    "  R -- bright red\n"                                                      \
    "  G -- bright green\n"                                                    \
    "  Y -- bright yellow\n"                                                   \
    "  B -- bright blue\n"                                                     \
    "  M -- bright magenta\n"                                                  \
    "  C -- bright cyan\n"                                                     \
    "  W -- white\n"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

CONSOLE_COLOR
ColorEchoConvertToColor (
    CHAR Character
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
ColorEchoMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the color echo program.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR Argument;
    ULONG ArgumentIndex;
    ULONG ArgumentLength;
    CONSOLE_COLOR BackgroundColor;
    CHAR Character;
    ULONG CharacterIndex;
    CHAR DigitCount;
    BOOL EscapeProcessing;
    CONSOLE_COLOR ForegroundColor;
    BOOL PrintTrailingNewline;
    CHAR Value;
    BOOL WasBackslash;

    EscapeProcessing = FALSE;
    PrintTrailingNewline = TRUE;
    BackgroundColor = ConsoleColorDefault;
    ForegroundColor = ConsoleColorDefault;

    //
    // Loop through processing arguments.
    //

    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        Argument = Arguments[ArgumentIndex];
        if (Argument[0] != '-') {
            break;
        }

        if (strcmp(Argument, "--help") == 0) {
            printf(CECHO_USAGE);
            return 1;
        }

        while (TRUE) {
            Argument += 1;
            if (Argument[0] == '\0') {
                break;

            } else if (Argument[0] == 'e') {
                EscapeProcessing = TRUE;

            } else if (Argument[0] == 'E') {
                EscapeProcessing = FALSE;

            } else if (Argument[0] == 'n') {
                PrintTrailingNewline = FALSE;

            } else if (Argument[0] == 'f') {
                ForegroundColor = ColorEchoConvertToColor(Argument[1]);
                if (ForegroundColor == -1) {
                    return 1;
                }

                Argument += 1;

            } else if (Argument[0] == 'b') {
                BackgroundColor = ColorEchoConvertToColor(Argument[1]);
                if (BackgroundColor == -1) {
                    return 1;
                }

                Argument += 1;

            } else if (Argument[0] == '-') {
                break;

            } else {
                break;
            }
        }

        if (Argument[0] != '\0') {
            break;
        }
    }

    //
    // Echo out the remainder of the arguments.
    //

    while (ArgumentIndex < ArgumentCount) {
        Argument = Arguments[ArgumentIndex];
        ArgumentIndex += 1;

        //
        // If not processing backslashes or there are none, just print it
        // directly and continue.
        //

        if ((EscapeProcessing == FALSE) || (strchr(Argument, '\\') == NULL)) {
            SwPrintInColor(BackgroundColor, ForegroundColor, "%s", Argument);

        //
        // Escape the darn thing by looping through every character in the
        // string.
        //

        } else {
            Value = 0;
            DigitCount = 0;
            WasBackslash = FALSE;
            ArgumentLength = strlen(Argument);
            for (CharacterIndex = 0;
                 CharacterIndex < ArgumentLength;
                 CharacterIndex += 1) {

                Character = Argument[CharacterIndex];

                //
                // If a \0 was detected, then this is in the process of
                // working through \0NNN, where NNN is one to three octal
                // characters.
                //

                if (DigitCount != 0) {
                    if ((Character >= '0') && (Character <= '7')) {
                        Value = (Value * 8) + (Character - '0');
                        DigitCount += 1;
                        if (DigitCount == 4) {
                            DigitCount = 0;
                            SwPrintInColor(BackgroundColor,
                                           ForegroundColor,
                                           "%c",
                                           Value);
                        }

                        //
                        // This was a digit that counted towards the value, so
                        // it shouldn't be considered for further printing.
                        //

                        continue;

                    //
                    // The number ended a bit early, so print it out and
                    // consider this character for further printing.
                    //

                    } else {
                        DigitCount = 0;
                        SwPrintInColor(BackgroundColor,
                                       ForegroundColor,
                                       "%c",
                                       Value);
                    }
                }

                if (WasBackslash != FALSE) {
                    if (Character == 'a') {

                    } else if (Character == 'b') {
                        SwPrintInColor(BackgroundColor, ForegroundColor, "\b");

                    } else if (Character == 'c') {
                        PrintTrailingNewline = FALSE;
                        goto MainEnd;

                    } else if (Character == 'f') {
                        SwPrintInColor(BackgroundColor, ForegroundColor, "\f");

                    } else if (Character == 'n') {
                        SwPrintInColor(BackgroundColor, ForegroundColor, "\n");

                    } else if (Character == 'r') {
                        SwPrintInColor(BackgroundColor, ForegroundColor, "\r");

                    } else if (Character == 't') {
                        SwPrintInColor(BackgroundColor, ForegroundColor, "\t");

                    } else if (Character == '\\') {
                        SwPrintInColor(BackgroundColor, ForegroundColor, "\\");

                    } else if (Character == '0') {
                        Value = 0;
                        DigitCount = 1;

                    } else {

                        //
                        // This backslash escape sequence isn't recognized, so
                        // just print it out verbatim.
                        //

                        SwPrintInColor(BackgroundColor,
                                       ForegroundColor,
                                       "\\%c",
                                       Character);
                    }

                //
                // Just a regular old character, print it out unless it begins
                // an escaped sequence.
                //

                } else if (Character != '\\') {
                    SwPrintInColor(BackgroundColor,
                                   ForegroundColor,
                                   "%c",
                                   Character);
                }

                if (Character == '\\') {
                    WasBackslash = !WasBackslash;

                } else {
                    WasBackslash = FALSE;
                }
            }
        }

        if (ArgumentIndex != ArgumentCount) {
            SwPrintInColor(BackgroundColor, ForegroundColor, " ");
        }
    }

MainEnd:
    if (PrintTrailingNewline != FALSE) {
        SwPrintInColor(BackgroundColor, ForegroundColor, "\n");
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

CONSOLE_COLOR
ColorEchoConvertToColor (
    CHAR Character
    )

/*++

Routine Description:

    This routine converts a color character to a console color enum.

Arguments:

    Character - Supplies the character to convert.

Return Value:

    Returns the console color on success.

    -1 if the character is not valid.

--*/

{

    CONSOLE_COLOR Color;

    switch (Character) {
    case 'd':
        Color = ConsoleColorDefault;
        break;

    case 'k':
        Color = ConsoleColorBlack;
        break;

    case 'r':
        Color = ConsoleColorDarkRed;
        break;

    case 'g':
        Color = ConsoleColorDarkGreen;
        break;

    case 'y':
        Color = ConsoleColorDarkYellow;
        break;

    case 'b':
        Color = ConsoleColorDarkBlue;
        break;

    case 'm':
        Color = ConsoleColorDarkMagenta;
        break;

    case 'c':
        Color = ConsoleColorDarkCyan;
        break;

    case 'a':
        Color = ConsoleColorDarkGray;
        break;

    case 'D':
        Color = ConsoleColorBoldDefault;
        break;

    case 'A':
        Color = ConsoleColorGray;
        break;

    case 'R':
        Color = ConsoleColorRed;
        break;

    case 'G':
        Color = ConsoleColorGreen;
        break;

    case 'Y':
        Color = ConsoleColorYellow;
        break;

    case 'B':
        Color = ConsoleColorBlue;
        break;

    case 'M':
        Color = ConsoleColorMagenta;
        break;

    case 'C':
        Color = ConsoleColorCyan;
        break;

    case 'W':
        Color = ConsoleColorWhite;
        break;

    default:
        fprintf(stderr, "cecho: Invalid color '%c'.\n", Character);
        Color = -1;
    }

    return Color;
}

