/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    exec.c

Abstract:

    This module handles execution of commands for the shell.

Author:

    Evan Green 6-Jun-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "sh.h"
#include "shparse.h"
#include "../swiss.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../swlib.h"

//
// --------------------------------------------------------------------- Macros
//

//
// Define the file creation mask for new files created by I/O redirection.
//

#define SHELL_FILE_CREATION_MASK 0664

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
ShExecuteNode (
    PSHELL Shell,
    PSHELL_NODE Node
    );

BOOL
ShExecuteAsynchronousNode (
    PSHELL Shell,
    PSHELL_NODE Node
    );

BOOL
ShExecuteList (
    PSHELL Shell,
    PSHELL_EXECUTION_NODE ExecutionNode
    );

BOOL
ShExecuteAndOr (
    PSHELL Shell,
    PSHELL_EXECUTION_NODE ExecutionNode
    );

BOOL
ShExecutePipeline (
    PSHELL Shell,
    PSHELL_EXECUTION_NODE ExecutionNode
    );

BOOL
ShExecuteSimpleCommand (
    PSHELL Shell,
    PSHELL_EXECUTION_NODE ExecutionNode
    );

BOOL
ShExecuteFunctionDefinition (
    PSHELL Shell,
    PSHELL_EXECUTION_NODE ExecutionNode
    );

BOOL
ShExecuteFunctionInvocation (
    PSHELL Shell,
    PSHELL_EXECUTION_NODE ExecutingNode,
    PSHELL_NODE Function,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

BOOL
ShExecuteIf (
    PSHELL Shell,
    PSHELL_EXECUTION_NODE ExecutionNode
    );

BOOL
ShExecuteFor (
    PSHELL Shell,
    PSHELL_EXECUTION_NODE ExecutionNode
    );

BOOL
ShExecuteCase (
    PSHELL Shell,
    PSHELL_EXECUTION_NODE ExecutionNode
    );

BOOL
ShExecuteWhileOrUntil (
    PSHELL Shell,
    PSHELL_EXECUTION_NODE ExecutionNode
    );

BOOL
ShExecuteSubshellGroup (
    PSHELL Shell,
    PSHELL_EXECUTION_NODE ExecutionNode
    );

VOID
ShExitOnError (
    PSHELL Shell
    );

BOOL
ShApplyRedirections (
    PSHELL Shell,
    PSHELL_EXECUTION_NODE ExecutionNode
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Set this variable if swiss command should be recognized even before
// searching the path.
//

BOOL ShUseSwissBuiltins = TRUE;

//
// Define the quoted at arguments string.
//

CHAR ShQuotedAtArgumentsString[] =
    {SHELL_CONTROL_QUOTE, '$', '@', SHELL_CONTROL_QUOTE, '\0'};

//
// ------------------------------------------------------------------ Functions
//

BOOL
ShExecute (
    PSHELL Shell,
    PINT ReturnValue
    )

/*++

Routine Description:

    This routine executes commands from the input of the shell.

Arguments:

    Shell - Supplies a pointer to the shell whose input should be read and
        executed.

    ReturnValue - Supplies a pointer where the return value of the shell will
        be returned.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSHELL_NODE Command;
    BOOL Result;

    Result = FALSE;
    ShPrintPrompt(Shell, 1);
    while (Shell->Exited == FALSE) {
        ShCheckForSignals(Shell);
        Result = ShParse(Shell, &Command);
        if (Result == FALSE) {
            if ((Shell->Options & SHELL_OPTION_INTERACTIVE) != 0) {
                ShPrintPrompt(Shell, 1);
                Shell->Lexer.LexerPrimed = FALSE;
                continue;
            }

            break;
        }

        if (Command == NULL) {
            break;
        }

        if ((Shell->Options & SHELL_OPTION_NO_EXECUTE) == 0) {
            Result = ShExecuteNode(Shell, Command);
        }

        ShReleaseNode(Command);
        if (Result == FALSE) {
            if ((Shell->Options & SHELL_OPTION_INTERACTIVE) == 0) {
                break;
            }
        }
    }

    *ReturnValue = Shell->LastReturnValue;
    return Result;
}

VOID
ShRestoreRedirections (
    PSHELL Shell,
    PLIST_ENTRY ActiveRedirectList
    )

/*++

Routine Description:

    This routine restores all active redirections back to their previous state.

Arguments:

    Shell - Supplies a pointer to the shell.

    ActiveRedirectList - Supplies a pointer to the active redirect list.

Return Value:

    None.

--*/

{

    PSHELL_ACTIVE_REDIRECT ActiveRedirect;
    INT ReplacedDescriptor;

    while (LIST_EMPTY(ActiveRedirectList) == FALSE) {

        //
        // Loop backwards so that if the same descriptor is redirected multiple
        // times then its value gets popped back to the original.
        //

        ActiveRedirect = LIST_VALUE(ActiveRedirectList->Previous,
                                    SHELL_ACTIVE_REDIRECT,
                                    ListEntry);

        LIST_REMOVE(&(ActiveRedirect->ListEntry));
        if (ActiveRedirect->OriginalDescriptor != -1) {
            ReplacedDescriptor = ShDup2(Shell,
                                        ActiveRedirect->OriginalDescriptor,
                                        ActiveRedirect->FileNumber);

            if (ReplacedDescriptor < 0) {
                PRINT_ERROR("Failed to restore file number %d.\n",
                            ActiveRedirect->FileNumber);
            }

            ShClose(Shell, ActiveRedirect->OriginalDescriptor);

        //
        // If there was no original descriptor there, close whatever there is
        // now to restore it to former non-glory.
        //

        } else {
            ShClose(Shell, ActiveRedirect->FileNumber);
        }

        if (ActiveRedirect->ChildProcessId > 0) {
            SwWaitPid(ActiveRedirect->ChildProcessId, 0, NULL);
        }

        free(ActiveRedirect);
    }

    return;
}

VOID
ShSetTerminalMode (
    PSHELL Shell,
    BOOL Raw
    )

/*++

Routine Description:

    This routine potentially sets the terminal input mode one way or another.
    If the interactive flag is not set, this does nothing.

Arguments:

    Shell - Supplies a pointer to the shell. If the interactive flag is not set
        in this shell, then nothing happens.

    Raw - Supplies a boolean indicating whether to set it in raw mode (TRUE)
        or it's previous original mode (FALSE).

Return Value:

    None.

--*/

{

    if ((Shell->Options & SHELL_OPTION_INTERACTIVE) == 0) {
        return;
    }

    if (Raw != FALSE) {
        SwSetRawInputMode(&ShBackspaceCharacter, &ShKillLineCharacter);

    } else {
        SwRestoreInputMode();
    }

    return;
}

INT
ShRunCommand (
    PSHELL Shell,
    PSTR Command,
    PSTR *Arguments,
    INT ArgumentCount,
    INT Asynchronous,
    PINT ReturnValue
    )

/*++

Routine Description:

    This routine is called to run a basic command for the shell.

Arguments:

    Shell - Supplies a pointer to the shell.

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
    PSTR FullCommandPath;
    ULONG FullCommandPathSize;
    BOOL Result;
    INT Status;
    PSWISS_COMMAND_ENTRY SwissCommand;
    id_t UserId;

    FullCommandPath = NULL;
    *ReturnValue = -1;
    Child = -1;

    //
    // If enabled, try the builtin commands.
    //

    if (ShUseSwissBuiltins != FALSE) {
        SwissCommand = SwissFindCommand(Arguments[0]);

        //
        // If the command is setuid and the environment is currently not setuid,
        // pretend like the command wasn't found.
        //

        if ((SwissCommand != NULL) &&
            ((SwissCommand->Flags & SWISS_APP_SETUID_OK) != 0)) {

            UserId = SwGetEffectiveUserId();
            if ((UserId != 0) && (UserId == SwGetRealUserId())) {
                SwissCommand = NULL;
            }
        }

        if (SwissCommand != NULL) {
            if (SwForkSupported != 0) {
                Child = SwFork();
                if (Child < 0) {
                    PRINT_ERROR("sh: Failed to fork: %s\n", strerror(errno));
                    Status = -1;
                    goto RunCommandEnd;

                } else if (Child == 0) {
                    ShRestoreOriginalSignalDispositions();
                    SwissRunCommand(SwissCommand,
                                    Arguments,
                                    ArgumentCount,
                                    FALSE,
                                    TRUE,
                                    ReturnValue);

                    exit(*ReturnValue);

                //
                // In the parent, jump down to wait for the child.
                //

                } else {
                    Status = 0;
                    goto RunCommandEnd;
                }

            //
            // If fork is not supported (Windows), just execute the command in
            // a separate process.
            //

            } else {
                fflush(NULL);
                Result = SwissRunCommand(SwissCommand,
                                         Arguments,
                                         ArgumentCount,
                                         TRUE,
                                         !Asynchronous,
                                         ReturnValue);

                if (Result != FALSE) {
                    Status = 0;
                    ShOsConvertExitStatus(ReturnValue);
                    goto RunCommandEnd;
                }
            }
        }
    }

    *ReturnValue = 0;
    Result = ShLocateCommand(Shell,
                             Arguments[0],
                             strlen(Arguments[0]) + 1,
                             TRUE,
                             &FullCommandPath,
                             &FullCommandPathSize,
                             ReturnValue);

    if (Result == FALSE) {
        Status = -1;
        goto RunCommandEnd;
    }

    if (*ReturnValue != 0) {
        if (*ReturnValue == SHELL_ERROR_OPEN) {
            PRINT_ERROR("sh: %s: Command not found.\n", Arguments[0]);

        } else if (*ReturnValue == SHELL_ERROR_EXECUTE) {
            PRINT_ERROR("sh: %s: Permission denied.\n", Arguments[0]);
        }

        Status = 0;
        goto RunCommandEnd;
    }

    if (SwForkSupported != 0) {
        Child = SwFork();
        if (Child < 0) {
            PRINT_ERROR("sh: Failed to fork: %s\n", strerror(errno));
            Status = -1;
            goto RunCommandEnd;

        } else if (Child == 0) {
            ShRestoreOriginalSignalDispositions();
            SwExec(FullCommandPath, Arguments, ArgumentCount);
            exit(errno);

        //
        // In the parent, jump down to wait for the child.
        //

        } else {
            Status = 0;
            goto RunCommandEnd;
        }

    } else {
        fflush(NULL);
        Status = SwRunCommand(FullCommandPath,
                              Arguments,
                              ArgumentCount,
                              Asynchronous,
                              ReturnValue);

        ShOsConvertExitStatus(ReturnValue);
    }

RunCommandEnd:

    //
    // Wait for the child if there is one.
    //

    if (Child > 0) {
        if (Asynchronous != 0) {
            *ReturnValue = 0;

        } else {
            Status = SwWaitPid(Child, 0, ReturnValue);
            if (Status != Child) {
                PRINT_ERROR("sh: Failed to wait for child %d\n", Child);
                Status = -1;

            } else {
                ShOsConvertExitStatus(ReturnValue);
            }
        }
    }

    if (*ReturnValue > SHELL_EXIT_SIGNALED) {
        if ((Shell->Options & SHELL_OPTION_INTERACTIVE) != 0) {
            printf("%s terminated by signal %d: %s\n",
                   Arguments[0],
                   *ReturnValue - SHELL_EXIT_SIGNALED,
                   strsignal(*ReturnValue - SHELL_EXIT_SIGNALED));
        }

    //
    // If the command exited normally, save any terminal changes it may have
    // made.
    //

    } else {
        SwSaveTerminalMode();
    }

    if ((FullCommandPath != NULL) && (FullCommandPath != Arguments[0])) {
        free(FullCommandPath);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
ShExecuteNode (
    PSHELL Shell,
    PSHELL_NODE Node
    )

/*++

Routine Description:

    This routine executes a generic shell node.

Arguments:

    Shell - Supplies a pointer to the shell containing the node.

    Node - Supplies a pointer to the node to execute.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSHELL_EXECUTION_NODE ExecutionNode;
    ULONG OriginalLineNumber;
    BOOL Result;

    if ((Node->RunInBackground != FALSE) && (SwForkSupported != 0)) {
        return ShExecuteAsynchronousNode(Shell, Node);
    }

    //
    // Create an execution node and push it on the stack.
    //

    ExecutionNode = malloc(sizeof(SHELL_EXECUTION_NODE));
    if (ExecutionNode == NULL) {
        return FALSE;
    }

    INITIALIZE_LIST_HEAD(&(ExecutionNode->VariableList));
    INITIALIZE_LIST_HEAD(&(ExecutionNode->ArgumentList));
    INITIALIZE_LIST_HEAD(&(ExecutionNode->ActiveRedirectList));
    ExecutionNode->Node = Node;
    ExecutionNode->Flags = 0;
    ExecutionNode->ReturnValue = 0;
    INSERT_AFTER(&(ExecutionNode->ListEntry), &(Shell->ExecutionStack));
    OriginalLineNumber = Shell->ExecutingLineNumber;
    Shell->ExecutingLineNumber = Node->LineNumber;
    Result = ShApplyRedirections(Shell, ExecutionNode);
    if (Result == FALSE) {
        goto ExecuteNodeEnd;
    }

    Result = FALSE;
    switch (Node->Type) {
    case ShellNodeList:
    case ShellNodeTerm:
    case ShellNodeBraceGroup:
        Result = ShExecuteList(Shell, ExecutionNode);
        break;

    case ShellNodeAndOr:
        Result = ShExecuteAndOr(Shell, ExecutionNode);
        break;

    case ShellNodePipeline:
        Result = ShExecutePipeline(Shell, ExecutionNode);
        break;

    case ShellNodeSimpleCommand:
        Result = ShExecuteSimpleCommand(Shell, ExecutionNode);
        break;

    case ShellNodeFunction:
        Result = ShExecuteFunctionDefinition(Shell, ExecutionNode);
        break;

    case ShellNodeIf:
        Result = ShExecuteIf(Shell, ExecutionNode);
        break;

    case ShellNodeFor:
        Result = ShExecuteFor(Shell, ExecutionNode);
        break;

    case ShellNodeCase:
        Result = ShExecuteCase(Shell, ExecutionNode);
        break;

    case ShellNodeWhile:
    case ShellNodeUntil:
        Result = ShExecuteWhileOrUntil(Shell, ExecutionNode);
        break;

    case ShellNodeSubshell:
        Result = ShExecuteSubshellGroup(Shell, ExecutionNode);
        break;

    default:

        assert(FALSE);

        Result = FALSE;
    }

ExecuteNodeEnd:

    //
    // Remove the node from the stack if not already done.
    //

    if (ExecutionNode->ListEntry.Next != NULL) {
        LIST_REMOVE(&(ExecutionNode->ListEntry));
    }

    ShDestroyArgumentList(&(ExecutionNode->ArgumentList));
    ShDestroyVariableList(&(ExecutionNode->VariableList));
    ShRestoreRedirections(Shell, &(ExecutionNode->ActiveRedirectList));
    free(ExecutionNode);
    Shell->ExecutingLineNumber = OriginalLineNumber;
    Shell->LastReturnValue = Shell->ReturnValue;
    return Result;
}

BOOL
ShExecuteAsynchronousNode (
    PSHELL Shell,
    PSHELL_NODE Node
    )

/*++

Routine Description:

    This routine executes a shell node asynchronously.

Arguments:

    Shell - Supplies a pointer to the shell containing the node.

    Node - Supplies a pointer to the node to execute.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    pid_t Process;
    BOOL Result;

    assert((Node->RunInBackground != FALSE) && (SwForkSupported != 0));

    //
    // Attempt to fork the current process, which is easiest and most in line
    // with what the shell would like to do.
    //

    Process = SwFork();

    //
    // If this is the child, set the node to be synchronous, run the node, and
    // exit straight away. Altering the node here doesn't change the memory of
    // the parent process remember. See how easy that was?
    //

    if (Process == 0) {
        if (Shell->PostForkCloseDescriptor != -1) {
            close(Shell->PostForkCloseDescriptor);
            Shell->PostForkCloseDescriptor = -1;
        }

        Node->RunInBackground = FALSE;
        ShExecuteNode(Shell, Node);
        exit(Shell->LastReturnValue);

    //
    // If this is the parent process, then work is finished here. Even easier!
    //

    } else if (Process != -1) {
        Result = TRUE;
        goto ExecuteAsynchronousNodeEnd;

    //
    // Fork failed.
    //

    } else {
        Result = FALSE;
        goto ExecuteAsynchronousNodeEnd;
    }

    Result = TRUE;

ExecuteAsynchronousNodeEnd:
    if (Result == FALSE) {
        Shell->ReturnValue = 1;

    } else {
        Shell->ReturnValue = 0;
    }

    return Result;
}

BOOL
ShExecuteList (
    PSHELL Shell,
    PSHELL_EXECUTION_NODE ExecutionNode
    )

/*++

Routine Description:

    This routine executes a list node.

Arguments:

    Shell - Supplies a pointer to the shell containing the node.

    ExecutionNode - Supplies a pointer to the node to execute.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSHELL_NODE Child;
    PLIST_ENTRY CurrentEntry;
    PSHELL_NODE Node;
    BOOL Result;

    Node = ExecutionNode->Node;

    assert((Node->Type == ShellNodeList) || (Node->Type == ShellNodeTerm) ||
           (Node->Type == ShellNodeBraceGroup));

    CurrentEntry = Node->Children.Next;
    while (CurrentEntry != &(Node->Children)) {
        Child = LIST_VALUE(CurrentEntry, SHELL_NODE, SiblingListEntry);
        CurrentEntry = CurrentEntry->Next;
        Result = ShExecuteNode(Shell, Child);
        if (Result == FALSE) {
            return FALSE;
        }

        if (Shell->Exited != FALSE) {
            break;
        }

        //
        // Break out if the execution node was removed from the stack.
        //

        if (ExecutionNode->ListEntry.Next == NULL) {
            break;
        }
    }

    return TRUE;
}

BOOL
ShExecuteAndOr (
    PSHELL Shell,
    PSHELL_EXECUTION_NODE ExecutionNode
    )

/*++

Routine Description:

    This routine executes a logical And-Or (&&/||) node.

Arguments:

    Shell - Supplies a pointer to the shell containing the node.

    ExecutionNode - Supplies a pointer to the node to execute.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSHELL_NODE Child;
    PLIST_ENTRY CurrentEntry;
    BOOL Execute;
    PSHELL_NODE Node;
    PSHELL_NODE Previous;
    BOOL Result;

    Node = ExecutionNode->Node;

    assert(Node->Type == ShellNodeAndOr);

    Previous = NULL;
    CurrentEntry = Node->Children.Next;
    while (CurrentEntry != &(Node->Children)) {
        Child = LIST_VALUE(CurrentEntry, SHELL_NODE, SiblingListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // The first node always executes. If the previous node was an AND,
        // then don't execute this node if the previous node failed. If the
        // previous node was an OR, then don't execute this node if the
        // previous node succeeded.
        //

        Execute = TRUE;
        if (Previous != NULL) {
            if (Previous->AndOr == TOKEN_DOUBLE_AND) {
                if (Shell->LastReturnValue != 0) {
                    Execute = FALSE;
                }

            } else if (Previous->AndOr == TOKEN_DOUBLE_OR) {
                if (Shell->LastReturnValue == 0) {
                    Execute = FALSE;
                }
            }
        }

        if (Execute != FALSE) {
            Result = ShExecuteNode(Shell, Child);
            if (Result == FALSE) {
                return FALSE;
            }
        }

        if (Shell->Exited != FALSE) {
            break;
        }

        //
        // Break out if the execution node was removed from the stack.
        //

        if (ExecutionNode->ListEntry.Next == NULL) {
            break;
        }

        Previous = Child;
    }

    return TRUE;
}

BOOL
ShExecutePipeline (
    PSHELL Shell,
    PSHELL_EXECUTION_NODE ExecutionNode
    )

/*++

Routine Description:

    This routine executes a pipeline.

Arguments:

    Shell - Supplies a pointer to the shell containing the node.

    ExecutionNode - Supplies a pointer to the node to execute.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSHELL_NODE Child;
    PLIST_ENTRY CurrentEntry;
    INT NextPipe[2];
    PSHELL_NODE Node;
    INT OriginalStandardIn;
    INT OriginalStandardOut;
    INT PreviousPipeRead;
    BOOL Result;

    NextPipe[0] = -1;
    NextPipe[1] = -1;
    Node = ExecutionNode->Node;
    OriginalStandardIn = -1;
    OriginalStandardOut = -1;
    PreviousPipeRead = -1;
    Result = TRUE;

    assert(Node->Type == ShellNodePipeline);

    CurrentEntry = Node->Children.Next;
    while (CurrentEntry != &(Node->Children)) {
        Child = LIST_VALUE(CurrentEntry, SHELL_NODE, SiblingListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // If this is not the last node, create a new pipe and wire standard out
        // up to that pipe. If it is the last node, leave standard out wired up
        // the way it is.
        //

        if (Child->SiblingListEntry.Next != &(Node->Children)) {
            Result = ShCreatePipe(NextPipe);
            if (Result == FALSE) {
                return FALSE;
            }

            OriginalStandardOut = ShDup(Shell, STDOUT_FILENO, FALSE);
            ShDup2(Shell, NextPipe[1], STDOUT_FILENO);
            ShClose(Shell, NextPipe[1]);
            NextPipe[1] = -1;
        }

        //
        // If this is not the first node, wire up standard input to the
        // previous pipe's read end.
        //

        if (Child->SiblingListEntry.Previous != &(Node->Children)) {
            OriginalStandardIn = ShDup(Shell, STDIN_FILENO, FALSE);

            assert(PreviousPipeRead != -1);

            ShDup2(Shell, PreviousPipeRead, STDIN_FILENO);
            ShClose(Shell, PreviousPipeRead);
            PreviousPipeRead = -1;
        }

        //
        // Save the previous pipe's read entry. Make it a non-inheritable
        // handle so that when the next process closes standard in, that's the
        // last open handle.
        //

        if (NextPipe[0] != -1) {
            PreviousPipeRead = ShDup(Shell, NextPipe[0], FALSE);
            ShClose(Shell, NextPipe[0]);
            NextPipe[0] = -1;
            Shell->PostForkCloseDescriptor = PreviousPipeRead;
        }

        Result = ShExecuteNode(Shell, Child);

        //
        // Restore standard in and standard out if they were changed.
        //

        if (OriginalStandardIn != -1) {
            ShDup2(Shell, OriginalStandardIn, STDIN_FILENO);
            ShClose(Shell, OriginalStandardIn);
            OriginalStandardIn = -1;
        }

        if (OriginalStandardOut != -1) {
            ShDup2(Shell, OriginalStandardOut, STDOUT_FILENO);
            ShClose(Shell, OriginalStandardOut);
            OriginalStandardOut = -1;
        }

        if (PreviousPipeRead != -1) {

            assert((Shell->PostForkCloseDescriptor == PreviousPipeRead) ||
                   (Shell->PostForkCloseDescriptor == -1));

            Shell->PostForkCloseDescriptor = -1;
        }

        //
        // If executing the command failed, stop now.
        //

        if (Result == FALSE) {
            goto ExecutePipelineEnd;
        }

        if (Shell->Exited != FALSE) {
            break;
        }

        //
        // Break out if the execution node was removed from the stack.
        //

        if (ExecutionNode->ListEntry.Next == NULL) {
            break;
        }
    }

    assert(Shell->ReturnValue == Shell->LastReturnValue);

    if ((Shell->Exited == FALSE) && (Node->U.Pipeline.Bang != FALSE)) {
        Shell->ReturnValue = !Shell->LastReturnValue;
    }

ExecutePipelineEnd:
    if (OriginalStandardIn != -1) {
        ShDup2(Shell, OriginalStandardIn, STDIN_FILENO);
        ShClose(Shell, OriginalStandardIn);
        OriginalStandardIn = -1;
    }

    if (OriginalStandardOut != -1) {
        ShDup2(Shell, OriginalStandardOut, STDOUT_FILENO);
        ShClose(Shell, OriginalStandardOut);
        OriginalStandardOut = -1;
    }

    if (NextPipe[0] != -1) {
        ShClose(Shell, NextPipe[0]);
    }

    if (NextPipe[1] != -1) {
        ShClose(Shell, NextPipe[1]);
    }

    //
    // Check for signals to reap any child processes that were created.
    //

    ShCheckForSignals(Shell);
    return Result;
}

BOOL
ShExecuteSimpleCommand (
    PSHELL Shell,
    PSHELL_EXECUTION_NODE ExecutionNode
    )

/*++

Routine Description:

    This routine executes a simple command. The function makes it sound easier
    than it is.

Arguments:

    Shell - Supplies a pointer to the shell containing the node.

    ExecutionNode - Supplies a pointer to the node to execute.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG ArgumentCount;
    ULONG ArgumentIndex;
    PSTR *Arguments;
    BOOL Asynchronous;
    PSHELL_BUILTIN_COMMAND BuiltinCommand;
    PSTR ExpandedArguments;
    UINTN ExpandedArgumentsSize;
    PSHELL_FUNCTION Function;
    PSHELL_EXECUTION_NODE LatestExecutionNode;
    PSHELL_NODE Node;
    BOOL Result;
    INT ReturnValue;
    PSHELL_NODE_SIMPLE_COMMAND SimpleCommand;

    ArgumentCount = 0;
    Arguments = NULL;
    ExpandedArguments = NULL;
    Node = ExecutionNode->Node;

    assert(Node->Type == ShellNodeSimpleCommand);

    SimpleCommand = &(Node->U.SimpleCommand);
    if ((Shell->Options & SHELL_OPTION_TRACE_COMMAND) != 0) {
        ShPrintPrompt(Shell, 4);
    }

    Shell->ReturnValue = 0;
    Result = ShExecuteVariableAssignments(Shell, ExecutionNode);
    if (Result == FALSE) {
        goto ExecuteSimpleCommandEnd;
    }

    if (SimpleCommand->Arguments != NULL) {

        //
        // Perform expansions, field splitting, and quote removal.
        //

        Result = ShPerformExpansions(Shell,
                                     SimpleCommand->Arguments,
                                     SimpleCommand->ArgumentsSize,
                                     0,
                                     &ExpandedArguments,
                                     &ExpandedArgumentsSize,
                                     &Arguments,
                                     &ArgumentCount);

        if (Result == FALSE) {
            goto ExecuteSimpleCommandEnd;
        }
    }

    //
    // If tracing is enabled, print the tracing prompt and then the command
    //

    if ((Shell->Options & SHELL_OPTION_TRACE_COMMAND) != 0) {
        for (ArgumentIndex = 0;
             ArgumentIndex < ArgumentCount;
             ArgumentIndex += 1) {

            ShPrintTrace(Shell, "%s ", Arguments[ArgumentIndex]);
        }

        ShPrintTrace(Shell, "\n");
    }

    if (SimpleCommand->Arguments == NULL) {
        Result = TRUE;
        goto ExecuteSimpleCommandEnd;
    }

    //
    // If the command is empty, don't do much.
    //

    if ((ArgumentCount == 0) || (*(Arguments[0]) == '\0')) {
        Result = TRUE;
        goto ExecuteSimpleCommandEnd;
    }

    Asynchronous = FALSE;
    if (Node->RunInBackground != FALSE) {
        Asynchronous = TRUE;
    }

    //
    // Check to see if this is a builtin command, and run it if it is.
    //

    BuiltinCommand = ShIsBuiltinCommand(Arguments[0]);
    if (BuiltinCommand != NULL) {
        ReturnValue = ShRunBuiltinCommand(Shell,
                                          BuiltinCommand,
                                          ArgumentCount,
                                          Arguments);

        //
        // Put the return value on the most recent execution node.
        //

        if (!LIST_EMPTY(&(Shell->ExecutionStack))) {
            LatestExecutionNode = LIST_VALUE(Shell->ExecutionStack.Next,
                                             SHELL_EXECUTION_NODE,
                                             ListEntry);

            LatestExecutionNode->ReturnValue = ReturnValue;
        }

    } else {

        //
        // Look to see if this is a function, and run that function if so.
        //

        Function = ShGetFunction(Shell, Arguments[0], strlen(Arguments[0]) + 1);
        if (Function != NULL) {
            Result = ShExecuteFunctionInvocation(Shell,
                                                 ExecutionNode,
                                                 Function->Node,
                                                 Arguments + 1,
                                                 ArgumentCount - 1);

            goto ExecuteSimpleCommandEnd;
        }

        Result = TRUE;
        ShRunCommand(Shell,
                     Arguments[0],
                     Arguments,
                     ArgumentCount,
                     Asynchronous,
                     &ReturnValue);
    }

    Shell->ReturnValue = ReturnValue;
    Result = TRUE;

ExecuteSimpleCommandEnd:

    //
    // If the simple command failed and exit on errors is set, potentially
    // exit.
    //

    if ((Shell->Options & SHELL_OPTION_EXIT_ON_FAILURE) != 0) {
        ShExitOnError(Shell);
    }

    if (Arguments != NULL) {
        free(Arguments);
    }

    if (ExpandedArguments != NULL) {
        free(ExpandedArguments);
    }

    return Result;
}

BOOL
ShExecuteFunctionDefinition (
    PSHELL Shell,
    PSHELL_EXECUTION_NODE ExecutionNode
    )

/*++

Routine Description:

    This routine executes a function definition node. The definitions don't
    actually run the function, so this is a no-op.

Arguments:

    Shell - Supplies a pointer to the shell containing the node.

    ExecutionNode - Supplies a pointer to the node to execute.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    BOOL Result;

    //
    // Make the function declaration active in the shell.
    //

    Result = ShDeclareFunction(Shell, ExecutionNode->Node);
    if (Result == FALSE) {
        return FALSE;
    }

    //
    // Function definitions are successful if they were parsed correctly, which
    // it was.
    //

    Shell->ReturnValue = 0;
    return TRUE;
}

BOOL
ShExecuteFunctionInvocation (
    PSHELL Shell,
    PSHELL_EXECUTION_NODE ExecutingNode,
    PSHELL_NODE Function,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine executes a function.

Arguments:

    Shell - Supplies a pointer to the shell containing the node.

    ExecutingNode - Supplies a pointer to the execution node this function is
        run as. This will temporarily get pointed to the function node.

    Function - Supplies a pointer to the function to execute.

    Arguments - Supplies a pointer to the array of strings containing the
        arguments.

    ArgumentCount - Supplies the number of arguments in the array.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSHELL_NODE Body;
    PSHELL_NODE OriginalNode;
    BOOL Result;

    assert(ExecutingNode->Node->Type == ShellNodeSimpleCommand);
    assert((ExecutingNode->Flags & SHELL_EXECUTION_BODY) == 0);

    OriginalNode = ExecutingNode->Node;
    ExecutingNode->Node = Function;
    ExecutingNode->Flags |= SHELL_EXECUTION_BODY;

    assert(LIST_EMPTY(&(ExecutingNode->ArgumentList)) != FALSE);

    //
    // Create an argument list out of the incoming arguments.
    //

    Result = ShCreateArgumentList(Arguments,
                                  ArgumentCount,
                                  &(ExecutingNode->ArgumentList));

    if (Result == FALSE) {
        goto ExecuteFunctionInvocationEnd;
    }

    //
    // There should only be one thing in the children list, the compound
    // body statement.
    //

    assert((LIST_EMPTY(&(Function->Children)) == FALSE) &&
           (Function->Children.Next->Next == &(Function->Children)));

    Body = LIST_VALUE(Function->Children.Next, SHELL_NODE, SiblingListEntry);
    Result = ShExecuteNode(Shell, Body);
    if (Result == FALSE) {
        goto ExecuteFunctionInvocationEnd;
    }

ExecuteFunctionInvocationEnd:
    ExecutingNode->Flags &= ~SHELL_EXECUTION_BODY;

    //
    // If the options were made local, restore them now.
    //

    if ((ExecutingNode->Flags & SHELL_EXECUTION_RESTORE_OPTIONS) != 0) {
        Shell->Options = ExecutingNode->SavedOptions;
    }

    ExecutingNode->Node = OriginalNode;

    //
    // Destroy the current argument list, as it's the ones set up for the
    // function.
    //

    ShDestroyArgumentList(&(ExecutingNode->ArgumentList));
    return Result;
}

BOOL
ShExecuteIf (
    PSHELL Shell,
    PSHELL_EXECUTION_NODE ExecutionNode
    )

/*++

Routine Description:

    This routine executes an if statement.

Arguments:

    Shell - Supplies a pointer to the shell containing the node.

    ExecutionNode - Supplies a pointer to the node to execute.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSHELL_NODE Condition;
    INT ConditionReturn;
    PSHELL_NODE FalseStatement;
    PSHELL_NODE Node;
    BOOL Result;
    PSHELL_NODE TrueStatement;

    Node = ExecutionNode->Node;

    assert(Node->Type == ShellNodeIf);

    //
    // Get the condition, true, and maybe the false conditions.
    //

    assert(Node->Children.Next != &(Node->Children));

    Condition = LIST_VALUE(Node->Children.Next, SHELL_NODE, SiblingListEntry);

    assert(Condition->SiblingListEntry.Next != &(Node->Children));

    TrueStatement = LIST_VALUE(Condition->SiblingListEntry.Next,
                               SHELL_NODE,
                               SiblingListEntry);

    FalseStatement = NULL;
    if (TrueStatement->SiblingListEntry.Next != &(Node->Children)) {
        FalseStatement = LIST_VALUE(TrueStatement->SiblingListEntry.Next,
                                    SHELL_NODE,
                                    SiblingListEntry);
    }

    Result = ShExecuteNode(Shell, Condition);
    if (Result == FALSE) {
        return FALSE;
    }

    if (Shell->Exited != FALSE) {
        return TRUE;
    }

    ConditionReturn = Shell->LastReturnValue;
    Shell->ReturnValue = 0;

    //
    // Break out if no longer on the execution stack.
    //

    if (ExecutionNode->ListEntry.Next == NULL) {
        return TRUE;
    }

    //
    // Go to the true statement if the return value was zero.
    //

    ExecutionNode->Flags |= SHELL_EXECUTION_BODY;
    if (ConditionReturn == 0) {
        Result = ShExecuteNode(Shell, TrueStatement);

    } else if (FalseStatement != NULL) {
        Result = ShExecuteNode(Shell, FalseStatement);
    }

    ExecutionNode->Flags &= ~SHELL_EXECUTION_BODY;
    return Result;
}

BOOL
ShExecuteFor (
    PSHELL Shell,
    PSHELL_EXECUTION_NODE ExecutionNode
    )

/*++

Routine Description:

    This routine executes a for loop.

Arguments:

    Shell - Supplies a pointer to the shell containing the node.

    ExecutionNode - Supplies a pointer to the node to execute.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSHELL_NODE DoGroup;
    PSHELL_NODE_FOR ForStatement;
    PSHELL_NODE Node;
    BOOL Result;
    PSTR SplitBuffer;
    ULONG WordCount;
    ULONG WordIndex;
    PSTR WordListString;
    UINTN WordListStringSize;
    PSTR *Words;

    Node = ExecutionNode->Node;
    SplitBuffer = NULL;
    WordListString = NULL;
    Words = NULL;

    assert(Node->Type == ShellNodeFor);
    assert(LIST_EMPTY(&(Node->Children)) == FALSE);

    ForStatement = &(Node->U.For);
    DoGroup = LIST_VALUE(Node->Children.Next, SHELL_NODE, SiblingListEntry);

    assert(DoGroup->SiblingListEntry.Next == &(Node->Children));

    //
    // Expand the word list. If there is no word list, use the parameters.
    //

    if (ForStatement->WordListBuffer == NULL) {
        Result = ShPerformExpansions(Shell,
                                     ShQuotedAtArgumentsString,
                                     sizeof(ShQuotedAtArgumentsString),
                                     0,
                                     &WordListString,
                                     &WordListStringSize,
                                     &Words,
                                     &WordCount);

    } else {
        Result = ShPerformExpansions(Shell,
                                     ForStatement->WordListBuffer,
                                     ForStatement->WordListBufferSize,
                                     0,
                                     &WordListString,
                                     &WordListStringSize,
                                     &Words,
                                     &WordCount);
    }

    if (Result == FALSE) {
        goto ExecuteForEnd;
    }

    //
    // If there are no words anymore, simply end.
    //

    if (WordCount == 0) {
        Shell->ReturnValue = 0;
        Result = TRUE;
        goto ExecuteForEnd;
    }

    //
    // Loop through every word, assign the variable, and execute the do group.
    //

    for (WordIndex = 0; WordIndex < WordCount; WordIndex += 1) {
        Result = ShSetVariable(Shell,
                               ForStatement->Name,
                               ForStatement->NameSize,
                               Words[WordIndex],
                               strlen(Words[WordIndex]) + 1);

        if (Result == FALSE) {
            goto ExecuteForEnd;
        }

        Result = ShExecuteNode(Shell, DoGroup);
        if (Result == FALSE) {
            goto ExecuteForEnd;
        }

        if (Shell->Exited != FALSE) {
            break;
        }

        //
        // Stop if this execution node is no longer on the stack.
        //

        if (ExecutionNode->ListEntry.Next == NULL) {
            break;
        }
    }

ExecuteForEnd:
    if (WordListString != NULL) {
        free(WordListString);
    }

    if (SplitBuffer != NULL) {
        free(SplitBuffer);
    }

    if (Words != NULL) {
        free(Words);
    }

    return Result;
}

BOOL
ShExecuteCase (
    PSHELL Shell,
    PSHELL_EXECUTION_NODE ExecutionNode
    )

/*++

Routine Description:

    This routine executes a list node.

Arguments:

    Shell - Supplies a pointer to the shell containing the node.

    ExecutionNode - Supplies a pointer to the node to execute.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PLIST_ENTRY CurrentPatternEntry;
    PLIST_ENTRY CurrentSetEntry;
    PSTR ExpandedPattern;
    UINTN ExpandedPatternSize;
    PSTR Input;
    UINTN InputSize;
    BOOL Match;
    PSHELL_NODE Node;
    ULONG Options;
    PSHELL_CASE_PATTERN_ENTRY PatternEntry;
    BOOL Result;
    PSHELL_CASE_PATTERN_SET Set;

    Input = NULL;
    Match = FALSE;
    Node = ExecutionNode->Node;

    assert(Node->Type == ShellNodeCase);

    //
    // Get and expand the input.
    //

    Result = ShPerformExpansions(Shell,
                                 Node->U.Case.Name,
                                 Node->U.Case.NameSize,
                                 SHELL_EXPANSION_OPTION_NO_FIELD_SPLIT,
                                 &Input,
                                 &InputSize,
                                 NULL,
                                 NULL);

    if (Result == FALSE) {
        goto ExecuteCaseEnd;
    }

    if (LIST_EMPTY(&(Node->U.Case.PatternList)) != FALSE) {
        Result = TRUE;
        goto ExecuteCaseEnd;
    }

    Options = SHELL_EXPANSION_OPTION_NO_FIELD_SPLIT |
              SHELL_EXPANSION_OPTION_NO_QUOTE_REMOVAL;

    //
    // Loop through every case and see if any of the sets of patterns match.
    //

    CurrentSetEntry = Node->U.Case.PatternList.Next;
    while (CurrentSetEntry != &(Node->U.Case.PatternList)) {
        Set = LIST_VALUE(CurrentSetEntry, SHELL_CASE_PATTERN_SET, ListEntry);
        CurrentSetEntry = CurrentSetEntry->Next;

        //
        // Loop through every pattern in the set.
        //

        CurrentPatternEntry = Set->PatternEntryList.Next;
        while (CurrentPatternEntry != &(Set->PatternEntryList)) {
            PatternEntry = LIST_VALUE(CurrentPatternEntry,
                                      SHELL_CASE_PATTERN_ENTRY,
                                      ListEntry);

            CurrentPatternEntry = CurrentPatternEntry->Next;
            Result = ShPerformExpansions(Shell,
                                         PatternEntry->Pattern,
                                         PatternEntry->PatternSize,
                                         Options,
                                         &ExpandedPattern,
                                         &ExpandedPatternSize,
                                         NULL,
                                         NULL);

            if (Result == FALSE) {
                goto ExecuteCaseEnd;
            }

            ShStringDequote(ExpandedPattern,
                            ExpandedPatternSize,
                            SHELL_DEQUOTE_FOR_PATTERN_MATCHING,
                            &ExpandedPatternSize);

            Match = SwDoesPatternMatch(Input,
                                       InputSize,
                                       ExpandedPattern,
                                       ExpandedPatternSize);

            free(ExpandedPattern);

            //
            // If the input matches the case, run the action associated with
            // it and end the case.
            //

            if (Match != FALSE) {
                Result = TRUE;
                if (Set->Action != NULL) {
                    Result = ShExecuteNode(Shell, Set->Action);
                }

                goto ExecuteCaseEnd;
            }
        }
    }

ExecuteCaseEnd:
    if (Input != NULL) {
        free(Input);
    }

    //
    // If no case was executed, the return value is zero.
    //

    if (Match == FALSE) {
        Shell->ReturnValue = 0;
    }

    return Result;
}

BOOL
ShExecuteWhileOrUntil (
    PSHELL Shell,
    PSHELL_EXECUTION_NODE ExecutionNode
    )

/*++

Routine Description:

    This routine executes a while statement or an until statement.

Arguments:

    Shell - Supplies a pointer to the shell containing the node.

    ExecutionNode - Supplies a pointer to the node to execute.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    BOOL BeenAround;
    PSHELL_NODE Condition;
    INT ConditionResult;
    PSHELL_NODE DoGroup;
    BOOL ExecuteDoGroup;
    PSHELL_NODE Node;
    BOOL Result;

    Node = ExecutionNode->Node;

    assert((Node->Type == ShellNodeWhile) || (Node->Type == ShellNodeUntil));

    //
    // Get the condition, true, and maybe the false conditions.
    //

    assert(Node->Children.Next != &(Node->Children));

    Condition = LIST_VALUE(Node->Children.Next, SHELL_NODE, SiblingListEntry);

    assert(Condition->SiblingListEntry.Next != &(Node->Children));

    DoGroup = LIST_VALUE(Condition->SiblingListEntry.Next,
                         SHELL_NODE,
                         SiblingListEntry);

    //
    // Execute the do-group as long as the condition is zero for while loops
    // or non-zero for until loops.
    //

    BeenAround = FALSE;
    while (TRUE) {
        Result = ShExecuteNode(Shell, Condition);
        if (Result == FALSE) {
            return FALSE;
        }

        if (Shell->Exited != FALSE) {
            break;
        }

        ConditionResult = Shell->LastReturnValue;
        Shell->ReturnValue = 0;

        //
        // Break out if no longer on the execution stack.
        //

        if (ExecutionNode->ListEntry.Next == NULL) {
            return TRUE;
        }

        //
        // Figure out whether or not to execute the do group.
        //

        ExecuteDoGroup = FALSE;
        if (Node->Type == ShellNodeWhile) {
            if (ConditionResult == 0) {
                ExecuteDoGroup = TRUE;
            }

        } else {
            if (ConditionResult != 0) {
                ExecuteDoGroup = TRUE;
            }
        }

        //
        // If the do-group isn't going to be executed and never has before,
        // the return value is zero. Otherwise the return value is left as the
        // last command in the do-group.
        //

        if (ExecuteDoGroup == FALSE) {
            if (BeenAround == FALSE) {
                Shell->ReturnValue = 0;
            }

            break;
        }

        //
        // Run the do-group.
        //

        ExecutionNode->Flags |= SHELL_EXECUTION_BODY;
        Result = ShExecuteNode(Shell, DoGroup);
        ExecutionNode->Flags &= ~SHELL_EXECUTION_BODY;
        if (Result == FALSE) {
            return FALSE;
        }

        //
        // Break out if no longer on the execution stack, otherwise loop
        // around and run the condition again.
        //

        if (ExecutionNode->ListEntry.Next == NULL) {
            return TRUE;
        }
    }

    return Result;
}

BOOL
ShExecuteSubshellGroup (
    PSHELL Shell,
    PSHELL_EXECUTION_NODE ExecutionNode
    )

/*++

Routine Description:

    This routine executes a subshell compound statement, which is a compound
    list inside of parentheses.

Arguments:

    Shell - Supplies a pointer to the shell containing the node.

    ExecutionNode - Supplies a pointer to the node to execute.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSHELL_NODE Child;
    pid_t ChildProcess;
    PLIST_ENTRY CurrentEntry;
    PSHELL_NODE Node;
    PSTR OriginalDirectory;
    BOOL Result;
    INT Status;
    PSHELL Subshell;
    pid_t WaitResult;

    Node = ExecutionNode->Node;
    OriginalDirectory = NULL;

    assert(Node->Type == ShellNodeSubshell);

    Subshell = ShCreateSubshell(Shell, NULL, 0, FALSE);
    if (Subshell == NULL) {
        return FALSE;
    }

    ChildProcess = -1;
    if (SwForkSupported != FALSE) {
        ChildProcess = SwFork();

    } else {

        //
        // Save the current directory.
        //

        OriginalDirectory = getcwd(NULL, 0);
    }

    //
    // Execute all the children on the subshell (either if this is the child
    // process or fork never happened).
    //

    if (ChildProcess <= 0) {
        CurrentEntry = Node->Children.Next;
        while (CurrentEntry != &(Node->Children)) {
            Child = LIST_VALUE(CurrentEntry, SHELL_NODE, SiblingListEntry);
            CurrentEntry = CurrentEntry->Next;
            Result = ShExecuteNode(Subshell, Child);
            if (Result == FALSE) {
                break;
            }

            if (Shell->Exited != FALSE) {
                break;
            }

            //
            // Break out if the execution node was removed from the stack.
            //

            if (ExecutionNode->ListEntry.Next == NULL) {
                break;
            }
        }
    }

    //
    // If this is the child process, exit now.
    //

    if (ChildProcess == 0) {
        exit(Subshell->LastReturnValue);

    //
    // If this is the parent process, wait for the child.
    //

    } else if (ChildProcess > 0) {
        WaitResult = SwWaitPid(ChildProcess, 0, &(Subshell->LastReturnValue));
        if (WaitResult == -1) {
            Subshell->LastReturnValue = SHELL_ERROR_OPEN;
            SwPrintError(errno,
                         NULL,
                         "Failed to wait for pid %d",
                         ChildProcess);

            Result = FALSE;
            goto ExecuteSubshellGroupEnd;
        }

        ShOsConvertExitStatus(&(Subshell->LastReturnValue));
    }

    Shell->ReturnValue = Subshell->LastReturnValue;
    Result = TRUE;

ExecuteSubshellGroupEnd:
    if (Subshell != NULL) {
        ShDestroyShell(Subshell);
    }

    //
    // Restore the current directory.
    //

    if (OriginalDirectory != NULL) {
        Status = chdir(OriginalDirectory);
        if (Status != 0) {
            Result = FALSE;
        }

        free(OriginalDirectory);
    }

    return Result;
}

VOID
ShExitOnError (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine exits the shell if the most recent simple command failed,
    unless the simple command is part of a compound list inside a while, until,
    or if, is part of an And-Or list, or is a pipeline with a bang.

Arguments:

    Shell - Supplies a pointer to the shell containing the node.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PSHELL_EXECUTION_NODE ExecutionNode;
    SHELL_NODE_TYPE Type;

    if ((Shell->Exited != FALSE) || (Shell->ReturnValue == 0)) {
        return;
    }

    CurrentEntry = Shell->ExecutionStack.Next;
    while (CurrentEntry != &(Shell->ExecutionStack)) {
        ExecutionNode = LIST_VALUE(CurrentEntry,
                                   SHELL_EXECUTION_NODE,
                                   ListEntry);

        CurrentEntry = CurrentEntry->Next;

        //
        // If an if/while/until is found with the body executing, an And-Or
        // is found, or a pipeline is found with a bang then don't exit, just
        // return.
        //

        Type = ExecutionNode->Node->Type;
        if (((Type == ShellNodeIf) || (Type == ShellNodeWhile) ||
             (Type == ShellNodeUntil) || (Type == ShellNodeAndOr)) &&
            ((ExecutionNode->Flags & SHELL_EXECUTION_BODY) == 0)) {

            return;
        }

        if ((Type == ShellNodePipeline) &&
            (ExecutionNode->Node->U.Pipeline.Bang != FALSE)) {

            return;
        }
    }

    //
    // None of the conditions were met, so exit this shell.
    //

    Shell->Exited = TRUE;
    return;
}

BOOL
ShApplyRedirections (
    PSHELL Shell,
    PSHELL_EXECUTION_NODE ExecutionNode
    )

/*++

Routine Description:

    This routine applies any redirections to the current command.

Arguments:

    Shell - Supplies a pointer to the shell.

    ExecutionNode - Supplies a pointer to the execution node to apply
        redirections of.

Return Value:

    TRUE on success.

--*/

{

    PSHELL_ACTIVE_REDIRECT ActiveRedirect;
    PSTR AfterScan;
    PLIST_ENTRY CurrentEntry;
    PSTR DocumentText;
    UINTN DocumentTextSize;
    PSTR ExpandedFileName;
    UINTN ExpandedFileNameSize;
    PSTR ExpandedString;
    UINTN ExpandedStringSize;
    PSHELL_HERE_DOCUMENT HereDocument;
    INT NewDescriptor;
    INT NewDescriptorAnywhere;
    INT OpenFlags;
    ULONG Options;
    INT OriginalDescriptor;
    unsigned long PathSize;
    INT Pipe[2];
    PSHELL_IO_REDIRECT Redirect;
    BOOL Result;
    INT SourceFileNumber;
    SHELL_IO_REDIRECTION_TYPE Type;
    INT WriteCopy;

    ActiveRedirect = NULL;
    ExpandedFileName = NULL;
    ExpandedFileNameSize = 0;
    ExpandedString = NULL;
    Pipe[0] = -1;
    Pipe[1] = -1;

    //
    // Loop through all the redirections.
    //

    CurrentEntry = ExecutionNode->Node->RedirectList.Next;
    while (CurrentEntry != &(ExecutionNode->Node->RedirectList)) {
        Redirect = LIST_VALUE(CurrentEntry, SHELL_IO_REDIRECT, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        Type = Redirect->Type;

        //
        // Allocate an active redirect entry.
        //

        ActiveRedirect = malloc(sizeof(SHELL_ACTIVE_REDIRECT));
        if (ActiveRedirect == NULL) {
            Result = FALSE;
            goto ApplyRedirectionsEnd;
        }

        ActiveRedirect->ChildProcessId = -1;

        //
        // Expand the file name.
        //

        if (Redirect->FileName != NULL) {
            Options = SHELL_EXPANSION_OPTION_NO_FIELD_SPLIT;
            Result = ShPerformExpansions(Shell,
                                         Redirect->FileName,
                                         Redirect->FileNameSize,
                                         Options,
                                         &ExpandedFileName,
                                         &ExpandedFileNameSize,
                                         NULL,
                                         NULL);

            if (Result == FALSE) {
                goto ApplyRedirectionsEnd;
            }

            //
            // Let the OS layer play with the path if it wants.
            //

            PathSize = ExpandedFileNameSize;
            Result = ShFixUpPath(&ExpandedFileName, &PathSize);
            if (Result == FALSE) {
                goto ApplyRedirectionsEnd;
            }
        }

        //
        // Perform normal file redirections.
        //

        if ((Type == ShellRedirectRead) ||
            (Type == ShellRedirectWrite) ||
            (Type == ShellRedirectAppend) ||
            (Type == ShellRedirectReadWrite) ||
            (Type == ShellRedirectClobber)) {

            OpenFlags = O_CREAT | O_BINARY;
            if ((Shell->Options & SHELL_OPTION_NO_CLOBBER) != 0) {
                OpenFlags |= O_EXCL;
            }

            if (Type == ShellRedirectRead) {
                OpenFlags |= O_RDONLY;
                OpenFlags &= ~(O_CREAT | O_EXCL);

            } else if (Type == ShellRedirectWrite) {
                OpenFlags |= O_WRONLY | O_TRUNC;

            } else if (Type == ShellRedirectAppend) {
                OpenFlags |= O_WRONLY | O_APPEND;

            } else if (Type == ShellRedirectReadWrite) {
                OpenFlags |= O_RDWR;

            } else if (Type == ShellRedirectClobber) {
                OpenFlags |= O_WRONLY | O_TRUNC;
                OpenFlags &= ~O_EXCL;
            }

            //
            // Open up the file.
            //

            NewDescriptorAnywhere = SwOpen(ExpandedFileName,
                                           OpenFlags,
                                           SHELL_FILE_CREATION_MASK);

            if (NewDescriptorAnywhere < 0) {
                PRINT_ERROR("sh: Unable to open redirection file %s: %s.\n",
                            ExpandedFileName,
                            strerror(errno));

                Result = FALSE;
                goto ApplyRedirectionsEnd;
            }

            //
            // Copy the original descriptor somewhere, then close the
            // descriptor and copy the newly opened file into it.
            //

            OriginalDescriptor = ShDup(Shell, Redirect->FileNumber, FALSE);
            if (NewDescriptorAnywhere != Redirect->FileNumber) {
                NewDescriptor = ShDup2(Shell,
                                       NewDescriptorAnywhere,
                                       Redirect->FileNumber);

                if (NewDescriptor < 0) {
                    Result = FALSE;
                    goto ApplyRedirectionsEnd;
                }

                ShClose(Shell, NewDescriptorAnywhere);
            }

        //
        // Perform redirections from other file descriptors.
        //

        } else if ((Type == ShellRedirectReadFromDescriptor) ||
                   (Type == ShellRedirectWriteToDescriptor)) {

            //
            // If the source file number evaluates to -, then the file number is
            // closed.
            //

            if (strcmp(ExpandedFileName, "-") == 0) {
                OriginalDescriptor = ShDup(Shell, Redirect->FileNumber, FALSE);
                ShClose(Shell, Redirect->FileNumber);

            } else {
                SourceFileNumber = strtol(ExpandedFileName, &AfterScan, 10);
                if ((SourceFileNumber < 0) || (AfterScan == ExpandedFileName)) {
                    PRINT_ERROR("sh: Bad file descriptor number '%s'.",
                                ExpandedFileName);

                    Result = FALSE;
                    goto ApplyRedirectionsEnd;
                }

                //
                // Copy the original descriptor, then close the destination and
                // copy the source in there.
                //

                OriginalDescriptor = ShDup(Shell, Redirect->FileNumber, FALSE);
                if (Redirect->FileNumber != SourceFileNumber) {
                    NewDescriptor = ShDup2(Shell,
                                           SourceFileNumber,
                                           Redirect->FileNumber);

                    if (NewDescriptor < 0) {
                        PRINT_ERROR("sh: Unable to duplicate file %d.\n",
                                    SourceFileNumber);

                        Result = FALSE;
                        goto ApplyRedirectionsEnd;
                    }
                }
            }

        //
        // Perform a redirection from a here document.
        //

        } else if ((Type == ShellRedirectHereDocument) ||
                   (Type == ShellRedirectStrippedHereDocument)) {

            HereDocument = Redirect->HereDocument;
            DocumentText = HereDocument->Document;
            DocumentTextSize = HereDocument->DocumentSize;

            //
            // Perform expansions on the here document.
            //

            if (Redirect->HereDocument->EndWordWasQuoted == FALSE) {
                Options = SHELL_EXPANSION_OPTION_NO_TILDE_EXPANSION |
                          SHELL_EXPANSION_OPTION_NO_FIELD_SPLIT;

                Result = ShPerformExpansions(Shell,
                                             DocumentText,
                                             DocumentTextSize,
                                             Options,
                                             &ExpandedString,
                                             &ExpandedStringSize,
                                             NULL,
                                             NULL);

                if (Result == FALSE) {
                    goto ApplyRedirectionsEnd;
                }

                DocumentText = ExpandedString;
                DocumentTextSize = ExpandedStringSize;
            }

            //
            // Create a pipe for the here document and wire up the file
            // descriptor to the read end of the pipe.
            //

            Result = ShCreatePipe(Pipe);
            if (Result == FALSE) {
                goto ApplyRedirectionsEnd;
            }

            OriginalDescriptor = ShDup(Shell, Redirect->FileNumber, FALSE);
            if (OriginalDescriptor == -1) {
                Result = FALSE;
                goto ApplyRedirectionsEnd;
            }

            //
            // Copy the write descriptor out of range of the shell standard
            // descriptors, since on Windows the write side is just a thread,
            // so it stays open.
            //

            if (SwForkSupported == 0) {
                WriteCopy = ShDup(Shell, Pipe[1], 0);
                ShClose(Shell, Pipe[1]);
                Pipe[1] = WriteCopy;
            }

            //
            // Launch the subprocess or thread to feed the document text into
            // the descriptor.
            //

            assert(DocumentTextSize != 0);

            DocumentTextSize -= 1;
            Result = ShPushInputText(DocumentText, DocumentTextSize, Pipe);
            if (Result < 0) {
                goto ApplyRedirectionsEnd;
            }

            Pipe[1] = -1;
            ShDup2(Shell, Pipe[0], Redirect->FileNumber);
            ShClose(Shell, Pipe[0]);
            Pipe[0] = -1;
            if (Result > 0) {
                ActiveRedirect->ChildProcessId = Result;
            }

        } else {

            assert(FALSE);

            Result = FALSE;
            goto ApplyRedirectionsEnd;
        }

        if (Redirect->FileNumber == STDOUT_FILENO) {
            fflush(stdout);

        } else if (Redirect->FileNumber == STDERR_FILENO) {
            fflush(stderr);
        }

        //
        // Initialize the active redirect so that the original descriptor can
        // be restored.
        //

        ActiveRedirect->FileNumber = Redirect->FileNumber;
        ActiveRedirect->OriginalDescriptor = OriginalDescriptor;
        INSERT_BEFORE(&(ActiveRedirect->ListEntry),
                      &(ExecutionNode->ActiveRedirectList));

        ActiveRedirect = NULL;

        //
        // Free the expanded file name.
        //

        if (ExpandedFileName != NULL) {
            free(ExpandedFileName);
            ExpandedFileName = NULL;
        }
    }

    Result = TRUE;

ApplyRedirectionsEnd:
    if (Pipe[0] != -1) {
        ShClose(Shell, Pipe[0]);
    }

    if (Pipe[1] != -1) {
        ShClose(Shell, Pipe[1]);
    }

    if (ExpandedString != NULL) {
        free(ExpandedString);
    }

    if (ExpandedFileName != NULL) {
        free(ExpandedFileName);
    }

    if (Result == FALSE) {
        if (ActiveRedirect != NULL) {
            free(ActiveRedirect);
        }
    }

    return Result;
}

