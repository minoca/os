/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ssdaemon.c

Abstract:

    This module implements the start-stop-daemon command, which is used,
    predictably, for starting and stopping daemons.

Author:

    Evan Green 23-Mar-2015

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "swlib.h"
#include "login/lutil.h"

//
// ---------------------------------------------------------------- Definitions
//

#define SS_DAEMON_VERSION_MAJOR 1
#define SS_DAEMON_VERSION_MINOR 0

#define SS_DAEMON_USAGE                                                        \
    "usage: start-stop-daemon [options] command\n"                             \
    "The start-stop-daemon utility is used control system-level processes.\n"  \
    "This utility can find existing instances of a running process, spawn, \n" \
    "or terminate processes. Options are\n"                                    \
    "  -S, --start -- Start the given command if it is not already running.\n" \
    "  -K, --stop -- Signal the specified process and exit.\n"                 \
    "Matching options:\n"                                                      \
    "  -p, --pidfile=file -- Check whether a process has created a pidfile.\n" \
    "  -x, --exec=executable -- Check for processes that are an instance of \n"\
    "      the given executable. This should be an absolute path.\n"           \
    "  -n, --name=name -- Check for processes that match the given name.\n"    \
    "  -u, --user=user -- Check for processes owned by the given user.\n"      \
    "Generic options:\n"                                                       \
    "  -g, --group=group -- Change to the given group when starting.\n"        \
    "  -s, --signal=signal -- Specify the signal to use to stop a process.\n"  \
    "      The default is TERM.\n"                                             \
    "  -a, --startas=path -- Start the process specified. The default is the\n"\
    "      argument given by --exec.\n"                                        \
    "  -t, --test -- Print actions that would occur but do nothing.\n"         \
    "  -o, --oknodo -- Exit with status 0 instead of 1 if no actions are \n"   \
    "      or would be taken.\n"                                               \
    "  -q, --quiet -- Display only error messages.\n"                          \
    "  -c, --chuid=user[:group] -- Change to the given user/group before \n"   \
    "      starting the process.\n"                                            \
    "  -r, --chroot=root -- Change to the given root before operating.\n"      \
    "  -d, --chdir=dir -- Change to the given working directory before \n"     \
    "      operating.\n"                                                       \
    "  -b, --background -- Force the application into the background by \n"    \
    "      forking twice.\n" \
    "  -C, --no-close -- Don't close file descriptors when forcing a \n"       \
    "      daemon into the background.\n"                                      \
    "  -N, --nicelevel=nice -- Alter the priority of the starting process.\n"  \
    "  -k, --umask=mask -- Sets the umask before starting the process.\n"      \
    "  -m, --make-pidfile -- Create the pidfile specified by --pidfile right\n"\
    "      before execing the process. This file will not be removed when \n"  \
    "      the process exits.\n"                                               \
    "  -v, --verbose -- Print more messages.\n"                                \
    "  --help -- Displays this help text and exits.\n"                         \
    "  --version -- Displays the application version and exits.\n"

#define SS_DAEMON_OPTIONS_STRING "SKp:x:n:u:g:s:a:toqc:r:d:bCN:k:mvHV"

//
// Define application options.
//

//
// Set this option to run a start action.
//

#define SS_DAEMON_OPTION_START 0x00000001

//
// Set this option to run a stop action.
//

#define SS_DAEMON_OPTION_STOP 0x00000002

//
// Set this option to test, but not actually do anything.
//

#define SS_DAEMON_OPTION_TEST 0x00000004

//
// Set this option to return successfully if no actions were performed.
//

#define SS_DAEMON_OPTION_NOTHING_OK 0x00000008

//
// Set this option to be quiet.
//

#define SS_DAEMON_OPTION_QUIET 0x00000010

//
// Set this option to be loud.
//

#define SS_DAEMON_OPTION_VERBOSE 0x00000020

//
// Set this option for force background the starting app.
//

#define SS_DAEMON_OPTION_BACKGROUND 0x00000040

//
// Set this option to not close file descriptors when backgrounding.
//

#define SS_DAEMON_OPTION_NO_CLOSE 0x00000080

//
// Set this option to make a pidfile.
//

#define SS_DAEMON_OPTION_MAKE_PIDFILE 0x00000100

//
// Set this option if the caller wanted to change the nice level.
//

#define SS_DAEMON_OPTION_NICE 0x00000200

//
// Define the number of initial elements in the match ID array.
//

#define SS_DAEMON_INITIAL_MATCH_ARRAY_SIZE 32

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the application context for the start-stop-daemon
    utility.

Members:

    Options - Stores a bitfield of application options. See SS_DAEMON_OPTION_*
        definitions.

    ChangeGroup - Stores the group ID to change to, or -1 if unspecified.

    ChangeUser - Stores the user ID to change to, or -1 if unspecified.

    ExecPath - Stores the path to execute to start the daemon.

    MatchUserId - Stores the user ID to match against when stopping, or -1 if
        unspecified.

    NiceLevel - Stores the nice level to use when starting, or -1 if
        unspecified.

    PidFilePath - Stores an optional pointer to the path to the pid file.

    MatchProcessIds - Stores an array of process IDs that match the criteria.

    MatchProcessIdCount - Stores the number of valid elements in the match
        process IDs array.

    MatchProcessIdCapacity - Stores the buffer size in elements of the match
        process IDs array.

    ProcessId - Stores the process ID loaded from the pid file.

    ProcessName - Stores the process name to match against.

    RootDirectory - Stores the root directory to switch to before operating.

    Signal - Stores the signal to send to stop a daemon.

    StartAs - Stores the first argument to the exec functions, which may be
        different than the exec path.

    Umask - Stores the umask to set, or -1 if unspecified.

    WorkingDirectory - Stores the optional working directory to change to
        before starting.

--*/

typedef struct _SS_DAEMON_CONTEXT {
    INT Options;
    gid_t ChangeGroup;
    uid_t ChangeUser;
    PSTR ExecPath;
    uid_t MatchUserId;
    long NiceLevel;
    PSTR PidFilePath;
    pid_t *MatchProcessIds;
    UINTN MatchProcessIdCount;
    UINTN MatchProcessIdCapacity;
    PSTR ProcessName;
    PSTR RootDirectory;
    INT Signal;
    PSTR StartAs;
    long Umask;
    PSTR WorkingDirectory;
} SS_DAEMON_CONTEXT, *PSS_DAEMON_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
SsDaemonReadPidFile (
    PSS_DAEMON_CONTEXT Context
    );

VOID
SsDaemonWritePidFile (
    PSS_DAEMON_CONTEXT Context
    );

INT
SsDaemonMatchAllProcesses (
    PSS_DAEMON_CONTEXT Context
    );

VOID
SsDaemonMatchPid (
    PSS_DAEMON_CONTEXT Context,
    pid_t ProcessId
    );

INT
SsDaemonStop (
    PSS_DAEMON_CONTEXT Context
    );

VOID
SsDaemonPrintStopDescription (
    PSS_DAEMON_CONTEXT Context
    );

//
// -------------------------------------------------------------------- Globals
//

struct option SsDaemonLongOptions[] = {
    {"start", no_argument, 0, 'S'},
    {"stop", no_argument, 0, 'K'},
    {"pidfile", required_argument, 0, 'p'},
    {"exec", required_argument, 0, 'x'},
    {"name", required_argument, 0, 'n'},
    {"user", required_argument, 0, 'u'},
    {"group", required_argument, 0, 'g'},
    {"signal", required_argument, 0, 's'},
    {"startas", required_argument, 0, 'a'},
    {"test", no_argument, 0, 't'},
    {"oknodo", no_argument, 0, 'o'},
    {"quiet", no_argument, 0, 'q'},
    {"chuid", required_argument, 0, 'c'},
    {"chroot", required_argument, 0, 'r'},
    {"chdir", required_argument, 0, 'd'},
    {"background", no_argument, 0, 'b'},
    {"no-close", no_argument, 0, 'C'},
    {"nicelevel", required_argument, 0, 'N'},
    {"umask", required_argument, 0, 'k'},
    {"make-pidfile", no_argument, 0, 'm'},
    {"verbose", no_argument, 0, 'v'},
    {"help", no_argument, 0, 'H'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
SsDaemonMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the start-stop-daemon utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    INT ActionCount;
    PSTR AfterScan;
    ULONG ArgumentIndex;
    pid_t Child;
    SS_DAEMON_CONTEXT Context;
    int DevNull;
    gid_t DummyGroup;
    PSTR *NewArguments;
    INT Option;
    ULONG Options;
    int Status;
    struct passwd *User;

    memset(&Context, 0, sizeof(SS_DAEMON_CONTEXT));
    Context.ChangeGroup = -1;
    Context.ChangeUser = -1;
    Context.MatchUserId = -1;
    Context.NiceLevel = -1;
    Context.Signal = SIGTERM;
    Context.Umask = -1;
    NewArguments = NULL;
    Options = 0;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             SS_DAEMON_OPTIONS_STRING,
                             SsDaemonLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'S':
            if ((Options & SS_DAEMON_OPTION_STOP) != 0) {
                SwPrintError(0,
                             NULL,
                             "Exactly one of --start or --stop must be "
                             "specified.");

                Status = 1;
                goto MainEnd;
            }

            Options |= SS_DAEMON_OPTION_START;
            break;

        case 'K':
            if ((Options & SS_DAEMON_OPTION_START) != 0) {
                SwPrintError(0,
                             NULL,
                             "Exactly one of --start or --stop must be "
                             "specified.");

                Status = 1;
                goto MainEnd;
            }

            Options |= SS_DAEMON_OPTION_STOP;
            break;

        case 'p':
            Context.PidFilePath = optarg;
            break;

        case 'x':
            Context.ExecPath = optarg;
            break;

        case 'n':
            Context.ProcessName = optarg;
            break;

        case 'u':
            Status = SwGetUserIdFromName(optarg, &(Context.MatchUserId));
            if (Status != 0) {
                Context.MatchUserId = strtoul(optarg, &AfterScan, 10);
                if (AfterScan == optarg) {
                    SwPrintError(0, optarg, "Invalid user");
                    goto MainEnd;
                }
            }

            break;

        case 'g':
            Status = SwGetGroupIdFromName(optarg, &(Context.ChangeGroup));
            if (Status != 0) {
                Context.ChangeGroup = strtoul(optarg, &AfterScan, 10);
                if (AfterScan == optarg) {
                    SwPrintError(0, optarg, "Invalid group");
                    goto MainEnd;
                }
            }

            break;

        case 's':
            Context.Signal = SwGetSignalNumberFromName(optarg);
            if (Context.Signal == -1) {
                SwPrintError(0, optarg, "Invalid signal");
                Status = EINVAL;
                goto MainEnd;
            }

            break;

        case 'a':
            Context.StartAs = optarg;
            break;

        case 't':
            Options |= SS_DAEMON_OPTION_TEST;
            break;

        case 'o':
            Options |= SS_DAEMON_OPTION_NOTHING_OK;
            break;

        case 'q':
            Options |= SS_DAEMON_OPTION_QUIET;
            Options &= ~SS_DAEMON_OPTION_VERBOSE;
            break;

        case 'v':
            Options |= SS_DAEMON_OPTION_VERBOSE;
            Options &= ~SS_DAEMON_OPTION_QUIET;
            break;

        case 'c':
            Status = SwParseUserAndGroupString(optarg,
                                               &(Context.ChangeUser),
                                               &DummyGroup);

            if (Status != 0) {
                SwPrintError(0, optarg, "Invalid user:group");
                goto MainEnd;
            }

            if (DummyGroup != -1) {
                if (Context.ChangeGroup == -1) {
                    Context.ChangeGroup = DummyGroup;
                }
            }

            break;

        case 'r':
            Context.RootDirectory = optarg;
            break;

        case 'd':
            Context.WorkingDirectory = optarg;
            break;

        case 'b':
            Options |= SS_DAEMON_OPTION_BACKGROUND;
            break;

        case 'C':
            Options |= SS_DAEMON_OPTION_NO_CLOSE;
            break;

        case 'N':
            Context.NiceLevel = strtoul(optarg, &AfterScan, 10);
            if (AfterScan == optarg) {
                SwPrintError(0, optarg, "Invalid nice level");
                Status = EINVAL;
                goto MainEnd;
            }

            Options |= SS_DAEMON_OPTION_NICE;
            break;

        case 'k':
            Context.Umask = strtoul(optarg, &AfterScan, 8);
            if (AfterScan == optarg) {
                SwPrintError(0, optarg, "Invalid umask");
                Status = EINVAL;
                goto MainEnd;
            }

            break;

        case 'm':
            Options |= SS_DAEMON_OPTION_MAKE_PIDFILE;
            break;

        case 'V':
            SwPrintVersion(SS_DAEMON_VERSION_MAJOR, SS_DAEMON_VERSION_MINOR);
            return 1;

        case 'H':
            printf(SS_DAEMON_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    Context.Options = Options;
    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    //
    // At least one of start or stop is required.
    //

    if ((Options & (SS_DAEMON_OPTION_STOP | SS_DAEMON_OPTION_START)) == 0) {
        SwPrintError(0,
                     NULL,
                     "Exactly one of --start or --stop must be specified.");

        Status = 1;
        goto MainEnd;
    }

    //
    // A pidfile is required if the caller wants to make one.
    //

    if (((Options & SS_DAEMON_OPTION_MAKE_PIDFILE) != 0) &&
        (Context.PidFilePath == NULL)) {

        SwPrintError(0, NULL, "-p is required with -m");
        Status = EINVAL;
        goto MainEnd;
    }

    //
    // Check stop requirements.
    //

    if ((Options & SS_DAEMON_OPTION_STOP) != 0) {
        if ((Context.ExecPath == NULL) &&
            (Context.PidFilePath == NULL) &&
            (Context.MatchUserId == -1) &&
            (Context.ProcessName == NULL)) {

            SwPrintError(0, NULL, "At least one of -xpun is required with -K");
            Status = EINVAL;
            goto MainEnd;
        }

    } else if ((Options & SS_DAEMON_OPTION_START) != 0) {
        if ((Context.ExecPath == NULL) && (Context.StartAs == NULL)) {
            SwPrintError(0, NULL, "At least one of -xa is required for -S");
            Status = EINVAL;
            goto MainEnd;
        }

    //
    // At least one of start or stop is required.
    //

    } else {
        SwPrintError(0,
                     NULL,
                     "Exactly one of --start or --stop must be specified.");

        Status = EINVAL;
        goto MainEnd;
    }

    //
    // If no exec name was given but a start-as was, use that as the exec name.
    //

    if (Context.StartAs == NULL) {
        Context.StartAs = Context.ExecPath;
    }

    if ((Context.ExecPath == NULL) && (Context.StartAs != NULL)) {
        Context.ExecPath = Context.StartAs;
    }

    //
    // Chroot if requested.
    //

    if (Context.RootDirectory != NULL) {
        Status = chroot(Context.RootDirectory);
        if (Status != 0) {
            Status = errno;
            SwPrintError(Status, Context.RootDirectory, "Failed to chroot");
            goto MainEnd;
        }

        Status = chdir("/");
        if (Status != 0) {
            Status = errno;
            SwPrintError(Status, Context.RootDirectory, "Failed to chdir");
            goto MainEnd;
        }
    }

    //
    // Change directories if requested too.
    //

    if (Context.WorkingDirectory != NULL) {
        Status = chdir(Context.WorkingDirectory);
        if (Status != 0) {
            Status = errno;
            SwPrintError(Status,
                         Context.WorkingDirectory,
                         "Failed to change directory");

            goto MainEnd;
        }
    }

    //
    // Open up the pid file if specified.
    //

    if (Context.PidFilePath != NULL) {
        Status = SsDaemonReadPidFile(&Context);
        if (Status != 0) {
            SwPrintError(0, Context.PidFilePath, "Failed to read");
            goto MainEnd;
        }

    } else {
        Status = SsDaemonMatchAllProcesses(&Context);
        if (Status != 0) {
            SwPrintError(0, NULL, "Failed to get process list");
        }
    }

    //
    // Perform stop actions if this is a stop request.
    //

    if ((Options & SS_DAEMON_OPTION_STOP) != 0) {
        ActionCount = SsDaemonStop(&Context);
        if (ActionCount == 0) {
            if ((Options & SS_DAEMON_OPTION_NOTHING_OK) != 0) {
                Status = 0;

            } else {
                Status = 1;
            }

        } else {
            Status = 0;
        }

        goto MainEnd;
    }

    //
    // This is a start operation. If something matches already, do nothing.
    //

    if (Context.MatchProcessIdCount != 0) {
        if ((Options & SS_DAEMON_OPTION_QUIET) == 0) {
            printf("%s is already running with pid %lu\n",
                   Context.ExecPath,
                   Context.MatchProcessIds[0]);
        }

        Status = 1;
        if ((Options & SS_DAEMON_OPTION_NOTHING_OK) != 0) {
            Status = 0;
        }

        goto MainEnd;
    }

    //
    // Create the new arguments array.
    //

    assert(ArgumentIndex != 0);

    ArgumentIndex -= 1;
    NewArguments = malloc(sizeof(PSTR) * (ArgumentCount - ArgumentIndex + 1));
    if (NewArguments == NULL) {
        Status = ENOMEM;
        goto MainEnd;
    }

    memcpy(NewArguments,
           &(Arguments[ArgumentIndex]),
           sizeof(PSTR) * (ArgumentCount - ArgumentIndex + 1));

    NewArguments[0] = Context.StartAs;

    //
    // Get into the background if requested.
    //

    if ((Options & SS_DAEMON_OPTION_BACKGROUND) != 0) {

        //
        // Fork and exit in the parent, continue in the child.
        //

        Child = fork();
        if (Child < 0) {
            Status = errno;
            SwPrintError(Status, NULL, "Failed to fork");
            goto MainEnd;
        }

        //
        // Exit immediately in the parent, continue in the child.
        //

        if (Child > 0) {
            Status = 0;
            goto MainEnd;
        }

        //
        // Become a session leader, detaching from the controlling terminal.
        //

        if (setsid() < 0) {
            Status = errno;
            goto MainEnd;
        }

        //
        // Point standard in, out, and error at /dev/null.
        //

        if ((Options & SS_DAEMON_OPTION_NO_CLOSE) == 0) {
            DevNull = SwOpen("/dev/null", O_RDWR, 0);
            if (DevNull >= 0) {
                dup2(DevNull, STDIN_FILENO);
                dup2(DevNull, STDOUT_FILENO);
                dup2(DevNull, STDERR_FILENO);
                close(DevNull);
            }

            SwCloseFrom(STDERR_FILENO + 1);
        }

        //
        // Double fork. Do this to prevent the grandchild process from ever
        // acquiring a controlling terminal. Since only the session leader
        // can acquire a controlling terminal, after the fork its new PID
        // will not be the session leader ID.
        //

        Child = fork();
        if (Child < 0) {
            exit(1);

        } else if (Child != 0) {
            exit(0);
        }

        //
        // The remainder now runs as the grandchild.
        //

    }

    //
    // Write the pid file.
    //

    if ((Options & SS_DAEMON_OPTION_MAKE_PIDFILE) != 0) {
        SsDaemonWritePidFile(&Context);
    }

    //
    // Change identity if requested.
    //

    if (Context.ChangeUser != -1) {
        User = getpwuid(Context.ChangeUser);
        if (Context.ChangeGroup != -1) {
            User->pw_gid = Context.ChangeGroup;
        }

        SwBecomeUser(User);

    } else if (Context.ChangeGroup != -1) {
        Status = setgid(Context.ChangeGroup);
        if (Status != 0) {
            Status = errno;
            SwPrintError(Status, NULL, "setgid failed");
            goto MainEnd;
        }

        setgroups(1, &(Context.ChangeGroup));
    }

    //
    // Change nice level if requested.
    //

    if ((Options & SS_DAEMON_OPTION_NICE) != 0) {
        Status = nice(Context.NiceLevel);
        if (Status != 0) {
            Status = errno;
            SwPrintError(Status, NULL, "nice failed");
            goto MainEnd;
        }
    }

    //
    // Change the umask if requested.
    //

    if (Context.Umask != -1) {
        umask(Context.Umask);
    }

    //
    // Make it rain.
    //

    execvp(NewArguments[0], NewArguments);
    Status = errno;
    SwPrintError(Status, Arguments[ArgumentIndex], "Cannot execute");

MainEnd:
    if (NewArguments != NULL) {
        free(NewArguments);
    }

    if (Context.MatchProcessIds != NULL) {
        free(Context.MatchProcessIds);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
SsDaemonReadPidFile (
    PSS_DAEMON_CONTEXT Context
    )

/*++

Routine Description:

    This routine tries to read a pid file. It is not an error if the file does
    not exist.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success or if the file does not exist.

    Non-zero on any other failures.

--*/

{

    FILE *File;
    unsigned int ScannedPid;

    File = fopen(Context->PidFilePath, "r");
    if (File == NULL) {
        if (errno == ENOENT) {
            return 0;
        }

        SwPrintError(errno, Context->PidFilePath, "Cannot open");
        return errno;
    }

    if (fscanf(File, "%u", &ScannedPid) != 1) {
        fclose(File);
        return EINVAL;
    }

    fclose(File);
    SsDaemonMatchPid(Context, ScannedPid);
    return 0;
}

VOID
SsDaemonWritePidFile (
    PSS_DAEMON_CONTEXT Context
    )

/*++

Routine Description:

    This routine tries to write the current process ID to a pid file.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None. At this point it's too late for errors.

--*/

{

    CHAR Buffer[64];
    ssize_t BytesWritten;
    int File;
    int Size;
    size_t TotalBytesWritten;

    File = SwOpen(Context->PidFilePath, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if (File >= 0) {
        Size = snprintf(Buffer,
                        sizeof(Buffer),
                        "%lu",
                        (long unsigned int)getpid());

        TotalBytesWritten = 0;
        do {
            BytesWritten = write(File,
                                 Buffer + TotalBytesWritten,
                                 Size - TotalBytesWritten);

            if (BytesWritten <= 0) {
                if (errno == EINTR) {
                    continue;
                }

                break;
            }

            TotalBytesWritten += BytesWritten;

        } while (TotalBytesWritten < Size);

        close(File);
    }

    return;
}

INT
SsDaemonMatchAllProcesses (
    PSS_DAEMON_CONTEXT Context
    )

/*++

Routine Description:

    This routine reads in all the current processes.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    size_t Count;
    UINTN Index;
    pid_t *ProcessIds;
    size_t Size;
    INT Status;

    ProcessIds = NULL;
    Size = 0;
    SwGetProcessIdList(NULL, &Size);
    if (Size == 0) {
        return ESRCH;
    }

    //
    // Add a fudge factor in case more processes come in.
    //

    Size *= 2;
    ProcessIds = malloc(Size);
    if (ProcessIds == NULL) {
        return ENOMEM;
    }

    memset(ProcessIds, 0, Size);
    Status = SwGetProcessIdList(ProcessIds, &Size);
    if (Status != 0) {
        SwPrintError(0, NULL, "Failed to get process ID list");
        goto GetAllProcessesEnd;
    }

    Count = Size / sizeof(pid_t);
    for (Index = 0; Index < Count; Index += 1) {
        SsDaemonMatchPid(Context, ProcessIds[Index]);
    }

    Status = 0;

GetAllProcessesEnd:
    if (ProcessIds != NULL) {
        free(ProcessIds);
    }

    return Status;
}

VOID
SsDaemonMatchPid (
    PSS_DAEMON_CONTEXT Context,
    pid_t ProcessId
    )

/*++

Routine Description:

    This routine matches a given process ID against the user specified criteria.
    If the given process ID matches, it will be added to the context's array
    of matching processes.

Arguments:

    Context - Supplies a pointer to the application context.

    ProcessId - Supplies the process ID to match against.

Return Value:

    None.

--*/

{

    PSTR BaseName;
    BOOL Match;
    PVOID NewBuffer;
    UINTN NewCapacity;
    PSWISS_PROCESS_INFORMATION ProcessInformation;
    int Status;

    Status = SwGetProcessInformation(ProcessId, &ProcessInformation);
    if (Status != 0) {
        return;
    }

    Match = TRUE;
    if (Context->ExecPath != NULL) {
        if (strcmp(ProcessInformation->Name, Context->ExecPath) != 0) {
            Match = FALSE;
        }
    }

    if ((Match != FALSE) && (Context->ProcessName != NULL)) {
        BaseName = basename(ProcessInformation->Name);
        if (strcmp(BaseName, Context->ProcessName) != 0) {
            Match = FALSE;
        }
    }

    if ((Match != FALSE) && (Context->MatchUserId != -1)) {
        if (ProcessInformation->RealUserId != Context->MatchUserId) {
            Match = FALSE;
        }
    }

    SwDestroyProcessInformation(ProcessInformation);
    if (Match == FALSE) {
        return;
    }

    //
    // Add the process to the match array.
    //

    if (Context->MatchProcessIdCount >= Context->MatchProcessIdCapacity) {
        if (Context->MatchProcessIdCount == 0) {
            NewCapacity = SS_DAEMON_INITIAL_MATCH_ARRAY_SIZE;

        } else {
            NewCapacity = Context->MatchProcessIdCapacity * 2;
        }

        assert(NewCapacity > Context->MatchProcessIdCount + 1);

        NewBuffer = realloc(Context->MatchProcessIds,
                            NewCapacity * sizeof(pid_t));

        if (NewBuffer == NULL) {

            //
            // Quietly ignore allocation failures.
            //

            return;
        }

        Context->MatchProcessIds = NewBuffer;
        Context->MatchProcessIdCapacity = NewCapacity;
    }

    Context->MatchProcessIds[Context->MatchProcessIdCount] = ProcessId;
    Context->MatchProcessIdCount += 1;
    return;
}

INT
SsDaemonStop (
    PSS_DAEMON_CONTEXT Context
    )

/*++

Routine Description:

    This routine performs stop actions on matchind PIDs.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    Returns the number of actions performed.

--*/

{

    UINTN Index;
    PSTR Plural;
    INT ProcessesKilled;
    INT Signal;

    ProcessesKilled = 0;
    if (Context->MatchProcessIdCount == 0) {
        SsDaemonPrintStopDescription(Context);
        printf(": No processes found\n");
        goto StopEnd;
    }

    for (Index = 0; Index < Context->MatchProcessIdCount; Index += 1) {
        Signal = Context->Signal;
        if ((Context->Options & SS_DAEMON_OPTION_TEST) != 0) {
            Signal = 0;
        }

        if (kill(Context->MatchProcessIds[Index], Signal) == 0) {
            ProcessesKilled += 1;

        } else {
            SwPrintError(errno,
                         NULL,
                         "Failed to kill process %lu",
                         (long unsigned int)(Context->MatchProcessIds[Index]));

            Context->MatchProcessIds[Index] = 0;
            if ((Context->Options & SS_DAEMON_OPTION_TEST) != 0) {
                ProcessesKilled = -1;
                goto StopEnd;
            }
        }
    }

    if ((ProcessesKilled > 0) &&
        ((Context->Options & SS_DAEMON_OPTION_QUIET) == 0)) {

        printf("Stopped ");
        SsDaemonPrintStopDescription(Context);
        Plural = "";
        if (ProcessesKilled > 1) {
            Plural = "s";
        }

        printf(" (pid%s", Plural);
        for (Index = 0; Index < Context->MatchProcessIdCount; Index += 1) {
            if (Context->MatchProcessIds[Index] != 0) {
                printf(" %lu",
                       (long unsigned int)(Context->MatchProcessIds[Index]));
            }
        }

        printf(")\n");
    }

StopEnd:
    return ProcessesKilled;
}

VOID
SsDaemonPrintStopDescription (
    PSS_DAEMON_CONTEXT Context
    )

/*++

Routine Description:

    This routine prints to standard out a description of what is being stopped,
    without a newline.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    PSTR Space;

    Space = "";
    if (Context->ProcessName != NULL) {
        printf("%s", Context->ProcessName);
        Space = " ";
    }

    if (Context->ExecPath != NULL) {
        printf("%s%s", Space, Context->ExecPath);
        Space = " ";
    }

    if (Context->PidFilePath != NULL) {
        printf("%sprocess in pid file '%s'", Space, Context->PidFilePath);
        Space = " ";
    }

    if (Context->MatchUserId != -1) {
        printf("%sprocesses owned by user %ld",
               Space,
               (long int)Context->MatchUserId);
    }

    return;
}

