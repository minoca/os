/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    wchar.h

Abstract:

    This header contains definitions for widc character functions.

Author:

    Evan Green 17-Aug-2013

--*/

#ifndef _WCHAR_H
#define _WCHAR_H

//
// ------------------------------------------------------------------- Includes
//

//
// Set the wint_t macro so that the compiler's stddef.h will define the type.
//

#define __need_wint_t
#include <stddef.h>
#include <libcbase.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define EOF for wchars.
//

#define WEOF (-1)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef unsigned long wctype_t;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
mbsinit (
    const mbstate_t *State
    );

/*++

Routine Description:

    This routine determines if the given state structure is in its initial
    shift state.

Arguments:

    State - Supplies a pointer to the state to query.

Return Value:

    Returns non-zero if the given state was a NULL pointer or is in its initial
    conversion state.

    0 if the given state is not in its initial conversion state.

--*/

LIBC_API
wint_t
btowc (
    int Character
    );

/*++

Routine Description:

    This routine attempts to convert a single byte into a wide character at
    the initial shift state.

Arguments:

    Character - Supplies the character.

Return Value:

    Returns the wide character representation of the character.

    WEOF if the input character is EOF or if the character (cast to an unsigned
    char) does not constitute a valid one byte character in the initial shift
    state.

--*/

LIBC_API
int
wctob (
    wint_t Character
    );

/*++

Routine Description:

    This routine converts the given wide character into its corresponding
    single-byte character if possible, starting at the initial shift state.

Arguments:

    Character - Supplies the wide character to convert to a byte.

Return Value:

    Returns the byte representation of the character.

    EOF if the wide character is invalid or cannot be represented in a single
    byte.

--*/

LIBC_API
size_t
mbtowc (
    wchar_t *WideCharacter,
    const char *MultibyteCharacter,
    size_t ByteCount
    );

/*++

Routine Description:

    This routine attempts to convert a multibyte character into a wide
    character. This routine is equivalent to calling mbrtowc with a NULL
    state pointer.

Arguments:

    WideCharacter - Supplies an optional pointer wehre the converted wide
        character will be returned on success.

    MultibyteCharacter - Supplies a pointer to the multibyte character to
        convert.

    ByteCount - Supplies the maximum number of bytes to inspect in the
        multibyte character buffer.

Return Value:

    0 if the next character is the null character.

    Returns a positive value on success indicating the number of bytes that
    were used to construct the wide character.

    -2 if the byte count was too small, as the multibyte character could only
    be partially assembled with the given maximum number of bytes.

    -1 if an encoding error occurred.

--*/

LIBC_API
size_t
mbrtowc (
    wchar_t *WideCharacter,
    const char *MultibyteCharacter,
    size_t ByteCount,
    mbstate_t *State
    );

/*++

Routine Description:

    This routine attempts to convert a multibyte character into a wide
    character.

Arguments:

    WideCharacter - Supplies an optional pointer wehre the converted wide
        character will be returned on success.

    MultibyteCharacter - Supplies a pointer to the multibyte character to
        convert.

    ByteCount - Supplies the maximum number of bytes to inspect in the
        multibyte character buffer.

    State - Supplies an optional pointer to a multibyte shift state object to
        use. If this value is not supplied, an internal state will be used.
        The downside of using the internal state is that it makes this function
        not thread safe nor reentrant.

Return Value:

    0 if the next character is the null character.

    Returns a positive value on success indicating the number of bytes that
    were used to construct the wide character.

    -2 if the byte count was too small, as the multibyte character could only
    be partially assembled with the given maximum number of bytes.

    -1 if an encoding error occurred.

--*/

LIBC_API
int
wctomb (
    char *MultibyteCharacter,
    wchar_t WideCharacter
    );

/*++

Routine Description:

    This routine attempts to convert a single wide character into a multibyte
    character.

Arguments:

    MultibyteCharacter - Supplies an optional pointer to the buffer where the
        multibyte character will be returned. This buffer is assumed to be at
        least MB_CUR_MAX bytes large. If this is NULL, then this function will
        determine whether or not the given character has state-dependent
        encodings.

    WideCharacter - Supplies a pointer to the wide character to convert. If this
        is a null terminator, then the shift state will be reset to its initial
        shift state.

Return Value:

    0 if the multibyte character is NULL and the character does not have state
    dependent encodings.

    Returns the number of bytes stored in the multibyte array, or that would
    be stored in the array were it non-NULL.

    -1 if an encoding error occurred, and errno may be set to EILSEQ.

--*/

LIBC_API
size_t
wcrtomb (
    char *MultibyteCharacter,
    wchar_t WideCharacter,
    mbstate_t *State
    );

/*++

Routine Description:

    This routine attempts to convert a single wide character into a multibyte
    character.

Arguments:

    MultibyteCharacter - Supplies an optional pointer to the buffer where the
        multibyte character will be returned. This buffer is assumed to be at
        least MB_CUR_MAX bytes large. If this is NULL, then functionality will
        be equivalent to wcrtomb(Buffer, L'\0', State), where Buffer is an
        internal buffer.

    WideCharacter - Supplies a pointer to the wide character to convert. If this
        is a null terminator, then the shift state will be reset to its initial
        shift state.

    State - Supplies an optional pointer to a multibyte shift state object to
        use. If this value is not supplied, an internal state will be used.
        The downside of using the internal state is that it makes this function
        not thread safe nor reentrant.

Return Value:

    Returns the number of bytes stored in the multibyte array.

    -1 if an encoding error occurred, and errno may be set to EILSEQ.

--*/

LIBC_API
size_t
mbstowcs (
    wchar_t *Destination,
    const char *Source,
    size_t DestinationSize
    );

/*++

Routine Description:

    This routine converts a null-terminated sequence of multi-byte characters
    beginning in the inital shift state to a string of wide characters, up to
    and including a null terminator.

Arguments:

    Destination - Supplies an optional pointer where the wide character string
        will be returned.

    Source - Supplies a pointer to the null-terminated multibyte string. No
        characters are examined after a null terminator is found.

    DestinationSize - Supplies the maximum number of elements to place in the
        wide string.

Return Value:

    Returns the number of wide character array elements modified (or required
    if the wide string is NULL), not including the terminating NULL.

    -1 if an invalid character is encountered. The errno variable may be set
    to provide more information.

--*/

LIBC_API
size_t
mbsrtowcs (
    wchar_t *Destination,
    const char **Source,
    size_t DestinationSize,
    mbstate_t *State
    );

/*++

Routine Description:

    This routine converts a null-terminated sequence of multi-byte characters
    beginning in the inital shift state to a string of wide characters, up to
    and including a null terminator.

Arguments:

    Destination - Supplies an optional pointer where the wide character string
        will be returned.

    Source - Supplies a pointer that upon input contains a pointer to the null
        terminated multibyte string to convert. On output, this will contain
        one of two values. If the null terminator was encountered in the
        multibyte string, then the value returned here will be NULL. If the
        conversion stopped because it would exceed the wide string size, then
        the value returned here will be a pointer to the character one after
        the last character successfully converted. If the wide string is NULL,
        the pointer will remained unchanged on output.

    DestinationSize - Supplies the maximum number of elements to place in the
        wide string.

    State - Supplies an optional pointer to a multibyte shift state object to
        use. If this value is not supplied, an internal state will be used.
        The downside of using the internal state is that it makes this function
        not thread safe nor reentrant.

Return Value:

    Returns the number of wide character array elements modified (or required
    if the wide string is NULL), not including the terminating NULL.

    -1 if an invalid character is encountered. The errno variable may be set
    to provide more information.

--*/

LIBC_API
size_t
wcstombs (
    char *Destination,
    const wchar_t *Source,
    size_t DestinationSize
    );

/*++

Routine Description:

    This routine converts a string of wide characters into a multibyte string,
    up to and including a wide null terminator.

Arguments:

    Destination - Supplies an optional pointer to a destination where the
        multibyte characters will be returned.

    Source - Supplies a pointer to the null terminated wide character string to
        convert.

    DestinationSize - Supplies the number of bytes in the destination buffer
        (or the theoretical destination buffer if one was not supplied).

Return Value:

    Returns the number of bytes in the resulting character sequence, not
    including the null terminator (if any).

    -1 if an invalid wide character is encountered. The errno variable may be
    set to provide more information.

--*/

LIBC_API
size_t
wcsrtombs (
    char *Destination,
    const wchar_t **Source,
    size_t DestinationSize,
    mbstate_t *State
    );

/*++

Routine Description:

    This routine converts a string of wide characters into a multibyte string,
    up to and including a wide null terminator.

Arguments:

    Destination - Supplies an optional pointer to a destination where the
        multibyte characters will be returned.

    Source - Supplies a pointer that upon input contains a pointer to the
        null terminated wide character string to convert. On output, this will
        contain one of two values. If the null terminator was encountered in
        the source string, then the value returned here will be NULL. If the
        conversion stopped because it would exceed the destination size,
        then the value returned here will be a pointer to the character one
        after the last character successfully converted. If the destination
        is NULL, the pointer will remained unchanged on ouput.

    DestinationSize - Supplies the number of bytes in the destination buffer
        (or the theoretical destination buffer if one was not supplied).

    State - Supplies an optional pointer to a multibyte shift state object to
        use. If this value is not supplied, an internal state will be used.
        The downside of using the internal state is that it makes this function
        not thread safe nor reentrant.

Return Value:

    Returns the number of bytes in the resulting character sequence, not
    including the null terminator (if any).

    -1 if an invalid wide character is encountered. The errno variable may be
    set to provide more information.

--*/

LIBC_API
int
mblen (
    const char *MultibyteCharacter,
    size_t Size
    );

/*++

Routine Description:

    This routine returns the number of bytes constituting the given multibyte
    character. It shall be equivalent to:
    mbtowc(NULL, MultibyteCharacter, Size);

    except that the builtin state of mbtowc is not affected.

Arguments:

    MultibyteCharacter - Supplies an optional pointer to the multibyte
        character to get the length of.

    Size - Supplies the size of the multibyte character buffer.

Return Value:

    0 if the next character corresponds to the null wide character.

    Returns the positive number of bytes constituting the next character on
    success.

    -2 if the size of the buffer is too small, such that only a partial wide
    character could be constructed using the given bytes.

    -1 on error, and errno will be set to contain more information.

--*/

LIBC_API
size_t
mbrlen (
    const char *MultibyteCharacter,
    size_t Size,
    mbstate_t *State
    );

/*++

Routine Description:

    This routine returns the number of bytes constituting the given multibyte
    character. It shall be equivalent to:
    mbrtowc(NULL, MultibyteCharacter, Size, State);.

Arguments:

    MultibyteCharacter - Supplies an optional pointer to the multibyte
        character to get the length of.

    Size - Supplies the size of the multibyte character buffer.

    State - Supplies an optional pointer to an initialized multibyte conversion
        state buffer. If this is not supplied, an internal state buffer will
        be used, however using the internal one makes this function neither
        safe nor reentrant.

Return Value:

    0 if the next character corresponds to the null wide character.

    Returns the positive number of bytes constituting the next character on
    success.

    -2 if the size of the buffer is too small, such that only a partial wide
    character could be constructed using the given bytes.

    -1 on error, and errno will be set to contain more information.

--*/

LIBC_API
int
wcwidth (
    wchar_t Character
    );

/*++

Routine Description:

    This routine returns the number of display column positions the given wide
    character occupies.

Arguments:

    Character - Supplies the character to examine.

Return Value:

    0 for the null character.

    -1 if the character is not printable.

    Otherwise, returns the number of columns the given character takes up.

--*/

LIBC_API
wint_t
fgetwc (
    FILE *Stream
    );

/*++

Routine Description:

    This routine retrieves the next wide character from the given file stream.

Arguments:

    Stream - Supplies a pointer to the file stream.

Return Value:

    Returns the next wide character in stream on success.

    WEOF on failure or if the end of the file was reached. The error or end of
    file indicators will be set on the stream.

--*/

LIBC_API
wint_t
fgetwc_unlocked (
    FILE *Stream
    );

/*++

Routine Description:

    This routine retrieves the next wide character from the given file stream,
    without acquiring the stream lock.

Arguments:

    Stream - Supplies a pointer to the file stream.

Return Value:

    Returns the next wide character in stream on success.

    WEOF on failure or if the end of the file was reached. The error or end of
    file indicators will be set on the stream.

--*/

LIBC_API
wint_t
getwchar (
    void
    );

/*++

Routine Description:

    This routine reads one wide character from standard in.

Arguments:

    None.

Return Value:

    Returns the wide character from standard in on success.

    WEOF on failure or the end of the file, and errno will contain more
    information.

--*/

LIBC_API
wint_t
getwc (
    FILE *Stream
    );

/*++

Routine Description:

    This routine reads one wide character from the given file stream. It is
    equivalent to the fgetwc function.

Arguments:

    Stream - Supplies a pointer to the open file stream.

Return Value:

    Returns the wide character on success.

    WEOF on failure or the end of the file, and errno will contain more
    information.

--*/

LIBC_API
wchar_t *
fgetws (
    wchar_t *Buffer,
    int ElementCount,
    FILE *Stream
    );

/*++

Routine Description:

    This routine reads wide characters out of the given stream until a newline
    or the maximum number of elements minus one is read. Then the string is
    null terminated.

Arguments:

    Buffer - Supplies a pointer to the wide character array where the read
        characters will be returned.

    ElementCount - Supplies the maximum number of wide characters to return in
        the given buffer.

    Stream - Supplies a pointer to the file stream to read from.

Return Value:

    Returns a pointer to the input buffer on success.

    NULL if a read error occurs or the end of the file is reached. If at the
    end of the file, the end of file indicator will be set on the stream. If an
    error occurs, he error indicator will be set for the stream, and the errno
    variable will be set to provide more information.

--*/

LIBC_API
wchar_t *
fgetws_unlocked (
    wchar_t *Buffer,
    int ElementCount,
    FILE *Stream
    );

/*++

Routine Description:

    This routine reads wide characters out of the given stream until a newline
    or the maximum number of elements minus one is read. Then the string is
    null terminated. This routine does not acquire the stream lock.

Arguments:

    Buffer - Supplies a pointer to the wide character array where the read
        characters will be returned.

    ElementCount - Supplies the maximum number of wide characters to return in
        the given buffer.

    Stream - Supplies a pointer to the file stream to read from.

Return Value:

    Returns a pointer to the input buffer on success.

    NULL if a read error occurs or the end of the file is reached. If at the
    end of the file, the end of file indicator will be set on the stream. If an
    error occurs, he error indicator will be set for the stream, and the errno
    variable will be set to provide more information.

--*/

LIBC_API
wint_t
fputwc (
    wchar_t WideCharacter,
    FILE *Stream
    );

/*++

Routine Description:

    This routine writes the given wide character out to the given stream.

Arguments:

    WideCharacter - Supplies the wide character to write.

    Stream - Supplies the stream to write to.

Return Value:

    Returns the wide character on success.

    EOF on error. The error indicator for the stream will be set and errno will
    be set to contain more information.

--*/

LIBC_API
wint_t
fputwc_unlocked (
    wchar_t WideCharacter,
    FILE *Stream
    );

/*++

Routine Description:

    This routine writes the given wide character out to the given stream
    without acquiring the stream lock.

Arguments:

    WideCharacter - Supplies the wide character to write.

    Stream - Supplies the stream to write to.

Return Value:

    Returns the wide character on success.

    WEOF on error. The error indicator for the stream will be set and errno will
    be set to contain more information.

--*/

LIBC_API
wint_t
putwc (
    wchar_t Character,
    FILE *Stream
    );

/*++

Routine Description:

    This routine writes a wide character to the given file stream. It is
    equivalent to the fputwc function.

Arguments:

    Character - Supplies the character to write.

    Stream - Supplies the stream to write the character to.

Return Value:

    Returns the character it has written on success.

    WEOF on failure, and errno will contain more information.

--*/

LIBC_API
wint_t
putwchar (
    wchar_t Character
    );

/*++

Routine Description:

    This routine writes a wide character to standard out. This routine is
    equivalent to fputwc(Character, stdout).

Arguments:

    Character - Supplies the character to write.

Return Value:

    Returns the character it has written on success.

    WEOF on failure, and errno will contain more information.

--*/

LIBC_API
int
fputws (
    const wchar_t *WideString,
    FILE *Stream
    );

/*++

Routine Description:

    This routine writes the given null-terminated wide character string to the
    given stream.

Arguments:

    WideString - Supplies a pointer to the null terminated wide string to write.
        The null terminator itself will not be written.

    Stream - Supplies the stream to write to.

Return Value:

    Returns a non-negative number on success.

    -1 on failure, and errno will be set to contain more information. The error
    indicator for the stream will also be set.

--*/

LIBC_API
int
fputws_unlocked (
    const wchar_t *WideString,
    FILE *Stream
    );

/*++

Routine Description:

    This routine writes the given null-terminated wide character string to the
    given stream. This routine does not acquire the stream lock.

Arguments:

    WideString - Supplies a pointer to the null terminated wide string to write.
        The null terminator itself will not be written.

    Stream - Supplies the stream to write to.

Return Value:

    Returns a non-negative number on success.

    -1 on failure, and errno will be set to contain more information. The error
    indicator for the stream will also be set.

--*/

LIBC_API
wint_t
ungetwc (
    wint_t Character,
    FILE *Stream
    );

/*++

Routine Description:

    This routine pushes the specified wide character back onto the input
    stream. The pushed back character shall be returned by subsequent reads on
    that stream in the reverse order of their pushing. A successful intervening
    call seek or flush will discard any pushed back bytes for the stream. One
    character of push back is provided.

Arguments:

    Character - Supplies the character (converted to a wchar_t) to push back.

    Stream - Supplies the stream to push the character on to.

Return Value:

    Returns the character pushed back on success.

    EOF on failure, and errno will contain more information.

--*/

LIBC_API
wint_t
ungetwc_unlocked (
    wint_t Character,
    FILE *Stream
    );

/*++

Routine Description:

    This routine pushes the specified wide character back onto the input
    stream. The pushed back character shall be returned by subsequent reads on
    that stream in the reverse order of their pushing. A successful intervening
    call seek or flush will discard any pushed back bytes for the stream. One
    character of push back is provided. This routine does not acquire the
    internal stream lock.

Arguments:

    Character - Supplies the character (converted to a wchar_t) to push back.

    Stream - Supplies the stream to push the character on to.

Return Value:

    Returns the character pushed back on success.

    EOF on failure, and errno will contain more information.

--*/

LIBC_API
int
fwide (
    FILE *Stream,
    int Mode
    );

/*++

Routine Description:

    This routine determines and potentially sets the orientation of the given
    stream.

Arguments:

    Stream - Supplies a pointer to the stream.

    Mode - Supplies an operation to perform. If this parameter is greater than
        zero, then this routine will attempt to make the stream wide-oriented.
        If this parameter is less than zero, this routine will attempt to make
        the stream byte oriented. If this parameter is 0, no change will be
        made to the stream's orientation.

Return Value:

    >0 if after this call the stream is wide-oriented.

    <0 if after this call the stream is byte-oriented.

    0 if the stream has no orientation.

--*/

LIBC_API
int
wprintf (
    const wchar_t *Format,
    ...
    );

/*++

Routine Description:

    This routine prints a formatted wide string to the standard output file
    stream.

Arguments:

    Format - Supplies the printf wide format string.

    ... - Supplies a variable number of arguments, as required by the printf
        format string argument.

Return Value:

    Returns the number of wide characters successfully converted, not
    including the null terminator.

    Returns a negative number if an error was encountered.

--*/

LIBC_API
int
fwprintf (
    FILE *Stream,
    const wchar_t *Format,
    ...
    );

/*++

Routine Description:

    This routine prints a formatted wide string to the given file stream.

Arguments:

    Stream - Supplies the file stream to print to.

    Format - Supplies the printf wide format string.

    ... - Supplies a variable number of arguments, as required by the printf
        format string argument.

Return Value:

    Returns the number of wide characters successfully converted, not
    including the null terminator.

    Returns a negative number if an error was encountered.

--*/

LIBC_API
int
vfwprintf (
    FILE *File,
    const wchar_t *Format,
    va_list Arguments
    );

/*++

Routine Description:

    This routine prints a formatted wide string to the given file pointer.

Arguments:

    File - Supplies a pointer to the file stream to output to.

    Format - Supplies the printf wide format string.

    Arguments - Supplies the argument list to the format string. The va_end
        macro is not invoked on this list.

Return Value:

    Returns the number of wide characters successfully converted, not
    including the null terminator.

    Returns a negative number if an error was encountered.

--*/

LIBC_API
int
vfwprintf_unlocked (
    FILE *File,
    const wchar_t *Format,
    va_list Arguments
    );

/*++

Routine Description:

    This routine prints a formatted wide string to the given file pointer. This
    routine does not acquire the stream lock.

Arguments:

    File - Supplies a pointer to the file stream to output to.

    Format - Supplies the printf wide format string.

    Arguments - Supplies the argument list to the format string. The va_end
        macro is not invoked on this list.

Return Value:

    Returns the number of wide characters successfully converted, not
    including the null terminator.

    Returns a negative number if an error was encountered.

--*/

LIBC_API
int
vwprintf (
    const wchar_t *Format,
    va_list Arguments
    );

/*++

Routine Description:

    This routine prints a formatted wide string to the standard output file
    stream.

Arguments:

    Format - Supplies the printf wide format string.

    Arguments - Supplies the argument list to the format string. The va_end
        macro is not invoked on this list.

Return Value:

    Returns the number of wide characters successfully converted, not
    including the null terminator.

    Returns a negative number if an error was encountered.

--*/

LIBC_API
int
swprintf (
    wchar_t *OutputString,
    size_t OutputStringCount,
    const wchar_t *Format,
    ...
    );

/*++

Routine Description:

    This routine prints a formatted wide string to the given bounded buffer.

Arguments:

    OutputString - Supplies the buffer where the formatted wide string will be
        returned.

    OutputStringCount - Supplies the number of wide characters that can fit in
        the output buffer.

    Format - Supplies the printf wide format string.

    ... - Supplies a variable number of arguments, as required by the printf
        format string argument.

Return Value:

    Returns the number of wide characters successfully converted, not
    including the null terminator.

    Returns a negative number if OutputStringCount or more wide characters
    needed to be converted or if an error was encountered.

--*/

LIBC_API
int
vswprintf (
    wchar_t *OutputString,
    size_t OutputStringSize,
    const wchar_t *Format,
    va_list Arguments
    );

/*++

Routine Description:

    This routine implements the core string print format function.

Arguments:

    OutputString - Supplies a pointer to the buffer where the resulting string
        will be written.

    OutputStringSize - Supplies the size of the output string buffer, in bytes.
        If the format is too long for the output buffer, the resulting string
        will be truncated and the last byte will always be a null terminator.

    Format - Supplies the printf format string.

    Arguments - Supplies the argument list to the format string. The va_end
        macro is not invoked on this list.

Return Value:

    Returns the number of wide characters successfully converted, not
    including the null terminator.

    Returns a negative number if OutputStringCount or more wide characters
    needed to be converted or if an error was encountered.

--*/

LIBC_API
int
swscanf (
    const wchar_t *Input,
    const wchar_t *Format,
    ...
    );

/*++

Routine Description:

    This routine scans in a wide string and converts it to a number of arguments
    based on a format string.

Arguments:

    Input - Supplies a pointer to the wide input string to scan.

    Format - Supplies the format wide string that specifies how to convert the
        input to the arguments.

    ... - Supplies the remaining pointer arguments where the scanned data will
        be returned.

Return Value:

    Returns the number of successfully matched items on success. If the input
    ends before the first matching failure or conversion, EOF is returned. If
    a read error occurs, EOF shall be returned and errno shall be set to
    indicate the error.

--*/

LIBC_API
int
vswscanf (
    const wchar_t *String,
    const wchar_t *Format,
    va_list ArgumentList
    );

/*++

Routine Description:

    This routine scans in a wide string and converts it to a number of arguments
    based on a format string.

Arguments:

    String - Supplies a pointer to the wide input string to scan.

    Format - Supplies the wide format string that specifies how to convert the
        input to the arguments.

    ArgumentList - Supplies the remaining arguments, which are all pointers to
        various types to be scanned.

Return Value:

    Returns the number of successfully matched items on success. If the input
    ends before the first matching failure or conversion, EOF is returned. If
    a read error occurs, EOF shall be returned and errno shall be set to
    indicate the error.

--*/

LIBC_API
int
fwscanf (
    FILE *Stream,
    const wchar_t *Format,
    ...
    );

/*++

Routine Description:

    This routine scans in a string from a stream and converts it to a number
    of arguments based on a wide format string.

Arguments:

    Stream - Supplies a pointer to the input stream.

    Format - Supplies the wide format string that specifies how to convert the
        input to the arguments.

    ... - Supplies the remaining arguments, which are all pointers to
        various types to be scanned.

Return Value:

    Returns the number of successfully matched items on success. If the input
    ends before the first matching failure or conversion, EOF is returned. If
    a read error occurs, EOF shall be returned and errno shall be set to
    indicate the error.

--*/

LIBC_API
int
vfwscanf (
    FILE *Stream,
    const wchar_t *Format,
    va_list ArgumentList
    );

/*++

Routine Description:

    This routine scans in a string from a stream and converts it to a number
    of arguments based on a format string.

Arguments:

    Stream - Supplies a pointer to the input stream.

    Format - Supplies the wide format string that specifies how to convert the
        input to the arguments.

    ArgumentList - Supplies the remaining arguments, which are all pointers to
        various types to be scanned.

Return Value:

    Returns the number of successfully matched items on success. If the input
    ends before the first matching failure or conversion, EOF is returned. If
    a read error occurs, EOF shall be returned and errno shall be set to
    indicate the error.

--*/

LIBC_API
int
vfwscanf_unlocked (
    FILE *Stream,
    const wchar_t *Format,
    va_list ArgumentList
    );

/*++

Routine Description:

    This routine scans in a string from a stream and converts it to a number
    of arguments based on a format string. This routine does not acquire the
    stream's lock.

Arguments:

    Stream - Supplies a pointer to the input stream.

    Format - Supplies the side format string that specifies how to convert the
        input to the arguments.

    ArgumentList - Supplies the remaining arguments, which are all pointers to
        various types to be scanned.

Return Value:

    Returns the number of successfully matched items on success. If the input
    ends before the first matching failure or conversion, EOF is returned. If
    a read error occurs, EOF shall be returned and errno shall be set to
    indicate the error.

--*/

LIBC_API
int
wscanf (
    const wchar_t *Format,
    ...
    );

/*++

Routine Description:

    This routine scans in a string from standard in and converts it to a number
    of arguments based on a format string.

Arguments:

    Format - Supplies the wide format string that specifies how to convert the
        input to the arguments.

    ... - Supplies the remaining arguments, which are all pointers to
        various types to be scanned.

Return Value:

    Returns the number of successfully matched items on success. If the input
    ends before the first matching failure or conversion, EOF is returned. If
    a read error occurs, EOF shall be returned and errno shall be set to
    indicate the error.

--*/

LIBC_API
int
vwscanf (
    const wchar_t *Format,
    va_list ArgumentList
    );

/*++

Routine Description:

    This routine scans in a string from standard in and converts it to a number
    of arguments based on a format string.

Arguments:

    Format - Supplies the wide format string that specifies how to convert the
        input to the arguments.

    ArgumentList - Supplies the remaining arguments, which are all pointers to
        various types to be scanned.

Return Value:

    Returns the number of successfully matched items on success. If the input
    ends before the first matching failure or conversion, EOF is returned. If
    a read error occurs, EOF shall be returned and errno shall be set to
    indicate the error.

--*/

LIBC_API
float
wcstof (
    const wchar_t *String,
    wchar_t **StringAfterScan
    );

/*++

Routine Description:

    This routine converts the initial portion of the given wide string into a
    float. This routine will scan past any whitespace at the beginning of
    the string.

Arguments:

    String - Supplies a pointer to the null terminated wide string to convert
        to afloat.

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

LIBC_API
double
wcstod (
    const wchar_t *String,
    wchar_t **StringAfterScan
    );

/*++

Routine Description:

    This routine converts the initial portion of the given wide string into a
    double. This routine will scan past any whitespace at the beginning of
    the string.

Arguments:

    String - Supplies a pointer to the null terminated wide string to convert
        to a double.

    StringAfterScan - Supplies a pointer where a pointer will be returned
        representing the remaining portion of the wide string after the double
        was scanned. If the entire string is made up of whitespace or invalid
        characters, then this will point to the beginning of the given string
        (the scanner will not be advanced).

Return Value:

    Returns the double representation of the wide string. If the value could
    not be converted, 0 is returned, and errno will be set to either EINVAL if
    the number could not be converted or ERANGE if the number is outside of the
    return type's expressible range.

--*/

LIBC_API
long double
wcstold (
    const wchar_t *String,
    wchar_t **StringAfterScan
    );

/*++

Routine Description:

    This routine converts the initial portion of the given wide string into a
    long double. This routine will scan past any whitespace at the beginning of
    the string.

Arguments:

    String - Supplies a pointer to the null terminated wide string to convert
        to a long double.

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

LIBC_API
long
wcstol (
    const wchar_t *String,
    wchar_t **StringAfterScan,
    int Base
    );

/*++

Routine Description:

    This routine converts the initial portion of the given wide string into an
    integer. This routine will scan past any whitespace at the beginning of
    the string. The string may have an optional plus or minus in front of the
    number to indicate sign.

Arguments:

    String - Supplies a pointer to the null terminated wide string to convert
        to an integer.

    StringAfterScan - Supplies a pointer where a pointer will be returned
        representing the remaining portion of the wide string after the integer
        was scanned. If the entire string is made up of whitespace or invalid
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

LIBC_API
long long
wcstoll (
    const wchar_t *String,
    wchar_t **StringAfterScan,
    int Base
    );

/*++

Routine Description:

    This routine converts the initial portion of the given wide string into an
    integer. This routine will scan past any whitespace at the beginning of
    the string. The string may have an optional plus or minus in front of the
    number to indicate sign.

Arguments:

    String - Supplies a pointer to the null terminated wide string to convert
        to an integer.

    StringAfterScan - Supplies a pointer where a pointer will be returned
        representing the remaining portion of the wide string after the integer
        was scanned. If the entire string is made up of whitespace or invalid
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
long
wcstoul (
    const wchar_t *String,
    wchar_t **StringAfterScan,
    int Base
    );

/*++

Routine Description:

    This routine converts the initial portion of the given wide string into an
    integer. This routine will scan past any whitespace at the beginning of
    the string. The string may have an optional plus or minus in front of the
    number to indicate sign.

Arguments:

    String - Supplies a pointer to the null terminated wide string to convert
        to an integer.

    StringAfterScan - Supplies a pointer where a pointer will be returned
        representing the remaining portion of the wide string after the integer
        was scanned. If the entire string is made up of whitespace or invalid
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

LIBC_API
long long
wcstoull (
    const wchar_t *String,
    wchar_t **StringAfterScan,
    int Base
    );

/*++

Routine Description:

    This routine converts the initial portion of the given wide string into an
    integer. This routine will scan past any whitespace at the beginning of
    the string. The string may have an optional plus or minus in front of the
    number to indicate sign.

Arguments:

    String - Supplies a pointer to the null terminated wide string to convert
        to an integer.

    StringAfterScan - Supplies a pointer where a pointer will be returned
        representing the remaining portion of the wide string after the integer
        was scanned. If the entire string is made up of whitespace or invalid
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
wchar_t *
wmemchr (
    const wchar_t *Buffer,
    wchar_t Character,
    size_t Size
    );

/*++

Routine Description:

    This routine attempts to locate the first occurrence of the given character
    within the given buffer.

Arguments:

    Buffer - Supplies a pointer to the buffer of wide characters.

    Character - Supplies the wide character to locate.

    Size - Supplies the size of the buffer, in characters.

Return Value:

    Returns a pointer to the first occurrence of the character within the
    buffer on success.

    NULL on failure.

--*/

LIBC_API
int
wmemcmp (
    const wchar_t *Left,
    const wchar_t *Right,
    size_t Size
    );

/*++

Routine Description:

    This routine compares two wide strings of memory byte for byte. The null
    wide character is not treated specially here.

Arguments:

    Left - Supplies the first wide string of the comparison.

    Right - Supplies the second wide string of the comparison.

    Size - Supplies the maximum number of characters to compare.

Return Value:

    >0 if Left > Right.

    0 is Left == Right.

    <0 if Left <= Right.

--*/

LIBC_API
wchar_t *
wmemcpy (
    wchar_t *Destination,
    const wchar_t *Source,
    size_t CharacterCount
    );

/*++

Routine Description:

    This routine copies characters directly between buffers. The null wide
    character is not treated specially here.

Arguments:

    Destination - Supplies a pointer to the destination of the copy.

    Source - Supplies a pointer to the source data to copy.

    CharacterCount - Supplies the number of characters to copy.

Return Value:

    Returns the destination parameter.

--*/

LIBC_API
wchar_t *
wmemmove (
    wchar_t *Destination,
    const wchar_t *Source,
    size_t CharacterCount
    );

/*++

Routine Description:

    This routine copies bytes between buffers. Copying takes place as if the
    bytes are first copied into a temporary buffer that does not overlap the
    two buffers, and then are copied to the destination.

Arguments:

    Destination - Supplies a pointer to the destination of the copy.

    Source - Supplies a pointer to the source data to copy.

    CharacterCount - Supplies the number of characters to copy.

Return Value:

    Returns the destination parameter.

--*/

LIBC_API
wchar_t *
wmemset (
    wchar_t *Destination,
    wchar_t Character,
    size_t CharacterCount
    );

/*++

Routine Description:

    This routine copies the given character repeatedly into the given buffer.

Arguments:

    Destination - Supplies a pointer to the destination of the copy.

    Character - Supplies a character (it will be cast down to a character) to
        fill the buffer with.

    CharacterCount - Supplies the number of characters to set.

Return Value:

    Returns the destination parameter.

--*/

LIBC_API
wchar_t *
wcschr (
    const wchar_t *String,
    wchar_t Character
    );

/*++

Routine Description:

    This routine finds the first instance of the given character (converted to
    a char) in the given wide string.

Arguments:

    String - Supplies a pointer to the string to search for the character in.

    Character - Supplies the character to search for.

Return Value:

    Returns a pointer to the first occurrence of the character in the given
    string, or NULL if the character doesn't exist in the string.

--*/

LIBC_API
wchar_t *
wcsrchr (
    const wchar_t *String,
    wchar_t Character
    );

/*++

Routine Description:

    This routine finds the last occurrence of the given character (converted to
    a char) in the given wide string.

Arguments:

    String - Supplies a pointer to the wide string to search for the character
        in.

    Character - Supplies the character to search for.

Return Value:

    Returns a pointer to the last occurrence of the character in the given
    string, or NULL if the character doesn't exist in the string.

--*/

LIBC_API
size_t
wcslen (
    const wchar_t *String
    );

/*++

Routine Description:

    This routine computes the length of the given string, not including the
    null terminator.

Arguments:

    String - Supplies a pointer to the string whose length should be computed.

Return Value:

    Returns the length of the string, not including the null terminator.

--*/

LIBC_API
int
wcswidth (
    const wchar_t *String,
    size_t Size
    );

/*++

Routine Description:

    This routine computes the display width of the given string.

Arguments:

    String - Supplies a pointer to the string whose display width should be
        computed.

    Size - Supplies the size of the string in characters.

Return Value:

    Returns the number of columns the given string occupies.

    -1 if one of the characters is invalid.

--*/

LIBC_API
wchar_t *
wcscpy (
    wchar_t *DestinationString,
    const wchar_t *SourceString
    );

/*++

Routine Description:

    This routine copies the given source wide string over the given destination
    string. This routine should be avoided if at all possible as it can be the
    cause of buffer overflow problems. Use functions like wcsncpy that place
    explicit bounds on the destination buffer.

Arguments:

    DestinationString - Supplies a pointer where the source string will be
        copied to.

    SourceString - Supplies the string that will be copied.

Return Value:

    Returns the destination string.

--*/

LIBC_API
wchar_t *
wcsncpy (
    wchar_t *DestinationString,
    const wchar_t *SourceString,
    size_t CharacterCount
    );

/*++

Routine Description:

    This routine copies the given source string over the given destination
    string.

Arguments:

    DestinationString - Supplies a pointer where the source string will be
        copied to.

    SourceString - Supplies the string that will be copied.

    CharacterCount - Supplies the maximum number of characters to copy. If the
        source string is shorter than this value, then only characters up to
        and including the null terminator will be copied. The remaining
        characters in the destination string will be zeroed out. If the source
        string is longer than this value, then the destination string will not
        be null terminated.

Return Value:

    Returns the destination string.

--*/

LIBC_API
wchar_t *
wcscat (
    wchar_t *DestinationString,
    const wchar_t *SourceString
    );

/*++

Routine Description:

    This routine appends bytes to the end of the given wide string. The
    destination string will always be returned with a null terminator.

Arguments:

    DestinationString - Supplies a pointer containing the string that will be
        appended to.

    SourceString - Supplies a pointer to the string to append.

Return Value:

    Returns the destination string.

--*/

LIBC_API
wchar_t *
wcsncat (
    wchar_t *DestinationString,
    const wchar_t *SourceString,
    size_t CharactersToAppend
    );

/*++

Routine Description:

    This routine appends characters to the end of the given wide string. The
    destination string will always be returned with a wide null terminator.

Arguments:

    DestinationString - Supplies a pointer containing the string that will be
        appended to.

    SourceString - Supplies a pointer to the string to append.

    CharactersToAppend - Supplies the number of bytes of the source string to
        append to the destination, NOT including the null terminator. This
        means that the destination string buffer must be at least large enough
        to take this number plus one bytes on the end of the existing string. If
        the source string is shorter than this value, this routine will stop at
        the terminator.

Return Value:

    Returns the destination string.

--*/

LIBC_API
int
wcscmp (
    const wchar_t *String1,
    const wchar_t *String2
    );

/*++

Routine Description:

    This routine compares two wide strings for equality.

Arguments:

    String1 - Supplies the first wide string to compare.

    String2 - Supplies the second wide string to compare.

Return Value:

    0 if the strings are equal all the way through their null terminators.

    Non-zero if the strings are different. The sign of the return value will be
    determined by the sign of the difference between the values of the first
    pair of bytes (both interpreted as type unsigned char) that differ in the
    strings being compared.

--*/

LIBC_API
int
wcsicmp (
    const wchar_t *String1,
    const wchar_t *String2
    );

/*++

Routine Description:

    This routine compares two wide strings for equality, ignoring case.

Arguments:

    String1 - Supplies the first wide string to compare.

    String2 - Supplies the second wide string to compare.

Return Value:

    0 if the strings are equal all the way through their null terminators.

    Non-zero if the strings are different. The sign of the return value will be
    determined by the sign of the difference between the values of the first
    pair of bytes (both interpreted as type unsigned char) that differ in the
    strings being compared.

--*/

LIBC_API
int
wcsncmp (
    const wchar_t *String1,
    const wchar_t *String2,
    size_t CharacterCount
    );

/*++

Routine Description:

    This routine compares two wide strings for equality, up to a bounded amount.

Arguments:

    String1 - Supplies the first wide string to compare.

    String2 - Supplies the second wide string to compare.

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

LIBC_API
int
wcsnicmp (
    const wchar_t *String1,
    const wchar_t *String2,
    size_t CharacterCount
    );

/*++

Routine Description:

    This routine compares two wide strings for equality, ignoring case, up to a
    bounded amount.

Arguments:

    String1 - Supplies the first wide string to compare.

    String2 - Supplies the second wide string to compare.

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

LIBC_API
int
wcscasecmp (
    const wchar_t *String1,
    const wchar_t *String2
    );

/*++

Routine Description:

    This routine compares two wide strings for equality, ignoring case. This
    routine will act for the purposes of comparison like all characters are
    converted to lowercase.

Arguments:

    String1 - Supplies the first wide string to compare.

    String2 - Supplies the second wide string to compare.

Return Value:

    0 if the strings are equal all the way through their null terminators.

    Non-zero if the strings are different. The sign of the return value will be
    determined by the sign of the difference between the values of the first
    pair of bytes (both interpreted as type unsigned char) that differ in the
    strings being compared.

--*/

LIBC_API
int
wcsncasecmp (
    const wchar_t *String1,
    const wchar_t *String2,
    size_t CharacterCount
    );

/*++

Routine Description:

    This routine compares two wide strings for equality, ignoring case, up to a
    bounded amount. This routine will act for the purposes of comparison like
    all characters are converted to lowercase.

Arguments:

    String1 - Supplies the wide first string to compare.

    String2 - Supplies the wide second string to compare.

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

LIBC_API
int
wcscoll (
    const wchar_t *String1,
    const wchar_t *String2
    );

/*++

Routine Description:

    This routine compares two wide strings, both interpreted as appropriate to
    the LC_COLLATE category of the current locale.

Arguments:

    String1 - Supplies a pointer to the first wide string.

    String2 - Supplies a pointer to the second wide string.

Return Value:

    >0 if the first string is greater than the second string.

    0 if the first string is equal to the second string.

    <0 if the first string is less than the second string.

--*/

LIBC_API
wchar_t *
wcsdup (
    const wchar_t *String
    );

/*++

Routine Description:

    This routine returns a pointer to a newly allocated wide string which is a
    duplicate of the given input wide string. This returned pointer must be
    passed to the free function when the caller is done with it.

Arguments:

    String - Supplies a pointer to the wide string to duplicate.

Return Value:

    Returns a pointer to the newly allocated duplicate wide string on success.

    NULL on failure.

--*/

LIBC_API
wchar_t *
wcspbrk (
    const wchar_t *String,
    const wchar_t *Characters
    );

/*++

Routine Description:

    This routine locates the first occurrence in the given wide string of any
    character from the given character set.

Arguments:

    String - Supplies a pointer to the wide string to search.

    Characters - Supplies a pointer to a null terminated wide string containing
        the acceptable set of characters.

Return Value:

    Returns a pointer within the given string to the first character in the
    requested set.

    NULL if no bytes from the set occur in the given string.

--*/

LIBC_API
size_t
wcscspn (
    const wchar_t *Input,
    const wchar_t *Characters
    );

/*++

Routine Description:

    This routine computes the length in bytes of the initial portion of the
    given input that's made up only of characters not in the given set. For
    example, an input of "abc123" and a set of "0123456789" would return a value
    of 3.

Arguments:

    Input - Supplies a pointer to a null terminated wide string containing the
        string to query.

    Characters - Supplies a pointer to a null terminated wide string containing
        the set of characters.

Return Value:

    Returns the count of initial characters in the string not in the given
    set.

--*/

LIBC_API
size_t
wcsspn (
    const wchar_t *Input,
    const wchar_t *Characters
    );

/*++

Routine Description:

    This routine computes the length in bytes of the initial portion of the
    given input that's made up only of characters from the given set. For
    example, an input of "129th" and a set of "0123456789" would return a value
    of 3.

Arguments:

    Input - Supplies a pointer to a null terminated wide string containing the
        string to query.

    Characters - Supplies a pointer to a null terminated wide string containing
        the acceptable set of characters.

Return Value:

    Returns the count of initial characters in the string in the given set.

--*/

LIBC_API
wchar_t *
wcsstr (
    const wchar_t *InputString,
    const wchar_t *QueryString
    );

/*++

Routine Description:

    This routine attempts to find the first occurrence of the wide query string
    in the given wide input string.

Arguments:

    InputString - Supplies a pointer to the wide input string to search.

    QueryString - Supplies a pointer to the wide query string to search for.

Return Value:

    Returns a pointer within the input string to the first instance of the
    query string.

    NULL if no instances of the query string were found in the input string.

--*/

LIBC_API
wchar_t *
wcswcs (
    const wchar_t *InputString,
    const wchar_t *QueryString
    );

/*++

Routine Description:

    This routine attempts to find the first occurrence of the wide query string
    in the given wide input string.

Arguments:

    InputString - Supplies a pointer to the wide input string to search.

    QueryString - Supplies a pointer to the wide query string to search for.

Return Value:

    Returns a pointer within the input string to the first instance of the
    query string.

    NULL if no instances of the query string were found in the input string.

--*/

LIBC_API
wchar_t *
wcstok (
    wchar_t *InputString,
    const wchar_t *Separators,
    wchar_t **LastToken
    );

/*++

Routine Description:

    This routine breaks a wide string into a series of tokens delimited by any
    character from the given separator set. The first call passes an input
    string in. This routine scans looking for a non-separator character, which
    marks the first token. It then scans looking for a separator character, and
    sets that byte to the null terminator to delimit the first character.
    Subsequent calls should pass NULL as the input string, and the context
    pointer will be updated so that successive calls return the next tokens.
    This routine is thread safe and re-entrant so long as the same context
    pointer is not used by multiple threads.

Arguments:

    InputString - Supplies a pointer to the wide input string to tokenize. If
        supplied, this will reset the value returned in the last token context
        pointer.

    Separators - Supplies a pointer to a null terminated wide string containing
        the set of characters that delimit tokens. This may vary from call to
        call of this routine with the same context pointer.

    LastToken - Supplies a pointer where a context pointer will be stored
        allowing this routine to keep its place and return successive tokens.

Return Value:

    Returns a pointer to the next token on success.

    NULL if there are no more tokens.

--*/

LIBC_API
size_t
wcsxfrm (
    wchar_t *Result,
    const wchar_t *Input,
    size_t ResultSize
    );

/*++

Routine Description:

    This routine transforms the given input string in such a way that using
    strcmp on two transformed strings will return the same value as strcoll
    would return on the untransformed strings. The transformed string is not
    necessarily readable. It is used primarily if a string is going to be
    compared repeatedly, as it explicitly performs the transformation process
    once rather than on each strcoll comparison.

Arguments:

    Result - Supplies an optional pointer where the transformed string will be
        returned. This can be NULL to just get the size of the transformed
        string.

    Input - Supplies a pointer to the string to transform according to the
        current value of LC_COLLATE.

    ResultSize - Supplies the size of the result buffer in bytes. This routine
        will not write more than this number of bytes to the result buffer.

Return Value:

    Returns the size of the complete transform (even if a buffer is not
    supplied or is too small) not including the null terminator byte.

--*/

LIBC_API
size_t
wcsftime (
    wchar_t *Buffer,
    size_t BufferSize,
    const wchar_t *Format,
    const struct tm *Time
    );

/*++

Routine Description:

    This routine converts the given calendar time into a wide string governed
    by the given format string.

Arguments:

    Buffer - Supplies a pointer where the converted wide string will be
        returned.

    BufferSize - Supplies the size of the string buffer in characters.

    Format - Supplies the wide format string to govern the conversion. Ordinary
        characters in the format string will be copied verbatim to the output
        string. Conversions will be substituted for their corresponding value
        in the provided calendar time. The conversions follow the same format
        as the non-wide print time function.

    Time - Supplies a pointer to the calendar time value to use in the
        substitution.

Return Value:

    Returns the number of characters written to the output buffer, including
    the null terminator.

--*/

#ifdef __cplusplus

}

#endif
#endif

