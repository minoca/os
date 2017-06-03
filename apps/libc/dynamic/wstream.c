/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    wstream.c

Abstract:

    This module implements support for wide character stream operations.

Author:

    Evan Green 28-Aug-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
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
ClpFileFormatWriteWideCharacter (
    INT Character,
    PPRINT_FORMAT_CONTEXT Context
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
wint_t
fgetwc (
    FILE *Stream
    )

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

{

    wint_t Result;

    ClpLockStream(Stream);
    Result = fgetwc_unlocked(Stream);
    ClpUnlockStream(Stream);
    return Result;
}

LIBC_API
wint_t
fgetwc_unlocked (
    FILE *Stream
    )

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

{

    int Character;
    CHAR MultibyteBuffer[MB_LEN_MAX];
    ULONG MultibyteSize;
    int Result;
    wchar_t WideCharacter;

    ORIENT_STREAM(Stream, FILE_FLAG_WIDE_ORIENTED);

    //
    // If there's an unget character, return that.
    //

    if ((Stream->Flags & FILE_FLAG_UNGET_VALID) != 0) {
        Stream->Flags &= ~FILE_FLAG_UNGET_VALID;
        return Stream->UngetCharacter;
    }

    //
    // Loop getting normal characters, adding them to the buffer, and then
    // attempting to convert to a wide character.
    //

    MultibyteSize = 0;
    while (MultibyteSize < MB_LEN_MAX) {
        Character = fgetc_unlocked(Stream);
        if (Character == EOF) {
            return EOF;
        }

        MultibyteBuffer[MultibyteSize] = (CHAR)Character;
        MultibyteSize += 1;
        Result = mbrtowc(&WideCharacter,
                         MultibyteBuffer,
                         MultibyteSize,
                         &(Stream->ShiftState));

        if (Result == 0) {
            return L'\0';

        } else if (Result > 0) {

            assert(Result == MultibyteSize);

            return WideCharacter;

        } else if (Result == -1) {
            Stream->Flags |= FILE_FLAG_ERROR;
            return WEOF;
        }

        //
        // -2 means the conversion function needs more characters. Anything
        // else is unexpected.
        //

        assert(Result == -2);

    }

    //
    // It would be weird if the max weren't really enough to convert any
    // characters.
    //

    assert(FALSE);

    Stream->Flags |= FILE_FLAG_ERROR;
    errno = EILSEQ;
    return WEOF;
}

LIBC_API
wint_t
getwchar (
    void
    )

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

{

    return fgetwc(stdin);
}

LIBC_API
wint_t
getwc (
    FILE *Stream
    )

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

{

    return fgetwc(Stream);
}

LIBC_API
wchar_t *
fgetws (
    wchar_t *Buffer,
    int ElementCount,
    FILE *Stream
    )

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

{

    wchar_t *Result;

    ClpLockStream(Stream);
    Result = fgetws_unlocked(Buffer, ElementCount, Stream);
    ClpUnlockStream(Stream);
    return Result;
}

LIBC_API
wchar_t *
fgetws_unlocked (
    wchar_t *Buffer,
    int ElementCount,
    FILE *Stream
    )

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

{

    wint_t Character;
    int Index;

    Character = WEOF;
    if ((Buffer == NULL) || (ElementCount < 1)) {
        return NULL;
    }

    //
    // Loop reading in characters until the buffer is full.
    //

    for (Index = 0; Index < ElementCount - 1; Index += 1) {
        Character = fgetwc_unlocked(Stream);
        if (Character == WEOF) {
            break;
        }

        Buffer[Index] = Character;
        Index += 1;
        if (Character == L'\n') {
            break;
        }
    }

    Buffer[Index] = L'\0';
    if (Character == WEOF) {
        return NULL;
    }

    return Buffer;
}

LIBC_API
wint_t
fputwc (
    wchar_t WideCharacter,
    FILE *Stream
    )

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

{

    wint_t Result;

    ClpLockStream(Stream);
    Result = fputwc_unlocked(WideCharacter, Stream);
    ClpUnlockStream(Stream);
    return Result;
}

LIBC_API
wint_t
fputwc_unlocked (
    wchar_t WideCharacter,
    FILE *Stream
    )

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

{

    CHAR Buffer[MB_LEN_MAX];
    ULONG ByteIndex;
    size_t Length;
    size_t Result;

    ORIENT_STREAM(Stream, FILE_FLAG_WIDE_ORIENTED);

    //
    // Convert the wide character to a multibyte sequence.
    //

    Length = wcrtomb(Buffer, WideCharacter, &(Stream->ShiftState));
    if (Length == -1) {
        Stream->Flags |= FILE_FLAG_ERROR;
        return WEOF;
    }

    //
    // Write the bytes out.
    //

    for (ByteIndex = 0; ByteIndex < Length; ByteIndex += 1) {
        Result = fputc_unlocked(Buffer[ByteIndex], Stream);
        if (Result == EOF) {
            return WEOF;
        }
    }

    return WideCharacter;
}

LIBC_API
wint_t
putwc (
    wchar_t Character,
    FILE *Stream
    )

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

{

    return fputwc(Character, Stream);
}

LIBC_API
wint_t
putwchar (
    wchar_t Character
    )

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

{

    return fputwc(Character, stdout);
}

LIBC_API
int
fputws (
    const wchar_t *WideString,
    FILE *Stream
    )

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

{

    int Result;

    ClpLockStream(Stream);
    Result = fputws_unlocked(WideString, Stream);
    ClpUnlockStream(Stream);
    return Result;
}

LIBC_API
int
fputws_unlocked (
    const wchar_t *WideString,
    FILE *Stream
    )

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

{

    wint_t Result;

    if (WideString == NULL) {
        return 0;
    }

    while (*WideString != L'\0') {
        Result = fputwc_unlocked(*WideString, Stream);
        if (Result == WEOF) {
            return -1;
        }

        WideString += 1;
    }

    return 0;
}

LIBC_API
wint_t
ungetwc (
    wint_t Character,
    FILE *Stream
    )

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

{

    int Result;

    ClpLockStream(Stream);
    Result = ungetwc_unlocked(Character, Stream);
    ClpUnlockStream(Stream);
    return Result;
}

LIBC_API
wint_t
ungetwc_unlocked (
    wint_t Character,
    FILE *Stream
    )

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

{

    if (Character == WEOF) {
        return WEOF;
    }

    ORIENT_STREAM(Stream, FILE_FLAG_BYTE_ORIENTED);
    if ((Stream->Flags & FILE_FLAG_UNGET_VALID) == 0) {
        Stream->Flags |= FILE_FLAG_UNGET_VALID;
        Stream->Flags &= ~FILE_FLAG_END_OF_FILE;
        Stream->UngetCharacter = (wchar_t)Character;
        return (wchar_t)Character;
    }

    return WEOF;
}

LIBC_API
int
fwide (
    FILE *Stream,
    int Mode
    )

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

{

    ULONG OrientationFlags;

    if (Mode > 0) {
        ORIENT_STREAM(Stream, FILE_FLAG_WIDE_ORIENTED);

    } else if (Mode < 0) {
        ORIENT_STREAM(Stream, FILE_FLAG_BYTE_ORIENTED);
    }

    OrientationFlags = Stream->Flags;
    if ((OrientationFlags & FILE_FLAG_WIDE_ORIENTED) != 0) {
        return 1;

    } else if ((OrientationFlags & FILE_FLAG_BYTE_ORIENTED) != 0) {
        return -1;
    }

    return 0;
}

LIBC_API
int
wprintf (
    const wchar_t *Format,
    ...
    )

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

{

    va_list Arguments;
    int Result;

    va_start(Arguments, Format);
    Result = vfwprintf(stdout, Format, Arguments);
    va_end(Arguments);
    return Result;
}

LIBC_API
int
fwprintf (
    FILE *Stream,
    const wchar_t *Format,
    ...
    )

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

{

    va_list Arguments;
    int Result;

    va_start(Arguments, Format);
    Result = vfwprintf(Stream, Format, Arguments);
    va_end(Arguments);
    return Result;
}

LIBC_API
int
vfwprintf (
    FILE *File,
    const wchar_t *Format,
    va_list Arguments
    )

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

{

    int Result;

    ClpLockStream(File);
    Result = vfwprintf_unlocked(File, Format, Arguments);
    ClpUnlockStream(File);
    return Result;
}

LIBC_API
int
vfwprintf_unlocked (
    FILE *File,
    const wchar_t *Format,
    va_list Arguments
    )

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

{

    PRINT_FORMAT_CONTEXT PrintContext;

    memset(&PrintContext, 0, sizeof(PRINT_FORMAT_CONTEXT));
    PrintContext.Context = File;
    PrintContext.WriteCharacter = ClpFileFormatWriteWideCharacter;
    RtlFormatWide(&PrintContext, (PWSTR)Format, Arguments);
    return PrintContext.CharactersWritten;
}

LIBC_API
int
vwprintf (
    const wchar_t *Format,
    va_list Arguments
    )

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

{

    return vfwprintf(stdout, Format, Arguments);
}

LIBC_API
int
swprintf (
    wchar_t *OutputString,
    size_t OutputStringCount,
    const wchar_t *Format,
    ...
    )

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

{

    va_list Arguments;
    int Result;

    va_start(Arguments, Format);
    Result = vswprintf(OutputString, OutputStringCount, Format, Arguments);
    va_end(Arguments);
    return Result;
}

LIBC_API
int
vswprintf (
    wchar_t *OutputString,
    size_t OutputStringSize,
    const wchar_t *Format,
    va_list Arguments
    )

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

{

    ULONG Result;

    Result = RtlFormatStringWide(OutputString,
                                 OutputStringSize,
                                 CharacterEncodingDefault,
                                 (PWSTR)Format,
                                 Arguments);

    if (Result > OutputStringSize) {
        return -1;
    }

    return Result - 1;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
ClpFileFormatWriteWideCharacter (
    INT Character,
    PPRINT_FORMAT_CONTEXT Context
    )

/*++

Routine Description:

    This routine writes a wide character to the output during a printf-style
    formatting operation.

Arguments:

    Character - Supplies the character to be written.

    Context - Supplies a pointer to the printf-context.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    if (fputwc_unlocked(Character, Context->Context) == -1) {
        return FALSE;
    }

    return TRUE;
}
