/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    signals.c

Abstract:

    This module implements signal handling functionality for the shell.

Author:

    Evan Green 14-Jun-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "sh.h"
#include "shparse.h"
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "../swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _SHELL_SIGNAL_STRING {
    PSTR Name;
    SHELL_SIGNAL Number;
} SHELL_SIGNAL_STRING, *PSHELL_SIGNAL_STRING;

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
ShSetSignalAction (
    PSHELL Shell,
    SHELL_SIGNAL Signal,
    PSTR ActionString
    );

SHELL_SIGNAL
ShGetSignalFromArgument (
    PSTR Argument
    );

SHELL_SIGNAL
ShGetSignalNumberFromName (
    PSTR Name
    );

PSTR
ShGetSignalNameFromNumber (
    SHELL_SIGNAL Number
    );

PSHELL_SIGNAL_ACTION
ShGetSignalAction (
    PSHELL Shell,
    SHELL_SIGNAL SignalNumber
    );

PSHELL_SIGNAL_ACTION
ShCreateSignalAction (
    SHELL_SIGNAL SignalNumber,
    PSTR ActionString,
    UINTN ActionStringSize
    );

VOID
ShDestroySignalAction (
    PSHELL_SIGNAL_ACTION SignalAction
    );

BOOL
ShPrintTraps (
    PSHELL Shell
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store an array of which signals are pending.
//

volatile int ShPendingSignals[ShellSignalCount];

//
// Store the count of signals, which is used for quick detection of whether or
// not new signals have come in.
//

volatile int ShSignalCount;

//
// Define the translation between signal names and numbers.
//

SHELL_SIGNAL_STRING ShSignalNames[] = {
    {"EXIT", ShellSignalOnExit},
    {"HUP", ShellSignalHangup},
    {"INT", ShellSignalInterrupt},
    {"QUIT", ShellSignalQuit},
    {"ILL", ShellSignalIllegalInstruction},
    {"TRAP", ShellSignalTrap},
    {"ABRT", ShellSignalAbort},
    {"FPE", ShellSignalFloatingPointException},
    {"KILL", ShellSignalKill},
    {"BUS", ShellSignalBusError},
    {"SEGV", ShellSignalSegmentationFault},
    {"SYS", ShellSignalBadSystemCall},
    {"PIPE", ShellSignalPipe},
    {"ALRM", ShellSignalAlarm},
    {"TERM", ShellSignalTerminate},
    {"URG", ShellSignalUrgentData},
    {"STOP", ShellSignalStop},
    {"TSTP", ShellSignalTerminalStop},
    {"CONT", ShellSignalContinue},
    {"CHLD", ShellSignalChild},
    {"TTIN", ShellSignalTerminalInput},
    {"TTOU", ShellSignalTerminalOutput},
    {"XCPU", ShellSignalCpuTime},
    {"XFSZ", ShellSignalFileSize},
    {"VTALRM", ShellSignalVirtualTimeAlarm},
    {"PROF", ShellSignalProfiling},
    {"WINCH", ShellSignalWindowChange},
    {"USR1", ShellSignalUser1},
    {"USR2", ShellSignalUser2},
    {NULL, 0},
};

//
// ------------------------------------------------------------------ Functions
//

void
ShSignalHandler (
    int SignalNumber
    )

/*++

Routine Description:

    This routine is called when a signal comes in. It marks the signal as
    pending and makes an effort to get out as quickly as possible. The signal
    execution environment is fairly hostile, so there's not much that could be
    done anyway.

Arguments:

    SignalNumber - Supplies the signal number of the signal that came in.

Return Value:

    None.

--*/

{

    if (SignalNumber >= ShellSignalCount) {
        PRINT_ERROR("Unexpected signal %d came in.\n", SignalNumber);
        return;
    }

    //
    // Mark the specific signal as pending, and up the total number of signals
    // so the topmost shell knows to take a closer look.
    //

    ShPendingSignals[SignalNumber] = 1;
    ShSignalCount += 1;
    return;
}

VOID
ShInitializeSignals (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine initializes the shell signals.

Arguments:

    Shell - Supplies a pointer to the shell.

Return Value:

    None.

--*/

{

    if ((Shell->Options & SHELL_OPTION_INTERACTIVE) != 0) {
        ShSetSignalDisposition(ShellSignalInterrupt,
                               ShellSignalDispositionTrap);

        ShSetSignalDisposition(ShellSignalQuit, ShellSignalDispositionTrap);

    } else {
        ShSetSignalDisposition(ShellSignalInterrupt,
                               ShellSignalDispositionDefault);

        ShSetSignalDisposition(ShellSignalQuit, ShellSignalDispositionDefault);
    }

    return;
}

VOID
ShCheckForSignals (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine checks for any pending signals and runs their associated
    actions if they're set.

Arguments:

    Shell - Supplies a pointer to the shell to run any pending signals on.

Return Value:

    None.

--*/

{

    PSHELL_SIGNAL_ACTION Action;
    pid_t Child;
    PSTR EvalArguments[2];
    INT SavedReturnValue;
    INT Signal;

    //
    // If fork is supported, perform wait to clean up any asynchronous nodes.
    //

    if (SwForkSupported != FALSE) {
        while (TRUE) {
            Child = SwWaitPid(-1, 1, NULL);
            if (Child <= 0) {
                break;
            }
        }
    }

    if (ShSignalCount == Shell->LastSignalCount) {
        return;
    }

    Shell->LastSignalCount = ShSignalCount;
    for (Signal = 0; Signal < ShellSignalCount; Signal += 1) {
        if (ShPendingSignals[Signal] == 0) {
            continue;
        }

        ShPendingSignals[Signal] = 0;
        Action = ShGetSignalAction(Shell, Signal);
        if ((Action != NULL) && (Action->ActionSize > 1)) {
            SavedReturnValue = Shell->LastReturnValue;
            EvalArguments[0] = "eval";
            EvalArguments[1] = Action->Action;
            ShBuiltinEval(Shell, 2, EvalArguments);
            Shell->LastReturnValue = SavedReturnValue;
        }
    }

    return;
}

VOID
ShRunAtExitSignal (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine runs the "exit" trap handler on signal 0 if one was set.

Arguments:

    Shell - Supplies a pointer to the shell to run any pending signals on.

Return Value:

    None.

--*/

{

    if (Shell->SkipExitSignal != FALSE) {
        return;
    }

    if (ShGetSignalAction(Shell, ShellSignalOnExit) != NULL) {

        assert(Shell->Exited != FALSE);

        Shell->Exited = FALSE;

        //
        // Avoid calling the at exit signal again if the at exit signal calls
        // exit.
        //

        Shell->SkipExitSignal = TRUE;

        //
        // This is not a real signal so doing this won't race with incrementing
        // any actual signals. The total signal count might, but it's ok if
        // it only gets incremented by 1 instead of 2, the only real important
        // thing is that it's different from what's in the shell.
        //

        ShPendingSignals[ShellSignalOnExit] += 1;
        ShSignalCount += 1;
        ShCheckForSignals(Shell);
        Shell->Exited = TRUE;
    }

    return;
}

VOID
ShSetAllSignalDispositions (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine sets all the signal dispositions in accordance with the
    given shell. This is usually called when entering or exiting a subshell.

Arguments:

    Shell - Supplies a pointer to the shell to operate in accordance with.

Return Value:

    None.

--*/

{

    PSHELL_SIGNAL_ACTION Action;
    SHELL_SIGNAL Signal;

    for (Signal = 1; Signal < ShellSignalCount; Signal += 1) {
        Action = ShGetSignalAction(Shell, Signal);

        //
        // Trap the signal if one of the following is true:
        // 1) There's a requested action.
        // 2) It's an interactive shell and the signal is SIGINT or SIGQUIT.
        //

        if ((Action != NULL) ||
            (((Shell->Options & SHELL_OPTION_INTERACTIVE) != 0) &&
             ((Signal == ShellSignalInterrupt) ||
              (Signal == ShellSignalQuit)))) {

            ShSetSignalDisposition(Signal, ShellSignalDispositionTrap);

        } else {
            ShSetSignalDisposition(Signal, ShellSignalDispositionDefault);
        }
    }

    return;
}

VOID
ShDestroySignalActionList (
    PLIST_ENTRY ActionList
    )

/*++

Routine Description:

    This routine destroys all the signal actions on the given signal action
    list.

Arguments:

    ActionList - Supplies a pointer to the head of the action list.

Return Value:

    None.

--*/

{

    PSHELL_SIGNAL_ACTION Action;

    while (LIST_EMPTY(ActionList) == FALSE) {
        Action = LIST_VALUE(ActionList->Next, SHELL_SIGNAL_ACTION, ListEntry);
        LIST_REMOVE(&(Action->ListEntry));
        ShDestroySignalAction(Action);
    }

    return;
}

INT
ShBuiltinTrap (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine implements the trap command, which handles signal catching
    within the shell.

Arguments:

    Shell - Supplies a pointer to the shell.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument.

Return Value:

    0 on success.

    1 on failure.

--*/

{

    PSHELL_SIGNAL_ACTION Action;
    ULONG ArgumentIndex;
    BOOL Result;
    SHELL_SIGNAL Signal;
    ULONG StartIndex;

    StartIndex = 1;

    //
    // A -- at the beginning is basically ignored as an argument.
    //

    if ((ArgumentCount > 1) && (strcmp(Arguments[1], "--") == 0)) {
        StartIndex = 2;
    }

    if (ArgumentCount <= StartIndex) {
        Result = ShPrintTraps(Shell);
        if (Result == FALSE) {
            return 1;
        }

        return 0;
    }

    //
    // If there's only one argument, then delete the action for the given
    // signal.
    //

    if (ArgumentCount == StartIndex + 1) {
        Signal = ShGetSignalFromArgument(Arguments[StartIndex]);
        if (Signal == ShellSignalInvalid) {
            PRINT_ERROR("trap: %s: bad trap\n", Arguments[StartIndex]);
            return 1;
        }

        Action = ShGetSignalAction(Shell, Signal);
        if (Action != NULL) {
            LIST_REMOVE(&(Action->ListEntry));
            ShDestroySignalAction(Action);
        }

        return 0;
    }

    //
    // Loop through all the signal numbers.
    //

    for (ArgumentIndex = StartIndex + 1;
         ArgumentIndex < ArgumentCount;
         ArgumentIndex += 1) {

        Signal = ShGetSignalFromArgument(Arguments[ArgumentIndex]);
        if (Signal == ShellSignalInvalid) {
            PRINT_ERROR("trap: %s: bad trap\n", Arguments[ArgumentIndex]);
            return 1;
        }

        Result = ShSetSignalAction(Shell, Signal, Arguments[StartIndex]);
        if (Result == FALSE) {
            return 1;
        }
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
ShSetSignalAction (
    PSHELL Shell,
    SHELL_SIGNAL Signal,
    PSTR ActionString
    )

/*++

Routine Description:

    This routine sets the signal action for the given signal.

Arguments:

    Shell - Supplies a pointer to the shell where the action will be set.

    Signal - Supplies the signal number.

    ActionString - Supplies the action to perform. If this is "-" the signal
        will be ignored. Otherwise the action word will be "eval"ed when the
        signal occurs.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSHELL_SIGNAL_ACTION Action;
    UINTN ActionStringSize;
    BOOL Result;

    assert(Signal < ShellSignalCount);

    Action = ShGetSignalAction(Shell, Signal);

    //
    // If the action is "-", delete any existing action and reset the signal
    // to its default action.
    //

    if (strcmp(ActionString, "-") == 0) {
        if (Action != NULL) {
            LIST_REMOVE(&(Action->ListEntry));
            ShDestroySignalAction(Action);
        }

        //
        // Continue to trap SIGINT and SIGQUIT on interactive shells.
        //

        if (((Shell->Options & SHELL_OPTION_INTERACTIVE) != 0) &&
            ((Signal == ShellSignalInterrupt) || (Signal == ShellSignalTrap))) {

            Result = ShSetSignalDisposition(Signal,
                                            ShellSignalDispositionTrap);

        } else {
            Result = ShSetSignalDisposition(Signal,
                                            ShellSignalDispositionDefault);
        }

        if (Result == FALSE) {
            return FALSE;
        }

    //
    // Set the signal to the given action word.
    //

    } else {
        ActionStringSize = strlen(ActionString) + 1;
        if (Action != NULL) {
            if (Action->Action != NULL) {
                free(Action->Action);
            }

            Action->Action = SwStringDuplicate(ActionString, ActionStringSize);
            if (Action->Action == NULL) {
                LIST_REMOVE(&(Action->ListEntry));
                ShDestroySignalAction(Action);
                return FALSE;
            }

            Action->ActionSize = ActionStringSize;

        } else {
            Action = ShCreateSignalAction(Signal,
                                          ActionString,
                                          ActionStringSize);

            if (Action == NULL) {
                return FALSE;
            }

            INSERT_BEFORE(&(Action->ListEntry), &(Shell->SignalActionList));
        }

        if (Signal != ShellSignalOnExit) {
            Result = ShSetSignalDisposition(Signal, ShellSignalDispositionTrap);
            if (Result == FALSE) {
                return FALSE;
            }
        }
    }

    return TRUE;
}

SHELL_SIGNAL
ShGetSignalFromArgument (
    PSTR Argument
    )

/*++

Routine Description:

    This routine returns the signal number for the given signal name.

Arguments:

    Argument - Supplies the name of the signal.

Return Value:

    Returns the signal number on success.

    ShellSignalInvalid if the name did not match any signals.

--*/

{

    PSTR AfterScan;
    SHELL_SIGNAL Signal;

    Signal = ShGetSignalNumberFromName(Argument);
    if (Signal != ShellSignalInvalid) {
        return Signal;
    }

    Signal = strtoul(Argument, &AfterScan, 10);
    if (AfterScan == Argument) {
        return ShellSignalInvalid;
    }

    if (ShGetSignalNameFromNumber(Signal) == NULL) {
        return ShellSignalInvalid;
    }

    return Signal;
}

SHELL_SIGNAL
ShGetSignalNumberFromName (
    PSTR Name
    )

/*++

Routine Description:

    This routine returns the signal number for the given signal name.

Arguments:

    Name - Supplies the name of the signal.

Return Value:

    Returns the signal number on success.

    ShellSignalInvalid if the name did not match any signals.

--*/

{

    ULONG Index;

    Index = 0;
    while (ShSignalNames[Index].Name != NULL) {
        if (strcasecmp(Name, ShSignalNames[Index].Name) == 0) {
            return ShSignalNames[Index].Number;
        }

        Index += 1;
    }

    return ShellSignalInvalid;
}

PSTR
ShGetSignalNameFromNumber (
    SHELL_SIGNAL Number
    )

/*++

Routine Description:

    This routine returns the signal name for the given signal number.

Arguments:

    Number - Supplies the signal number.

Return Value:

    Returns the name string on success. The caller does not own this memory,
    and should not modify or free it.

    NULL if the given number is not valid.

--*/

{

    ULONG Index;

    Index = 0;
    while (ShSignalNames[Index].Name != NULL) {
        if (ShSignalNames[Index].Number == Number) {
            return ShSignalNames[Index].Name;
        }

        Index += 1;
    }

    return NULL;
}

PSHELL_SIGNAL_ACTION
ShGetSignalAction (
    PSHELL Shell,
    SHELL_SIGNAL SignalNumber
    )

/*++

Routine Description:

    This routine attempts to return the signal action structure associated
    with the given signal number.

Arguments:

    Shell - Supplies a pointer to the shell whose actions should be searched.

    SignalNumber - Supplies the signal number.

Return Value:

    Returns a pointer to the signal action structure on success.

    NULL if the signal has no action.

--*/

{

    PSHELL_SIGNAL_ACTION Action;
    PLIST_ENTRY CurrentEntry;

    CurrentEntry = Shell->SignalActionList.Next;
    while (CurrentEntry != &(Shell->SignalActionList)) {
        Action = LIST_VALUE(CurrentEntry, SHELL_SIGNAL_ACTION, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (Action->SignalNumber == SignalNumber) {
            return Action;
        }
    }

    return NULL;
}

PSHELL_SIGNAL_ACTION
ShCreateSignalAction (
    SHELL_SIGNAL SignalNumber,
    PSTR ActionString,
    UINTN ActionStringSize
    )

/*++

Routine Description:

    This routine creates a shell action structure.

Arguments:

    SignalNumber - Supplies the signal number this action is associated with.

    ActionString - Supplies a pointer to the action string to eval when the
        signal occurs. A copy of this string will be made.

    ActionStringSize - Supplies the size of the action string in bytes including
        the null terminator.

Return Value:

    Returns a pointer to the shiny new action structure on success.

    NULL on failure.

--*/

{

    PSHELL_SIGNAL_ACTION Action;
    BOOL Result;

    Result = FALSE;
    Action = malloc(sizeof(SHELL_SIGNAL_ACTION));
    if (Action == NULL) {
        goto CreateSignalActionEnd;
    }

    memset(Action, 0, sizeof(SHELL_SIGNAL_ACTION));
    Action->SignalNumber = SignalNumber;
    Action->Action = SwStringDuplicate(ActionString, ActionStringSize);
    if (Action->Action == NULL) {
        goto CreateSignalActionEnd;
    }

    Action->ActionSize = ActionStringSize;
    Result = TRUE;

CreateSignalActionEnd:
    if (Result == FALSE) {
        if (Action != NULL) {
            if (Action->Action != NULL) {
                free(Action->Action);
            }

            free(Action);
            Action = NULL;
        }
    }

    return Action;
}

VOID
ShDestroySignalAction (
    PSHELL_SIGNAL_ACTION SignalAction
    )

/*++

Routine Description:

    This routine destroys a shell signal action structure. It assumes it has
    already been removed from its list.

Arguments:

    SignalAction - Supplies a pointer to the action to destroy.

Return Value:

    None.

--*/

{

    if (SignalAction->Action != NULL) {
        free(SignalAction->Action);
    }

    free(SignalAction);
}

BOOL
ShPrintTraps (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine prints all the shell traps.

Arguments:

    Shell - Supplies a pointer to the shell.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSHELL_SIGNAL_ACTION Action;
    PLIST_ENTRY CurrentEntry;
    PSTR QuotedString;
    UINTN QuotedStringSize;
    BOOL Result;
    PSTR SignalName;

    CurrentEntry = Shell->SignalActionList.Next;
    while (CurrentEntry != &(Shell->SignalActionList)) {
        Action = LIST_VALUE(CurrentEntry, SHELL_SIGNAL_ACTION, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        Result = ShStringFormatForReentry(Action->Action,
                                          Action->ActionSize,
                                          &QuotedString,
                                          &QuotedStringSize);

        if (Result == FALSE) {
            return FALSE;
        }

        SignalName = ShGetSignalNameFromNumber(Action->SignalNumber);

        assert(SignalName != NULL);

        printf("trap -- %s %s\n", QuotedString, SignalName);
        free(QuotedString);
    }

    return TRUE;
}

