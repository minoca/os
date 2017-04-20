/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ntos.c

Abstract:

    This module implements the Win32 operating system dependent portion of the
    Swiss common library.

Author:

    Evan Green 2-Jul-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#define _WIN32_WINNT 0x0501

#include <windows.h>
#include <psapi.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "../swlibos.h"

//
// ---------------------------------------------------------------- Definitions
//

#define UNAME_NT_SYSTEM_NAME "MINGW32_NT-%d.%d"

#define NTOS_TERMINAL_CHARACTER_SIZE 5

//
// Define ANSI escape codes.
//

#define ANSI_ESCAPE_CODE 0x1B
#define ANSI_ESCAPE_INTRODUCER '['
#define RUBOUT_CHARACTER 0x7F

//
// Define the number of times to retry a remove directory.
//

#define UNLINK_RETRY_COUNT 20
#define UNLINK_RETRY_DELAY 50

//
// Define the number of seconds from the NT Epoch (midnight January 1, 1601
// GMT) to the Unix Epoc (midnight January 1, 1970).
//

#define NT_EPOCH_TO_UNIX_EPOCH_SECONDS 11644473600LL

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

int
SwpEscapeArguments (
    char **Arguments,
    unsigned long ArgumentCount,
    char ***NewArgumentArray
    );

void
SwpGetTimeOfDay (
    struct timeval *Time
    );

void
SwpConvertNtFileTimeToUnixTime (
    FILETIME *NtTime,
    struct timeval *UnixTime
    );

void
SwpConvertUnixTimeToNtFileTime (
    const struct timeval *UnixTime,
    FILETIME *NtTime
    );

void
SwpConvertNtFileTimeToTimeval (
    FILETIME *NtTime,
    struct timeval *UnixTime
    );

int
SwpReadSheBang (
    char *Command,
    char **NewCommand,
    char **NewArgument
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
    {SIGABRT, "ABRT"},
    {SIGKILL, "KILL"},
    {SIGALRM, "ALRM"},
    {SIGTERM, "TERM"},
    {SIGCONT, "CONT"},
    {SIGSTOP, "STOP"},
    {-1, NULL},
};

//
// Prevent Windows from "globbing": converting things like asterisks into all
// the files in the current directory.
//

int _CRT_glob = 0;

//
// Store a global that's non-zero if this OS supports forking.
//

int SwForkSupported = 0;

//
// Store a global that's non-zero if this OS supports symbolic links.
//

int SwSymlinkSupported = 0;

//
// Store a buffer to the original executable path.
//

char SwExecutablePath[MAX_PATH];

//
// Store buffered keys.
//

CHAR SwCharacterBuffer[NTOS_TERMINAL_CHARACTER_SIZE];
ULONG SwCharacterCount;
ULONG SwCharacterIndex;
ULONG SwCharacterRepeatCount;

//
// Store the original console mode.
//

DWORD SwOriginalConsoleMode;
BOOL SwConsoleModeSaved;

//
// Store the conversion from the color enum to the console attributes.
//

WORD SwForegroundColors[ConsoleColorCount] = {
    0,
    0,
    FOREGROUND_RED,
    FOREGROUND_GREEN,
    FOREGROUND_RED | FOREGROUND_GREEN,
    FOREGROUND_BLUE,
    FOREGROUND_RED | FOREGROUND_BLUE,
    FOREGROUND_GREEN | FOREGROUND_BLUE,
    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
    0,
    FOREGROUND_INTENSITY,
    FOREGROUND_RED | FOREGROUND_INTENSITY,
    FOREGROUND_GREEN | FOREGROUND_INTENSITY,
    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
    FOREGROUND_BLUE | FOREGROUND_INTENSITY,
    FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
    FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
};

WORD SwBackgroundColors[ConsoleColorCount] = {
    0,
    0,
    BACKGROUND_RED,
    BACKGROUND_GREEN,
    BACKGROUND_RED | BACKGROUND_GREEN,
    BACKGROUND_BLUE,
    BACKGROUND_RED | BACKGROUND_BLUE,
    BACKGROUND_GREEN | BACKGROUND_BLUE,
    BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE,
    0,
    BACKGROUND_INTENSITY,
    BACKGROUND_RED | BACKGROUND_INTENSITY,
    BACKGROUND_GREEN | BACKGROUND_INTENSITY,
    BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_INTENSITY,
    BACKGROUND_BLUE | BACKGROUND_INTENSITY,
    BACKGROUND_RED | BACKGROUND_BLUE | BACKGROUND_INTENSITY,
    BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY,
    BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY,
};

//
// Store a buffer initialized to the NT security authority.
//

SID_IDENTIFIER_AUTHORITY SwNtAuthority = {SECURITY_NT_AUTHORITY};
BOOL SwIsAdministratorDetermined = FALSE;
BOOL SwIsAdministrator = FALSE;

//
// Remember whether or not WSAStartup has been called.
//

BOOL SwWsaStartupCalled = FALSE;

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

    assert(0);

    return EINVAL;
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

    return ENOSYS;
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

    return ENOSYS;
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

    if (UserName != NULL) {
        *UserName = NULL;
    }

    return EINVAL;
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

    return EINVAL;
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

    if (GroupName != NULL) {
        *GroupName = NULL;
    }

    return EINVAL;
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

    return EINVAL;
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

    *UserInformation = NULL;
    return ENOSYS;
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

    *UserInformation = NULL;
    return ENOSYS;
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

    Buffer = malloc(sizeof(gid_t));
    if (Buffer == NULL) {
        return ENOMEM;
    }

    *Buffer = GroupId;
    *Groups = Buffer;
    *GroupCount = 1;
    return 0;
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

    return Stat->st_size / 512;
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

    return 512;
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

    return mkdir(Path);
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
    PSTR ExePath;
    INT FileDescriptor;
    size_t PathLength;
    INT Result;
    struct stat Stat;

    ErrorResult = 0;
    ExePath = NULL;
    Result = FALSE;

    //
    // Handle the "is a terminal" case separately since the path here is
    // actually a file descriptor number.
    //

    if (Operator == FileTestDescriptorIsTerminal) {
        FileDescriptor = strtol(Path, &AfterScan, 10);
        if ((FileDescriptor < 0) || (AfterScan == Path)) {
            ErrorResult = EINVAL;
            goto EvaluateFileTestEnd;
        }

        if (isatty(FileDescriptor) != 0) {
            Result = TRUE;
        }

        goto EvaluateFileTestEnd;
    }

    //
    // Get the file information. If the file doesn't exist, none of the file
    // tests pass.
    //

    if (SwOsStat(Path, TRUE, &Stat) != 0) {

        //
        // Try it with a .exe on the end if it, as a lot of build scripts
        // fail to properly put .exe on their binary names.
        //

        PathLength = strlen(Path);
        ExePath = malloc(PathLength + sizeof(".exe"));
        if (ExePath == NULL) {
            goto EvaluateFileTestEnd;
        }

        strcpy(ExePath, Path);
        strcpy(ExePath + PathLength, ".exe");
        Result = SwOsStat(ExePath, TRUE, &Stat);
        free(ExePath);
        if (Result != 0) {
            Result = FALSE;
            goto EvaluateFileTestEnd;
        }
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

    case FileTestIsFifo:
        if (S_ISFIFO(Stat.st_mode)) {
            Result = TRUE;
        }

        break;

    case FileTestIsNonEmpty:
        if (Stat.st_size > 0) {
            Result = TRUE;
        }

        break;

    case FileTestCanRead:
        if ((Stat.st_mode & S_IRUSR) != 0) {
            Result = TRUE;
        }

        break;

    case FileTestCanWrite:
        if ((Stat.st_mode & S_IWUSR) != 0) {
            Result = TRUE;
        }

        break;

    case FileTestCanExecute:
        if ((Stat.st_mode & S_IXUSR) != 0) {
            Result = TRUE;
        }

        break;

    case FileTestIsSymbolicLink:
    case FileTestIsSocket:
    case FileTestHasSetGroupId:
    case FileTestHasSetUserId:
        break;

    default:

        assert(FALSE);

        break;
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

    if (Error != NULL) {
        *Error = 0;
    }

    return FALSE;
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

    return ENOSYS;
}

INT
SwCreateSymlink (
    PSTR LinkTarget,
    PSTR LinkName
    )

/*++

Routine Description:

    This routine creates a symbolic link.

Arguments:

    LinkTarget - Supplies the destination that the symbolic link will point to.

    LinkName - Supplies the file path of the symbolic link itself.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    return ENOSYS;
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

    return 0;
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

    //
    // It's an absolute path if it has a forward slash or a backslash.
    //

    if ((strchr(Path, '/') != NULL) || (strchr(Path, '\\') != NULL)) {
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

    DWORD NodeNameSize;
    SYSTEM_INFO SystemInfo;
    OSVERSIONINFOEX VersionInfo;

    memset(&VersionInfo, 0, sizeof(VersionInfo));
    VersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    memset(&SystemInfo, 0, sizeof(SystemInfo));
    GetSystemInfo(&SystemInfo);
    GetVersionEx((LPOSVERSIONINFO)&VersionInfo);
    NodeNameSize = sizeof(Name->NodeName);
    GetComputerName(Name->NodeName, &NodeNameSize);
    snprintf(Name->SystemName,
             sizeof(Name->SystemName),
             UNAME_NT_SYSTEM_NAME,
             VersionInfo.dwMajorVersion,
             VersionInfo.dwMinorVersion);

    snprintf(Name->Release,
             sizeof(Name->Release),
             "%d.%d",
             VersionInfo.dwMajorVersion,
             VersionInfo.dwMinorVersion);

    snprintf(Name->Version,
             sizeof(Name->Version),
             "%d %s",
             VersionInfo.dwBuildNumber,
             VersionInfo.szCSDVersion);

    switch (SystemInfo.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_AMD64:
        strcpy(Name->Machine, "x86_64");
        break;

    case PROCESSOR_ARCHITECTURE_ARM:
        strcpy(Name->Machine, "armv7");
        break;

    case PROCESSOR_ARCHITECTURE_IA64:
        strcpy(Name->Machine, "ia64");
        break;

    case PROCESSOR_ARCHITECTURE_INTEL:
    default:
        strcpy(Name->Machine, "i686");
        break;
    }

    NodeNameSize = sizeof(Name->DomainName);
    GetComputerNameEx(ComputerNameDnsDomain, Name->DomainName, &NodeNameSize);
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

    char **EscapedArguments;
    int InsertCount;
    int Mode;
    char **NewArguments;
    char *NewCommand;
    int Result;
    char *SheBangArgument;

    *ReturnValue = -1;
    EscapedArguments = NULL;

    //
    // Windows takes in a carefully laid out array of arguments and jams them
    // onto one command line, which then gets re-parsed out by spaces. The
    // result of which is that if there are arguments with spaces, they get
    // split by the newly running process. Escape them with double quotes.
    //

    Result = SwpEscapeArguments(Arguments, ArgumentCount, &EscapedArguments);
    if (Result != 0) {
        return Result;
    }

    if (Asynchronous != FALSE) {
        Mode = _P_NOWAIT;

    } else {
        Mode = _P_WAIT;
    }

    errno = 0;
    Result = _spawnvp(Mode, Command, (const char *const *)EscapedArguments);
    if (errno != 0) {
        Result = errno;
        if ((Result == ENOFILE) || (Result == ENOEXEC)) {
            Result = ENOEXEC;

            //
            // Look for a she-bang, and run that if it's there.
            //

            SwpReadSheBang(Command, &NewCommand, &SheBangArgument);
            if (NewCommand != NULL) {
                NewArguments = malloc((ArgumentCount + 3) * sizeof(char *));
                if (NewArguments == NULL) {
                    goto RunCommandEnd;
                }

                InsertCount = 1;
                if (SheBangArgument != NULL) {
                    InsertCount = 2;
                }

                memcpy(&(NewArguments[InsertCount]),
                       EscapedArguments,
                       (ArgumentCount + 1) * sizeof(char *));

                NewArguments[0] = NewCommand;
                if (SheBangArgument != NULL) {
                    NewArguments[1] = SheBangArgument;
                }

                errno = 0;
                Result = _spawnvp(Mode,
                                  NewCommand,
                                  (const char*const *)NewArguments);

                free(NewArguments);
                free(NewCommand);
                if (SheBangArgument != NULL) {
                    free(SheBangArgument);
                }

                if (errno != 0) {
                    Result = errno;
                    if (Result == ENOFILE) {
                        Result = ENOEXEC;
                    }
                }
            }

            //
            // If no she-bang or she-bang didn't work, try running the command
            // under sh (which is hopefully in the path).
            //

            if (errno != 0) {

                //
                // Try running the command under sh if it came back with an
                // executable format error.
                //

                NewArguments = malloc((ArgumentCount + 2) * sizeof(char *));
                if (NewArguments != NULL) {
                    memcpy(&(NewArguments[1]),
                           EscapedArguments,
                           (ArgumentCount + 1) * sizeof(char *));

                    NewArguments[0] = "sh.exe";
                    NewArguments[1] = Command;
                    Command = NewArguments[0];
                    errno = 0;
                    Result = _spawnvp(Mode,
                                      Command,
                                      (const char *const *)NewArguments);

                    free(NewArguments);
                    if (errno != 0) {
                        Result = errno;
                        if (Result == ENOFILE) {
                            Result = ENOEXEC;
                        }
                    }
                }
            }
        }
    }

    if (errno == 0) {
        if (Asynchronous == FALSE) {
            *ReturnValue = Result;

        } else {
            *ReturnValue = 0;
        }

        Result = 0;
    }

RunCommandEnd:
    free(EscapedArguments);
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

    char **EscapedArguments;
    int Result;

    Result = SwpEscapeArguments(Arguments, ArgumentCount, &EscapedArguments);
    if (Result != 0) {
        return Result;
    }

    Result = _spawnvp(_P_WAIT, Command, (const char *const *)EscapedArguments);
    free(EscapedArguments);
    if (Result == -1) {
        return Result;
    }

    _exit(Result);
    return Result;
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
        Result = localtime(Time);

    } else {
        Result = gmtime(Time);
    }

    if (Result == NULL) {
        return -1;
    }

    memcpy(TimeFields, Result, sizeof(struct tm));
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

    //
    // Windows can't do that.
    //

    return -1;
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

    DWORD Result;

    if (*SwExecutablePath != '\0') {
        return SwExecutablePath;
    }

    Result = GetModuleFileName(GetModuleHandle(NULL),
                               SwExecutablePath,
                               sizeof(SwExecutablePath));

    if (Result == 0) {
        return NULL;
    }

    return SwExecutablePath;
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

    return -1;
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

    DWORD Access;
    HANDLE Process;
    BOOL Result;
    INT Status;

    Process = NULL;
    Status = 0;

    //
    // Only handle the kill signal on NT.
    //

    if (SignalNumber == SIGKILL) {

        //
        // Open up a handle to the given process.
        //

        Access = PROCESS_TERMINATE;
        Process = OpenProcess(Access, FALSE, ProcessId);
        if (Process == NULL) {
            Status = -1;
            goto KillEnd;
        }

        //
        // Now terminate it.
        //

        Result = TerminateProcess(Process, 1);
        if (Result == FALSE) {
            Status = -1;
            goto KillEnd;
        }

        printf("Sent termination signal to process %d.\n", ProcessId);
    }

KillEnd:
    if (Process != NULL) {
        CloseHandle(Process);
    }

    return Status;
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

    size_t LastIndex;
    PSTR PathCopy;
    INT Result;

    //
    // Make a copy of the path and remove any trailing slashes.
    //

    PathCopy = strdup(Path);
    if (PathCopy == NULL) {
        return ENOMEM;
    }

    LastIndex = strlen(PathCopy);
    while ((LastIndex > 1) &&
           ((PathCopy[LastIndex - 1] == '/') ||
            (PathCopy[LastIndex - 1] == '\\'))) {

        PathCopy[LastIndex - 1] = '\0';
        LastIndex -= 1;
    }

    Result = stat(PathCopy, Stat);
    if (Result != 0) {
        Result = errno;
    }

    free(PathCopy);
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

    int Result;

    if (EnableBinaryMode != 0) {
        Result = _setmode(FileDescriptor, O_BINARY);

    } else {
        Result = _setmode(FileDescriptor, O_TEXT);
    }

    if (Result < 0) {
        return errno;
    }

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

    INT Character;
    BOOL ControlOrShiftPressed;
    DWORD ControlState;
    DWORD EventsRead;
    HANDLE Handle;
    CHAR PrimaryKey;
    INPUT_RECORD Record;
    DWORD Status;

    Handle = GetStdHandle(STD_INPUT_HANDLE);
    if (Handle == INVALID_HANDLE_VALUE) {
        return EOF;
    }

    //
    // Return buffered keys if they're there.
    //

    if (SwCharacterCount != 0) {

        assert(SwCharacterIndex < SwCharacterCount);

        Character = SwCharacterBuffer[SwCharacterIndex];
        SwCharacterIndex += 1;

        //
        // If the index reached the character size, reset it to zero.
        //

        if (SwCharacterIndex == SwCharacterCount) {
            SwCharacterIndex = 0;

            //
            // If this should repeat more times, decrement the repeat count
            // and leave the size where it was. Otherwise, the repeat count is
            // zero, so reset the size.
            //

            if (SwCharacterRepeatCount != 0) {
                SwCharacterRepeatCount -= 1;

            } else {
                SwCharacterCount = 0;
            }
        }

        return Character;
    }

    assert(SwCharacterCount == 0);

    while (TRUE) {
        Status = WaitForSingleObject(Handle, INFINITE);
        if (Status != WAIT_OBJECT_0) {
            continue;
        }

        Status = ReadConsoleInput(Handle, &Record, 1, &EventsRead);
        if ((Status == FALSE) || (EventsRead != 1)) {
            fprintf(stderr, "Error: ReadConsoleInput failed.\n");
            return EOF;
        }

        //
        // Skip anything except key events.
        //

        if (Record.EventType != KEY_EVENT) {
            continue;
        }

        //
        // Skip keys no one cares about.
        //

        if ((Record.Event.KeyEvent.bKeyDown == FALSE) ||
            ((Record.Event.KeyEvent.wVirtualKeyCode >= VK_SHIFT) &&
             (Record.Event.KeyEvent.wVirtualKeyCode <= VK_CAPITAL))) {

            continue;
        }

        assert(Record.Event.KeyEvent.wRepeatCount != 0);

        SwCharacterRepeatCount = Record.Event.KeyEvent.wRepeatCount - 1;
        PrimaryKey = Record.Event.KeyEvent.uChar.AsciiChar;

        //
        // Change backspace to rubout.
        //

        if (PrimaryKey == '\b') {
            PrimaryKey = RUBOUT_CHARACTER;
        }

        //
        // Make escape into the kill character for consistency with the Windows
        // shell.
        //

        if (PrimaryKey == ANSI_ESCAPE_CODE) {
            PrimaryKey = 0x0B;
        }

        ControlState = Record.Event.KeyEvent.dwControlKeyState;
        if ((ControlState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0) {
            SwCharacterBuffer[SwCharacterCount] = ANSI_ESCAPE_CODE;
            SwCharacterCount += 1;
        }

        ControlOrShiftPressed = FALSE;
        if ((ControlState &
             (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED | SHIFT_PRESSED)) != 0) {

            ControlOrShiftPressed = TRUE;
        }

        if ((ControlState & ENHANCED_KEY) != 0) {
            switch (Record.Event.KeyEvent.wVirtualKeyCode) {
            case VK_HOME:
                SwCharacterBuffer[SwCharacterCount] = ANSI_ESCAPE_CODE;
                SwCharacterCount += 1;
                SwCharacterBuffer[SwCharacterCount] = ANSI_ESCAPE_INTRODUCER;
                SwCharacterCount += 1;
                SwCharacterBuffer[SwCharacterCount] = '1';
                SwCharacterCount += 1;
                SwCharacterBuffer[SwCharacterCount] = '~';
                SwCharacterCount += 1;
                break;

            case VK_END:
                SwCharacterBuffer[SwCharacterCount] = ANSI_ESCAPE_CODE;
                SwCharacterCount += 1;
                SwCharacterBuffer[SwCharacterCount] = ANSI_ESCAPE_INTRODUCER;
                SwCharacterCount += 1;
                SwCharacterBuffer[SwCharacterCount] = '4';
                SwCharacterCount += 1;
                SwCharacterBuffer[SwCharacterCount] = '~';
                SwCharacterCount += 1;
                break;

            case VK_LEFT:
                SwCharacterBuffer[SwCharacterCount] = ANSI_ESCAPE_CODE;
                SwCharacterCount += 1;
                SwCharacterBuffer[SwCharacterCount] = ANSI_ESCAPE_INTRODUCER;
                if (ControlOrShiftPressed != FALSE) {
                    SwCharacterBuffer[SwCharacterCount] = 'O';
                }

                SwCharacterCount += 1;
                SwCharacterBuffer[SwCharacterCount] = 'D';
                SwCharacterCount += 1;
                break;

            case VK_RIGHT:
                SwCharacterBuffer[SwCharacterCount] = ANSI_ESCAPE_CODE;
                SwCharacterCount += 1;
                SwCharacterBuffer[SwCharacterCount] = ANSI_ESCAPE_INTRODUCER;
                if (ControlOrShiftPressed != FALSE) {
                    SwCharacterBuffer[SwCharacterCount] = 'O';
                }

                SwCharacterCount += 1;
                SwCharacterBuffer[SwCharacterCount] = 'C';
                SwCharacterCount += 1;
                break;

            case VK_UP:
                SwCharacterBuffer[SwCharacterCount] = ANSI_ESCAPE_CODE;
                SwCharacterCount += 1;
                SwCharacterBuffer[SwCharacterCount] = ANSI_ESCAPE_INTRODUCER;
                if (ControlOrShiftPressed != FALSE) {
                    SwCharacterBuffer[SwCharacterCount] = 'O';
                }

                SwCharacterCount += 1;
                SwCharacterBuffer[SwCharacterCount] = 'A';
                SwCharacterCount += 1;
                break;

            case VK_DOWN:
                SwCharacterBuffer[SwCharacterCount] = ANSI_ESCAPE_CODE;
                SwCharacterCount += 1;
                SwCharacterBuffer[SwCharacterCount] = ANSI_ESCAPE_INTRODUCER;
                if (ControlOrShiftPressed != FALSE) {
                    SwCharacterBuffer[SwCharacterCount] = 'O';
                }

                SwCharacterCount += 1;
                SwCharacterBuffer[SwCharacterCount] = 'B';
                SwCharacterCount += 1;
                break;

            case VK_DELETE:
                SwCharacterBuffer[SwCharacterCount] = ANSI_ESCAPE_CODE;
                SwCharacterCount += 1;
                SwCharacterBuffer[SwCharacterCount] = ANSI_ESCAPE_INTRODUCER;
                SwCharacterCount += 1;
                SwCharacterBuffer[SwCharacterCount] = '3';
                SwCharacterCount += 1;
                SwCharacterBuffer[SwCharacterCount] = '~';
                SwCharacterCount += 1;
                break;

            //
            // Handle Page Up.
            //

            case VK_PRIOR:
                SwCharacterBuffer[SwCharacterCount] = ANSI_ESCAPE_CODE;
                SwCharacterCount += 1;
                SwCharacterBuffer[SwCharacterCount] = ANSI_ESCAPE_INTRODUCER;
                SwCharacterCount += 1;
                SwCharacterBuffer[SwCharacterCount] = '5';
                SwCharacterCount += 1;
                SwCharacterBuffer[SwCharacterCount] = '~';
                SwCharacterCount += 1;
                break;

            //
            // Handle Page Down.
            //

            case VK_NEXT:
                SwCharacterBuffer[SwCharacterCount] = ANSI_ESCAPE_CODE;
                SwCharacterCount += 1;
                SwCharacterBuffer[SwCharacterCount] = ANSI_ESCAPE_INTRODUCER;
                SwCharacterCount += 1;
                SwCharacterBuffer[SwCharacterCount] = '6';
                SwCharacterCount += 1;
                SwCharacterBuffer[SwCharacterCount] = '~';
                SwCharacterCount += 1;
                break;

            //
            // Handle a keypad return (which is just a regular return).
            //

            case '\r':
                SwCharacterBuffer[SwCharacterCount] = PrimaryKey;
                SwCharacterCount += 1;
                break;

            default:

                //
                // Skip this record and get another one.
                //

                continue;
            }

        //
        // No fancy control stuff, just add the primary key.
        //

        } else {
            SwCharacterBuffer[SwCharacterCount] = PrimaryKey;
            SwCharacterCount += 1;
        }

        break;
    }

    //
    // Recurse just briefly to return what's been added in the key buffer.
    //

    assert(SwCharacterCount != 0);

    return SwReadInputCharacter();
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

    CONSOLE_SCREEN_BUFFER_INFO ConsoleInformation;
    HANDLE Handle;

    fflush(NULL);
    Handle = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(Handle, &ConsoleInformation);
    ConsoleInformation.dwCursorPosition.X += XPosition;
    while (ConsoleInformation.dwCursorPosition.X < 0) {
        ConsoleInformation.dwCursorPosition.X += ConsoleInformation.dwSize.X;
        ConsoleInformation.dwCursorPosition.Y -= 1;
    }

    while (ConsoleInformation.dwCursorPosition.X >=
           ConsoleInformation.dwSize.X) {

        ConsoleInformation.dwCursorPosition.X -= ConsoleInformation.dwSize.X;
        ConsoleInformation.dwCursorPosition.Y += 1;
    }

    SetConsoleCursorPosition(Handle, ConsoleInformation.dwCursorPosition);
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

    CONSOLE_SCREEN_BUFFER_INFO ConsoleInformation;
    HANDLE Handle;

    fflush(NULL);
    Handle = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(Handle, &ConsoleInformation);
    ConsoleInformation.dwCursorPosition.X = XPosition;
    ConsoleInformation.dwCursorPosition.Y = ConsoleInformation.srWindow.Top +
                                            YPosition;

    SetConsoleCursorPosition(Handle, ConsoleInformation.dwCursorPosition);
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

    CONSOLE_CURSOR_INFO Cursor;
    HANDLE Handle;

    fflush(NULL);
    Handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!GetConsoleCursorInfo(Handle, &Cursor)) {
        return;
    }

    if (Enable != 0) {
        Cursor.bVisible = TRUE;

    } else {
        Cursor.bVisible = FALSE;
    }

    SetConsoleCursorInfo(Handle, &Cursor);
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

    CONSOLE_SCREEN_BUFFER_INFO CurrentInfo;
    HANDLE Handle;
    BOOL Result;
    SMALL_RECT Window;

    if (Rows == 0) {
        return;
    }

    Handle = GetStdHandle(STD_OUTPUT_HANDLE);
    Result = GetConsoleScreenBufferInfo(Handle, &CurrentInfo);
    if (Result == FALSE) {
        printf("GetConsoleScreenBufferInfo failed.\n");
        return;
    }

    //
    // Don't scroll outside the screen buffer boundaries, or the window starts
    // to do a weird resizing thing.
    //

    if (Rows < 0) {
        if (CurrentInfo.srWindow.Top < -Rows) {
            Rows = -CurrentInfo.srWindow.Top;
        }

    } else {
        if (CurrentInfo.srWindow.Bottom + Rows >= CurrentInfo.dwSize.Y - 1) {
            Rows = CurrentInfo.dwSize.Y - 1 - CurrentInfo.srWindow.Bottom;
        }
    }

    Window.Top = Rows;
    Window.Bottom = Rows;
    Window.Left = 0;
    Window.Right = 0;
    Result = SetConsoleWindowInfo(Handle, FALSE, &Window);
    if (Result == FALSE) {
        printf("SetConsoleWindowInfo failed.\n");
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

    CONSOLE_SCREEN_BUFFER_INFO ConsoleInformation;
    HANDLE Handle;
    BOOL Result;

    Handle = GetStdHandle(STD_OUTPUT_HANDLE);
    Result = GetConsoleScreenBufferInfo(Handle, &ConsoleInformation);
    if (Result == FALSE) {
        return ENOSYS;
    }

    if (XSize != NULL) {
        *XSize = ConsoleInformation.srWindow.Right -
                 ConsoleInformation.srWindow.Left + 1;
    }

    if (YSize != NULL) {
        *YSize = ConsoleInformation.srWindow.Bottom -
                 ConsoleInformation.srWindow.Top + 1;
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
    WORD Attributes;
    HANDLE Handle;
    BOOL Result;
    CONSOLE_SCREEN_BUFFER_INFO ScreenInformation;

    Attributes = 0;
    if (Foreground < ConsoleColorCount) {
        Attributes |= SwForegroundColors[Foreground];
    }

    if (Background < ConsoleColorCount) {
        Attributes |= SwBackgroundColors[Background];
    }

    if (Attributes == 0) {
        va_start(ArgumentList, Format);
        vprintf(Format, ArgumentList);
        va_end(ArgumentList);
        return 0;
    }

    Handle = GetStdHandle(STD_OUTPUT_HANDLE);

    //
    // Get the original attributes.
    //

    Result = GetConsoleScreenBufferInfo(Handle, &ScreenInformation);
    if (Result == FALSE) {
        return ENOSYS;
    }

    fflush(NULL);
    Result = SetConsoleTextAttribute(Handle, Attributes);
    if (Result == FALSE) {
        return ENOSYS;
    }

    va_start(ArgumentList, Format);
    vprintf(Format, ArgumentList);
    va_end(ArgumentList);
    fflush(NULL);
    Result = SetConsoleTextAttribute(Handle,
                                     ScreenInformation.wAttributes);

    if (Result == FALSE) {
        return ENOSYS;
    }

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

    DWORD ActualCount;
    WORD Attributes;
    CONSOLE_SCREEN_BUFFER_INFO ConsoleInformation;
    HANDLE Handle;
    int RowIndex;
    COORD Start;
    int TerminalHeight;
    int TerminalWidth;

    Attributes = 0;
    if (Foreground < ConsoleColorCount) {
        Attributes |= SwForegroundColors[Foreground];
    }

    if (Background < ConsoleColorCount) {
        Attributes |= SwBackgroundColors[Background];
    }

    if ((Width == -1) || (Height == -1)) {
        SwGetTerminalDimensions(&TerminalWidth, &TerminalHeight);
        if (Width == -1) {
            Width = TerminalWidth - Column;
        }

        if (Height == -1) {
            Height = TerminalHeight - Row;
        }
    }

    Handle = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(Handle, &ConsoleInformation);
    Start.Y = ConsoleInformation.srWindow.Top + Row;
    Start.X = Column;
    for (RowIndex = 0; RowIndex < Height; RowIndex += 1) {
        FillConsoleOutputAttribute(Handle,
                                   Attributes,
                                   Width,
                                   Start,
                                   &ActualCount);

        FillConsoleOutputCharacter(Handle,
                                   ' ',
                                   Width,
                                   Start,
                                   &ActualCount);

        Start.Y += 1;
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

    Sleep(Microseconds / 1000);
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

    return ENOSYS;
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

    return ENOSYS;
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

    return ENOSYS;
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

    return ENOSYS;
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

    PSID AdministratorsGroup;
    BOOL Result;

    if (SwIsAdministratorDetermined == FALSE) {

        //
        // Return 0 if the current user is an admin, or 1 if the user is not an
        // admin or something goes wrong.
        //

        Result = AllocateAndInitializeSid(&SwNtAuthority,
                                          2,
                                          SECURITY_BUILTIN_DOMAIN_RID,
                                          DOMAIN_ALIAS_RID_ADMINS,
                                          0,
                                          0,
                                          0,
                                          0,
                                          0,
                                          0,
                                          &AdministratorsGroup);

        if (Result != FALSE) {
            if (CheckTokenMembership(NULL,
                                     AdministratorsGroup,
                                     &SwIsAdministrator) == FALSE) {

                SwIsAdministrator = FALSE;
            }

            FreeSid(AdministratorsGroup);
        }

        SwIsAdministratorDetermined = TRUE;
    }

    if (SwIsAdministrator != FALSE) {
        return 0;
    }

    return 1;
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

    return SwGetRealUserId();
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

    return SwGetRealUserId();
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

    return SwGetRealGroupId();
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

    errno = ENOSYS;
    return -1;
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

    BOOL Result;
    DWORD SessionId;

    Result = ProcessIdToSessionId(ProcessId, &SessionId);
    if (Result == FALSE) {
        return -1;
    }

    return SessionId;
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

    FILETIME CreationFileTime;
    FILETIME ExitFileTime;
    FILETIME KernelFileTime;
    BOOL Result;
    FILETIME UserFileTime;

    //
    // Get the times for the current process.
    //

    Result = GetProcessTimes(GetCurrentProcess(),
                             &CreationFileTime,
                             &ExitFileTime,
                             &KernelFileTime,
                             &UserFileTime);

    if (Result == FALSE) {
        return -1;
    }

    SwpConvertNtFileTimeToTimeval(&UserFileTime, UserTime);
    SwpConvertNtFileTimeToTimeval(&KernelFileTime, SystemTime);
    SwpGetTimeOfDay(RealTime);
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

    int Result;
    int Try;

    //
    // Windows sometimes returns directory not empty for moments after a
    // directory was recently emptied. Retry to avoid breaking scripts
    // everywhere.
    //

    for (Try = 0; Try < UNLINK_RETRY_COUNT; Try += 1) {
        Result = rmdir(Directory);
        if ((Result != -1) || (errno != ENOTEMPTY)) {
            break;
        }

        Sleep(UNLINK_RETRY_DELAY);
    }

    return Result;
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

    int Result;
    int Try;

    //
    // Windows sometimes returns directory not empty for moments after a
    // directory was recently emptied. Retry to avoid breaking scripts
    // everywhere.
    //

    for (Try = 0; Try < UNLINK_RETRY_COUNT; Try += 1) {
        Result = unlink(Path);
        if (Result != -1) {
            break;
        }

        Sleep(UNLINK_RETRY_DELAY);
    }

    return Result;
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

    0 on success, or -1 on failure.

--*/

{

    FILETIME FileTime;
    BOOL Result;
    SYSTEMTIME SystemTime;

    SwpConvertUnixTimeToNtFileTime(NewTime, &FileTime);
    Result = FileTimeToSystemTime(&FileTime, &SystemTime);
    if (Result == FALSE) {
        return -1;
    }

    Result = SetSystemTime(&SystemTime);
    if (Result == FALSE) {
        return -1;
    }

    return 0;
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

    PVOID Buffer;
    ULONG BufferSize;
    PCHAR OriginalTimeZone;
    time_t TimeInSeconds;

    //
    // Temporarily switch to the GMT time zone.
    //

    OriginalTimeZone = getenv("TZ");
    putenv("TZ=GMT");
    tzset();
    TimeInSeconds = mktime(Time);
    if (OriginalTimeZone != NULL) {
        BufferSize = strlen("TZ=") + strlen(OriginalTimeZone) + 1;
        Buffer = malloc(BufferSize);
        snprintf(Buffer, BufferSize, "TZ=%s", OriginalTimeZone);
        putenv(Buffer);
        free(Buffer);

    } else {
        putenv("TZ=");
    }

    tzset();
    return TimeInSeconds;
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

    SYSTEM_INFO SystemInformation;

    GetSystemInfo(&SystemInformation);
    return SystemInformation.dwPageSize;
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

    return ENOSYS;
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

    return _getpid();
}

int
SwGetProcessIdList (
    pid_t *ProcessIdList,
    size_t *ProcessIdListSize
    )

/*++

Routine Description:

    This routine returns a list of identifiers for the currently running
    processes.

Arguments:

    ProcessIdList - Supplies a pointer to an array of process IDs that is
        filled in by the routine. NULL can be supplied to determine the
        required size.

    ProcessIdListSize - Supplies a pointer that on input contains the size of
        the process ID list, in bytes. On output, it contains the actual size
        of the process ID list needed, in bytes.

Return Value:

    0 on success, or -1 on failure.

--*/

{

    DWORD OriginalSize;
    INT Result;
    DWORD Size;

    OriginalSize = *ProcessIdListSize;
    Size = OriginalSize;

    assert(sizeof(pid_t) == sizeof(DWORD));

    Result = EnumProcesses((PDWORD)ProcessIdList, OriginalSize, &Size);
    if (Result == 0) {
        return -1;
    }

    *ProcessIdListSize = Size;
    if (OriginalSize == Size) {
        return -1;
    }

    return 0;
}

int
SwGetProcessInformation (
    pid_t ProcessId,
    PSWISS_PROCESS_INFORMATION *ProcessInformation
    )

/*++

Routine Description:

    This routine gets process information for the specified process.

Arguments:

    ProcessId - Supplies the ID of the process whose information is to be
        gathered.

    ProcessInformation - Supplies a pointer that receives a pointer to process
        information structure. The caller is expected to free the buffer.

Return Value:

    0 on success, or -1 on failure.

--*/

{

    DWORD Access;
    ULONG AllocationSize;
    FILETIME CreationTime;
    FILETIME ExitTime;
    FILETIME KernelTime;
    PSWISS_PROCESS_INFORMATION LocalProcessInformation;
    HANDLE Process;
    char ProcessName[MAX_PATH];
    DWORD ProcessNameLength;
    BOOL Result;
    DWORD SessionId;
    INT Status;
    struct timeval Time;
    FILETIME UserTime;

    LocalProcessInformation = NULL;

    //
    // Open up a handle to the given process.
    //

    Access = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ;
    Process = OpenProcess(Access, FALSE, ProcessId);
    if (Process == NULL) {
        Status = -1;
        goto GetProcessInformationEnd;
    }

    //
    // Convert the process ID to a session ID.
    //

    Result = ProcessIdToSessionId(ProcessId, &SessionId);
    if (Result == FALSE) {
        return -1;
    }

    //
    // Get the process start time and CPU cycle information.
    //

    Result = GetProcessTimes(Process,
                             &CreationTime,
                             &ExitTime,
                             &KernelTime,
                             &UserTime);

    if (Result == FALSE) {
        Status = -1;
        goto GetProcessInformationEnd;
    }

    //
    // Get the image name. Ignore failure here.
    //

    ProcessNameLength = GetProcessImageFileName(Process, ProcessName, MAX_PATH);
    ProcessNameLength += 1;
    AllocationSize = sizeof(SWISS_PROCESS_INFORMATION) + ProcessNameLength;
    LocalProcessInformation = malloc(AllocationSize);
    if (LocalProcessInformation == NULL) {
        Status = -1;
        goto GetProcessInformationEnd;
    }

    //
    // Set the collected contents into the allocated structure.
    //

    memset(LocalProcessInformation, 0, AllocationSize);
    LocalProcessInformation->ProcessId = ProcessId;
    LocalProcessInformation->SessionId = SessionId;
    LocalProcessInformation->EffectiveUserId = 0;
    LocalProcessInformation->RealUserId = 0;
    LocalProcessInformation->State = SwissProcessStateUnknown;
    SwpConvertNtFileTimeToUnixTime(&CreationTime, &Time);
    LocalProcessInformation->StartTime = Time.tv_sec;
    SwpConvertNtFileTimeToTimeval(&KernelTime, &Time);
    LocalProcessInformation->KernelTime = Time.tv_sec;
    SwpConvertNtFileTimeToTimeval(&UserTime, &Time);
    LocalProcessInformation->UserTime = Time.tv_sec;

    assert(ProcessNameLength != 0);

    LocalProcessInformation->Name = (char *)(LocalProcessInformation + 1);
    strncpy(LocalProcessInformation->Name,
            ProcessName,
            ProcessNameLength);

    LocalProcessInformation->NameLength = ProcessNameLength;
    Status = 0;

GetProcessInformationEnd:
    if (Process != NULL) {
        CloseHandle(Process);
    }

    *ProcessInformation = LocalProcessInformation;
    return Status;
}

void
SwDestroyProcessInformation (
    PSWISS_PROCESS_INFORMATION ProcessInformation
    )

/*++

Routine Description:

    This routine destroys an allocated swiss process information structure.

Arguments:

    ProcessInformation - Supplies a pointer to the process informaiton to
        release.

Return Value:

    None.

--*/

{

    free(ProcessInformation);
    return;
}

int
SwResetSystem (
    SWISS_REBOOT_TYPE RebootType
    )

/*++

Routine Description:

    This routine resets the running system.

Arguments:

    RebootType - Supplies the type of reboot to perform.

Return Value:

    0 if the reboot was successfully requested.

    Non-zero on error.

--*/

{

    UINT Flags;
    BOOL Result;

    Flags = 0;
    switch (RebootType) {
    case RebootTypeWarm:
    case RebootTypeCold:
        Flags = EWX_REBOOT;
        break;

    case RebootTypeHalt:
        Flags = EWX_SHUTDOWN;
        break;

    default:
        return EINVAL;
    }

    Result = ExitWindowsEx(Flags, 0);
    if (Result != 0) {
        return 0;
    }

    return EINVAL;
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

    return SwResetSystem(RebootType);
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

    int Result;
    WSADATA WsaData;

    if (SwWsaStartupCalled == FALSE) {
        SwWsaStartupCalled = TRUE;
        Result = WSAStartup(MAKEWORD(2, 2), &WsaData);
        if (Result != 0) {
            return Result;
        }
    }

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

    LARGE_INTEGER Frequency;
    LARGE_INTEGER Value;

    if (QueryPerformanceCounter(&Value) == FALSE) {
        return -1;
    }

    QueryPerformanceFrequency(&Frequency);
    Time->tv_sec = Value.QuadPart / Frequency.QuadPart;
    Value.QuadPart -= (Time->tv_sec * Frequency.QuadPart);
    Time->tv_nsec = (Value.QuadPart * 1000000000ULL) / Frequency.QuadPart;
    return 0;
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

    HANDLE Console;
    BOOL Result;

    Console = GetStdHandle(STD_INPUT_HANDLE);
    Result = GetConsoleMode(Console, &SwOriginalConsoleMode);
    if (Result == FALSE) {
        return 0;
    }

    SwConsoleModeSaved = TRUE;
    return 1;
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

    HANDLE Console;
    DWORD OriginalMode;
    DWORD RawMode;
    BOOL Result;

    Console = GetStdHandle(STD_INPUT_HANDLE);
    if (SwConsoleModeSaved == FALSE) {
        if (!SwSaveTerminalMode()) {
            return 0;
        }
    }

    OriginalMode = SwOriginalConsoleMode;
    RawMode = OriginalMode;
    RawMode &= ~(ENABLE_ECHO_INPUT | ENABLE_INSERT_MODE |
                 ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);

    RawMode |= ENABLE_EXTENDED_FLAGS | ENABLE_QUICK_EDIT_MODE |
               ENABLE_INSERT_MODE;

    Result = SetConsoleMode(Console, RawMode);
    if (Result == FALSE) {
        return 0;
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

    HANDLE Console;

    if (SwConsoleModeSaved != FALSE) {
        Console = GetStdHandle(STD_INPUT_HANDLE);
        SetConsoleMode(Console, SwOriginalConsoleMode);
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

    ULONG Count;
    SYSTEM_INFO SystemInfo;

    GetSystemInfo(&SystemInfo);
    Count = SystemInfo.dwNumberOfProcessors;
    return Count;
}

int
sigaction (
    int SignalNumber,
    struct sigaction *NewAction,
    struct sigaction *OriginalAction
    )

/*++

Routine Description:

    This routine sets a new signal action for the given signal number.

Arguments:

    SignalNumber - Supplies the signal number that will be affected.

    NewAction - Supplies an optional pointer to the new signal action to
        perform upon receiving that signal. If this pointer is NULL, then no
        change will be made to the signal's action.

    OriginalAction - Supplies a pointer where the original signal action will
        be returned.

Return Value:

    0 on success.

    -1 on error, and the errno variable will contain more information.

--*/

{

    void *OriginalFunction;

    if (SignalNumber == 0) {
        return 0;
    }

    OriginalFunction = signal(SignalNumber, NewAction->sa_handler);
    if (OriginalAction != NULL) {
        OriginalAction->sa_handler = OriginalFunction;
    }

    return 0;
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

    DWORD CreationDisposition;
    DWORD DesiredAccess;
    INT FileDescriptor;
    HANDLE FileHandle;
    DWORD FlagsAndAttributes;
    DWORD ShareMode;

    //
    // Translate the access flags to the Win32 bits. If the file is to be
    // opened in append mode, do not set the write flags. For whatever reason,
    // the write flags take precedence over the append flag.
    //

    if ((OpenFlags & O_APPEND) != 0) {
        DesiredAccess = FILE_APPEND_DATA;

    } else {
        DesiredAccess = GENERIC_READ;
        if (((OpenFlags & O_WRONLY) != 0) || ((OpenFlags & O_RDWR) != 0)) {
            DesiredAccess |= GENERIC_WRITE;
        }
    }

    //
    // Convert the create flags to the Win32 version bits.
    //

    CreationDisposition = OPEN_EXISTING;
    if ((OpenFlags & O_CREAT) != 0) {
        if ((OpenFlags & O_EXCL) != 0) {
            CreationDisposition = CREATE_NEW;

        } else if ((OpenFlags & O_TRUNC) != 0) {
            CreationDisposition = CREATE_ALWAYS;

        } else {
            CreationDisposition = OPEN_ALWAYS;
        }

    } else if ((OpenFlags & O_TRUNC) != 0) {
        CreationDisposition = TRUNCATE_EXISTING;
    }

    //
    // Default to normal file attributes. If this is a create and write
    // permissions are not specified, set the attributes to read-only.
    //

    FlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
    if ((OpenFlags & O_CREAT) != 0) {
        if ((Mode & S_IWUSR) == 0) {
            FlagsAndAttributes = FILE_ATTRIBUTE_READONLY;
        }
    }

    //
    // Always open the file in read/write share mode for now.
    //

    ShareMode = FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE;
    FileHandle = CreateFile(Path,
                            DesiredAccess,
                            ShareMode,
                            NULL,
                            CreationDisposition,
                            FlagsAndAttributes,
                            NULL);

    if (FileHandle == INVALID_HANDLE_VALUE) {

        //
        // Try to open the file again so that errno gets set correctly.
        //

        FileDescriptor = open(Path, OpenFlags, Mode);
        return FileDescriptor;
    }

    //
    // Swiss assumes that the default translation mode is text mode, but
    // CreateFile defaults to binary mode. If binary mode is not explicitly
    // specified, OR in the text mode bit.
    //

    if ((OpenFlags & O_BINARY) == 0) {
        OpenFlags |= O_TEXT;
    }

    FileDescriptor = _open_osfhandle((intptr_t)FileHandle, OpenFlags);
    if (FileDescriptor == -1) {
        CloseHandle(FileHandle);
        return -1;
    }

    return FileDescriptor;
}

//
// --------------------------------------------------------- Internal Functions
//

int
SwpEscapeArguments (
    char **Arguments,
    unsigned long ArgumentCount,
    char ***NewArgumentArray
    )

/*++

Routine Description:

    This routine creates a copy of the given arguments, surrounded by double
    quotes and escaped.

Arguments:

    Arguments - Supplies the arguments to copy. Nothing will be sliced off of
        this array, just added to the beginning of it.

    ArgumentCount - Supplies the number of elements in the array.

    NewArgumentArray - Supplies a pointer where the new argument array will be
        returned on success. It is the caller's responsibility to free this
        array when finished.

Return Value:

    0 on success.

    ENOMEM on allocation failure.

--*/

{

    ULONG AllocationSize;
    ULONG ArgumentIndex;
    PSTR *NewArguments;
    BOOL NextIsQuote;
    PSTR Search;
    PSTR Source;
    PSTR String;

    assert(ArgumentCount != 0);

    *NewArgumentArray = NULL;
    AllocationSize = (ArgumentCount + 1) * sizeof(PSTR);
    for (ArgumentIndex = 0; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {

        //
        // In the worst case, the whole argument is quotes, so double it, then
        // add four for "<string>\"<null>.
        //

        AllocationSize += (strlen(Arguments[ArgumentIndex]) * 2) + 4;
    }

    //
    // Add space for the string "ImageName+CommandName".
    //

    NewArguments = malloc(AllocationSize);
    if (NewArguments == NULL) {
        return ENOMEM;
    }

    memset(NewArguments, 0, AllocationSize);
    String = (PSTR)(NewArguments + ArgumentCount + 1);
    NewArguments[0] = String;
    for (ArgumentIndex = 0; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        NewArguments[ArgumentIndex] = String;
        Source = Arguments[ArgumentIndex];
        Search = Source;

        //
        // If there are no spaces, backslashes, or double quotes, then there's
        // no need to escape.
        //

        if ((*Source != '\0') && (strpbrk(Source, " \"\t\n\v\f\\") == NULL)) {
            while (*Source != '\0') {
                *String = *Source;
                String += 1;
                Source += 1;
            }

            *String = '\0';
            String += 1;
            continue;
        }

        NextIsQuote = FALSE;
        *String = '"';
        String += 1;
        while (*Source != '\0') {

            //
            // Search out ahead and look to see if the next non-backslash is
            // a double quote. If it is, every pair of double backslashes gets
            // collapsed, so add an extra to it. For example "\\a" comes out
            // with two backslashes, "\\\\" comes out as two backslashes, and
            // "\"a" comes out as "a.
            //

            if (Search == Source) {
                while (*Search == '\\') {
                    Search += 1;
                }

                if ((*Search == '\0') || (*Search == '"')) {
                    NextIsQuote = TRUE;

                } else {
                    NextIsQuote = FALSE;
                }

                Search += 1;
            }

            //
            // If there's a backslash and the next thing is a quote, put
            // another backslash in front of it.
            //

            if ((NextIsQuote != FALSE) && (*Source == '\\')) {
                *String = '\\';
                String += 1;
            }

            //
            // If this is a quote, put a backslash in front of it to escape it.
            //

            if (*Source == '"') {
                *String = '\\';
                String += 1;
            }

            *String = *Source;
            String += 1;
            Source += 1;
        }

        *String = '"';
        String += 1;
        *String = '\0';
        String += 1;
    }

    *NewArgumentArray = NewArguments;
    return 0;
}

void
SwpGetTimeOfDay (
    struct timeval *Time
    )

/*++

Routine Description:

    This routine returns the current time.

Arguments:

    Time - Supplies a pointer where the current time since the Epoch will be
        return.

Return Value:

    None.

--*/

{

    FILETIME SystemTime;

    //
    // Get the system time as a file time and convert it to shell time.
    //

    GetSystemTimeAsFileTime(&SystemTime);
    SwpConvertNtFileTimeToUnixTime(&SystemTime, Time);
    return;
}

void
SwpConvertNtFileTimeToUnixTime (
    FILETIME *NtTime,
    struct timeval *UnixTime
    )

/*++

Routine Description:

    This routine converts NT file time (seconds since midnight January 1,
    1601 GMT) to Unix time (seconds since midnight January 1, 1970 GMT).

Arguments:

    NtTime - Supplies a pointer to the NT file time to be converted.

    UnixTime - Supplies a pointer that receives the converted Unix time.

Return Value:

    None.

--*/

{

    unsigned long long Microseconds;
    unsigned long long Seconds;

    //
    // Convert the NT file time to microseconds.
    //

    Microseconds = (((unsigned long long)NtTime->dwHighDateTime << 32) |
                    NtTime->dwLowDateTime) / 10;

    //
    // Break the microseconds out into seconds and microsecond values.
    //

    Seconds = Microseconds / 1000000;
    Microseconds -= (Seconds * 1000000);

    //
    // Convert the seconds from NT Epoch seconds to Unix Epoch seconds.
    //

    Seconds -= NT_EPOCH_TO_UNIX_EPOCH_SECONDS;

    //
    // This might truncate the seconds. Tough luck.
    //

    UnixTime->tv_sec = (time_t)Seconds;
    UnixTime->tv_usec = Microseconds;
    return;
}

void
SwpConvertUnixTimeToNtFileTime (
    const struct timeval *UnixTime,
    FILETIME *NtTime
    )

/*++

Routine Description:

    This routine converts Unix time (seconds since midnight January 1,
    1970 GMT) to NT file time (seconds since midnight January 1, 1601 GMT).

Arguments:

    UnixTime - Supplies a pointer to the Unix time to be converted.

    NtTime - Supplies a pointer that receives the converted NT file time.

Return Value:

    None.

--*/

{

    unsigned long long Microseconds;
    unsigned long long NtTimeUnits;
    unsigned long long Seconds;

    //
    // Add the appropriate seconds to the Unix time and calculate the total
    // time since the NT Epoch in microseconds.
    //

    Seconds = UnixTime->tv_sec + NT_EPOCH_TO_UNIX_EPOCH_SECONDS;
    Microseconds = (Seconds * 1000000) + UnixTime->tv_usec;

    //
    // Convert from microseconds to 100-ns units.
    //

    NtTimeUnits = Microseconds * 10;
    NtTime->dwHighDateTime = NtTimeUnits >> 32;
    NtTime->dwLowDateTime = (unsigned long)NtTimeUnits;
    return;
}

void
SwpConvertNtFileTimeToTimeval (
    FILETIME *NtTime,
    struct timeval *Time
    )

/*++

Routine Description:

    This routine converts NT file time in raw 100-nanoseconds units into a
    POSIX timeval structure.

Arguments:

    NtTime - Supplies a pointer to the NT file time to be converted.

    Time - Supplies a pointer that receives the converted Unix time.

Return Value:

    None.

--*/

{

    unsigned long long Microseconds;
    unsigned long long Seconds;

    //
    // Convert the NT file time to microseconds.
    //

    Microseconds = (((unsigned long long)NtTime->dwHighDateTime << 32) |
                    NtTime->dwLowDateTime) / 10;

    //
    // Break the microseconds out into seconds and microsecond values.
    //

    Seconds = Microseconds / 1000000;
    Microseconds -= (Seconds * 1000000);

    //
    // This might truncate the seconds. Tough luck.
    //

    Time->tv_sec = (time_t)Seconds;
    Time->tv_usec = Microseconds;
    return;
}

int
SwpReadSheBang (
    char *Command,
    char **NewCommand,
    char **NewArgument
    )

/*++

Routine Description:

    This routine peeks into the file and determines if it starts with a #!.

Arguments:

    Command - Supplies a pointer to the file to look into.

    NewCommand - Supplies a pointer where the new command will be returned on
        success. It is the caller's responsibility to free this memory.

    NewArgument - Supplies a pointer where an optional command argument
        will be returned. It is the caller's responsibility to free this memory.

Return Value:

    0 on success.

    Non-zero if the file could not be opened or read.

--*/

{

    char *Argument;
    size_t BytesRead;
    char Character;
    char *Executable;
    FILE *File;
    char Line[2048];
    char *LineEnd;
    int Status;

    Argument = NULL;
    Executable = NULL;
    *NewCommand = NULL;
    *NewArgument = NULL;
    File = fopen(Command, "rb");
    if (File == NULL) {
        return errno;
    }

    BytesRead = fread(Line, 1, sizeof(Line) - 1, File);
    if (BytesRead <= 2) {
        Status = errno;
        goto ReadSheBangEnd;
    }

    Status = 0;
    Line[sizeof(Line) - 1] = '\0';
    if ((Line[0] != '#') || (Line[1] != '!')) {
        goto ReadSheBangEnd;
    }

    Executable = &(Line[2]);
    while (isblank(*Executable)) {
        Executable += 1;
    }

    if (*Executable == '\0') {
        Executable = NULL;
        goto ReadSheBangEnd;
    }

    //
    // Get past the executable.
    //

    Argument = Executable;
    while ((!isspace(*Argument)) && (*Argument != '\0')) {
        Argument += 1;
    }

    Character = *Argument;
    *Argument = '\0';

    //
    // If it's /bin/sh, just make it sh on windows.
    //

    if (strcmp(Executable, "/bin/sh") == 0) {
        Executable += 5;
    }

    if ((Character == '\0') || (Character == '\r') || (Character == '\n')) {
        Argument = NULL;
        goto ReadSheBangEnd;
    }

    //
    // Look for the end of the line.
    //

    Argument += 1;
    LineEnd = Argument;
    while ((*LineEnd != '\0') && (*LineEnd != '\r') && (*LineEnd != '\n')) {
        LineEnd += 1;
    }

    if (LineEnd == Argument) {
        Argument = NULL;
        goto ReadSheBangEnd;
    }

    *LineEnd = '\0';

ReadSheBangEnd:
    fclose(File);
    if (Status == 0) {
        if (Executable != NULL) {
            *NewCommand = strdup(Executable);
            if (*NewCommand == NULL) {
                Status = ENOMEM;
            }
        }

        if ((Status == 0) && (Argument != NULL)) {
            *NewArgument = strdup(Argument);
            if (*NewArgument == NULL) {
                free(*NewCommand);
                *NewCommand = NULL;
                Status = ENOMEM;
            }
        }
    }

    return Status;
}

