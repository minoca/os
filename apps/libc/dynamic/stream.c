/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    stream.c

Abstract:

    This module implements the higher level file stream interface.

Author:

    Evan Green 18-Jun-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of standard handles.
//

#define STANDARD_HANDLE_COUNT 3

//
// Define the creation mask for stream files.
//

#define STREAM_FILE_CREATION_MASK \
    (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

//
// Define the maximum size of the stack-allocated print buffer.
//

#define STREAM_PRINT_BUFFER_SIZE 128

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the print context used for streams. This allows
    unbuffered streams to batch prints, rather than write them out character by
    character.

Members:

    Stream - Store a pointer to the file stream to print to.

    Buffer - Stores a pointer to the buffered print output for unbuffered
        streams.

    BufferNextIndex - Stores the index into the buffer to store the next
        character.

    CharactersWritten - Stores the number of characters actually written to the
        stream.

--*/

typedef struct _STREAM_PRINT_CONTEXT {
    FILE *Stream;
    CHAR Buffer[STREAM_PRINT_BUFFER_SIZE];
    ULONG BufferNextIndex;
    ULONG CharactersWritten;
} STREAM_PRINT_CONTEXT, *PSTREAM_PRINT_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

PFILE
ClpCreateFileStructure (
    ULONG Descriptor,
    ULONG OpenFlags,
    ULONG BufferMode
    );

VOID
ClpDestroyFileStructure (
    PFILE File
    );

BOOL
ClpFileFormatWriteCharacter (
    INT Character,
    PPRINT_FORMAT_CONTEXT Context
    );

INT
ClpConvertStreamModeStringToOpenFlags (
    PSTR ModeString,
    INT *OpenFlags
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the standard file handle pointers.
//

LIBC_API FILE *stdin;
LIBC_API FILE *stdout;
LIBC_API FILE *stderr;

//
// Store the global list of streams, protected by a single global spin lock.
//

LIST_ENTRY ClStreamList;
pthread_mutex_t ClStreamListLock;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
FILE *
fopen (
    const char *FileName,
    const char *Mode
    )

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

{

    mode_t CreatePermissions;
    int Descriptor;
    int Error;
    FILE *NewFile;
    int OpenFlags;
    BOOL Success;

    CreatePermissions = STREAM_FILE_CREATION_MASK;
    Descriptor = -1;
    NewFile = NULL;
    OpenFlags = 0;
    Success = FALSE;

    //
    // Get the open flags.
    //

    Error = ClpConvertStreamModeStringToOpenFlags((PSTR)Mode, &OpenFlags);
    if (Error != 0) {
        goto fopenEnd;
    }

    //
    // Open up the file with the operating system.
    //

    Descriptor = open(FileName, OpenFlags, CreatePermissions);
    if (Descriptor == -1) {
        goto fopenEnd;
    }

    NewFile = ClpCreateFileStructure(Descriptor, OpenFlags, _IOFBF);
    if (NewFile == NULL) {
        goto fopenEnd;
    }

    Success = TRUE;

fopenEnd:
    if (Success == FALSE) {
        if (NewFile != NULL) {
            if (NewFile->Descriptor != -1) {
                close(NewFile->Descriptor);
            }

            if (NewFile->Buffer != NULL) {
                free(NewFile->Buffer);
            }

            free(NewFile);
            NewFile = NULL;
        }
    }

    if (Error != 0) {
        errno = Error;
    }

    return NewFile;
}

LIBC_API
FILE *
fdopen (
    int OpenFileDescriptor,
    const char *Mode
    )

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

{

    int Error;
    FILE *NewFile;
    int OpenFlags;

    NewFile = NULL;
    OpenFlags = 0;
    if (OpenFileDescriptor == -1) {
        Error = EBADF;
        goto fdopenEnd;
    }

    Error = ClpConvertStreamModeStringToOpenFlags((PSTR)Mode, &OpenFlags);
    if (Error != 0) {
        goto fdopenEnd;
    }

    NewFile = ClpCreateFileStructure(OpenFileDescriptor, OpenFlags, _IOFBF);
    if (NewFile == NULL) {
        Error = ENOMEM;
        goto fdopenEnd;
    }

    Error = 0;

fdopenEnd:
    if (Error != 0) {
        if (NewFile != NULL) {
            if (NewFile->Buffer != NULL) {
                free(NewFile->Buffer);
            }

            free(NewFile);
            NewFile = NULL;
        }
    }

    if (Error != 0) {
        errno = Error;
    }

    return NewFile;
}

LIBC_API
FILE *
freopen (
    const char *FileName,
    const char *Mode,
    FILE *Stream
    )

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

{

    int Error;
    int NewDescriptor;
    int OpenFlags;

    NewDescriptor = -1;
    if (Stream == NULL) {
        errno = EBADF;
        return NULL;
    }

    ClpLockStream(Stream);

    //
    // Flush and close the original descriptor.
    //

    fflush_unlocked(Stream);
    if (Stream->Descriptor != -1) {
        close(Stream->Descriptor);
        Stream->Descriptor = -1;
    }

    //
    // Get the open flags.
    //

    OpenFlags = 0;
    Error = ClpConvertStreamModeStringToOpenFlags((PSTR)Mode, &OpenFlags);
    if (Error != 0) {
        goto freopenEnd;
    }

    //
    // Consider implementing support for changing permissions on the currently
    // open file if people trying to use that.
    //

    assert(FileName != NULL);

    //
    // Open up the new descriptor.
    //

    NewDescriptor = open(FileName, OpenFlags);
    if (NewDescriptor < 0) {
        NewDescriptor = -1;
        Error = errno;
        goto freopenEnd;
    }

freopenEnd:

    //
    // Set the underlying descriptor to the new descriptor, which may or may
    // not have failed to open.
    //

    Stream->Descriptor = NewDescriptor;
    Stream->OpenFlags = OpenFlags;
    Stream->BufferNextIndex = 0;
    Stream->BufferValidSize = 0;
    Stream->Flags &= (FILE_FLAG_BUFFER_ALLOCATED | FILE_FLAG_STANDARD_IO);
    if ((OpenFlags & O_ACCMODE) != O_WRONLY) {
        Stream->Flags |= FILE_FLAG_CAN_READ;
    }

    ClpUnlockStream(Stream);
    return Stream;
}

LIBC_API
int
fclose (
    FILE *Stream
    )

/*++

Routine Description:

    This routine closes an open file stream.

Arguments:

    Stream - Supplies a pointer to the open stream.

Return Value:

    0 on success.

    Returns EOF if there was an error flushing or closing the stream.

--*/

{

    int Result;

    Result = fflush(Stream);
    if (Stream->Descriptor != -1) {
        Result |= close(Stream->Descriptor);
        Stream->Descriptor = -1;
    }

    //
    // Don't actually free the stream if it's one of the standard ones.
    // Applications have come to expect to be able to fclose stdout and then
    // freopen it.
    //

    if ((Stream->Flags & FILE_FLAG_STANDARD_IO) == 0) {
        ClpDestroyFileStructure(Stream);
    }

    return Result;
}

LIBC_API
size_t
fread (
    void *Buffer,
    size_t Size,
    size_t ItemCount,
    FILE *Stream
    )

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

{

    size_t Result;

    ClpLockStream(Stream);
    Result = fread_unlocked(Buffer, Size, ItemCount, Stream);
    ClpUnlockStream(Stream);
    return Result;
}

LIBC_API
size_t
fread_unlocked (
    void *Buffer,
    size_t Size,
    size_t ItemCount,
    FILE *Stream
    )

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

{

    size_t BytesToRead;
    ssize_t Result;
    size_t TotalBytesRead;
    size_t TotalBytesToRead;

    TotalBytesRead = 0;
    TotalBytesToRead = Size * ItemCount;
    if ((Stream->Flags & FILE_FLAG_CAN_READ) == 0) {
        Stream->Flags |= FILE_FLAG_ERROR;
        errno = EACCES;
        return 0;
    }

    if (Stream->Descriptor == -1) {
        errno = EBADF;
        return 0;
    }

    if ((Size == 0) || (ItemCount == 0)) {
        return 0;
    }

    //
    // Set the last operation to be a read.
    //

    Stream->Flags |= FILE_FLAG_READ_LAST;

    //
    // If the unget character is valid, stick that in there first.
    //

    if ((Stream->Flags & FILE_FLAG_UNGET_VALID) != 0) {
        *((PUCHAR)Buffer) = Stream->UngetCharacter;
        Stream->Flags &= ~FILE_FLAG_UNGET_VALID;
        TotalBytesRead += 1;
        if (TotalBytesRead == TotalBytesToRead) {
            return TotalBytesRead / Size;
        }
    }

    //
    // For unbuffered streams, just read the file contents directly.
    //

    if (Stream->BufferMode == _IONBF) {
        ClpFlushAllStreams(FALSE, Stream);
        while (TotalBytesRead != TotalBytesToRead) {
            do {
                Result = read(Stream->Descriptor,
                              Buffer + TotalBytesRead,
                              TotalBytesToRead - TotalBytesRead);

            } while ((Result < 0) && (errno == EINTR));

            if (Result <= 0) {
                if (Result < 0) {
                    Stream->Flags |= FILE_FLAG_ERROR;

                } else {
                    Stream->Flags |= FILE_FLAG_END_OF_FILE;
                }

                break;
            }

            TotalBytesRead += Result;
        }

        return TotalBytesRead / Size;
    }

    assert(Stream->Buffer != NULL);

    //
    // Grab as much as needed out of the buffer.
    //

    BytesToRead = Stream->BufferValidSize - Stream->BufferNextIndex;
    if (BytesToRead > (TotalBytesToRead - TotalBytesRead)) {
        BytesToRead = TotalBytesToRead - TotalBytesRead;
    }

    memcpy(Buffer + TotalBytesRead,
           Stream->Buffer + Stream->BufferNextIndex,
           BytesToRead);

    TotalBytesRead += BytesToRead;
    Stream->BufferNextIndex += BytesToRead;
    if (Stream->BufferNextIndex == Stream->BufferValidSize) {
        Stream->BufferNextIndex = 0;
        Stream->BufferValidSize = 0;
    }

    //
    // Do direct reads to the caller's buffer if they're as large as the
    // buffer itself to avoid silly copies.
    //

    if (TotalBytesToRead >= Stream->BufferSize) {
        while (TotalBytesRead != TotalBytesToRead) {
            BytesToRead = TotalBytesToRead - TotalBytesRead;
            do {
                Result = read(Stream->Descriptor,
                              Buffer + TotalBytesRead,
                              BytesToRead);

            } while ((Result == -1) && (errno == EINTR));

            if (Result <= 0) {
                if (Result < 0) {
                    Stream->Flags |= FILE_FLAG_ERROR;

                } else {
                    Stream->Flags |= FILE_FLAG_END_OF_FILE;
                }

                break;
            }

            TotalBytesRead += Result;
        }

    //
    // This is a smaller read, use the buffer.
    //

    } else {
        while (TotalBytesRead != TotalBytesToRead) {

            //
            // The buffer should have been cleared out by the first portion of
            // this function or fully satisfied by it.
            //

            assert((Stream->BufferValidSize == 0) &&
                   (Stream->BufferNextIndex == 0));

            do {
                Result = read(Stream->Descriptor,
                              Stream->Buffer,
                              Stream->BufferSize);

            } while ((Result == -1) && (errno == EINTR));

            if (Result <= 0) {
                if (Result < 0) {
                    Stream->Flags |= FILE_FLAG_ERROR;

                } else {
                    Stream->Flags |= FILE_FLAG_END_OF_FILE;
                }

                break;
            }

            BytesToRead = Result;
            if (BytesToRead > (TotalBytesToRead - TotalBytesRead)) {
                BytesToRead = TotalBytesToRead - TotalBytesRead;
                Stream->BufferValidSize = Result;
                Stream->BufferNextIndex = BytesToRead;
            }

            memcpy(Buffer + TotalBytesRead, Stream->Buffer, BytesToRead);
            TotalBytesRead += BytesToRead;
        }
    }

    return TotalBytesRead / Size;
}

LIBC_API
size_t
fwrite (
    const void *Buffer,
    size_t Size,
    size_t ItemCount,
    FILE *Stream
    )

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

{

    size_t Result;

    ClpLockStream(Stream);
    Result = fwrite_unlocked(Buffer, Size, ItemCount, Stream);
    ClpUnlockStream(Stream);
    return Result;
}

LIBC_API
size_t
fwrite_unlocked (
    const void *Buffer,
    size_t Size,
    size_t ItemCount,
    FILE *Stream
    )

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

{

    size_t BytesToWrite;
    ULONG CharacterIndex;
    BOOL Flush;
    ssize_t Result;
    PSTR String;
    size_t TotalBytesToWrite;
    size_t TotalBytesWritten;

    TotalBytesWritten = 0;
    TotalBytesToWrite = Size * ItemCount;
    if ((Stream->OpenFlags & O_WRONLY) == 0) {
        errno = EACCES;
        return 0;
    }

    if (TotalBytesToWrite == 0) {
        return 0;
    }

    if (Stream->Descriptor == -1) {
        errno = EBADF;
        return 0;
    }

    //
    // The unget character isn't valid after things have been written.
    //

    if ((Stream->Flags & FILE_FLAG_UNGET_VALID) != 0) {
        Stream->Flags &= ~FILE_FLAG_UNGET_VALID;
    }

    //
    // For unbuffered streams or large writes, just write the file contents
    // directly.
    //

    if ((Stream->BufferMode == _IONBF) ||
        (TotalBytesToWrite > Stream->BufferSize)) {

        if (fflush_unlocked(Stream) != 0) {
            return -1;
        }

        //
        // Set the last thing that happened to be a write.
        //

        Stream->Flags &= ~FILE_FLAG_READ_LAST;
        while (TotalBytesWritten != TotalBytesToWrite) {
            do {
                Result = write(Stream->Descriptor,
                               Buffer + TotalBytesWritten,
                               TotalBytesToWrite - TotalBytesWritten);

            } while ((Result < 0) && (errno == EINTR));

            if (Result <= 0) {
                Stream->Flags |= FILE_FLAG_ERROR;
                break;
            }

            TotalBytesWritten += Result;
        }

        return TotalBytesWritten / Size;
    }

    //
    // If the last thing that happened was a read, flush the buffer.
    //

    if ((Stream->Flags & FILE_FLAG_READ_LAST) != 0) {
        if (fflush_unlocked(Stream) != 0) {
            return -1;
        }

        Stream->Flags &= ~FILE_FLAG_READ_LAST;
    }

    //
    // Loop writing stuff to the buffer and flushing the buffer.
    //

    Flush = FALSE;
    while (TotalBytesWritten != TotalBytesToWrite) {
        BytesToWrite = Stream->BufferSize - Stream->BufferNextIndex;
        if (BytesToWrite > (TotalBytesToWrite - TotalBytesWritten)) {
            BytesToWrite = TotalBytesToWrite - TotalBytesWritten;
        }

        //
        // If the buffer is line buffered, look for a newline, which would
        // indicate the need to flush, and cut the copy short if one is found.
        //

        if (Stream->BufferMode == _IOLBF) {
            String = (PSTR)Buffer + TotalBytesWritten;
            for (CharacterIndex = 0;
                 CharacterIndex < BytesToWrite;
                 CharacterIndex += 1) {

                if (String[CharacterIndex] == '\n') {
                    Flush = TRUE;
                    BytesToWrite = CharacterIndex + 1;
                    break;
                }
            }
        }

        assert(Stream->BufferNextIndex + BytesToWrite <= Stream->BufferSize);

        //
        // If there is any space left, copy the bytes into the buffer.
        //

        if (BytesToWrite != 0) {
            memcpy(Stream->Buffer + Stream->BufferNextIndex,
                   (PVOID)Buffer + TotalBytesWritten,
                   BytesToWrite);

            assert(Stream->BufferValidSize == Stream->BufferNextIndex);

            Stream->BufferNextIndex += BytesToWrite;
            Stream->BufferValidSize = Stream->BufferNextIndex;
            if (Stream->BufferNextIndex == Stream->BufferSize) {
                Flush = TRUE;
            }

            TotalBytesWritten += BytesToWrite;

        //
        // If there's no space left, flush the buffer to make more.
        //

        } else {
            Flush = TRUE;
        }

        //
        // For the buffer not to want to flush it had better be done.
        //

        assert((Flush != FALSE) || (TotalBytesWritten == TotalBytesToWrite));

        if (Flush != FALSE) {
            if (fflush_unlocked(Stream) < 0) {
                break;
            }
        }
    }

    return TotalBytesWritten / Size;
}

LIBC_API
int
fflush (
    FILE *Stream
    )

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

{

    int Result;

    if (Stream == NULL) {
        ClpFlushAllStreams(FALSE, NULL);
        return 0;
    }

    ClpLockStream(Stream);
    Result = fflush_unlocked(Stream);
    ClpUnlockStream(Stream);
    return Result;
}

LIBC_API
int
fflush_unlocked (
    FILE *Stream
    )

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

{

    ssize_t BytesWritten;
    off_t Offset;
    int PreviousError;
    ssize_t Result;

    if (Stream == NULL) {
        ClpFlushAllStreams(FALSE, NULL);
        return 0;
    }

    Result = 0;
    if (Stream->BufferMode == _IONBF) {
        return 0;
    }

    if (Stream->Descriptor == -1) {
        errno = EBADF;
        return EOF;
    }

    //
    // If the buffer is full of read data, try and back up the file pointer.
    // Ignore failures.
    //

    if ((Stream->Flags & FILE_FLAG_READ_LAST) != 0) {
        Offset = Stream->BufferValidSize - Stream->BufferNextIndex;
        if ((Stream->Flags & FILE_FLAG_UNGET_VALID) != 0) {
            Offset += 1;
        }

        PreviousError = errno;
        lseek(Stream->Descriptor, -Offset, SEEK_CUR);
        errno = PreviousError;

    //
    // The buffer is full of dirty data. Write it out.
    //

    } else {
        BytesWritten = 0;
        while (BytesWritten < Stream->BufferNextIndex) {
            do {
                Result = write(Stream->Descriptor,
                               Stream->Buffer + BytesWritten,
                               Stream->BufferNextIndex - BytesWritten);

            } while ((Result < 0) && (errno == EINTR));

            if (Result <= 0) {
                Stream->Flags |= FILE_FLAG_ERROR;
                return EOF;
            }

            BytesWritten += Result;
        }
    }

    Stream->BufferNextIndex = 0;
    Stream->BufferValidSize = 0;
    Stream->Flags &= ~FILE_FLAG_UNGET_VALID;
    return 0;
}

LIBC_API
long
ftell (
    FILE *Stream
    )

/*++

Routine Description:

    This routine returns the given stream's file position.

Arguments:

    Stream - Supplies a pointer to the open file stream.

Return Value:

    Returns the current file position on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    off_t Result;

    Result = ftello(Stream);
    if ((long)Result != Result) {
        errno = ERANGE;
        return -1;
    }

    return (long)Result;
}

LIBC_API
off_t
ftello (
    FILE *Stream
    )

/*++

Routine Description:

    This routine returns the given stream's file position.

Arguments:

    Stream - Supplies a pointer to the open file stream.

Return Value:

    Returns the current file position on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    off_t Result;

    //
    // One might ask why the lock needs to be held for what amounts to just a
    // single read. The answer is that the file position may be larger than
    // the native integer size of the machine, and so the read may not be
    // atomic. Without the lock, a torn read could result. This could be
    // optimized for 64-bit systems where those reads are atomic.
    //

    ClpLockStream(Stream);
    Result = ftello_unlocked(Stream);
    ClpUnlockStream(Stream);
    return Result;
}

LIBC_API
off64_t
ftello64 (
    FILE *Stream
    )

/*++

Routine Description:

    This routine returns the given stream's file position.

Arguments:

    Stream - Supplies a pointer to the open file stream.

Return Value:

    Returns the current file position on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    return ftello(Stream);
}

LIBC_API
off_t
ftello_unlocked (
    FILE *Stream
    )

/*++

Routine Description:

    This routine returns the given stream's file position.

Arguments:

    Stream - Supplies a pointer to the open file stream.

Return Value:

    Returns the current file position on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    off_t NewOffset;

    NewOffset = lseek(Stream->Descriptor, 0, SEEK_CUR);
    if (NewOffset == -1) {
        return -1;
    }

    if ((Stream->Flags & FILE_FLAG_READ_LAST) != 0) {
        NewOffset -= Stream->BufferValidSize - Stream->BufferNextIndex;
        if ((Stream->Flags & FILE_FLAG_UNGET_VALID) != 0) {
            NewOffset -= 1;
        }

    } else {
        NewOffset += Stream->BufferValidSize;
    }

    return NewOffset;
}

LIBC_API
int
fseek (
    FILE *Stream,
    long Offset,
    int Whence
    )

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

{

    int Result;

    ClpLockStream(Stream);
    Result = fseeko_unlocked(Stream, Offset, Whence);
    ClpUnlockStream(Stream);
    return Result;
}

LIBC_API
int
fseeko (
    FILE *Stream,
    off_t Offset,
    int Whence
    )

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

{

    int Result;

    ClpLockStream(Stream);
    Result = fseeko_unlocked(Stream, Offset, Whence);
    ClpUnlockStream(Stream);
    return Result;
}

LIBC_API
int
fseeko64 (
    FILE *Stream,
    off64_t Offset,
    int Whence
    )

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

{

    return fseeko(Stream, Offset, Whence);
}

LIBC_API
int
fseeko_unlocked (
    FILE *Stream,
    off_t Offset,
    int Whence
    )

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

{

    //
    // It would be great to save the system call (or several) if the seek is
    // currently within the buffer, however apps (like m4 for example) rely on
    // using fseek to determine whether a descriptor is seekable, so ultimately
    // this function has to hit lseek somewhere.
    //

    fflush_unlocked(Stream);
    Stream->BufferNextIndex = 0;
    Stream->BufferValidSize = 0;
    Stream->Flags &= ~(FILE_FLAG_END_OF_FILE | FILE_FLAG_ERROR);
    if (lseek(Stream->Descriptor, Offset, Whence) != -1) {
        return 0;
    }

    return -1;
}

LIBC_API
int
fgetpos (
    FILE *Stream,
    fpos_t *Position
    )

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

{

    off_t Offset;
    int Result;

    ClpLockStream(Stream);
    Offset = ftello_unlocked(Stream);
    if (Offset == -1) {
        Result = -1;
        goto getposEnd;
    }

    Position->Offset = Offset;
    Position->ShiftState = Stream->ShiftState;
    Result = 0;

getposEnd:
    ClpUnlockStream(Stream);
    return Result;
}

LIBC_API
int
fsetpos (
    FILE *Stream,
    const fpos_t *Position
    )

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

{

    int Result;

    ClpLockStream(Stream);
    Result = fseeko_unlocked(Stream, Position->Offset, SEEK_SET);
    if (Result != 0) {
        goto setposEnd;
    }

    Stream->ShiftState = Position->ShiftState;
    Result = 0;

setposEnd:
    ClpUnlockStream(Stream);
    return Result;
}

LIBC_API
void
rewind (
    FILE *Stream
    )

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

{

    fseek(Stream, 0, SEEK_SET);
    clearerr(Stream);
    return;
}

LIBC_API
int
fileno (
    FILE *Stream
    )

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

{

    if (Stream == NULL) {
        errno = EBADF;
        return -1;
    }

    return Stream->Descriptor;
}

LIBC_API
int
fgetc (
    FILE *Stream
    )

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

{

    int Result;

    ClpLockStream(Stream);
    Result = fgetc_unlocked(Stream);
    ClpUnlockStream(Stream);
    return Result;
}

LIBC_API
int
fgetc_unlocked (
    FILE *Stream
    )

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

{

    unsigned char Byte;
    ssize_t Result;

    ORIENT_STREAM(Stream, FILE_FLAG_BYTE_ORIENTED);
    Result = fread_unlocked(&Byte, 1, 1, Stream);
    if (Result == 0) {
        return EOF;
    }

    return Byte;
}

LIBC_API
int
getchar (
    void
    )

/*++

Routine Description:

    This routine reads one byte from stdin.

Arguments:

    None.

Return Value:

    Returns the byte from stdin on success.

    -1 on failure, and errno will contain more information.

--*/

{

    return fgetc(stdin);
}

LIBC_API
int
getchar_unlocked (
    void
    )

/*++

Routine Description:

    This routine reads one byte from stdin without acquiring the file lock.

Arguments:

    None.

Return Value:

    Returns the byte from stdin on success.

    -1 on failure, and errno will contain more information.

--*/

{

    return fgetc_unlocked(stdin);
}

LIBC_API
int
getc (
    FILE *Stream
    )

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

{

    return fgetc(Stream);
}

LIBC_API
int
getc_unlocked (
    FILE *Stream
    )

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

{

    return fgetc_unlocked(Stream);
}

LIBC_API
char *
gets (
    char *Line
    )

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

{

    int Character;
    int Index;

    Character = EOF;
    if (Line == NULL) {
        return NULL;
    }

    //
    // Loop reading in characters.
    //

    Index = 0;
    while (TRUE) {
        Character = fgetc(stdin);
        if (Character == EOF) {
            break;
        }

        if (Character == '\n') {
            break;
        }

        Line[Index] = Character;
        Index += 1;
    }

    Line[Index] = '\0';
    if (Character == EOF) {
        return NULL;
    }

    return Line;
}

LIBC_API
char *
fgets (
    char *Buffer,
    int BufferSize,
    FILE *Stream
    )

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

{

    char *Result;

    ClpLockStream(Stream);
    Result = fgets_unlocked(Buffer, BufferSize, Stream);
    ClpUnlockStream(Stream);
    return Result;
}

LIBC_API
char *
fgets_unlocked (
    char *Buffer,
    int BufferSize,
    FILE *Stream
    )

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

{

    int Character;
    int Index;

    Character = EOF;
    if ((Buffer == NULL) || (BufferSize < 1)) {
        return NULL;
    }

    //
    // Loop reading in characters until the buffer is full.
    //

    Index = 0;
    while (Index < BufferSize - 1) {
        Character = fgetc_unlocked(Stream);
        if (Character == EOF) {
            break;
        }

        Buffer[Index] = Character;
        Index += 1;
        if (Character == '\n') {
            break;
        }
    }

    Buffer[Index] = '\0';
    if (Index == 0) {
        return NULL;
    }

    return Buffer;
}

LIBC_API
int
fputc (
    int Character,
    FILE *Stream
    )

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

{

    int Result;

    ClpLockStream(Stream);
    Result = fputc_unlocked(Character, Stream);
    ClpUnlockStream(Stream);
    return Result;
}

LIBC_API
int
fputc_unlocked (
    int Character,
    FILE *Stream
    )

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

{

    unsigned char Byte;
    int Result;

    ORIENT_STREAM(Stream, FILE_FLAG_BYTE_ORIENTED);
    Byte = (unsigned char)Character;
    Result = fwrite_unlocked(&Byte, 1, 1, Stream);
    if (Result > 0) {
        return Byte;
    }

    return EOF;
}

LIBC_API
int
putc (
    int Character,
    FILE *Stream
    )

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

{

    return fputc(Character, Stream);
}

LIBC_API
int
putc_unlocked (
    int Character,
    FILE *Stream
    )

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

{

    return fputc_unlocked(Character, Stream);
}

LIBC_API
int
putchar (
    int Character
    )

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

{

    return fputc(Character, stdout);
}

LIBC_API
int
putchar_unlocked (
    int Character
    )

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

{

    return fputc_unlocked(Character, stdout);
}

LIBC_API
int
puts (
    const char *String
    )

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

{

    int Result;

    Result = fputs(String, stdout);
    if (Result == EOF) {
        return Result;
    }

    return fputc('\n', stdout);
}

LIBC_API
int
fputs (
    const char *String,
    FILE *Stream
    )

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

{

    int Result;

    ClpLockStream(Stream);
    Result = fputs_unlocked(String, Stream);
    ClpUnlockStream(Stream);
    return Result;
}

LIBC_API
int
fputs_unlocked (
    const char *String,
    FILE *Stream
    )

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

{

    size_t Length;
    int Result;

    Result = 0;
    if (String == NULL) {
        return Result;
    }

    ORIENT_STREAM(Stream, FILE_FLAG_BYTE_ORIENTED);
    Length = strlen(String);
    Result = fwrite_unlocked(String, 1, Length, Stream);
    if (Result == Length) {
        return Result;
    }

    return EOF;
}

LIBC_API
int
ungetc (
    int Character,
    FILE *Stream
    )

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

{

    int Result;

    ClpLockStream(Stream);
    Result = ungetc_unlocked(Character, Stream);
    ClpUnlockStream(Stream);
    return Result;
}

LIBC_API
int
ungetc_unlocked (
    int Character,
    FILE *Stream
    )

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

{

    unsigned char Byte;

    if (Character == EOF) {
        return EOF;
    }

    ORIENT_STREAM(Stream, FILE_FLAG_BYTE_ORIENTED);
    Byte = (unsigned char)Character;
    if ((Stream->Flags & FILE_FLAG_UNGET_VALID) == 0) {
        Stream->Flags |= FILE_FLAG_UNGET_VALID;
        Stream->Flags &= ~FILE_FLAG_END_OF_FILE;
        Stream->UngetCharacter = Byte;
        return Byte;
    }

    return EOF;
}

LIBC_API
int
setvbuf (
    FILE *Stream,
    char *Buffer,
    int Mode,
    size_t BufferSize
    )

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

{

    int Result;

    Result = -1;
    ClpLockStream(Stream);
    if ((Mode != _IOLBF) && (Mode != _IOFBF) && (Mode != _IONBF)) {
        errno = EINVAL;
        goto setvbufEnd;
    }

    //
    // Flush the file for safety, even though generally users aren't supposed
    // to call this after they've started doing I/O on the stream.
    //

    fflush_unlocked(Stream);

    //
    // Free the old buffer.
    //

    if (((Stream->Flags & FILE_FLAG_BUFFER_ALLOCATED) != 0) &&
        (Stream->Buffer != NULL)) {

        free(Stream->Buffer);
        Stream->Flags &= ~FILE_FLAG_BUFFER_ALLOCATED;
    }

    Stream->Buffer = NULL;

    //
    // Un-buffered mode is easy, just leave the buffer NULLed out.
    //

    if (Mode == _IONBF) {
        Stream->BufferSize = 0;

    //
    // For buffered mode, either use the buffer they provided or allocate
    // one.
    //

    } else {
        if ((Buffer == NULL) || (BufferSize == 0)) {
            if (BufferSize == 0) {
                BufferSize = BUFSIZ;
            }

            Buffer = malloc(BufferSize);
            if (Buffer == NULL) {
                errno = ENOMEM;
                goto setvbufEnd;
            }

            Stream->Flags |= FILE_FLAG_BUFFER_ALLOCATED;
        }

        Stream->Buffer = Buffer;
        Stream->BufferSize = BufferSize;
    }

    Stream->BufferMode = Mode;
    Result = 0;

setvbufEnd:
    ClpUnlockStream(Stream);
    return Result;
}

LIBC_API
void
setbuf (
    FILE *Stream,
    char *Buffer
    )

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

{

    if (Buffer != NULL) {
        setvbuf(Stream, Buffer, _IOFBF, BUFSIZ);

    } else {
        setvbuf(Stream, Buffer, _IONBF, BUFSIZ);
    }

    return;
}

LIBC_API
void
clearerr (
    FILE *Stream
    )

/*++

Routine Description:

    This routine clears the error and end-of-file indicators for the given
    stream.

Arguments:

    Stream - Supplies a pointer to the file stream.

Return Value:

    None.

--*/

{

    ClpLockStream(Stream);
    clearerr_unlocked(Stream);
    ClpUnlockStream(Stream);
    return;
}

LIBC_API
void
clearerr_unlocked (
    FILE *Stream
    )

/*++

Routine Description:

    This routine clears the error and end-of-file indicators for the given
    stream. This routine does not acquire the file lock.

Arguments:

    Stream - Supplies a pointer to the file stream.

Return Value:

    None.

--*/

{

    Stream->Flags &= ~(FILE_FLAG_ERROR | FILE_FLAG_END_OF_FILE);
    return;
}

LIBC_API
int
feof (
    FILE *Stream
    )

/*++

Routine Description:

    This routine returns whether or not the current stream has attempted to
    read beyond the end of the file.

Arguments:

    Stream - Supplies a pointer to the file stream.

Return Value:

    Returns non-zero if the end of file indicator is set for the given stream.

--*/

{

    int Result;

    ClpLockStream(Stream);
    Result = feof_unlocked(Stream);
    ClpUnlockStream(Stream);
    return Result;
}

LIBC_API
int
feof_unlocked (
    FILE *Stream
    )

/*++

Routine Description:

    This routine returns whether or not the current stream has attempted to
    read beyond the end of the file, without acquiring the file lock.

Arguments:

    Stream - Supplies a pointer to the file stream.

Return Value:

    Returns non-zero if the end of file indicator is set for the given stream.

--*/

{

    if ((Stream->Flags & FILE_FLAG_END_OF_FILE) != 0) {
        return TRUE;
    }

    return FALSE;
}

LIBC_API
int
ferror (
    FILE *Stream
    )

/*++

Routine Description:

    This routine returns whether or not the error indicator is set for the
    current stream.

Arguments:

    Stream - Supplies a pointer to the file stream.

Return Value:

    Returns non-zero if the error indicator is set for the given stream.

--*/

{

    int Result;

    ClpLockStream(Stream);
    Result = ferror_unlocked(Stream);
    ClpUnlockStream(Stream);
    return Result;
}

LIBC_API
int
ferror_unlocked (
    FILE *Stream
    )

/*++

Routine Description:

    This routine returns whether or not the error indicator is set for the
    current stream, without acquiring the file lock.

Arguments:

    Stream - Supplies a pointer to the file stream.

Return Value:

    Returns non-zero if the error indicator is set for the given stream.

--*/

{

    if ((Stream->Flags & FILE_FLAG_ERROR) != 0) {
        return TRUE;
    }

    return FALSE;
}

LIBC_API
void
flockfile (
    FILE *Stream
    )

/*++

Routine Description:

    This routine explicitly locks a file stream. It can be used on the standard
    I/O streams to group a batch of I/O together.

Arguments:

    Stream - Supplies a pointer to the file stream.

Return Value:

    None.

--*/

{

    return ClpLockStream(Stream);
}

LIBC_API
int
ftrylockfile (
    FILE *Stream
    )

/*++

Routine Description:

    This routine attempts to acquire the lock for a given stream.

Arguments:

    Stream - Supplies a pointer to the file stream.

Return Value:

    0 if the lock was successfully acquired.

    Non-zero if the file lock could not be acquired.

--*/

{

    if (ClpTryToLockStream(Stream) != FALSE) {
        return 0;
    }

    return -1;
}

LIBC_API
void
funlockfile (
    FILE *Stream
    )

/*++

Routine Description:

    This routine explicitly unlocks a file stream that had been previously
    locked with flockfile or ftrylockfile (on a successful attempt).

Arguments:

    Stream - Supplies a pointer to the file stream.

Return Value:

    None.

--*/

{

    return ClpUnlockStream(Stream);
}

LIBC_API
int
printf (
    const char *Format,
    ...
    )

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

{

    va_list Arguments;
    int Result;

    va_start(Arguments, Format);
    Result = vfprintf(stdout, Format, Arguments);
    va_end(Arguments);
    return Result;
}

LIBC_API
int
fprintf (
    FILE *Stream,
    const char *Format,
    ...
    )

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

{

    va_list Arguments;
    int Result;

    va_start(Arguments, Format);
    Result = vfprintf(Stream, Format, Arguments);
    va_end(Arguments);
    return Result;
}

LIBC_API
int
fprintf_unlocked (
    FILE *Stream,
    const char *Format,
    ...
    )

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

{

    va_list Arguments;
    int Result;

    va_start(Arguments, Format);
    Result = vfprintf_unlocked(Stream, Format, Arguments);
    va_end(Arguments);
    return Result;
}

LIBC_API
int
vfprintf (
    FILE *File,
    const char *Format,
    va_list Arguments
    )

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

{

    int Result;

    ClpLockStream(File);
    Result = vfprintf_unlocked(File, Format, Arguments);
    ClpUnlockStream(File);
    return Result;
}

LIBC_API
int
vfprintf_unlocked (
    FILE *File,
    const char *Format,
    va_list Arguments
    )

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

{

    ULONG CharactersWritten;
    PRINT_FORMAT_CONTEXT PrintContext;
    STREAM_PRINT_CONTEXT StreamContext;

    StreamContext.Stream = File;
    StreamContext.BufferNextIndex = 0;
    StreamContext.CharactersWritten = 0;
    memset(&PrintContext, 0, sizeof(PRINT_FORMAT_CONTEXT));
    PrintContext.Context = &StreamContext;
    PrintContext.WriteCharacter = ClpFileFormatWriteCharacter;
    RtlInitializeMultibyteState(&(PrintContext.State),
                                CharacterEncodingDefault);

    RtlFormat(&PrintContext, (PSTR)Format, Arguments);

    //
    // Flush the remaining buffered bytes if necessary.
    //

    if (File->BufferMode == _IONBF) {
        if (StreamContext.BufferNextIndex != 0) {
            CharactersWritten = fwrite_unlocked(StreamContext.Buffer,
                                                1,
                                                StreamContext.BufferNextIndex,
                                                File);

            if (CharactersWritten > 0) {
                StreamContext.CharactersWritten += CharactersWritten;
            }
        }

        CharactersWritten = StreamContext.CharactersWritten;

        assert(CharactersWritten <= PrintContext.CharactersWritten);

    } else {
        CharactersWritten = PrintContext.CharactersWritten;
    }

    return CharactersWritten;
}

LIBC_API
int
vprintf (
    const char *Format,
    va_list Arguments
    )

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

{

    return vfprintf(stdout, Format, Arguments);
}

BOOL
ClpInitializeFileIo (
    VOID
    )

/*++

Routine Description:

    This routine initializes the file I/O subsystem of the C library.

Arguments:

    None.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    pthread_mutexattr_t Attribute;
    ULONG BufferMode;
    BOOL Result;

    Result = FALSE;

    //
    // Initialize the global stream list.
    //

    INITIALIZE_LIST_HEAD(&ClStreamList);
    pthread_mutexattr_init(&Attribute);
    pthread_mutexattr_settype(&Attribute, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&ClStreamListLock, &Attribute);
    pthread_mutexattr_destroy(&Attribute);

    //
    // Create file pointers for the three standard streams. Standard in and
    // standard out are fully buffered only if they are not pointing at an
    // interactive device.
    //

    BufferMode = _IOFBF;
    if (isatty(STDIN_FILENO) != 0) {
        BufferMode = _IOLBF;
    }

    stdin = ClpCreateFileStructure(STDIN_FILENO, O_RDONLY, BufferMode);
    if (stdin == NULL) {
        goto InitializeFileIoEnd;
    }

    stdin->Flags |= FILE_FLAG_STANDARD_IO;
    BufferMode = _IOFBF;
    if (isatty(STDOUT_FILENO) != 0) {
        BufferMode = _IOLBF;
    }

    stdout = ClpCreateFileStructure(STDOUT_FILENO, O_WRONLY, BufferMode);
    if (stdout == NULL) {
        goto InitializeFileIoEnd;
    }

    stdout->Flags |= FILE_FLAG_STANDARD_IO;

    //
    // Standard error is never buffered. Default to unbuffered.
    //

    stderr = ClpCreateFileStructure(STDERR_FILENO, O_WRONLY, _IONBF);
    if (stderr == NULL) {
        goto InitializeFileIoEnd;
    }

    stderr->Flags |= FILE_FLAG_STANDARD_IO;
    Result = TRUE;

InitializeFileIoEnd:
    if (Result == FALSE) {
        if (stdin != NULL) {
            ClpDestroyFileStructure(stdin);
            stdin = NULL;
        }

        if (stdout != NULL) {
            ClpDestroyFileStructure(stdout);
            stdout = NULL;
        }

        if (stderr != NULL) {
            ClpDestroyFileStructure(stderr);
            stderr = NULL;
        }
    }

    return Result;
}

VOID
ClpLockStream (
    FILE *Stream
    )

/*++

Routine Description:

    This routine locks the file stream.

Arguments:

    Stream - Supplies a pointer to the stream to lock.

Return Value:

    None.

--*/

{

    int Status;

    if ((Stream->Flags & FILE_FLAG_DISABLE_LOCKING) != 0) {
        return;
    }

    Status = pthread_mutex_lock(&(Stream->Lock));

    ASSERT(Status == 0);

    return;
}

BOOL
ClpTryToLockStream (
    FILE *Stream
    )

/*++

Routine Description:

    This routine makes a single attempt at locking the file stream. If locking
    is disabled on the stream, this always returns TRUE.

Arguments:

    Stream - Supplies a pointer to the stream to try to lock.

Return Value:

    TRUE if the lock was successfully acquired.

    FALSE if the lock was not successfully acquired.

--*/

{

    int Status;

    if ((Stream->Flags & FILE_FLAG_DISABLE_LOCKING) != 0) {
        return TRUE;
    }

    Status = pthread_mutex_trylock(&(Stream->Lock));
    if (Status == 0) {
        return TRUE;
    }

    return FALSE;
}

VOID
ClpUnlockStream (
    FILE *Stream
    )

/*++

Routine Description:

    This routine unlocks the file stream.

Arguments:

    Stream - Supplies a pointer to the stream to unlock.

Return Value:

    None.

--*/

{

    if ((Stream->Flags & FILE_FLAG_DISABLE_LOCKING) != 0) {
        return;
    }

    pthread_mutex_unlock(&(Stream->Lock));
    return;
}

VOID
ClpFlushAllStreams (
    BOOL AllUnlocked,
    PFILE UnlockedStream
    )

/*++

Routine Description:

    This routine flushes every stream in the application.

Arguments:

    AllUnlocked - Supplies a boolean that if TRUE flushes every stream without
        acquiring the file lock. This is used during aborts.

    UnlockedStream - Supplies a specific stream that when flushed should be
        flushed unlocked.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PFILE Stream;

    pthread_mutex_lock(&ClStreamListLock);
    CurrentEntry = ClStreamList.Next;
    while (CurrentEntry != &ClStreamList) {
        Stream = LIST_VALUE(CurrentEntry, FILE, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // Flush any dirty streams.
        //

        if ((Stream->Flags & FILE_FLAG_READ_LAST) == 0) {
            if ((AllUnlocked != FALSE) || (Stream == UnlockedStream)) {
                fflush_unlocked(Stream);

            } else {
                fflush(Stream);
            }
        }
    }

    pthread_mutex_unlock(&ClStreamListLock);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

PFILE
ClpCreateFileStructure (
    ULONG Descriptor,
    ULONG OpenFlags,
    ULONG BufferMode
    )

/*++

Routine Description:

    This routine creates a file stream structure.

Arguments:

    Descriptor - Supplies the underlying file descriptor number associated with
        this stream.

    OpenFlags - Supplies the flags the file was opened with.

    BufferMode - Supplies the buffering mode for the file.

Return Value:

    Returns a pointer to the created file on success.

    NULL on allocation failure.

--*/

{

    pthread_mutexattr_t Attribute;
    FILE *File;
    BOOL Result;

    Result = FALSE;
    File = malloc(sizeof(FILE));
    if (File == NULL) {
        goto CreateFileEnd;
    }

    RtlZeroMemory(File, sizeof(FILE));
    File->Descriptor = Descriptor;
    File->OpenFlags = OpenFlags;
    pthread_mutexattr_init(&Attribute);
    pthread_mutexattr_settype(&Attribute, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&(File->Lock), &Attribute);
    pthread_mutexattr_destroy(&Attribute);
    File->BufferMode = BufferMode;
    if ((OpenFlags & O_ACCMODE) != O_WRONLY) {
        File->Flags |= FILE_FLAG_CAN_READ;
        if ((OpenFlags & O_ACCMODE) == O_RDONLY) {
            File->Flags |= FILE_FLAG_READ_LAST;
        }
    }

    //
    // If the stream is anything other than non-buffered, create a buffer for
    // it.
    //

    if (File->BufferMode != _IONBF) {
        File->Buffer = malloc(BUFSIZ);
        if (File->Buffer == NULL) {
            goto CreateFileEnd;
        }

        File->BufferSize = BUFSIZ;
        File->Flags |= FILE_FLAG_BUFFER_ALLOCATED;
    }

    //
    // Add the file to the global list, making it officially open for business.
    //

    pthread_mutex_lock(&ClStreamListLock);
    INSERT_AFTER(&(File->ListEntry), &ClStreamList);
    pthread_mutex_unlock(&ClStreamListLock);
    Result = TRUE;

CreateFileEnd:
    if (Result == FALSE) {
        if (File != NULL) {
            if (File->Buffer != NULL) {
                free(File->Buffer);
            }

            free(File);
            File = NULL;
        }
    }

    return File;
}

VOID
ClpDestroyFileStructure (
    PFILE File
    )

/*++

Routine Description:

    This routine destroys a file stream structure.

Arguments:

    File - Supplies a pointer to the file stream structure to destroy.

Return Value:

    None.

--*/

{

    if (File != NULL) {
        if (File->ListEntry.Next != NULL) {
            pthread_mutex_lock(&ClStreamListLock);
            LIST_REMOVE(&(File->ListEntry));
            pthread_mutex_unlock(&ClStreamListLock);
        }

        File->ListEntry.Next = NULL;
        if (((File->Flags & FILE_FLAG_BUFFER_ALLOCATED) != 0) &&
            (File->Buffer != NULL)) {

            free(File->Buffer);
        }

        pthread_mutex_destroy(&(File->Lock));
        free(File);
    }

    return;
}

BOOL
ClpFileFormatWriteCharacter (
    INT Character,
    PPRINT_FORMAT_CONTEXT Context
    )

/*++

Routine Description:

    This routine writes a character to the output during a printf-style
    formatting operation.

Arguments:

    Character - Supplies the character to be written.

    Context - Supplies a pointer to the printf-context.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG CharactersWritten;
    PSTREAM_PRINT_CONTEXT StreamContext;

    StreamContext = Context->Context;

    //
    // If the stream is buffered in any way, then pass the character on to the
    // stream.
    //

    if (StreamContext->Stream->BufferMode != _IONBF) {
        if (fputc_unlocked(Character, StreamContext->Stream) == -1) {
            return FALSE;
        }

    //
    // If the stream is unbuffered, then locally buffer some characters
    // together before flushing. This reduces the number of system calls
    // required for fprintf on unbuffered streams.
    //

    } else {
        StreamContext->Buffer[StreamContext->BufferNextIndex] = Character;
        StreamContext->BufferNextIndex += 1;

        //
        // If the local buffer is full, then write to the stream. This will
        // flush the data immediately.
        //

        if (StreamContext->BufferNextIndex == STREAM_PRINT_BUFFER_SIZE) {
            StreamContext->BufferNextIndex = 0;
            CharactersWritten = fwrite_unlocked(StreamContext->Buffer,
                                                1,
                                                STREAM_PRINT_BUFFER_SIZE,
                                                StreamContext->Stream);

            if (CharactersWritten < 0) {
                return FALSE;
            }

            StreamContext->CharactersWritten += CharactersWritten;
            if (CharactersWritten != STREAM_PRINT_BUFFER_SIZE) {
                return FALSE;
            }
        }
    }

    return TRUE;
}

INT
ClpConvertStreamModeStringToOpenFlags (
    PSTR ModeString,
    INT *OpenFlags
    )

/*++

Routine Description:

    This routine converts a mode string supplied with a stream open command to
    a set of open flags.

Arguments:

    ModeString - Supplies a pointer to the mode string.

    OpenFlags - Supplies a pointer where the open flags will be returned.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    INT Flags;

    Flags = 0;
    *OpenFlags = 0;

    //
    // Get the open flags.
    //

    while (*ModeString != '\0') {
        switch (*ModeString) {
        case 'r':
            Flags |= O_RDONLY;
            break;

        case 'w':
            Flags |= O_WRONLY | O_CREAT | O_TRUNC;
            break;

        case 'a':
            Flags |= O_WRONLY | O_CREAT | O_APPEND;
            break;

        case '+':
            Flags &= ~O_ACCMODE;
            Flags |= O_RDWR;
            break;

        case 'b':
        case 't':
            break;

        //
        // TODO: Open the file with O_CLOEXEC.
        //

        case 'e':
            break;

        case 'x':
            Flags |= O_EXCL;
            break;

        default:
            return EINVAL;
        }

        ModeString += 1;
    }

    *OpenFlags = Flags;
    return 0;
}

