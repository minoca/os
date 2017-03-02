/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    stat.h

Abstract:

    This header contains standard file status definitions.

Author:

    Evan Green 17-Jun-2013

--*/

#ifndef _STAT_H
#define _STAT_H

//
// ------------------------------------------------------------------- Includes
//

#include <sys/types.h>
#include <time.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro returns non-zero if the given mode bits are set for a block
// special file.
//

#define S_ISBLK(_Mode) (((_Mode) & S_IFMT) == S_IFBLK)

//
// This macro returns non-zero if the given mode bits are set for a character
// special file.
//

#define S_ISCHR(_Mode) (((_Mode) & S_IFMT) == S_IFCHR)

//
// This macro returns non-zero if the given mode bits are set for a directory
//

#define S_ISDIR(_Mode) (((_Mode) & S_IFMT) == S_IFDIR)

//
// This macro returns non-zero if the given mode bits are set for a FIFO
// special file.
//

#define S_ISFIFO(_Mode) (((_Mode) & S_IFMT) == S_IFIFO)

//
// This macro returns non-zero if the given mode bits are set for a regular
// file.
//

#define S_ISREG(_Mode) (((_Mode) & S_IFMT) == S_IFREG)

//
// This macro returns non-zero if the given mode bits are set for a symbolic
// link.
//

#define S_ISLNK(_Mode) (((_Mode) & S_IFMT) == S_IFLNK)

//
// This macro returns non-zero if the given mode bits are set for a socket.
//

#define S_ISSOCK(_Mode) (((_Mode) & S_IFMT) == S_IFSOCK)

//
// Define the time value used in the nanosecond field to indicate that the
// current time should be used.
//

#define UTIME_NOW ((long)-1)

//
// Define the time value used in the nanosecond field to indicate that the
// time setting should be omitted.
//

#define UTIME_OMIT ((long)-2)

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define the types of files, starting with the overall mask. Hardcoded values
// are often baked into applications, so these values line up most accurately
// with historical implementations.
//

#define S_IFMT 0x0000F000

//
// FIFO special device
//

#define S_IFIFO 0x00001000

//
// Character special device
//

#define S_IFCHR 0x00002000

//
// Regular directory
//

#define S_IFDIR 0x00004000

//
// Block special device
//

#define S_IFBLK 0x00006000

//
// Regular file
//

#define S_IFREG 0x00008000

//
// Symbolic link
//

#define S_IFLNK 0x0000A000

//
// Socket file type
//

#define S_IFSOCK 0x0000C000

//
// Define the file mode bits. These values are actually standardized, in
// addition to being hardcoded into applications.
//

//
// Other read, write, and execute bits
//

#define S_IXOTH 0x00000001
#define S_IWOTH 0x00000002
#define S_IROTH 0x00000004
#define S_IRWXO (S_IROTH | S_IWOTH | S_IXOTH)

//
// Group read, write, and execute bits
//

#define S_IXGRP 0x00000008
#define S_IWGRP 0x00000010
#define S_IRGRP 0x00000020
#define S_IRWXG (S_IRGRP | S_IWGRP | S_IXGRP)

//
// User read, write, and execute bits
//

#define S_IXUSR 0x00000040
#define S_IWUSR 0x00000080
#define S_IRUSR 0x00000100
#define S_IRWXU (S_IRUSR | S_IWUSR | S_IXUSR)

//
// User read, write, and execute synonyms for compatibility.
//

#define S_IEXEC S_IXUSR
#define S_IWRITE S_IWUSR
#define S_IREAD S_IRUSR

//
// Restricted deletion in directory flag
//

#define S_ISVTX 0x00000200

//
// Set user ID and set group ID on execution bits
//

#define S_ISGID 0x00000400
#define S_ISUID 0x00000800

//
// Define common bit masks.
//

#define ACCESSPERMS (S_IRWXU | S_IRWXG | S_IRWXO)
#define ALLPERMS (ACCESSPERMS | S_ISUID | S_ISGID | S_ISVTX)
#define DEFFILEMODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

//
// These definitions create the proper POSIX structure members for struct
// stat's access, modification, and status change time.
//

#define st_atime st_atim.tv_sec
#define st_mtime st_mtim.tv_sec
#define st_ctime st_ctim.tv_sec
#define st_birthtime st_birthtim.tv_sec

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines file object information.

Members:

    st_dev - Stores the device ID of the containing file.

    st_ino - Stores the file serial number.

    __st_type - Stores the file type.

    st_mode - Stores the file mode bits. See S_I* definitions.

    st_nlink - Stores the number of hard links to the file.

    st_uid - Stores the user ID of the file.

    st_gid - Stores the group ID of the file.

    st_rdev - Stores the device ID if the device is a character or block
        special device.

    st_size - Stores the file size in bytes for regular files. For symbolic
        links, stores the length in bytes of the pathname contained in the
        symbolic link.

    st_atim - Stores the time of the last access.

    st_mtim - Stores the time of the last data modification.

    st_ctim - Stores the time of the last status change.

    st_birthtim - Stores the creation time of the file.

    st_blksize - Stores a file system specific preferred I/O block size for
        this object. This may vary from file to file.

    st_blocks - Stores the number of blocks allocated for this object.

    st_flags - Stores user defined file flags.

    st_gen - Stores the file generation number.

--*/

struct stat {
    dev_t st_dev;
    ino_t st_ino;
    int __st_type;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    off_t st_size;
    struct timespec st_atim;
    struct timespec st_mtim;
    struct timespec st_ctim;
    struct timespec st_birthtim;
    blksize_t st_blksize;
    blkcnt_t st_blocks;
    unsigned int st_flags;
    unsigned int st_gen;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
stat (
    const char *Path,
    struct stat *Stat
    );

/*++

Routine Description:

    This routine gets file information for the given file.

Arguments:

    Path - Supplies the path string of the file to get the status information
        for.

    Stat - Supplies a pointer where the information will be returned on
        success.

Return Value:

    0 on success.

    -1 on failure and the errno variable will be set to provide more
    information.

--*/

LIBC_API
int
lstat (
    const char *Path,
    struct stat *Stat
    );

/*++

Routine Description:

    This routine gets file information for the given file. It is the same as
    the stat function, except that when the given path refers to a symbolic
    link, this routine returns information for the link itself, where stat
    returns information for the link destination.

Arguments:

    Path - Supplies the path string of the file to get the status information
        for.

    Stat - Supplies a pointer where the information will be returned on
        success.

Return Value:

    0 on success.

    -1 on failure and the errno variable will be set to provide more
    information.

--*/

LIBC_API
int
fstatat (
    int Directory,
    const char *Path,
    struct stat *Stat,
    int Flags
    );

/*++

Routine Description:

    This routine gets file information for the given file.

Arguments:

    Directory - Supplies an optional file descriptor. If the given path
        is a relative path, the directory referenced by this descriptor will
        be used as a starting point for path resolution. Supply AT_FDCWD to
        use the working directory for relative paths.

    Path - Supplies the path string of the file to get the status information
        for.

    Stat - Supplies a pointer where the information will be returned on
        success.

    Flags - Supplies AT_SYMLINK_NOFOLLOW if the routine should return
        information for the symbolic link itself, or 0 if the call should
        follow a symbolic link at the destination.

Return Value:

    0 on success.

    -1 on failure and the errno variable will be set to provide more
    information.

--*/

LIBC_API
int
creat (
    const char *Path,
    mode_t Mode
    );

/*++

Routine Description:

    This routine attempts to create a new file or truncate an existing one.
    This routine is equivalent to
    open(Path, O_WRONLY | O_CREAT | O_TRUNC, Mode).

Arguments:

    Path - Supplies a pointer to the null terminated string containing the file
        path to open.

    Mode - Supplies the mode to open the file with.

Return Value:

    Like open, returns the new file descriptor on success.

    -1 on error, and errno will be set to indicate the error.

--*/

LIBC_API
int
fstat (
    int FileDescriptor,
    struct stat *Stat
    );

/*++

Routine Description:

    This routine gets file information corresponding to the given file
    descriptor.

Arguments:

    FileDescriptor - Supplies the open file descriptor to get file information
        for.

    Stat - Supplies a pointer where the file information will be returned on
        success.

Return Value:

    0 on success.

    -1 on failure and the errno variable will be set to provide more
    information.

--*/

LIBC_API
int
fchmod (
    int FileDescriptor,
    mode_t Mode
    );

/*++

Routine Description:

    This routine sets the file permissions of the file opened with the given
    file descriptor.

Arguments:

    FileDescriptor - Supplies the file descriptor whose permissions should be
        modified.

    Mode - Supplies the new mode to set.

Return Value:

    0 on success.

    -1 on failure. The errno variable will be set to indicate the error.

--*/

LIBC_API
int
mkdir (
    const char *Path,
    mode_t Permissions
    );

/*++

Routine Description:

    This routine creates a new directory.

Arguments:

    Path - Supplies the path string of the directory to create.

    Permissions - Supplies the permission bits to create the file with.

Return Value:

    0 on success.

    -1 on failure and the errno variable will be set to provide more
    information.

--*/

LIBC_API
int
mkdirat (
    int Directory,
    const char *Path,
    mode_t Permissions
    );

/*++

Routine Description:

    This routine creates a new directory.

Arguments:

    Directory - Supplies an optional file descriptor. If the given path
        is a relative path, the directory referenced by this descriptor will
        be used as a starting point for path resolution. Supply AT_FDCWD to
        use the working directory for relative paths.

    Path - Supplies the path string of the directory to create.

    Permissions - Supplies the permission bits to create the file with.

Return Value:

    0 on success.

    -1 on failure and the errno variable will be set to provide more
    information.

--*/

LIBC_API
mode_t
umask (
    mode_t CreationMask
    );

/*++

Routine Description:

    This routine sets the creation mask for file permissions on calls to open,
    creat, mkdir, and mkfifo.

Arguments:

    CreationMask - Supplies the new mask to set. Bits set in this creation mask
        will be cleared from the permissions given to open, creat, mkdir, and
        mkfifo.

Return Value:

    Returns the original value of the creation mask.

--*/

LIBC_API
int
chmod (
    const char *Path,
    mode_t Permissions
    );

/*++

Routine Description:

    This routine sets the file permission bits of the given path.

Arguments:

    Path - Supplies a pointer to the path whose permissions should be changed.

    Permissions - Supplies the new file permissions to set.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
fchmodat (
    int Directory,
    const char *Path,
    mode_t Permissions,
    int Flags
    );

/*++

Routine Description:

    This routine sets the file permission bits of the given path.

Arguments:

    Directory - Supplies an optional file descriptor. If the given path
        is a relative path, the directory referenced by this descriptor will
        be used as a starting point for path resolution. Supply AT_FDCWD to
        use the working directory for relative paths.

    Path - Supplies a pointer to the path whose permissions should be changed.

    Permissions - Supplies the new file permissions to set.

    Flags - Supplies AT_SYMLINK_NOFOLLOW if the routine should affect a
        symbolic link itself, or AT_SYMLINK_FOLLOW if the call should follow a
        symbolic link at the destination.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
mkfifo (
    const char *Path,
    mode_t Permissions
    );

/*++

Routine Description:

    This routine creates a new named pipe.

Arguments:

    Path - Supplies a pointer to the path of the new named pipe. This path must
        not already exist.

    Permissions - Supplies the initial permissions of the pipe.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
mkfifoat (
    int Directory,
    const char *Path,
    mode_t Permissions
    );

/*++

Routine Description:

    This routine creates a new named pipe.

Arguments:

    Directory - Supplies an optional file descriptor. If the given path
        is a relative path, the directory referenced by this descriptor will
        be used as a starting point for path resolution. Supply AT_FDCWD to
        use the working directory for relative paths.

    Path - Supplies a pointer to the path of the new named pipe. This path must
        not already exist.

    Permissions - Supplies the initial permissions of the pipe.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
mknod (
    const char *Path,
    mode_t Mode,
    dev_t Device
    );

/*++

Routine Description:

    This routine creates a new regular file or special file.

Arguments:

    Path - Supplies a pointer to the path to create.

    Mode - Supplies the type of file and permissions to create.

    Device - Supplies the device number to create.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
mknodat (
    int Directory,
    const char *Path,
    mode_t Mode,
    dev_t Device
    );

/*++

Routine Description:

    This routine creates a new regular file or special file.

Arguments:

    Directory - Supplies an optional file descriptor. If the given path
        is a relative path, the directory referenced by this descriptor will
        be used as a starting point for path resolution. Supply AT_FDCWD to
        use the working directory for relative paths.

    Path - Supplies a pointer to the path to create.

    Mode - Supplies the type of file and permissions to create.

    Device - Supplies the device number to create.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
utimensat (
    int Directory,
    const char *Path,
    const struct timespec Times[2],
    int Flags
    );

/*++

Routine Description:

    This routine sets the access and modification times of the given file. The
    effective user ID of the process must match the owner of the file, or the
    process must have appropriate privileges.

Arguments:

    Directory - Supplies an optional file descriptor. If the given path
        is a relative path, the directory referenced by this descriptor will
        be used as a starting point for path resolution. Supply AT_FDCWD to
        use the working directory for relative paths.

    Path - Supplies a pointer to the path of the file to change times for.

    Times - Supplies an optional array of time spec structures containing the
        access (index 0) and modification (index 1) times to set. If NULL is
        supplied, then the current time is used for both values. If either
        value has UTIME_NOW in the nanoseconds field, then the curren ttime is
        used. If either value has UTIME_OMIT in the nanoseconds field, then
        that field will not be changed.

    Flags - Supplies AT_SYMLINK_NOFOLLOW if the routine should modify
        information for the symbolic link itself, or 0 if the call should
        follow a symbolic link at the destination.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
futimens (
    int File,
    const struct timespec Times[2]
    );

/*++

Routine Description:

    This routine sets the access and modification times of the file referenced
    by the given file descriptor.

Arguments:

    File - Supplies the file descriptor of the file to modify.

    Times - Supplies an optional array of time spec structures containing the
        access (index 0) and modification (index 1) times to set. If NULL is
        supplied, then the current time is used for both values. If either
        value has UTIME_NOW in the nanoseconds field, then the curren ttime is
        used. If either value has UTIME_OMIT in the nanoseconds field, then
        that field will not be changed.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

#ifdef __cplusplus

}

#endif
#endif

