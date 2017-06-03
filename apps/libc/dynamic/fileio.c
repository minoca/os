/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fileio.c

Abstract:

    This module implements file I/O routines.

Author:

    Evan Green 5-Mar-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <alloca.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <limits.h>

//
// --------------------------------------------------------------------- Macros
//

#define ASSERT_POLL_FLAGS_EQUIVALENT() \
    ASSERT((POLLIN == POLL_EVENT_IN) && \
           (POLLRDBAND == POLL_EVENT_IN_HIGH_PRIORITY) && \
           (POLLOUT == POLL_EVENT_OUT) && \
           (POLLWRBAND == POLL_EVENT_OUT_HIGH_PRIORITY) && \
           (POLLERR == POLL_EVENT_ERROR) && \
           (POLLHUP == POLL_EVENT_DISCONNECTED) && \
           (POLLNVAL == POLL_EVENT_INVALID_HANDLE))

//
// Figure this out on 64 bits, as pollfd needs to have an int for a descriptor,
// but the kernel wants pointer sized descriptors.
//

#define ASSERT_POLL_STRUCTURE_EQUIVALENT() \
    ASSERT(sizeof(struct pollfd) == sizeof(POLL_DESCRIPTOR))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the initial size for the terminal name buffer.
//

#define INITIAL_TERMINAL_NAME_BUFFER_SIZE 64

//
// Define the initial allocation size for the asprintf destination string.
//

#define ASPRINT_INITIAL_BUFFER_SIZE 64

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the context used during the asprintf/vasprintf
    routines.

Members:

    Buffer - Stores a pointer to the allocated string buffer.

    Size - Stores the number of valid characters currently in the buffer.

    Capacity - Stores the maximum number of bytes that can fit in the buffer
        before it will need to be reallocated.

--*/

typedef struct _ASPRINT_CONTEXT {
    PSTR Buffer;
    UINTN Size;
    UINTN Capacity;
} ASPRINT_CONTEXT, *PASPRINT_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

int
ClpOpen (
    int Directory,
    const char *Path,
    int OpenFlags,
    va_list ArgumentList
    );

BOOL
ClpAsPrintWriteCharacter (
    INT Character,
    PPRINT_FORMAT_CONTEXT Context
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the global buffer used by the ttyname function.
//

PSTR ClTerminalNameBuffer;
size_t ClTerminalNameBufferSize;

//
// Store a pointer to the global buffer used by the ctermid function.
//

PSTR ClTerminalIdBuffer;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
int
open (
    const char *Path,
    int OpenFlags,
    ...
    )

/*++

Routine Description:

    This routine opens a file and connects it to a file descriptor.

Arguments:

    Path - Supplies a pointer to a null terminated string containing the path
        of the file to open.

    OpenFlags - Supplies a set of flags ORed together. See O_* definitions.

    ... - Supplies an optional integer representing the permission mask to set
        if the file is to be created by this open call.

Return Value:

    Returns a file descriptor on success.

    -1 on failure. The errno variable will be set to indicate the error.

--*/

{

    va_list ArgumentList;
    int Result;

    va_start(ArgumentList, OpenFlags);
    Result = ClpOpen(AT_FDCWD, Path, OpenFlags, ArgumentList);
    va_end(ArgumentList);
    return Result;
}

LIBC_API
int
openat (
    int Directory,
    const char *Path,
    int OpenFlags,
    ...
    )

/*++

Routine Description:

    This routine opens a file and connects it to a file descriptor.

Arguments:

    Directory - Supplies an optional file descriptor. If the given path
        is a relative path, the directory referenced by this descriptor will
        be used as a starting point for path resolution. Supply AT_FDCWD to
        use the working directory for relative paths.

    Path - Supplies a pointer to a null terminated string containing the path
        of the file to open.

    OpenFlags - Supplies a set of flags ORed together. See O_* definitions.

    ... - Supplies an optional integer representing the permission mask to set
        if the file is to be created by this open call.

Return Value:

    Returns a file descriptor on success.

    -1 on failure. The errno variable will be set to indicate the error.

--*/

{

    va_list ArgumentList;
    int Result;

    va_start(ArgumentList, OpenFlags);
    Result = ClpOpen(Directory, Path, OpenFlags, ArgumentList);
    va_end(ArgumentList);
    return Result;
}

LIBC_API
int
fcntl (
    int FileDescriptor,
    int Command,
    ...
    )

/*++

Routine Description:

    This routine performs a file control operation on an open file handle.

Arguments:

    FileDescriptor - Supplies the file descriptor to operate on.

    Command - Supplies the file control command. See F_* definitions.

    ... - Supplies any additional command-specific arguments.

Return Value:

    Returns some value other than -1 to indicate success. For some commands
    (like F_DUPFD) this is a file descriptor. For others (like F_GETFD and
    F_GETFL) this is a bitfield of status flags.

    -1 on error, and errno will be set to indicate the error.

--*/

{

    va_list ArgumentList;
    off_t CurrentOffset;
    int DescriptorMinimum;
    FILE_CONTROL_COMMAND FileControlCommand;
    struct flock *FileLock;
    ULONG Flags;
    off_t Length;
    FILE_CONTROL_PARAMETERS_UNION Parameters;
    int ReturnValue;
    int SetFlags;
    struct stat Stat;
    KSTATUS Status;

    ReturnValue = -1;
    FileControlCommand = FileControlCommandInvalid;
    FileLock = NULL;
    va_start(ArgumentList, Command);
    switch (Command) {
    case F_DUPFD:
        FileControlCommand = FileControlCommandDuplicate;
        DescriptorMinimum = va_arg(ArgumentList, int);
        if (DescriptorMinimum < 0) {
            Status = STATUS_INVALID_PARAMETER;
            goto fcntlEnd;
        }

        Parameters.DuplicateDescriptor = (HANDLE)(UINTN)DescriptorMinimum;
        break;

    case F_GETFD:
        FileControlCommand = FileControlCommandGetFlags;
        Parameters.Flags = 0;
        break;

    case F_SETFD:
        FileControlCommand = FileControlCommandSetFlags;
        Parameters.Flags = 0;
        SetFlags = va_arg(ArgumentList, int);
        if ((SetFlags & FD_CLOEXEC) != 0) {
            Parameters.Flags |= FILE_DESCRIPTOR_CLOSE_ON_EXECUTE;
        }

        break;

    case F_GETFL:
        FileControlCommand = FileControlCommandGetStatusAndAccess;
        Parameters.Flags = 0;
        break;

    case F_SETFL:
        FileControlCommand = FileControlCommandSetStatus;
        Parameters.Flags = 0;

        //
        // Only a few flags are honored by the kernel. Changing access mode
        // for instance is not possible.
        //

        SetFlags = va_arg(ArgumentList, int);
        if ((SetFlags & O_APPEND) != 0) {
            Parameters.Flags |= SYS_OPEN_FLAG_APPEND;
        }

        if ((SetFlags & O_NONBLOCK) != 0) {
            Parameters.Flags |= SYS_OPEN_FLAG_NON_BLOCKING;
        }

        if ((SetFlags & O_NOATIME) != 0) {
            Parameters.Flags |= SYS_OPEN_FLAG_NO_ACCESS_TIME;
        }

        if ((SetFlags & O_ASYNC) != 0) {
            Parameters.Flags |= SYS_OPEN_FLAG_ASYNCHRONOUS;
        }

        break;

    case F_GETOWN:
        FileControlCommand = FileControlCommandGetSignalOwner;
        Parameters.Owner = 0;
        break;

    case F_SETOWN:
        FileControlCommand = FileControlCommandSetSignalOwner;
        Parameters.Owner = va_arg(ArgumentList, pid_t);
        break;

    case F_GETLK:
    case F_SETLK:
    case F_SETLKW:
        if (Command == F_GETLK) {
            FileControlCommand = FileControlCommandGetLock;

        } else if (Command == F_SETLK) {
            FileControlCommand = FileControlCommandSetLock;

        } else {

            assert(Command == F_SETLKW);

            FileControlCommand = FileControlCommandBlockingSetLock;
        }

        //
        // Convert the flock structure to a file lock. Start with the type.
        //

        FileLock = va_arg(ArgumentList, struct flock *);
        switch (FileLock->l_type) {
        case F_RDLCK:
            Parameters.FileLock.Type = FileLockRead;
            break;

        case F_WRLCK:
            Parameters.FileLock.Type = FileLockReadWrite;
            break;

        case F_UNLCK:
            Parameters.FileLock.Type = FileLockUnlock;
            break;

        default:
            Status = STATUS_INVALID_PARAMETER;
            goto fcntlEnd;
        }

        //
        // Make the offset relative to the beginning of the file.
        //

        Parameters.FileLock.Offset = FileLock->l_start;
        switch (FileLock->l_whence) {
        case SEEK_SET:
            break;

        case SEEK_CUR:
            CurrentOffset = lseek(FileDescriptor, 0, SEEK_CUR);
            if (CurrentOffset == -1) {
                Status = STATUS_INVALID_PARAMETER;
                goto fcntlEnd;
            }

            Parameters.FileLock.Offset += CurrentOffset;
            break;

        case SEEK_END:
            if (fstat(FileDescriptor, &Stat) != 0) {
                Status = STATUS_INVALID_PARAMETER;
                goto fcntlEnd;
            }

            Parameters.FileLock.Offset += Stat.st_size;
            break;
        }

        //
        // Get the length sorted out, which may be negative.
        //

        Length = FileLock->l_len;
        if (Length < 0) {
            if (Parameters.FileLock.Offset < (ULONGLONG)Length) {
                Length = Parameters.FileLock.Offset;
            }

            Parameters.FileLock.Offset -= Length;
            Length = -Length;
        }

        Parameters.FileLock.Size = Length;
        Parameters.FileLock.ProcessId = 0;
        break;

    case F_CLOSEM:
        FileControlCommand = FileControlCommandCloseFrom;
        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        goto fcntlEnd;
    }

    Status = OsFileControl((HANDLE)(UINTN)FileDescriptor,
                           FileControlCommand,
                           &Parameters);

    if (!KSUCCESS(Status)) {

        //
        // The kernel returns access denied if the open handle permissions
        // aren't correct, which this routine converts to invalid handle.
        // The kernel also returns resource in use, which this routine
        // converts to try again.
        //

        if ((Command == F_GETLK) || (Command == F_SETLK) ||
            (Command == F_SETLKW)) {

            if (Status == STATUS_ACCESS_DENIED) {
                Status = STATUS_INVALID_HANDLE;

            } else if (Status == STATUS_RESOURCE_IN_USE) {
                Status = STATUS_TRY_AGAIN;
            }
        }

        goto fcntlEnd;
    }

    switch (Command) {
    case F_DUPFD:
        ReturnValue = (int)(UINTN)(Parameters.DuplicateDescriptor);
        break;

    case F_GETFD:
        ReturnValue = 0;
        if ((Parameters.Flags & FILE_DESCRIPTOR_CLOSE_ON_EXECUTE) != 0) {
            ReturnValue |= FD_CLOEXEC;
        }

        break;

    case F_SETFD:
    case F_SETFL:
    case F_CLOSEM:
        ReturnValue = 0;
        break;

    case F_GETFL:
        ReturnValue = 0;
        Flags = Parameters.Flags;
        if ((Flags & SYS_OPEN_FLAG_READ) != 0) {
            ReturnValue |= O_RDONLY;
        }

        if ((Flags & SYS_OPEN_FLAG_WRITE) != 0) {
            ReturnValue |= O_WRONLY;
        }

        if ((Flags & SYS_OPEN_FLAG_EXECUTE) != 0) {
            ReturnValue |= O_EXEC;
        }

        if ((Flags & SYS_OPEN_FLAG_TRUNCATE) != 0) {
            ReturnValue |= O_TRUNC;
        }

        if ((Flags & SYS_OPEN_FLAG_APPEND) != 0) {
            ReturnValue |= O_APPEND;
        }

        if ((Flags & SYS_OPEN_FLAG_NON_BLOCKING) != 0) {
            ReturnValue |= O_NONBLOCK;
        }

        if ((Flags & SYS_OPEN_FLAG_CREATE) != 0) {
            ReturnValue |= O_CREAT;
        }

        if ((Flags & SYS_OPEN_FLAG_FAIL_IF_EXISTS) != 0) {
            ReturnValue |= O_EXCL;
        }

        if ((Flags & SYS_OPEN_FLAG_DIRECTORY) != 0) {
            ReturnValue |= O_DIRECTORY;
        }

        if ((Flags & SYS_OPEN_FLAG_NO_SYMBOLIC_LINK) != 0) {
            ReturnValue |= O_NOFOLLOW;
        }

        if ((Flags & SYS_OPEN_FLAG_SYNCHRONIZED) != 0) {
            ReturnValue |= O_SYNC;
        }

        if ((Flags & SYS_OPEN_FLAG_NO_CONTROLLING_TERMINAL) != 0) {
            ReturnValue |= O_NOCTTY;
        }

        if ((Flags & SYS_OPEN_FLAG_NO_ACCESS_TIME) != 0) {
            ReturnValue |= O_NOATIME;
        }

        if ((Flags & SYS_OPEN_FLAG_ASYNCHRONOUS) != 0) {
            ReturnValue |= O_ASYNC;
        }

        break;

    case F_GETLK:
    case F_SETLK:
    case F_SETLKW:

        //
        // Convert back to an flock structure.
        //

        switch (Parameters.FileLock.Type) {
        case FileLockRead:
            FileLock->l_type = F_RDLCK;
            break;

        case FileLockReadWrite:
            FileLock->l_type = F_WRLCK;
            break;

        //
        // If unlocked, don't convert any other parameters. F_GETLK is
        // supposed to return EINVAL if no valid locking information was
        // returned.
        //

        case FileLockUnlock:
            FileLock->l_type = F_UNLCK;
            if (Command == F_GETLK) {
                Status = STATUS_INVALID_PARAMETER;
            }

            ReturnValue = 0;
            goto fcntlEnd;

        default:

            assert(FALSE);

            Status = STATUS_INVALID_PARAMETER;
            goto fcntlEnd;
        }

        FileLock->l_start = Parameters.FileLock.Offset;
        FileLock->l_len = Parameters.FileLock.Size;
        FileLock->l_pid = Parameters.FileLock.ProcessId;
        FileLock->l_whence = SEEK_SET;
        ReturnValue = 0;
        break;

    case F_GETOWN:
        ReturnValue = Parameters.Owner;
        break;

    case F_SETOWN:
        ReturnValue = 0;
        break;

    default:

        assert(FALSE);

        goto fcntlEnd;
    }

fcntlEnd:
    va_end(ArgumentList);
    if (!KSUCCESS(Status)) {
        ReturnValue = -1;
        errno = ClConvertKstatusToErrorNumber(Status);
    }

    return ReturnValue;
}

LIBC_API
int
close (
    int FileDescriptor
    )

/*++

Routine Description:

    This routine closes a file descriptor.

Arguments:

    FileDescriptor - Supplies the file descriptor to close.

Return Value:

    0 on success.

    -1 if the file could not be closed properly. The state of the file
    descriptor is undefined, but in many cases is still open. The errno
    variable will be set to contain more detailed information.

--*/

{

    KSTATUS Status;

    Status = OsClose((HANDLE)(UINTN)FileDescriptor);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

LIBC_API
int
closefrom (
    int FileDescriptor
    )

/*++

Routine Description:

    This routine closes all file descriptors with a value greater than or
    equal to the given file descriptor.

Arguments:

    FileDescriptor - Supplies the minimum file descriptor number.

Return Value:

    0 on success.

    -1 if a file descriptor could not be closed properly. The state of the file
    descriptor is undefined, but in many cases is still open. The errno
    variable will be set to contain more detailed information.

--*/

{

    return fcntl(FileDescriptor, F_CLOSEM);
}

LIBC_API
ssize_t
read (
    int FileDescriptor,
    void *Buffer,
    size_t ByteCount
    )

/*++

Routine Description:

    This routine attempts to read the specifed number of bytes from the given
    open file descriptor.

Arguments:

    FileDescriptor - Supplies the file descriptor returned by the open function.

    Buffer - Supplies a pointer to the buffer where the read bytes will be
        returned.

    ByteCount - Supplies the number of bytes to read.

Return Value:

    Returns the number of bytes successfully read from the file.

    -1 on failure, and errno will contain more information.

--*/

{

    UINTN BytesCompleted;
    KSTATUS Status;

    //
    // Truncate the byte count, so that it does not exceed the maximum number
    // of bytes that can be returned.
    //

    if (ByteCount > (size_t)SSIZE_MAX) {
        ByteCount = (size_t)SSIZE_MAX;
    }

    //
    // Ask the OS to actually do the I/O.
    //

    Status = OsPerformIo((HANDLE)(UINTN)FileDescriptor,
                         IO_OFFSET_NONE,
                         ByteCount,
                         0,
                         SYS_WAIT_TIME_INDEFINITE,
                         (PVOID)Buffer,
                         &BytesCompleted);

    if (Status == STATUS_TIMEOUT) {
        errno = EAGAIN;
        return -1;

    } else if ((!KSUCCESS(Status)) && (Status != STATUS_END_OF_FILE)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        if (BytesCompleted == 0) {
            BytesCompleted = -1;
        }
    }

    return (ssize_t)BytesCompleted;
}

LIBC_API
ssize_t
pread (
    int FileDescriptor,
    void *Buffer,
    size_t ByteCount,
    off_t Offset
    )

/*++

Routine Description:

    This routine attempts to read the specifed number of bytes from the given
    open file descriptor at a given offset. It does not change the current
    file pointer.

Arguments:

    FileDescriptor - Supplies the file descriptor returned by the open function.

    Buffer - Supplies a pointer to the buffer where the read bytes will be
        returned.

    ByteCount - Supplies the number of bytes to read.

    Offset - Supplies the offset from the start of the file to read from.

Return Value:

    Returns the number of bytes successfully read from the file.

    -1 on failure, and errno will contain more information.

--*/

{

    UINTN BytesCompleted;
    KSTATUS Status;

    //
    // Truncate the byte count, so that it does not exceed the maximum number
    // of bytes that can be returned.
    //

    if (ByteCount > (size_t)SSIZE_MAX) {
        ByteCount = (size_t)SSIZE_MAX;
    }

    //
    // Ask the OS to actually do the I/O.
    //

    Status = OsPerformIo((HANDLE)(UINTN)FileDescriptor,
                         Offset,
                         ByteCount,
                         0,
                         SYS_WAIT_TIME_INDEFINITE,
                         (PVOID)Buffer,
                         &BytesCompleted);

    if (Status == STATUS_TIMEOUT) {
        errno = EAGAIN;
        return -1;

    } else if ((!KSUCCESS(Status)) && (Status != STATUS_END_OF_FILE)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        if (BytesCompleted == 0) {
            BytesCompleted = -1;
        }
    }

    return (ssize_t)BytesCompleted;
}

LIBC_API
int
rename (
    const char *SourcePath,
    const char *DestinationPath
    )

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

{

    return renameat(AT_FDCWD, SourcePath, AT_FDCWD, DestinationPath);
}

LIBC_API
int
renameat (
    int SourceDirectory,
    const char *SourcePath,
    int DestinationDirectory,
    const char *DestinationPath
    )

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

{

    KSTATUS Status;

    if ((SourcePath == NULL) || (DestinationPath == NULL)) {
        errno = EINVAL;
        return -1;
    }

    Status = OsRename((HANDLE)(UINTN)SourceDirectory,
                      (PSTR)SourcePath,
                      strlen(SourcePath) + 1,
                      (HANDLE)(UINTN)DestinationDirectory,
                      (PSTR)DestinationPath,
                      strlen(DestinationPath) + 1);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

LIBC_API
ssize_t
write (
    int FileDescriptor,
    const void *Buffer,
    size_t ByteCount
    )

/*++

Routine Description:

    This routine attempts to write the specifed number of bytes to the given
    open file descriptor.

Arguments:

    FileDescriptor - Supplies the file descriptor returned by the open function.

    Buffer - Supplies a pointer to the buffer containing the bytes to be
        written.

    ByteCount - Supplies the number of bytes to write.

Return Value:

    Returns the number of bytes successfully written to the file.

    -1 on failure, and errno will contain more information.

--*/

{

    UINTN BytesCompleted;
    KSTATUS Status;

    //
    // Truncate the byte count, so that it does not exceed the maximum number
    // of bytes that can be returned.
    //

    if (ByteCount > (size_t)SSIZE_MAX) {
        ByteCount = (size_t)SSIZE_MAX;
    }

    //
    // Ask the OS to actually do the I/O.
    //

    Status = OsPerformIo((HANDLE)(UINTN)FileDescriptor,
                         IO_OFFSET_NONE,
                         ByteCount,
                         SYS_IO_FLAG_WRITE,
                         SYS_WAIT_TIME_INDEFINITE,
                         (PVOID)Buffer,
                         &BytesCompleted);

    if (Status == STATUS_TIMEOUT) {
        errno = EAGAIN;
        return -1;

    } else if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        BytesCompleted = -1;
    }

    return (ssize_t)BytesCompleted;
}

LIBC_API
ssize_t
pwrite (
    int FileDescriptor,
    const void *Buffer,
    size_t ByteCount,
    off_t Offset
    )

/*++

Routine Description:

    This routine attempts to write the specifed number of bytes to the given
    open file descriptor at a given offset. It does not update the current
    file position.

Arguments:

    FileDescriptor - Supplies the file descriptor returned by the open function.

    Buffer - Supplies a pointer to the buffer containing the bytes to be
        written.

    ByteCount - Supplies the number of bytes to write.

    Offset - Supplies the offset from the start of the file to write to.

Return Value:

    Returns the number of bytes successfully written to the file.

    -1 on failure, and errno will contain more information.

--*/

{

    UINTN BytesCompleted;
    KSTATUS Status;

    //
    // Truncate the byte count, so that it does not exceed the maximum number
    // of bytes that can be returned.
    //

    if (ByteCount > (size_t)SSIZE_MAX) {
        ByteCount = (size_t)SSIZE_MAX;
    }

    //
    // Ask the OS to actually do the I/O.
    //

    Status = OsPerformIo((HANDLE)(UINTN)FileDescriptor,
                         Offset,
                         ByteCount,
                         SYS_IO_FLAG_WRITE,
                         SYS_WAIT_TIME_INDEFINITE,
                         (PVOID)Buffer,
                         &BytesCompleted);

    if (Status == STATUS_TIMEOUT) {
        errno = EAGAIN;
        return -1;

    } else if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        BytesCompleted = -1;
    }

    return (ssize_t)BytesCompleted;
}

LIBC_API
int
fsync (
    int FileDescriptor
    )

/*++

Routine Description:

    This routine flushes all the data associated with the open file descriptor
    to its corresponding backing device. It does not return until the data has
    been flushed.

Arguments:

    FileDescriptor - Supplies the file descriptor returned by the open function.

Return Value:

    0 on success.

    -1 on failure, and the errno variable will be set to contain more
    information.

--*/

{

    KSTATUS Status;

    Status = OsFlush((HANDLE)(UINTN)FileDescriptor, 0);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

LIBC_API
int
fdatasync (
    int FileDescriptor
    )

/*++

Routine Description:

    This routine flushes all the data associated with the open file descriptor
    to its corresponding backing device. It does not return until the data has
    been flushed. It is similar to fsync but does not flush modified metadata
    if that metadata is unimportant to retrieving the file later. For example,
    last access and modified times wouldn't require a metadata flush, but file
    size change would.

Arguments:

    FileDescriptor - Supplies the file descriptor returned by the open function.

Return Value:

    0 on success.

    -1 on failure, and the errno variable will be set to contain more
    information.

--*/

{

    //
    // For now, there is no actual distinction between this function and fsync.
    //

    return fsync(FileDescriptor);
}

LIBC_API
void
sync (
    void
    )

/*++

Routine Description:

    This routine schedules a flush for all file system related data that is in
    memory. Upon return, it is not guaranteed that the writing of the data is
    complete.

Arguments:

    None.

Return Value:

    None.

--*/

{

    OsFlush(INVALID_HANDLE, SYS_FLUSH_FLAG_ALL);
    return;
}

LIBC_API
off_t
lseek (
    int FileDescriptor,
    off_t Offset,
    int Whence
    )

/*++

Routine Description:

    This routine sets the file offset for the open file descriptor.

Arguments:

    FileDescriptor - Supplies the file descriptor returned by the open function.

    Offset - Supplies the offset from the reference location given in the
        Whence argument.

    Whence - Supplies the reference location to base the offset off of. Valid
        value are:

        SEEK_SET - The offset will be added to the the beginning of the file.

        SEEK_CUR - The offset will be added to the current file position.

        SEEK_END - The offset will be added to the end of the file.

Return Value:

    Returns the resulting file offset after the operation.

    -1 on failure, and errno will contain more information. The file offset
    will remain unchanged.

--*/

{

    IO_OFFSET NewOffset;
    SEEK_COMMAND SeekCommand;
    KSTATUS Status;

    switch (Whence) {
    case SEEK_SET:
        SeekCommand = SeekCommandFromBeginning;
        break;

    case SEEK_CUR:
        SeekCommand = SeekCommandFromCurrentOffset;
        break;

    case SEEK_END:
        SeekCommand = SeekCommandFromEnd;
        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        goto lseekEnd;
    }

    Status = OsSeek((HANDLE)(UINTN)FileDescriptor,
                    SeekCommand,
                    Offset,
                    &NewOffset);

    if (!KSUCCESS(Status)) {
        goto lseekEnd;
    }

lseekEnd:
    if (!KSUCCESS(Status)) {
        if (Status == STATUS_NOT_SUPPORTED) {
            errno = ESPIPE;

        } else {
            errno = ClConvertKstatusToErrorNumber(Status);
        }

        return (off_t)-1;
    }

    return (off_t)NewOffset;
}

LIBC_API
int
ftruncate (
    int FileDescriptor,
    off_t NewSize
    )

/*++

Routine Description:

    This routine sets the file size of the given file descriptor. If the new
    size is smaller than the original size, then the remaining data will be
    discarded. If the new size is larger than the original size, then the
    extra space will be filled with zeroes.

Arguments:

    FileDescriptor - Supplies the file descriptor whose size should be
        modified.

    NewSize - Supplies the new size of the file descriptor in bytes.

Return Value:

    0 on success.

    -1 on failure. The errno variable will be set to indicate the error.

--*/

{

    FILE_CONTROL_PARAMETERS_UNION Parameters;
    FILE_PROPERTIES Properties;
    KSTATUS Status;

    Properties.Size = NewSize;
    Parameters.SetFileInformation.FieldsToSet = FILE_PROPERTY_FIELD_FILE_SIZE;
    Parameters.SetFileInformation.FileProperties = &Properties;
    Status = OsFileControl((HANDLE)(UINTN)FileDescriptor,
                           FileControlCommandSetFileInformation,
                           &Parameters);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

LIBC_API
int
truncate (
    const char *Path,
    off_t NewSize
    )

/*++

Routine Description:

    This routine sets the file size of the given file path. If the new size is
    smaller than the original size, then the remaining data will be discarded.
    If the new size is larger than the original size, then the extra space will
    be filled with zeroes.

Arguments:

    Path - Supplies a pointer to a null terminated string containing the path
        of the file whose size should be changed.

    NewSize - Supplies the new size of the file descriptor in bytes.

Return Value:

    0 on success.

    -1 on failure. The errno variable will be set to indicate the error.

--*/

{

    FILE_PROPERTIES Properties;
    SET_FILE_INFORMATION Request;
    KSTATUS Status;

    Properties.Size = NewSize;
    Request.FieldsToSet = FILE_PROPERTY_FIELD_FILE_SIZE;
    Request.FileProperties = &Properties;
    Status = OsSetFileInformation(INVALID_HANDLE,
                                  (PSTR)Path,
                                  strlen(Path) + 1,
                                  TRUE,
                                  &Request);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

LIBC_API
int
pipe (
    int FileDescriptors[2]
    )

/*++

Routine Description:

    This routine creates an anonymous pipe.

Arguments:

    FileDescriptors - Supplies a pointer where handles will be returned
        representing the read and write ends of the pipe.

Return Value:

    0 on success.

    -1 on failure. The errno variable will be set to indicate the error.

--*/

{

    return pipe2(FileDescriptors, 0);
}

LIBC_API
int
pipe2 (
    int FileDescriptors[2],
    int Flags
    )

/*++

Routine Description:

    This routine creates an anonymous pipe.

Arguments:

    FileDescriptors - Supplies a pointer where handles will be returned
        representing the read and write ends of the pipe.

    Flags - Supplies a bitfield of open flags governing the behavior of the new
        descriptors. Only O_NONBLOCK and O_CLOEXEC are honored.

Return Value:

    0 on success.

    -1 on failure. The errno variable will be set to indicate the error.

--*/

{

    ULONG OpenFlags;
    ULONG Permissions;
    HANDLE ReadHandle;
    KSTATUS Status;
    HANDLE WriteHandle;

    Permissions = FILE_PERMISSION_USER_READ | FILE_PERMISSION_USER_WRITE;
    OpenFlags = 0;
    if ((Flags & O_CLOEXEC) != 0) {
        OpenFlags |= SYS_OPEN_FLAG_CLOSE_ON_EXECUTE;
    }

    if ((Flags & O_NONBLOCK) != 0) {
        OpenFlags |= SYS_OPEN_FLAG_NON_BLOCKING;
    }

    Status = OsCreatePipe(INVALID_HANDLE,
                          NULL,
                          0,
                          OpenFlags,
                          Permissions,
                          &ReadHandle,
                          &WriteHandle);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    FileDescriptors[0] = (int)(UINTN)ReadHandle;
    FileDescriptors[1] = (int)(UINTN)WriteHandle;
    return 0;
}

LIBC_API
int
symlink (
    const char *LinkTarget,
    const char *LinkName
    )

/*++

Routine Description:

    This routine creates a symbolic link with the given name pointed at the
    supplied target path.

Arguments:

    LinkTarget - Supplies the location that the link points to.

    LinkName - Supplies the path where the link should be created.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    return symlinkat(LinkTarget, AT_FDCWD, LinkName);
}

LIBC_API
int
symlinkat (
    const char *LinkTarget,
    int Directory,
    const char *LinkName
    )

/*++

Routine Description:

    This routine creates a symbolic link with the given name pointed at the
    supplied target path.

Arguments:

    LinkTarget - Supplies the location that the link points to.

    Directory - Supplies an optional file descriptor. If the given path to the
        link name is a relative path, the directory referenced by this
        descriptor will be used as a starting point for path resolution. Supply
        AT_FDCWD to use the working directory for relative paths.

    LinkName - Supplies the path where the link should be created.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    KSTATUS Status;

    Status = OsCreateSymbolicLink((HANDLE)(UINTN)Directory,
                                  (PSTR)LinkName,
                                  strlen(LinkName) + 1,
                                  (PSTR)LinkTarget,
                                  strlen(LinkTarget));

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

LIBC_API
ssize_t
readlink (
    const char *Path,
    char *LinkDestinationBuffer,
    size_t LinkDestinationBufferSize
    )

/*++

Routine Description:

    This routine reads the destination path of a symbolic link.

Arguments:

    Path - Supplies a pointer to the symbolic link path.

    LinkDestinationBuffer - Supplies a pointer to a buffer where the
        destination of the link will be returned. A null terminator is not
        written.

    LinkDestinationBufferSize - Supplies the size of the link destination
        buffer in bytes.

Return Value:

    Returns the number of bytes placed into the buffer on success.

    -1 on failure. The errno variable will be set to indicate the error, and
    the buffer will remain unchanged.

--*/

{

    ssize_t Result;

    Result = readlinkat(AT_FDCWD,
                        Path,
                        LinkDestinationBuffer,
                        LinkDestinationBufferSize);

    return Result;
}

LIBC_API
ssize_t
readlinkat (
    int Directory,
    const char *Path,
    char *LinkDestinationBuffer,
    size_t LinkDestinationBufferSize
    )

/*++

Routine Description:

    This routine reads the destination path of a symbolic link.

Arguments:

    Directory - Supplies an optional file descriptor. If the given path
        is a relative path, the directory referenced by this descriptor will
        be used as a starting point for path resolution. Supply AT_FDCWD to
        use the working directory for relative paths.

    Path - Supplies a pointer to the symbolic link path.

    LinkDestinationBuffer - Supplies a pointer to a buffer where the
        destination of the link will be returned. A null terminator is not
        written.

    LinkDestinationBufferSize - Supplies the size of the link destination
        buffer in bytes.

Return Value:

    Returns the number of bytes placed into the buffer on success.

    -1 on failure. The errno variable will be set to indicate the error, and
    the buffer will remain unchanged.

--*/

{

    ULONG LinkDestinationSize;
    KSTATUS Status;

    Status = OsReadSymbolicLink((HANDLE)(UINTN)Directory,
                                (PSTR)Path,
                                strlen(Path) + 1,
                                LinkDestinationBuffer,
                                LinkDestinationBufferSize,
                                &LinkDestinationSize);

    if (!KSUCCESS(Status)) {
        if (Status == STATUS_BUFFER_TOO_SMALL) {
            errno = ERANGE;

        } else {
            errno = ClConvertKstatusToErrorNumber(Status);
        }

        return -1;
    }

    return LinkDestinationSize;
}

LIBC_API
int
link (
    const char *ExistingFile,
    const char *LinkPath
    )

/*++

Routine Description:

    This routine creates a hard link to the given file.

Arguments:

    ExistingFile - Supplies a pointer to a null-terminated string containing
        the path to the file that already exists.

    LinkPath - Supplies a pointer to a null-terminated string containing the
        path of the new link to create.

Return Value:

    0 on success.

    -1 on failure. The errno variable will be set to indicate the error.

--*/

{

    return linkat(AT_FDCWD, ExistingFile, AT_FDCWD, LinkPath, 0);
}

LIBC_API
int
linkat (
    int ExistingFileDirectory,
    const char *ExistingFile,
    int LinkPathDirectory,
    const char *LinkPath,
    int Flags
    )

/*++

Routine Description:

    This routine creates a hard link to the given file.

Arguments:

    ExistingFileDirectory - Supplies an optional file descriptor. If the given
        existing file path is a relative path, the directory referenced by this
        descriptor will be used as a starting point for path resolution. Supply
        AT_FDCWD to use the working directory for relative paths.

    ExistingFile - Supplies a pointer to a null-terminated string containing
        the path to the file that already exists.

    LinkPathDirectory - Supplies an optional file descriptor. If the given new
        link is a relative path, the directory referenced by this descriptor
        will be used as a starting point for path resolution. Supply AT_FDCWD
        to use the working directory for relative paths.

    LinkPath - Supplies a pointer to a null-terminated string containing the
        path of the new link to create.

    Flags - Supplies AT_SYMLINK_FOLLOW if the routine should link to the
        destination of the symbolic link if the existing file path is a
        symbolic link. Supply 0 to create a link to the symbolic link itself.

Return Value:

    0 on success.

    -1 on failure. The errno variable will be set to indicate the error.

--*/

{

    BOOL FollowLinks;
    KSTATUS Status;

    FollowLinks = FALSE;
    if ((Flags & AT_SYMLINK_FOLLOW) != 0) {
        FollowLinks = TRUE;
    }

    Status = OsCreateHardLink((HANDLE)(UINTN)ExistingFileDirectory,
                              (PSTR)ExistingFile,
                              strlen(ExistingFile) + 1,
                              (HANDLE)(UINTN)LinkPathDirectory,
                              (PSTR)LinkPath,
                              strlen(LinkPath) + 1,
                              FollowLinks);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

LIBC_API
int
remove (
    const char *Path
    )

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

{

    int Result;
    struct stat Stat;

    Result = lstat(Path, &Stat);
    if (Result < 0) {
        return Result;
    }

    if (S_ISDIR(Stat.st_mode)) {
        return rmdir(Path);
    }

    return unlink(Path);
}

LIBC_API
int
unlink (
    const char *Path
    )

/*++

Routine Description:

    This routine attempts to delete the object at the given path. If the path
    points to a directory, the directory must be empty. If the path points to
    a file, the hard link count on the file is decremented. If the hard link
    count reaches zero and no processes have the file open, the contents of the
    file are destroyed. If processes have open handles to the file, the
    destruction of the file contents are deferred until the last handle to the
    old file is closed. If the path points to a symbolic link, the link itself
    is removed and not the destination. The removal of the entry from the
    directory is immediate.

Arguments:

    Path - Supplies a pointer to a null terminated string containing the path
        of the entry to remove.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to provide more details. In failure
    cases, the directory will not be removed.

--*/

{

    return unlinkat(AT_FDCWD, Path, 0);
}

LIBC_API
int
unlinkat (
    int Directory,
    const char *Path,
    int Flags
    )

/*++

Routine Description:

    This routine attempts to delete the object at the given path. If the path
    points to a directory, the directory must be empty. If the path points to
    a file, the hard link count on the file is decremented. If the hard link
    count reaches zero and no processes have the file open, the contents of the
    file are destroyed. If processes have open handles to the file, the
    destruction of the file contents are deferred until the last handle to the
    old file is closed. If the path points to a symbolic link, the link itself
    is removed and not the destination. The removal of the entry from the
    directory is immediate.

Arguments:

    Directory - Supplies an optional file descriptor. If the given path
        is a relative path, the directory referenced by this descriptor will
        be used as a starting point for path resolution. Supply AT_FDCWD to
        use the working directory for relative paths.

    Path - Supplies a pointer to a null terminated string containing the path
        of the entry to remove.

    Flags - Supplies a bitfield of flags. Supply AT_REMOVEDIR to attempt to
        remove a directory (and only a directory). Supply zero to attempt to
        remove a non-directory.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to provide more details. In failure
    cases, the directory will not be removed.

--*/

{

    ULONG OsFlags;
    KSTATUS Status;

    OsFlags = 0;
    if ((Flags & AT_REMOVEDIR) != 0) {
        OsFlags |= SYS_DELETE_FLAG_DIRECTORY;
    }

    Status = OsDelete((HANDLE)(UINTN)Directory,
                      (PSTR)Path,
                      strlen(Path) + 1,
                      OsFlags);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

LIBC_API
int
dup (
    int FileDescriptor
    )

/*++

Routine Description:

    This routine duplicates the given file descriptor.

Arguments:

    FileDescriptor - Supplies the file descriptor to duplicate.

Return Value:

    Returns the new file descriptor which represents a copy of the original
    file descriptor.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    HANDLE NewHandle;
    KSTATUS Status;

    NewHandle = INVALID_HANDLE;
    Status = OsDuplicateHandle((HANDLE)(UINTN)FileDescriptor, &NewHandle, 0);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return (int)(UINTN)NewHandle;
}

LIBC_API
int
dup2 (
    int FileDescriptor,
    int CopyDescriptor
    )

/*++

Routine Description:

    This routine duplicates the given file descriptor to the destination
    descriptor, closing the original destination descriptor file along the way.

Arguments:

    FileDescriptor - Supplies the file descriptor to duplicate.

    CopyDescriptor - Supplies the descriptor number of returned copy.

Return Value:

    Returns the new file descriptor which represents a copy of the original,
    which is also equal to the input copy descriptor parameter.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    HANDLE NewHandle;
    KSTATUS Status;

    NewHandle = (HANDLE)(UINTN)CopyDescriptor;
    if (CopyDescriptor < 0) {
        errno = EBADF;
        return -1;
    }

    Status = OsDuplicateHandle((HANDLE)(UINTN)FileDescriptor, &NewHandle, 0);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    assert(NewHandle == (HANDLE)(UINTN)CopyDescriptor);

    return (int)(UINTN)NewHandle;
}

LIBC_API
int
dup3 (
    int FileDescriptor,
    int CopyDescriptor,
    int Flags
    )

/*++

Routine Description:

    This routine duplicates the given file descriptor to the destination
    descriptor, closing the original destination descriptor file along the way.

Arguments:

    FileDescriptor - Supplies the file descriptor to duplicate.

    CopyDescriptor - Supplies the descriptor number of returned copy. If this
        is equal to the original file descriptor, then the call fails with
        EINVAL.

    Flags - Supplies O_* open flags governing the new descriptor. Only
        O_CLOEXEC is permitted.

Return Value:

    Returns the new file descriptor which represents a copy of the original,
    which is also equal to the input copy descriptor parameter.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    HANDLE NewHandle;
    ULONG OpenFlags;
    KSTATUS Status;

    if (FileDescriptor == CopyDescriptor) {
        errno = EINVAL;
        return -1;
    }

    NewHandle = (HANDLE)(UINTN)CopyDescriptor;
    OpenFlags = 0;
    if ((Flags & O_CLOEXEC) != 0) {
        OpenFlags |= SYS_OPEN_FLAG_CLOSE_ON_EXECUTE;
    }

    Status = OsDuplicateHandle((HANDLE)(UINTN)FileDescriptor,
                               &NewHandle,
                               OpenFlags);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    assert(NewHandle == (HANDLE)(UINTN)CopyDescriptor);

    return (int)(UINTN)NewHandle;
}

LIBC_API
int
lockf (
    int FileDescriptor,
    int Function,
    off_t Size
    )

/*++

Routine Description:

    This routine locks or unlocks sections of a file with advisory-mode locks.
    All locks for a process are removed when the process terminates. Record
    locking is supported at least for regular files, and may be supported for
    other file types.

Arguments:

    FileDescriptor - Supplies the file descriptor to query. To establish a
        lock, the given file descriptor must be opened with O_WRONLY or O_RDWR.

    Function - Supplies the action to be taken. Valid values are:
        F_ULOCK - Unlocks a locked section.
        F_LOCK - Locks a section for exclusive use (blocking if already locked).
        F_TLOCK - Test and lock for exclusive use, not blocking.
        F_TEST - Test for a section of locks by other processes.

    Size - Supplies the number of contiguous bytes to be locked or unlocked.
        The section to be locked or unlocked starts at the current offset in
        the file and extends forward for a positve size or backwards for a
        negative size (the preceding bytes up to but not including the current
        offset). If size is 0, the section from the current offset through the
        largest possible offset shall be locked. Locks may exist past the
        current end of file.

Return Value:

    0 on success.

    -1 on error, and errno will be set to contain more information. The errno
    variable may be set to the following values:

    EACCES or EAGAIN if the function argument is F_TLOCK or F_TEST and the
    section is already locked by another process.

    EDEADLK if the function argument is F_LOCK and a deadlock is detected.

    EINVAL if the function is valid or the size plus the current offset is less
    than zero.

    EOVERFLOW if the range cannot properly be represented in an off_t.

--*/

{

    int ControlOperation;
    struct flock Parameters;

    Parameters.l_start = 0;
    Parameters.l_len = Size;
    Parameters.l_pid = 0;
    Parameters.l_type = F_WRLCK;
    Parameters.l_whence = SEEK_CUR;
    if (Function == F_ULOCK) {
        ControlOperation = F_SETLK;
        Parameters.l_type = F_UNLCK;

    } else if (Function == F_LOCK) {
        ControlOperation = F_SETLKW;

    } else if (Function == F_TLOCK) {
        ControlOperation = F_SETLK;

    } else if (Function == F_TEST) {
        ControlOperation = F_GETLK;

    } else {
        errno = EINVAL;
        return -1;
    }

    return fcntl(FileDescriptor, ControlOperation, &Parameters);
}

LIBC_API
int
dprintf (
    int FileDescriptor,
    const char *Format,
    ...
    )

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

{

    va_list Arguments;
    int Result;

    va_start(Arguments, Format);
    Result = vdprintf(FileDescriptor, Format, Arguments);
    va_end(Arguments);
    return Result;
}

LIBC_API
int
vdprintf (
    int FileDescriptor,
    const char *Format,
    va_list Arguments
    )

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

{

    int Result;
    PSTR String;
    size_t TotalWritten;
    ssize_t Written;

    String = NULL;
    Result = vasprintf(&String, Format, Arguments);
    if (Result <= 0) {
        return Result;
    }

    TotalWritten = 0;
    while (TotalWritten < Result) {
        do {
            Written = write(FileDescriptor,
                            String + TotalWritten,
                            Result - TotalWritten);

        } while ((Written < 0) && (errno == EINTR));

        if (Written <= 0) {
            Result = Written;
            break;
        }

        TotalWritten += Written;
    }

    free(String);
    return Result;
}

LIBC_API
int
sprintf (
    char *OutputString,
    const char *Format,
    ...
    )

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

{

    va_list Arguments;
    int Result;

    va_start(Arguments, Format);
    Result = vsprintf(OutputString, Format, Arguments);
    va_end(Arguments);
    return Result;
}

LIBC_API
int
snprintf (
    char *OutputString,
    size_t OutputStringSize,
    const char *Format,
    ...
    )

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

{

    va_list Arguments;
    int Result;

    va_start(Arguments, Format);
    Result = vsnprintf(OutputString, OutputStringSize, Format, Arguments);
    va_end(Arguments);
    return Result;
}

LIBC_API
int
vsnprintf (
    char *OutputString,
    size_t OutputStringSize,
    const char *Format,
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

    Returns the number of bytes that would have been converted had
    OutputStringSize been large enough, not including the null terminator.

    Returns a negative number if an error was encountered.

--*/

{

    ULONG Result;

    Result = RtlFormatString(OutputString,
                             OutputStringSize,
                             CharacterEncodingDefault,
                             (PSTR)Format,
                             Arguments);

    return Result - 1;
}

LIBC_API
int
vsprintf (
    char *OutputString,
    const char *Format,
    va_list Arguments
    )

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

{

    return vsnprintf(OutputString, MAX_LONG, Format, Arguments);
}

LIBC_API
int
asprintf (
    char **OutputString,
    const char *Format,
    ...
    )

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

{

    va_list Arguments;
    int Status;

    va_start(Arguments, Format);
    Status = vasprintf(OutputString, Format, Arguments);
    va_end(Arguments);
    return Status;
}

LIBC_API
int
vasprintf (
    char **OutputString,
    const char *Format,
    va_list Arguments
    )

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

{

    ASPRINT_CONTEXT AsContext;
    PRINT_FORMAT_CONTEXT PrintContext;

    *OutputString = NULL;
    memset(&PrintContext, 0, sizeof(PRINT_FORMAT_CONTEXT));
    PrintContext.Context = &AsContext;
    PrintContext.WriteCharacter = ClpAsPrintWriteCharacter;
    RtlInitializeMultibyteState(&(PrintContext.State),
                                CharacterEncodingDefault);

    AsContext.Buffer = malloc(ASPRINT_INITIAL_BUFFER_SIZE);
    if (AsContext.Buffer == NULL) {
        return -1;
    }

    AsContext.Size = 0;
    AsContext.Capacity = ASPRINT_INITIAL_BUFFER_SIZE;
    RtlFormat(&PrintContext, (PSTR)Format, Arguments);
    if (AsContext.Buffer == NULL) {
        return -1;
    }

    ASSERT(AsContext.Size < AsContext.Capacity);

    AsContext.Buffer[AsContext.Size] = '\0';
    *OutputString = AsContext.Buffer;
    return PrintContext.CharactersWritten;
}

LIBC_API
int
poll (
    struct pollfd PollDescriptors[],
    nfds_t DescriptorCount,
    int Timeout
    )

/*++

Routine Description:

    This routine blocks waiting for specified activity on a range of file
    descriptors.

Arguments:

    PollDescriptors - Supplies an array of poll descriptor structures,
        indicating which descriptors should be waited on and which events
        should qualify in each descriptor.

    DescriptorCount - Supplies the number of descriptors in the array.

    Timeout - Supplies the amount of time in milliseconds to block before
        giving up and returning anyway. Supply 0 to not block at all, and
        supply -1 to wait for an indefinite amount of time. The timeout will be
        at least as long as supplied, but may also be rounded up.

Return Value:

    Returns a positive number to indicate success and the number of file
    descriptors that had events occur.

    Returns 0 to indicate a timeout.

    Returns -1 to indicate an error, and errno will be set to contain more
    information.

--*/

{

    struct timespec *TimeoutPointer;
    struct timespec Timespec;

    if (Timeout >= 0) {
        Timespec.tv_sec = Timeout / MILLISECONDS_PER_SECOND;
        Timespec.tv_nsec = (Timeout % MILLISECONDS_PER_SECOND) *
                           NANOSECONDS_PER_MILLISECOND;

        TimeoutPointer = &Timespec;

    } else {
        TimeoutPointer = NULL;
    }

    return ppoll(PollDescriptors, DescriptorCount, TimeoutPointer, NULL);
}

LIBC_API
int
ppoll (
    struct pollfd PollDescriptors[],
    nfds_t DescriptorCount,
    const struct timespec *Timeout,
    const sigset_t *SignalMask
    )

/*++

Routine Description:

    This routine blocks waiting for specified activity on a range of file
    descriptors.

Arguments:

    PollDescriptors - Supplies an array of poll descriptor structures,
        indicating which descriptors should be waited on and which events
        should qualify in each descriptor.

    DescriptorCount - Supplies the number of descriptors in the array.

    Timeout - Supplies the amount of time to block before giving up and
        returning anyway. Supply 0 to not block at all, and supply -1 to wait
        for an indefinite amount of time. The timeout will be at least as long
        as supplied, but may also be rounded up.

    SignalMask - Supplies an optional pointer to a signal mask to set
        atomically for the duration of the wait.

Return Value:

    Returns a positive number to indicate success and the number of file
    descriptors that had events occur.

    Returns 0 to indicate a timeout.

    Returns -1 to indicate an error, and errno will be set to contain more
    information.

--*/

{

    ULONG DescriptorsSelected;
    INT Result;
    KSTATUS Status;
    ULONG TimeoutMilliseconds;

    if (DescriptorCount > sysconf(_SC_OPEN_MAX)) {
        errno = EINVAL;
        return -1;
    }

    ASSERT_POLL_FLAGS_EQUIVALENT();
    ASSERT_POLL_STRUCTURE_EQUIVALENT();

    Result = ClpConvertSpecificTimeoutToSystemTimeout(Timeout,
                                                      &TimeoutMilliseconds);

    if (Result != 0) {
        errno = Result;
        return -1;
    }

    //
    // Perform the actual poll call, and return if failure is received.
    //

    Status = OsPoll((PSIGNAL_SET)SignalMask,
                    (PPOLL_DESCRIPTOR)PollDescriptors,
                    DescriptorCount,
                    TimeoutMilliseconds,
                    &DescriptorsSelected);

    if (!KSUCCESS(Status)) {
        goto pollEnd;
    }

pollEnd:
    if ((!KSUCCESS(Status)) && (Status != STATUS_TIMEOUT)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return (int)DescriptorsSelected;
}

LIBC_API
int
select (
    int DescriptorCount,
    fd_set *ReadDescriptors,
    fd_set *WriteDescriptors,
    fd_set *ErrorDescriptors,
    struct timeval *Timeout
    )

/*++

Routine Description:

    This routine indicates which of the specified file descriptors are ready
    for reading, writing, and have error conditions.

Arguments:

    DescriptorCount - Supplies the range of file descriptors to be tested.
        This routine tests file descriptors in the range of 0 to the descriptor
        count - 1.

    ReadDescriptors - Supplies an optional pointer to a set of descriptors that
        on input supplies the set of descriptors to be checked for reading. On
        output, contains the set of descriptors that are ready to be read.

    WriteDescriptors - Supplies an optional pointer to a set of descriptors that
        on input supplies the set of descriptors to be checked for writing. On
        output, contains the set of descriptors that are ready to be written to.

    ErrorDescriptors - Supplies an optional pointer to a set of descriptors that
        on input supplies the set of descriptors to be checked for errors. On
        output, contains the set of descriptors that have errors.

    Timeout - Supplies an optional to a structure that defines how long to wait
        for one or more of the descriptors to become ready. If all members of
        this structure are 0, the function will not block. If this argument is
        not supplied, the function will block indefinitely until one of the
        events is ready. If all three descriptor structure pointers is null,
        this routine will block for the specified amount of time and then
        return.

Return Value:

    On success, returns the total number of bits set in the resulting bitmaps.

    0 if the timeout expired.

    -1 on error, and errno will be set to contain more information.

--*/

{

    int Result;
    struct timespec *TimeoutPointer;
    struct timespec Timespec;

    if (Timeout != NULL) {
        Timespec.tv_sec = Timeout->tv_sec;
        Timespec.tv_nsec = Timeout->tv_usec * NANOSECONDS_PER_MICROSECOND;
        TimeoutPointer = &Timespec;

    } else {
        TimeoutPointer = NULL;
    }

    Result = pselect(DescriptorCount,
                     ReadDescriptors,
                     WriteDescriptors,
                     ErrorDescriptors,
                     TimeoutPointer,
                     NULL);

    return Result;
}

LIBC_API
int
pselect (
    int DescriptorCount,
    fd_set *ReadDescriptors,
    fd_set *WriteDescriptors,
    fd_set *ErrorDescriptors,
    const struct timespec *Timeout,
    const sigset_t *SignalMask
    )

/*++

Routine Description:

    This routine indicates which of the specified file descriptors are ready
    for reading, writing, and have error conditions.

Arguments:

    DescriptorCount - Supplies the range of file descriptors to be tested.
        This routine tests file descriptors in the range of 0 to the descriptor
        count - 1.

    ReadDescriptors - Supplies an optional pointer to a set of descriptors that
        on input supplies the set of descriptors to be checked for reading. On
        output, contains the set of descriptors that are ready to be read.

    WriteDescriptors - Supplies an optional pointer to a set of descriptors that
        on input supplies the set of descriptors to be checked for writing. On
        output, contains the set of descriptors that are ready to be written to.

    ErrorDescriptors - Supplies an optional pointer to a set of descriptors that
        on input supplies the set of descriptors to be checked for errors. On
        output, contains the set of descriptors that have errors.

    Timeout - Supplies an optional to a structure that defines how long to wait
        for one or more of the descriptors to become ready. If all members of
        this structure are 0, the function will not block. If this argument is
        not supplied, the function will block indefinitely until one of the
        events is ready. If all three descriptor structure pointers is null,
        this routine will block for the specified amount of time and then
        return.

    SignalMask - Supplies an optional pointer to the signal mask to set for
        the duration of the wait.

Return Value:

    On success, returns the total number of bits set in the resulting bitmaps.

    0 if the timeout expired.

    -1 on error, and errno will be set to contain more information.

--*/

{

    ULONG ArrayIndex;
    INT BitCount;
    PPOLL_DESCRIPTOR Descriptor;
    ULONG DescriptorIndex;
    PPOLL_DESCRIPTOR Descriptors;
    ULONG DescriptorsSelected;
    ULONG Events;
    ULONG PollIndex;
    INT Result;
    KSTATUS Status;
    ULONG TimeoutInMilliseconds;

    if (DescriptorCount < 0) {
        errno = EINVAL;
        return -1;
    }

    Result = ClpConvertSpecificTimeoutToSystemTimeout(Timeout,
                                                      &TimeoutInMilliseconds);

    if (Result != 0) {
        errno = Result;
        return -1;
    }

    Descriptors = NULL;
    DescriptorsSelected = 0;
    if (DescriptorCount > FD_SETSIZE) {
        DescriptorCount = FD_SETSIZE;
    }

    Descriptors = alloca(sizeof(POLL_DESCRIPTOR) * DescriptorCount);
    if (Descriptors == NULL) {
        errno = ENOMEM;
        return -1;
    }

    //
    // Fill out the new poll descriptors.
    //

    ArrayIndex = 0;
    for (DescriptorIndex = 0;
         DescriptorIndex < DescriptorCount;
         DescriptorIndex += 1) {

        Events = 0;
        if ((ReadDescriptors != NULL) &&
            (FD_ISSET(DescriptorIndex, ReadDescriptors) != FALSE)) {

            Events |= POLL_EVENT_IN;
        }

        if ((WriteDescriptors != NULL) &&
            (FD_ISSET(DescriptorIndex, WriteDescriptors) != FALSE)) {

            Events |= POLL_EVENT_OUT;
        }

        if ((ErrorDescriptors != NULL) &&
            (FD_ISSET(DescriptorIndex, ErrorDescriptors) != FALSE)) {

            Events |= POLL_EVENT_ERROR;
        }

        if (Events == 0) {
            continue;
        }

        Descriptor = &(Descriptors[ArrayIndex]);
        ArrayIndex += 1;
        Descriptor->Handle = (HANDLE)(UINTN)DescriptorIndex;
        Descriptor->Events = Events;
        Descriptor->ReturnedEvents = 0;
    }

    //
    // Peform the poll.
    //

    Status = OsPoll((PSIGNAL_SET)SignalMask,
                    Descriptors,
                    ArrayIndex,
                    TimeoutInMilliseconds,
                    &DescriptorsSelected);

    if ((!KSUCCESS(Status)) && (Status != STATUS_TIMEOUT)) {
        goto pselectEnd;
    }

    //
    // Check for invalid handles, since POSIX says on failure the bitmasks
    // should not be modified.
    //

    for (PollIndex = 0; PollIndex < ArrayIndex; PollIndex += 1) {
        if ((Descriptors[PollIndex].ReturnedEvents &
             POLL_EVENT_INVALID_HANDLE) != 0) {

            Status = STATUS_INVALID_HANDLE;
            goto pselectEnd;
        }
    }

    //
    // Go back and mark all the descriptors in the set that had events. Loop
    // over the poll events this time to skip the empty regions of the bitmasks.
    //

    BitCount = 0;
    Descriptor = &(Descriptors[0]);
    for (PollIndex = 0; PollIndex < ArrayIndex; PollIndex += 1) {
        Events = Descriptor->ReturnedEvents;
        DescriptorIndex = (UINTN)(Descriptor->Handle);

        ASSERT(DescriptorIndex < DescriptorCount);

        //
        // If the caller didn't want error events but one fired, set the in and
        // out events to force them to take action.
        //

        if (((Events & POLL_ERROR_EVENTS) != 0) && (ErrorDescriptors == NULL)) {
            Events |= POLL_EVENT_IN | POLL_EVENT_OUT;
        }

        if ((ReadDescriptors != NULL) &&
            (FD_ISSET(DescriptorIndex, ReadDescriptors) != FALSE)) {

            if ((Events & POLL_EVENT_IN) == 0) {
                FD_CLR(DescriptorIndex, ReadDescriptors);

            } else {
                BitCount += 1;
            }
        }

        if ((WriteDescriptors != NULL) &&
            (FD_ISSET(DescriptorIndex, WriteDescriptors) != FALSE)) {

            if ((Events & POLL_EVENT_OUT) == 0) {
                FD_CLR(DescriptorIndex, WriteDescriptors);

            } else {
                BitCount += 1;
            }
        }

        //
        // Errors work a little differently, if it's supplied then the bits get
        // set whether they were asked for or not.
        //

        if (ErrorDescriptors != NULL) {
            if ((Events & POLL_NONMASKABLE_EVENTS) != 0) {
                FD_SET(DescriptorIndex, ErrorDescriptors);
                BitCount += 1;

            } else {
                FD_CLR(DescriptorIndex, ErrorDescriptors);
            }
        }

        Descriptor += 1;
    }

pselectEnd:
    if (!KSUCCESS(Status)) {
        if (Status != STATUS_TIMEOUT) {
            errno = ClConvertKstatusToErrorNumber(Status);
            return -1;
        }
    }

    return BitCount;
}

LIBC_API
char *
ttyname (
    int FileDescriptor
    )

/*++

Routine Description:

    This routine returns the null-terminated pathname of the terminal
    associated with the given file descriptor. This function is neither
    reentrant nor thread safe.

Arguments:

    FileDescriptor - Supplies the file descriptor to query.

Return Value:

    Returns a pointer to the supplied buffer on success. This buffer may be
    overwritten by subsequent calls to this routine.

    NULL on failure and errno will be set to contain more information. Common
    error values are:

    EBADF if the file descriptor is not valid.

    ENOTTY if the file descriptor is not a terminal.

    ENOMEM if not enough memory was available.

--*/

{

    char *NewBuffer;
    size_t NewBufferSize;
    int OldError;
    char *Result;

    if (ClTerminalNameBufferSize == 0) {
        ClTerminalNameBuffer = malloc(INITIAL_TERMINAL_NAME_BUFFER_SIZE);
        if (ClTerminalNameBuffer == NULL) {
            errno = ENOMEM;
            return NULL;
        }

        ClTerminalNameBufferSize = INITIAL_TERMINAL_NAME_BUFFER_SIZE;
    }

    OldError = errno;
    while (TRUE) {
        Result = ttyname_r(FileDescriptor,
                           ClTerminalNameBuffer,
                           ClTerminalNameBufferSize);

        if ((Result != NULL) || (errno != ERANGE)) {
            break;
        }

        errno = OldError;
        NewBufferSize = ClTerminalNameBufferSize * 2;
        NewBuffer = realloc(ClTerminalNameBuffer, NewBufferSize);
        if (NewBuffer == NULL) {
            errno = ENOMEM;
            return NULL;
        }

        ClTerminalNameBuffer = NewBuffer;
        ClTerminalNameBufferSize = NewBufferSize;
    }

    return Result;
}

LIBC_API
char *
ttyname_r (
    int FileDescriptor,
    char *Name,
    size_t NameSize
    )

/*++

Routine Description:

    This routine returns the null-terminated pathname of the terminal
    associated with the given file descriptor.

Arguments:

    FileDescriptor - Supplies the file descriptor to query.

    Name - Supplies a pointer to the buffer where the name will be returned
        on success.

    NameSize - Supplies the size of the name buffer in bytes.

Return Value:

    Returns a pointer to the supplied buffer on success.

    NULL on failure and errno will be set to contain more information. Common
    error values are:

    EBADF if the file descriptor is not valid.

    ENOTTY if the file descriptor is not a terminal.

    ERANGE if the supplied buffer was not large enough.

--*/

{

    UINTN Size;
    KSTATUS Status;

    if (isatty(FileDescriptor) == 0) {
        errno = ENOTTY;
        return NULL;
    }

    Size = NameSize;
    Status = OsGetFilePath((HANDLE)(UINTN)FileDescriptor, Name, &Size);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return NULL;
    }

    return Name;
}

LIBC_API
char *
ctermid (
    char *Buffer
    )

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

{

    if (ClTerminalIdBuffer == NULL) {
        ClTerminalIdBuffer = malloc(L_ctermid);
        if (ClTerminalIdBuffer == NULL) {
            errno = ENOMEM;
            return NULL;
        }
    }

    return ctermid_r(ClTerminalIdBuffer);
}

LIBC_API
char *
ctermid_r (
    char *Buffer
    )

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

{

    if (Buffer == NULL) {
        return NULL;
    }

    snprintf(Buffer, L_ctermid, _PATH_TTY);
    return Buffer;
}

LIBC_API
int
ioctl (
    int FileDescriptor,
    int Request,
    ...
    )

/*++

Routine Description:

    This routine sends an I/O control request to the given file descriptor.

Arguments:

    FileDescriptor - Supplies the file descriptor to send the control request
        to.

    Request - Supplies the numeric request to send. This is device-specific.

    ... - Supplies a variable number of arguments for historical reasons, but
        this routine expects there to be exactly one more argument, a pointer
        to memory. The size of this memory is request-specific, but can be no
        larger than 4096 bytes. The native version of this routine can specify
        larger values.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    void *Argument;
    va_list ArgumentList;
    KSTATUS Status;

    va_start(ArgumentList, Request);
    Argument = va_arg(ArgumentList, void *);
    va_end(ArgumentList);
    Status = OsUserControl((HANDLE)(UINTN)FileDescriptor,
                           Request,
                           Argument,
                           4096);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

int
ClpOpen (
    int Directory,
    const char *Path,
    int OpenFlags,
    va_list ArgumentList
    )

/*++

Routine Description:

    This routine opens a file and connects it to a file descriptor.

Arguments:

    Directory - Supplies an optional file descriptor. If the given path
        is a relative path, the directory referenced by this descriptor will
        be used as a starting point for path resolution. Supply AT_FDCWD to
        use the working directory for relative paths. This is normally the
        expected behavior.

    Path - Supplies a pointer to a null terminated string containing the path
        of the file to open.

    OpenFlags - Supplies a set of flags ORed together. See O_* definitions.

    ArgumentList - Supplies the variadic arguments to the open call.

Return Value:

    Returns a file descriptor on success.

    -1 on failure. The errno variable will be set to indicate the error.

--*/

{

    mode_t CreateMode;
    FILE_PERMISSIONS CreatePermissions;
    HANDLE FileHandle;
    ULONG OsOpenFlags;
    ULONG PathLength;
    KSTATUS Status;

    if (Path == NULL) {
        errno = EINVAL;
        return -1;
    }

    PathLength = RtlStringLength((PSTR)Path) + 1;
    OsOpenFlags = 0;
    CreatePermissions = 0;

    //
    // This assert stands for not just the openat call, but for all the *at
    // calls out there that rely on this assumption.
    //

    assert(INVALID_HANDLE == (HANDLE)AT_FDCWD);

    //
    // Set the access mask.
    //

    switch (OpenFlags & O_ACCMODE) {
    case O_RDONLY:
        OsOpenFlags |= SYS_OPEN_FLAG_READ;
        break;

    case O_WRONLY:
        OsOpenFlags |= SYS_OPEN_FLAG_WRITE;
        break;

    case O_RDWR:
        OsOpenFlags |= SYS_OPEN_FLAG_READ | SYS_OPEN_FLAG_WRITE;
        break;

    default:
        break;
    }

    assert(O_EXEC == O_SEARCH);

    if ((OpenFlags & O_EXEC) != 0) {
        OsOpenFlags |= SYS_OPEN_FLAG_EXECUTE;
    }

    if ((OpenFlags & O_TRUNC) != 0) {
        OsOpenFlags |= SYS_OPEN_FLAG_TRUNCATE;
    }

    if ((OpenFlags & O_APPEND) != 0) {
        OsOpenFlags |= SYS_OPEN_FLAG_APPEND;
    }

    if ((OpenFlags & O_NONBLOCK) != 0) {
        OsOpenFlags |= SYS_OPEN_FLAG_NON_BLOCKING;
    }

    if ((OpenFlags & O_DIRECTORY) != 0) {
        OsOpenFlags |= SYS_OPEN_FLAG_DIRECTORY;
    }

    if ((OpenFlags & O_NOFOLLOW) != 0) {
        OsOpenFlags |= SYS_OPEN_FLAG_NO_SYMBOLIC_LINK;
    }

    if ((OpenFlags & O_NOATIME) != 0) {
        OsOpenFlags |= SYS_OPEN_FLAG_NO_ACCESS_TIME;
    }

    assert((O_SYNC == O_DSYNC) && (O_SYNC == O_RSYNC));

    if ((OpenFlags & O_SYNC) != 0) {
        OsOpenFlags |= SYS_OPEN_FLAG_SYNCHRONIZED;
    }

    if ((OpenFlags & O_NOCTTY) != 0) {
        OsOpenFlags |= SYS_OPEN_FLAG_NO_CONTROLLING_TERMINAL;
    }

    if ((OpenFlags & O_CLOEXEC) != 0) {
        OsOpenFlags |= SYS_OPEN_FLAG_CLOSE_ON_EXECUTE;
    }

    //
    // O_PATH is equivalent to opening with no access.
    //

    if ((OpenFlags & O_PATH) != 0) {
        OsOpenFlags &= ~(SYS_OPEN_FLAG_READ | SYS_OPEN_FLAG_WRITE |
                         SYS_OPEN_FLAG_EXECUTE);
    }

    if ((OpenFlags & O_ASYNC) != 0) {
        OsOpenFlags |= SYS_OPEN_FLAG_ASYNCHRONOUS;
    }

    //
    // Set other flags.
    //

    if ((OpenFlags & O_CREAT) != 0) {
        OsOpenFlags |= SYS_OPEN_FLAG_CREATE;
        if ((OpenFlags & O_EXCL) != 0) {
            OsOpenFlags |= SYS_OPEN_FLAG_FAIL_IF_EXISTS;
        }

        CreateMode = va_arg(ArgumentList, mode_t);

        ASSERT_FILE_PERMISSIONS_EQUIVALENT();

        CreatePermissions = CreateMode;
    }

    Status = OsOpen((HANDLE)(UINTN)Directory,
                    (PSTR)Path,
                    PathLength,
                    OsOpenFlags,
                    CreatePermissions,
                    &FileHandle);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return (int)(UINTN)FileHandle;
}

BOOL
ClpAsPrintWriteCharacter (
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

    PASPRINT_CONTEXT AsContext;
    PSTR NewBuffer;
    UINTN NewCapacity;

    AsContext = Context->Context;

    //
    // Reallocate the buffer if needed.
    //

    if (AsContext->Size + 1 >= AsContext->Capacity) {
        NewCapacity = AsContext->Capacity * 2;
        NewBuffer = NULL;
        if (NewCapacity > AsContext->Capacity) {
            NewBuffer = realloc(AsContext->Buffer, NewCapacity);
        }

        if (NewBuffer == NULL) {
            free(AsContext->Buffer);
            AsContext->Buffer = NULL;
            return FALSE;
        }

        AsContext->Buffer = NewBuffer;
        AsContext->Capacity = NewCapacity;
    }

    AsContext->Buffer[AsContext->Size] = Character;
    AsContext->Size += 1;
    return TRUE;
}

