/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    debug.c

Abstract:

    This module implements the debugging client.

Author:

    Evan Green 2-Jul-2012

Environment:

    Debug Client

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/debug/spproto.h>
#include <minoca/lib/im.h>
#include <minoca/debug/dbgext.h>
#include "symbols.h"
#include "dbgapi.h"
#include "dbgrprof.h"
#include "console.h"
#include "userdbg.h"
#include "dbgrcomm.h"
#include "extsp.h"
#include "consio.h"
#include "remsrv.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

//
// ---------------------------------------------------------------- Definitions
//

#define DEBUGGER_VERSION_MAJOR 1
#define DEBUGGER_VERSION_MINOR 1

#define DEBUGGER_COMMAND_BUFFER_SIZE 10000
#define DEBUGGER_COMMAND_HISTORY_SIZE 50

#define DEBUGGER_DEFAULT_BAUD_RATE 115200

//
// Define the program usage.
//

#define DEBUGGER_USAGE                                                         \
    "Usage: debug [-i] [-s <path>...] [-e <path>...] "                         \
    "[-k <connection>] [-b <baud_rate>] [-r remote:port] \n"                   \
    "[-- <child_parameters...>]\n\n"        \
    "The Minoca debugger facilitates debugging, tracing, and profiling of \n"  \
    "user mode programs and remote kernels. Options are:\n"                    \
    "  -b, --baud-rate=<baud_rate> -- Specify the baud rate for kernel \n"     \
    "      serial port connections. If not specified, the default is \n"       \
    "      115200bps.\n"                                                       \
    "  -i, --initial-break -- Request an initial breakpoint upon connection.\n"\
    "  -e, --extension=<path> -- Load the debugger extension at the given \n"  \
    "      path. This can also be done at runtime using the load command.\n"   \
    "  -k, --kernel=<connection> -- Connect to a kernel on another maching \n" \
    "      using the given connection string. Connections can be named \n"     \
    "      pipes like '\\\\.\\pipe\\mypipe' or can be serial ports like \n"    \
    "      'COM1'.\n"                                                          \
    "  -r, --remote=<address:port> -- Connect to a remote debug server \n"     \
    "      using the given form. IPv6 addresses should be enclosed in \n"      \
    "      [square] brackets to disambiguate the colon separating the \n"      \
    "      address from the port.\n"                                           \
    "  -R, --reverse-remote=<address:port> -- Connect to a remote debug \n"    \
    "      server by opening up a port and waiting for an incoming \n"         \
    "      connection. This is useful when the debug server cannot accept \n"  \
    "      incoming connections.\n"                                            \
    "  -s, --symbol-path=<path> -- Add the given path to the symbol search \n" \
    "      path. This option can be specified multiple times, or the path \n"  \
    "      argument can be semicolon-delimited list of paths.\n"               \
    "  -S, --source-path=<prefix=path> -- Add the given path to the source \n" \
    "      search path. If the optional prefix matches a symbol source \n"     \
    "      path, it will be stripped off and replaced with the path. \n"       \
    "  --help -- Display this help text and exit.\n"                           \
    "  --version -- Display the application and kernel protocol version and \n"\
    "      exit.\n"                                                            \
    "  child_parameters -- Specifies the program name and subsequent \n"       \
    "      arguments of the child process to launch and attach to. \n"         \
    "      Debugging a child process is incompatible with the -k option.\n\n"

#define DEBUG_SHORT_OPTIONS "b:e:ik:r:R:s:S:"

//
// -------------------------------------------------------------------- Globals
//

struct option DbgrLongOptions[] = {
    {"baud-rate", required_argument, 0, 'b'},
    {"extension", required_argument, 0, 'e'},
    {"initial-break", no_argument, 0, 'i'},
    {"kernel", required_argument, 0, 'k'},
    {"remote", required_argument, 0, 'r'},
    {"reverse-remote", required_argument, 0, 'R'},
    {"symbol-path", required_argument, 0, 's'},
    {"source-path", required_argument, 0, 'S'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
DbgrpPrintCommandPrompt (
    PDEBUGGER_CONTEXT Context
    );

BOOL
DbgrpSplitCommandArguments (
    PSTR Input,
    PSTR Arguments[DEBUGGER_MAX_COMMAND_ARGUMENTS],
    PULONG ArgumentCount
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ------------------------------------------------------------------ Functions
//

INT
DbgrMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the debugger.

Arguments:

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies an array of strings representing the arguments.

Return Value:

    Returns 0 on success, nonzero on failure.

--*/

{

    PSTR AfterScan;
    ULONG ArgumentIndex;
    ULONG BaudRate;
    PSTR Channel;
    PSTR Command;
    ULONG CommandArgumentCount;
    PSTR CommandArguments[DEBUGGER_MAX_COMMAND_ARGUMENTS];
    PDEBUGGER_COMMAND_ENTRY CommandEntry;
    DEBUG_CONNECTION_TYPE ConnectionType;
    DEBUGGER_CONTEXT Context;
    BOOL ExtensionsInitialized;
    ULONG HistoryIndex;
    INT Option;
    ULONG PathIndex;
    PSTR RemoteAddress;
    INT Result;
    INT ReturnValue;
    BOOL ReverseRemote;
    ULONG TargetArgumentCount;
    PSTR *TargetArguments;

    BaudRate = DEBUGGER_DEFAULT_BAUD_RATE;
    ConnectionType = DebugConnectionInvalid;
    Channel = NULL;
    ExtensionsInitialized = FALSE;
    RemoteAddress = NULL;
    ReverseRemote = FALSE;
    TargetArguments = NULL;
    TargetArgumentCount = 0;
    srand(time(NULL));
    memset(&Context, 0, sizeof(DEBUGGER_CONTEXT));
    Context.TargetFlags = DEBUGGER_TARGET_RUNNING;
    Context.Server.Socket = -1;
    Context.Client.Socket = -1;
    INITIALIZE_LIST_HEAD(&(Context.Server.ClientList));
    INITIALIZE_LIST_HEAD(&(Context.StandardIn.RemoteCommandList));
    INITIALIZE_LIST_HEAD(&(Context.SourcePathList));
    Context.CommandHistory =
                          malloc(sizeof(PSTR) * DEBUGGER_COMMAND_HISTORY_SIZE);

    if (Context.CommandHistory == NULL) {
        Result = ENOMEM;
        goto MainEnd;
    }

    memset(Context.CommandHistory,
           0,
           sizeof(PSTR) * DEBUGGER_COMMAND_HISTORY_SIZE);

    Context.CommandHistorySize = DEBUGGER_COMMAND_HISTORY_SIZE;
    Context.CommandBufferSize = DEBUGGER_COMMAND_BUFFER_SIZE;
    Context.CommandBuffer = malloc(Context.CommandBufferSize);
    if (Context.CommandBuffer == NULL) {
        Result = ENOMEM;
        goto MainEnd;
    }

    Context.CommandBuffer[0] = '\0';

    //
    // Initialize the console layer.
    //

    Result = DbgrInitializeConsoleIo(&Context);
    if (Result != 0) {
        goto MainEnd;
    }

    //
    // Initialize extensions.
    //

    Result = DbgInitializeExtensions(&Context);
    ExtensionsInitialized = TRUE;
    if (Result != 0) {
        goto MainEnd;
    }

    DbgOut("Minoca debugger version %d.%d. Protocol version %d.%d.\n",
           DEBUGGER_VERSION_MAJOR,
           DEBUGGER_VERSION_MINOR,
           DEBUG_PROTOCOL_MAJOR_VERSION,
           DEBUG_PROTOCOL_REVISION);

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             DEBUG_SHORT_OPTIONS,
                             DbgrLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Result = EINVAL;
            goto MainEnd;
        }

        switch (Option) {
        case 'b':
            BaudRate = strtoul(optarg, &AfterScan, 0);
            if (AfterScan == optarg) {
                DbgOut("Error: Invalid baud rate '%s'.\n", optarg);
                Result = EINVAL;
                goto MainEnd;
            }

            break;

        case 'e':
            Result = DbgLoadExtension(&Context, optarg);
            if (Result != 0) {
                DbgOut("Failed to load extension '%s'.\n", optarg);
                goto MainEnd;
            }

            break;

        case 'i':
            Context.Flags |= DEBUGGER_FLAG_INITIAL_BREAK;
            break;

        case 'k':
            if (ConnectionType != DebugConnectionInvalid) {
                DbgOut("Error: -k conflicts with a previous argument that "
                       "defines the debugger connection type.\n");

                Result = EINVAL;
                goto MainEnd;
            }

            Channel = optarg;
            ConnectionType = DebugConnectionKernel;
            break;

        case 'R':
            ReverseRemote = TRUE;

            //
            // Fall through.
            //

        case 'r':
            if (ConnectionType != DebugConnectionInvalid) {
                DbgOut("Error: -r conflicts with a previously specified "
                       "connection type.\n");

                Result = EINVAL;
                goto MainEnd;
            }

            ConnectionType = DebugConnectionRemote;
            RemoteAddress = optarg;
            break;

        case 's':
            Result = DbgrSetSymbolPath(&Context, optarg, TRUE);
            if (Result != 0) {
                DbgOut("Failed to set initial symbol path.\n");
                goto MainEnd;
            }

            break;

        case 'S':
            Result = DbgrpAddSourcePath(&Context, optarg);
            if (Result != 0) {
                DbgOut("Failed to add source path %s.\n", optarg);
                goto MainEnd;
            }

            break;

        case 'V':

            //
            // The version information was already printed above.
            //

            return 1;

        case 'h':
            DbgOut(DEBUGGER_USAGE);
            return 1;

        default:

            assert(FALSE);

            Result = EINVAL;
            goto MainEnd;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    //
    // Any additional arguments imply a usermode debugger. If kernel parameters
    // were supplied then this is an invalid configuration.
    //

    if (ArgumentIndex < ArgumentCount) {
        if (ConnectionType != DebugConnectionInvalid) {
            DbgOut("Error: Additional command line arguments imply a user "
                   "mode debugger, but an alternate form (such as a kernel "
                   "connection parameter) was specified in the arguments.\n");

            Result = EINVAL;
            goto MainEnd;
        }

        TargetArguments = &(Arguments[ArgumentIndex]);
        TargetArgumentCount = ArgumentCount - ArgumentIndex;
        ConnectionType = DebugConnectionUser;
    }

    //
    // Chide the user and exit if there's nothing valid to do.
    //

    if (ConnectionType == DebugConnectionInvalid) {
        DbgOut(DEBUGGER_USAGE);
        Result = FALSE;
        goto MainEnd;
    }

    Result = DbgrInitialize(&Context, ConnectionType);
    if (Result != 0) {
        goto MainEnd;
    }

    //
    // For kernel debugging, set up the communications channel.
    //

    if (ConnectionType == DebugConnectionKernel) {
        Result = InitializeCommunications(Channel, BaudRate);
        if (Result == FALSE) {
            DbgOut("Unable to setup communcations using %s\n", Channel);
            Result = EINVAL;
            goto MainEnd;
        }

        //
        // Connect to the target.
        //

        Result = DbgrConnect(&Context);
        if (Result != 0) {
            DbgOut("Unable to connect to target!\n");
            goto MainEnd;
        }

    //
    // For user mode debugging, set up the child process.
    //

    } else if (ConnectionType == DebugConnectionUser) {

        assert(TargetArguments != NULL);

        //
        // Anything with an equals sign at the beginning gets set as an
        // environment variable.
        //

        while (TargetArgumentCount != 0) {
            if (strchr(TargetArguments[0], '=') != NULL) {
                DbgOut("Setting environment variable: %s\n",
                       TargetArguments[0]);

                putenv(TargetArguments[0]);
                TargetArguments += 1;
                TargetArgumentCount -= 1;

            } else {
                break;
            }
        }

        if (TargetArgumentCount == 0) {
            DbgOut("Error: No command to launch!\n");
            Result = -1;
            goto MainEnd;
        }

        DbgOut("Launching: ");
        for (ArgumentIndex = 0;
             ArgumentIndex < TargetArgumentCount;
             ArgumentIndex += 1) {

            DbgOut("%s ", TargetArguments[ArgumentIndex]);
        }

        DbgOut("\n");
        Result = LaunchChildProcess(TargetArgumentCount, TargetArguments);
        if (Result == FALSE) {
            DbgOut("Error: Failed to launch target process \"%s\".\n",
                   TargetArguments[0]);

            Result = EINVAL;
            goto MainEnd;
        }

    } else {

        assert(ConnectionType == DebugConnectionRemote);

        Result = DbgrClientMainLoop(&Context, RemoteAddress, ReverseRemote);
        goto MainEnd;
    }

    //
    // Loop breaking in and waiting for the target.
    //

    while ((Context.Flags & DEBUGGER_FLAG_EXITING) == 0) {

        //
        // Loop waiting for the target to break in.
        //

        while ((Context.TargetFlags & DEBUGGER_TARGET_RUNNING) != 0) {

            //
            // Acquire the standard out lock to synchronize with remote threads
            // trying to send updated source information.
            //

            AcquireDebuggerLock(Context.StandardOut.Lock);
            DbgrUnhighlightCurrentLine(&Context);
            ReleaseDebuggerLock(Context.StandardOut.Lock);
            Result = DbgrWaitForEvent(&Context);
            if (Result != 0) {
                DbgOut("Error getting data from target!\n");
                goto MainEnd;
            }
        }

        //
        // Process a command from the user.
        //

        DbgrpPrintCommandPrompt(&Context);
        Result = DbgrGetCommand(&Context);
        UiEnableCommands(FALSE);
        if (Result == FALSE) {
            DbgOut("Failed to get command.\n");
            Result = EINVAL;
            goto MainEnd;
        }

        if (Context.CommandBuffer[0] == '\0') {
            continue;
        }

        Result = DbgrpSplitCommandArguments(Context.CommandBuffer,
                                            CommandArguments,
                                            &CommandArgumentCount);

        if (Result == FALSE) {
            Result = EINVAL;
            goto MainEnd;
        }

        Command = CommandArguments[0];
        CommandEntry = DbgrLookupCommand(Command);
        if (CommandEntry == NULL) {
            DbgOut("Error: Unrecognized command \"%s\"\n", Command);
            continue;
        }

        //
        // Run the command.
        //

        CommandEntry->CommandRoutine(&Context,
                                     CommandArguments,
                                     CommandArgumentCount);

        DbgrpSetPromptText(&Context, NULL);
    }

    Result = 0;

MainEnd:
    DbgrDestroy(&Context, ConnectionType);
    if (ExtensionsInitialized != FALSE) {
        DbgUnloadAllExtensions(&Context);
    }

    DestroyCommunications();
    if (Context.SymbolPath != NULL) {
        for (PathIndex = 0;
             PathIndex < Context.SymbolPathCount;
             PathIndex += 1) {

            if (Context.SymbolPath[PathIndex] != NULL) {
                free(Context.SymbolPath[PathIndex]);
            }
        }

        free(Context.SymbolPath);
    }

    ReturnValue = 0;
    if (Result != 0) {
        DbgOut("*** Session Ended ***\n");
        DbgrOsPrepareToReadInput();
        DbgrOsGetCharacter(NULL, NULL);
        DbgrOsPostInputCallback();
        ReturnValue = 1;
    }

    DbgrpServerDestroy(&Context);
    DbgrDestroyConsoleIo(&Context);
    for (HistoryIndex = 0;
         HistoryIndex < Context.CommandHistorySize;
         HistoryIndex += 1) {

        if (Context.CommandHistory[HistoryIndex] != NULL) {
            free(Context.CommandHistory[HistoryIndex]);
        }
    }

    if (Context.CommandHistory != NULL) {
        free(Context.CommandHistory);
    }

    if (Context.CommandBuffer != NULL) {
        free(Context.CommandBuffer);
    }

    if (Context.SourceFile.Path != NULL) {
        free(Context.SourceFile.Path);
    }

    if (Context.SourceFile.ActualPath != NULL) {
        free(Context.SourceFile.ActualPath);
    }

    if (Context.SourceFile.Contents != NULL) {
        free(Context.SourceFile.Contents);
    }

    DbgrpDestroyAllSourcePaths(&Context);
    exit(ReturnValue);
    return ReturnValue;
}

BOOL
DbgrGetCommand (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine retrieves a command from the user or a remote client.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    Returns TRUE on success, FALSE on failure.

--*/

{

    ULONG BufferSize;
    PSTR CommandBuffer;
    ULONG CommandLength;
    UCHAR ControlKey;
    BOOL Done;
    PSTR *History;
    ULONG HistoryIndex;
    ULONG HistoryNextIndex;
    LONG HistoryOffset;
    ULONG HistorySize;
    UCHAR Key;
    LONG NextHistoryOffset;
    PSTR PreviousCommand;
    PDEBUGGER_REMOTE_COMMAND RemoteCommand;
    BOOL Result;

    CommandBuffer = Context->CommandBuffer;
    BufferSize = Context->CommandBufferSize;

    assert((CommandBuffer != NULL) && (BufferSize != 0));

    History = Context->CommandHistory;
    HistoryOffset = 0;
    HistorySize = Context->CommandHistorySize;
    HistoryNextIndex = Context->CommandHistoryNextIndex;
    if (HistoryNextIndex == 0) {
        PreviousCommand = History[HistorySize - 1];

    } else {
        PreviousCommand = History[HistoryNextIndex - 1];
    }

    DbgrOsPrepareToReadInput();
    CommandLength = 0;
    Done = FALSE;
    while (Done == FALSE) {

        //
        // Retrieve one key.
        //

        Result = DbgrOsGetCharacter(&Key, &ControlKey);
        if (Result == FALSE) {
            goto GetCommandEnd;
        }

        //
        // Process non-printing keys.
        //

        if (Key == 0) {
            switch (ControlKey) {

            //
            // Enter signals the completion of a command.
            //

            case KEY_RETURN:
                Done = TRUE;
                break;

            //
            // Escape deletes everything on the current line.
            //

            case KEY_ESCAPE:
                UiSetCommandText("");
                CommandLength = 0;
                break;

            //
            // Up and down get recently entered commands.
            //

            case KEY_DOWN:
            case KEY_UP:
                NextHistoryOffset = HistoryOffset;
                if (ControlKey == KEY_UP) {
                    if (HistoryOffset + 1 < HistorySize) {
                        NextHistoryOffset = HistoryOffset + 1;
                    }

                } else {
                    if (HistoryOffset > 0) {
                        NextHistoryOffset = HistoryOffset - 1;
                    }
                }

                if (NextHistoryOffset > HistoryNextIndex) {
                    HistoryIndex = HistoryNextIndex + HistorySize -
                                   NextHistoryOffset;

                } else {
                    HistoryIndex = HistoryNextIndex - NextHistoryOffset;
                }

                if (History[HistoryIndex] != NULL) {
                    UiSetCommandText(History[HistoryIndex]);
                    CommandLength = 0;
                    HistoryOffset = NextHistoryOffset;
                }

                break;

            //
            // Check for a remote command.
            //

            case KEY_REMOTE:
                RemoteCommand = NULL;
                AcquireDebuggerLock(Context->StandardIn.Lock);
                if (LIST_EMPTY(&(Context->StandardIn.RemoteCommandList)) ==
                    FALSE) {

                    RemoteCommand = LIST_VALUE(
                                    Context->StandardIn.RemoteCommandList.Next,
                                    DEBUGGER_REMOTE_COMMAND,
                                    ListEntry);

                    LIST_REMOVE(&(RemoteCommand->ListEntry));
                }

                ReleaseDebuggerLock(Context->StandardIn.Lock);
                if (RemoteCommand != NULL) {
                    strncpy(CommandBuffer, RemoteCommand->Command, BufferSize);
                    CommandBuffer[BufferSize - 1] = '\0';
                    DbgOut("%s\t\t[%s@%s]\n",
                           CommandBuffer,
                           RemoteCommand->User,
                           RemoteCommand->Host);

                    free(RemoteCommand->Command);
                    free(RemoteCommand->User);
                    free(RemoteCommand->Host);
                    free(RemoteCommand);
                    goto GetCommandEnd;
                }

                break;

            default:
                break;
            }

        } else {

            //
            // Copy the key into the current buffer position.
            //

            CommandBuffer[CommandLength] = Key;
            CommandLength += 1;
            if ((Context->Flags & DEBUGGER_FLAG_ECHO_COMMANDS) != 0) {
                DbgOut("%c", Key);
            }

            if (CommandLength + 1 >= BufferSize) {
                Done = TRUE;
            }
        }
    }

    DbgrOsPostInputCallback();
    CommandBuffer[CommandLength] = '\0';

    //
    // If the command was not empty, copy it into the history as the most
    // recent entry.
    //

    if (CommandLength != 0) {
        if ((PreviousCommand == NULL) ||
            (strcmp(CommandBuffer, PreviousCommand) != 0)) {

            if (History[HistoryNextIndex] != NULL) {
                free(History[HistoryNextIndex]);
            }

            History[HistoryNextIndex] = strdup(CommandBuffer);
            HistoryNextIndex += 1;
            if (HistoryNextIndex == HistorySize) {
                HistoryNextIndex = 0;
            }
        }

    //
    // If the command was empty, repeat the most recent command.
    //

    } else {
        *CommandBuffer = '\0';
        if (PreviousCommand != NULL) {
            strcpy(CommandBuffer, PreviousCommand);
        }

        if (CommandLength == 0) {
            DbgOut(CommandBuffer);
        }

        if ((Context->Flags & DEBUGGER_FLAG_ECHO_COMMANDS) == 0) {
            DbgOut("\n");
        }
    }

    if ((Context->Flags & DEBUGGER_FLAG_ECHO_COMMANDS) != 0) {
        DbgOut("\n");
    }

    Result = TRUE;

GetCommandEnd:
    Context->CommandHistoryNextIndex = HistoryNextIndex;
    return Result;
}

VOID
DbgrpSetPromptText (
    PDEBUGGER_CONTEXT Context,
    PSTR Prompt
    )

/*++

Routine Description:

    This routine sets the command prompt to the given string.

Arguments:

    Context - Supplies a pointer to the applicaton context.

    Prompt - Supplies a pointer to the null terminated string containing the
        prompt to set.

Return Value:

    None.

--*/

{

    AcquireDebuggerLock(Context->StandardOut.Lock);
    if (Context->StandardOut.Prompt != NULL) {
        free(Context->StandardOut.Prompt);
        Context->StandardOut.Prompt = NULL;
    }

    if (Prompt != NULL) {
        Context->StandardOut.Prompt = strdup(Prompt);
        UiSetPromptText(Prompt);

    } else {
        UiSetPromptText("");
    }

    DbgrpServerNotifyClients(Context);
    ReleaseDebuggerLock(Context->StandardOut.Lock);
    return;
}

BOOL
DbgrpSplitCommandArguments (
    PSTR Input,
    PSTR Arguments[DEBUGGER_MAX_COMMAND_ARGUMENTS],
    PULONG ArgumentCount
    )

/*++

Routine Description:

    This routine splits a command line into its arguments.

Arguments:

    Input - Supplies a pointer to the input command line buffer.

    Arguments - Supplies a pointer to an array of strings that will receive any
        additional arguments.

    ArgumentCount - Supplies a pointer where the count of arguments will be
        returned.

Return Value:

    Returns TRUE on success, FALSE on failure.

--*/

{

    ULONG ArgumentIndex;
    BOOL Result;

    Result = TRUE;
    ArgumentIndex = 0;
    while (TRUE) {

        //
        // Remove spaces before the argument.
        //

        while (isspace(*Input) != 0) {
            Input += 1;
        }

        if (*Input == '\0') {
            break;
        }

        if (ArgumentIndex < DEBUGGER_MAX_COMMAND_ARGUMENTS) {
            Arguments[ArgumentIndex] = Input;
            ArgumentIndex += 1;
        }

        //
        // Loop until there's a space.
        //

        while ((*Input != '\0') && (isspace(*Input) == 0)) {
            Input += 1;
        }

        //
        // If it's a terminator, stop. Otherwise, terminate the argument and
        // keep moving.
        //

        if (*Input == '\0') {
            break;

        } else {
            *Input = '\0';
            Input += 1;
        }
    }

    *ArgumentCount = ArgumentIndex;
    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
DbgrpPrintCommandPrompt (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine prints the debugger command prompt, indicating to the user to
    enter a command.

Arguments:

    Context - Supplies a pointer to the debugger context.

Return Value:

    None.

--*/

{

    PBREAK_NOTIFICATION Break;
    CHAR Prompt[32];

    assert(Context->CurrentEvent.Type == DebuggerEventBreak);

    Break = &(Context->CurrentEvent.BreakNotification);
    if (Context->ConnectionType == DebugConnectionKernel) {
        if (Break->ProcessorOrThreadCount > 1) {
            sprintf(Prompt,
                    "%d : kd>",
                    Break->ProcessorOrThreadNumber);

        } else {
            sprintf(Prompt, "kd>");
        }

    } else {

        assert(Context->ConnectionType == DebugConnectionUser);

        sprintf(Prompt,
                "%x:%x>",
                Break->Process,
                Break->ProcessorOrThreadNumber);
    }

    DbgrpSetPromptText(Context, Prompt);
    DbgOut(Prompt);
    UiSetCommandText("");
    UiEnableCommands(TRUE);
    return;
}

