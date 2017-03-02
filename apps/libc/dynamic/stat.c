/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    stat.c

Abstract:

    This module implements file information retrieval and updates.

Author:

    Evan Green 17-Jun-2013

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
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro asserts if the effective access flags defined by the C library
// don't match up with the flags defined by the kernel. It should just compile
// out.
//

#define ASSERT_ACCESS_FLAGS_ARE_EQUAL()                 \
    if (!((R_OK == EFFECTIVE_ACCESS_READ) &&            \
          (W_OK == EFFECTIVE_ACCESS_WRITE) &&           \
          (X_OK == EFFECTIVE_ACCESS_EXECUTE))) {        \
                                                        \
        assert(FALSE);                                  \
                                                        \
    }

//
// This macro asserts that the fields in FILE_PROPERTIES line up with the
// fields in struct stat.
//

#define ASSERT_STAT_FILE_PROPERTIES_ALIGN()                     \
    ASSERT((sizeof(FILE_PROPERTIES) == sizeof(struct stat)) &&  \
           (FIELD_OFFSET(FILE_PROPERTIES, DeviceId) ==          \
            FIELD_OFFSET(struct stat, st_dev)) &&               \
           (FIELD_OFFSET(FILE_PROPERTIES, FileId) ==            \
            FIELD_OFFSET(struct stat, st_ino)) &&               \
           (FIELD_OFFSET(FILE_PROPERTIES, Permissions) ==       \
            FIELD_OFFSET(struct stat, st_mode)) &&              \
           (FIELD_OFFSET(FILE_PROPERTIES, HardLinkCount) ==     \
            FIELD_OFFSET(struct stat, st_nlink)) &&             \
           (FIELD_OFFSET(FILE_PROPERTIES, UserId) ==            \
            FIELD_OFFSET(struct stat, st_uid)) &&               \
           (FIELD_OFFSET(FILE_PROPERTIES, GroupId) ==           \
            FIELD_OFFSET(struct stat, st_gid)) &&               \
           (FIELD_OFFSET(FILE_PROPERTIES, RelatedDevice) ==     \
            FIELD_OFFSET(struct stat, st_rdev)) &&              \
           (FIELD_OFFSET(FILE_PROPERTIES, Size) ==              \
            FIELD_OFFSET(struct stat, st_size)) &&              \
           (FIELD_OFFSET(FILE_PROPERTIES, AccessTime) ==        \
            FIELD_OFFSET(struct stat, st_atim)) &&              \
           (FIELD_OFFSET(FILE_PROPERTIES, ModifiedTime) ==      \
            FIELD_OFFSET(struct stat, st_mtim)) &&              \
           (FIELD_OFFSET(FILE_PROPERTIES, StatusChangeTime) ==  \
            FIELD_OFFSET(struct stat, st_ctim)) &&              \
           (FIELD_OFFSET(FILE_PROPERTIES, CreationTime) ==      \
            FIELD_OFFSET(struct stat, st_birthtim)) &&          \
           (FIELD_OFFSET(FILE_PROPERTIES, BlockSize) ==         \
            FIELD_OFFSET(struct stat, st_blksize)) &&           \
           (FIELD_OFFSET(FILE_PROPERTIES, BlockCount) ==        \
            FIELD_OFFSET(struct stat, st_blocks)) &&            \
           (FIELD_OFFSET(FILE_PROPERTIES, Flags) ==             \
            FIELD_OFFSET(struct stat, st_flags)) &&             \
           (FIELD_OFFSET(FILE_PROPERTIES, Generation) ==        \
            FIELD_OFFSET(struct stat, st_gen)));

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
ClpConvertFilePropertiesToStat (
    PFILE_PROPERTIES Properties,
    struct stat *Stat
    );

//
// -------------------------------------------------------------------- Globals
//

mode_t ClStatFileTypeConversions[IoObjectTypeCount] = {
    0,
    S_IFDIR,
    S_IFREG,
    S_IFBLK,
    S_IFCHR,
    S_IFIFO,
    S_IFDIR,
    S_IFSOCK,
    S_IFCHR,
    S_IFCHR,
    S_IFREG,
    S_IFLNK
};

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
int
access (
    const char *Path,
    int Mode
    )

/*++

Routine Description:

    This routine checks the given path for accessibility using the real user
    ID and real group ID.

Arguments:

    Path - Supplies the path string of the file to get the accessibility
        information for.

    Mode - Supplies the mode bits the caller is interested in. Valid values are
        F_OK to check if the file exists, R_OK to check if the file is readable,
        W_OK to check if the file is writable, and X_OK to check if the file is
        executable.

Return Value:

    0 on success.

    -1 on failure and the errno variable will be set to provide more
    information.

--*/

{

    return faccessat(AT_FDCWD, Path, Mode, 0);
}

LIBC_API
int
faccessat (
    int Directory,
    const char *Path,
    int Mode,
    int Flags
    )

/*++

Routine Description:

    This routine checks the given path for accessibility using the real user
    ID and real group ID rather than the effective user and group IDs.

Arguments:

    Directory - Supplies an optional file descriptor. If the given path
        is a relative path, the directory referenced by this descriptor will
        be used as a starting point for path resolution. Supply AT_FDCWD to
        use the working directory for relative paths.

    Path - Supplies the path string of the file to get the accessibility
        information for.

    Mode - Supplies the mode bits the caller is interested in. Valid values are
        F_OK to check if the file exists, R_OK to check if the file is readable,
        W_OK to check if the file is writable, and X_OK to check if the file is
        executable.

    Flags - Supplies a bitfield of flags. Supply AT_EACCESS if the checks for
        accessibility should be performed using the effective user and group
        ID rather than the real user and group ID.

Return Value:

    0 on success.

    -1 on failure and the errno variable will be set to provide more
    information.

--*/

{

    ULONG Access;
    ULONG PathLength;
    KSTATUS Status;
    BOOL UseRealIds;

    ASSERT_ACCESS_FLAGS_ARE_EQUAL();

    PathLength = 0;
    if (Path != NULL) {
        PathLength = strlen(Path) + 1;
    }

    UseRealIds = TRUE;
    if ((Flags & AT_EACCESS) != 0) {
        UseRealIds = FALSE;
    }

    Status = OsGetEffectiveAccess((HANDLE)(UINTN)Directory,
                                  (PSTR)Path,
                                  PathLength,
                                  Mode,
                                  UseRealIds,
                                  &Access);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    if (Access != (ULONG)Mode) {
        errno = EACCES;
        return -1;
    }

    return 0;
}

LIBC_API
int
stat (
    const char *Path,
    struct stat *Stat
    )

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

{

    return fstatat(AT_FDCWD, Path, Stat, 0);
}

LIBC_API
int
lstat (
    const char *Path,
    struct stat *Stat
    )

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

{

    return fstatat(AT_FDCWD, Path, Stat, AT_SYMLINK_NOFOLLOW);
}

LIBC_API
int
fstatat (
    int Directory,
    const char *Path,
    struct stat *Stat,
    int Flags
    )

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

{

    BOOL FollowLinks;
    ULONG PathLength;
    FILE_PROPERTIES Properties;
    KSTATUS Status;

    PathLength = 0;
    if (Path != NULL) {
        PathLength = strlen(Path) + 1;
    }

    FollowLinks = TRUE;
    if ((Flags & AT_SYMLINK_NOFOLLOW) != 0) {
        FollowLinks = FALSE;
    }

    Status = OsGetFileInformation((HANDLE)(UINTN)Directory,
                                  (PSTR)Path,
                                  PathLength,
                                  FollowLinks,
                                  &Properties);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    ClpConvertFilePropertiesToStat(&Properties, Stat);
    return 0;
}

LIBC_API
int
creat (
    const char *Path,
    mode_t Mode
    )

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

{

    return open(Path, O_WRONLY | O_CREAT | O_TRUNC, Mode);
}

LIBC_API
int
fstat (
    int FileDescriptor,
    struct stat *Stat
    )

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

{

    FILE_CONTROL_PARAMETERS_UNION Parameters;
    KSTATUS Status;

    ASSERT(sizeof(FILE_PROPERTIES) == sizeof(struct stat));

    Parameters.SetFileInformation.FileProperties = (PFILE_PROPERTIES)Stat;
    Status = OsFileControl((HANDLE)(UINTN)FileDescriptor,
                           FileControlCommandGetFileInformation,
                           &Parameters);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    ClpConvertFilePropertiesToStat((PFILE_PROPERTIES)Stat, Stat);
    return 0;
}

LIBC_API
int
fchmod (
    int FileDescriptor,
    mode_t Mode
    )

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

{

    FILE_CONTROL_PARAMETERS_UNION Parameters;
    FILE_PROPERTIES Properties;
    KSTATUS Status;

    Parameters.SetFileInformation.FieldsToSet = FILE_PROPERTY_FIELD_PERMISSIONS;
    Parameters.SetFileInformation.FileProperties = &Properties;

    ASSERT_FILE_PERMISSIONS_EQUIVALENT();

    Properties.Permissions = Mode & FILE_PERMISSION_MASK;
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
fchown (
    int FileDescriptor,
    uid_t Owner,
    gid_t Group
    )

/*++

Routine Description:

    This routine sets the file owner and group of the file opened with the
    given file descriptor.

Arguments:

    FileDescriptor - Supplies the file descriptor whose owner and group should
        be modified.

    Owner - Supplies the new owner of the file.

    Group - Supplies the new group of the file.

Return Value:

    0 on success.

    -1 on failure. The errno variable will be set to indicate the error.

--*/

{

    FILE_CONTROL_PARAMETERS_UNION Parameters;
    FILE_PROPERTIES Properties;
    KSTATUS Status;

    Parameters.SetFileInformation.FieldsToSet = FILE_PROPERTY_FIELD_USER_ID |
                                                FILE_PROPERTY_FIELD_GROUP_ID;

    Parameters.SetFileInformation.FileProperties = &Properties;
    Properties.UserId = Owner;
    Properties.GroupId = Group;
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
mkdir (
    const char *Path,
    mode_t Permissions
    )

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

{

    return mkdirat(AT_FDCWD, Path, Permissions);
}

LIBC_API
int
mkdirat (
    int Directory,
    const char *Path,
    mode_t Permissions
    )

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

{

    FILE_PERMISSIONS CreatePermissions;
    ULONG Flags;
    HANDLE Handle;
    size_t Length;
    KSTATUS Status;

    ASSERT_FILE_PERMISSIONS_EQUIVALENT();

    CreatePermissions = Permissions;
    Flags = SYS_OPEN_FLAG_CREATE | SYS_OPEN_FLAG_DIRECTORY |
            SYS_OPEN_FLAG_FAIL_IF_EXISTS;

    //
    // Strip trailing slashes, as the kernel path resolution would treat it
    // like "mydir/.", which presumably doesn't exist yet.
    //

    Length = strlen(Path);
    while ((Length > 1) && (Path[Length - 1] == '/')) {
        Length -= 1;
    }

    Status = OsOpen((HANDLE)(UINTN)Directory,
                    (PSTR)Path,
                    Length + 1,
                    Flags,
                    CreatePermissions,
                    &Handle);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    OsClose(Handle);
    return 0;
}

LIBC_API
mode_t
umask (
    mode_t CreationMask
    )

/*++

Routine Description:

    This routine sets the creation mask for file permissions on calls to open,
    creat, shm_open, mkdir, and mkfifo.

Arguments:

    CreationMask - Supplies the new mask to set. Bits set in this creation mask
        will be cleared from the permissions given to open, creat, mkdir, and
        mkfifo.

Return Value:

    Returns the original value of the creation mask.

--*/

{

    return OsSetUmask(CreationMask);
}

LIBC_API
int
chmod (
    const char *Path,
    mode_t Permissions
    )

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

{

    return fchmodat(AT_FDCWD, Path, Permissions, 0);
}

LIBC_API
int
fchmodat (
    int Directory,
    const char *Path,
    mode_t Permissions,
    int Flags
    )

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

{

    BOOL FollowLinks;
    FILE_PROPERTIES Properties;
    SET_FILE_INFORMATION Request;
    KSTATUS Status;

    ASSERT_FILE_PERMISSIONS_EQUIVALENT();

    FollowLinks = TRUE;
    if ((Flags & AT_SYMLINK_NOFOLLOW) != 0) {
        FollowLinks = FALSE;
    }

    Properties.Permissions = Permissions & FILE_PERMISSION_MASK;
    Request.FieldsToSet = FILE_PROPERTY_FIELD_PERMISSIONS;
    Request.FileProperties = &Properties;
    Status = OsSetFileInformation((HANDLE)(UINTN)Directory,
                                  (PSTR)Path,
                                  strlen(Path) + 1,
                                  FollowLinks,
                                  &Request);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

LIBC_API
int
chown (
    const char *Path,
    uid_t Owner,
    gid_t Group
    )

/*++

Routine Description:

    This routine sets the file owner of the given path.

Arguments:

    Path - Supplies a pointer to the path whose owner should be changed.

    Owner - Supplies the new owner of the path.

    Group - Supplies the new owner group of the path.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    return fchownat(AT_FDCWD, Path, Owner, Group, 0);
}

LIBC_API
int
lchown (
    const char *Path,
    uid_t Owner,
    gid_t Group
    )

/*++

Routine Description:

    This routine sets the file owner of the given path. The only difference
    between this routine and chown is that if the path given to this routine
    refers to a symbolic link, the operation will be done on the link itself
    (as opposed to the destination of the link, which is what chown would
    operate on).

Arguments:

    Path - Supplies a pointer to the path whose owner should be changed.

    Owner - Supplies the new owner of the path.

    Group - Supplies the new owner group of the path.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    return fchownat(AT_FDCWD, Path, Owner, Group, AT_SYMLINK_NOFOLLOW);
}

LIBC_API
int
fchownat (
    int Directory,
    const char *Path,
    uid_t Owner,
    gid_t Group,
    int Flags
    )

/*++

Routine Description:

    This routine sets the file owner of the given path.

Arguments:

    Directory - Supplies an optional file descriptor. If the given path
        is a relative path, the directory referenced by this descriptor will
        be used as a starting point for path resolution. Supply AT_FDCWD to
        use the working directory for relative paths.

    Path - Supplies a pointer to the path whose owner should be changed.

    Owner - Supplies the new owner of the path.

    Group - Supplies the new owner group of the path.

    Flags - Supplies AT_SYMLINK_NOFOLLOW if the routine should modify
        information for the symbolic link itself, or 0 if the call should
        follow a symbolic link at the destination.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    BOOL FollowLinks;
    FILE_PROPERTIES Properties;
    SET_FILE_INFORMATION Request;
    KSTATUS Status;

    Request.FieldsToSet = 0;
    Request.FileProperties = &Properties;
    if (Owner != (uid_t)-1) {
        Request.FieldsToSet |= FILE_PROPERTY_FIELD_USER_ID;
        Properties.UserId = Owner;
    }

    if (Group != (gid_t)-1) {
        Request.FieldsToSet |= FILE_PROPERTY_FIELD_GROUP_ID;
        Properties.GroupId = Group;
    }

    FollowLinks = TRUE;
    if ((Flags & AT_SYMLINK_NOFOLLOW) != 0) {
        FollowLinks = FALSE;
    }

    Status = OsSetFileInformation((HANDLE)(UINTN)Directory,
                                  (PSTR)Path,
                                  strlen(Path) + 1,
                                  FollowLinks,
                                  &Request);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

LIBC_API
int
mkfifo (
    const char *Path,
    mode_t Permissions
    )

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

{

    return mkfifoat(AT_FDCWD, Path, Permissions);
}

LIBC_API
int
mkfifoat (
    int Directory,
    const char *Path,
    mode_t Permissions
    )

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

{

    KSTATUS Status;

    ASSERT_FILE_PERMISSIONS_EQUIVALENT();

    if (Path == NULL) {
        errno = EINVAL;
        return -1;
    }

    Status = OsCreatePipe((HANDLE)(UINTN)Directory,
                          (PSTR)Path,
                          strlen(Path) + 1,
                          0,
                          (FILE_PERMISSIONS)Permissions,
                          NULL,
                          NULL);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

LIBC_API
int
mknod (
    const char *Path,
    mode_t Mode,
    dev_t Device
    )

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

{

    return mknodat(AT_FDCWD, Path, Mode, Device);
}

LIBC_API
int
mknodat (
    int Directory,
    const char *Path,
    mode_t Mode,
    dev_t Device
    )

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

{

    errno = ENOSYS;
    return -1;
}

LIBC_API
int
utime (
    const char *Path,
    const struct utimbuf *Times
    )

/*++

Routine Description:

    This routine sets the access and modification times of the given file. The
    effective user ID of the process must match the owner of the file, or the
    process must have appropriate privileges.

Arguments:

    Path - Supplies a pointer to the path of the file to change times for.

    Times - Supplies an optional pointer to a time buffer structure containing
        the access and modification times to set. If this parameter is NULL,
        the current time will be used.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    struct timespec NanoTimes[2];

    if (Times == NULL) {
        return utimensat(AT_FDCWD, Path, NULL, 0);
    }

    NanoTimes[0].tv_sec = Times->actime;
    NanoTimes[0].tv_nsec = 0;
    NanoTimes[1].tv_sec = Times->modtime;
    NanoTimes[1].tv_nsec = 0;
    return utimensat(AT_FDCWD, Path, NanoTimes, 0);
}

LIBC_API
int
utimes (
    const char *Path,
    const struct timeval Times[2]
    )

/*++

Routine Description:

    This routine sets the access and modification times of the given file. The
    effective user ID of the process must match the owner of the file, or the
    process must have appropriate privileges.

Arguments:

    Path - Supplies a pointer to the path of the file to change times for.

    Times - Supplies an optional array of time value structures containing the
        access and modification times to set.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    struct timespec NanoTimes[2];

    if (Times == NULL) {
        return utimensat(AT_FDCWD, Path, NULL, 0);
    }

    NanoTimes[0].tv_sec = Times[0].tv_sec;
    NanoTimes[0].tv_nsec = Times[0].tv_usec * 1000;
    NanoTimes[1].tv_sec = Times[1].tv_sec;
    NanoTimes[1].tv_nsec = Times[1].tv_usec * 1000;
    return utimensat(AT_FDCWD, Path, NanoTimes, 0);
}

LIBC_API
int
lutimes (
    const char *Path,
    const struct timeval Times[2]
    )

/*++

Routine Description:

    This routine sets the access and modification times of the given file. The
    effective user ID of the process must match the owner of the file, or the
    process must have appropriate privileges. The only difference between this
    function and utimes is that if the path references a symbolic link, the
    times of the link itself will be changed rather than the file to which
    it refers.

Arguments:

    Path - Supplies a pointer to the path of the file to change times for.

    Times - Supplies an optional array of time value structures containing the
        access and modification times to set.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    struct timespec NanoTimes[2];

    if (Times == NULL) {
        return utimensat(AT_FDCWD, Path, NULL, 0);
    }

    NanoTimes[0].tv_sec = Times[0].tv_sec;
    NanoTimes[0].tv_nsec = Times[0].tv_usec * 1000;
    NanoTimes[1].tv_sec = Times[1].tv_sec;
    NanoTimes[1].tv_nsec = Times[1].tv_usec * 1000;
    return utimensat(AT_FDCWD, Path, NanoTimes, AT_SYMLINK_NOFOLLOW);
}

LIBC_API
int
utimensat (
    int Directory,
    const char *Path,
    const struct timespec Times[2],
    int Flags
    )

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

    Path - Supplies an optional pointer to the path of the file to change times
        for. If no file path is supplied, then the directory descriptor is
        acted upon.

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

{

    SYSTEM_TIME CurrentTime;
    BOOL FollowLinks;
    FILE_PROPERTIES Properties;
    SET_FILE_INFORMATION Request;
    KSTATUS Status;

    Request.FieldsToSet = FILE_PROPERTY_FIELD_ACCESS_TIME |
                          FILE_PROPERTY_FIELD_MODIFIED_TIME;

    Request.FileProperties = &Properties;
    OsGetSystemTime(&CurrentTime);

    //
    // Corral the access time.
    //

    if ((Times == NULL) || (Times[0].tv_nsec == UTIME_NOW)) {
        Properties.AccessTime = CurrentTime;

    } else if (Times[0].tv_nsec == UTIME_OMIT) {
        Request.FieldsToSet &= ~FILE_PROPERTY_FIELD_ACCESS_TIME;

    } else {
        ClpConvertUnixTimeToSystemTime(&(Properties.AccessTime),
                                       Times[0].tv_sec);

        Properties.AccessTime.Nanoseconds = Times[0].tv_nsec;
    }

    //
    // Round up the modified time.
    //

    if ((Times == NULL) || (Times[1].tv_nsec == UTIME_NOW)) {
        Properties.ModifiedTime = CurrentTime;

    } else if (Times[1].tv_nsec == UTIME_OMIT) {
        Request.FieldsToSet &= ~FILE_PROPERTY_FIELD_MODIFIED_TIME;

    } else {
        ClpConvertUnixTimeToSystemTime(&(Properties.ModifiedTime),
                                       Times[1].tv_sec);

        Properties.ModifiedTime.Nanoseconds = Times[1].tv_nsec;
    }

    FollowLinks = TRUE;
    if ((Flags & AT_SYMLINK_NOFOLLOW) != 0) {
        FollowLinks = FALSE;
    }

    //
    // If there's no path and no directory, make the path the current directory.
    //

    if ((Path == NULL) && (Directory == AT_FDCWD)) {
        Path = ".";
    }

    if (Path != NULL) {
        Status = OsSetFileInformation((HANDLE)(UINTN)Directory,
                                      (PSTR)Path,
                                      strlen(Path) + 1,
                                      FollowLinks,
                                      &Request);

        if (!KSUCCESS(Status)) {
            errno = ClConvertKstatusToErrorNumber(Status);
            return -1;
        }

    //
    // If no path was specified, operate on the directory handle.
    //

    } else {
        Status = OsFileControl((HANDLE)(UINTN)Directory,
                               FileControlCommandSetFileInformation,
                               (PFILE_CONTROL_PARAMETERS_UNION)&Request);

        if (!KSUCCESS(Status)) {
            errno = ClConvertKstatusToErrorNumber(Status);
            return -1;
        }
    }

    return 0;
}

LIBC_API
int
futimes (
    int File,
    const struct timeval Times[2]
    )

/*++

Routine Description:

    This routine sets the access and modification times of the given file. The
    effective user ID of the process must match the owner of the file, or the
    process must have appropriate privileges.

Arguments:

    File - Supplies the open file descriptor of the file to change the access
        and modification times for.

    Times - Supplies an optional array of time value structures containing the
        access and modification times to set.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    struct timespec NanoTimes[2];

    if (Times == NULL) {
        return futimens(File, NULL);
    }

    NanoTimes[0].tv_sec = Times[0].tv_sec;
    NanoTimes[0].tv_nsec = Times[0].tv_usec * 1000;
    NanoTimes[1].tv_sec = Times[1].tv_sec;
    NanoTimes[1].tv_nsec = Times[1].tv_usec * 1000;
    return futimens(File, NanoTimes);
}

LIBC_API
int
futimens (
    int File,
    const struct timespec Times[2]
    )

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

{

    if (File < 0) {
        errno = EBADF;
        return -1;
    }

    return utimensat(File, NULL, Times, 0);
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
ClpConvertFilePropertiesToStat (
    PFILE_PROPERTIES Properties,
    struct stat *Stat
    )

/*++

Routine Description:

    This routine converts file properties into a stat structure.

Arguments:

    Properties - Supplies a pointer to the file properties to convert.

    Stat - Supplies a pointer where the stat structure information will be
        returned.

Return Value:

    None.

--*/

{

    ASSERT_STAT_FILE_PROPERTIES_ALIGN();

    if (Properties != (PFILE_PROPERTIES)Stat) {
        memcpy(Stat, Properties, sizeof(struct stat));
    }

    Stat->st_ctime =
               ClpConvertSystemTimeToUnixTime(&(Properties->StatusChangeTime));

    Stat->st_mtime =
                   ClpConvertSystemTimeToUnixTime(&(Properties->ModifiedTime));

    Stat->st_atime = ClpConvertSystemTimeToUnixTime(&(Properties->AccessTime));

    //
    // Convert the I/O object type into mode bits.
    //

    ASSERT_FILE_PERMISSIONS_EQUIVALENT();
    assert((Properties->Permissions & (~FILE_PERMISSION_MASK)) == 0);
    assert(Properties->Type < IoObjectTypeCount);

    //
    // Please update the array (and this assert) if a new I/O object type is
    // added.
    //

    assert(IoObjectSymbolicLink + 1 == IoObjectTypeCount);

    Stat->st_mode |= ClStatFileTypeConversions[Properties->Type];
    return;
}

