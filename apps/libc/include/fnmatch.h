/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    fnmatch.h

Abstract:

    This header contains the definition of the fnmatch function, used for
    pattern matching.

Author:

    Evan Green 10-Feb-2015

--*/

#ifndef _FNMATCH_H
#define _FNMATCH_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define flags that can be passed in to fnmatch.
//

//
// If this flag is set, wildcards will not match slashes '/'.
//

#define FNM_PATHNAME 0x00000001

//
// If this flag is set, backslashes don't quote special characters.
//

#define FNM_NOESCAPE 0x00000002

//
// If this flag is set, a leading period is matched explicitly.
//

#define FNM_PERIOD 0x00000004

//
// If this flag is set, ignore the remainder of a path (/...) after a match.
//

#define FNM_LEADING_DIR 0x00000008

//
// If this flag is set, case is ignored.
//

#define FNM_CASEFOLD 0x00000010

//
// This value is returned by fnmatch if there is no match.
//

#define FNM_NOMATCH 1

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
fnmatch (
    const char *Pattern,
    const char *String,
    int Flags
    );

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

#ifdef __cplusplus

}

#endif
#endif

