/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    sh.c

Abstract:

    This module implements the shell application.

Author:

    Evan Green 5-Jun-2013

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "sh.h"
#include "shparse.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define SHELL_PS1_INTERACTIVE_DEFAULT "\\w\\$ "

#define SH_USAGE                                                               \
    "usage: sh [-abCefhimnuvx] [-o option] command_file [argument...]\n"       \
    "       sh -c [-abCefhimnuvx] command_string [command_name [argument...]]" \
    "\n"                                                                       \
    "       sh -s [-abCefhimnuvx] [argument]\n"                                \
    "The sh utility provides a basic POSIX shell. Basic forms are:\n"          \
    "  sh ... command_file - Read shell commands from the given file.\n"       \
    "  sh ... -c command_string - Interpret the given command string in the "  \
    "shell.\n"                                                                 \
    "  sh ... -s - Read commands from standard in. This is the default.\n\n"   \
    "Options can be turned on by specifying -abCefhimnuvx or -o <option>.\n"   \
    "Options can be turned off by using +abCefhimnuvx or +o <option>.\n"       \
    "Options are:\n"                                                           \
    "  -a (allexport) -- Set the export attribute to any variable \n"          \
    "        assignment of shell-wide scope.\n"                                \
    "  -b (notify) -- Enables asynchronous background notifications.\n"        \
    "  -C -- Do not clobber existing files with the > redirection operator.\n" \
    "  -d -- Debug mode. Prints the lexing and parsing of shell commands.\n"   \
    "  -e (errexit) -- Exit the shell if any command returns a non-zero \n"    \
    "        status.\n"                                                        \
    "  -f (noglob) -- Disables pathname expansions.\n"                         \
    "  -h -- Cache utility paths invoked by functions.\n"                      \
    "  -i -- Treat the shell as interactive.\n"                                \
    "  -m -- Run all jobs in their own process groups.\n"                      \
    "  -n (noexec) -- Read but do not execute commands (ignored if \n"         \
    "        interactive).\n"                                                  \
    "  -o -- Sets a long-form option (clear on +o).\n"                         \
    "  -u (nounset) -- Print a message to standard error whenever an \n"       \
    "        attempt is made to expand an unset variable and immediately \n"   \
    "        exit (except if interactive).\n"                                  \
    "  -v (verbose) -- Write all input to standard out as it is read.\n"       \
    "  -x (xtrace) -- Write a trace of each command after it expands but \n"   \
    "        before it executes.\n"                                            \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Show the application version information and exit.\n\n"    \

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _SHELL_OPTION_STRING {
    PSTR String;
    CHAR Character;
    ULONG Option;
} SHELL_OPTION_STRING, *PSHELL_OPTION_STRING;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
ShRunEnvVariable (
    PSHELL Shell
    );

//
// -------------------------------------------------------------------- Globals
//

SHELL_OPTION_STRING ShOptionStrings[] = {
    {"allexport", 'a', SHELL_OPTION_EXPORT_ALL},
    {"errexit", 'e', SHELL_OPTION_EXIT_ON_FAILURE},
    {"ignoreeof", 0, SHELL_OPTION_IGNORE_EOF},
    {"monitor", 'm', SHELL_OPTION_RUN_JOBS_IN_SEPARATE_PROCESS_GROUP},
    {"noclobber", 'c', SHELL_OPTION_NO_CLOBBER},
    {"noglob", 'f', SHELL_OPTION_NO_PATHNAME_EXPANSION},
    {"noexec", 'n', SHELL_OPTION_NO_EXECUTE},
    {"nolog", 0, SHELL_OPTION_NO_COMMAND_HISTORY},
    {"notify", 'b', SHELL_OPTION_ASYNCHRONOUS_JOB_NOTIFICATION},
    {"nounset", 'u', SHELL_OPTION_EXIT_ON_UNSET_VARIABLE},
    {"verbose", 'v', SHELL_OPTION_DISPLAY_INPUT},
    {"interactive", 'i', SHELL_INTERACTIVE_OPTIONS},
    {"xtrace", 'x', SHELL_OPTION_TRACE_COMMAND},
    {"debug", 'd', SHELL_OPTION_DEBUG},
    {"stdin", 's', SHELL_OPTION_READ_FROM_STDIN},
    {NULL, 'h', SHELL_OPTION_LOCATE_UTILITIES_IN_DECLARATION},
};

//
// ------------------------------------------------------------------ Functions
//

INT
ShMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine implements the main entry point for the shell app.

Arguments:

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies an array of pointers to strings representing the
        arguments.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR Argument;
    ULONG ArgumentIndex;
    BOOL ArgumentIsInput;
    UINTN ArgumentSize;
    INT InputDescriptor;
    INT InputDescriptorHigh;
    BOOL Result;
    INT ReturnValue;
    BOOL Set;
    PSHELL Shell;
    INT StandardErrorCopy;

    InputDescriptor = -1;
    InputDescriptorHigh = -1;
    srand(time(NULL));
    ReturnValue = ENOMEM;
    Shell = ShCreateShell(NULL, 0);
    if (Shell == NULL) {
        PRINT_ERROR("Error: Unable to allocate shell.\n");
        goto MainEnd;
    }

    StandardErrorCopy = ShDup(Shell, STDERR_FILENO, FALSE);
    if (StandardErrorCopy >= 0) {
        Shell->NonStandardError = fdopen(StandardErrorCopy, "w");
        if (Shell->NonStandardError == NULL) {
            ShClose(Shell, StandardErrorCopy);
        }
    }

    //
    // Loop through all the options.
    //

    ArgumentIsInput = FALSE;
    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        Argument = Arguments[ArgumentIndex];

        //
        // First look out for the longform options.
        //

        if ((strcmp(Argument, "-o") == 0) || (strcmp(Argument, "+o") == 0)) {
            Set = TRUE;
            if (Argument[0] == '+') {
                Set = FALSE;
            }

            if (ArgumentIndex == ArgumentCount - 1) {
                Argument = "";
                ArgumentSize = 1;

            } else {
                ArgumentIndex += 1;
                Argument = Arguments[ArgumentIndex];
                ArgumentSize = strlen(Arguments[ArgumentIndex + 1]) + 1;
            }

            Result = ShSetOptions(Shell,
                                  Argument,
                                  ArgumentSize,
                                  TRUE,
                                  Set);

            if (Result == FALSE) {
                PRINT_ERROR("Error: Unknown option %s.\n",
                            Arguments[ArgumentIndex + 1]);

                ReturnValue = EINVAL;
                goto MainEnd;
            }

            continue;
        }

        //
        // Stop processing for --.
        //

        if (strcmp(Argument, "--") == 0) {
            break;
        }

        //
        // Look for help or version if they're the only arguments.
        //

        if (ArgumentCount == 2) {
            if (strcmp(Argument, "--help") == 0) {
                printf(SH_USAGE);
                Result = 1;
                goto MainEnd;

            } else if (strcmp(Argument, "--version") == 0) {
                SwPrintVersion(SH_VERSION_MAJOR, SH_VERSION_MINOR, REVISION);
                Result = 1;
                goto MainEnd;
            }
        }

        //
        // If -c is found, then the command comes directly from the command
        // line.
        //

        if (strcmp(Argument, "-c") == 0) {
            ArgumentIsInput = TRUE;

        } else if ((Argument[0] == '-') || (Argument[0] == '+')) {
            Result = ShSetOptions(Shell,
                                  Argument,
                                  strlen(Argument) + 1,
                                  FALSE,
                                  FALSE);

            if (Result == FALSE) {
                ReturnValue = EINVAL;
                goto MainEnd;
            }

        //
        // This is either a command file, command string, or argument, so stop
        // processing options.
        //

        } else {
            break;
        }
    }

    //
    // If the next command is the input, set that up directly.
    //

    if (ArgumentIsInput != FALSE) {
        if (ArgumentIndex == ArgumentCount) {
            PRINT_ERROR("Error: -c requires an argument.");
            ReturnValue = EINVAL;
            goto MainEnd;
        }

        Argument = Arguments[ArgumentIndex];
        Shell->Lexer.InputBufferSize = strlen(Argument) + 1;
        Shell->Lexer.InputBuffer = SwStringDuplicate(
                                                 Argument,
                                                 Shell->Lexer.InputBufferSize);

        if (Shell->Lexer.InputBuffer == NULL) {
            goto MainEnd;
        }

        Shell->Lexer.InputBufferCapacity = Shell->Lexer.InputBufferSize;
        ArgumentIndex += 1;

        //
        // The following argument if provided is the command name.
        //

        if (ArgumentIndex < ArgumentCount) {
            Argument = Arguments[ArgumentIndex];
            ArgumentIndex += 1;

        //
        // If no argument zero was provided, then use this argument zero.
        //

        } else {
            Argument = Arguments[0];
        }

        Shell->CommandNameSize = strlen(Argument) + 1;
        Shell->CommandName = SwStringDuplicate(Argument,
                                               Shell->CommandNameSize);

        if (Shell->CommandName == NULL) {
            goto MainEnd;
        }

    //
    // If not reading from standard in, then the next argument is a script. If
    // there is no argument, then it's standard in.
    //

    } else if ((Shell->Options & SHELL_OPTION_READ_FROM_STDIN) == 0) {
        if (ArgumentIndex == ArgumentCount) {
            Shell->Lexer.InputFile = NULL;
            Shell->Options |= SHELL_OPTION_READ_FROM_STDIN;

        } else {
            Argument = Arguments[ArgumentIndex];
            InputDescriptor = open(Argument, O_RDONLY | O_BINARY);
            if (InputDescriptor < 0) {
                SwPrintError(errno, Argument, "Unable to open script");
                ReturnValue = SHELL_ERROR_OPEN;
                goto MainEnd;
            }

            if (InputDescriptor >= SHELL_MINIMUM_FILE_DESCRIPTOR) {
                InputDescriptorHigh = InputDescriptor;

            } else {
                InputDescriptorHigh = ShDup(Shell, InputDescriptor, FALSE);
                if (InputDescriptorHigh < 0) {
                    SwPrintError(errno, Argument, "Unable to dup");
                    ReturnValue = SHELL_ERROR_OPEN;
                    goto MainEnd;
                }

                assert(InputDescriptorHigh >= SHELL_MINIMUM_FILE_DESCRIPTOR);

                close(InputDescriptor);
            }

            InputDescriptor = -1;
            Shell->Lexer.InputFile = fdopen(InputDescriptorHigh, "rb");
            if (Shell->Lexer.InputFile == NULL) {
                PRINT_ERROR("Error: Unable to open script %s.\n", Argument);
                ReturnValue = SHELL_ERROR_OPEN;
                goto MainEnd;
            }

            InputDescriptorHigh = -1;

            //
            // Also set the first argument to the name of this script.
            //

            Shell->CommandNameSize = strlen(Argument) + 1;
            Shell->CommandName = SwStringDuplicate(Argument,
                                                   Shell->CommandNameSize);

            if (Shell->CommandName == NULL) {
                goto MainEnd;
            }

            ArgumentIndex += 1;
        }
    }

    //
    // If the command name has not yet been set, assign it to the first
    // argument.
    //

    if (Shell->CommandName == NULL) {
        Shell->CommandNameSize = strlen(Arguments[0]) + 1;
        Shell->CommandName = SwStringDuplicate(Arguments[0],
                                               Shell->CommandNameSize);

        if (Shell->CommandName == NULL) {
            goto MainEnd;
        }
    }

    //
    // Set up any remaining arguments as the positional arguments.
    //

    if (ArgumentIndex != ArgumentCount) {
        Result = ShCreateArgumentList(Arguments + ArgumentIndex,
                                      ArgumentCount - ArgumentIndex,
                                      &(Shell->ArgumentList));

        if (Result == FALSE) {
            goto MainEnd;
        }
    }

    //
    // If the input is a terminal, then mark this shell as interactive.
    //

    assert(InputDescriptor == -1);

    if ((Shell->Options & SHELL_OPTION_READ_FROM_STDIN) != 0) {
        InputDescriptor = STDIN_FILENO;

    } else if (Shell->Lexer.InputFile != NULL) {
        InputDescriptor = fileno(Shell->Lexer.InputFile);
    }

    if ((InputDescriptor != -1) && (isatty(InputDescriptor) != 0)) {
        Shell->Options |= SHELL_INTERACTIVE_OPTIONS;
        ShSetTerminalMode(Shell, TRUE);
    }

    InputDescriptor = -1;

    //
    // Give them something a little snazzier than a plain jane dollar sign
    // if it's interactive. Failure here isn't critical.
    //

    if ((Shell->Options & SHELL_OPTION_INTERACTIVE) != 0) {
        ShSetVariable(Shell,
                      SHELL_PS1,
                      sizeof(SHELL_PS1),
                      SHELL_PS1_INTERACTIVE_DEFAULT,
                      sizeof(SHELL_PS1_INTERACTIVE_DEFAULT));
    }

    ShInitializeSignals(Shell);

    //
    // Run the contents of the ENV environment variable if appropriate.
    //

    ShRunEnvVariable(Shell);

    //
    // Here we go, run that shell!
    //

    Result = ShExecute(Shell, &ReturnValue);
    if (Result == FALSE) {
        ReturnValue = EINVAL;
    }

    Shell->Exited = TRUE;
    ShRunAtExitSignal(Shell);
    ShSetTerminalMode(Shell, FALSE);
    ShRestoreOriginalSignalDispositions();

MainEnd:
    if (InputDescriptor >= 0) {
        close(InputDescriptor);
    }

    if (InputDescriptorHigh >= 0) {
        close(InputDescriptor);
    }

    if (Shell != NULL) {
        ShDestroyShell(Shell);
    }

    return ReturnValue;
}

BOOL
ShSetOptions (
    PSHELL Shell,
    PSTR String,
    UINTN StringSize,
    BOOL LongForm,
    BOOL Set
    )

/*++

Routine Description:

    This routine sets shell behavior options.

Arguments:

    Shell - Supplies a pointer to the shell.

    String - Supplies a pointer to the string containing the options.

    StringSize - Supplies the size of the string in bytes including the null
        terminator.

    LongForm - Supplies a boolean indicating if this is a long form option,
        where the name of the option is spelled out.

    Set - Supplies whether or not the longform argument is a set (-o) or clear
        (+o) operation. For non longform arguments, this parameter is ignored.

Return Value:

    TRUE on success.

    FALSE if an invalid option was supplied.

--*/

{

    UINTN Index;
    UINTN OptionCount;
    ULONG Options;
    UINTN StringIndex;
    BOOL WriteOptions;

    WriteOptions = FALSE;
    Options = 0;
    OptionCount = sizeof(ShOptionStrings) / sizeof(ShOptionStrings[0]);
    if (LongForm != FALSE) {
        for (Index = 0; Index < OptionCount; Index += 1) {
            if ((ShOptionStrings[Index].String != NULL) &&
                (strcasecmp(String, ShOptionStrings[Index].String) == 0)) {

                Options |= ShOptionStrings[Index].Option;
                break;
            }
        }

        if (Options == 0) {
            if (StringSize == 1) {
                WriteOptions = TRUE;

            } else {
                SwPrintError(0, String, "Unrecognized option");
                return FALSE;
            }
        }

    } else {
        Set = TRUE;
        if (String[0] == '+') {
            Set = FALSE;
        }

        for (StringIndex = 1; StringIndex < StringSize; StringIndex += 1) {
            if (String[StringIndex] == '\0') {
                break;
            }

            for (Index = 0; Index < OptionCount; Index += 1) {
                if ((ShOptionStrings[Index].Character != 0) &&
                    (ShOptionStrings[Index].Character == String[StringIndex])) {

                    Options |= ShOptionStrings[Index].Option;
                    break;
                }
            }

            if (Index == OptionCount) {
                PRINT_ERROR("Error: Invalid option '%c'.\n",
                            String[StringIndex]);

                return FALSE;
            }
        }
    }

    if (String[0] == '-') {
        Shell->Options |= Options;

    } else {
        Shell->Options &= ~Options;
    }

    //
    // Set or clear the various debug globals.
    //

    if ((Shell->Options & SHELL_OPTION_DEBUG) != 0) {
        ShDebugAlias = TRUE;
        ShDebugArithmeticLexer = TRUE;
        ShDebugArithmeticParser = TRUE;
        ShDebugLexer = TRUE;
        ShDebugPrintParseTree = TRUE;

    } else {
        ShDebugAlias = FALSE;
        ShDebugArithmeticLexer = FALSE;
        ShDebugArithmeticParser = FALSE;
        ShDebugLexer = FALSE;
        ShDebugPrintParseTree = FALSE;
    }

    //
    // If asked, write out all the options.
    //

    if (WriteOptions != FALSE) {
        Options = Shell->Options;
        for (Index = 0; Index < OptionCount; Index += 1) {
            if (ShOptionStrings[Index].String != NULL) {
                if ((ShOptionStrings[Index].Option & Options) != 0) {
                    printf("set -o %s\n", ShOptionStrings[Index].String);

                } else {
                    printf("set +o %s\n", ShOptionStrings[Index].String);
                }
            }
        }
    }

    return TRUE;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
ShRunEnvVariable (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine expands and runs the contents of the ENV variable in the
    current context if appropriate.

Arguments:

    Shell - Supplies a pointer to the shell.

Return Value:

    None.

--*/

{

    int DescriptorAnywhere;
    int EnvironmentDescriptor;
    FILE *EnvironmentFile;
    PSTR ExpandedScript;
    UINTN ExpandedScriptSize;
    SHELL_LEXER_STATE OriginalLexer;
    ULONG OriginalOptions;
    BOOL Result;
    INT ReturnValue;
    PSTR Value;
    UINTN ValueSize;

    EnvironmentFile = NULL;
    ExpandedScript = NULL;

    //
    // Ignore the ENV variable if this is not an interactive script or the
    // real and effective user IDs differ.
    //

    if (((Shell->Options & SHELL_OPTION_INTERACTIVE) == 0) ||
        (SwGetRealUserId() != SwGetEffectiveUserId())) {

        return;
    }

    //
    // If the ENV variable is set, it is expanded and then represents the
    // path of a file to execute in the context of this shell.
    //

    Result = ShGetVariable(Shell,
                           SHELL_ENV,
                           sizeof(SHELL_ENV),
                           &Value,
                           &ValueSize);

    if (Result == FALSE) {
        goto RunEnvVariableEnd;
    }

    Result = ShPerformExpansions(Shell,
                                 Value,
                                 ValueSize,
                                 SHELL_EXPANSION_OPTION_NO_FIELD_SPLIT,
                                 &ExpandedScript,
                                 &ExpandedScriptSize,
                                 NULL,
                                 NULL);

    if (Result == FALSE) {
        PRINT_ERROR("Warning: Unable to expand ENV.\n");
        goto RunEnvVariableEnd;
    }

    assert(Shell->Lexer.LexerPrimed == FALSE);
    assert(Shell->Lexer.InputBufferNextIndex == 0);
    assert(Shell->Lexer.UnputCharacterValid == FALSE);
    assert(Shell->Lexer.LineNumber == 1);

    //
    // Open up the env file.
    //

    DescriptorAnywhere = open(ExpandedScript, O_RDONLY | O_BINARY);
    if (DescriptorAnywhere == -1) {
        PRINT_ERROR("Warning: Unable to open ENV file %s: %s.\n",
                    ExpandedScript,
                    strerror(errno));

        goto RunEnvVariableEnd;
    }

    EnvironmentDescriptor = ShDup(Shell, DescriptorAnywhere, TRUE);
    close(DescriptorAnywhere);
    if (EnvironmentDescriptor < 0) {
        goto RunEnvVariableEnd;
    }

    EnvironmentFile = fdopen(EnvironmentDescriptor, "rb");
    if (EnvironmentFile == NULL) {
        close(EnvironmentDescriptor);
        goto RunEnvVariableEnd;
    }

    assert((Shell->Options & SHELL_OPTION_INPUT_BUFFER_ONLY) == 0);

    //
    // Copy the original lexer state over and reinitialize the lexer
    // for this string.
    //

    memcpy(&OriginalLexer, &(Shell->Lexer), sizeof(SHELL_LEXER_STATE));
    Result = ShInitializeLexer(&(Shell->Lexer),
                               EnvironmentFile,
                               NULL,
                               0);

    if (Result == FALSE) {
        goto RunEnvVariableEnd;
    }

    EnvironmentFile = NULL;

    //
    // Run the env script.
    //

    OriginalOptions = Shell->Options;
    Shell->Options &= ~(SHELL_OPTION_RAW_INPUT |
                        SHELL_OPTION_PRINT_PROMPTS |
                        SHELL_OPTION_NO_COMMAND_HISTORY |
                        SHELL_OPTION_READ_FROM_STDIN);

    Result = ShExecute(Shell, &ReturnValue);
    Shell->Options = OriginalOptions;
    if (Result == FALSE) {
        PRINT_ERROR("Warning: Failed to execute ENV script %s.\n",
                    ExpandedScript);
    }

    //
    // Clean up this lexer (which includes closing the file) and
    // restore the original lexer.
    //

    ShDestroyLexer(&(Shell->Lexer));
    memcpy(&(Shell->Lexer), &OriginalLexer, sizeof(SHELL_LEXER_STATE));
    ShSetAllSignalDispositions(Shell);
    Result = TRUE;

RunEnvVariableEnd:
    if (EnvironmentFile != NULL) {
        fclose(EnvironmentFile);
    }

    if (ExpandedScript != NULL) {
        free(ExpandedScript);
    }

    return;
}

