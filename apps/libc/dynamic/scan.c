/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    scan.c

Abstract:

    This module implements string scanning functions.

Author:

    Evan Green 25-May-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

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
ClpStreamScannerGetInput (
    PSCAN_INPUT Input,
    PCHAR Character
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
int
sscanf (
    const char *Input,
    const char *Format,
    ...
    )

/*++

Routine Description:

    This routine scans in a string and converts it to a number of arguments
    based on a format string.

Arguments:

    Input - Supplies a pointer to the input string to scan.

    Format - Supplies the format string that specifies how to convert the input
        to the arguments.

    ... - Supplies the remaining pointer arguments where the scanned data will
        be returned.

Return Value:

    Returns the number of successfully matched items on success. If the input
    ends before the first matching failure or conversion, EOF is returned. If
    a read error occurs, EOF shall be returned and errno shall be set to
    indicate the error.

--*/

{

    va_list ArgumentList;
    ULONG FormatLength;
    ULONG InputLength;
    ULONG ItemsScanned;
    int ReturnValue;
    KSTATUS Status;

    va_start(ArgumentList, Format);
    InputLength = MAX_ULONG;
    FormatLength = MAX_ULONG;
    Status = RtlStringScanVaList((PSTR)Input,
                                 InputLength,
                                 (PSTR)Format,
                                 FormatLength,
                                 CharacterEncodingDefault,
                                 &ItemsScanned,
                                 ArgumentList);

    if (!KSUCCESS(Status)) {
        ReturnValue = EOF;
        errno = ClConvertKstatusToErrorNumber(Status);
        if (Status != STATUS_END_OF_FILE) {
            if (ItemsScanned != 0) {
                ReturnValue = ItemsScanned;
            }
        }

    } else {
        ReturnValue = ItemsScanned;
    }

    va_end(ArgumentList);
    return ReturnValue;
}

LIBC_API
int
vsscanf (
    const char *String,
    const char *Format,
    va_list ArgumentList
    )

/*++

Routine Description:

    This routine scans in a string and converts it to a number of arguments
    based on a format string.

Arguments:

    String - Supplies a pointer to the input string to scan.

    Format - Supplies the format string that specifies how to convert the input
        to the arguments.

    ArgumentList - Supplies the remaining arguments, which are all pointers to
        various types to be scanned.

Return Value:

    Returns the number of successfully matched items on success. If the input
    ends before the first matching failure or conversion, EOF is returned. If
    a read error occurs, EOF shall be returned and errno shall be set to
    indicate the error.

--*/

{

    ULONG FormatLength;
    ULONG InputLength;
    ULONG ItemsScanned;
    int ReturnValue;
    KSTATUS Status;

    InputLength = MAX_ULONG;
    FormatLength = MAX_ULONG;
    Status = RtlStringScanVaList((PSTR)String,
                                 InputLength,
                                 (PSTR)Format,
                                 FormatLength,
                                 CharacterEncodingDefault,
                                 &ItemsScanned,
                                 ArgumentList);

    if (Status == STATUS_END_OF_FILE) {

        assert(ItemsScanned == 0);

        ReturnValue = EOF;

    } else {
        ReturnValue = ItemsScanned;
    }

    return ReturnValue;
}

LIBC_API
int
fscanf (
    FILE *Stream,
    const char *Format,
    ...
    )

/*++

Routine Description:

    This routine scans in a string from a stream and converts it to a number of
    arguments based on a format string.

Arguments:

    Stream - Supplies a pointer to the input stream.

    Format - Supplies the format string that specifies how to convert the input
        to the arguments.

    ... - Supplies the remaining arguments, which are all pointers to
        various types to be scanned.

Return Value:

    Returns the number of successfully matched items on success. If the input
    ends before the first matching failure or conversion, EOF is returned. If
    a read error occurs, EOF shall be returned and errno shall be set to
    indicate the error.

--*/

{

    va_list ArgumentList;
    int Result;

    va_start(ArgumentList, Format);
    Result = vfscanf(Stream, Format, ArgumentList);
    va_end(ArgumentList);
    return Result;
}

LIBC_API
int
vfscanf (
    FILE *Stream,
    const char *Format,
    va_list ArgumentList
    )

/*++

Routine Description:

    This routine scans in a string from a stream and converts it to a number
    of arguments based on a format string.

Arguments:

    Stream - Supplies a pointer to the input stream.

    Format - Supplies the format string that specifies how to convert the input
        to the arguments.

    ArgumentList - Supplies the remaining arguments, which are all pointers to
        various types to be scanned.

Return Value:

    Returns the number of successfully matched items on success. If the input
    ends before the first matching failure or conversion, EOF is returned. If
    a read error occurs, EOF shall be returned and errno shall be set to
    indicate the error.

--*/

{

    int Result;

    ClpLockStream(Stream);
    Result = vfscanf_unlocked(Stream, Format, ArgumentList);
    ClpUnlockStream(Stream);
    return Result;
}

LIBC_API
int
vfscanf_unlocked (
    FILE *Stream,
    const char *Format,
    va_list ArgumentList
    )

/*++

Routine Description:

    This routine scans in a string from a stream and converts it to a number
    of arguments based on a format string. This routine does not acquire the
    stream's lock.

Arguments:

    Stream - Supplies a pointer to the input stream.

    Format - Supplies the format string that specifies how to convert the input
        to the arguments.

    ArgumentList - Supplies the remaining arguments, which are all pointers to
        various types to be scanned.

Return Value:

    Returns the number of successfully matched items on success. If the input
    ends before the first matching failure or conversion, EOF is returned. If
    a read error occurs, EOF shall be returned and errno shall be set to
    indicate the error.

--*/

{

    SCAN_INPUT Input;
    ULONG ItemsScanned;
    int ReturnValue;
    KSTATUS Status;

    RtlZeroMemory(&Input, sizeof(SCAN_INPUT));
    Input.DataU.Context = Stream;
    Input.ReadU.GetInput = ClpStreamScannerGetInput;
    RtlInitializeMultibyteState(&(Input.State), CharacterEncodingDefault);
    Status = RtlScan(&Input,
                     (PSTR)Format,
                     MAX_ULONG,
                     &ItemsScanned,
                     ArgumentList);

    if (Status == STATUS_END_OF_FILE) {

        assert(ItemsScanned == 0);

        ReturnValue = EOF;

    } else {
        ReturnValue = ItemsScanned;
    }

    //
    // Unget any characters that might be there.
    //

    while (Input.ValidUnputCharacters != 0) {
        ungetc_unlocked(Input.UnputCharacters[Input.ValidUnputCharacters - 1],
                        Stream);

        Input.ValidUnputCharacters -= 1;
    }

    return ReturnValue;
}

LIBC_API
int
scanf (
    const char *Format,
    ...
    )

/*++

Routine Description:

    This routine scans in a string from standard in and converts it to a number
    of arguments based on a format string.

Arguments:

    Format - Supplies the format string that specifies how to convert the input
        to the arguments.

    ... - Supplies the remaining arguments, which are all pointers to
        various types to be scanned.

Return Value:

    Returns the number of successfully matched items on success. If the input
    ends before the first matching failure or conversion, EOF is returned. If
    a read error occurs, EOF shall be returned and errno shall be set to
    indicate the error.

--*/

{

    va_list ArgumentList;
    int Result;

    va_start(ArgumentList, Format);
    Result = vscanf(Format, ArgumentList);
    va_end(ArgumentList);
    return Result;
}

LIBC_API
int
vscanf (
    const char *Format,
    va_list ArgumentList
    )

/*++

Routine Description:

    This routine scans in a string from standard in and converts it to a number
    of arguments based on a format string.

Arguments:

    Format - Supplies the format string that specifies how to convert the input
        to the arguments.

    ArgumentList - Supplies the remaining arguments, which are all pointers to
        various types to be scanned.

Return Value:

    Returns the number of successfully matched items on success. If the input
    ends before the first matching failure or conversion, EOF is returned. If
    a read error occurs, EOF shall be returned and errno shall be set to
    indicate the error.

--*/

{

    int Result;

    Result = vfscanf(stdin, Format, ArgumentList);
    return Result;
}

LIBC_API
int
atoi (
    const char *String
    )

/*++

Routine Description:

    This routine converts a string to an integer. This routine is provided for
    compatibility with existing applications. New applications should use
    strtol instead.

Arguments:

    String - Supplies a pointer to the null terminated string to convert to an
        integer.

Return Value:

    Returns the integer representation of the string. If the value could not be
    converted, 0 is returned.

--*/

{

    return (int)strtol(String, NULL, 10);
}

LIBC_API
double
atof (
    const char *String
    )

/*++

Routine Description:

    This routine converts a string to a double floating point value. This
    routine is provided for compatibility with existing applications. New
    applications should use strtod instead.

Arguments:

    String - Supplies a pointer to the null terminated string to convert to a
        double.

Return Value:

    Returns the floating point representation of the string. If the value could
    not be converted, 0 is returned.

--*/

{

    return strtod(String, NULL);
}

LIBC_API
long
atol (
    const char *String
    )

/*++

Routine Description:

    This routine converts a string to an integer. This routine is provided for
    compatibility with existing applications. New applications should use
    strtol instead.

Arguments:

    String - Supplies a pointer to the null terminated string to convert to an
        integer.

Return Value:

    Returns the integer representation of the string. If the value could not be
    converted, 0 is returned.

--*/

{

    return strtol(String, NULL, 10);
}

LIBC_API
long long
atoll (
    const char *String
    )

/*++

Routine Description:

    This routine converts a string to an integer. This routine is provided for
    compatibility with existing applications. New applications should use
    strtoll instead.

Arguments:

    String - Supplies a pointer to the null terminated string to convert to an
        integer.

Return Value:

    Returns the integer representation of the string. If the value could not be
    converted, 0 is returned.

--*/

{

    return strtoll(String, NULL, 10);
}

LIBC_API
float
strtof (
    const char *String,
    char **StringAfterScan
    )

/*++

Routine Description:

    This routine converts the initial portion of the given string into a
    float. This routine will scan past any whitespace at the beginning of
    the string.

Arguments:

    String - Supplies a pointer to the null terminated string to convert to a
        float.

    StringAfterScan - Supplies a pointer where a pointer will be returned
        representing the remaining portion of the string after the float was
        scanned. If the entire string is made up of whitespace or invalid
        characters, then this will point to the beginning of the given string
        (the scanner will not be advanced).

Return Value:

    Returns the float representation of the string. If the value could not be
    converted, 0 is returned, and errno will be set to either EINVAL if the
    number could not be converted or ERANGE if the number is outside of the
    return type's expressible range.

--*/

{

    double Double;

    Double = strtod(String, StringAfterScan);
    return (float)Double;
}

LIBC_API
double
strtod (
    const char *String,
    char **StringAfterScan
    )

/*++

Routine Description:

    This routine converts the initial portion of the given string into a
    double. This routine will scan past any whitespace at the beginning of
    the string.

Arguments:

    String - Supplies a pointer to the null terminated string to convert to a
        double.

    StringAfterScan - Supplies a pointer where a pointer will be returned
        representing the remaining portion of the string after the double was
        scanned. If the entire string is made up of whitespace or invalid
        characters, then this will point to the beginning of the given string
        (the scanner will not be advanced).

Return Value:

    Returns the double representation of the string. If the value could not be
    converted, 0 is returned, and errno will be set to either EINVAL if the
    number could not be converted or ERANGE if the number is outside of the
    return type's expressible range.

--*/

{

    double Double;
    PCSTR RemainingString;
    KSTATUS Status;
    ULONG StringLength;

    StringLength = MAX_ULONG;
    RemainingString = (PSTR)String;
    Status = RtlStringScanDouble(&RemainingString, &StringLength, &Double);
    if (StringAfterScan != NULL) {
        *StringAfterScan = (PSTR)RemainingString;
    }

    if (!KSUCCESS(Status)) {
        if (Status == STATUS_INVALID_SEQUENCE) {
            Status = STATUS_INVALID_PARAMETER;
        }

        errno = ClConvertKstatusToErrorNumber(Status);
        return Double;
    }

    return Double;
}

LIBC_API
long double
strtold (
    const char *String,
    char **StringAfterScan
    )

/*++

Routine Description:

    This routine converts the initial portion of the given string into a
    long double. This routine will scan past any whitespace at the beginning of
    the string.

Arguments:

    String - Supplies a pointer to the null terminated string to convert to a
        long double.

    StringAfterScan - Supplies a pointer where a pointer will be returned
        representing the remaining portion of the string after the long double
        was scanned. If the entire string is made up of whitespace or invalid
        characters, then this will point to the beginning of the given string
        (the scanner will not be advanced).

Return Value:

    Returns the long double representation of the string. If the value could not
    be converted, 0 is returned, and errno will be set to either EINVAL if the
    number could not be converted or ERANGE if the number is outside of the
    return type's expressible range.

--*/

{

    double Double;

    Double = strtod(String, StringAfterScan);
    return (long double)Double;
}

LIBC_API
long
strtol (
    const char *String,
    char **StringAfterScan,
    int Base
    )

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
    converted, 0 is returned, and errno will be set to either EINVAL if the
    number could not be converted or ERANGE if the number is outside of the
    return type's expressible range.

--*/

{

    LONGLONG Integer;
    PCSTR RemainingString;
    KSTATUS Status;
    ULONG StringLength;

    StringLength = MAX_ULONG;
    RemainingString = (PSTR)String;
    Status = RtlStringScanInteger(&RemainingString,
                                  &StringLength,
                                  Base,
                                  TRUE,
                                  &Integer);

    if (StringAfterScan != NULL) {
        *StringAfterScan = (PSTR)RemainingString;
    }

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);

        //
        // On integer overflow, set errno to ERANGE, but still return the
        // value, which will be a very extreme value.
        //

        if (Status == STATUS_INTEGER_OVERFLOW) {
            if (Integer == LLONG_MAX) {
                return LONG_MAX;
            }

            return LONG_MIN;

        } else {
            return 0;
        }
    }

    if (Integer > LONG_MAX) {
        errno = ERANGE;
        return LONG_MAX;

    } else if (Integer < LONG_MIN) {
        errno = ERANGE;
        return LONG_MIN;
    }

    return (LONG)Integer;
}

LIBC_API
long long
strtoll (
    const char *String,
    char **StringAfterScan,
    int Base
    )

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

{

    LONGLONG Integer;
    PCSTR RemainingString;
    KSTATUS Status;
    ULONG StringLength;

    StringLength = MAX_ULONG;
    RemainingString = (PSTR)String;
    Status = RtlStringScanInteger(&RemainingString,
                                  &StringLength,
                                  Base,
                                  TRUE,
                                  &Integer);

    if (StringAfterScan != NULL) {
        *StringAfterScan = (PSTR)RemainingString;
    }

    if (!KSUCCESS(Status)) {
        if (Status != STATUS_INTEGER_OVERFLOW) {
            Integer = 0;
        }

        errno = ClConvertKstatusToErrorNumber(Status);
    }

    return Integer;
}

LIBC_API
unsigned long
strtoul (
    const char *String,
    char **StringAfterScan,
    int Base
    )

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
    converted, 0 is returned, and errno will be set to either EINVAL if the
    number could not be converted or ERANGE if the number is outside of the
    return type's expressible range.

--*/

{

    LONGLONG Integer;
    PCSTR RemainingString;
    KSTATUS Status;
    ULONG StringLength;

    StringLength = MAX_ULONG;
    RemainingString = (PSTR)String;
    Status = RtlStringScanInteger(&RemainingString,
                                  &StringLength,
                                  Base,
                                  FALSE,
                                  &Integer);

    if (StringAfterScan != NULL) {
        *StringAfterScan = (PSTR)RemainingString;
    }

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        if (Status == STATUS_INTEGER_OVERFLOW) {
            return ULONG_MAX;

        } else {
            return 0;
        }
    }

    if ((ULONGLONG)Integer > ULONG_MAX) {
        errno = ERANGE;
        return ULONG_MAX;
    }

    return (ULONG)(ULONGLONG)Integer;
}

LIBC_API
unsigned long long
strtoull (
    const char *String,
    char **StringAfterScan,
    int Base
    )

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

{

    LONGLONG Integer;
    PCSTR RemainingString;
    KSTATUS Status;
    ULONG StringLength;

    StringLength = MAX_ULONG;
    RemainingString = (PSTR)String;
    Status = RtlStringScanInteger(&RemainingString,
                                  &StringLength,
                                  Base,
                                  FALSE,
                                  &Integer);

    if (StringAfterScan != NULL) {
        *StringAfterScan = (PSTR)RemainingString;
    }

    if (!KSUCCESS(Status)) {
        if (Status != STATUS_INTEGER_OVERFLOW) {
            Integer = 0;
        }

        errno = ClConvertKstatusToErrorNumber(Status);
    }

    return (ULONGLONG)Integer;
}

LIBC_API
intmax_t
strtoimax (
    const char *String,
    char **StringAfterScan,
    int Base
    )

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

{

    return strtoll(String, StringAfterScan, Base);
}

LIBC_API
uintmax_t
strtoumax (
    const char *String,
    char **StringAfterScan,
    int Base
    )

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

{

    return strtoull(String, StringAfterScan, Base);
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
ClpStreamScannerGetInput (
    PSCAN_INPUT Input,
    PCHAR Character
    )

/*++

Routine Description:

    This routine retrieves another byte of input from the input scanner for a
    stream based scanner.

Arguments:

    Input - Supplies a pointer to the input scanner structure.

    Character - Supplies a pointer where the character will be returned on
        success.

Return Value:

    TRUE if a character was read.

    FALSE if the end of the file or string was encountered.

--*/

{

    int NewCharacter;

    NewCharacter = fgetc_unlocked(Input->DataU.Context);
    if (NewCharacter == EOF) {
        return FALSE;
    }

    *Character = (CHAR)NewCharacter;
    Input->CharactersRead += 1;
    return TRUE;
}

