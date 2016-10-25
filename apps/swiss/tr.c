/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    tr.c

Abstract:

    This module implements support for the "tr" utility, which transforms a
    set of characters.

Author:

    Evan Green 15-Aug-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define TR_VERSION_MAJOR 1
#define TR_VERSION_MINOR 0

#define TR_USAGE                                                               \
    "usage: [-ds][-c | -C] string1 [string2]\n\n"                              \
    "The tr utility copies characters from standard in to standard out, \n"    \
    "translating a specified set of characters along the way. Options are:\n"  \
    "  -d, --delete -- Delete all occurrences of characters specified by \n"   \
    "        string1.\n"                                                       \
    "  -C, --complement -- Take string1 as the complement of all character \n" \
    "        specified.\n"                                                     \
    "  -c -- Same as -C, complement string1.\n"                                \
    "  -s, --squeeze-repeats -- Squeeze. After translations, reduce all \n"    \
    "        repeated occurrences of characters in string2 with a single \n"   \
    "        occurrence.\n"                                                    \
    "  string1/2 -- Specifies the set of characters to translate. This can \n" \
    "        be a regular character, control character \\[abfnrv0\\], or \n"   \
    "        octal escape character \\NNN. It can also be a character class \n"\
    "        [:class:]. Finally, it can also be a repeated sequence [x*n], \n" \
    "        where n is the repeat count (which if unspecified goes to the \n" \
    "        end of the string in string2.\n"                                  \
    "  --help -- Display this help text and exit.\n"                           \
    "  --version -- Display the version number and exit.\n\n"                  \

#define TR_OPTIONS_STRING "dCcs"

//
// Define tr options.
//

//
// Set this option to use the complement of the character set.
//

#define TR_OPTION_COMPLEMENT_STRING 0x00000001

//
// Set this option to delete characters in the string.
//

#define TR_OPTION_DELETE 0x00000002

//
// Set this option to squeeze repeated characters down to one.
//

#define TR_OPTION_SQUEEZE 0x00000004

#define TR_INITIAL_STRING_CAPACITY 32

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
int
(*PCHARACTER_CLASS_FUNCTION) (
    int Character
    );

/*++

Routine Description:

    This routine represents the prototype for one of the character class
    routines.

Arguments:

    Character - Supplies the character to test.

Return Value:

    Non-zero if the character belongs to the character class.

    0 if the character is not part of the character class.

--*/

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
TrCreateSet (
    PSTR Argument,
    ULONG Options,
    ULONG ZeroRepeatSize,
    PSTR *NewSet,
    PULONG NewSetSize
    );

INT
TrSetAddCharacter (
    PSTR *Set,
    CHAR Character,
    PULONG Size,
    PULONG Capacity
    );

CHAR
TrParseCharacter (
    PSTR *Argument
    );

INT
TrIsCharacterInSet (
    PSTR Set,
    ULONG SetSize,
    INT Character
    );

//
// -------------------------------------------------------------------- Globals
//

struct option TrLongOptions[] = {
    {"delete", no_argument, 0, 'd'},
    {"complement", no_argument, 0, 'C'},
    {"squeeze-repeats", no_argument, 0, 's'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0}
};

//
// ------------------------------------------------------------------ Functions
//

INT
TrMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the tr utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    ULONG ArgumentIndex;
    INT Character;
    FILE *Input;
    INT Option;
    ULONG Options;
    INT PreviousCharacter;
    PSTR Set1;
    ULONG Set1Size;
    PSTR Set2;
    ULONG Set2Size;
    INT SetIndex;
    PSTR SqueezeSet;
    ULONG SqueezeSetSize;
    int Status;
    PSTR String1;
    PSTR String2;

    Options = 0;
    PreviousCharacter = 0;
    Set1 = NULL;
    Set2 = NULL;
    String1 = NULL;
    String2 = NULL;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             TR_OPTIONS_STRING,
                             TrLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            return Status;
        }

        switch (Option) {
        case 'C':
        case 'c':
            Options |= TR_OPTION_COMPLEMENT_STRING;
            break;

        case 'd':
            Options |= TR_OPTION_DELETE;
            break;

        case 's':
            Options |= TR_OPTION_SQUEEZE;
            break;

        case 'V':
            SwPrintVersion(TR_VERSION_MAJOR, TR_VERSION_MINOR);
            return 1;

        case 'h':
            printf(TR_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            return Status;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    if (ArgumentIndex < ArgumentCount) {
        String1 = Arguments[ArgumentIndex];
        if (ArgumentIndex + 1 < ArgumentCount) {
            String2 = Arguments[ArgumentIndex + 1];
        }
    }

    //
    // Fail if there were not enough arguments.
    //

    if (String1 == NULL) {
        SwPrintError(0, NULL, "Argument expected. Try --help for usage");
        return 1;
    }

    Status = TrCreateSet(String1, Options, 0, &Set1, &Set1Size);
    if (Status != 0) {
        SwPrintError(Status, String1, "Failed to create character set");
        goto MainEnd;
    }

    SqueezeSet = Set1;
    SqueezeSetSize = Set1Size;

    //
    // If there's a second string, create a set from that too.
    //

    if (String2 != NULL) {
        Status = TrCreateSet(String2, 0, Set1Size, &Set2, &Set2Size);
        if (Status != 0) {
            goto MainEnd;
        }

        SqueezeSet = Set2;
        SqueezeSetSize = Set2Size;
    }

    Input = stdin;
    Status = SwSetBinaryMode(fileno(Input), TRUE);
    if (Status != 0) {
        goto MainEnd;
    }

    Status = SwSetBinaryMode(fileno(stdout), TRUE);
    if (Status != 0) {
        goto MainEnd;
    }

    //
    // Begin translation.
    //

    while (TRUE) {
        Character = fgetc(Input);
        if (Character == EOF) {
            break;
        }

        SetIndex = TrIsCharacterInSet(Set1, Set1Size, Character);
        if ((Options & TR_OPTION_DELETE) != 0) {
            if (SetIndex != -1) {
                continue;
            }

        //
        // Translate the character if there's a second set.
        //

        } else if ((SetIndex != -1) && (Set2 != NULL)) {
            if (SetIndex >= Set2Size) {
                SetIndex = Set2Size - 1;
            }

            Character = Set2[SetIndex];
        }

        //
        // If squeeze is on, then omit repeated values.
        //

        if ((Options & TR_OPTION_SQUEEZE) != 0) {
            SetIndex = TrIsCharacterInSet(SqueezeSet,
                                          SqueezeSetSize,
                                          Character);

            if ((SetIndex != -1) && (Character == PreviousCharacter)) {
                continue;
            }
        }

        //
        // Write the translated character back out.
        //

        fputc(Character, stdout);
        PreviousCharacter = Character;
    }

    if (ferror(Input) != 0) {
        Status = errno;
        SwPrintError(Status, NULL, "Failed to read input");
        goto MainEnd;
    }

MainEnd:
    if (Set1 != NULL) {
        free(Set1);
    }

    if (Set2 != NULL) {
        free(Set2);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
TrCreateSet (
    PSTR Argument,
    ULONG Options,
    ULONG ZeroRepeatSize,
    PSTR *NewSet,
    PULONG NewSetSize
    )

/*++

Routine Description:

    This routine creates a translation set for the tr utility.

Arguments:

    Argument - Supplies a pointer to the raw argument.

    Options - Supplies a bitfield of options about this set, namely whether or
        not to complement them.

    ZeroRepeatSize - Supplies the total size of the set if a zero repeat is
        specified. If this is 0, repeats will not be recognized.

    NewSet - Supplies a pointer where the new set will be returned on success.

    NewSetSize - Supplies a pointer where the size of the new set will be
        returned on success.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    INT Character;
    PSTR Complement;
    ULONG ComplementCapacity;
    ULONG ComplementSize;
    PSTR CurrentString;
    CHAR EndRange;
    PCHARACTER_CLASS_FUNCTION IsInClass;
    CHAR PreviousCharacter;
    LONG RepeatIndex;
    LONG RepeatValue;
    PSTR Set;
    ULONG SetCapacity;
    ULONG SetSize;
    INT Status;
    CHAR TwoAgo;

    Complement = NULL;
    ComplementSize = 0;
    ComplementCapacity = 0;
    Set = NULL;
    SetSize = 0;
    SetCapacity = 0;
    CurrentString = Argument;
    PreviousCharacter = 0;
    TwoAgo = 0;
    while (*CurrentString != '\0') {
        IsInClass = NULL;

        //
        // Look for character classes.
        //

        if (*CurrentString == '[') {
            if (strncmp(CurrentString, "[:alnum:]", 9) == 0) {
                IsInClass = isalnum;
                CurrentString += 9;

            } else if (strncmp(CurrentString, "[:alpha:]", 9) == 0) {
                IsInClass = isalpha;
                CurrentString += 9;

            } else if (strncmp(CurrentString, "[:blank:]", 9) == 0) {
                IsInClass = isblank;
                CurrentString += 9;

            } else if (strncmp(CurrentString, "[:cntrl:]", 9) == 0) {
                IsInClass = iscntrl;
                CurrentString += 9;

            } else if (strncmp(CurrentString, "[:digit:]", 9) == 0) {
                IsInClass = isdigit;
                CurrentString += 9;

            } else if (strncmp(CurrentString, "[:graph:]", 9) == 0) {
                IsInClass = isgraph;
                CurrentString += 9;

            } else if (strncmp(CurrentString, "[:lower:]", 9) == 0) {
                IsInClass = islower;
                CurrentString += 9;

            } else if (strncmp(CurrentString, "[:print:]", 9) == 0) {
                IsInClass = isprint;
                CurrentString += 9;

            } else if (strncmp(CurrentString, "[:punct:]", 9) == 0) {
                IsInClass = ispunct;
                CurrentString += 9;

            } else if (strncmp(CurrentString, "[:space:]", 9) == 0) {
                IsInClass = isspace;
                CurrentString += 9;

            } else if (strncmp(CurrentString, "[:upper:]", 9) == 0) {
                IsInClass = isupper;
                CurrentString += 9;

            } else if (strncmp(CurrentString, "[:xdigit:]", 10) == 0) {
                IsInClass = isxdigit;
                CurrentString += 10;
            }
        }

        //
        // If it matched one of the classes, add every character in the
        // class to the set.
        //

        if (IsInClass != NULL) {
            for (Character = 0; Character < MAX_UCHAR; Character += 1) {
                if (IsInClass(Character)) {
                    Status = TrSetAddCharacter(&Set,
                                               Character,
                                               &SetSize,
                                               &SetCapacity);

                    if (Status != 0) {
                        goto CreateSetEnd;
                    }
                }
            }

        } else {
            Character = TrParseCharacter(&CurrentString);
            EndRange = '\0';

            //
            // If the character was a -, then look for a range.
            //

            if ((Character == '-') && (CurrentString != Argument + 1)) {
                EndRange = TrParseCharacter(&CurrentString);

                //
                // If it was at the end, then it's just a literal -.
                //

                if (EndRange == '\0') {
                    Status = TrSetAddCharacter(&Set,
                                               Character,
                                               &SetSize,
                                               &SetCapacity);

                    if (Status != 0) {
                        goto CreateSetEnd;
                    }

                    break;

                //
                // Otherwise, add everything between the range.
                //

                } else {
                    for (Character = PreviousCharacter;
                         Character <= EndRange;
                         Character += 1) {

                        Status = TrSetAddCharacter(&Set,
                                                   Character,
                                                   &SetSize,
                                                   &SetCapacity);

                        if (Status != 0) {
                            goto CreateSetEnd;
                        }
                    }
                }
            }

            //
            // Look for the form [x*N] if the range above didn't match.
            //

            RepeatValue = 0;
            if ((EndRange == '\0') && (Character == '*') && (TwoAgo == '[') &&
                (ZeroRepeatSize != 0)) {

                RepeatValue = strtol(CurrentString, &CurrentString, 10);
                if ((*CurrentString == ']') && (RepeatValue >= 0)) {
                    CurrentString += 1;

                    //
                    // Back out the character and open bracket, then add in
                    // the repeats.
                    //

                    assert(SetSize >= 2);

                    SetSize -= 2;
                    if (RepeatValue == 0) {
                        RepeatValue = ZeroRepeatSize - SetSize;
                    }

                    Character = PreviousCharacter;
                    for (RepeatIndex = 0;
                         RepeatIndex < RepeatValue;
                         RepeatIndex += 1) {

                        Status = TrSetAddCharacter(&Set,
                                                   Character,
                                                   &SetSize,
                                                   &SetCapacity);

                        if (Status != 0) {
                            goto CreateSetEnd;
                        }
                    }
                }
            }

            //
            // If the character didn't fit the end range or repeat form, add it
            // to the set.
            //

            if ((EndRange == '\0') && (RepeatValue <= 0)) {
                Status = TrSetAddCharacter(&Set,
                                           Character,
                                           &SetSize,
                                           &SetCapacity);

                if (Status != 0) {
                    goto CreateSetEnd;
                }
            }
        }

        TwoAgo = PreviousCharacter;
        PreviousCharacter = Character;
    }

    //
    // My, you're looking lovely today!
    //

    if ((Options & TR_OPTION_COMPLEMENT_STRING) != 0) {
        for (Character = 0; Character < MAX_UCHAR; Character += 1) {
            if (TrIsCharacterInSet(Set, SetSize, Character) == -1) {
                Status = TrSetAddCharacter(&Complement,
                                           Character,
                                           &ComplementSize,
                                           &ComplementCapacity);

                if (Status != 0) {
                    goto CreateSetEnd;
                }
            }
        }

        if (Set != NULL) {
            free(Set);
        }

        Set = Complement;
        SetSize = ComplementSize;
        Complement = NULL;
    }

    Status = 0;

CreateSetEnd:
    if (Status != 0) {
        if (Complement != NULL) {
            free(Complement);
        }

        if (Set != NULL) {
            free(Set);
            Set = NULL;
        }

        SetSize = 0;
    }

    *NewSet = Set;
    *NewSetSize = SetSize;
    return Status;
}

INT
TrSetAddCharacter (
    PSTR *Set,
    CHAR Character,
    PULONG Size,
    PULONG Capacity
    )

/*++

Routine Description:

    This routine adds a character to a set.

Arguments:

    Set - Supplies a pointer where the set buffer is stored on input. On output
        contains the resulting set buffer, which may have been reallocated.

    Character - Supplies the character to add.

    Size - Supplies a pointer to the size of the set on input. On output, this
        value will be incremented on success.

    Capacity - Supplies a pointer that on input contains the size of the
        allocation of the passed in set. On output, this value will be updated
        if the buffer was reallocated.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PVOID NewBuffer;
    ULONG NewCapacity;

    //
    // Up the size if needed.
    //

    if (*Size == *Capacity) {
        NewCapacity = *Capacity * 2;
        if (NewCapacity == 0) {
            NewCapacity = TR_INITIAL_STRING_CAPACITY;
        }

        NewBuffer = realloc(*Set, NewCapacity);
        if (NewBuffer == NULL) {
            return ENOMEM;
        }

        *Set = NewBuffer;
        *Capacity = NewCapacity;
    }

    (*Set)[*Size] = Character;
    *Size += 1;
    return 0;
}

CHAR
TrParseCharacter (
    PSTR *Argument
    )

/*++

Routine Description:

    This routine parses an ordinary character, backslash escape character, or
    octal character.

Arguments:

    Argument - Supplies a pointer where the character argumentis stored on
        input. On output this buffer will be advanced past the next character.

Return Value:

    Returns the next character in the argument.

--*/

{

    CHAR Character;
    PSTR CurrentString;
    ULONG DigitIndex;

    CurrentString = *Argument;
    if (*CurrentString == '\\') {

        //
        // Parse an octal sequence of one to three digits.
        //

        if ((*(CurrentString + 1) >= '0') &&
            (*(CurrentString + 1) <= '7')) {

            Character = 0;
            CurrentString += 1;
            for (DigitIndex = 0; DigitIndex < 3; DigitIndex += 1) {
                if ((*CurrentString < '0') || (*CurrentString > '7')) {
                    break;
                }

                Character = Character * 8 + (*CurrentString - '0');
                CurrentString += 1;
            }

        } else {
            switch (*(CurrentString + 1)) {
            case 'a':
                Character = '\a';
                break;

            case 'b':
                Character = '\b';
                break;

            case 'f':
                Character = '\f';
                break;

            case 'r':
                Character = '\r';
                break;

            case 'n':
                Character = '\n';
                break;

            case 't':
                Character = '\t';
                break;

            case 'v':
                Character = '\v';
                break;

            case '\\':
                Character = '\\';
                break;

            //
            // The escape code is not recognized, so treat it as literal
            // characters.
            //

            default:
                Character = '\\';
                CurrentString -= 1;
                break;
            }

            //
            // Skip over the backslash and the next character.
            //

            CurrentString += 2;
        }

    //
    // This is not a backslash. Just return the character.
    //

    } else {
        Character = *CurrentString;
        CurrentString += 1;
    }

    *Argument = CurrentString;
    return Character;
}

INT
TrIsCharacterInSet (
    PSTR Set,
    ULONG SetSize,
    INT Character
    )

/*++

Routine Description:

    This routine determines if the given character is part of the given
    character set.

Arguments:

    Set - Supplies a pointer to the character set buffer.

    SetSize - Supplies the size of the set buffer in bytes.

    Character - Supplies the character to check for.

Return Value:

    Returns the index within the set if the character belongs to the set.

    -1 if the character is not in the set.

--*/

{

    ULONG SetIndex;

    for (SetIndex = 0; SetIndex < SetSize; SetIndex += 1) {
        if (Set[SetIndex] == Character) {
            return SetIndex;
        }
    }

    return -1;
}

