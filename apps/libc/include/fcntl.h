/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    fcntl.h

Abstract:

    This header contains definitions for file control operations.

Author:

    Evan Green 14-Mar-2013

--*/

#ifndef _FCNTL_H
#define _FCNTL_H

//
// ------------------------------------------------------------------- Includes
//

#include <sys/stat.h>
#include <unistd.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define file control commands.
//

//
// Duplicate a file descriptor.
//

#define F_DUPFD 1

//
// Get file descriptor flags.
//

#define F_GETFD 2

//
// Set file descriptor flags.
//

#define F_SETFD 3

//
// Get status flags and file access modes.
//

#define F_GETFL 4

//
// Set status flags.
//

#define F_SETFL 5

//
// Get record locking information.
//

#define F_GETLK 6

//
// Set record locking information.
//

#define F_SETLK 7

//
// Set record locking information, wait if blocked.
//

#define F_SETLKW 8

//
// Get process or process group ID to receive SIGURG signals.
//

#define F_GETOWN 9

//
// Set process or process group ID to receive SIGURG signals.
//

#define F_SETOWN 10

//
// Close all file descriptors greater than or equal to the given value.
//

#define F_CLOSEM 11

//
// There's no need for 64-bit versions, since off_t is always 64 bits.
//

#define F_GETLK64 F_GETLK
#define F_SETLK64 F_SETLK
#define F_SETLKW64 F_SETLKW

//
// There is no struct flock64, so just define it to be the same as the regular
// structure.
//

#define flock64 flock

//
// Define file creation flags for the open call.
//

//
// Open the file for reading only.
//

#define O_RDONLY 0x00000001

//
// Open the flag for writing only.
//

#define O_WRONLY 0x00000002

//
// Open the flag for reading and writing.
//

#define O_RDWR (O_RDONLY | O_WRONLY)

//
// Define the access mode mask.
//

#define O_ACCMODE (O_RDONLY | O_WRONLY | O_RDWR)

//
// Set this flag to have all writes append to the end of the file.
//

#define O_APPEND 0x00000008

//
// Set this flag to open the file with execute permissions.
//

#define O_EXEC 0x00000010

//
// Set this flag to open a directory for search only (meaning no reads, but
// it can be used with the at* functions).
//

#define O_SEARCH O_EXEC

//
// Set this flag to open a directory. If the given path does not resolve to
// a directory, then an open attempt fails.
//

#define O_DIRECTORY 0x00000020

//
// Set this flag to fail if the path names a symbolic link. Symbolic links in
// earlier components of the path will still be followed.
//

#define O_NOFOLLOW 0x00000040

//
// Set this flag to cause all I/O to cause writes to be sent down to the
// underlying hardware immediately. When the write function returns, the data
// will be in the hands of the hardware.
//

#define O_SYNC 0x00000080
#define O_DSYNC 0x00000080
#define O_RSYNC 0x00000080

//
// Set this flag to create the file if it doesn't exist.
//

#define O_CREAT 0x00000100

//
// Set this flag if the file should be truncated to a zero size when opened.
//

#define O_TRUNC 0x00000200

//
// Set this flag to create the file exclusively (fail if the file exists).
//

#define O_EXCL 0x00000400

//
// Set this flag if when opening a terminal device, the terminal should not
// become the processes controlling terminal.
//

#define O_NOCTTY 0x00000800

//
// Set this flag to use non-blocking mode, meaning I/O operations return
// immediately if no I/O can be performed at the time of the call.
//

#define O_NONBLOCK 0x00001000
#define O_NDELAY O_NONBLOCK

//
// Set this flag to avoid updating the access time of the file when it is read.
//

#define O_NOATIME 0x00002000

//
// Set this flag to have the handle be automatically closed when an exec
// function is called.
//

#define O_CLOEXEC 0x00004000

//
// Set this flag to open the handle only for path traversal, and with no
// read or write access.
//

#define O_PATH 0x00008000

//
// Set this flag to open the file with asynchronous mode. Note that
// fcntl(F_SETOWN) still needs to be called to fully enable asynchronous mode.
//

#define O_ASYNC 0x00010000
#define FASYNC O_ASYNC

//
// Set this flag to enable opening files whose offsets cannot be described in
// off_t types but can be described in off64_t. Since off_t is always 64-bits,
// this flag is ignored and the definition is provided only for compatibility
// with older operating systems.
//

#define O_LARGEFILE 0x0000

//
// Define file descriptor flags.
//

//
// This flag is set if the file descriptor is closed when a new image is
// executed.
//

#define FD_CLOEXEC 0x0001

//
// Define file lock types.
//

//
// Read locks block write locks, but do not block other read locks.
//

#define F_RDLCK 1

//
// Write locks block any other lock on that portion of the file.
//

#define F_WRLCK 2

//
// The unlock value is used to release a record lock on a region.
//

#define F_UNLCK 3

//
// Supply this value to the at* functions to use the current working directory
// for relative paths (the same behavior as the non-at equivalents).
//

#define AT_FDCWD (-1)

//
// Set this flag to get information for a symbolic link itself, and not the
// destination of the symbolic link.
//

#define AT_SYMLINK_NOFOLLOW 0x00000001

//
// Set this flag to follow a symbolic link.
//

#define AT_SYMLINK_FOLLOW   0x00000002

//
// Set this flag in the faccessat function to use the effective user and group
// IDs for permission checking rather than the real user and group IDs.
//

#define AT_EACCESS 0x00000004

//
// Set this flag in the unlinkat function to attempt to remove a directory.
//

#define AT_REMOVEDIR 0x00000008

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores information about an advisory file record lock.

Members:

    l_start - Stores the starting offset within the file of the record lock.

    l_len - Stores the length of the record lock, in bytes. A value of zero
        indicates the record extends to the end of the file.

    l_pid - Stores the identifier of the process that owns the lock. This is
        filled in by the get lock operation, and ignored when creating a lock.

    l_type - Stores the type of lock. Valid values are F_RDLCK, F_WRLCK, and
        F_UNLCK.

    l_whence - Stores the SEEK_* parameter that defines the origin of the
        offset. This is always SEEK_SET when lock information is returned.

--*/

struct flock {
    off_t l_start;
    off_t l_len;
    pid_t l_pid;
    short l_type;
    short l_whence;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
open (
    const char *Path,
    int OpenFlags,
    ...
    );

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

LIBC_API
int
openat (
    int Directory,
    const char *Path,
    int OpenFlags,
    ...
    );

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

LIBC_API
int
fcntl (
    int FileDescriptor,
    int Command,
    ...
    );

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

#ifdef __cplusplus

}

#endif
#endif

