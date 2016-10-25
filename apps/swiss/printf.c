/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    printf.c

Abstract:

    This module implements the printf utility.

Author:

    Evan Green 16-Jul-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
PrintfUnescapeString (
    PSTR String
    );

VOID
PrintfRemoveCarriageReturns (
    PSTR String
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
PrintfMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine implements the main entry point for the printf utility.

Arguments:

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies an array of pointers to strings representing the
        arguments.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR AfterScan;
    PSTR Argument;
    ULONG ArgumentIndex;
    LONGLONG BigInteger;
    CHAR Character;
    CHAR ClobberedCharacter;
    ULONG ConversionIndex;
    CHAR ConversionSpecifier;
    CHAR Digit;
    ULONG DigitIndex;
    PSTR Format;
    BOOL FoundFormatSpecifiers;
    ULONG Index;
    INT Integer;
    BOOL PrintCharacter;
    BOOL QuadWord;
    INT ReturnValue;
    BOOL Stop;
    BOOL WasBackslash;

    if (ArgumentCount < 2) {
        ReturnValue = 1;
        goto MainEnd;
    }

    ReturnValue = 0;
    Format = Arguments[1];
    FoundFormatSpecifiers = FALSE;
    PrintfRemoveCarriageReturns(Format);
    ArgumentIndex = 2;
    Index = 0;
    Stop = FALSE;
    WasBackslash = FALSE;
    while (Stop == FALSE) {
        PrintCharacter = TRUE;
        Character = Format[Index];
        if (Character == '\0') {
            if ((ArgumentIndex < ArgumentCount) &&
                (FoundFormatSpecifiers != FALSE)) {

                Index = 0;
                WasBackslash = FALSE;
                continue;
            }

            break;
        }

        if (WasBackslash != FALSE) {
            if (Character == '\\') {
                Character = '\\';

            } else if (Character == 'a') {
                Character = '\a';

            } else if (Character == 'b') {
                Character = '\b';

            } else if (Character == 'f') {
                Character = '\f';

            } else if (Character == 'n') {
                Character = '\n';

            } else if (Character == 'r') {
                Character = '\r';

            } else if (Character == 't') {
                Character = '\t';

            } else if (Character == 'v') {
                Character = '\v';

            } else if (Character == 'x') {
                Character = 0;
                for (DigitIndex = 1; DigitIndex < 3; DigitIndex += 1) {
                    Digit = Format[Index + DigitIndex];
                    if (isdigit(Digit)) {
                        Digit -= '0';

                    } else if ((Digit >= 'A') && (Digit <= 'F')) {
                        Digit = (Digit - 'A') + 0xA;

                    } else if ((Digit >= 'a') && (Digit <= 'f')) {
                        Digit = (Digit - 'a') + 0xA;

                    } else {
                        break;
                    }

                    Character = (Character * 16) + Digit;
                }

                Index += DigitIndex - 1;

            } else if ((Character >= '0') && (Character <= '7')) {

                //
                // The first digit is already in the character, which is why
                // the loop only iterates twice for three digits.
                //

                Character -= '0';
                for (DigitIndex = 1; DigitIndex < 3; DigitIndex += 1) {
                    Digit = Format[Index + DigitIndex];
                    if ((Digit < '0') || (Digit > '7')) {
                        break;
                    }

                    Character = (Character * 8) + (Digit - '0');
                }

                Index += DigitIndex - 1;

            //
            // An unknown backslash escape was used. Treat the backslash
            // literally.
            //

            } else {
                printf("\\%c", Character);
                PrintCharacter = FALSE;
            }

        //
        // This character is not preceded by a backslash.
        //

        } else {
            if (Character == '\\') {
                PrintCharacter = FALSE;

            } else if (Character == '%') {
                ConversionIndex = Index + 1;

                //
                // Get through any flags.
                //

                while (TRUE) {
                    Character = Format[ConversionIndex];
                    if ((Character != '+') && (Character != '-') &&
                        (Character != ' ') && (Character != '#') &&
                        (Character != '0')) {

                        break;
                    }

                    ConversionIndex += 1;
                }

                //
                // Get through any field width digits.
                //

                while (isdigit(Format[ConversionIndex])) {
                    ConversionIndex += 1;
                }

                //
                // Get through an optional dot and precision.
                //

                if (Format[ConversionIndex] == '.') {
                    ConversionIndex += 1;
                }

                while (isdigit(Format[ConversionIndex])) {
                    ConversionIndex += 1;
                }

                //
                // Look to see if length modifiers exist that make this a quad
                // word. Then get past all length modifiers.
                //

                QuadWord = FALSE;
                if ((Format[ConversionIndex] == 'l') &&
                    (Format[ConversionIndex + 1] == 'l')) {

                    QuadWord = TRUE;
                }

                while (TRUE) {
                    Character = Format[ConversionIndex];
                    if ((Character != 'h') && (Character != 'l') &&
                        (Character != 'j') && (Character != 'z') &&
                        (Character != 't') && (Character != 'L')) {

                        break;
                    }

                    ConversionIndex += 1;
                }

                //
                // Get the conversion specifier and temporarily null terminate
                // the format specifier.
                //

                ConversionSpecifier = Format[ConversionIndex];
                ConversionIndex += 1;
                ClobberedCharacter = Format[ConversionIndex];
                Format[ConversionIndex] = '\0';
                if (ArgumentIndex < ArgumentCount) {
                    Argument = Arguments[ArgumentIndex];

                } else {
                    Argument = NULL;
                }

                //
                // Convert an integer of some kind.
                //

                if ((ConversionSpecifier == 'd') ||
                    (ConversionSpecifier == 'i') ||
                    (ConversionSpecifier == 'o') ||
                    (ConversionSpecifier == 'u') ||
                    (ConversionSpecifier == 'x') ||
                    (ConversionSpecifier == 'X') ||
                    (ConversionSpecifier == 'c') ||
                    (ConversionSpecifier == 'p') ||
                    (ConversionSpecifier == 'C')) {

                    if (QuadWord != FALSE) {
                        BigInteger = strtoll(Argument, &AfterScan, 0);
                        if (((BigInteger == -1) && (errno != 0)) ||
                            (AfterScan == Argument)) {

                            SwPrintError(0, Argument, "Invalid number");
                            ReturnValue = errno;
                        }

                        printf(Format + Index, BigInteger);

                    } else {
                        if ((ConversionSpecifier == 'c') ||
                            (ConversionSpecifier == 'C')) {

                            Integer = 0;
                            if (Argument != NULL) {
                                Integer = Argument[0];
                            }

                        } else {
                            Integer = strtol(Argument, &AfterScan, 0);
                            if ((AfterScan == Argument) ||
                                (*AfterScan != '\0')) {

                                SwPrintError(0, Argument, "Invalid number");
                                ReturnValue = errno;
                            }
                        }

                        printf(Format + Index, Integer);
                    }

                //
                // Convert a string.
                //

                } else if ((ConversionSpecifier == 's') ||
                           (ConversionSpecifier == 'b')) {

                    if ((ConversionSpecifier == 'b') && (Argument != NULL)) {
                        Stop = PrintfUnescapeString(Argument);
                        Format[ConversionIndex - 1] = 's';
                    }

                    if (Argument == NULL) {
                        Argument = "";
                    }

                    PrintfRemoveCarriageReturns(Argument);
                    printf(Format + Index, Argument);

                //
                // Oh, it's just an unescaped percent.
                //

                } else if (ConversionSpecifier == '%') {
                    fputc('%', stdout);
                    Argument = NULL;

                //
                // This is an unknown conversion specifier.
                //

                } else {
                    SwPrintError(0,
                                 NULL,
                                 "Unknown conversion specifier '%c'",
                                 ConversionSpecifier);

                    ReturnValue = 1;
                    Argument = NULL;
                }

                if (Argument != NULL) {
                    ArgumentIndex += 1;
                    FoundFormatSpecifiers = TRUE;
                }

                //
                // Restore the string and move beyond this specifier.
                //

                Format[ConversionIndex] = ClobberedCharacter;
                Index = ConversionIndex - 1;
                PrintCharacter = FALSE;
            }
        }

        if (PrintCharacter != FALSE) {
            fputc(Character, stdout);
        }

        if (Character == '\\') {
            WasBackslash = !WasBackslash;

        } else {
            WasBackslash = FALSE;
        }

        Index += 1;
    }

MainEnd:
    return ReturnValue;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
PrintfUnescapeString (
    PSTR String
    )

/*++

Routine Description:

    This routine unescapes backslash sequences \\ \a \b \f \n \r \t \v, \c, and
    \0ddd (where d is zero to three octal digits).

Arguments:

    String - Supplies a pointer to the string to unescape.

Return Value:

    TRUE if a \c was found.

    FALSE if no \c was found.

--*/

{

    CHAR Character;
    ULONG DeleteCount;
    CHAR Digit;
    ULONG DigitIndex;
    ULONG Index;
    BOOL Stop;
    UINTN StringSize;
    BOOL WasBackslash;

    Index = 0;
    StringSize = strlen(String) + 1;
    Stop = FALSE;
    WasBackslash = FALSE;
    while (Stop == FALSE) {
        Character = String[Index];
        if (Character == '\0') {
            break;
        }

        if (WasBackslash != FALSE) {
            DeleteCount = 1;
            if (Character == '\\') {
                Character = '\\';

            } else if (Character == 'a') {
                Character = '\a';

            } else if (Character == 'b') {
                Character = '\b';

            } else if (Character == 'c') {
                Character = '\0';
                Stop = TRUE;

            } else if (Character == 'f') {
                Character = '\f';

            } else if (Character == 'n') {
                Character = '\n';

            } else if (Character == 'r') {
                Character = '\r';

            } else if (Character == 't') {
                Character = '\t';

            } else if (Character == 'v') {
                Character = '\v';

            } else if (Character == '0') {
                Character = 0;
                for (DigitIndex = 1; DigitIndex < 4; DigitIndex += 1) {
                    Digit = String[Index + DigitIndex];
                    if ((Digit < '0') || (Digit > '7')) {
                        break;
                    }

                    Character = (Character * 8) + (Digit - '0');
                }

                DeleteCount = 1 + DigitIndex;

            //
            // An unknown backslash escape was used. Treat the backslash
            // literally.
            //

            } else {
                DeleteCount = 0;
            }

            if (DeleteCount != 0) {
                Index -= 1;
                SwStringRemoveRegion(String,
                                     &StringSize,
                                     Index,
                                     DeleteCount);

                String[Index] = Character;
            }
        }

        if (Character == '\\') {
            WasBackslash = !WasBackslash;

        } else {
            WasBackslash = FALSE;
        }

        Index += 1;
    }

    return Stop;
}

VOID
PrintfRemoveCarriageReturns (
    PSTR String
    )

/*++

Routine Description:

    This routine removes any \r characters from the input string, as they
    pile up in Windows platforms and are generally useless.

Arguments:

    String - Supplies a pointer to the string to strip of \r characters.

Return Value:

    None.

--*/

{

    UINTN Index;
    UINTN StringSize;

    StringSize = strlen(String) + 1;
    Index = 0;
    while (String[Index] != '\0') {
        if (String[Index] == '\r') {
            SwStringRemoveRegion(String, &StringSize, Index, 1);

        } else {
            Index += 1;
        }
    }

    return;
}

