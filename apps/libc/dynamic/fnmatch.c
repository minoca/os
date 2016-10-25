/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fnmatch.c

Abstract:

    This module implements the fnmatch function, which is used to match
    patterns.

Author:

    Evan Green 10-Feb-2015

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <ctype.h>
#include <fnmatch.h>
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

int
ClpFnMatch (
    const char *Pattern,
    const char *String,
    const char *Start,
    int Flags
    );

INT
ClpFnMatchPatternSet (
    const char *Pattern,
    char Character,
    int Flags,
    const char **PatternAfterSet
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
int
fnmatch (
    const char *Pattern,
    const char *String,
    int Flags
    )

/*++

Routine Description:

    This routine matches patterns as described by POSIX in the shell grammar
    sections of "Patterns Matching a Single Character", "Patterns Matching
    Multiple Characters", and "Patterns Used for Filename Expansion".

Arguments:

    Pattern - Supplies a pointer to the null terminated pattern string.

    String - Supplies a pointer to the null terminated string to match against.

    Flags - Supplies a bitfield of flags governing the behavior of the matching
        function. See FNM_* definitions.

Return Value:

    0 if the pattern matches.

    FNM_NOMATCH if the pattern does not match.

    -1 on error.

--*/

{

    return ClpFnMatch(Pattern, String, String, Flags);
}

//
// --------------------------------------------------------- Internal Functions
//

int
ClpFnMatch (
    const char *Pattern,
    const char *String,
    const char *Start,
    int Flags
    )

/*++

Routine Description:

    This routine represents the inner worker for the fnmatch function, which
    may be called recursively.

Arguments:

    Pattern - Supplies a pointer to the null terminated pattern string.

    String - Supplies a pointer to the null terminated string to match against.

    Start - Supplies the original beginning of the string to match against.

    Flags - Supplies a bitfield of flags governing the behavior of the matching
        function. See FNM_* definitions.

Return Value:

    0 if the pattern matches.

    FNM_NOMATCH if the pattern does not match.

    -1 on error.

--*/

{

    const char *PatternAfterSet;
    CHAR PatternCharacter;
    INT SetResult;
    CHAR StringCharacter;

    while (TRUE) {
        PatternCharacter = *Pattern;
        Pattern += 1;
        StringCharacter = *String;
        switch (PatternCharacter) {
        case '\0':
            if (((Flags & FNM_LEADING_DIR) != 0) && (StringCharacter == '/')) {
                return 0;
            }

            if (StringCharacter == PatternCharacter) {
                return 0;
            }

            return FNM_NOMATCH;

        //
        // Question mark matches any character.
        //

        case '?':
            if (StringCharacter == '\0') {
                return FNM_NOMATCH;
            }

            //
            // If the pathname flag is set, don't match wildcards against
            // slashes.
            //

            if ((StringCharacter == '/') && ((Flags & FNM_PATHNAME) != 0)) {
                return FNM_NOMATCH;
            }

            //
            // If the period flag is set, then a leading period is matched
            // explicitly. If the pathname flag is set, then leading means at
            // the beginning of a path component. Otherwise, leading means at
            // the beginning of the string.
            //

            if ((StringCharacter == '.') && ((Flags & FNM_PERIOD) != 0)) {
                if (String == Start) {
                    return FNM_NOMATCH;

                } else if (((Flags & FNM_PATHNAME) != 0) &&
                           (*(String - 1) == '/')) {

                    return FNM_NOMATCH;
                }
            }

            String += 1;
            break;

        //
        // Asterisks match a bunch of any character.
        //

        case '*':

            //
            // Collapse multiple asterisks in a row.
            //

            while (*Pattern == '*') {
                Pattern += 1;
            }

            //
            // Again, if the period flag is set, then leading periods don't
            // match against wildcards.
            //

            if ((StringCharacter == '.') && ((Flags & FNM_PERIOD) != 0)) {
                if (String == Start) {
                    return FNM_NOMATCH;

                } else if (((Flags & FNM_PATHNAME) != 0) &&
                           (*(String - 1) == '/')) {

                    return FNM_NOMATCH;
                }
            }

            //
            // Specially handle an asterisk at the end of the pattern.
            //

            if (*Pattern == '\0') {
                if ((Flags & FNM_PATHNAME) == 0) {
                    return 0;
                }

                //
                // The pathname flag is set. If there are no more path
                // components, or the leading directory flag is set, then this
                // matches. Otherwise, it doesn't.
                //

                if (((Flags & FNM_LEADING_DIR) != 0) ||
                    (strchr(String, '/') == NULL)) {

                    return 0;
                }

                return FNM_NOMATCH;

            //
            // If the next pattern character is a path separator and the
            // pathname flag is set, then the star only matches up to the next
            // slash.
            //

            } else if ((*Pattern == '/') && ((Flags & FNM_PATHNAME) != 0)) {
                String = strchr(String, '/');
                if (String == NULL) {
                    return FNM_NOMATCH;
                }

                //
                // The asterisk matched up to the next slash.
                //

                break;
            }

            //
            // Detrermine how much of the string to chew through with the
            // asterisk.
            //

            while (StringCharacter != '\0') {
                if (ClpFnMatch(Pattern, String, Start, Flags) == 0) {
                    return 0;
                }

                StringCharacter = *String;
                if ((StringCharacter == '/') && ((Flags & FNM_PATHNAME) != 0)) {
                    break;
                }

                String += 1;
            }

            return FNM_NOMATCH;

        //
        // An open bracket matches a set of characters or a character class.
        //

        case '[':
            if (StringCharacter == '\0') {
                return FNM_NOMATCH;
            }

            //
            // Slashes don't match wildcards if the pathname flag is set.
            //

            if ((StringCharacter == '/') && ((Flags & FNM_PATHNAME) != 0)) {
                return FNM_NOMATCH;
            }

            //
            // Leading periods don't match wildcards if the period flag is set.
            //

            if ((StringCharacter == '.') && ((Flags & FNM_PERIOD) != 0)) {
                if (String == Start) {
                    return FNM_NOMATCH;

                } else if (((Flags & FNM_PATHNAME) != 0) &&
                           (*(String - 1) == '/')) {

                    return FNM_NOMATCH;
                }
            }

            SetResult = ClpFnMatchPatternSet(Pattern,
                                             StringCharacter,
                                             Flags,
                                             &PatternAfterSet);

            if (SetResult == 0) {
                Pattern = PatternAfterSet;
                String += 1;
                break;

            } else if (SetResult == FNM_NOMATCH) {
                return FNM_NOMATCH;
            }

            //
            // Fall through if the pattern set had an error and treat it as a
            // normal character. The lack of a break is intentional.
            //

        default:

            //
            // This is the normal character area. If it's a backslash, then the
            // normal character is actually the next character (unless
            // escaping was disabled).
            //

            if (PatternCharacter == '\\') {
                if ((Flags & FNM_NOESCAPE) == 0) {
                    PatternCharacter = *Pattern;
                    Pattern += 1;
                }
            }

            if (PatternCharacter != StringCharacter) {
                if (((Flags & FNM_CASEFOLD) == 0) ||
                    (tolower(PatternCharacter) != tolower(StringCharacter))) {

                    return FNM_NOMATCH;
                }
            }

            String += 1;
            break;
        }
    }

    //
    // Execution never gets here.
    //

    assert(FALSE);

    return -1;
}

INT
ClpFnMatchPatternSet (
    const char *Pattern,
    char Character,
    int Flags,
    const char **PatternAfterSet
    )

/*++

Routine Description:

    This routine matches against a character against a character set.

Arguments:

    Pattern - Supplies a pointer to the null terminated pattern string.

    Character - Supplies the character to match against.

    Flags - Supplies a bitfield of flags governing the behavior of the matching
        function. See FNM_* definitions.

    PatternAfterSet - Supplies a pointer where the advanced pattern string will
        be returned on success.

Return Value:

    0 if the pattern matches.

    FNM_NOMATCH if the pattern does not match.

    -1 on error.

--*/

{

    char EndCharacter;
    BOOL Found;
    BOOL Negated;
    char PatternCharacter;
    const char *PatternStart;

    //
    // Treat a ! or a ^ as a negation of the character set.
    //

    Negated = FALSE;
    if ((*Pattern == '!') || (*Pattern == '^')) {
        Negated = TRUE;
        Pattern += 1;
    }

    if ((Flags & FNM_CASEFOLD) != 0) {
        Character = tolower(Character);
    }

    PatternStart = Pattern;
    Found = FALSE;
    while (TRUE) {

        //
        // Look for the closing bracket, and stop looping once found. If the
        // closing bracket is the very first character, it is treated as a
        // normal character.
        //

        if ((*Pattern == ']') && (Pattern > PatternStart)) {
            Pattern += 1;
            break;

        //
        // This wasn't a valid pattern set if the string ended before a closing
        // bracket (ie. [abc).
        //

        } else if (*Pattern == '\0') {
            return -1;

        //
        // If the pathname flag is set, slashes had better not be in the
        // pattern.
        //

        } else if ((*Pattern == '/') && ((Flags & FNM_PATHNAME) != 0)) {
            return FNM_NOMATCH;

        //
        // Backslash escapes characters (unless disabled).
        //

        } else if ((*Pattern == '\\') && ((Flags & FNM_NOESCAPE) == 0)) {
            Pattern += 1;
        }

        PatternCharacter = *Pattern;
        Pattern += 1;
        if ((Flags & FNM_CASEFOLD) != 0) {
            PatternCharacter = tolower(PatternCharacter);
        }

        //
        // Handle a range, like a-f.
        //

        if ((*Pattern == '-') && (*(Pattern + 1) != '\0') &&
            (*(Pattern + 1) != ']')) {

            Pattern += 1;
            if ((*Pattern == '\\') && ((Flags & FNM_NOESCAPE) == 0) &&
                (*(Pattern + 1) != '\0')) {

                Pattern += 1;
            }

            EndCharacter = *Pattern;
            Pattern += 1;
            if (EndCharacter == '\0') {
                return -1;
            }

            if ((Flags & FNM_CASEFOLD) != 0) {
                EndCharacter = tolower(EndCharacter);
            }

            if ((Character >= PatternCharacter) &&
                (Character <= EndCharacter)) {

                Found = TRUE;
            }

        //
        // Otherwise, just look to see if this particular character matches.
        //

        } else if (Character == PatternCharacter) {
            Found = TRUE;
        }
    }

    *PatternAfterSet = Pattern;
    if (Negated != FALSE) {
        if (Found != FALSE) {
            Found = FALSE;

        } else {
            Found = TRUE;
        }
    }

    if (Found != FALSE) {
        return 0;
    }

    return FNM_NOMATCH;
}
