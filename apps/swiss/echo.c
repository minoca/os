/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    echo.c

Abstract:

    This module implements the echo application.

Author:

    Evan Green 13-Jun-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <stdio.h>
#include <string.h>

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
EchoIsStringBackslashEscaped (
    PSTR String
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
EchoMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the echo program.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    0 always.

--*/

{

    PSTR Argument;
    ULONG ArgumentIndex;
    ULONG ArgumentLength;
    CHAR Character;
    ULONG CharacterIndex;
    CHAR DigitCount;
    BOOL EscapeProcessing;
    BOOL PrintTrailingNewline;
    CHAR Value;
    BOOL WasBackslash;

    EscapeProcessing = FALSE;
    PrintTrailingNewline = TRUE;

    //
    // Loop through processing arguments.
    //

    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        Argument = Arguments[ArgumentIndex];
        if (Argument[0] != '-') {
            break;
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

        if ((EscapeProcessing == FALSE) ||
            (EchoIsStringBackslashEscaped(Argument) == FALSE)) {

            printf("%s", Argument);

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
                            printf("%c", Value);
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
                        printf("%c", Value);
                    }
                }

                if (WasBackslash != FALSE) {
                    if (Character == 'a') {

                    } else if (Character == 'b') {
                        printf("\b");

                    } else if (Character == 'c') {
                        PrintTrailingNewline = FALSE;
                        goto MainEnd;

                    } else if (Character == 'f') {
                        printf("\f");

                    } else if (Character == 'n') {
                        printf("\n");

                    } else if (Character == 'r') {
                        printf("\r");

                    } else if (Character == 't') {
                        printf("\t");

                    } else if (Character == '\\') {
                        printf("\\");

                    } else if (Character == '0') {
                        Value = 0;
                        DigitCount = 1;

                    } else {

                        //
                        // This backslash escape sequence isn't recognized, so
                        // just print it out verbatim.
                        //

                        printf("\\%c", Character);
                    }

                //
                // Just a regular old character, print it out unless it begins
                // an escaped sequence.
                //

                } else if (Character != '\\') {
                    printf("%c", Character);
                }

                if (Character == '\\') {
                    WasBackslash = !WasBackslash;

                } else {
                    WasBackslash = FALSE;
                }
            }
        }

        if (ArgumentIndex != ArgumentCount) {
            printf(" ");
        }
    }

MainEnd:
    if (PrintTrailingNewline != FALSE) {
        printf("\n");
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
EchoIsStringBackslashEscaped (
    PSTR String
    )

/*++

Routine Description:

    This routine determines if the given string has a backslash character in it.

Arguments:

    String - Supplies a pointer to the null terminated string to check.

Return Value:

    TRUE if the string contains a backslash.

    FALSE if the string contains no backslashes.

--*/

{

    if (strchr(String, '\\') != NULL) {
        return TRUE;
    }

    return FALSE;
}

