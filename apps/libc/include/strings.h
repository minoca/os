/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    strings.h

Abstract:

    This header contains definitions for certain string manipulation functions.

Author:

    Evan Green 20-Aug-2013

--*/

#ifndef _STRINGS_H
#define _STRINGS_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>
#include <stddef.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

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
strcasecmp (
    const char *String1,
    const char *String2
    );

/*++

Routine Description:

    This routine compares two strings for equality, ignoring case. This routine
    will act for the purposes of comparison like all characters are converted
    to lowercase.

Arguments:

    String1 - Supplies the first string to compare.

    String2 - Supplies the second string to compare.

Return Value:

    0 if the strings are equal all the way through their null terminators.

    Non-zero if the strings are different. The sign of the return value will be
    determined by the sign of the difference between the values of the first
    pair of bytes (both interpreted as type unsigned char) that differ in the
    strings being compared.

--*/

LIBC_API
int
strncasecmp (
    const char *String1,
    const char *String2,
    size_t CharacterCount
    );

/*++

Routine Description:

    This routine compares two strings for equality, ignoring case, up to a
    bounded amount. This routine will act for the purposes of comparison like
    all characters are converted to lowercase.

Arguments:

    String1 - Supplies the first string to compare.

    String2 - Supplies the second string to compare.

    CharacterCount - Supplies the maximum number of characters to compare.
        Characters after a null terminator in either string are not compared.

Return Value:

    0 if the strings are equal all the way through their null terminators or
    character count.

    Non-zero if the strings are different. The sign of the return value will be
    determined by the sign of the difference between the values of the first
    pair of bytes (both interpreted as type unsigned char) that differ in the
    strings being compared.

--*/

#ifdef __cplusplus

}

#endif
#endif

