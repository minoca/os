/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    init.c

Abstract:

    This module implements the init utility, which serves as the first user
    process on most Unix-like operating systems.

Author:

    Evan Green 18-Mar-2015

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#ifndef __FreeBSD__
#include <alloca.h>
#endif
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <utmpx.h>

#include "swlib.h"
#include "login/lutil.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro converts the given character into its control code.
//

#define INIT_CONTROL(_Character) ((_Character) ^ 0x40)

//
// ---------------------------------------------------------------- Definitions
//

#define INIT_VERSION_MAJOR 1
#define INIT_VERSION_MINOR 0

#define INIT_USAGE                                                             \
    "usage: init [options] [runlevel]\n"                                       \
    "The init utility performs system initialization steps. The runlevel be \n"\
    "1-6, a-c (for on-demand runlevels), q to re-examine inittab, s to \n"     \
    "switch to single user mode, or u to re-execute. Options are:\n"           \
    "  -d, --debug -- Debug mode, prints more things.\n"                       \
    "  -s, -S, --single -- Single-user mode. Examines /etc/inittab and runs \n"\
    "      bootup rc scripts, then runs a single user shell.\n"                \
    "  -b, --emergency -- Boot directly into a single user shell without\n"    \
    "      running any other startup scripts.\n"                               \
    "  --help -- Displays this help text and exits.\n"                         \
    "  --version -- Displays the application version and exits.\n"

#define INIT_OPTIONS_STRING "bdsS:hV"
#define INIT_DEFAULT_TERMINAL_TYPE "xterm"
#define INIT_INITTAB_PATH "/etc/inittab"
#define INIT_DEFAULT_CONSOLE "/dev/console"

#define INIT_INIT_SCRIPT "/etc/init.d/rcS"

//
// Define the time between sending SIGTERM and SIGKILL when reloading inittab
// or switching runlevels.
//

#define INIT_KILL_DELAY 5

//
// Define application options.
//

//
// Set this option to to boot into a single user shell, non-emergency.
//

#define INIT_OPTION_SINGLE_USER 0x00000001

//
// Set this option to boot into a single user shell, emergency mode.
//

#define INIT_OPTION_EMERGENCY 0x00000002

//
// Set this option to enable debug mode.
//

#define INIT_OPTION_DEBUG 0x00000004

//
// Define init log destinations.
//

#define INIT_LOG_SYSLOG 0x00000001
#define INIT_LOG_CONSOLE 0x00000002
#define INIT_LOG_DEBUG 0x00000004

//
// Define the init runlevel masks.
//

#define INIT_RUNLEVEL_0 0x00000001
#define INIT_RUNLEVEL_1 0x00000002
#define INIT_RUNLEVEL_2 0x00000004
#define INIT_RUNLEVEL_3 0x00000008
#define INIT_RUNLEVEL_4 0x00000010
#define INIT_RUNLEVEL_5 0x00000020
#define INIT_RUNLEVEL_6 0x00000040
#define INIT_RUNLEVEL_7 0x00000080
#define INIT_RUNLEVEL_8 0x00000100
#define INIT_RUNLEVEL_9 0x00000200
#define INIT_RUNLEVEL_A 0x00000400
#define INIT_RUNLEVEL_B 0x00000800
#define INIT_RUNLEVEL_C 0x00001000
#define INIT_RUNLEVEL_S 0x00002000

#define INIT_RUNLEVEL_MASK 0x00003FFF

#define INIT_RUNLEVEL_NAMES "0123456789ABCS"

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _INIT_ACTION_TYPE {
    InitActionInvalid,
    InitActionNone,
    InitActionSysinit,
    InitActionBoot,
    InitActionBootWait,
    InitActionWait,
    InitActionOnce,
    InitActionRespawn,
    InitActionCtrlAltDel,
    InitActionShutdown,
    InitActionRestart,
    InitActionOnDemand,
    InitActionInitDefault,
    InitActionCount
} INIT_ACTION_TYPE, *PINIT_ACTION_TYPE;

typedef enum _INIT_REBOOT_PHASE {
    InitNotRebooting,
    InitRebootRunningActions,
    InitRebootTerm,
    InitRebootKill,
    InitRebootComplete
} INIT_REBOOT_PHASE, *PINIT_REBOOT_PHASE;

/*++

Structure Description:

    This structure stores information about an init action.

Members:

    ListEntry - Stores pointers to the next and previous init actions.

    ProcessId - Stores the process ID if the action is currently running.

    Id - Stores the ID of the command.

    RunLevels - Stores the bitmask of runlevels this action applies to.

    Type - Stores the action flavor.

    Command - Stores the command to execute.

--*/

typedef struct _INIT_ACTION {
    LIST_ENTRY ListEntry;
    pid_t ProcessId;
    CHAR Id[5];
    ULONG RunLevels;
    INIT_ACTION_TYPE Type;
    PSTR Command;
} INIT_ACTION, *PINIT_ACTION;

/*++

Structure Description:

    This structure stores information about an init application instance.

Members:

    SyslogOpen - Stores a boolean indicating whether the system log has been
        opened yet or not.

    Options - Stores the application options. See INIT_OPTION_* definitions.

    ActionList - Stores the head of the list of init actions.

    DefaultRunLevel - Stores the default runlevel (mask) to go to. Only one
        bit should be set here.

    CurrentRunLevel - Stores the current runlevel (mask). Only one bit should
        be set here.

    PreviousRunLevel - Stores the previous runlevel (mask). Only one bit should
        be set.

    RebootPhase - Stores the phase of system reboot init is currently working
        towards.

    RebootSignal - Stores the signal that initiated the reboot action, which
        also dictates the type.

--*/

typedef struct _INIT_CONTEXT {
    BOOL SyslogOpen;
    ULONG Options;
    LIST_ENTRY ActionList;
    ULONG DefaultRunLevel;
    ULONG CurrentRunLevel;
    ULONG PreviousRunLevel;
    INIT_REBOOT_PHASE RebootPhase;
    ULONG RebootSignal;
} INIT_CONTEXT, *PINIT_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
InitInitializeConsole (
    PINIT_CONTEXT Context
    );

VOID
InitConfigureTerminal (
    VOID
    );

BOOL
InitCheckSignals (
    PINIT_CONTEXT Context
    );

VOID
InitReloadInittab (
    PINIT_CONTEXT Context
    );

VOID
InitReexec (
    PINIT_CONTEXT Context
    );

VOID
InitRunResetSystem (
    PINIT_CONTEXT Context,
    INT Signal
    );

VOID
InitShutdownAndKillProcesses (
    PINIT_CONTEXT Context
    );

VOID
InitReboot (
    PINIT_CONTEXT Context,
    SWISS_REBOOT_TYPE RebootType
    );

VOID
InitParseInittab (
    PINIT_CONTEXT Context
    );

VOID
InitCreateAction (
    PINIT_CONTEXT Context,
    PSTR Id,
    ULONG RunLevels,
    INIT_ACTION_TYPE ActionType,
    PSTR Command
    );

VOID
InitRunActions (
    PINIT_CONTEXT Context,
    INIT_ACTION_TYPE ActionType,
    ULONG RunLevelMask
    );

pid_t
InitRunAction (
    PINIT_CONTEXT Context,
    PINIT_ACTION Action
    );

VOID
InitExec (
    PINIT_CONTEXT Context,
    PSTR Command
    );

VOID
InitWaitForProcess (
    PINIT_CONTEXT Context,
    pid_t ProcessId
    );

PINIT_ACTION
InitMarkProcessTerminated (
    PINIT_CONTEXT Context,
    pid_t ProcessId,
    int Status
    );

VOID
InitResetSignalHandlers (
    VOID
    );

VOID
InitLog (
    PINIT_CONTEXT Context,
    ULONG Destination,
    PSTR Format,
    ...
    );

void
InitSignalHandler (
    int Signal
    );

void
InitAddUtmpEntry (
    PINIT_ACTION Action
    );

//
// -------------------------------------------------------------------- Globals
//

extern char **environ;

struct option InitLongOptions[] = {
    {"debug", no_argument, 0, 'd'},
    {"single", no_argument, 0, 's'},
    {"emergency", no_argument, 0, 'b'},
    {"help", no_argument, 0, 'H'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

const PSTR InitActionTypeNames[InitActionCount] = {
    "INVALID",
    "off",
    "sysinit",
    "boot",
    "bootwait",
    "wait",
    "once",
    "respawn",
    "ctrlaltdel",
    "shutdown",
    "restart",
    "ondemand",
    "initdefault"
};

PUINTN InitSignalCounts = NULL;

//
// ------------------------------------------------------------------ Functions
//

INT
InitMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the init utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PINIT_ACTION Action;
    ULONG ArgumentIndex;
    CHAR Character;
    INIT_CONTEXT Context;
    PLIST_ENTRY CurrentEntry;
    UINTN Index;
    int NoHang;
    INT Option;
    ULONG Options;
    pid_t ProcessId;
    PSTR RunLevelString;
    struct sigaction SignalAction;
    UINTN SignalCounts[NSIG];
    int Status;

    Options = 0;
    RunLevelString = NULL;
    memset(&Context, 0, sizeof(INIT_CONTEXT));
    INITIALIZE_LIST_HEAD(&(Context.ActionList));
    memset(SignalCounts, 0, sizeof(SignalCounts));
    InitSignalCounts = SignalCounts;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             INIT_OPTIONS_STRING,
                             InitLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'b':
            Options |= INIT_OPTION_EMERGENCY;
            break;

        case 'd':
            Options |= INIT_OPTION_DEBUG;
            break;

        case 'S':
        case 's':
            Options |= INIT_OPTION_SINGLE_USER;
            break;

        case 'V':
            SwPrintVersion(INIT_VERSION_MAJOR, INIT_VERSION_MINOR);
            return 1;

        case 'h':
            printf(INIT_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    if (ArgumentIndex < ArgumentCount) {
        RunLevelString = Arguments[ArgumentIndex];
        ArgumentIndex += 1;
    }

    Context.Options = Options;
    InitInitializeConsole(&Context);
    InitConfigureTerminal();
    Status = chdir("/");
    if (Status != 0) {
        Status = errno;
        goto MainEnd;
    }

    if (setsid() < 0) {
        Status = errno;
        goto MainEnd;
    }

    //
    // Set some default environment variables.
    //

    setenv("HOME", "/", 0);
    setenv("PATH", SUPERUSER_DEFAULT_PATH, 0);
    setenv("SHELL", USER_FALLBACK_SHELL, 0);
    if (RunLevelString != NULL) {
        setenv("RUNLEVEL", RunLevelString, 1);
    }

    InitLog(&Context,
            INIT_LOG_SYSLOG | INIT_LOG_DEBUG,
            "Minoca init v%d.%d.%d",
            INIT_VERSION_MAJOR,
            INIT_VERSION_MINOR,
            SwGetSerialVersion());

    Status = 0;

    //
    // In emergency mode, just specify a shell to drop into.
    //

    if ((Options & INIT_OPTION_EMERGENCY) != 0) {
        InitCreateAction(&Context,
                         "0",
                         INIT_RUNLEVEL_MASK,
                         InitActionRespawn,
                         USER_FALLBACK_SHELL);

    } else {
        InitParseInittab(&Context);
    }

    //
    // In single-user mode, shoot for S.
    //

    if ((Options & INIT_OPTION_SINGLE_USER) != 0) {
        Context.DefaultRunLevel = INIT_RUNLEVEL_S;

    //
    // Shoot for whatever runlevel is on the command line.
    //

    } else if (RunLevelString != NULL) {
        if (strlen(RunLevelString) != 1) {
            InitLog(&Context,
                    INIT_LOG_SYSLOG | INIT_LOG_CONSOLE,
                    "Invalid runlevel argument: %s",
                    RunLevelString);

        } else {
            Index = 0;
            Character = toupper(*RunLevelString);
            while (INIT_RUNLEVEL_NAMES[Index] != '\0') {
                if (INIT_RUNLEVEL_NAMES[Index] == Character) {
                    Context.DefaultRunLevel = 1 << Index;
                    break;
                }
            }

            if (INIT_RUNLEVEL_NAMES[Index] == '\0') {
                InitLog(&Context,
                        INIT_LOG_SYSLOG | INIT_LOG_CONSOLE,
                        "Invalid runlevel argument: %s",
                        RunLevelString);
            }
        }
    }

    Context.CurrentRunLevel = Context.DefaultRunLevel;
    Context.PreviousRunLevel = Context.CurrentRunLevel;

    //
    // Set up the signal handlers.
    //

    memset(&SignalAction, 0, sizeof(SignalAction));
    sigfillset(&(SignalAction.sa_mask));
    SignalAction.sa_handler = SIG_IGN;
    sigaction(SIGTSTP, &SignalAction, NULL);
    SignalAction.sa_handler = InitSignalHandler;
    sigaction(SIGINT, &SignalAction, NULL);
    sigaction(SIGQUIT, &SignalAction, NULL);
    sigaction(SIGUSR1, &SignalAction, NULL);
    sigaction(SIGUSR2, &SignalAction, NULL);
    sigaction(SIGTERM, &SignalAction, NULL);
    sigaction(SIGHUP, &SignalAction, NULL);
    sigaction(SIGALRM, &SignalAction, NULL);

    //
    // Perform the actions.
    //

    InitRunActions(&Context, InitActionSysinit, 0);
    InitCheckSignals(&Context);
    InitRunActions(&Context, InitActionBoot, 0);
    InitCheckSignals(&Context);
    InitRunActions(&Context, InitActionBootWait, 0);
    InitCheckSignals(&Context);
    InitRunActions(&Context, InitActionWait, Context.CurrentRunLevel);
    InitCheckSignals(&Context);
    InitRunActions(&Context, InitActionOnce, Context.CurrentRunLevel);

    //
    // Now loop forever.
    //

    while (TRUE) {
        NoHang = 0;
        if (InitCheckSignals(&Context) != FALSE) {
            NoHang = WNOHANG;
        }

        //
        // Respawn processes unless a reboot is in progress.
        //

        if (Context.RebootPhase == InitNotRebooting) {
            InitRunActions(&Context,
                           InitActionRespawn,
                           Context.CurrentRunLevel);
        }

        if (InitCheckSignals(&Context) != FALSE) {
            NoHang = WNOHANG;
        }

        if (Context.RebootPhase == InitNotRebooting) {
            sleep(1);
        }

        if (InitCheckSignals(&Context) != FALSE) {
            NoHang = WNOHANG;
        }

        //
        // Loop getting all dead processes. There is a race in here where if a
        // new signal were to come in now but no child processes were ready,
        // the wait would just block, leaving a signal delivered but not dealt
        // with. That signal will be dealt with once the next child dies.
        //

        while (TRUE) {
            ProcessId = waitpid(-1, &Status, NoHang);
            if (ProcessId <= 0) {

                //
                // If there are no more children left and a reboot is requested,
                // go do it now.
                //

                if ((Context.RebootPhase > InitRebootRunningActions) &&
                    (errno == ECHILD) && (NoHang == 0)) {

                    Context.RebootPhase = InitRebootComplete;
                    InitRunResetSystem(&Context, 0);
                }

                break;
            }

            InitMarkProcessTerminated(&Context, ProcessId, Status);
            NoHang = WNOHANG;
        }
    }

    Status = 0;

MainEnd:
    InitResetSignalHandlers();
    CurrentEntry = Context.ActionList.Next;
    while (CurrentEntry != &(Context.ActionList)) {
        Action = LIST_VALUE(CurrentEntry, INIT_ACTION, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        free(Action);
    }

    InitSignalCounts = NULL;
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
InitInitializeConsole (
    PINIT_CONTEXT Context
    )

/*++

Routine Description:

    This routine initializes the console.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    PSTR Console;
    int Descriptor;
    PSTR TermVariable;

    Console = getenv("CONSOLE");
    if (Console == NULL) {
        Console = getenv("console");
    }

    //
    // Set it to a default.
    //

    if (Console == NULL) {
        Console = INIT_DEFAULT_CONSOLE;
        setenv("CONSOLE", Console, 1);
    }

    if (Console != NULL) {
        Descriptor = SwOpen(Console, O_RDWR | O_NONBLOCK | O_NOCTTY, 0);
        if (Descriptor >= 0) {
            dup2(Descriptor, STDIN_FILENO);
            dup2(Descriptor, STDOUT_FILENO);
            dup2(Descriptor, STDERR_FILENO);
            if (Descriptor > STDERR_FILENO) {
                close(Descriptor);
            }
        }

        InitLog(Context, INIT_LOG_SYSLOG, "CONSOLE=%s", Console);
    }

    TermVariable = getenv("TERM");
    if (TermVariable == NULL) {
        setenv("TERM", INIT_DEFAULT_TERMINAL_TYPE, 1);
    }

    return;
}

VOID
InitConfigureTerminal (
    VOID
    )

/*++

Routine Description:

    This routine sets some sane defaults for the terminal.

Arguments:

    None.

Return Value:

    None.

--*/

{

    struct termios Settings;

    if (tcgetattr(STDIN_FILENO, &Settings) != 0) {
        return;
    }

    Settings.c_cc[VINTR] = INIT_CONTROL('C');
    Settings.c_cc[VQUIT] = INIT_CONTROL('\\');
    Settings.c_cc[VERASE] = INIT_CONTROL('?');
    Settings.c_cc[VKILL] = INIT_CONTROL('U');
    Settings.c_cc[VEOF] = INIT_CONTROL('D');
    Settings.c_cc[VSTART] = INIT_CONTROL('Q');
    Settings.c_cc[VSTOP] = INIT_CONTROL('S');
    Settings.c_cc[VSUSP] = INIT_CONTROL('Z');

    //
    // Save the character size, stop bits, and parity configuration. Add in
    // receiver enable, hangup on close, and the local flag.
    //

    Settings.c_cflag &= CSIZE | CSTOPB | PARENB | PARODD;
    Settings.c_cflag |= CREAD | HUPCL | CLOCAL;
    Settings.c_iflag = ICRNL | IXON | IXOFF | IMAXBEL;
    Settings.c_oflag = OPOST | ONLCR;
    Settings.c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOKE | ECHOCTL | IEXTEN;
    tcsetattr(STDIN_FILENO, TCSANOW, &Settings);
    return;
}

BOOL
InitCheckSignals (
    PINIT_CONTEXT Context
    )

/*++

Routine Description:

    This routine checks for any signals that might have occurred recently.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    TRUE if a signal was processed.

    FALSE if no signals were processed.

--*/

{

    INT Signal;
    BOOL SignalsSeen;

    SignalsSeen = FALSE;
    while (TRUE) {

        //
        // Exit quickly if no signals occurred.
        //

        if (InitSignalCounts[0] == 0) {
            break;
        }

        //
        // Clear the "signals seen" boolean before checking.
        //

        InitSignalCounts[0] = 0;
        for (Signal = 1; Signal < NSIG; Signal += 1) {
            if (InitSignalCounts[Signal] != 0) {
                SignalsSeen = TRUE;
                InitSignalCounts[Signal] = 0;
                if (Signal == SIGINT) {
                    InitRunActions(Context, InitActionCtrlAltDel, 0);

                } else if (Signal == SIGQUIT) {
                    InitReexec(Context);

                } else if (Signal == SIGHUP) {
                    InitReloadInittab(Context);

                } else if (Signal == SIGALRM) {
                    if (Context->RebootPhase == InitRebootTerm) {
                        Context->RebootPhase = InitRebootKill;
                        InitRunResetSystem(Context, Signal);

                    } else if (Context->RebootPhase == InitRebootKill) {
                        Context->RebootPhase = InitRebootComplete;
                        InitRunResetSystem(Context, Signal);
                    }

                //
                // Other signals initiate a reboot.
                //

                } else {
                    if (Context->RebootPhase == InitNotRebooting) {
                        InitRunResetSystem(Context, Signal);
                    }
                }
            }
        }
    }

    return SignalsSeen;
}

VOID
InitReloadInittab (
    PINIT_CONTEXT Context
    )

/*++

Routine Description:

    This routine attempts to reload the inittab file, and reconcile the
    processes.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    PINIT_ACTION Action;
    pid_t Child;
    PLIST_ENTRY CurrentEntry;

    InitLog(Context,
            INIT_LOG_SYSLOG | INIT_LOG_DEBUG,
            "Reloading inittab");

    //
    // Clear out all the action types to know which entries don't show up
    // in the new file.
    //

    CurrentEntry = Context->ActionList.Next;
    while (CurrentEntry != &(Context->ActionList)) {
        Action = LIST_VALUE(CurrentEntry, INIT_ACTION, ListEntry);
        Action->Type = InitActionInvalid;
        CurrentEntry = CurrentEntry->Next;
    }

    Context->PreviousRunLevel = Context->CurrentRunLevel;
    InitParseInittab(Context);

    //
    // Remove any leftover entries.
    //

    CurrentEntry = Context->ActionList.Next;
    while (CurrentEntry != &(Context->ActionList)) {
        Action = LIST_VALUE(CurrentEntry, INIT_ACTION, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // Kill a running entry that's either 1) not in the new inittab or
        // 2) Got a runlevel and it's not the current runlevel.
        //

        if ((Action->ProcessId > 0) &&
            ((Action->Type == InitActionInvalid) ||
             ((Action->RunLevels != 0) &&
              ((Action->RunLevels & Context->CurrentRunLevel) == 0)))) {

            InitLog(Context,
                    INIT_LOG_DEBUG,
                    "Killing: %d: %s",
                    Action->ProcessId,
                    Action->Command);

            kill(Action->ProcessId, SIGTERM);
        }
    }

    //
    // Fork, wait a bit, and then send kill to all these processes.
    //

    Child = fork();
    if (Child == 0) {
        sleep(INIT_KILL_DELAY);
        CurrentEntry = Context->ActionList.Next;
        while (CurrentEntry != &(Context->ActionList)) {
            Action = LIST_VALUE(CurrentEntry, INIT_ACTION, ListEntry);
            if ((Action->ProcessId > 0) &&
                ((Action->Type == InitActionInvalid) ||
                 ((Action->RunLevels != 0) &&
                  ((Action->RunLevels & Context->CurrentRunLevel) == 0)))) {

                kill(Action->ProcessId, SIGKILL);
            }

            CurrentEntry = CurrentEntry->Next;
        }

        _exit(0);
    }

    //
    // Remove the unused entries. Also take the opportunity to free sysinit and
    // boot entries, which are never used again.
    //

    CurrentEntry = Context->ActionList.Next;
    while (CurrentEntry != &(Context->ActionList)) {
        Action = LIST_VALUE(CurrentEntry, INIT_ACTION, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if ((Action->Type == InitActionInvalid) ||
            (Action->Type == InitActionSysinit) ||
            (Action->Type == InitActionBoot) ||
            (Action->Type == InitActionBootWait)) {

            LIST_REMOVE(&(Action->ListEntry));
            free(Action);
        }
    }

    return;
}

VOID
InitReexec (
    PINIT_CONTEXT Context
    )

/*++

Routine Description:

    This routine attempts to run the restart action, execing init into that
    action.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None. Does not return on success.

--*/

{

    PINIT_ACTION Action;
    PLIST_ENTRY CurrentEntry;

    CurrentEntry = Context->ActionList.Next;
    while (CurrentEntry != &(Context->ActionList)) {
        Action = LIST_VALUE(CurrentEntry, INIT_ACTION, ListEntry);
        if (Action->Type == InitActionRestart) {
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    if (CurrentEntry == &(Context->ActionList)) {
        InitLog(Context, INIT_LOG_SYSLOG, "No restart action found");
        return;
    }

    InitResetSignalHandlers();
    InitShutdownAndKillProcesses(Context);
    InitLog(Context,
            INIT_LOG_SYSLOG | INIT_LOG_CONSOLE,
            "Re-exec init: %s",
            Action->Command);

    InitAddUtmpEntry(Action);
    InitExec(Context, Action->Command);
    InitReboot(Context, RebootTypeHalt);
    return;
}

VOID
InitRunResetSystem (
    PINIT_CONTEXT Context,
    INT Signal
    )

/*++

Routine Description:

    This routine runs the tasks associated with resetting the system, and then
    resets the system.

Arguments:

    Context - Supplies a pointer to the application context.

    Signal - Supplies the signal, which dictates the type of actions and
        power state to enter.

Return Value:

    None.

--*/

{

    PSTR Message;
    SWISS_REBOOT_TYPE RebootType;

    switch (Context->RebootPhase) {
    case InitNotRebooting:
        Context->RebootPhase = InitRebootRunningActions;
        Context->RebootSignal = Signal;
        InitResetSignalHandlers();

    case InitRebootRunningActions:
        InitShutdownAndKillProcesses(Context);
        Context->RebootPhase = InitRebootTerm;

    //
    // Fall through.
    //

    case InitRebootTerm:
    case InitRebootKill:
        InitShutdownAndKillProcesses(Context);
        alarm(10);
        break;

    case InitRebootComplete:
        Signal = Context->RebootSignal;
        Message = "halt";
        RebootType = RebootTypeHalt;
        if (Signal == SIGTERM) {
            Message = "reboot";
            RebootType = RebootTypeWarm;

        } else if (Signal == SIGUSR2) {
            Message = "poweroff";
        }

        InitLog(Context,
                INIT_LOG_CONSOLE | INIT_LOG_SYSLOG,
                "Requesting system %s.",
                Message);

        InitReboot(Context, RebootType);
        break;

    default:

        assert(FALSE);

        break;
    }

    return;
}

VOID
InitShutdownAndKillProcesses (
    PINIT_CONTEXT Context
    )

/*++

Routine Description:

    This routine runs the shutdown action and attempts to kill all processes.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    if (Context->RebootPhase == InitNotRebooting) {
        InitRunActions(Context, InitActionShutdown, 0);
        InitLog(Context,
                INIT_LOG_CONSOLE | INIT_LOG_SYSLOG,
                "The system is going down.");

        kill(-1, SIGTERM);
        sleep(1);
        kill(-1, SIGKILL);
        sync();

    } else if (Context->RebootPhase == InitRebootRunningActions) {
        InitRunActions(Context, InitActionShutdown, 0);
        InitLog(Context,
                INIT_LOG_CONSOLE | INIT_LOG_SYSLOG,
                "The system is going down.");

    } else if (Context->RebootPhase == InitRebootTerm) {
        kill(-1, SIGTERM);
        InitLog(Context,
                INIT_LOG_CONSOLE | INIT_LOG_SYSLOG,
                "Sent SIG%s to all processes.",
                "TERM");

    } else if (Context->RebootPhase == InitRebootKill) {
        kill(-1, SIGKILL);
        InitLog(Context,
                INIT_LOG_CONSOLE | INIT_LOG_SYSLOG,
                "Sent SIG%s to all processes.",
                "KILL");
    }

    return;
}

VOID
InitReboot (
    PINIT_CONTEXT Context,
    SWISS_REBOOT_TYPE RebootType
    )

/*++

Routine Description:

    This routine actually resets the system.

Arguments:

    Context - Supplies a pointer to the application context.

    RebootType - Supplies the type of reboot to perform.

Return Value:

    None.

--*/

{

    pid_t Child;

    sleep(1);

    //
    // Do this in a child process since some reboot implementations exit,
    // which some OSes might have a problem with for pid 1.
    //

    Child = fork();
    if (Child == 0) {
        SwResetSystem(RebootType);
        _exit(0);
    }

    _exit(0);
    return;
}

VOID
InitParseInittab (
    PINIT_CONTEXT Context
    )

/*++

Routine Description:

    This routine parses the inittab file.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    INIT_ACTION_TYPE ActionType;
    CHAR Character;
    PSTR Colon;
    PSTR Fields[4];
    FILE *File;
    UINTN Index;
    char *Line;
    size_t LineBufferSize;
    ssize_t LineSize;
    ULONG RunLevelMask;
    INT Status;
    PSTR String;

    Line = NULL;
    LineBufferSize = 0;
    File = fopen(INIT_INITTAB_PATH, "r");
    if (File == NULL) {
        Status = errno;

        //
        // If there is no inittab, create a basic one.
        //

        if (Status == ENOENT) {
            InitCreateAction(Context,
                             "1",
                             INIT_RUNLEVEL_1,
                             InitActionSysinit,
                             INIT_INIT_SCRIPT);

            InitCreateAction(Context,
                             "2",
                             INIT_RUNLEVEL_1,
                             InitActionOnce,
                             USER_FALLBACK_SHELL);

            InitCreateAction(Context,
                             "3",
                             INIT_RUNLEVEL_1,
                             InitActionInitDefault,
                             NULL);

            InitCreateAction(Context,
                             "4",
                             0,
                             InitActionCtrlAltDel,
                             "reboot");

            InitCreateAction(Context,
                             "5",
                             0,
                             InitActionShutdown,
                             "reboot -h");

            InitCreateAction(Context,
                             "6",
                             0,
                             InitActionRestart,
                             "init");

            Status = 0;
        }

        goto ParseInittabEnd;
    }

    //
    // Loop parsing entries in the following form:
    // id:runlevels:action:command...
    //

    while (TRUE) {
        LineSize = getline(&Line, &LineBufferSize, File);
        if (LineSize < 0) {
            break;
        }

        while ((LineSize != 0) && (isspace(Line[LineSize - 1]))) {
            LineSize -= 1;
        }

        String = Line;
        String[LineSize] = '\0';

        //
        // Get past whitespace.
        //

        while (isspace(*String)) {
            String += 1;
        }

        //
        // Skip any commented lines.
        //

        if ((*String == '\0') || (*String == '#')) {
            continue;
        }

        //
        // Parse out the first three fields that have colons after them.
        //

        for (Index = 0; Index < 3; Index += 1) {
            Fields[Index] = String;
            Colon = strchr(String, ':');
            if (Colon == NULL) {
                InitLog(Context,
                        INIT_LOG_SYSLOG | INIT_LOG_CONSOLE,
                        "Ignoring: %s",
                        Line);

                break;
            }

            *Colon = '\0';
            String = Colon + 1;
        }

        if (Index != 3) {
            continue;
        }

        //
        // The last field gets the rest of the line.
        //

        Fields[Index] = String;

        //
        // Figure out the action type, the third field.
        //

        for (Index = 0; Index < InitActionCount; Index += 1) {
            if (strcmp(Fields[2], InitActionTypeNames[Index]) == 0) {
                ActionType = Index;
                break;
            }
        }

        if (Index == InitActionCount) {
            InitLog(Context,
                    INIT_LOG_SYSLOG | INIT_LOG_CONSOLE,
                    "Unknown action type: %s",
                    Fields[2]);

            continue;
        }

        //
        // Figure out the runlevel mask.
        //

        RunLevelMask = 0;
        String = Fields[1];
        while (*String != '\0') {
            Index = 0;
            Character = toupper(*String);
            while (INIT_RUNLEVEL_NAMES[Index] != '\0') {
                if (Character == INIT_RUNLEVEL_NAMES[Index]) {
                    RunLevelMask |= 1 << Index;
                    break;
                }

                Index += 1;
            }

            if (INIT_RUNLEVEL_NAMES[Index] == '\0') {
                InitLog(Context,
                        INIT_LOG_SYSLOG | INIT_LOG_CONSOLE,
                        "Ignoring unknown runlevel %c",
                        Character);
            }

            String += 1;
        }

        InitCreateAction(Context,
                         Fields[0],
                         RunLevelMask,
                         ActionType,
                         Fields[3]);
    }

    Status = 0;

ParseInittabEnd:
    if (File != NULL) {
        fclose(File);
    }

    if (Line != NULL) {
        free(Line);
    }

    if (Status != 0) {
        InitLog(Context,
                INIT_LOG_SYSLOG | INIT_LOG_CONSOLE,
                "Failed to parse inittab, adding default entry: %s",
                strerror(Status));

        InitCreateAction(Context,
                         "0",
                         INIT_RUNLEVEL_MASK,
                         InitActionRespawn,
                         USER_FALLBACK_SHELL);
    }

    return;
}

VOID
InitCreateAction (
    PINIT_CONTEXT Context,
    PSTR Id,
    ULONG RunLevels,
    INIT_ACTION_TYPE ActionType,
    PSTR Command
    )

/*++

Routine Description:

    This routine creates and adds a new init action to the application context.

Arguments:

    Context - Supplies a pointer to the application context.

    Id - Supplies up to 4 characters of ID information.

    RunLevels - Supplies the mask of runlevels this action is active for.

    ActionType - Supplies the type of action.

    Command - Supplies a pointer to the command to run.

Return Value:

    None.

--*/

{

    PINIT_ACTION Action;
    size_t AllocationSize;
    PLIST_ENTRY CurrentEntry;

    if (Id == NULL) {
        Id = "";
    }

    //
    // If this is an "init default" action, just save the default run-level
    // but don't bother creating a full action.
    //

    if (ActionType == InitActionInitDefault) {
        Context->DefaultRunLevel = RunLevels;
        return;
    }

    //
    // Search for an action that exists already. Use this to avoid losing
    // running actions.
    //

    CurrentEntry = Context->ActionList.Next;
    while (CurrentEntry != &(Context->ActionList)) {
        Action = LIST_VALUE(CurrentEntry, INIT_ACTION, ListEntry);
        if ((strcmp(Id, Action->Id) == 0) &&
            (strcmp(Command, Action->Command) == 0)) {

            LIST_REMOVE(&(Action->ListEntry));
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    //
    // Allocate a new entry if one was not found.
    //

    if (CurrentEntry == &(Context->ActionList)) {
        AllocationSize = sizeof(INIT_ACTION) + strlen(Command) + 1;
        Action = malloc(AllocationSize);
        if (Action == NULL) {
            return;
        }

        memset(Action, 0, AllocationSize);
        Action->Command = (PSTR)(Action + 1);
    }

    strncpy(Action->Id, Id, sizeof(Action->Id) - 1);
    strcpy(Action->Command, Command);
    Action->Type = ActionType;
    Action->RunLevels = RunLevels;
    INSERT_BEFORE(&(Action->ListEntry), &(Context->ActionList));
    InitLog(Context,
            INIT_LOG_DEBUG,
            "New Action: %s:%x:%s:%s",
            Action->Id,
            Action->RunLevels,
            InitActionTypeNames[Action->Type],
            Action->Command);

    return;
}

VOID
InitRunActions (
    PINIT_CONTEXT Context,
    INIT_ACTION_TYPE ActionType,
    ULONG RunLevelMask
    )

/*++

Routine Description:

    This routine runs all actions with a given action type that have a bit set
    in the given runlevel mask.

Arguments:

    Context - Supplies a pointer to the application context.

    ActionType - Supplies the action type to filter.

    RunLevelMask - Supplies the run-level mask to filter.

Return Value:

    None.

--*/

{

    PINIT_ACTION Action;
    PLIST_ENTRY CurrentEntry;

    CurrentEntry = Context->ActionList.Next;
    while (CurrentEntry != &(Context->ActionList)) {
        Action = LIST_VALUE(CurrentEntry, INIT_ACTION, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (Action->Type != ActionType) {
            continue;
        }

        if ((RunLevelMask != 0) && ((Action->RunLevels & RunLevelMask) == 0)) {
            continue;
        }

        //
        // For respawn actions, only run them if they're not already running.
        //

        if (ActionType == InitActionRespawn) {
            if (Action->ProcessId <= 0) {
                Action->ProcessId = InitRunAction(Context, Action);
            }

        } else {
            Action->ProcessId = InitRunAction(Context, Action);
            if ((ActionType == InitActionSysinit) ||
                (ActionType == InitActionWait) ||
                (ActionType == InitActionOnce) ||
                (ActionType == InitActionCtrlAltDel) ||
                (ActionType == InitActionShutdown)) {

                InitWaitForProcess(Context, Action->ProcessId);
            }
        }
    }

    return;
}

pid_t
InitRunAction (
    PINIT_CONTEXT Context,
    PINIT_ACTION Action
    )

/*++

Routine Description:

    This routine fires up the given action.

Arguments:

    Context - Supplies a pointer to the application context.

    Action - Supplies a pointer to the action to run.

Return Value:

    Returns the process ID of the newly running process.

--*/

{

    int Flags;
    pid_t ProcessId;

    ProcessId = fork();
    if (ProcessId < 0) {
        InitLog(Context,
                INIT_LOG_CONSOLE | INIT_LOG_SYSLOG,
                "Failed to fork: %s",
                strerror(errno));

        return 0;
    }

    //
    // If this is the parent, just walk right back out with the new process ID
    // in hand.
    //

    if (ProcessId > 0) {
        return ProcessId;
    }

    ProcessId = getpid();

    //
    // Put signals back to their standard configuration.
    //

    InitResetSignalHandlers();

    //
    // Create a new session and process group.
    //

    if (setsid() < 0) {
        InitLog(Context,
                INIT_LOG_CONSOLE | INIT_LOG_SYSLOG,
                "Failed to setsid: %s",
                strerror(errno));

        exit(-1);
    }

    //
    // For certain types of entries, force the console to be the controlling
    // terminal.
    //

    if ((Action->Type == InitActionSysinit) ||
        (Action->Type == InitActionBootWait) ||
        (Action->Type == InitActionWait)) {

        ioctl(STDIN_FILENO, TIOCSCTTY, 1);
        Flags = fcntl(STDIN_FILENO, F_GETFL);
        if (Flags != -1) {
            Flags &= ~O_NONBLOCK;
            fcntl(STDIN_FILENO, F_SETFL, Flags);
        }
    }

    InitLog(Context,
            INIT_LOG_SYSLOG,
            "Starting ID %s, PID %d: %s",
            Action->Id,
            ProcessId,
            Action->Command);

    InitAddUtmpEntry(Action);
    InitExec(Context, Action->Command);
    _exit(-1);
}

VOID
InitExec (
    PINIT_CONTEXT Context,
    PSTR Command
    )

/*++

Routine Description:

    This routine executes the given command.

Arguments:

    Context - Supplies a pointer to the application context.

    Command - Supplies a pointer to the command to execute.

Return Value:

    None.

--*/

{

    PSTR *Array;
    UINTN ArrayCount;
    UINTN CommandLength;
    BOOL HasDash;
    UINTN Index;
    BOOL WasBlank;

    HasDash = FALSE;
    if (Command[0] == '-') {
        HasDash = TRUE;
        Command += 1;
    }

    CommandLength = strlen(Command);

    //
    // If there is anything weird in the command, let the shell navigate it.
    // The login shell define has a leading dash in front of it.
    //

    if (strpbrk(Command, "~`!$^&*()=\\|[]{};'\"<>?") != NULL) {
        Array = alloca(sizeof(PSTR) * 5);
        if (HasDash != FALSE) {
            Array[0] = USER_DEFAULT_LOGIN_SHELL;

        } else {
            Array[0] = USER_DEFAULT_LOGIN_SHELL + 1;
        }

        Array[1] = "-c";
        Array[2] = alloca(CommandLength + 6);
        snprintf(Array[2], CommandLength + 6, "exec %s", Command);
        Array[3] = NULL;
        Command = USER_DEFAULT_LOGIN_SHELL + 1;

    } else {
        ArrayCount = (CommandLength / 2) + 2;
        Array = alloca(sizeof(PSTR) * ArrayCount);
        Index = 0;
        WasBlank = TRUE;
        while (*Command != '\0') {

            //
            // The previous character was blank. If this one is too, keep
            // going, otherwise mark a new argument.
            //

            if (WasBlank != FALSE) {
                if (!isblank(*Command)) {
                    Array[Index] = Command;
                    Index += 1;
                    WasBlank = FALSE;
                }

            //
            // The previous character was not blank. If it becomes blank,
            // null out this blank character to delimit the previous argument.
            //

            } else {
                if (isblank(*Command)) {
                    WasBlank = TRUE;
                    *Command = '\0';
                }
            }

            Command += 1;
        }

        Array[Index] = NULL;

        assert(Index < ArrayCount);
    }

    //
    // If there's a dash, then this is an interactive session. Attempt to set
    // the controlling terminal if it's not already set. Don't be forceful
    // though.
    //

    if (HasDash != FALSE) {
        ioctl(STDIN_FILENO, TIOCSCTTY, 0);
    }

    execve(Array[0], Array, environ);
    InitLog(Context,
            INIT_LOG_SYSLOG | INIT_LOG_CONSOLE,
            "Failed to exec %s: %s",
            Array[0],
            strerror(errno));

    return;
}

VOID
InitWaitForProcess (
    PINIT_CONTEXT Context,
    pid_t ProcessId
    )

/*++

Routine Description:

    This routine waits for a specific process to complete.

Arguments:

    Context - Supplies a pointer to the application context.

    ProcessId - Supplies the process ID to wait for.

Return Value:

    None.

--*/

{

    pid_t DeadProcess;
    int Status;

    if (ProcessId <= 0) {
        return;
    }

    while (TRUE) {
        DeadProcess = wait(&Status);
        InitMarkProcessTerminated(Context, DeadProcess, Status);
        if (DeadProcess == ProcessId) {
            break;
        }
    }

    return;
}

PINIT_ACTION
InitMarkProcessTerminated (
    PINIT_CONTEXT Context,
    pid_t ProcessId,
    int Status
    )

/*++

Routine Description:

    This routine cleans up after a dead process.

Arguments:

    Context - Supplies a pointer to the application context.

    ProcessId - Supplies the process ID that ended.

    Status - Supplies the exit status of the process.

Return Value:

    Returns a pointer to the action associated with the process ID if it's
    one of init's processes.

    NULL if the process is not a tracked process.

--*/

{

    PINIT_ACTION Action;
    PLIST_ENTRY CurrentEntry;
    PINIT_ACTION FoundAction;

    FoundAction = NULL;
    if (ProcessId > 0) {
        SwUpdateUtmp(ProcessId, DEAD_PROCESS, NULL, NULL, NULL);
        CurrentEntry = Context->ActionList.Next;
        while (CurrentEntry != &(Context->ActionList)) {
            Action = LIST_VALUE(CurrentEntry, INIT_ACTION, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            if (Action->ProcessId == ProcessId) {
                Action->ProcessId = 0;
                FoundAction = Action;
                break;
            }
        }
    }

    if ((FoundAction != NULL) && (Action->Type == InitActionRespawn)) {
        InitLog(Context,
                INIT_LOG_DEBUG | INIT_LOG_SYSLOG,
                "Process '%s' (pid %d) exited with status %d. Scheduling for "
                "restart",
                FoundAction->Command,
                ProcessId,
                Status);

    } else {
        InitLog(Context,
                INIT_LOG_DEBUG,
                "Process id %d exited with status %d.",
                ProcessId,
                Status);
    }

    return FoundAction;
}

VOID
InitResetSignalHandlers (
    VOID
    )

/*++

Routine Description:

    This routine resets signal handlers back to their default values.

Arguments:

    None.

Return Value:

    None.

--*/

{

    struct sigaction SignalAction;

    memset(&SignalAction, 0, sizeof(SignalAction));
    sigemptyset(&(SignalAction.sa_mask));
    SignalAction.sa_handler = SIG_DFL;
    sigaction(SIGTSTP, &SignalAction, NULL);
    sigaction(SIGINT, &SignalAction, NULL);
    sigaction(SIGQUIT, &SignalAction, NULL);
    sigaction(SIGUSR1, &SignalAction, NULL);
    sigaction(SIGUSR2, &SignalAction, NULL);
    sigaction(SIGTERM, &SignalAction, NULL);
    sigaction(SIGHUP, &SignalAction, NULL);
    sigprocmask(SIG_SETMASK, &(SignalAction.sa_mask), NULL);
    return;
}

VOID
InitLog (
    PINIT_CONTEXT Context,
    ULONG Destination,
    PSTR Format,
    ...
    )

/*++

Routine Description:

    This routine prints a message to the system log, console, or both.

Arguments:

    Context - Supplies a pointer to the application context.

    Destination - Supplies the bitfield of destinations the message should be
        printed to. See INIT_LOG_* definitions.

    Format - Supplies the printf-style format of the message.

    ... - Supplies the additional arguments dictated by the format.

Return Value:

    None.

--*/

{

    va_list ArgumentList;

    if ((Destination & INIT_LOG_DEBUG) != 0) {
        if ((Context->Options & INIT_OPTION_DEBUG) != 0) {
            Destination |= INIT_LOG_SYSLOG | INIT_LOG_CONSOLE;
        }
    }

    if ((Destination & INIT_LOG_SYSLOG) != 0) {
        if (Context->SyslogOpen == FALSE) {
            openlog("init", 0, LOG_DAEMON);
            Context->SyslogOpen = TRUE;
        }

        va_start(ArgumentList, Format);
        vsyslog(LOG_INFO, Format, ArgumentList);
        va_end(ArgumentList);
    }

    if ((Destination & INIT_LOG_CONSOLE) != 0) {
        va_start(ArgumentList, Format);
        vfprintf(stderr, Format, ArgumentList);
        va_end(ArgumentList);
        fprintf(stderr, "\n");
    }

    return;
}

void
InitSignalHandler (
    int Signal
    )

/*++

Routine Description:

    This routine is called when a signal fires. It simply records that the
    signal occurred.

Arguments:

    Signal - Supplies the signal that fired.

Return Value:

    None.

--*/

{

    assert(Signal < NSIG);

    //
    // Mark that a signal was seen in slot 0, and then increment the count for
    // the particular signal.
    //

    InitSignalCounts[0] = 1;
    InitSignalCounts[Signal] += 1;
    return;
}

void
InitAddUtmpEntry (
    PINIT_ACTION Action
    )

/*++

Routine Description:

    This routine adds an init utmp entry for the process about to be launched.

Arguments:

    Action - Supplies a pointer to the action being launched.

Return Value:

    None.

--*/

{

    struct utmpx Entry;
    char *Terminal;
    struct timeval Time;

    memset(&Entry, 0, sizeof(Entry));
    Entry.ut_type = INIT_PROCESS;
    Entry.ut_pid = getpid();
    Terminal = ttyname(STDIN_FILENO);
    if (Terminal != NULL) {
        strncpy(Entry.ut_line, Terminal, sizeof(Entry.ut_line));
    }

    strncpy(Entry.ut_id, Action->Id, sizeof(Entry.ut_id));

    //
    // Manually set the time members in case this is a 64 bit system doing some
    // sort of weird 32-bit time_t compatibility thing.
    //

    gettimeofday(&Time, NULL);
    Entry.ut_tv.tv_sec = Time.tv_sec;
    Entry.ut_tv.tv_usec = Time.tv_usec;
    setutxent();
    pututxline(&Entry);
    return;
}

