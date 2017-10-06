/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    stdio.h

Abstract:

    This header contains standard C input and output definitions.

Author:

    Evan Green 4-Mar-2013

--*/

#ifndef _STDIO_H
#define _STDIO_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stddef.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define the End Of File value returned by some file operation functions.
//

#define EOF (-1)

//
// Define the size of the standard I/O file stream buffers.
//

#define BUFSIZ 8192

//
// Define file buffering modes.
//

//
// In this mode files are fully buffered, meaning they will use the internal
// stream buffer to batch calls to the operating system as much as possible.
//

#define _IOFBF 1

//
// In this mode files are line buffered, which is the same as fully buffered
// except a flush occurs whenever a newline is output.
//

#define _IOLBF 2

//
// In this mode files are not buffered, all reads and writes go directly to the
// low level file interface.
//

#define _IONBF 3

//
// Define seek reference locations.
//

//
// Specify this value to indicate that an offset starts from the beginning of
// the file (the offset is set directly as the file pointer).
//

#define SEEK_SET 0

//
// Specify this value to indicate that the offset should be added to the
// current file position.
//

#define SEEK_CUR 1

//
// Specify this value to indicate that the offset should be added to the end
// of the file.
//

#define SEEK_END 2

//
// Define the maximum number of streams which the implementation guarantees
// can be open simultaneously. It's effectively limitless, but a value is
// presented here for compatibility.
//

#define FOPEN_MAX 16

//
// Define the maximum size of a temporary file name.
//

#define L_tmpnam 20

//
// Define the maximum size of the controlling terminal.
//

#define L_ctermid 256

//
// Define the location of a temporary directory.
//

#define P_tmpdir "/tmp"

//
// Define the number of times an application can call the temporary file name
// functions reliably.
//

#define TMP_MAX 60466176

//
// Define the maximum reliable length of a file name.
//

#define FILENAME_MAX 4096

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the opaque file type.
//

typedef struct _FILE FILE;

//
// Define the multibyte state in here as opposed to wchar.h to avoid a nasty
// circular dependency where stdio.h needs mbstate_t, but wchar.h needs FILE.
//

/*++

Structure Description:

    This structure stores the current shift state for a multibyte character
    conversion.

Members:

    Data - Stores the opaque multibyte state data.

--*/

typedef struct {
    long Data[6];
} mbstate_t;

/*++

Structure Description:

    This structure defines the type used for completely specifying a file type.
    Users must treat this as an opaque type, as its contents may change without
    notice.

Members:

    Offset - Storees the file offset.

    ShiftState - Stores the multi-byte shift state at the given offset.

--*/

typedef struct {
    off_t Offset;
    mbstate_t ShiftState;
} fpos_t;

//
// -------------------------------------------------------------------- Globals
//

//
// Define the standard file pointers.
//

LIBC_API extern FILE *stdin;
LIBC_API extern FILE *stdout;
LIBC_API extern FILE *stderr;

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
char *
ctermid (
    char *Buffer
    );

/*++

Routine Description:

    This routine returns the null-terminated path of the controlling terminal
    for the current process. Access to the terminal path returned by this
    function is not guaranteed.

Arguments:

    Buffer - Supplies an optional pointer to a buffer of at least length
        L_ctermid where the path to the terminal will be returned. If this is
        NULL, static storage will be used and returned, in which case the
        caller should not modify or free the buffer.

Return Value:

    Returns a pointer to the string containing the path of the controlling
    terminal on success.

    NULL on failure.

--*/

LIBC_API
char *
ctermid_r (
    char *Buffer
    );

/*++

Routine Description:

    This routine returns the null-terminated path of the controlling terminal
    for the current process.

Arguments:

    Buffer - Supplies a pointer to a buffer of at least length L_ctermid where
        the path to the terminal will be returned.

Return Value:

    Returns a pointer to the supplied buffer on success.

    NULL on failure.

--*/

LIBC_API
int
rename (
    const char *SourcePath,
    const char *DestinationPath
    );

/*++

Routine Description:

    This routine attempts to rename the object at the given path. This routine
    operates on symbolic links themselves, not the destinations of symbolic
    links. If the source and destination paths are equal, this routine will do
    nothing and return successfully. If the source path is not a directory, the
    destination path must not be a directory. If the destination file exists,
    it will be deleted. The caller must have write access in both the old and
    new directories. If the source path is a directory, the destination path
    must not exist or be an empty directory. The destination path must not have
    a path prefix of the source (ie it's illegal to move /my/path into
    /my/path/stuff).

Arguments:

    SourcePath - Supplies a pointer to a null terminated string containing the
        name of the file or directory to rename.

    DestinationPath - Supplies a pointer to a null terminated string
        containing the path to rename the file or directory to. This path
        cannot span file systems.

Return Value:

    0 on success.

    -1 on failure, and the errno variable will be set to contain more
    information.

--*/

LIBC_API
int
renameat (
    int SourceDirectory,
    const char *SourcePath,
    int DestinationDirectory,
    const char *DestinationPath
    );

/*++

Routine Description:

    This routine operates the same as the rename function, except it allows
    relative source and/or destination paths to begin from a directory
    specified by the given file descriptors.

Arguments:

    SourceDirectory - Supplies a file descriptor to the directory to start
        source path searches from. If the source path is absolute, this value
        is ignored. If this is AT_FDCWD, then source path searches will
        start from the current working directory.

    SourcePath - Supplies a pointer to a null terminated string containing the
        name of the file or directory to rename.

    DestinationPath - Supplies a pointer to a null terminated string
        containing the path to rename the file or directory to. This path
        cannot span file systems.

    DestinationDirectory - Supplies an optional file descriptor to the
        directory to start destination path searches from. If the destination
        path is absolute, this value is ignored. If this is AT_FDCWD, then
        destination path searches will start from the current working directory.

Return Value:

    0 on success.

    -1 on failure, and the errno variable will be set to contain more
    information.

--*/

LIBC_API
int
remove (
    const char *Path
    );

/*++

Routine Description:

    This routine attempts to delete the object at the given path. If the
    object pointed to by the given path is a directory, the behavior of remove
    is identical to rmdir. Otherwise, the behavior of remove is identical to
    unlink.

Arguments:

    Path - Supplies a pointer to a null terminated string containing the path
        of the entry to remove.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to provide more details. In failure
    cases, the directory will not be removed.

--*/

LIBC_API
FILE *
fopen (
    const char *FileName,
    const char *Mode
    );

/*++

Routine Description:

    This routine opens the given file and associates a stream with it.

Arguments:

    FileName - Supplies a pointer to a string containing the path to the file
        to open.

    Mode - Supplies a pointer to a null terminated string specifying the mode
        to open the file with. Valid value are "r", "w", "a", and may optionally
        have a + or b appended to it. The + symbol opens the file for updating,
        meaning that the system does not flush immediately after writes. The "b"
        option has no effect.

Return Value:

    Returns a pointer to the file stream on success.

    NULL on failure, and errno contains more information.

--*/

LIBC_API
FILE *
fdopen (
    int OpenFileDescriptor,
    const char *Mode
    );

/*++

Routine Description:

    This routine associates a stream with the given file descriptor. The mode
    argument must agree with the flags the original descriptor was opened with.
    On success, the stream now "owns" the file descriptor, a call to fclose
    on the stream will also call close on the underlying descriptor.

Arguments:

    OpenFileDescriptor - Supplies the open file descriptor to create a stream
        around.

    Mode - Supplies a pointer to a null terminated string specifying the mode
        to open the file with. Valid value are "r", "w", "a", and may optionally
        have a + or b appended to it. The + symbol opens the file for updating,
        meaning that the system does not flush immediately after writes. The "b"
        option has no effect.

Return Value:

    Returns a pointer to the file stream on success.

    NULL on failure, and errno contains more information.

--*/

LIBC_API
FILE *
freopen (
    const char *FileName,
    const char *Mode,
    FILE *Stream
    );

/*++

Routine Description:

    This routine attempts to flush the given stream and close any file
    descriptor associated with the stream. Failure to flush or close the
    file descriptor is ignored. The error and end-of-file indicators are
    cleared. This routine then attempts to open the given file with the given
    mode and associate it with the stream. The previous file descriptor
    associated with this stream is closed whether or not the new descriptor
    could be opened.

    The standard says that passing in NULL for the file name will change the
    permissions of the existing descriptor. This implementation currently does
    not support that and sets errno to EBADF if attempted.

Arguments:

    FileName - Supplies a pointer to the path to open and associate with this
        stream.

    Mode - Supplies a pointer to the string describing the desired access to
        this path. This takes the same format as the fopen mode string.

    Stream - Supplies a pointer to the open stream.

Return Value:

    Returns a pointer to the given stream on success, now with a different
    file descriptor.

    NULL on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
fclose (
    FILE *Stream
    );

/*++

Routine Description:

    This routine closes an open file stream.

Arguments:

    Stream - Supplies a pointer to the open stream.

Return Value:

    0 on success.

    Returns EOF if there was an error flushing or closing the stream.

--*/

LIBC_API
size_t
fread (
    void *Buffer,
    size_t Size,
    size_t ItemCount,
    FILE *Stream
    );

/*++

Routine Description:

    This routine reads from a file stream.

Arguments:

    Buffer - Supplies a pointer to the buffer where the data will be returned.

    Size - Supplies the size of each element to read.

    ItemCount - Supplies the number of elements to read.

    Stream - Supplies a pointer to the file stream object to read from.

Return Value:

    Returns the number of elements successfully read from the file. On failure,
    the error indicator for the stream will be set, and errno will set to
    provide details on the error that occurred.

--*/

LIBC_API
size_t
fread_unlocked (
    void *Buffer,
    size_t Size,
    size_t ItemCount,
    FILE *Stream
    );

/*++

Routine Description:

    This routine reads from a file stream without acquiring the internal file
    lock.

Arguments:

    Buffer - Supplies a pointer to the buffer where the data will be returned.

    Size - Supplies the size of each element to read.

    ItemCount - Supplies the number of elements to read.

    Stream - Supplies a pointer to the file stream object to read from.

Return Value:

    Returns the number of elements successfully read from the file. On failure,
    the error indicator for the stream will be set, and errno will set to
    provide details on the error that occurred.

--*/

LIBC_API
size_t
fwrite (
    const void *Buffer,
    size_t Size,
    size_t ItemCount,
    FILE *Stream
    );

/*++

Routine Description:

    This routine writes to a file stream.

Arguments:

    Buffer - Supplies a pointer to the buffer containing the data to write.

    Size - Supplies the size of each element to write.

    ItemCount - Supplies the number of elements to write.

    Stream - Supplies a pointer to the file stream object to write to.

Return Value:

    Returns the number of elements successfully written to the file. On failure,
    the error indicator for the stream will be set, and errno will set to
    provide details on the error that occurred.

--*/

LIBC_API
size_t
fwrite_unlocked (
    const void *Buffer,
    size_t Size,
    size_t ItemCount,
    FILE *Stream
    );

/*++

Routine Description:

    This routine writes to a file stream without acquiring the internal file
    lock.

Arguments:

    Buffer - Supplies a pointer to the buffer containing the data to write.

    Size - Supplies the size of each element to write.

    ItemCount - Supplies the number of elements to write.

    Stream - Supplies a pointer to the file stream object to write to.

Return Value:

    Returns the number of elements successfully written to the file. On failure,
    the error indicator for the stream will be set, and errno will set to
    provide details on the error that occurred.

--*/

LIBC_API
int
fflush (
    FILE *Stream
    );

/*++

Routine Description:

    This routine flushes any data sitting in the file stream that has not yet
    made it out to the operating system. This is only relevant for output
    streams.

Arguments:

    Stream - Supplies a pointer to the open file stream to flush.

Return Value:

    0 on success.

    EOF on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
fflush_unlocked (
    FILE *Stream
    );

/*++

Routine Description:

    This routine flushes any data sitting in the file stream that has not yet
    made it out to the operating system. This routine does not acquire the
    internal stream lock. This is only relevant for output streams.

Arguments:

    Stream - Supplies a pointer to the open file stream to flush.

Return Value:

    0 on success.

    EOF on failure, and errno will be set to contain more information.

--*/

LIBC_API
long
ftell (
    FILE *Stream
    );

/*++

Routine Description:

    This routine returns the given stream's file position.

Arguments:

    Stream - Supplies a pointer to the open file stream.

Return Value:

    Returns the current file position on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
off_t
ftello (
    FILE *Stream
    );

/*++

Routine Description:

    This routine returns the given stream's file position.

Arguments:

    Stream - Supplies a pointer to the open file stream.

Return Value:

    Returns the current file position on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
off64_t
ftello64 (
    FILE *Stream
    );

/*++

Routine Description:

    This routine returns the given stream's file position.

Arguments:

    Stream - Supplies a pointer to the open file stream.

Return Value:

    Returns the current file position on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
off_t
ftello_unlocked (
    FILE *Stream
    );

/*++

Routine Description:

    This routine returns the given stream's file position.

Arguments:

    Stream - Supplies a pointer to the open file stream.

Return Value:

    Returns the current file position on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
fseek (
    FILE *Stream,
    long Offset,
    int Whence
    );

/*++

Routine Description:

    This routine sets the file position indicator for the given stream. If a
    read or write error occurs, the error indicator will be set for the stream
    and fseek fails. This routine will undo any effects of a previous call to
    unget.

Arguments:

    Stream - Supplies a pointer to the open file stream.

    Offset - Supplies the offset from the reference point given in the Whence
        argument.

    Whence - Supplies the reference location to base the offset off of. Valid
        value are:

        SEEK_SET - The offset will be added to the the beginning of the file.

        SEEK_CUR - The offset will be added to the current file position.

        SEEK_END - The offset will be added to the end of the file.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
fseeko (
    FILE *Stream,
    off_t Offset,
    int Whence
    );

/*++

Routine Description:

    This routine sets the file position indicator for the given stream. If a
    read or write error occurs, the error indicator will be set for the stream
    and fseek fails. This routine will undo any effects of a previous call to
    unget.

Arguments:

    Stream - Supplies a pointer to the open file stream.

    Offset - Supplies the offset from the reference point given in the Whence
        argument.

    Whence - Supplies the reference location to base the offset off of. Valid
        value are:

        SEEK_SET - The offset will be added to the the beginning of the file.

        SEEK_CUR - The offset will be added to the current file position.

        SEEK_END - The offset will be added to the end of the file.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
fseeko64 (
    FILE *Stream,
    off64_t Offset,
    int Whence
    );

/*++

Routine Description:

    This routine sets the file position indicator for the given stream. If a
    read or write error occurs, the error indicator will be set for the stream
    and fseek fails. This routine will undo any effects of a previous call to
    unget.

Arguments:

    Stream - Supplies a pointer to the open file stream.

    Offset - Supplies the offset from the reference point given in the Whence
        argument.

    Whence - Supplies the reference location to base the offset off of. Valid
        value are:

        SEEK_SET - The offset will be added to the the beginning of the file.

        SEEK_CUR - The offset will be added to the current file position.

        SEEK_END - The offset will be added to the end of the file.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
fseeko_unlocked (
    FILE *Stream,
    off_t Offset,
    int Whence
    );

/*++

Routine Description:

    This routine sets the file position indicator for the given stream. If a
    read or write error occurs, the error indicator will be set for the stream
    and fseek fails. This routine does not acquire the internal stream lock.

Arguments:

    Stream - Supplies a pointer to the open file stream.

    Offset - Supplies the offset from the reference point given in the Whence
        argument.

    Whence - Supplies the reference location to base the offset off of. Valid
        value are:

        SEEK_SET - The offset will be added to the the beginning of the file.

        SEEK_CUR - The offset will be added to the current file position.

        SEEK_END - The offset will be added to the end of the file.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
fgetpos (
    FILE *Stream,
    fpos_t *Position
    );

/*++

Routine Description:

    This routine returns an opaque structure representing the current absolute
    position within the given stream.

Arguments:

    Stream - Supplies a pointer to the open file stream.

    Position - Supplies a pointer where the opaque position will be returned.
        Callers must not presume that they can cast this type to an integer or
        compare these types in any way, they only serve as possible inputs to
        fsetpos to restore a file position to its current location.

Return Value:

    0 on success.

    -1 on failure, and errno will contain more information.

--*/

LIBC_API
int
fsetpos (
    FILE *Stream,
    const fpos_t *Position
    );

/*++

Routine Description:

    This routine sets the current file position.

Arguments:

    Stream - Supplies a pointer to the open file stream.

    Position - Supplies a pointer where the opaque position that was returned
        by a previous call to fgetpos.

Return Value:

    0 on success.

    -1 on failure, and errno will contain more information.

--*/

LIBC_API
void
rewind (
    FILE *Stream
    );

/*++

Routine Description:

    This routine positions the file indicator back to the beginning. It shall
    be equivalent to fseek(Stream, 0, SEEK_SET) except that it also clears
    the error indicator.

Arguments:

    Stream - Supplies a pointer to the open file stream.

Return Value:

    None. Applications wishing to detect an error occurring during this
    function should set errno 0, call the function, and then check errno.

--*/

LIBC_API
int
fileno (
    FILE *Stream
    );

/*++

Routine Description:

    This routine returns the integer file descriptor associated with the given
    stream.

Arguments:

    Stream - Supplies a pointer to the open file stream.

Return Value:

    Returns the integer value of the file descriptor associated with the given
    stream on success.

    -1 on failure, and errno will contain more information.

--*/

LIBC_API
int
fgetc (
    FILE *Stream
    );

/*++

Routine Description:

    This routine reads one byte from the given file stream.

Arguments:

    Stream - Supplies a pointer to the open file stream.

Return Value:

    Returns the byte on success.

    EOF on failure or the end of the file, and errno will contain more
    information.

--*/

LIBC_API
int
fgetc_unlocked (
    FILE *Stream
    );

/*++

Routine Description:

    This routine reads one byte from the given file stream without acquiring
    the internal stream lock.

Arguments:

    Stream - Supplies a pointer to the open file stream.

Return Value:

    Returns the byte on success.

    EOF on failure or the end of the file, and errno will contain more
    information.

--*/

LIBC_API
int
getchar (
    void
    );

/*++

Routine Description:

    This routine reads one byte from stdin.

Arguments:

    None.

Return Value:

    Returns the byte from stdin on success.

    -1 on failure, and errno will contain more information.

--*/

LIBC_API
int
getchar_unlocked (
    void
    );

/*++

Routine Description:

    This routine reads one byte from stdin without acquiring the file lock.

Arguments:

    None.

Return Value:

    Returns the byte from stdin on success.

    -1 on failure, and errno will contain more information.

--*/

LIBC_API
int
getc (
    FILE *Stream
    );

/*++

Routine Description:

    This routine reads one byte from the given file stream. It is equivalent to
    the fgetc function.

Arguments:

    Stream - Supplies a pointer to the open file stream.

Return Value:

    Returns the byte on success.

    EOF on failure or the end of the file, and errno will contain more
    information.

--*/

LIBC_API
int
getc_unlocked (
    FILE *Stream
    );

/*++

Routine Description:

    This routine reads one byte from the given file stream, without acquiring
    the internal file lock. It is equivalent to the fgetc_unlocked function.

Arguments:

    Stream - Supplies a pointer to the open file stream.

Return Value:

    Returns the byte on success.

    EOF on failure or the end of the file, and errno will contain more
    information.

--*/

LIBC_API
char *
gets (
    char *Line
    );

/*++

Routine Description:

    This routine gets a line of input from standard in, writing bytes to the
    supplied line until a newline or end of file is reached. The newline (if
    found) will be discarded and the string null terminated.

    Use of this function is highly discouraged, as there is no bound on the
    size of text the user can put on one line. Any use of this function is a
    security hole. Use fgets instead.

Arguments:

    Line - Supplies a pointer where the line will be returned on success.

Return Value:

    Returns a pointer to the supplied line buffer on success.

    NULL if EOF was encountered.

--*/

LIBC_API
char *
fgets (
    char *Buffer,
    int BufferSize,
    FILE *Stream
    );

/*++

Routine Description:

    This routine reads bytes from the given stream and places them in the
    given buffer until the buffer fills up, a newline is read and transferred,
    or the end of the file is reached. The string is then terminated with a
    null byte.

Arguments:

    Buffer - Supplies a pointer to the buffer where the read characters should
        be returned.

    BufferSize - Supplies the size of the supplied buffer in bytes.

    Stream - Supplies a pointer to the file stream to read out of.

Return Value:

    Returns the given buffer on success. If the stream is at end-of-file, the
    end-of-file indicator shall be set and this routine returns NULL. If a
    read error occurs, the error indicator for the stream shall be set, and
    this routine returns NULL. The errno variable may also be set to contain
    more information.

--*/

LIBC_API
char *
fgets_unlocked (
    char *Buffer,
    int BufferSize,
    FILE *Stream
    );

/*++

Routine Description:

    This routine reads bytes from the given stream and places them in the
    given buffer until the buffer fills up, a newline is read and transferred,
    or the end of the file is reached. The string is then terminated with a
    null byte.

Arguments:

    Buffer - Supplies a pointer to the buffer where the read characters should
        be returned.

    BufferSize - Supplies the size of the supplied buffer in bytes.

    Stream - Supplies a pointer to the file stream to read out of.

Return Value:

    Returns the given buffer on success. If the stream is at end-of-file, the
    end-of-file indicator shall be set and this routine returns NULL. If a
    read error occurs, the error indicator for the stream shall be set, and
    this routine returns NULL. The errno variable may also be set to contain
    more information.

--*/

LIBC_API
int
fputc (
    int Character,
    FILE *Stream
    );

/*++

Routine Description:

    This routine writes a byte to the given file stream.

Arguments:

    Character - Supplies the character (converted to an unsigned char) to
        write.

    Stream - Supplies the stream to write the character to.

Return Value:

    Returns the byte it has written on success.

    EOF on failure, and errno will contain more information.

--*/

LIBC_API
int
fputc_unlocked (
    int Character,
    FILE *Stream
    );

/*++

Routine Description:

    This routine writes a byte to the given file stream without acquiring the
    internal stream lock.

Arguments:

    Character - Supplies the character (converted to an unsigned char) to
        write.

    Stream - Supplies the stream to write the character to.

Return Value:

    Returns the byte it has written on success.

    EOF on failure, and errno will contain more information.

--*/

LIBC_API
int
putc (
    int Character,
    FILE *Stream
    );

/*++

Routine Description:

    This routine writes a byte to the given file stream. It is equivalent to
    the fputc function.

Arguments:

    Character - Supplies the character (converted to an unsigned char) to
        write.

    Stream - Supplies the stream to write the character to.

Return Value:

    Returns the byte it has written on success.

    EOF on failure, and errno will contain more information.

--*/

LIBC_API
int
putc_unlocked (
    int Character,
    FILE *Stream
    );

/*++

Routine Description:

    This routine writes a byte to the given file stream without acquiring the
    stream lock. It is equivalent to the fputc_unlocked function.

Arguments:

    Character - Supplies the character (converted to an unsigned char) to
        write.

    Stream - Supplies the stream to write the character to.

Return Value:

    Returns the byte it has written on success.

    EOF on failure, and errno will contain more information.

--*/

LIBC_API
int
putchar (
    int Character
    );

/*++

Routine Description:

    This routine writes a byte to standard out. This routine is equivalent to
    fputc(Character, stdout).

Arguments:

    Character - Supplies the character (converted to an unsigned char) to
        write.

Return Value:

    Returns the byte it has written on success.

    EOF on failure, and errno will contain more information.

--*/

LIBC_API
int
putchar_unlocked (
    int Character
    );

/*++

Routine Description:

    This routine writes a byte to standard out, without acquiring the stream
    lock. This routine is equivalent to fputc_unlocked(Character, stdout).

Arguments:

    Character - Supplies the character (converted to an unsigned char) to
        write.

Return Value:

    Returns the byte it has written on success.

    EOF on failure, and errno will contain more information.

--*/

LIBC_API
int
puts (
    const char *String
    );

/*++

Routine Description:

    This routine writes the given string to standard out. The null terminating
    byte is not written.

Arguments:

    String - Supplies a pointer to the null terminated string to write to.

    Stream - Supplies the stream to write the character to.

Return Value:

    Returns a non-negative number on success.

    Returns EOF on failure, and the error indicator will be set for the stream.
    The errno variable will also be set to provide more information.

--*/

LIBC_API
int
fputs (
    const char *String,
    FILE *Stream
    );

/*++

Routine Description:

    This routine writes the given string to the given file stream. The null
    terminating byte is not written.

Arguments:

    String - Supplies a pointer to the null terminated string to write to.

    Stream - Supplies the stream to write the character to.

Return Value:

    Returns a non-negative number on success.

    Returns EOF on failure, and the error indicator will be set for the stream.
    The errno variable will also be set to provide more information.

--*/

LIBC_API
int
fputs_unlocked (
    const char *String,
    FILE *Stream
    );

/*++

Routine Description:

    This routine writes the given string to the given file stream. The null
    terminating byte is not written. This routine does not acquire the stream
    lock.

Arguments:

    String - Supplies a pointer to the null terminated string to write to.

    Stream - Supplies the stream to write the character to.

Return Value:

    Returns a non-negative number on success.

    Returns EOF on failure, and the error indicator will be set for the stream.
    The errno variable will also be set to provide more information.

--*/

LIBC_API
int
ungetc (
    int Character,
    FILE *Stream
    );

/*++

Routine Description:

    This routine pushes the specified character back onto the input stream. The
    pushed back character shall be returned by subsequent reads on that stream
    in the reverse order of their pushing. A successful intervening call to
    seek or flush will discard any pushed back bytes for the stream. One byte
    of push back is provided.

Arguments:

    Character - Supplies the character (converted to an unsigned char) to
        push back.

    Stream - Supplies the stream to push the character on to.

Return Value:

    Returns the byte pushed back on success.

    EOF on failure, and errno will contain more information.

--*/

LIBC_API
int
ungetc_unlocked (
    int Character,
    FILE *Stream
    );

/*++

Routine Description:

    This routine pushes the specified character back onto the input stream. The
    pushed back character shall be returned by subsequent reads on that stream
    in the reverse order of their pushing. A successful intervening call to
    seek or flush will discard any pushed back bytes for the stream. One byte
    of push back is provided. This routine does not acquire the internal
    stream lock.

Arguments:

    Character - Supplies the character (converted to an unsigned char) to
        push back.

    Stream - Supplies the stream to push the character on to.

Return Value:

    Returns the byte pushed back on success.

    EOF on failure, and errno will contain more information.

--*/

LIBC_API
int
setvbuf (
    FILE *Stream,
    char *Buffer,
    int Mode,
    size_t BufferSize
    );

/*++

Routine Description:

    This routine sets the buffering mode and buffer (optionally) for the given
    file stream.

Arguments:

    Stream - Supplies a pointer to the file stream whose buffering
        characteristics should be altered.

    Buffer - Supplies an optional pointer to the buffer to use for fully
        buffered or line buffered mode. If either of those two modes are
        supplied and this buffer is not supplied, one will be malloced.

    Mode - Supplies the buffering mode to set for this file stream. Valid
        value are:

        _IONBF - Disable buffering on this file stream. All reads and writes
            will go straight to the low level I/O interface.

        _IOFBF - Fully buffered mode. The stream interface will batch reads
            and writes to the low level I/O interface to optimize performance.

        _IOLBF - Line buffered mode. This mode behaves the same as fully
            buffered mode, except that the buffer is flushed automatically when
            a newline character is written to the stream. For input buffers,
            the behavior is no different from fully buffered mode.

    BufferSize - Supplies the size of the supplied buffer in bytes.

Return Value:

    0 on success.

    -1 on failure, and errno will contain more information.

--*/

LIBC_API
void
setbuf (
    FILE *Stream,
    char *Buffer
    );

/*++

Routine Description:

    This routine sets the internal buffer on a stream. If the given buffer is
    not NULL, this routine shall be equivalent to:

    setvbuf(Stream, Buffer, _IOFBF, BUFSIZ);

    or,

    setvbuf(Stream, Buffer, _IONBF, BUFSIZ);

    if the given buffer is a NULL pointer.

Arguments:

    Stream - Supplies a pointer to the file stream.

    Buffer - Supplies the optional buffer to use.

Return Value:

    None.

--*/

LIBC_API
void
clearerr (
    FILE *Stream
    );

/*++

Routine Description:

    This routine clears the error and end-of-file indicators for the given
    stream.

Arguments:

    Stream - Supplies a pointer to the file stream.

Return Value:

    None.

--*/

LIBC_API
void
clearerr_unlocked (
    FILE *Stream
    );

/*++

Routine Description:

    This routine clears the error and end-of-file indicators for the given
    stream. This routine does not acquire the file lock.

Arguments:

    Stream - Supplies a pointer to the file stream.

Return Value:

    None.

--*/

LIBC_API
int
feof (
    FILE *Stream
    );

/*++

Routine Description:

    This routine returns whether or not the current stream has attempted to
    read beyond the end of the file.

Arguments:

    Stream - Supplies a pointer to the file stream.

Return Value:

    Returns non-zero if the end of file indicator is set for the given stream.

--*/

LIBC_API
int
feof_unlocked (
    FILE *Stream
    );

/*++

Routine Description:

    This routine returns whether or not the current stream has attempted to
    read beyond the end of the file, without acquiring the file lock.

Arguments:

    Stream - Supplies a pointer to the file stream.

Return Value:

    Returns non-zero if the end of file indicator is set for the given stream.

--*/

LIBC_API
int
ferror (
    FILE *Stream
    );

/*++

Routine Description:

    This routine returns whether or not the error indicator is set for the
    current stream.

Arguments:

    Stream - Supplies a pointer to the file stream.

Return Value:

    Returns non-zero if the error indicator is set for the given stream.

--*/

LIBC_API
int
ferror_unlocked (
    FILE *Stream
    );

/*++

Routine Description:

    This routine returns whether or not the error indicator is set for the
    current stream, without acquiring the file lock.

Arguments:

    Stream - Supplies a pointer to the file stream.

Return Value:

    Returns non-zero if the error indicator is set for the given stream.

--*/

LIBC_API
void
flockfile (
    FILE *Stream
    );

/*++

Routine Description:

    This routine explicitly locks a file stream. It can be used on the standard
    I/O streams to group a batch of I/O together.

Arguments:

    Stream - Supplies a pointer to the file stream.

Return Value:

    None.

--*/

LIBC_API
int
ftrylockfile (
    FILE *Stream
    );

/*++

Routine Description:

    This routine attempts to acquire the lock for a given stream.

Arguments:

    Stream - Supplies a pointer to the file stream.

Return Value:

    0 if the lock was successfully acquired.

    Non-zero if the file lock could not be acquired.

--*/

LIBC_API
void
funlockfile (
    FILE *Stream
    );

/*++

Routine Description:

    This routine explicitly unlocks a file stream that had been previously
    locked with flockfile or ftrylockfile (on a successful attempt).

Arguments:

    Stream - Supplies a pointer to the file stream.

Return Value:

    None.

--*/

LIBC_API
FILE *
popen (
    const char *Command,
    const char *Mode
    );

/*++

Routine Description:

    This routine executes the command specified by the given string. It shall
    create a pipe between the calling program and the executed command, and
    shall return a pointer to a stream that can be used to either read from or
    write to the pipe. Streams returned by this function should be closed with
    the pclose function.

Arguments:

    Command - Supplies a pointer to a null terminated string containing the
        command to execute.

    Mode - Supplies a pointer to a null terminated string containing the mode
        information of the returned string. If the first character of the
        given string is 'r', then the file stream returned can be read to
        retrieve the standard output of the executed process. Otherwise, the
        file stream returned can be written to to send data to the standard
        input of the executed process.

Return Value:

    Returns a pointer to a stream wired up to the standard in or standard out
    of the executed process.

    NULL on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
pclose (
    FILE *Stream
    );

/*++

Routine Description:

    This routine closes a stream opened by the popen function, wait for the
    command to terminate, and return the termination status of the process
    that was running the command language interpreter.

Arguments:

    Stream - Supplies a pointer to the stream opened with popen.

Return Value:

    Returns the execution status of the opened process.

    127 if the command language intepreter cannot be executed.

    -1 if an intervening call to wait or waitpid caused the termination status
    to be unavailable. In this case, errno will be set to ECHLD.

--*/

LIBC_API
char *
tmpnam (
    char *Buffer
    );

/*++

Routine Description:

    This routine generates a string that is a valid filename and is not the
    name of an existing file. This routine returns a different name each time
    it is called. Note that between the time the name is returned and when an
    application goes to create the file, the file may already be created.
    Applications may find the tmpfile function more robust and useful.

Arguments:

    Buffer - Supplies an optional pointer to a buffer where the name will be
        returned. This buffer is assumed to be at least L_tmpnam bytes large.
        If this buffer is not supplied, then a pointer to a global buffer will
        be returned. Subsequent calls to this routine will overwrite the
        contents of that returned buffer.

Return Value:

    Returns a pointer to a string containing the name of a temporary file. This
    returns the buffer if it is supplied, or a pointer to a global buffer
    otherwise.

--*/

LIBC_API
char *
tempnam (
    const char *Directory,
    const char *Prefix
    );

/*++

Routine Description:

    This routine generates path name that may be used for a temporary file.

Arguments:

    Directory - Supplies an optional pointer to a string containing the name of
        the directory in which the temporary file is to be created. If the
        directory is not supplied or is not an appropriate directory, then the
        path prefix defined as P_tmpdir in stdio.h shall be used.

    Prefix - Supplies a pointer to a string containing up to a five character
        prefix on the temporary file name.

Return Value:

    Returns a pointer to a string containing the name of a temporary file. The
    caller must call free when finished with this buffer to reclaim the memory
    allocated by this routine.

    NULL on failure, and errno will be set to contain more information.

--*/

LIBC_API
FILE *
tmpfile (
    void
    );

/*++

Routine Description:

    This routine creates a file and opens a corresponding stream. The file
    shall be automatically deleted when all references to the file are closed.
    The file is opened as in fopen for update ("w+").

Arguments:

    None.

Return Value:

    Returns an open file stream on success.

    NULL if a temporary file could not be created.

--*/

LIBC_API
void
perror (
    const char *String
    );

/*++

Routine Description:

    This routine maps the error number accessed through the symbol errno to
    a language-dependent error message, and prints that out to standard error
    with the given parameter.

Arguments:

    String - Supplies an optional pointer to a string to print before the
        error message. If this is supplied and of non-zero length, the format
        will be "<string>: <errno string>\n". Otherwise, the format will be
        "<errno string>\n".

Return Value:

    None.

--*/

LIBC_API
int
printf (
    const char *Format,
    ...
    );

/*++

Routine Description:

    This routine prints a formatted string to the standard output file stream.

Arguments:

    Format - Supplies the printf format string.

    ... - Supplies a variable number of arguments, as required by the printf
        format string argument.

Return Value:

    Returns the number of bytes successfully converted, not including the null
    terminator.

    Returns a negative number if an error was encountered.

--*/

LIBC_API
int
fprintf (
    FILE *Stream,
    const char *Format,
    ...
    );

/*++

Routine Description:

    This routine prints a formatted string to the given file stream.

Arguments:

    Stream - Supplies the file stream to print to.

    Format - Supplies the printf format string.

    ... - Supplies a variable number of arguments, as required by the printf
        format string argument.

Return Value:

    Returns the number of bytes successfully converted, not including the null
    terminator.

    Returns a negative number if an error was encountered.

--*/

LIBC_API
int
fprintf_unlocked (
    FILE *Stream,
    const char *Format,
    ...
    );

/*++

Routine Description:

    This routine prints a formatted string to the given file stream. This
    routine does not acquire the stream lock.

Arguments:

    Stream - Supplies the file stream to print to.

    Format - Supplies the printf format string.

    ... - Supplies a variable number of arguments, as required by the printf
        format string argument.

Return Value:

    Returns the number of bytes successfully converted, not including the null
    terminator.

    Returns a negative number if an error was encountered.

--*/

LIBC_API
int
vfprintf (
    FILE *File,
    const char *Format,
    va_list Arguments
    );

/*++

Routine Description:

    This routine prints a formatted string to the given file pointer.

Arguments:

    File - Supplies a pointer to the file stream to output to.

    Format - Supplies the printf format string.

    Arguments - Supplies the argument list to the format string. The va_end
        macro is not invoked on this list.

Return Value:

    Returns the number of bytes successfully converted. A null terminator is
    not written.

    Returns a negative number if an error was encountered.

--*/

LIBC_API
int
vfprintf_unlocked (
    FILE *File,
    const char *Format,
    va_list Arguments
    );

/*++

Routine Description:

    This routine prints a formatted string to the given file pointer. This
    routine does not acquire the stream lock.

Arguments:

    File - Supplies a pointer to the file stream to output to.

    Format - Supplies the printf format string.

    Arguments - Supplies the argument list to the format string. The va_end
        macro is not invoked on this list.

Return Value:

    Returns the number of bytes successfully converted. A null terminator is
    not written.

    Returns a negative number if an error was encountered.

--*/

LIBC_API
int
vprintf (
    const char *Format,
    va_list Arguments
    );

/*++

Routine Description:

    This routine prints a formatted string to the standard output file stream.

Arguments:

    Format - Supplies the printf format string.

    Arguments - Supplies the argument list to the format string. The va_end
        macro is not invoked on this list.

Return Value:

    Returns the number of bytes successfully converted, not including the null
    terminator.

    Returns a negative number if an error was encountered.

--*/

LIBC_API
int
dprintf (
    int FileDescriptor,
    const char *Format,
    ...
    );

/*++

Routine Description:

    This routine prints a formatted string to the given file descriptor.

Arguments:

    FileDescriptor - Supplies the file descriptor to print to.

    Format - Supplies the printf format string.

    ... - Supplies a variable number of arguments, as required by the printf
        format string argument.

Return Value:

    Returns the number of bytes successfully converted, not including the null
    terminator.

    Returns a negative number if an error was encountered.

--*/

LIBC_API
int
vdprintf (
    int FileDescriptor,
    const char *Format,
    va_list Arguments
    );

/*++

Routine Description:

    This routine prints a formatted string to the given file descriptor.

Arguments:

    FileDescriptor - Supplies the file descriptor to print to.

    Format - Supplies the printf format string.

    Arguments - Supplies the argument list to the format string. The va_end
        macro is not invoked on this list.

Return Value:

    Returns the number of bytes successfully converted. A null terminator is
    not written.

    Returns a negative number if an error was encountered.

--*/

LIBC_API
int
sprintf (
    char *OutputString,
    const char *Format,
    ...
    );

/*++

Routine Description:

    This routine prints a formatted string to the given buffer. This routine
    should be avoided if possible as it can be the cause of buffer overflow
    issues. Use snprintf instead, a function that explicitly bounds the output
    buffer.

Arguments:

    OutputString - Supplies the buffer where the formatted string will be
        returned. It is the caller's responsibility to ensure this buffer is
        large enough to hold the formatted string.

    Format - Supplies the printf format string.

    ... - Supplies a variable number of arguments, as required by the printf
        format string argument.

Return Value:

    Returns the number of bytes successfully converted, not including the null
    terminator.

    Returns a negative number if an error was encountered.

--*/

LIBC_API
int
snprintf (
    char *OutputString,
    size_t OutputStringSize,
    const char *Format,
    ...
    );

/*++

Routine Description:

    This routine prints a formatted string to the given bounded buffer.

Arguments:

    OutputString - Supplies the buffer where the formatted string will be
        returned.

    OutputStringSize - Supplies the number of bytes in the output buffer.

    Format - Supplies the printf format string.

    ... - Supplies a variable number of arguments, as required by the printf
        format string argument.

Return Value:

    Returns the number of bytes that would have been converted had
    OutputStringSize been large enough, not including the null terminator.

    Returns a negative number if an error was encountered.

--*/

LIBC_API
int
vsnprintf (
    char *OutputString,
    size_t OutputStringSize,
    const char *Format,
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

    Returns the number of bytes that would have been converted had
    OutputStringSize been large enough, not including the null terminator.

    Returns a negative number if an error was encountered.

--*/

LIBC_API
int
vsprintf (
    char *OutputString,
    const char *Format,
    va_list Arguments
    );

/*++

Routine Description:

    This routine implements the core string print format function.

Arguments:

    OutputString - Supplies a pointer to the buffer where the resulting string
        will be written.

    Format - Supplies the printf format string.

    Arguments - Supplies the argument list to the format string. The va_end
        macro is not invoked on this list.

Return Value:

    Returns the number of bytes successfully converted, not including the null
    terminator.

    Returns a negative number if an error was encountered.

--*/

LIBC_API
int
asprintf (
    char **OutputString,
    const char *Format,
    ...
    );

/*++

Routine Description:

    This routine prints a formatting string to a string similar to vsnprintf,
    except the destination string is allocated by this function using malloc.

Arguments:

    OutputString - Supplies a pointer where a pointer to a newly allocated
        buffer containing the formatted string result (including the null
        terminator) will be returned. The caller is reponsible for freeing this
        string.

    Format - Supplies the printf format string.

    ... - Supplies the argument list to the format string.

Return Value:

    Returns the number of bytes successfully converted, not including the null
    terminator.

    Returns a negative number if an error was encountered.

--*/

LIBC_API
int
vasprintf (
    char **OutputString,
    const char *Format,
    va_list Arguments
    );

/*++

Routine Description:

    This routine prints a formatting string to a string similar to vsnprintf,
    except the destination string is allocated by this function using malloc.

Arguments:

    OutputString - Supplies a pointer where a pointer to a newly allocated
        buffer containing the formatted string result (including the null
        terminator) will be returned. The caller is reponsible for freeing this
        string.

    Format - Supplies the printf format string.

    Arguments - Supplies the argument list to the format string. The va_end
        macro is not invoked on this list.

Return Value:

    Returns the number of bytes successfully converted, not including the null
    terminator.

    Returns a negative number if an error was encountered.

--*/

LIBC_API
int
sscanf (
    const char *Input,
    const char *Format,
    ...
    );

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

LIBC_API
int
vsscanf (
    const char *String,
    const char *Format,
    va_list ArgumentList
    );

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

LIBC_API
int
fscanf (
    FILE *Stream,
    const char *Format,
    ...
    );

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

LIBC_API
int
vfscanf (
    FILE *Stream,
    const char *Format,
    va_list ArgumentList
    );

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

LIBC_API
int
vfscanf_unlocked (
    FILE *Stream,
    const char *Format,
    va_list ArgumentList
    );

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

LIBC_API
int
scanf (
    const char *Format,
    ...
    );

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

LIBC_API
int
vscanf (
    const char *Format,
    va_list ArgumentList
    );

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

LIBC_API
ssize_t
getline (
    char **LinePointer,
    size_t *Size,
    FILE *Stream
    );

/*++

Routine Description:

    This routine reads an entire line from the given stream. This routine will
    allocate or reallocate the given buffer so that the buffer is big enough.

Arguments:

    LinePointer - Supplies a pointer that on input contains an optional pointer
        to a buffer to use to read the line. If the buffer ends up being not
        big enough, it will be reallocated. If no buffer is supplied, one will
        be allocated. On output, contains a pointer to the buffer containing
        the read line on success.

    Size - Supplies a pointer that on input contains the size in bytes of the
        supplied line pointer. On output, this value will be updated to contain
        the size of the buffer returned in the line buffer parameter.

    Stream - Supplies the stream to read the line from.

Return Value:

    On success, returns the number of characters read, including the delimiter
    character, but not including the null terminator.

    Returns -1 on failure (including an end of file condition), and errno will
    be set to contain more information.

--*/

LIBC_API
ssize_t
getdelim (
    char **LinePointer,
    size_t *Size,
    int Delimiter,
    FILE *Stream
    );

/*++

Routine Description:

    This routine reads an entire line from the given stream, delimited by the
    given delimiter character. This routine will allocate or reallocate the
    given buffer so that the buffer is big enough.

Arguments:

    LinePointer - Supplies a pointer that on input contains an optional pointer
        to a buffer to use to read the line. If the buffer ends up being not
        big enough, it will be reallocated. If no buffer is supplied, one will
        be allocated. On output, contains a pointer to the buffer containing
        the read line on success.

    Size - Supplies a pointer that on input contains the size in bytes of the
        supplied line pointer. On output, this value will be updated to contain
        the size of the buffer returned in the line buffer parameter.

    Delimiter - Supplies the delimiter to split the line on.

    Stream - Supplies the stream to read the line from.

Return Value:

    On success, returns the number of characters read, including the delimiter
    character, but not including the null terminator.

    Returns -1 on failure (including an end of file condition), and errno will
    be set to contain more information.

--*/

#ifdef __cplusplus

}

#endif
#endif

