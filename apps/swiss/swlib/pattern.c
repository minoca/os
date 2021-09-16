/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pattern.c

Abstract:

    This module implements support for pattern matching used by the shell and
    other utilities.

Author:

    Evan Green 29-Aug-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include "../swlib.h"

#include <assert.h>
#include <ctype.h>
#include <string.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro returns non-zero if the given character is a valid character in a
// name (at any position except the first, it may not be valid in the first
// position).
//

#define SWISS_NAME_CHARACTER(_Character) \
     ((((_Character) >= 'a') && ((_Character) <= 'z')) || \
      (((_Character) >= 'A') && ((_Character) <= 'Z')) || \
      (((_Character) >= '0') && ((_Character) <= '9')) || \
      ((_Character) == '_') || ((_Character) == '#'))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the character classes that can show up.
//

#define CHARACTER_CLASS_ALPHANUMERIC  "[:alnum:]"
#define CHARACTER_CLASS_ALPHABETIC    "[:alpha:]"
#define CHARACTER_CLASS_BLANK         "[:blank:]"
#define CHARACTER_CLASS_CONTROL       "[:cntrl:]"
#define CHARACTER_CLASS_DIGIT         "[:digit:]"
#define CHARACTER_CLASS_GRAPH         "[:graph:]"
#define CHARACTER_CLASS_LOWER_CASE    "[:lower:]"
#define CHARACTER_CLASS_PRINTABLE     "[:print:]"
#define CHARACTER_CLASS_PUNCTUATION   "[:punct:]"
#define CHARACTER_CLASS_SPACE         "[:space:]"
#define CHARACTER_CLASS_UPPER_CASE    "[:upper:]"
#define CHARACTER_CLASS_HEX_DIGIT     "[:xdigit:]"
#define CHARACTER_CLASS_NAME          "[:name:]"

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _CHARACTER_CLASS {
    CharacterClassInvalid,
    CharacterClassNone,
    CharacterClassAlphanumeric,
    CharacterClassAlphabetic,
    CharacterClassBlank,
    CharacterClassControl,
    CharacterClassDigit,
    CharacterClassGraph,
    CharacterClassLowerCase,
    CharacterClassPrintable,
    CharacterClassPunctuation,
    CharacterClassSpace,
    CharacterClassUpperCase,
    CharacterClassHexDigit,
    CharacterClassName
} CHARACTER_CLASS, *PCHARACTER_CLASS;

//
// ----------------------------------------------- Internal Function Prototypes
//

ULONG
SwpMatchBracketExpression (
    PSTR Input,
    UINTN InputSize,
    PSTR BracketExpansion,
    UINTN BracketExpansionSize,
    CHAR NotCharacter,
    PUINTN BracketExpansionLength
    );

CHARACTER_CLASS
SwpIsCharacterClassExpression (
    PSTR String,
    UINTN StringSize,
    PUINTN ExpressionSize
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

BOOL
SwDoesPathPatternMatch (
    PSTR Path,
    UINTN PathSize,
    PSTR Pattern,
    UINTN PatternSize
    )

/*++

Routine Description:

    This routine determines if a given path matches a given pattern. This
    routine assumes it's only comparing path components, and does not do any
    splitting or other special processing on slashes.

Arguments:

    Path - Supplies a pointer to the path component string.

    PathSize - Supplies the size of the path component in bytes including the
        null terminator.

    Pattern - Supplies the pattern string to match against.

    PatternSize - Supplies the size of the pattern string in bytes including
        the null terminator if there is one.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    BOOL Match;

    //
    // Special rule: if the input starts with a period then the pattern needs
    // to also start with an explicit period. Stars, questions, and character
    // classes don't match that leading period.
    //

    if ((PathSize != 0) && (*Path == '.')) {
        if ((PatternSize == 0) || (*Pattern != '.')) {
            return FALSE;
        }
    }

    Match = SwDoesPatternMatch(Path, PathSize, Pattern, PatternSize);
    return Match;
}

BOOL
SwDoesPatternMatch (
    PSTR Input,
    UINTN InputSize,
    PSTR Pattern,
    UINTN PatternSize
    )

/*++

Routine Description:

    This routine determines if the given input matches the given pattern.

Arguments:

    Input - Supplies a pointer to the input string to test.

    InputSize - Supplies the size of the input string in bytes including the
        null terminator.

    Pattern - Supplies a pointer to the pattern string to test against.

    PatternSize - Supplies the size of the pattern string in bytes including
        the null terminator.

Return Value:

    TRUE if the input matches the pattern.

    FALSE if the input does not match the pattern.

--*/

{

    UINTN BracketExpansionLength;
    UINTN InputIndex;
    UINTN LengthMatched;
    BOOL Match;
    CHAR PatternCharacter;
    UINTN PatternIndex;
    UINTN TrialInputIndex;

    InputIndex = 0;
    PatternIndex = 0;

    assert(PatternSize != 0);
    assert(InputSize != 0);

    if (PatternSize <= 1) {
        if (InputSize <= 1) {
            return TRUE;
        }

        return FALSE;
    }

    while (TRUE) {
        PatternCharacter = Pattern[PatternIndex];
        switch (PatternCharacter) {

        //
        // The ? takes any character.
        //

        case '?':
            if (InputIndex == InputSize - 1) {
                return FALSE;
            }

            InputIndex += 1;
            PatternIndex += 1;
            break;

        //
        // The * takes the longest sequence of characters that work to
        // match the remainder of the pattern.
        //

        case '*':
            PatternIndex += 1;

            //
            // If the star is the last thing, then yes it matches.
            //

            if ((PatternIndex == PatternSize - 1) ||
                (Pattern[PatternIndex] == '\0')) {

                return TRUE;
            }

            for (TrialInputIndex = InputSize - 1;
                 TrialInputIndex > InputIndex;
                 TrialInputIndex -= 1) {

                Match = SwDoesPatternMatch(Input + TrialInputIndex,
                                           InputSize - TrialInputIndex,
                                           Pattern + PatternIndex,
                                           PatternSize - PatternIndex);

                if (Match != FALSE) {
                    return TRUE;
                }
            }

            break;

        //
        // The [ opens a bracket expression.
        //

        case '[':
            if (InputIndex == InputSize - 1) {
                return FALSE;
            }

            LengthMatched = SwpMatchBracketExpression(
                                               Input + InputIndex,
                                               InputSize - InputIndex,
                                               Pattern + PatternIndex,
                                               PatternSize - PatternIndex,
                                               '!',
                                               &BracketExpansionLength);

            if (LengthMatched == 0) {
                return FALSE;
            }

            assert(InputIndex + LengthMatched < InputSize);

            InputIndex += LengthMatched;
            PatternIndex += BracketExpansionLength;
            break;

        //
        // Backslashes make the next character literal, unless it's the last
        // character.
        //

        case '\\':
            if (PatternIndex < PatternSize - 1) {
                PatternIndex += 1;
                PatternCharacter = Pattern[PatternIndex];
            }

            //
            // Fall through to the default literal case.
            //

        default:
            if ((InputIndex == InputSize - 1) ||
                (PatternCharacter != Input[InputIndex])) {

                return FALSE;
            }

            InputIndex += 1;
            PatternIndex += 1;
            break;
        }

        //
        // If the pattern has ended, check to see if the input has ended too.
        //

        if ((PatternIndex == PatternSize - 1) ||
            (Pattern[PatternIndex] == '\0')) {

            if ((InputIndex != InputSize - 1) && (Input[InputIndex] != '\0')) {
                return FALSE;
            }

            return TRUE;
        }
    }

    //
    // Execution should never get here.
    //

    assert(FALSE);

    return FALSE;
}

//
// --------------------------------------------------------- Internal Functions
//

ULONG
SwpMatchBracketExpression (
    PSTR Input,
    UINTN InputSize,
    PSTR BracketExpansion,
    UINTN BracketExpansionSize,
    CHAR NotCharacter,
    PUINTN BracketExpansionLength
    )

/*++

Routine Description:

    This routine attempts to match against a bracket expansion portion of a
    regular expression.

Arguments:

    Input - Supplies a pointer to the input string to test.

    InputSize - Supplies the size of the input string in bytes including the
        null terminator.

    BracketExpansion - Supplies a pointer to the pattern string to test against.

    BracketExpansionSize - Supplies the size of the pattern string in bytes
        including the null terminator.

    NotCharacter - Supplies the character for "not in list" statements. Usually
        for regular expressions this is a circumflex (^), but for the shell it's
        a bang (!).

    BracketExpansionLength - Supplies a pointer where the length of the bracket
        expansion in bytes will be returned.

Return Value:

    Returns the number of characters that matched the expression, either 0 or 1.

--*/

{

    CHARACTER_CLASS CharacterClass;
    UINTN ExpansionIndex;
    UINTN ExpansionSize;
    BOOL FoundClosingBracket;
    UINTN InnerExpansionSize;
    CHAR InputCharacter;
    BOOL Match;
    UINTN MatchCount;
    BOOL Not;
    CHAR PatternCharacter;
    CHAR RangeBegin;
    CHAR RangeEnd;

    ExpansionSize = 0;
    MatchCount = 0;
    if (InputSize == 0) {
        goto MatchBracketExpressionEnd;
    }

    //
    // Skip over the opening brace.
    //

    assert((BracketExpansionSize != 0) && (BracketExpansion[0] == '['));

    if (BracketExpansionSize < 2) {
        goto MatchBracketExpressionEnd;
    }

    BracketExpansion += 1;
    BracketExpansionSize -= 1;
    ExpansionSize += 1;

    //
    // Look for the character that inverts the logic.
    //

    Not = FALSE;
    if (BracketExpansion[0] == NotCharacter) {
        Not = TRUE;
        BracketExpansion += 1;
        BracketExpansionSize -= 1;
        ExpansionSize += 1;
    }

    //
    // Loop once to find the expansion size.
    //

    FoundClosingBracket = FALSE;
    ExpansionIndex = 0;
    while (ExpansionIndex < BracketExpansionSize) {
        PatternCharacter = BracketExpansion[ExpansionIndex];
        CharacterClass = CharacterClassNone;
        if (PatternCharacter == '[') {
            CharacterClass = SwpIsCharacterClassExpression(
                                         BracketExpansion + ExpansionIndex,
                                         BracketExpansionSize - ExpansionIndex,
                                         &InnerExpansionSize);

        } else if ((PatternCharacter == ']') && (ExpansionIndex != 0)) {
            ExpansionIndex += 1;
            FoundClosingBracket = TRUE;
            break;
        }

        if (CharacterClass != CharacterClassNone) {
            ExpansionIndex += InnerExpansionSize - 1;

        } else {
            ExpansionIndex += 1;
        }
    }

    if (FoundClosingBracket == FALSE) {
        ExpansionSize = 0;
        goto MatchBracketExpressionEnd;
    }

    ExpansionSize += ExpansionIndex;

    assert(BracketExpansion[ExpansionIndex - 1] == ']');

    ExpansionIndex = 0;
    InputCharacter = Input[MatchCount];

    //
    // Loop over every character in the bracket expansion to try to find
    // one that matches this input character.
    //

    Match = FALSE;
    while (ExpansionIndex < ExpansionSize - 1) {
        Match = FALSE;
        PatternCharacter = BracketExpansion[ExpansionIndex];
        CharacterClass = CharacterClassNone;
        InnerExpansionSize = 0;
        if (PatternCharacter == '[') {
            CharacterClass = SwpIsCharacterClassExpression(
                                         BracketExpansion + ExpansionIndex,
                                         ExpansionSize - ExpansionIndex,
                                         &InnerExpansionSize);
        }

        switch (CharacterClass) {
        case CharacterClassNone:
            if (PatternCharacter == InputCharacter) {
                Match = TRUE;
            }

            //
            // If it's a dash and there are characters before and after it,
            // it's a range.
            //

            if ((PatternCharacter == '-') && (ExpansionIndex != 0) &&
                (ExpansionIndex != ExpansionSize - 1)) {

                RangeBegin = BracketExpansion[ExpansionIndex - 1];
                RangeEnd = BracketExpansion[ExpansionIndex + 1];
                if ((InputCharacter >= RangeBegin) &&
                    (InputCharacter <= RangeEnd)) {

                    Match = TRUE;
                }

            } else if (PatternCharacter == InputCharacter) {
                Match = TRUE;
            }

            break;

        case CharacterClassAlphanumeric:
            if (isalnum(InputCharacter)) {
                Match = TRUE;
            }

            break;

        case CharacterClassAlphabetic:
            if (isalpha(InputCharacter)) {
                Match = TRUE;
            }

            break;

        case CharacterClassBlank:
            if (isblank(InputCharacter)) {
                Match = TRUE;
            }

            break;

        case CharacterClassControl:
            if (iscntrl(InputCharacter)) {
                Match = TRUE;
            }

            break;

        case CharacterClassDigit:
            if (isdigit(InputCharacter)) {
                Match = TRUE;
            }

            break;

        case CharacterClassGraph:
            if (isgraph(InputCharacter)) {
                Match = TRUE;
            }

            break;

        case CharacterClassLowerCase:
            if (islower(InputCharacter)) {
                Match = TRUE;
            }

            break;

        case CharacterClassPrintable:
            if (isprint(InputCharacter)) {
                Match = TRUE;
            }

            break;

        case CharacterClassPunctuation:
            if (ispunct(InputCharacter)) {
                Match = TRUE;
            }

            break;

        case CharacterClassSpace:
            if (isspace(InputCharacter)) {
                Match = TRUE;
            }

            break;

        case CharacterClassUpperCase:
            if (isupper(InputCharacter)) {
                Match = TRUE;
            }

            break;

        case CharacterClassHexDigit:
            if (isxdigit(InputCharacter)) {
                Match = TRUE;
            }

            break;

        case CharacterClassName:
            if (SWISS_NAME_CHARACTER(InputCharacter)) {
                Match = TRUE;
            }

            break;

        default:

            assert(FALSE);

            goto MatchBracketExpressionEnd;
        }

        //
        // If it matches, stop looking.
        //

        if (Match != FALSE) {
            break;
        }

        if (InnerExpansionSize != 0) {
            ExpansionIndex += InnerExpansionSize - 1;

        } else {
            ExpansionIndex += 1;
        }
    }

    //
    // Negate the results if desired.
    //

    if (Not != FALSE) {
        if (Match != FALSE) {
            Match = FALSE;

        } else {
            Match = TRUE;
        }
    }

    //
    // If it did not match, this is the end of the span.
    //

    if (Match == FALSE) {
        goto MatchBracketExpressionEnd;
    }

    MatchCount += 1;

MatchBracketExpressionEnd:
    *BracketExpansionLength = ExpansionSize;
    return MatchCount;
}

CHARACTER_CLASS
SwpIsCharacterClassExpression (
    PSTR String,
    UINTN StringSize,
    PUINTN ExpressionSize
    )

/*++

Routine Description:

    This routine checks to see if the given string is a character class
    expression.

Arguments:

    String - Supplies a pointer to the string containing the potential
        character class expression.

    StringSize - Supplies the total size of the string in bytes including the
        null terminator.

    ExpressionSize - Supplies a pointer where the size of the expression in
        bytes will be returned on success. Zero will be returned if this is
        not a character class expression.

Return Value:

    Returns the number of characters that matched the expression.

--*/

{

    INT Match;

    *ExpressionSize = 0;
    if (StringSize < 4) {
        return CharacterClassNone;
    }

    if ((String[0] != '[') || (String[1] != ':')) {
        return CharacterClassNone;
    }

    Match = FALSE;
    switch (String[2]) {
    case 'a':
        if (StringSize >= sizeof(CHARACTER_CLASS_ALPHANUMERIC)) {
            Match = strncmp(String + 2,
                            &CHARACTER_CLASS_ALPHANUMERIC[2],
                            sizeof(CHARACTER_CLASS_ALPHANUMERIC) - 3);

            if (Match == 0) {
                *ExpressionSize = sizeof(CHARACTER_CLASS_ALPHANUMERIC);
                return CharacterClassAlphanumeric;
            }
        }

        if (StringSize >= sizeof(CHARACTER_CLASS_ALPHABETIC)) {
            Match = strncmp(String + 2,
                            &CHARACTER_CLASS_ALPHABETIC[2],
                            sizeof(CHARACTER_CLASS_ALPHABETIC) - 3);

            if (Match == 0) {
                *ExpressionSize = sizeof(CHARACTER_CLASS_ALPHABETIC);
                return CharacterClassAlphabetic;
            }
        }

        break;

    case 'b':
        if (StringSize >= sizeof(CHARACTER_CLASS_BLANK)) {
            Match = strncmp(String + 2,
                            &CHARACTER_CLASS_BLANK[2],
                            sizeof(CHARACTER_CLASS_BLANK) - 3);

            if (Match == 0) {
                *ExpressionSize = sizeof(CHARACTER_CLASS_BLANK);
                return CharacterClassBlank;
            }
        }

        break;

    case 'c':
        if (StringSize >= sizeof(CHARACTER_CLASS_CONTROL)) {
            Match = strncmp(String + 2,
                            &CHARACTER_CLASS_CONTROL[2],
                            sizeof(CHARACTER_CLASS_CONTROL) - 3);

            if (Match == 0) {
                *ExpressionSize = sizeof(CHARACTER_CLASS_CONTROL);
                return CharacterClassControl;
            }
        }

        break;

    case 'd':
        if (StringSize >= sizeof(CHARACTER_CLASS_DIGIT)) {
            Match = strncmp(String + 2,
                            &CHARACTER_CLASS_DIGIT[2],
                            sizeof(CHARACTER_CLASS_DIGIT) - 3);

            if (Match == 0) {
                *ExpressionSize = sizeof(CHARACTER_CLASS_DIGIT);
                return CharacterClassDigit;
            }
        }

        break;

    case 'g':
        if (StringSize >= sizeof(CHARACTER_CLASS_GRAPH)) {
            Match = strncmp(String + 2,
                            &CHARACTER_CLASS_GRAPH[2],
                            sizeof(CHARACTER_CLASS_GRAPH) - 3);

            if (Match == 0) {
                *ExpressionSize = sizeof(CHARACTER_CLASS_GRAPH);
                return CharacterClassGraph;
            }
        }

        break;

    case 'l':
        if (StringSize >= sizeof(CHARACTER_CLASS_LOWER_CASE)) {
            Match = strncmp(String + 2,
                            &CHARACTER_CLASS_LOWER_CASE[2],
                            sizeof(CHARACTER_CLASS_LOWER_CASE) - 3);

            if (Match == 0) {
                *ExpressionSize = sizeof(CHARACTER_CLASS_LOWER_CASE);
                return CharacterClassLowerCase;
            }
        }

        break;

    case 'n':
        if (StringSize >= sizeof(CHARACTER_CLASS_NAME)) {
            Match = strncmp(String + 2,
                            &CHARACTER_CLASS_NAME[2],
                            sizeof(CHARACTER_CLASS_NAME) - 3);

            if (Match == 0) {
                *ExpressionSize = sizeof(CHARACTER_CLASS_NAME);
                return CharacterClassName;
            }
        }

        break;

    case 'p':
        if (StringSize >= sizeof(CHARACTER_CLASS_PRINTABLE)) {
            Match = strncmp(String + 2,
                            &CHARACTER_CLASS_PRINTABLE[2],
                            sizeof(CHARACTER_CLASS_PRINTABLE) - 3);

            if (Match == 0) {
                *ExpressionSize = sizeof(CHARACTER_CLASS_PRINTABLE);
                return CharacterClassPrintable;
            }
        }

        if (StringSize >= sizeof(CHARACTER_CLASS_PUNCTUATION)) {
            Match = strncmp(String + 2,
                            &CHARACTER_CLASS_PUNCTUATION[2],
                            sizeof(CHARACTER_CLASS_PUNCTUATION) - 3);

            if (Match == 0) {
                *ExpressionSize = sizeof(CHARACTER_CLASS_PUNCTUATION);
                return CharacterClassPunctuation;
            }
        }

        break;

    case 's':
        if (StringSize >= sizeof(CHARACTER_CLASS_SPACE)) {
            Match = strncmp(String + 2,
                            &CHARACTER_CLASS_SPACE[2],
                            sizeof(CHARACTER_CLASS_SPACE) - 3);

            if (Match == 0) {
                *ExpressionSize = sizeof(CHARACTER_CLASS_SPACE);
                return CharacterClassSpace;
            }
        }

        break;

    case 'u':
        if (StringSize >= sizeof(CHARACTER_CLASS_UPPER_CASE)) {
            Match = strncmp(String + 2,
                            &CHARACTER_CLASS_UPPER_CASE[2],
                            sizeof(CHARACTER_CLASS_UPPER_CASE) - 3);

            if (Match == 0) {
                *ExpressionSize = sizeof(CHARACTER_CLASS_UPPER_CASE);
                return CharacterClassUpperCase;
            }
        }

        break;

    case 'x':
        if (StringSize >= sizeof(CHARACTER_CLASS_HEX_DIGIT)) {
            Match = strncmp(String + 2,
                            &CHARACTER_CLASS_HEX_DIGIT[2],
                            sizeof(CHARACTER_CLASS_HEX_DIGIT) - 3);

            if (Match == 0) {
                *ExpressionSize = sizeof(CHARACTER_CLASS_HEX_DIGIT);
                return CharacterClassHexDigit;
            }
        }

        break;

    default:
        return CharacterClassNone;
    }

    return CharacterClassNone;
}

