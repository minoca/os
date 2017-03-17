/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    uos.c

Abstract:

    This module implements the POSIX operating system dependent portion of the
    Swiss common library.

Author:

    Evan Green 2-Jul-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#define _GNU_SOURCE 1

#include <minoca/lib/types.h>
#include <minoca/lib/termlib.h>

#include <assert.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "../swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the size of the buffer to allocate for symbolic link destinations.
//

#define LINK_DESTINATION_SIZE 1024

//
// Define the size of the buffer used to get the user and group information.
//

#define USER_INFORMATION_BUFFER_SIZE 4096
#define GROUP_INFORMATION_BUFFER_SIZE 4096

//
// Define the number of elements in the initial array of how many groups a user
// is in.
//

#define INITIAL_GROUP_COUNT 64

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

int
SwpEvaluateFileAccess (
    SWISS_FILE_TEST Operator,
    mode_t StatMode,
    uid_t FileOwner,
    gid_t FileGroup,
    int *Error
    );

int
SwpConvertPasswdToUserInformation (
    struct passwd *Passwd,
    PSWISS_USER_INFORMATION *UserInformation
    );

int
SwpSetColors (
    CONSOLE_COLOR Background,
    CONSOLE_COLOR Foreground
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the mapping of signal names to numbers.
//

SWISS_SIGNAL_NAME SwSignalMap[] = {
    {0, "T"},
    {SIGHUP, "HUP"},
    {SIGINT, "INT"},
    {SIGQUIT, "QUIT"},
    {SIGILL, "ILL"},
    {SIGTRAP, "TRAP"},
    {SIGABRT, "ABRT"},
    {SIGBUS, "BUS"},
    {SIGFPE, "FPE"},
    {SIGKILL, "KILL"},
    {SIGUSR1, "USR1"},
    {SIGSEGV, "SEGV"},
    {SIGUSR2, "USR2"},
    {SIGPIPE, "PIPE"},
    {SIGALRM, "ALRM"},
    {SIGTERM, "TERM"},
    {SIGCHLD, "CHLD"},
    {SIGCONT, "CONT"},
    {SIGSTOP, "STOP"},
    {SIGTSTP, "TSTP"},
    {SIGTTIN, "TTIN"},
    {SIGTTOU, "TTOU"},
    {SIGURG, "URG"},
    {SIGXCPU, "XCPU"},
    {SIGXFSZ, "XFSZ"},
    {SIGVTALRM, "VTALRM"},
    {SIGPROF, "PROF"},
    {SIGWINCH, "WINCH"},
    {SIGPOLL, "POLL"},
    {-1, NULL},
};

//
// Store a global that's non-zero if this OS supports forking.
//

int SwForkSupported = 1;

//
// Store a global that's non-zero if this OS supports symbolic links.
//

int SwSymlinkSupported = 1;

//
// Remember the original terminal settings.
//

struct termios SwOriginalTerminalSettings;
char SwOriginalTerminalSettingsValid = 0;

//
// ------------------------------------------------------------------ Functions
//

int
SwReadLink (
    char *LinkPath,
    char **Destination
    )

/*++

Routine Description:

    This routine gets the destination of the symbolic link.

Arguments:

    LinkPath - Supplies a pointer to the string containing the path to the
        link.

    Destination - Supplies a pointer where a pointer to a string containing the
        link path destination will be returned on success. The caller is
        responsible for freeing this memory.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    char *DestinationBuffer;
    int Result;

    DestinationBuffer = malloc(LINK_DESTINATION_SIZE);
    if (DestinationBuffer == NULL) {
        return ENOMEM;
    }

    Result = readlink(LinkPath, DestinationBuffer, LINK_DESTINATION_SIZE - 1);
    if (Result == -1) {
        free(DestinationBuffer);
        *Destination = NULL;
        return errno;
    }

    DestinationBuffer[Result] = '\0';
    *Destination = DestinationBuffer;
    return 0;
}

int
SwCreateHardLink (
    char *ExistingFilePath,
    char *LinkPath
    )

/*++

Routine Description:

    This routine creates a hard link.

Arguments:

    ExistingFilePath - Supplies a pointer to the existing file path to create
        the link from.

    LinkPath - Supplies a pointer to the destination of the link.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    int Result;

    Result = link(ExistingFilePath, LinkPath);
    if (Result == 0) {
        return 0;
    }

    return errno;
}

int
SwCreateSymbolicLink (
    char *LinkTarget,
    char *Link
    )

/*++

Routine Description:

    This routine creates a symbolic link.

Arguments:

    LinkTarget - Supplies the location that the link points to.

    Link - Supplies the path where the link should be created.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    int Result;

    Result = symlink(LinkTarget, Link);
    if (Result == 0) {
        return 0;
    }

    return errno;
}

int
SwGetUserNameFromId (
    uid_t UserId,
    char **UserName
    )

/*++

Routine Description:

    This routine converts the given user ID into a user name.

Arguments:

    UserId - Supplies the user ID to query.

    UserName - Supplies an optional pointer where a string will be returned
        containing the user name. The caller is responsible for freeing this
        memory.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    void *Buffer;
    struct passwd Information;
    char *Name;
    size_t NameSize;
    int Result;
    struct passwd *ResultPointer;

    Name = NULL;
    Buffer = malloc(USER_INFORMATION_BUFFER_SIZE);
    if (Buffer == NULL) {
        Result = ENOMEM;
        goto GetUserNameFromIdEnd;
    }

    Result = getpwuid_r(UserId,
                        &Information,
                        Buffer,
                        USER_INFORMATION_BUFFER_SIZE,
                        &ResultPointer);

    if (Result != 0) {
        Result = errno;
        goto GetUserNameFromIdEnd;
    }

    if ((ResultPointer != &Information) || (Information.pw_name == NULL)) {
        Result = ENOENT;
        goto GetUserNameFromIdEnd;
    }

    if (UserName == NULL) {
        goto GetUserNameFromIdEnd;
    }

    NameSize = strlen(Information.pw_name) + 1;
    Name = malloc(NameSize);
    if (Name == NULL) {
        Result = ENOMEM;
        goto GetUserNameFromIdEnd;
    }

    memcpy(Name, Information.pw_name, NameSize);
    Result = 0;

GetUserNameFromIdEnd:
    if (Buffer != NULL) {
        free(Buffer);
    }

    if (Result != 0) {
        if (Name != NULL) {
            free(Name);
            Name = NULL;
        }
    }

    if (UserName != NULL) {
        *UserName = Name;
    }

    return Result;
}

int
SwGetUserIdFromName (
    char *UserName,
    uid_t *UserId
    )

/*++

Routine Description:

    This routine converts the given user name into an ID.

Arguments:

    UserName - Supplies a pointer to the string containing the user name.

    UserId - Supplies a pointer where the user ID will be returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    void *Buffer;
    struct passwd Information;
    int Result;
    struct passwd *ResultPointer;

    Buffer = malloc(USER_INFORMATION_BUFFER_SIZE);
    if (Buffer == NULL) {
        Result = ENOMEM;
        goto GetUserIdFromNameEnd;
    }

    Result = getpwnam_r(UserName,
                        &Information,
                        Buffer,
                        USER_INFORMATION_BUFFER_SIZE,
                        &ResultPointer);

    if (Result != 0) {
        goto GetUserIdFromNameEnd;
    }

    if (ResultPointer != &Information) {
        Result = ENOENT;
        goto GetUserIdFromNameEnd;
    }

    *UserId = Information.pw_uid;
    Result = 0;

GetUserIdFromNameEnd:
    if (Buffer != NULL) {
        free(Buffer);
    }

    return Result;
}

int
SwGetGroupNameFromId (
    gid_t GroupId,
    char **GroupName
    )

/*++

Routine Description:

    This routine converts the given group ID into a group name.

Arguments:

    GroupId - Supplies the group ID to query.

    GroupName - Supplies an optional pointer where a string will be returned
        containing the group name. The caller is responsible for freeing this
        memory.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    void *Buffer;
    struct group Information;
    char *Name;
    size_t NameSize;
    int Result;
    struct group *ResultPointer;

    Name = NULL;
    Buffer = malloc(GROUP_INFORMATION_BUFFER_SIZE);
    if (Buffer == NULL) {
        Result = ENOMEM;
        goto GetGroupNameFromIdEnd;
    }

    Result = getgrgid_r(GroupId,
                        &Information,
                        Buffer,
                        GROUP_INFORMATION_BUFFER_SIZE,
                        &ResultPointer);

    if (Result != 0) {
        Result = errno;
        goto GetGroupNameFromIdEnd;
    }

    if ((ResultPointer != &Information) || (Information.gr_name == NULL)) {
        Result = ENOENT;
        goto GetGroupNameFromIdEnd;
    }

    if (GroupName == NULL) {
        goto GetGroupNameFromIdEnd;
    }

    NameSize = strlen(Information.gr_name) + 1;
    Name = malloc(NameSize);
    if (Name == NULL) {
        Result = ENOMEM;
        goto GetGroupNameFromIdEnd;
    }

    memcpy(Name, Information.gr_name, NameSize);
    Result = 0;

GetGroupNameFromIdEnd:
    if (Buffer != NULL) {
        free(Buffer);
    }

    if (Result != 0) {
        if (Name != NULL) {
            free(Name);
            Name = NULL;
        }
    }

    if (GroupName != NULL) {
        *GroupName = Name;
    }

    return Result;
}

int
SwGetGroupIdFromName (
    char *GroupName,
    gid_t *GroupId
    )

/*++

Routine Description:

    This routine converts the given group name into a group ID.

Arguments:

    GroupName - Supplies a pointer to the group name to query.

    GroupId - Supplies a pointer where the group ID will be returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    void *Buffer;
    struct group Information;
    int Result;
    struct group *ResultPointer;

    Buffer = malloc(GROUP_INFORMATION_BUFFER_SIZE);
    if (Buffer == NULL) {
        Result = ENOMEM;
        goto GetGroupIdFromName;
    }

    Result = getgrnam_r(GroupName,
                        &Information,
                        Buffer,
                        GROUP_INFORMATION_BUFFER_SIZE,
                        &ResultPointer);

    if (Result != 0) {
        Result = errno;
        goto GetGroupIdFromName;
    }

    if (ResultPointer != &Information) {
        Result = ENOENT;
        goto GetGroupIdFromName;
    }

    *GroupId = Information.gr_gid;
    Result = 0;

GetGroupIdFromName:
    if (Buffer != NULL) {
        free(Buffer);
    }

    return Result;
}

int
SwGetUserInformationByName (
    char *UserName,
    PSWISS_USER_INFORMATION *UserInformation
    )

/*++

Routine Description:

    This routine gets information about a user based on their login name.

Arguments:

    UserName - Supplies the user name to query.

    UserInformation - Supplies a pointer where the information will be
        returned on success. The caller is responsible for freeing this
        structure when finished. All strings in this structure are in the
        same allocation as the structure itself.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    void *Buffer;
    struct passwd Information;
    int Result;
    struct passwd *ResultPointer;

    *UserInformation = NULL;
    Buffer = malloc(USER_INFORMATION_BUFFER_SIZE);
    if (Buffer == NULL) {
        Result = ENOMEM;
        goto GetUserInformationByNameEnd;
    }

    Result = getpwnam_r(UserName,
                        &Information,
                        Buffer,
                        USER_INFORMATION_BUFFER_SIZE,
                        &ResultPointer);

    if (Result != 0) {
        Result = errno;
        goto GetUserInformationByNameEnd;
    }

    if (ResultPointer != &Information) {
        Result = ENOENT;
        goto GetUserInformationByNameEnd;
    }

    Result = SwpConvertPasswdToUserInformation(ResultPointer, UserInformation);

GetUserInformationByNameEnd:
    if (Buffer != NULL) {
        free(Buffer);
    }

    return Result;
}

int
SwGetUserInformationById (
    uid_t UserId,
    PSWISS_USER_INFORMATION *UserInformation
    )

/*++

Routine Description:

    This routine gets information about a user based on their user ID.

Arguments:

    UserId - Supplies the user ID to query.

    UserInformation - Supplies a pointer where the information will be
        returned on success. The caller is responsible for freeing this
        structure when finished. All strings in this structure are in the
        same allocation as the structure itself.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    void *Buffer;
    struct passwd Information;
    int Result;
    struct passwd *ResultPointer;

    *UserInformation = NULL;
    Buffer = malloc(USER_INFORMATION_BUFFER_SIZE);
    if (Buffer == NULL) {
        Result = ENOMEM;
        goto GetUserInformationByIdEnd;
    }

    Result = getpwuid_r(UserId,
                        &Information,
                        Buffer,
                        USER_INFORMATION_BUFFER_SIZE,
                        &ResultPointer);

    if (Result != 0) {
        Result = errno;
        goto GetUserInformationByIdEnd;
    }

    if (ResultPointer != &Information) {
        Result = ENOENT;
        goto GetUserInformationByIdEnd;
    }

    Result = SwpConvertPasswdToUserInformation(ResultPointer, UserInformation);

GetUserInformationByIdEnd:
    if (Buffer != NULL) {
        free(Buffer);
    }

    return Result;
}

int
SwGetGroupList (
    uid_t UserId,
    gid_t GroupId,
    gid_t **Groups,
    size_t *GroupCount
    )

/*++

Routine Description:

    This routine gets the list of groups that the given user belongs to.

Arguments:

    UserId - Supplies the ID of the user to get groups for.

    GroupId - Supplies a group ID that if not in the list of groups the given
        user belongs to will also be included in the return returns. Typically
        this argument is specified as the group ID from the password record
        for the given user.

    Groups - Supplies a pointer where the group list will be returned on
        success. It is the caller's responsibility to free this data.

    GroupCount - Supplies a pointer where the number of elements in the group
        list will be returned on success.

Return Value:

    Returns the number of groups the user belongs to on success.

    -1 if the number of groups the user belongs to is greater than the size of
    the buffer passed in. In this case the group count parameter will contain
    the correct number.

--*/

{

    gid_t *Buffer;
    int Count;
    int Result;
    char *UserName;

    Buffer = NULL;
    Count = 0;
    UserName = NULL;
    Result = SwGetUserNameFromId(UserId, &UserName);
    if (Result != 0) {
        return Result;
    }

    assert(sizeof(gid_t) == sizeof(id_t));

    Result = getgrouplist(UserName, GroupId, (void *)Buffer, &Count);
    if (Count == 0) {
        Result = EINVAL;
        goto GetGroupListEnd;
    }

    //
    // Add some extra overhead in case this data is in flux.
    //

    Count += 8;
    Buffer = malloc(Count * sizeof(gid_t));
    if (Buffer == NULL) {
        Result = ENOMEM;
        goto GetGroupListEnd;
    }

    Result = getgrouplist(UserName, GroupId, (void *)Buffer, &Count);
    if (Result > 0) {
        Result = 0;
    }

GetGroupListEnd:
    if (Result != 0) {
        if (Buffer != NULL) {
            free(Buffer);
            Buffer = NULL;
        }

        Count = 0;
    }

    if (UserName != NULL) {
        free(UserName);
    }

    *Groups = Buffer;
    *GroupCount = Count;
    return Result;
}

unsigned long long
SwGetBlockCount (
    struct stat *Stat
    )

/*++

Routine Description:

    This routine returns the number of blocks used by the file. The size of a
    block is implementation specific.

Arguments:

    Stat - Supplies a pointer to the stat structure.

Return Value:

    Returns the number of blocks used by the given file.

--*/

{

    return Stat->st_blocks;
}

unsigned long
SwGetBlockSize (
    struct stat *Stat
    )

/*++

Routine Description:

    This routine returns the number of size of a block for this file.

Arguments:

    Stat - Supplies a pointer to the stat structure.

Return Value:

    Returns the block size for this file.

--*/

{

    return Stat->st_blksize;
}

int
SwMakeDirectory (
    const char *Path,
    unsigned long long CreatePermissions
    )

/*++

Routine Description:

    This routine calls the system to create a new directory.

Arguments:

    Path - Supplies the path string of the directory to create.

    CreatePermissions - Supplies the permission bits to create the file with.

Return Value:

    0 on success.

    -1 on failure and the errno variable will be set to provide more
    information.

--*/

{

    return mkdir(Path, (mode_t)CreatePermissions);
}

int
SwEvaluateFileTest (
    SWISS_FILE_TEST Operator,
    const char *Path,
    int *Error
    )

/*++

Routine Description:

    This routine evaluates a file test.

Arguments:

    Operator - Supplies the operator.

    Path - Supplies a pointer to the string containing the file path to test.

    Error - Supplies an optional pointer that returns zero if the function
        succeeded in truly determining the result, or non-zero if there was an
        error.

Return Value:

    0 if the test did not pass.

    Non-zero if the test succeeded.

--*/

{

    PSTR AfterScan;
    INT ErrorResult;
    INT FileDescriptor;
    BOOL FollowLinks;
    INT Result;
    struct stat Stat;

    ErrorResult = 0;
    Result = FALSE;

    //
    // Handle the "is a terminal" case separately since the path here is
    // actually a file descriptor number.
    //

    if (Operator == FileTestDescriptorIsTerminal) {
        FileDescriptor = strtol(Path, &AfterScan, 10);
        if ((FileDescriptor < 0) || (AfterScan == Path)) {
            ErrorResult = EINVAL;
            SwPrintError(0, (PSTR)Path, "Invalid file descriptor");
            goto EvaluateFileTestEnd;
        }

        if (isatty(FileDescriptor) == 1) {
            Result = TRUE;
        }

        goto EvaluateFileTestEnd;
    }

    //
    // Get the file information. If the file doesn't exist, none of the file
    // tests pass.
    //

    FollowLinks = TRUE;
    if (Operator == FileTestIsSymbolicLink) {
        FollowLinks = FALSE;
    }

    if (SwStat((PSTR)Path, FollowLinks, &Stat) != 0) {
        goto EvaluateFileTestEnd;
    }

    switch (Operator) {
    case FileTestIsBlockDevice:
        if (S_ISBLK(Stat.st_mode)) {
            Result = TRUE;
        }

        break;

    case FileTestIsCharacterDevice:
        if (S_ISCHR(Stat.st_mode)) {
            Result = TRUE;
        }

        break;

    case FileTestIsDirectory:
        if (S_ISDIR(Stat.st_mode)) {
            Result = TRUE;
        }

        break;

    case FileTestExists:
        Result = TRUE;
        break;

    case FileTestIsRegularFile:
        if (S_ISREG(Stat.st_mode)) {
            Result = TRUE;
        }

        break;

    case FileTestHasSetGroupId:
        if ((Stat.st_mode & S_ISGID) != 0) {
            Result = TRUE;
        }

        break;

    case FileTestIsSymbolicLink:
        if (S_ISLNK(Stat.st_mode)) {
            Result = TRUE;
        }

        break;

    case FileTestIsFifo:
        if (S_ISFIFO(Stat.st_mode)) {
            Result = TRUE;
        }

        break;

    case FileTestIsSocket:
        if (S_ISSOCK(Stat.st_mode)) {
            Result = TRUE;
        }

        break;

    case FileTestIsNonEmpty:
        if (Stat.st_size > 0) {
            Result = TRUE;
        }

        break;

    case FileTestHasSetUserId:
        if ((Stat.st_mode & S_ISUID) != 0) {
            Result = TRUE;
        }

        break;

    case FileTestCanRead:
    case FileTestCanWrite:
    case FileTestCanExecute:
        Result = SwpEvaluateFileAccess(Operator,
                                       Stat.st_mode,
                                       Stat.st_uid,
                                       Stat.st_gid,
                                       &ErrorResult);

        break;

    default:

        assert(FALSE);

        return FALSE;
    }

EvaluateFileTestEnd:
    if (Error != NULL) {
        *Error = ErrorResult;
    }

    return Result;
}

int
SwIsCurrentUserMemberOfGroup (
    unsigned long long Group,
    int *Error
    )

/*++

Routine Description:

    This routine determines if the current user is a member of the given group.

Arguments:

    Group - Supplies the group ID to check.

    Error - Supplies an optional pointer that returns zero if the function
        succeeded in truly determining the result, or non-zero if there was an
        error.

Return Value:

    Non-zero if the user is a member of the group.

    Zero on failure or if the user is not a member of the group.

--*/

{

    ULONG Capacity;
    INT ErrorResult;
    INT GroupCount;
    ULONG GroupIndex;
    gid_t *Groups;
    gid_t *NewGroups;
    BOOL Result;

    ErrorResult = 0;
    Groups = NULL;
    Result = FALSE;

    //
    // Return immediately if it's the primary group.
    //

    if ((Group == getgid()) || (Group == getegid())) {
        Result = TRUE;
        goto IsCurrentUserMemberOfGroupEnd;
    }

    Capacity = INITIAL_GROUP_COUNT;
    Groups = malloc(Capacity * sizeof(gid_t));
    if (Groups == NULL) {
        ErrorResult = ENOMEM;
        Result = FALSE;
        goto IsCurrentUserMemberOfGroupEnd;
    }

    //
    // Get the list of groups this user is a part of.
    //

    while (TRUE) {
        GroupCount = getgroups(Capacity, Groups);
        if (GroupCount < 0) {
            *Error = EINVAL;
            Result = FALSE;
            goto IsCurrentUserMemberOfGroupEnd;
        }

        if (GroupCount <= Capacity) {
            break;
        }

        Capacity = GroupCount;
        NewGroups = realloc(Groups, Capacity);
        if (NewGroups == NULL) {
            ErrorResult = ENOMEM;
            Result = FALSE;
            goto IsCurrentUserMemberOfGroupEnd;
        }

        Groups = NewGroups;
    }

    //
    // Look through the list to see if this group is in there.
    //

    Result = TRUE;
    for (GroupIndex = 0; GroupIndex < GroupCount; GroupIndex += 1) {
        if (Groups[GroupIndex] == Group) {
            Result = TRUE;
            goto IsCurrentUserMemberOfGroupEnd;
        }
    }

IsCurrentUserMemberOfGroupEnd:
    if (Groups != NULL) {
        free(Groups);
    }

    if (Error != NULL) {
        *Error = ErrorResult;
    }

    return Result;
}

INT
SwMakeFifo (
    PSTR Path,
    mode_t Permissions
    )

/*++

Routine Description:

    This routine creates a FIFO object.

Arguments:

    Path - Supplies a pointer to the path to create the FIFO at.

    Permissions - Supplies the permission bits.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    if (mkfifo(Path, Permissions) == 0) {
        return 0;
    }

    return errno;
}

INT
SwChangeFileOwner (
    PSTR FilePath,
    int FollowLinks,
    uid_t UserId,
    gid_t GroupId
    )

/*++

Routine Description:

    This routine changes the owner of the file or object at the given path.

Arguments:

    FilePath - Supplies a pointer to the path to change the owner of.

    FollowLinks - Supplies a boolean indicating whether the operation should
        occur on the link itself (FALSE) or the destination of a symbolic link
        (TRUE).

    UserId - Supplies the new user ID to set as the file owner.

    GroupId - Supplies the new group ID to set as the file group owner.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    int Status;

    if (FollowLinks != FALSE) {
        Status = chown(FilePath, UserId, GroupId);

    } else {
        Status = lchown(FilePath, UserId, GroupId);
    }

    if (Status == 0) {
        return 0;
    }

    return errno;
}

int
SwDoesPathHaveSeparators (
    char *Path
    )

/*++

Routine Description:

    This routine determines if the given path has separators in it or not.
    Usually this is just the presence of a slash (or on some OSes, a backslash
    too).

Arguments:

    Path - Supplies a pointer to the path.

Return Value:

    0 if the path has no separators.

    Non-zero if the path has separators.

--*/

{

    if (strchr(Path, '/') != NULL) {
        return TRUE;
    }

    return FALSE;
}

int
SwGetSystemName (
    PSYSTEM_NAME Name
    )

/*++

Routine Description:

    This routine returns the name and version of the system.

Arguments:

    Name - Supplies a pointer where the name information will be returned.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    int Result;
    struct utsname UtsName;

    Result = uname(&UtsName);
    if (Result != 0) {
        return errno;
    }

    strncpy(Name->SystemName, UtsName.sysname, sizeof(Name->SystemName));
    Name->SystemName[sizeof(Name->SystemName) - 1] = '\0';
    strncpy(Name->NodeName, UtsName.nodename, sizeof(Name->NodeName));
    Name->NodeName[sizeof(Name->NodeName) - 1] = '\0';
    strncpy(Name->Release, UtsName.release, sizeof(Name->Release));
    Name->Release[sizeof(Name->Release) - 1] = '\0';
    strncpy(Name->Version, UtsName.version, sizeof(Name->Version));
    Name->Version[sizeof(Name->Version) - 1] = '\0';
    strncpy(Name->Machine, UtsName.machine, sizeof(Name->Machine));
    Name->Machine[sizeof(Name->Machine) - 1] = '\0';

#if defined(__APPLE__) || defined(__CYGWIN__) || defined(__FreeBSD__)

    Name->DomainName[0] = '\0';

#else

    strncpy(Name->DomainName, UtsName.domainname, sizeof(Name->DomainName));
    Name->DomainName[sizeof(Name->DomainName) - 1] = '\0';

#endif

    return 0;
}

int
SwRunCommand (
    char *Command,
    char **Arguments,
    int ArgumentCount,
    int Asynchronous,
    int *ReturnValue
    )

/*++

Routine Description:

    This routine is called to run a command.

Arguments:

    Command - Supplies a pointer to the command name to run.

    Arguments - Supplies a pointer to an array of command argument strings.
        This includes the first argument, the command name.

    ArgumentCount - Supplies the number of arguments on the command line.

    Asynchronous - Supplies 0 if the shell should wait until the command is
        finished, or 1 if the function should return immediately with a
        return value of 0.

    ReturnValue - Supplies a pointer where the return value from the executed
        program will be returned.

Return Value:

    0 if the executable was successfully launched.

    Non-zero if there was trouble launching the executable.

--*/

{

    pid_t Child;
    int Result;
    int Status;

    *ReturnValue = -1;

    assert(ArgumentCount != 0);

    //
    // Fork off into another process.
    //

    Child = fork();
    if (Child == -1) {
        printf("Failed to fork. Errno %d\n", errno);
        return errno;
    }

    //
    // If this is the parent, either return happily or wait for the child.
    //

    if (Child != 0) {

        //
        // If the caller would like to wait until this process is finished,
        // then wait for it to exit.
        //

        if (Asynchronous == 0) {
            do {
                Result = waitpid(Child, &Status, 0);

            } while ((Result == -1) && (errno == EINTR));

            if (Result == Child) {
                *ReturnValue = Status;
            }

            if (Result == -1) {
                Result = errno;

            } else {
                Result = 0;
            }

        } else {
            Result = 0;
        }

    //
    // If this is the child, then run the desired process. This usually
    // doesn't come back.
    //

    } else {
        Result = execvp(Command, (char *const *)Arguments);
        fprintf(stderr, "Unable to exec %s: %s\n", Command, strerror(errno));
        exit(errno);
    }

    return Result;
}

int
SwExec (
    char *Command,
    char **Arguments,
    int ArgumentCount
    )

/*++

Routine Description:

    This routine is called to run the exec function, which replaces this
    process with another image.

Arguments:

    Command - Supplies a pointer to the command name to run.

    Arguments - Supplies a pointer to an array of command argument strings.
        This includes the first argument, the command name.

    ArgumentCount - Supplies the number of arguments on the command line.

Return Value:

    On success this routine does not return.

    Returns an error code on failure.

--*/

{

    assert(ArgumentCount != 0);

    execvp(Command, (char *const *)Arguments);
    return errno;
}

int
SwBreakDownTime (
    int LocalTime,
    time_t *Time,
    struct tm *TimeFields
    )

/*++

Routine Description:

    This routine converts a time value into its corresponding broken down
    calendar fields.

Arguments:

    LocalTime - Supplies a value that's non-zero to indicate the time value
        should be converted to local time, and 0 to indicate the time value
        should be converted to GMT.

    Time - Supplies a pointer to the time to convert.

    TimeFields - Supplies a pointer where the time fields will be returned.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    struct tm *Result;

    if (LocalTime != 0) {
        Result = localtime_r(Time, TimeFields);

    } else {
        Result = gmtime_r(Time, TimeFields);
    }

    if (Result == NULL) {
        return -1;
    }

    return 0;
}

pid_t
SwFork (
    VOID
    )

/*++

Routine Description:

    This routine forks the current execution into a duplicate process.

Arguments:

    None.

Return Value:

    0 to the child process.

    Returns the ID of the child to the parent process.

    -1 on failure or if the fork call is not supported on this operating system.

--*/

{

    pid_t Result;

    fflush(NULL);
    Result = fork();
    return Result;
}

char *
SwGetExecutableName (
    VOID
    )

/*++

Routine Description:

    This routine returns the path to the current executable.

Arguments:

    None.

Return Value:

    Returns a path to the executable on success.

    NULL if not supported by the OS.

--*/

{

    //
    // Not supported.
    //

    return NULL;
}

pid_t
SwWaitPid (
    pid_t Pid,
    int NonBlocking,
    int *Status
    )

/*++

Routine Description:

    This routine waits for a given process ID to complete.

Arguments:

    Pid - Supplies the process ID of the process to wait for.

    NonBlocking - Supplies a non-zero value if this wait should return
        immediately if there are no child processes available.

    Status - Supplies an optional pointer where the child exit status will be
        returned.

Return Value:

    Returns the process ID of the child that completed.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    int ChildStatus;
    int Flags;
    pid_t Result;

    Flags = 0;
    if (NonBlocking != 0) {
        Flags = WNOHANG;
    }

    ChildStatus = 0;
    do {
        Result = waitpid(Pid, &ChildStatus, Flags);

    } while ((Result == -1) && (errno == EINTR));

    if (Status != NULL) {
        *Status = ChildStatus;
    }

    return Result;
}

int
SwKill (
    pid_t ProcessId,
    int SignalNumber
    )

/*++

Routine Description:

    This routine sends a signal to a process or group of processes.

Arguments:

    ProcessId - Supplies the process ID of the process to send the signal to.
        If zero is supplied, then the signal will be sent to all processes in
        the process group. If the process ID is -1, the signal will be sent to
        all processes the signal can reach. If the process ID is negative but
        not negative 1, then the signal will be sent to all processes whose
        process group ID is equal to the absolute value of the process ID (and
        for which the process has permission to send the signal to).

    SignalNumber - Supplies the signal number to send. This value is expected
        to be one of the standard signal numbers (i.e. not a real time signal
        number).

Return Value:

    0 if a signal was actually sent to any processes.

    -1 on error, and the errno variable will contain more information about the
    error.

--*/

{

    return kill(ProcessId, SignalNumber);
}

int
SwOsStat (
    const char *Path,
    int FollowLinks,
    struct stat *Stat
    )

/*++

Routine Description:

    This routine stats a file.

Arguments:

    Path - Supplies a pointer to a string containing the path to stat.

    FollowLinks - Supplies a boolean indicating whether or not to follow
        symbolic links or return information about a link itself. Normally
        this should be non-zero (so links are followed).

    Stat - Supplies a pointer to the stat structure where the information will
        be returned on success.

Return Value:

    0 on success.

    Returns an error number on failure (the value from errno).

--*/

{

    INT Result;

    if (FollowLinks != 0) {
        Result = stat(Path, Stat);

    } else {
        Result = lstat(Path, Stat);
    }

    if (Result != 0) {
        Result = errno;
    }

    return Result;
}

int
SwSetBinaryMode (
    int FileDescriptor,
    int EnableBinaryMode
    )

/*++

Routine Description:

    This routine sets or clears the O_BINARY flag on a file.

Arguments:

    FileDescriptor - Supplies the descriptor to set the flag on.

    EnableBinaryMode - Supplies a non-zero value to set O_BINARY on the
        descriptor, or zero to clear the O_BINARY flag on the descriptor.

Return Value:

    0 on success.

    Returns an error number on failure (the value from errno).

--*/

{

    return 0;
}

int
SwReadInputCharacter (
    void
    )

/*++

Routine Description:

    This routine routine reads a single terminal character from standard input.

Arguments:

    None.

Return Value:

    Returns the character received from standard in on success.

    EOF on failure or if no more characters could be read.

--*/

{

    char Character;
    int Status;

    do {
        Status = read(STDIN_FILENO, &Character, 1);

    } while ((Status < 0) && (errno == EINTR));

    if (Status <= 0) {
        return EOF;
    }

    return Character;
}

void
SwMoveCursorRelative (
    void *Stream,
    int XPosition,
    char *String
    )

/*++

Routine Description:

    This routine moves the cursor a relative amount from its current position.

Arguments:

    Stream - Supplies a pointer to the output file stream.

    XPosition - Supplies the number of columns to move the cursor in the X
        position.

    String - Supplies a pointer to the characters that should exist at the
        given columns if the cursor is moving forward. This is unused if the
        cursor is moving backwards. This routine does not guarantee to print
        these characters, some implementations can move the cursor without
        printing.

Return Value:

    None.

--*/

{

    int Index;

    if (XPosition <= 0) {
        for (Index = 0; Index < -XPosition; Index += 1) {
            fputc('\b', Stream);
        }

    } else {

        assert(String != NULL);

        for (Index = 0; Index < XPosition; Index += 1) {

            assert(String[Index] != '\0');

            fputc(String[Index], Stream);
        }
    }

    return;
}

void
SwScrollTerminal (
    int Rows
    )

/*++

Routine Description:

    This routine scrolls the terminal screen.

Arguments:

    Rows - Supplies the number of rows to scroll the screen down. This can be
        negative to scroll the screen up.

Return Value:

    None.

--*/

{

    TERMINAL_COMMAND_DATA Command;
    BOOL Result;
    CHAR Sequence[10];

    if (Rows == 0) {
        return;
    }

    memset(&Command, 0, sizeof(TERMINAL_COMMAND_DATA));
    if (Rows > 0) {
        Command.Command = TerminalCommandScrollDown;

    } else {
        Command.Command = TerminalCommandScrollUp;
        Rows = -Rows;
    }

    if (Rows != 1) {
        Command.ParameterCount = 1;
        Command.Parameter[0] = Rows;
    }

    Result = TermCreateOutputSequence(&Command, Sequence, sizeof(Sequence));
    if (Result != FALSE) {
        Sequence[sizeof(Sequence) - 1] = '\0';
        fwrite(Sequence, 1, strlen(Sequence), stdout);
        fflush(stdout);
    }

    return;
}

void
SwMoveCursor (
    void *Stream,
    int XPosition,
    int YPosition
    )

/*++

Routine Description:

    This routine moves the cursor to an absolute location.

Arguments:

    Stream - Supplies a pointer to the output file stream.

    XPosition - Supplies the zero-based column number to move the cursor to.

    YPosition - Supplies the zero-based row number to move the cursor to.

Return Value:

    None.

--*/

{

    TERMINAL_COMMAND_DATA Command;
    BOOL Result;
    CHAR Sequence[10];

    memset(&Command, 0, sizeof(TERMINAL_COMMAND_DATA));
    Command.Command = TerminalCommandCursorMove;
    Command.ParameterCount = 2;
    Command.Parameter[0] = YPosition + 1;
    Command.Parameter[1] = XPosition + 1;
    Result = TermCreateOutputSequence(&Command, Sequence, sizeof(Sequence));
    if (Result != FALSE) {
        Sequence[sizeof(Sequence) - 1] = '\0';
        fwrite(Sequence, 1, strlen(Sequence), Stream);
        fflush(Stream);
    }

    return;
}

void
SwEnableCursor (
    void *Stream,
    int Enable
    )

/*++

Routine Description:

    This routine enables or disables display of the cursor.

Arguments:

    Stream - Supplies a pointer to the output file stream.

    Enable - Supplies a boolean. If non-zero the cursor will be displayed. If
        zero, the cursor will be hidden.

Return Value:

    None.

--*/

{

    TERMINAL_COMMAND_DATA Command;
    BOOL Result;
    CHAR Sequence[10];

    memset(&Command, 0, sizeof(TERMINAL_COMMAND_DATA));
    if (Enable != 0) {
        Command.Command = TerminalCommandSetPrivateMode;

    } else {
        Command.Command = TerminalCommandClearPrivateMode;
    }

    Command.ParameterCount = 1;
    Command.Parameter[0] = TERMINAL_PRIVATE_MODE_CURSOR;
    Result = TermCreateOutputSequence(&Command, Sequence, sizeof(Sequence));
    if (Result != FALSE) {
        Sequence[sizeof(Sequence) - 1] = '\0';
        fwrite(Sequence, 1, strlen(Sequence), Stream);
        fflush(stdout);
    }

    return;
}

int
SwGetTerminalDimensions (
    int *XSize,
    int *YSize
    )

/*++

Routine Description:

    This routine gets the dimensions of the current terminal.

Arguments:

    XSize - Supplies a pointer where the number of columns will be returned.

    YSize - Supplies a pointer where the number of rows will be returned.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    int Result;
    struct winsize WindowSize;

    memset(&WindowSize, 0, sizeof(struct winsize));
    Result = ioctl(STDOUT_FILENO, TIOCGWINSZ, &WindowSize);
    if (Result != 0) {
        if (errno == 0) {
            return -1;
        }

        return errno;
    }

    if (XSize != NULL) {
        *XSize = WindowSize.ws_col;
    }

    if (YSize != NULL) {
        *YSize = WindowSize.ws_row;
    }

    return 0;
}

int
SwPrintInColor (
    CONSOLE_COLOR Background,
    CONSOLE_COLOR Foreground,
    const char *Format,
    ...
    )

/*++

Routine Description:

    This routine prints a formatted message to the console in color.

Arguments:

    Background - Supplies the background color.

    Foreground - Supplies the foreground color.

    Format - Supplies the format string to print.

    ... - Supplies the remainder of the print format arguments.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    va_list ArgumentList;

    //
    // If standard out is not a terminal, don't print color codes.
    //

    fflush(NULL);
    SwpSetColors(Background, Foreground);

    //
    // Now write the command.
    //

    va_start(ArgumentList, Format);
    vprintf(Format, ArgumentList);
    va_end(ArgumentList);
    SwpSetColors(ConsoleColorDefault, ConsoleColorDefault);
    fflush(NULL);
    return 0;
}

int
SwClearRegion (
    CONSOLE_COLOR Background,
    CONSOLE_COLOR Foreground,
    int Column,
    int Row,
    int Width,
    int Height
    )

/*++

Routine Description:

    This routine clears a region of the screen to the given foreground and
    background colors.

Arguments:

    Background - Supplies the background color to set for the region.

    Foreground - Supplies the foreground color to set for the region.

    Column - Supplies the zero-based column number of the upper-left region
        to clear.

    Row - Supplies the zero-based row number of the upper-left corner of the
        region to clear.

    Width - Supplies the width of the region to clear. Supply -1 to clear the
        whole width of the screen.

    Height - Supplies the height of the region to clear. Supply -1 to clear the
        whole height of the screen.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    TERMINAL_COMMAND_DATA CommandData;
    INT Index;
    BOOL Result;
    char Sequence[11];

    memset(&CommandData, 0, sizeof(TERMINAL_COMMAND_DATA));
    CommandData.Command = TerminalCommandEraseCharacters;
    CommandData.ParameterCount = 1;
    CommandData.Parameter[0] = Width;
    Result = TermCreateOutputSequence(&CommandData, Sequence, sizeof(Sequence));
    if (Result == FALSE) {
        return -1;
    }

    Sequence[sizeof(Sequence) - 1] = '\0';
    SwpSetColors(Background, Foreground);
    for (Index = 0; Index < Height; Index += 1) {
        SwMoveCursor(stdout, Column, Row + Index);
        printf("%s", Sequence);
    }

    return 0;
}

void
SwSleep (
    unsigned long long Microseconds
    )

/*++

Routine Description:

    This routine suspends the current thread for at least the given number of
    microseconds.

Arguments:

    Microseconds - Supplies the amount of time to sleep for in microseconds.

Return Value:

    None.

--*/

{

    unsigned long long Seconds;

    //
    // The usleep function probably only takes a 32-bit value. Use the
    // second-based sleep function to get within range of usleep.
    //

    while (Microseconds != (useconds_t)Microseconds) {
        Seconds = Microseconds / 1000000ULL;
        if (Seconds > UINT32_MAX) {
            Seconds = UINT32_MAX;
        }

        if (sleep((unsigned)Seconds) != 0) {
            return;
        }

        Microseconds -= Seconds * 1000000ULL;
    }

    usleep(Microseconds);
    return;
}

int
SwSetRealUserId (
    id_t UserId
    )

/*++

Routine Description:

    This routine sets the current real user ID.

Arguments:

    UserId - Supplies the user ID to set.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    int Status;

    Status = setuid(UserId);
    if (Status != 0) {
        return errno;
    }

    return 0;
}

int
SwSetEffectiveUserId (
    id_t UserId
    )

/*++

Routine Description:

    This routine sets the current effective user ID.

Arguments:

    UserId - Supplies the user ID to set.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    int Status;

    Status = seteuid(UserId);
    if (Status != 0) {
        return errno;
    }

    return 0;
}

int
SwSetRealGroupId (
    id_t GroupId
    )

/*++

Routine Description:

    This routine sets the current real group ID.

Arguments:

    GroupId - Supplies the group ID to set.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    int Status;

    Status = setgid(GroupId);
    if (Status != 0) {
        return errno;
    }

    return 0;
}

int
SwSetEffectiveGroupId (
    id_t GroupId
    )

/*++

Routine Description:

    This routine sets the current effective group ID.

Arguments:

    GroupId - Supplies the group ID to set.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    int Status;

    Status = setegid(GroupId);
    if (Status != 0) {
        return errno;
    }

    return 0;
}

id_t
SwGetRealUserId (
    void
    )

/*++

Routine Description:

    This routine returns the current real user ID.

Arguments:

    None.

Return Value:

    Returns the real user ID.

    -1 on failure.

--*/

{

    return getuid();
}

id_t
SwGetEffectiveUserId (
    void
    )

/*++

Routine Description:

    This routine returns the current effective user ID.

Arguments:

    None.

Return Value:

    Returns the effective user ID.

    -1 on failure.

--*/

{

    return geteuid();
}

id_t
SwGetRealGroupId (
    void
    )

/*++

Routine Description:

    This routine returns the current real group ID.

Arguments:

    None.

Return Value:

    Returns the real group ID.

    -1 on failure.

--*/

{

    return getgid();
}

id_t
SwGetEffectiveGroupId (
    void
    )

/*++

Routine Description:

    This routine returns the current effective group ID.

Arguments:

    None.

Return Value:

    Returns the effective group ID.

    -1 on failure.

--*/

{

    return getegid();
}

int
SwSetGroups (
    int Size,
    const gid_t *List
    )

/*++

Routine Description:

    This routine sets the list of supplementary groups the current user is
    a member of. The caller must have appropriate privileges.

Arguments:

    Size - Supplies the number of elements in the given list.

    List - Supplies a list where the group IDs will be returned.

Return Value:

    0 on success.

    -1 on failure, and errno is set to contain more information.

--*/

{

    return setgroups(Size, List);
}

int
SwGetTerminalId (
    void
    )

/*++

Routine Description:

    This routine returns the current terminal ID.

Arguments:

    None.

Return Value:

    Returns the terminal ID.

    -1 on failure.

--*/

{

    //
    // TODO: Get the terminal ID.
    //

    return 0;
}

int
SwGetTerminalNameFromId (
    unsigned long long TerminalId,
    char **TerminalName
    )

/*++

Routine Description:

    This routine converts the given terminal ID into a terminal name.

Arguments:

    TerminalId - Supplies the terminal ID to query.

    TerminalName - Supplies an optional pointer where the string will be
        returned containing the terminal name. The caller is responsible for
        freeing this memory.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    //
    // TODO: Get the terminal name.
    //

    if (TerminalName != NULL) {
        *TerminalName = NULL;
    }

    return EINVAL;
}

pid_t
SwGetSessionId (
    pid_t ProcessId
    )

/*++

Routine Description:

    This routine returns the process group ID of the process that is the
    session leader of the given process. If the given parameter is 0, then
    the current process ID is used as the parameter.

Arguments:

    ProcessId - Supplies a process ID of the process whose session leader
        should be returned.

Return Value:

    Returns the process group ID of the session leader of the specified process.

    -1 on failure.

--*/

{

    return getsid(ProcessId);
}

int
SwGetSessionNameFromId (
    unsigned long long SessionId,
    char **SessionName
    )

/*++

Routine Description:

    This routine converts the given terminal ID into a terminal name.

Arguments:

    SessionId - Supplies the terminal ID to query.

    SessionName - Supplies an optional pointer where the string will be
        returned containing the session name. The caller is responsible for
        freeing this memory.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    //
    // TODO: Get the session name.
    //

    if (SessionName != NULL) {
        *SessionName = NULL;
    }

    return EINVAL;
}

int
SwGetTimes (
    struct timeval *RealTime,
    struct timeval *UserTime,
    struct timeval *SystemTime
    )

/*++

Routine Description:

    This routine gets the specified times for the given process.

Arguments:

    RealTime - Supplies a pointer that receives the current wall clock time,
        represented as the number of seconds since the Epoch.

    UserTime - Supplies a pointer that receives the total number of CPU time
        the current process has spent in user mode.

    SystemTime - Supplies a pointer that receives the total number of CPU time
        the current process has spent in user mode.

Return Value:

    0 on success, or -1 on failure.

--*/

{

    long ClockTicksPerSecond;
    clock_t ElapsedRealTime;
    unsigned long long RemainingTicks;
    int Result;
    struct tms Times;
    clock_t TotalSystemTime;
    clock_t TotalUserTime;

    ClockTicksPerSecond = sysconf(_SC_CLK_TCK);
    if (ClockTicksPerSecond == -1) {
        return -1;
    }

    ElapsedRealTime = times(&Times);
    if (ElapsedRealTime == -1) {
        return -1;
    }

    //
    // Don't used the elapsed real time from the times call as that is more
    // likely to roll over on some systems. Get it from the time of day.
    //

    Result = gettimeofday(RealTime, NULL);
    if (Result != 0) {
        return Result;
    }

    //
    // Convert the user and system times from ticks into seconds and
    // microseconds.
    //

    TotalUserTime = Times.tms_utime + Times.tms_cutime;
    UserTime->tv_sec = TotalUserTime / ClockTicksPerSecond;
    RemainingTicks = TotalUserTime % ClockTicksPerSecond;
    UserTime->tv_usec = (RemainingTicks * 1000000ULL) / ClockTicksPerSecond;
    TotalSystemTime = Times.tms_stime + Times.tms_cstime;
    SystemTime->tv_sec = TotalSystemTime/ ClockTicksPerSecond;
    RemainingTicks = TotalSystemTime % ClockTicksPerSecond;
    SystemTime->tv_usec = (RemainingTicks * 1000000ULL) / ClockTicksPerSecond;
    return 0;
}

int
SwRemoveDirectory (
    const char *Directory
    )

/*++

Routine Description:

    This routine attempts to remove the specified directory.

Arguments:

    Directory - Supplies a pointer to the string containing the directory to
        remove. This directory must be empty.

Return Value:

    0 on success, or -1 on failure.

--*/

{

    return rmdir(Directory);
}

int
SwUnlink (
    const char *Path
    )

/*++

Routine Description:

    This routine attempts to remove the specified file or directory.

Arguments:

    Path - Supplies a pointer to the string containing the path to delete.

Return Value:

    0 on success, or -1 on failure.

--*/

{

    return unlink(Path);
}

int
SwSetTimeOfDay (
    const struct timeval *NewTime,
    void *UnusedParameter
    )

/*++

Routine Description:

    This routine sets the current time in terms of seconds from the Epoch,
    midnight on January 1, 1970 GMT. The timezone is always GMT. The caller
    must have appropriate privileges to set the system time.

Arguments:

    NewTime - Supplies a pointer where the result will be returned.

    UnusedParameter - Supplies an unused parameter provided for legacy reasons.
        It used to provide the current time zone.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    return settimeofday(NewTime, UnusedParameter);
}

time_t
SwConvertGmtTime (
    struct tm *Time
    )

/*++

Routine Description:

    This routine converts a broken down time structure, given in GMT time,
    back into its corresponding time value, in seconds since the Epoch,
    midnight on January 1, 1970 GMT. It will also normalize the given time
    structure so that each member is in the correct range.

Arguments:

    Time - Supplies a pointer to the broken down time structure.

Return Value:

    Returns the time value corresponding to the given broken down time.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    return timegm(Time);
}

size_t
SwGetPageSize (
    void
    )

/*++

Routine Description:

    This routine gets the current page size on the system.

Arguments:

    None.

Return Value:

    Returns the size of a page on success.

    -1 on failure.

--*/

{

    return sysconf(_SC_PAGE_SIZE);
}

int
SwChroot (
    const char *Path
    )

/*++

Routine Description:

    This routine changes the current root directory. The working directory is
    not changed. The caller must have sufficient privileges to change root
    directories.

Arguments:

    Path - Supplies a pointer to the null terminated string containing the
        path of the new root directory.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    INT Status;

    Status = chroot(Path);
    if (Status != 0) {
        return errno;
    }

    return 0;
}

pid_t
SwGetProcessId (
    void
    )

/*++

Routine Description:

    This routine returns the current process ID.

Arguments:

    None.

Return Value:

    Returns the current process ID.

--*/

{

    return getpid();
}

int
SwRequestReset (
    SWISS_REBOOT_TYPE RebootType
    )

/*++

Routine Description:

    This routine initiates a reboot of the running system.

Arguments:

    RebootType - Supplies the type of reboot to perform.

Return Value:

    0 if the reboot was successfully requested.

    Non-zero on error.

--*/

{

    INT Signal;

    switch (RebootType) {
    case RebootTypeCold:
    case RebootTypeWarm:
        Signal = SIGTERM;
        break;

    case RebootTypeHalt:
        Signal = SIGUSR2;
        break;

    default:
        return EINVAL;
    }

    //
    // Send a signal to init.
    //

    errno = 0;
    kill(1, Signal);
    return errno;
}

int
SwGetHostName (
    char *Name,
    size_t NameLength
    )

/*++

Routine Description:

    This routine returns the standard host name for the current machine.

Arguments:

    Name - Supplies a pointer where the null-terminated name will be returned
        on success.

    NameLength - Supplies the length of the name buffer in bytes.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to indicate the error.

--*/

{

    return gethostname(Name, NameLength);
}

int
SwGetMonotonicClock (
    struct timespec *Time
    )

/*++

Routine Description:

    This routine returns the current high resolution time.

Arguments:

    Time - Supplies a pointer where the time value (relative to an arbitrary
        epoch) will be returned.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    return clock_gettime(CLOCK_MONOTONIC, Time);
}

int
SwSaveTerminalMode (
    void
    )

/*++

Routine Description:

    This routine saves the current terminal mode as the mode to restore.

Arguments:

    None.

Return Value:

    1 on success.

    0 on failure.

--*/

{

    int Result;

    Result = tcgetattr(STDIN_FILENO, &SwOriginalTerminalSettings);
    if (Result == 0) {
        SwOriginalTerminalSettingsValid = 1;
        return 1;
    }

    return 0;
}

int
SwSetRawInputMode (
    char *BackspaceCharacter,
    char *KillCharacter
    )

/*++

Routine Description:

    This routine sets the terminal into raw input mode.

Arguments:

    BackspaceCharacter - Supplies an optional pointer where the backspace
        character will be returned on success.

    KillCharacter - Supplies an optional pointer where the kill character
        will be returned on success.

Return Value:

    1 on success.

    0 on failure.

--*/

{

    int Result;
    struct termios TerminalSettings;

    if (SwOriginalTerminalSettingsValid == 0) {
        if (!SwSaveTerminalMode()) {
            return 0;
        }
    }

    memcpy(&TerminalSettings,
           &SwOriginalTerminalSettings,
           sizeof(TerminalSettings));

    //
    // Disable break, CR to NL, parity check, strip characters, and output
    // control.
    //

    TerminalSettings.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

    //
    // Set 8 bit characters.
    //

    TerminalSettings.c_cflag |= CS8;

    //
    // Change the local mode to disable canonical mode, echo, erase, extended
    // functions, and signal characters.
    //

    TerminalSettings.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG | ECHONL);

    //
    // Return immediately always.
    //

    TerminalSettings.c_cc[VMIN] = 1;
    TerminalSettings.c_cc[VTIME] = 0;

    //
    // Set these new parameters.
    //

    Result = tcsetattr(STDIN_FILENO, TCSADRAIN, &TerminalSettings);
    if (Result != 0) {
        printf("Failed to set raw input mode.\n");
        return 0;
    }

    if (BackspaceCharacter != NULL) {
        *BackspaceCharacter = TerminalSettings.c_cc[VERASE];
    }

    if (KillCharacter != NULL) {
        *KillCharacter = TerminalSettings.c_cc[VKILL];
    }

    return 1;
}

void
SwRestoreInputMode (
    void
    )

/*++

Routine Description:

    This routine restores the terminal's input mode if it was put into raw mode
    earlier. If it was not, this is a no-op.

Arguments:

    None.

Return Value:

    None.

--*/

{

    if (SwOriginalTerminalSettingsValid != 0) {
        tcsetattr(STDIN_FILENO, TCSADRAIN, &SwOriginalTerminalSettings);
    }

    return;
}

int
SwGetProcessorCount (
    int Online
    )

/*++

Routine Description:

    This routine returns the number of processors in the system.

Arguments:

    Online - Supplies a boolean indicating whether to return only the number
        of processors currently online (TRUE), or the total number (FALSE).

Return Value:

    Returns the number of processors on success.

    -1 on failure.

--*/

{

    long Result;

    if (Online != 0) {
        Result = sysconf(_SC_NPROCESSORS_ONLN);

    } else {
        Result = sysconf(_SC_NPROCESSORS_CONF);
    }

    return Result;
}

int
SwOpen (
    const char *Path,
    int OpenFlags,
    mode_t Mode
    )

/*++

Routine Description:

    This routine opens a file and connects it to a file descriptor.

Arguments:

    Shell - Supplies a pointer to the shell.

    Path - Supplies a pointer to a null terminated string containing the path
        of the file to open.

    OpenFlags - Supplies a set of flags ORed together. See O_* definitions.

    Mode - Supplies an optional integer representing the permission mask to set
        if the file is to be created by this open call.

Return Value:

    Returns a file descriptor on success.

    -1 on failure. The errno variable will be set to indicate the error.

--*/

{

    return open(Path, OpenFlags, Mode);
}

//
// --------------------------------------------------------- Internal Functions
//

int
SwpEvaluateFileAccess (
    SWISS_FILE_TEST Operator,
    mode_t StatMode,
    uid_t FileOwner,
    gid_t FileGroup,
    int *Error
    )

/*++

Routine Description:

    This routine evaluates a file access test (can read/write/execute).

Arguments:

    Operator - Supplies the operator.

    StatMode - Supplies the st_mode bits from the stat call.

    FileOwner - Supplies the ID of the file's owner.

    FileGroup - Supplies the ID of the group that owns the file.

    Error - Supplies an optional pointer that returns zero if the function
        succeeded in truly determining the result, or non-zero if there was an
        error.

Return Value:

    Non-zero if the current user has the requested access to the file.

    Zero on error or if the user does not have acecss.

--*/

{

    uid_t EffectiveUserId;
    int ErrorResult;
    BOOL IsMember;
    BOOL Result;

    assert((Operator == FileTestCanRead) || (Operator == FileTestCanWrite) ||
           (Operator == FileTestCanExecute));

    EffectiveUserId = geteuid();
    ErrorResult = 0;
    Result = FALSE;

    //
    // Root can read from or write to any file, and can execute any file where
    // some execute bit is set.
    //

    if (EffectiveUserId == 0) {
        if ((Operator == FileTestCanRead) || (Operator == FileTestCanWrite)) {
            Result = TRUE;
        }

        if ((StatMode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0) {
            Result = TRUE;
        }

        goto EvaluateFileAccessEnd;
    }

    //
    // If this user owns the file, then check the user bits.
    //

    if (EffectiveUserId == FileOwner) {
        if (Operator == FileTestCanRead) {
            if ((StatMode & S_IRUSR) != 0) {
                Result = TRUE;
            }

        } else if (Operator == FileTestCanWrite) {
            if ((StatMode & S_IWUSR) != 0) {
                Result = TRUE;
            }

        } else {
            if ((StatMode & S_IXUSR) != 0) {
                Result = TRUE;
            }
        }
    }

    //
    // The most annoying one: check to see if the current user is a member of
    // the group that owns the file.
    //

    IsMember = SwIsCurrentUserMemberOfGroup(FileGroup, &ErrorResult);
    if (IsMember != FALSE) {
        if (Operator == FileTestCanRead) {
            if ((StatMode & S_IRGRP) != 0) {
                Result = TRUE;
            }

        } else if (Operator == FileTestCanWrite) {
            if ((StatMode & S_IWGRP) != 0) {
                Result = TRUE;
            }

        } else {
            if ((StatMode & S_IXGRP) != 0) {
                Result = TRUE;
            }
        }

    //
    // The current user isn't a member of the group, use the other bits.
    //

    } else {
        if (Operator == FileTestCanRead) {
            if ((StatMode & S_IROTH) != 0) {
                Result = TRUE;
            }

        } else if (Operator == FileTestCanWrite) {
            if ((StatMode & S_IWOTH) != 0) {
                Result = TRUE;
            }

        } else {
            if ((StatMode & S_IXOTH) != 0) {
                Result = TRUE;
            }
        }
    }

EvaluateFileAccessEnd:
    if (Error != NULL) {
        *Error = ErrorResult;
    }

    return Result;
}

int
SwpConvertPasswdToUserInformation (
    struct passwd *Passwd,
    PSWISS_USER_INFORMATION *UserInformation
    )

/*++

Routine Description:

    This routine converts a password structure into a single-allocation user
    information structure.

Arguments:

    Passwd - Supplies a pointer to the passwd structure.

    UserInformation - Supplies a pointer where the user information structure
        will be returned on success. The caller is responsible for freeing
        this structure.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    size_t AllocationSize;
    char *Buffer;
    PSWISS_USER_INFORMATION User;

    *UserInformation = NULL;
    if (Passwd == NULL) {
        return EINVAL;
    }

    AllocationSize = sizeof(SWISS_USER_INFORMATION);
    if (Passwd->pw_name != NULL) {
        AllocationSize += strlen(Passwd->pw_name) + 1;
    }

    if (Passwd->pw_passwd != NULL) {
        AllocationSize += strlen(Passwd->pw_passwd) + 1;
    }

    if (Passwd->pw_gecos != NULL) {
        AllocationSize += strlen(Passwd->pw_gecos) + 1;
    }

    if (Passwd->pw_dir != NULL) {
        AllocationSize += strlen(Passwd->pw_dir) + 1;
    }

    if (Passwd->pw_shell != NULL) {
        AllocationSize += strlen(Passwd->pw_shell) + 1;
    }

    Buffer = malloc(AllocationSize);
    if (Buffer == NULL) {
        return ENOMEM;
    }

    memset(Buffer, 0, AllocationSize);
    User = (PSWISS_USER_INFORMATION)Buffer;
    Buffer = (char *)(User + 1);
    User->UserId = Passwd->pw_uid;
    User->GroupId = Passwd->pw_gid;
    if (Passwd->pw_name != NULL) {
        User->Name = strcpy(Buffer, Passwd->pw_name);
        Buffer += strlen(Passwd->pw_name) + 1;
    }

    if (Passwd->pw_passwd != NULL) {
        User->Password = strcpy(Buffer, Passwd->pw_passwd);
        Buffer += strlen(Passwd->pw_passwd) + 1;
    }

    if (Passwd->pw_gecos != NULL) {
        User->Gecos = strcpy(Buffer, Passwd->pw_gecos);
        Buffer += strlen(Passwd->pw_gecos) + 1;
    }

    if (Passwd->pw_dir != NULL) {
        User->Directory = strcpy(Buffer, Passwd->pw_dir);
        Buffer += strlen(Passwd->pw_dir) + 1;
    }

    if (Passwd->pw_shell != NULL) {
        User->Name = strcpy(Buffer, Passwd->pw_shell);
        Buffer += strlen(Passwd->pw_shell) + 1;
    }

    *UserInformation = User;
    return 0;
}

int
SwpSetColors (
    CONSOLE_COLOR Background,
    CONSOLE_COLOR Foreground
    )

/*++

Routine Description:

    This routine prints a formatted message to the console in color.

Arguments:

    Background - Supplies the background color.

    Foreground - Supplies the foreground color.

    Format - Supplies the format string to print.

    ... - Supplies the remainder of the print format arguments.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    TERMINAL_COMMAND_DATA CommandData;
    int IsTty;
    BOOL Result;
    char Sequence[11];

    IsTty = isatty(STDOUT_FILENO);
    if (IsTty == 0) {
        return 0;
    }

    memset(&CommandData, 0, sizeof(TERMINAL_COMMAND_DATA));
    CommandData.Command = TerminalCommandSelectGraphicRendition;

    //
    // Bold background colors are not supported, just shift to the non-bold
    // ones.
    //

    if (Background >= ConsoleColorBoldDefault) {
        Background -= ConsoleColorBoldDefault;
    }

    if (Foreground >= ConsoleColorBoldDefault) {
        Foreground -= ConsoleColorBoldDefault;
        CommandData.Parameter[0] = TERMINAL_GRAPHICS_BOLD;
        CommandData.ParameterCount += 1;
    }

    if (Foreground != ConsoleColorDefault) {
        CommandData.Parameter[CommandData.ParameterCount] =
                 TERMINAL_GRAPHICS_FOREGROUND + Foreground - ConsoleColorBlack;

        CommandData.ParameterCount += 1;
    }

    if (Background != ConsoleColorDefault) {
        CommandData.Parameter[CommandData.ParameterCount] =
                 TERMINAL_GRAPHICS_BACKGROUND + Background - ConsoleColorBlack;

        CommandData.ParameterCount += 1;
    }

    Result = TermCreateOutputSequence(&CommandData, Sequence, sizeof(Sequence));
    if (Result != FALSE) {
        Sequence[sizeof(Sequence) - 1] = '\0';
        printf("%s", Sequence);
    }

    return 0;
}

