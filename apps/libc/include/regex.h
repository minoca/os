/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    regex.h

Abstract:

    This header contains definitions for compiling and executing Regular
    Expressions.

Author:

    Evan Green 8-Jul-2013

--*/

#ifndef _REGEX_H
#define _REGEX_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>
#include <stddef.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define flags to send into the regular expression compile function.
//

//
// Set this flag to use extended regular expressions, which recognize extra
// symbols like |, +, and ?.
//

#define REG_EXTENDED 0x00000001

//
// Set this flag to ignore case in the match.
//

#define REG_ICASE 0x00000002

//
// Set this flag to only report success/failure during execution, and not save
// the match offsets.
//

#define REG_NOSUB 0x00000004

//
// Set this flag to change newline behavior such that:
// 1. Newlines don't match a . expression or any form of a non-matching list.
// 2. A circumflex (^) will match any zero length string immediately after a
//    newline, regardless of the setting of REG_NOTBOL.
// 3. A dollar sign will match any zero length string before a newline,
//    regardless of the setting of REG_NOTEOL.
//

#define REG_NEWLINE 0x00000008

//
// Define flags to pass into the execution of regular expressions.
//

//
// Set this flag to indicate that the beginning of this string is not the
// beginning of the line, so a circumflex (^) used as an anchor should not
// match.
//

#define REG_NOTBOL 0x00000001

//
// Set this flag to indicate that the end of this string is not the end of the
// line, so a dollar sign ($) used as an anchor should not match.
//

#define REG_NOTEOL 0x00000002

//
// Define regular expression status codes.
//

//
// The regular expression failed to match.
//

#define REG_NOMATCH 1

//
// The regular expression pattern was invalid.
//

#define REG_BADPAT 2

//
// An invalid collating element was referenced.
//

#define REG_ECOLLATE 3

//
// An invalid character class type was referenced.
//

#define REG_ECTYPE 4

//
// A trailing backslash (\) was found in the pattern.
//

#define REG_EESCAPE 5

//
// A number in "\digit" is invalid or in error.
//

#define REG_ESUBREG 6

//
// There is a square bracket [] imbalance.
//

#define REG_EBRACK 7

//
// There is a \(\) or () imbalance.
//

#define REG_EPAREN 8
//
// The contents of \{\} are invalid: either not a number, too large of a number,
// more than two numbers, or the first number was larger than the second.
//

#define REG_BADBR 9

//
// The endpoint in a range expression is invalid.
//

#define REG_ERANGE 10

//
// The system failed a necessary memory allocation.
//

#define REG_ESPACE 11

//
// A '?', '*', or '+' was not preceded by a valid regular expression.
//

#define REG_BADRPT 12

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the type used for offsets into strings in regular expressions.
//

typedef int regoff_t;

/*++

Structure Description:

    This structure defines the regular expression structure.

Members:

    re_nsub - Stores the number of subexpressions in the regular expression.

    re_data - Stores an opaque pointer to the remainder of the regular
        expression data.

--*/

typedef struct _regex_t {
    size_t re_nsub;
    void *re_data;
} regex_t;

/*++

Structure Description:

    This structure defines the regular expression match structure.

Members:

    rm_so - Stores the starting offset of the regular expression.

    rm_eo - Stores one beyond the ending offset of the regular expression.

--*/

typedef struct _regmatch_t {
    regoff_t rm_so;
    regoff_t rm_eo;
} regmatch_t;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
regcomp (
    regex_t *RegularExpression,
    const char *Pattern,
    int Flags
    );

/*++

Routine Description:

    This routine compiles a regular expression.

Arguments:

    RegularExpression - Supplies a pointer to the regular expression structure
        where the compiled form will reside on success.

    Pattern - Supplies a pointer to the pattern input string.

    Flags - Supplies a bitfield of flags governing the behavior of the regular
        expression. See some REG_* definitions.

Return Value:

    0 on success.

    Returns a REG_* status code on failure.

--*/

LIBC_API
int
regexec (
    const regex_t *RegularExpression,
    const char *String,
    size_t MatchArraySize,
    regmatch_t Match[],
    int Flags
    );

/*++

Routine Description:

    This routine executes a regular expression, performing a search of the
    given string to see if it matches the regular expression.

Arguments:

    RegularExpression - Supplies a pointer to the compiled regular expression.

    String - Supplies a pointer to the string to check for a match.

    MatchArraySize - Supplies the number of elements in the match array
        parameter. Supply zero and the match array parameter will be ignored.

    Match - Supplies an optional pointer to an array where the string indices of
        the match and its subexpressions will be returned.

    Flags - Supplies a bitfield of flags governing the search. See some REG_*
        definitions (specifically REG_NOTBOL and REG_NOTEOL).

Return Value:

    0 on successful completion (there was a match).

    REG_NOMATCH if there was no match.

--*/

LIBC_API
void
regfree (
    regex_t *RegularExpression
    );

/*++

Routine Description:

    This routine destroys and frees all resources associated with a compiled
    regular expression.

Arguments:

    RegularExpression - Supplies a pointer to the regular expression structure
        to destroy. The caller owns the structure itself, this routine just
        guts all the innards.

Return Value:

    None.

--*/

LIBC_API
size_t
regerror (
    int ErrorCode,
    const regex_t *Expression,
    char *Buffer,
    size_t BufferSize
    );

/*++

Routine Description:

    This routine returns error information about what went wrong trying to
    compile the regular expression.

Arguments:

    ErrorCode - Supplies the error code returned from a regular expression
        token.

    Expression - Supplies an optional pointer to the expression.

    Buffer - Supplies a pointer to a buffer where the error string will be
        returned, always null terminated.

    BufferSize - Supplies the size of the buffer in bytes.

Return Value:

    Returns the number of bytes needed to hold the entire error string,
    including the null terminator. If the return value is greater than the
    supplied size, then the buffer will be truncated and null terminated.

--*/

#ifdef __cplusplus

}

#endif
#endif

