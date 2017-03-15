/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    swlibos.h

Abstract:

    This header contains definitions for operating system dependent
    functionality for the Swiss common library.

Author:

    Evan Green 2-Jul-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <pthread.h>
#include <time.h>
#include <sys/time.h>

//
// --------------------------------------------------------------------- Macros
//

#if defined(WIN32)

#define ctermid(_Buffer) NULL
#define strsignal(_Signal) "Unknown"
#define strftime ClStrftimeC90

#endif

#if defined(__CYGWIN__)

#define setdomainname(_Domainname, _Length) -1, errno = ENOSYS

#endif

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum length of each of the strings in the system name
// structure.
//

#define SYSTEM_NAME_STRING_SIZE 80

//
// Define the PATH list separator character.
//

#if defined(WIN32)

#define PATH_LIST_SEPARATOR ';'

#else

#define PATH_LIST_SEPARATOR ':'

#endif

//
// Define some constants that may not be defined in some environments.
//

#ifndef S_IRGRP

#define S_IRGRP 0x0

#endif

#ifndef S_IWGRP

#define S_IWGRP 0x0

#endif

#ifndef S_IXGRP

#define S_IXGRP 0x0

#endif

#ifndef S_IROTH

#define S_IROTH 0x0

#endif

#ifndef S_IWOTH

#define S_IWOTH 0x0

#endif

#ifndef S_IXOTH

#define S_IXOTH 0x0

#endif

#ifndef S_ISUID

#define S_ISUID 0x0

#endif

#ifndef S_ISGID

#define S_ISGID 0x0

#endif

#ifndef S_ISVTX

#define S_ISVTX 0x0

#endif

#ifndef S_IRWXG

#define S_IRWXG (S_IRGRP | S_IWGRP | S_IXGRP)

#endif

#ifndef S_IRWXO

#define S_IRWXO (S_IROTH | S_IWOTH | S_IXOTH)

#endif

#ifndef ALLPERMS

#define ALLPERMS (S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID | S_ISVTX)

#endif

#ifndef S_ISLNK

#define S_ISLNK(_Mode) 0x0

#endif

#ifndef S_ISSOCK

#define S_ISSOCK(_Mode) 0x0

#endif

#ifndef ELOOP

#define ELOOP ERANGE

#endif

#ifndef SIGHUP

#define SIGHUP 1

#endif

#ifndef SIGINT

#define SIGINT 2

#endif

#ifndef SIGQUIT

#define SIGQUIT 3

#endif

#ifndef SIGILL

#define SIGILL 4

#endif

#ifndef SIGTRAP

#define SIGTRAP 5

#endif

#ifndef SIGABRT

#define SIGABRT 6

#endif

#ifndef SIGBUS

#define SIGBUS 7

#endif

#ifndef SIGFPE

#define SIGFPE 8

#endif

#ifndef SIGKILL

#define SIGKILL 9

#endif

#ifndef SIGUSR1

#define SIGUSR1 10

#endif

#ifndef SIGSEGV

#define SIGSEGV 11

#endif

#ifndef SIGUSR2

#define SIGUSR2 12

#endif

#ifndef SIGPIPE

#define SIGPIPE 13

#endif

#ifndef SIGALRM

#define SIGALRM 14

#endif

#ifndef SIGTERM

#define SIGTERM 15

#endif

#ifndef SIGCHLD

#define SIGCHLD 16

#endif

#ifndef SIGCONT

#define SIGCONT 17

#endif

#ifndef SIGSTOP

#define SIGSTOP 18

#endif

#ifndef SIGTSTP

#define SIGTSTP 19

#endif

#ifndef SIGTTIN

#define SIGTTIN 20

#endif

#ifndef SIGTTOU

#define SIGTTOU 21

#endif

#ifndef SIGURG

#define SIGURG 22

#endif

#ifndef SIGXCPU

#define SIGXCPU 23

#endif

#ifndef SIGXFSZ

#define SIGXFSZ 24

#endif

#ifndef SIGVTALRM

#define SIGVTALRM 25

#endif

#ifndef SIGPROF

#define SIGPROF 26

#endif

#ifndef SIGWINCH

#define SIGWINCH 27

#endif

#ifndef SIGPOLL

#define SIGPOLL 28

#endif

#ifndef SIGSYS

#define SIGSYS 29

#endif

#ifndef WIFSIGNALED

#define WIFSIGNALED(_Status) 0

#endif

#ifndef WEXITSTATUS

#define WEXITSTATUS(_Status) (_Status)

#endif

#ifndef L_ctermid

#define L_ctermid 32

#endif

#ifndef O_NOCTTY

#define O_NOCTTY 0x0000

#endif

#ifndef O_DIRECTORY

#define O_DIRECTORY 0x0000

#endif

#ifndef O_SYNC

#define O_SYNC 0x0000

#endif

#ifndef O_NONBLOCK

#define O_NONBLOCK 0x0000

#endif

#ifndef O_NOATIME

#define O_NOATIME 0x0000

#endif

#ifndef O_NOFOLLOW

#define O_NOFOLLOW 0x0000

#endif

#ifndef O_DSYNC

#define O_DSYNC 0x0000

#endif

#ifndef O_BINARY

#define O_BINARY 0x0000

#endif

#ifndef O_TEXT

#define O_TEXT 0x0000

#endif

#ifndef SIGRTMIN

#define SIGRTMIN NSIG

#endif

#ifndef SIGRTMAX

#define SIGRTMAX NSIG

#endif

//
// ------------------------------------------------------ Data Type Definitions
//

#if defined(WIN32)

typedef unsigned long id_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;

struct sigaction {
    void *sa_handler;
};

#endif

typedef enum _SWISS_FILE_TEST {
    FileTestInvalid,
    FileTestIsBlockDevice,
    FileTestIsCharacterDevice,
    FileTestIsDirectory,
    FileTestExists,
    FileTestIsRegularFile,
    FileTestHasSetGroupId,
    FileTestIsSymbolicLink,
    FileTestIsFifo,
    FileTestCanRead,
    FileTestIsSocket,
    FileTestIsNonEmpty,
    FileTestDescriptorIsTerminal,
    FileTestHasSetUserId,
    FileTestCanWrite,
    FileTestCanExecute,
} SWISS_FILE_TEST, *PSWISS_FILE_TEST;

typedef enum _CONSOLE_COLOR {
    ConsoleColorDefault,
    ConsoleColorBlack,
    ConsoleColorDarkRed,
    ConsoleColorDarkGreen,
    ConsoleColorDarkYellow,
    ConsoleColorDarkBlue,
    ConsoleColorDarkMagenta,
    ConsoleColorDarkCyan,
    ConsoleColorGray,
    ConsoleColorBoldDefault,
    ConsoleColorDarkGray,
    ConsoleColorRed,
    ConsoleColorGreen,
    ConsoleColorYellow,
    ConsoleColorBlue,
    ConsoleColorMagenta,
    ConsoleColorCyan,
    ConsoleColorWhite,
    ConsoleColorCount
} CONSOLE_COLOR, *PCONSOLE_COLOR;

typedef enum _SWISS_REBOOT_TYPE {
    RebootTypeInvalid,
    RebootTypeCold,
    RebootTypeWarm,
    RebootTypeHalt,
} SWISS_REBOOT_TYPE, *PSWISS_REBOOT_TYPE;

/*++

Structure Description:

    This structure defines the buffer used to name the machine.

Members:

    SystemName - Stores a string describing the name of this implementation of
        the operating system.

    NodeName - Stores a string describing the name of this node within the
        communications network to which this node is attached, if any.

    Release - Stores a string containing the release level of this
        implementation.

    Version - Stores a string containing the version level of this release.

    Machine - Stores the name of the hardware type on which the system is
        running.

    DomainName - Stores the name of the network domain this machine resides in,
        if any.

--*/

typedef struct _SYSTEM_NAME {
    char SystemName[SYSTEM_NAME_STRING_SIZE];
    char NodeName[SYSTEM_NAME_STRING_SIZE];
    char Release[SYSTEM_NAME_STRING_SIZE];
    char Version[SYSTEM_NAME_STRING_SIZE];
    char Machine[SYSTEM_NAME_STRING_SIZE];
    char DomainName[SYSTEM_NAME_STRING_SIZE];
} SYSTEM_NAME, *PSYSTEM_NAME;

typedef enum _SWISS_PROCESS_STATE {
    SwissProcessStateRunning,
    SwissProcessStateUninterruptibleSleep,
    SwissProcessStateInterruptibleSleep,
    SwissProcessStateStoppped,
    SwissProcessStateDead,
    SwissProcessStateZombie,
    SwissProcessStateUnknown,
    SwissProcessStateMax
} SWISS_PROCESS_STATE, *PSWISS_PROCESS_STATE;

/*++

Structure Description:

    This structure defines information about a process.

Members:

    ProcessId - Stores the process identifier.

    ParentProcessId - Stores the parent process's identifier.

    ProcessGroupId - Stores the process's group identifier.

    SessionId - Stores the process's session identifier.

    RealUserId - Stores the real user ID of the process owner.

    EffectiveUserId - Stores the effective user ID of the process owner.

    RealGroupId - Stores the real group ID of the process owner.

    EffectiveGroupId - Stores the effective group ID of the process owner.

    TerminalId - Stores the ID of the terminal to which the process belongs.

    Priority - Stores the priority of the process.

    NiceValue - Stores the nice value of the process.

    Flags - Stores the process flags.

    State - Stores the process state.

    ImageSize - Stores the size, in bytes, of the process's main image.

    StartTime - Stores the process start time as a system time.

    KernelTime - Stores the time the process has spent in kernel mode, in
        seconds.

    UserTime - Stores the time the process has spent in user mode, in seconds.

    Name - Stores a pointer to the process name, which is a null-terminated
        string.

    NameLength - Stores the length of the process name, in characters,
        including the null terminator.

    Arguments - Stores a pointer to the buffer that contains the image name
        and process arguments as a series of strings.

    ArgumentsSize - Stores the size of the arguments buffer in bytes.

--*/

typedef struct _SWISS_PROCESS_INFORMATION {
    pid_t ProcessId;
    pid_t ParentProcessId;
    pid_t ProcessGroupId;
    pid_t SessionId;
    uid_t RealUserId;
    uid_t EffectiveUserId;
    gid_t RealGroupId;
    gid_t EffectiveGroupId;
    int TerminalId;
    int Priority;
    int NiceValue;
    unsigned long Flags;
    SWISS_PROCESS_STATE State;
    size_t ImageSize;
    time_t StartTime;
    time_t KernelTime;
    time_t UserTime;
    char *Name;
    unsigned long NameLength;
    void *Arguments;
    unsigned long ArgumentsSize;
} SWISS_PROCESS_INFORMATION, *PSWISS_PROCESS_INFORMATION;

/*++

Structure Description:

    This structure defines information about a user. It is the analogue to
    struct passwd.

Members:

    Name - Stores a pointer to a string containing the user's login name.

    Password - Stores a pointer to a string containing the user's encrypted
        password.

    UserId - Stores the user's unique identifier.

    GroupId - Stores the user's group identifier.

    Gecos - Stores a pointer to a string containing the user's real name,
        and possibly other information such as a phone number.

    Directory - Stores a pointer to a string containing the user's home
        directory path.

    Shell - Stores a pointer to a string containing the path to a program
        the user should use as a shell.

--*/

typedef struct _SWISS_USER_INFORMATION {
    char *Name;
    char *Password;
    id_t UserId;
    id_t GroupId;
    char *Gecos;
    char *Directory;
    char *Shell;
} SWISS_USER_INFORMATION, *PSWISS_USER_INFORMATION;

/*++

Structure Description:

    This structure defines a mapping between a signal number and its name.

Members:

    SignalNumber - Stores the signal number.

    SignalName - Stores the signal name.

--*/

typedef struct _SWISS_SIGNAL_NAME {
    int SignalNumber;
    char *SignalName;
} SWISS_SIGNAL_NAME, *PSWISS_SIGNAL_NAME;

//
// -------------------------------------------------------------------- Globals
//

//
// Define the mapping of signal names to numbers.
//

extern SWISS_SIGNAL_NAME SwSignalMap[];

//
// Store a global that's non-zero if this OS supports forking.
//

extern int SwForkSupported;

//
// Store a global that's non-zero if this OS supports symbolic links.
//

extern int SwSymlinkSupported;

//
// -------------------------------------------------------- Function Prototypes
//

#if defined(WIN32)

int
sigaction (
    int SignalNumber,
    struct sigaction *NewAction,
    struct sigaction *OriginalAction
    );

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

size_t
ClStrftimeC90 (
    char *Buffer,
    size_t BufferSize,
    const char *Format,
    const struct tm *Time
    );

/*++

Routine Description:

    This routine implements a C90 strftime, using the underlying system's C89
    strftime.

Arguments:

    Buffer - Supplies a pointer where the converted string will be returned.

    BufferSize - Supplies the size of the string buffer in bytes.

    Format - Supplies the format string to govern the conversion.

    Time - Supplies a pointer to the calendar time value to use in the
        substitution.

Return Value:

    Returns the number of characters written to the output buffer, including
    the null terminator.

--*/

#endif

int
SwReadLink (
    char *LinkPath,
    char **Destination
    );

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

int
SwCreateHardLink (
    char *ExistingFilePath,
    char *LinkPath
    );

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

int
SwCreateSymbolicLink (
    char *LinkTarget,
    char *Link
    );

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

int
SwGetUserNameFromId (
    uid_t UserId,
    char **UserName
    );

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

int
SwGetUserIdFromName (
    char *UserName,
    uid_t *UserId
    );

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

int
SwGetGroupNameFromId (
    gid_t GroupId,
    char **GroupName
    );

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

int
SwGetGroupIdFromName (
    char *GroupName,
    gid_t *GroupId
    );

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

int
SwGetUserInformationByName (
    char *UserName,
    PSWISS_USER_INFORMATION *UserInformation
    );

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

int
SwGetUserInformationById (
    uid_t UserId,
    PSWISS_USER_INFORMATION *UserInformation
    );

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

int
SwGetGroupList (
    uid_t UserId,
    gid_t GroupId,
    gid_t **Groups,
    size_t *GroupCount
    );

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

unsigned long long
SwGetBlockCount (
    struct stat *Stat
    );

/*++

Routine Description:

    This routine returns the number of blocks used by the file. The size of a
    block is implementation specific.

Arguments:

    Stat - Supplies a pointer to the stat structure.

Return Value:

    Returns the number of blocks used by the given file.

--*/

unsigned long
SwGetBlockSize (
    struct stat *Stat
    );

/*++

Routine Description:

    This routine returns the number of size of a block for this file.

Arguments:

    Stat - Supplies a pointer to the stat structure.

Return Value:

    Returns the block size for this file.

--*/

int
SwMakeDirectory (
    const char *Path,
    unsigned long long CreatePermissions
    );

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

int
SwEvaluateFileTest (
    SWISS_FILE_TEST Operator,
    const char *Path,
    int *Error
    );

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

int
SwIsCurrentUserMemberOfGroup (
    unsigned long long Group,
    int *Error
    );

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

INT
SwMakeFifo (
    PSTR Path,
    mode_t Permissions
    );

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

INT
SwChangeFileOwner (
    PSTR FilePath,
    int FollowLinks,
    uid_t UserId,
    gid_t GroupId
    );

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

int
SwDoesPathHaveSeparators (
    char *Path
    );

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

int
SwGetSystemName (
    PSYSTEM_NAME Name
    );

/*++

Routine Description:

    This routine returns the name and version of the system.

Arguments:

    Name - Supplies a pointer where the name information will be returned.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

int
SwRunCommand (
    char *Command,
    char **Arguments,
    int ArgumentCount,
    int Asynchronous,
    int *ReturnValue
    );

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

int
SwExec (
    char *Command,
    char **Arguments,
    int ArgumentCount
    );

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

int
SwBreakDownTime (
    int LocalTime,
    time_t *Time,
    struct tm *TimeFields
    );

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

pid_t
SwFork (
    VOID
    );

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

char *
SwGetExecutableName (
    VOID
    );

/*++

Routine Description:

    This routine returns the path to the current executable.

Arguments:

    None.

Return Value:

    Returns a path to the executable on success.

    NULL if not supported by the OS.

--*/

pid_t
SwWaitPid (
    pid_t Pid,
    int NonBlocking,
    int *Status
    );

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

int
SwKill (
    pid_t ProcessId,
    int SignalNumber
    );

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

int
SwOsStat (
    const char *Path,
    int FollowLinks,
    struct stat *Stat
    );

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

int
SwSetBinaryMode (
    int FileDescriptor,
    int EnableBinaryMode
    );

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

int
SwReadInputCharacter (
    void
    );

/*++

Routine Description:

    This routine routine reads a single terminal character from standard input.

Arguments:

    None.

Return Value:

    Returns the character received from standard in on success.

    EOF on failure or if no more characters could be read.

--*/

void
SwMoveCursorRelative (
    void *Stream,
    int XPosition,
    char *String
    );

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

void
SwMoveCursor (
    void *Stream,
    int XPosition,
    int YPosition
    );

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

void
SwEnableCursor (
    void *Stream,
    int Enable
    );

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

void
SwScrollTerminal (
    int Rows
    );

/*++

Routine Description:

    This routine scrolls the terminal screen.

Arguments:

    Rows - Supplies the number of rows to scroll the screen down. This can be
        negative to scroll the screen up.

Return Value:

    None.

--*/

int
SwGetTerminalDimensions (
    int *XSize,
    int *YSize
    );

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

int
SwClearRegion (
    CONSOLE_COLOR Background,
    CONSOLE_COLOR Foreground,
    int Column,
    int Row,
    int Width,
    int Height
    );

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

int
SwPrintInColor (
    CONSOLE_COLOR Background,
    CONSOLE_COLOR Foreground,
    const char *Format,
    ...
    );

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

void
SwSleep (
    unsigned long long Microseconds
    );

/*++

Routine Description:

    This routine suspends the current thread for at least the given number of
    microseconds.

Arguments:

    Microseconds - Supplies the amount of time to sleep for in microseconds.

Return Value:

    None.

--*/

int
SwSetRealUserId (
    id_t UserId
    );

/*++

Routine Description:

    This routine sets the current real user ID.

Arguments:

    UserId - Supplies the user ID to set.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

int
SwSetEffectiveUserId (
    id_t UserId
    );

/*++

Routine Description:

    This routine sets the current effective user ID.

Arguments:

    UserId - Supplies the user ID to set.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

int
SwSetRealGroupId (
    id_t GroupId
    );

/*++

Routine Description:

    This routine sets the current real group ID.

Arguments:

    GroupId - Supplies the group ID to set.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

int
SwSetEffectiveGroupId (
    id_t GroupId
    );

/*++

Routine Description:

    This routine sets the current effective group ID.

Arguments:

    GroupId - Supplies the group ID to set.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

id_t
SwGetRealUserId (
    void
    );

/*++

Routine Description:

    This routine returns the current real user ID.

Arguments:

    None.

Return Value:

    Returns the real user ID.

    -1 on failure.

--*/

id_t
SwGetEffectiveUserId (
    void
    );

/*++

Routine Description:

    This routine returns the current effective user ID.

Arguments:

    None.

Return Value:

    Returns the effective user ID.

    -1 on failure.

--*/

id_t
SwGetRealGroupId (
    void
    );

/*++

Routine Description:

    This routine returns the current real group ID.

Arguments:

    None.

Return Value:

    Returns the real group ID.

    -1 on failure.

--*/

id_t
SwGetEffectiveGroupId (
    void
    );

/*++

Routine Description:

    This routine returns the current effective group ID.

Arguments:

    None.

Return Value:

    Returns the effective group ID.

    -1 on failure.

--*/

int
SwSetGroups (
    int Size,
    const gid_t *List
    );

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

int
SwGetTerminalId (
    void
    );

/*++

Routine Description:

    This routine returns the current terminal ID.

Arguments:

    None.

Return Value:

    Returns the terminal ID.

    -1 on failure.

--*/

int
SwGetTerminalNameFromId (
    unsigned long long TerminalId,
    char **TerminalName
    );

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

pid_t
SwGetSessionId (
    pid_t ProcessId
    );

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

int
SwGetSessionNameFromId (
    unsigned long long SessionId,
    char **SessionName
    );

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

int
SwGetTimes (
    struct timeval *RealTime,
    struct timeval *UserTime,
    struct timeval *SystemTime
    );

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

int
SwRemoveDirectory (
    const char *Directory
    );

/*++

Routine Description:

    This routine attempts to remove the specified directory.

Arguments:

    Directory - Supplies a pointer to the string containing the directory to
        remove. This directory must be empty.

Return Value:

    0 on success, or -1 on failure.

--*/

int
SwUnlink (
    const char *Path
    );

/*++

Routine Description:

    This routine attempts to remove the specified file or directory.

Arguments:

    Path - Supplies a pointer to the string containing the path to delete.

Return Value:

    0 on success, or -1 on failure.

--*/

int
SwSetTimeOfDay (
    const struct timeval *NewTime,
    void *UnusedParameter
    );

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

time_t
SwConvertGmtTime (
    struct tm *Time
    );

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

size_t
SwGetPageSize (
    void
    );

/*++

Routine Description:

    This routine gets the current page size on the system.

Arguments:

    None.

Return Value:

    Returns the size of a page on success.

    -1 on failure.

--*/

int
SwChroot (
    const char *Path
    );

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

pid_t
SwGetProcessId (
    void
    );

/*++

Routine Description:

    This routine returns the current process ID.

Arguments:

    None.

Return Value:

    Returns the current process ID.

--*/

int
SwGetProcessIdList (
    pid_t *ProcessIdList,
    size_t *ProcessIdListSize
    );

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

int
SwGetProcessInformation (
    pid_t ProcessId,
    PSWISS_PROCESS_INFORMATION *ProcessInformation
    );

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

void
SwDestroyProcessInformation (
    PSWISS_PROCESS_INFORMATION ProcessInformation
    );

/*++

Routine Description:

    This routine destroys an allocated swiss process information structure.

Arguments:

    ProcessInformation - Supplies a pointer to the process informaiton to
        release.

Return Value:

    None.

--*/

int
SwCloseFrom (
    int Descriptor
    );

/*++

Routine Description:

    This routine closes all open file descriptors greater than or equal to
    the given descriptor.

Arguments:

    Descriptor - Supplies the minimum descriptor to close.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

int
SwResetSystem (
    SWISS_REBOOT_TYPE RebootType
    );

/*++

Routine Description:

    This routine resets the running system.

Arguments:

    RebootType - Supplies the type of reboot to perform.

Return Value:

    0 if the reboot was successfully requested.

    Non-zero on error.

--*/

int
SwRequestReset (
    SWISS_REBOOT_TYPE RebootType
    );

/*++

Routine Description:

    This routine initiates a reboot of the running system.

Arguments:

    RebootType - Supplies the type of reboot to perform.

Return Value:

    0 if the reboot was successfully requested.

    Non-zero on error.

--*/

int
SwGetHostName (
    char *Name,
    size_t NameLength
    );

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

int
SwGetMonotonicClock (
    struct timespec *Time
    );

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

int
SwSaveTerminalMode (
    void
    );

/*++

Routine Description:

    This routine saves the current terminal mode as the mode to restore.

Arguments:

    None.

Return Value:

    1 on success.

    0 on failure.

--*/

int
SwSetRawInputMode (
    char *BackspaceCharacter,
    char *KillCharacter
    );

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

void
SwRestoreInputMode (
    void
    );

/*++

Routine Description:

    This routine restores the terminal's input mode if it was put into raw mode
    earlier. If it was not, this is a no-op.

Arguments:

    None.

Return Value:

    None.

--*/

int
SwGetProcessorCount (
    int Online
    );

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

int
SwOpen (
    const char *Path,
    int OpenFlags,
    mode_t Mode
    );

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

