/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    inttypes.h

Abstract:

    This header contains definitions for fixed size integer types.

Author:

    Evan Green 3-May-2013

--*/

#ifndef _INTTYPES_H
#define _INTTYPES_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>
#include <stdint.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// On 64 bit architectures, use long as the 64 bit type and the pointer size.
//

#if (__SIZEOF_POINTER__ == 8)

#define __PRIPTR "l"
#define __PRI64 "l"

//
// On 32 bit architectures, use long long as the 64 bit type, and nothing (int)
// as the pointer size.
//

#elif (__SIZEOF_POINTER__ == 4)

#define __PRIPTR
#define __PRI64 "ll"

#endif

//
// Define print format string constants.
//

#define PRId8 "hhd"
#define PRIi8 "hhi"
#define PRIo8 "hho"
#define PRIu8 "hhu"
#define PRIx8 "hhx"
#define PRIX8 "hhX"

#define PRId16 "hd"
#define PRIi16 "hi"
#define PRIo16 "ho"
#define PRIu16 "hu"
#define PRIx16 "hx"
#define PRIX16 "hX"

#define PRId32 "d"
#define PRIi32 "i"
#define PRIo32 "o"
#define PRIu32 "u"
#define PRIx32 "x"
#define PRIX32 "X"

#define PRId64 __PRI64 "d"
#define PRIi64 __PRI64 "i"
#define PRIo64 __PRI64 "o"
#define PRIu64 __PRI64 "u"
#define PRIx64 __PRI64 "x"
#define PRIX64 __PRI64 "X"

#define PRIdLEAST8 PRId8
#define PRIiLEAST8 PRIi8
#define PRIoLEAST8 PRIo8
#define PRIuLEAST8 PRIu8
#define PRIxLEAST8 PRIx8
#define PRIXLEAST8 PRIX8

#define PRIdLEAST16 PRId16
#define PRIiLEAST16 PRIi16
#define PRIoLEAST16 PRIo16
#define PRIuLEAST16 PRIu16
#define PRIxLEAST16 PRIx16
#define PRIXLEAST16 PRIX16

#define PRIdLEAST32 PRId32
#define PRIiLEAST32 PRIi32
#define PRIoLEAST32 PRIo32
#define PRIuLEAST32 PRIu32
#define PRIxLEAST32 PRIx32
#define PRIXLEAST32 PRIX32

#define PRIdLEAST64 PRId64
#define PRIiLEAST64 PRIi64
#define PRIoLEAST64 PRIo64
#define PRIuLEAST64 PRIu64
#define PRIxLEAST64 PRIx64
#define PRIXLEAST64 PRIX64

#define PRIdFAST8 PRId32
#define PRIiFAST8 PRIi32
#define PRIoFAST8 PRIo32
#define PRIuFAST8 PRIu32
#define PRIxFAST8 PRIx32
#define PRIXFAST8 PRIX32

#define PRIdFAST16 PRId32
#define PRIiFAST16 PRIi32
#define PRIoFAST16 PRIo32
#define PRIuFAST16 PRIu32
#define PRIxFAST16 PRIx32
#define PRIXFAST16 PRIX32

#define PRIdFAST32 PRId32
#define PRIiFAST32 PRIi32
#define PRIoFAST32 PRIo32
#define PRIuFAST32 PRIu32
#define PRIxFAST32 PRIx32
#define PRIXFAST32 PRIX32

#define PRIdFAST64 PRId64
#define PRIiFAST64 PRIi64
#define PRIoFAST64 PRIo64
#define PRIuFAST64 PRIu64
#define PRIxFAST64 PRIx64
#define PRIXFAST64 PRIX64

#define PRIdPTR __PRIPTR "d"
#define PRIiPTR __PRIPTR "i"
#define PRIoPTR __PRIPTR "o"
#define PRIuPTR __PRIPTR "u"
#define PRIxPTR __PRIPTR "x"
#define PRIXPTR __PRIPTR "X"

#define PRIdMAX PRId64
#define PRIiMAX PRIi64
#define PRIoMAX PRIo64
#define PRIuMAX PRIu64
#define PRIxMAX PRIx64
#define PRIXMAX PRIX64

//
// Define scan format strings.
//

#define SCNd8 "hhd"
#define SCNi8 "hhi"
#define SCNo8 "hho"
#define SCNu8 "hhu"
#define SCNx8 "hhx"
#define SCNX8 "hhX"

#define SCNd16 "hd"
#define SCNi16 "hi"
#define SCNo16 "ho"
#define SCNu16 "hu"
#define SCNx16 "hx"
#define SCNX16 "hX"

#define SCNd32 "d"
#define SCNi32 "i"
#define SCNo32 "o"
#define SCNu32 "u"
#define SCNx32 "x"
#define SCNX32 "X"

#define SCNd64 __PRI64 "d"
#define SCNi64 __PRI64 "i"
#define SCNo64 __PRI64 "o"
#define SCNu64 __PRI64 "u"
#define SCNx64 __PRI64 "x"
#define SCNX64 __PRI64 "X"

#define SCNdLEAST8 SCNd8
#define SCNiLEAST8 SCNi8
#define SCNoLEAST8 SCNo8
#define SCNuLEAST8 SCNu8
#define SCNxLEAST8 SCNx8
#define SCNXLEAST8 SCNX8

#define SCNdLEAST16 SCNd16
#define SCNiLEAST16 SCNi16
#define SCNoLEAST16 SCNo16
#define SCNuLEAST16 SCNu16
#define SCNxLEAST16 SCNx16
#define SCNXLEAST16 SCNX16

#define SCNdLEAST32 SCNd32
#define SCNiLEAST32 SCNi32
#define SCNoLEAST32 SCNo32
#define SCNuLEAST32 SCNu32
#define SCNxLEAST32 SCNx32
#define SCNXLEAST32 SCNX32

#define SCNdLEAST64 SCNd64
#define SCNiLEAST64 SCNi64
#define SCNoLEAST64 SCNo64
#define SCNuLEAST64 SCNu64
#define SCNxLEAST64 SCNx64
#define SCNXLEAST64 SCNX64

#define SCNdFAST8 SCNd32
#define SCNiFAST8 SCNi32
#define SCNoFAST8 SCNo32
#define SCNuFAST8 SCNu32
#define SCNxFAST8 SCNx32
#define SCNXFAST8 SCNX32

#define SCNdFAST16 SCNd32
#define SCNiFAST16 SCNi32
#define SCNoFAST16 SCNo32
#define SCNuFAST16 SCNu32
#define SCNxFAST16 SCNx32
#define SCNXFAST16 SCNX32

#define SCNdFAST32 SCNd32
#define SCNiFAST32 SCNi32
#define SCNoFAST32 SCNo32
#define SCNuFAST32 SCNu32
#define SCNxFAST32 SCNx32
#define SCNXFAST32 SCNX32

#define SCNdFAST64 SCNd64
#define SCNiFAST64 SCNi64
#define SCNoFAST64 SCNo64
#define SCNuFAST64 SCNu64
#define SCNxFAST64 SCNx64
#define SCNXFAST64 SCNX64

#define SCNdPTR __PRIPTR "d"
#define SCNiPTR __PRIPTR "i"
#define SCNoPTR __PRIPTR "o"
#define SCNuPTR __PRIPTR "u"
#define SCNxPTR __PRIPTR "x"
#define SCNXPTR __PRIPTR "X"

#define SCNdMAX SCNd64
#define SCNiMAX SCNi64
#define SCNoMAX SCNo64
#define SCNuMAX SCNu64
#define SCNxMAX SCNx64
#define SCNXMAX SCNX64

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
intmax_t
strtoimax (
    const char *String,
    char **StringAfterScan,
    int Base
    );

/*++

Routine Description:

    This routine converts the initial portion of the given string into an
    integer. This routine will scan past any whitespace at the beginning of
    the string. The string may have an optional plus or minus in front of the
    number to indicate sign.

Arguments:

    String - Supplies a pointer to the null terminated string to convert to an
        integer.

    StringAfterScan - Supplies a pointer where a pointer will be returned
        representing the remaining portion of the string after the integer was
        scanned. If the entire string is made up of whitespace or invalid
        characters, then this will point to the beginning of the given string
        (the scanner will not be advanced).

    Base - Supplies the base system to interpret the number as. If zero is
        supplied, the base will be figured out based on the contents of the
        string. If the string begins with 0, it's treated as an octal (base 8)
        number. If the string begins with 1-9, it's treated as a decimal
        (base 10) number. And if the string begins with 0x or 0X, it's treated
        as a hexadecimal (base 16) number. Other base values must be specified
        explicitly here.

Return Value:

    Returns the integer representation of the string. If the value could not be
    converted, 0 is returned, and errno will be set to EINVAL to indicate the
    number could not be converted.

--*/

LIBC_API
uintmax_t
strtoumax (
    const char *String,
    char **StringAfterScan,
    int Base
    );

/*++

Routine Description:

    This routine converts the initial portion of the given string into an
    integer. This routine will scan past any whitespace at the beginning of
    the string. The string may have an optional plus or minus in front of the
    number to indicate sign.

Arguments:

    String - Supplies a pointer to the null terminated string to convert to an
        integer.

    StringAfterScan - Supplies a pointer where a pointer will be returned
        representing the remaining portion of the string after the integer was
        scanned. If the entire string is made up of whitespace or invalid
        characters, then this will point to the beginning of the given string
        (the scanner will not be advanced).

    Base - Supplies the base system to interpret the number as. If zero is
        supplied, the base will be figured out based on the contents of the
        string. If the string begins with 0, it's treated as an octal (base 8)
        number. If the string begins with 1-9, it's treated as a decimal
        (base 10) number. And if the string begins with 0x or 0X, it's treated
        as a hexadecimal (base 16) number. Other base values must be specified
        explicitly here.

Return Value:

    Returns the integer representation of the string. If the value could not be
    converted, 0 is returned, and errno will be set to EINVAL to indicate the
    number could not be converted.

--*/

#ifdef __cplusplus

}

#endif
#endif

