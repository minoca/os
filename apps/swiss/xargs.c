/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    xargs.c

Abstract:

    This module implements the xargs utility.

Author:

    Evan Green 9-May-2015

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define XARGS_VERSION_MAJOR 1
#define XARGS_VERSION_MINOR 0

#define XARGS_USAGE                                                            \
    "usage: xargs [options] [utility [arguments...]]\n"                        \
    "The xargs utility reads arguments from standard in and constructs a\n"    \
    "command line from those arguments. Options are:\n"                        \
    "  -0, --null -- Use the null character as the delimiter, and turn off \n" \
    "  quoting.\n"                                                             \
    "  -d, --delimiter=delim -- Use the given character as the delimiter, \n"  \
    "     and turn off quoting.\n"                                             \
    "  -E eof -- Use the given string as the logical end of file string.\n"    \
    "  -I replacement -- The utility is executed for each line from \n"        \
    "     standard input, taking the entire line as a single argument, and \n" \
    "     inserting it in each of the given command line arguments where \n"   \
    "     the replacement string is found. Blanks at the beginning of each \n" \
    "     line are ignored. Implies -x.\n"                                     \
    "  -L number -- The utility is executed for each non-empty number of \n"   \
    "     lines of arguments from standard input. Trailing blanks on a \n"     \
    "     line continue the line. -L and -n are mutually exclusive, the \n"    \
    "     last one takes effect.\n"                                            \
    "  -n, --max-args=number -- Invoke the utility using up to the given \n"   \
    "     number of arguments.\n"                                              \
    "  -p, --interactive prompt -- Prompt the user to execute each \n"         \
    "     invocation.\n"                                                       \
    "  -s, --max-chars=size -- Use at most the given size number of \n"        \
    "     characters per command line, including the command and initial \n"   \
    "     arguments.\n"                                                        \
    "  -t, --verbose -- Print each command before it's executed.\n"            \
    "  -x, --exit -- Exit is the size (-s) option is exceeded.\n"              \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n"

//
// The plus is there at the beginning so xargs doesn't permute around the
// initial command line arguments.
//

#define XARGS_OPTIONS_STRING "+0d:E:I:L:n:ps:tx"

#define XARGS_DEFAULT_UTILITY "/bin/echo"

#define XARGS_INITIAL_ARGUMENT_SIZE 64
#define XARGS_INITIAL_ARGUMENT_COUNT 32

//
// Define xargs options.
//

//
// Set this option to limit the number of lines per utility invocation.
//

#define XARGS_OPTION_LIMIT_LINES 0x00000001

//
// Set this option to limit the number of arguments per utility invocation.
//

#define XARGS_OPTION_LIMIT_COUNT 0x00000002

//
// Set this option to disable stardard xargs quoting (which is not the same
// as shell quoting).
//

#define XARGS_OPTION_DISABLE_QUOTING 0x00000004

//
// Set this option to prompt for each invocation.
//

#define XARGS_OPTION_PROMPT 0x00000008

//
// Set this option to print each invocation before it's run.
//

#define XARGS_OPTION_TRACE 0x00000010

//
// Set this option to terminate if the specified number of arguments (-n) or
// lines (-s) will not fit in the implied or specified size.
//

#define XARGS_OPTION_EXIT 0x00000020

//
// Set this option to activate replace mode, where each line of input replaces
// a string in the original command arguments.
//

#define XARGS_OPTION_REPLACE_MODE 0x00000040

//
// Define exit codes.
//

//
// This is returned if any invocation of the command exited with status 1-125.
//

#define XARGS_EXIT_COMMAND_FAILED 123

//
// This is returned if the command exited with status 255.
//

#define XARGS_EXIT_COMMAND_255 124

//
// This is returned if the command was killed by a signal.
//

#define XARGS_EXIT_COMMAND_SIGNALED 125

//
// This is returned if the command cannot be run.
//

#define XARGS_EXIT_COMMAND_RUN_FAILURE 126

//
// This is returned in the command is not found.
//

#define XARGS_EXIT_COMMAND_NOT_FOUND 127

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the context for the xargs utility.

Members:

    Options - Stores the application options. See XARG_OPTION_* definitions.

    Delimiter - Stores the delimiter character, or -1 for the default of blank
        and newline.

    Limit - Stores the line or argument limit.

    EndString - Stores an optional pointer to a string signifying end-of-file.

    AtEnd - Stores a boolean indicating if the last argument ever has been
        encountered.

--*/

typedef struct _XARGS_CONTEXT {
    ULONG Options;
    INT Delimiter;
    LONG Limit;
    PSTR EndString;
    BOOL AtEnd;
} XARGS_CONTEXT, *PXARGS_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
XargsReadArgument (
    PXARGS_CONTEXT Context,
    PSTR *ArgumentOut
    );

PSTR
XargsSubstitute (
    PSTR Template,
    PSTR Replace,
    PSTR Replacement
    );

UINTN
XargsGetArgumentsSize (
    PSTR *Arguments,
    UINTN ArgumentCount
    );

INT
XargsRunCommand (
    PXARGS_CONTEXT Context,
    PSTR *Arguments,
    UINTN ArgumentCount
    );

UINTN
XargsPrintCommand (
    PSTR *Arguments,
    UINTN ArgumentCount
    );

INT
XargsPrompt (
    VOID
    );

VOID
XargsFreeArrayElements (
    PSTR *Array,
    UINTN StartIndex,
    UINTN EndIndex
    );

//
// -------------------------------------------------------------------- Globals
//

struct option XargsLongOptions[] = {
    {"null", no_argument, 0, '0'},
    {"delimiter", required_argument, 0, 'd'},
    {"max-args", required_argument, 0, 'n'},
    {"interactive", no_argument, 0, 'p'},
    {"max-chars", required_argument, 0, 's'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {"verbose", no_argument, 0, 't'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
XargsMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the xargs utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR AfterScan;
    UINTN AllocatedArgumentStart;
    PSTR Argument;
    ULONG ArgumentIndex;
    PSTR *Array;
    UINTN ArrayCapacity;
    CHAR Character;
    XARGS_CONTEXT Context;
    UINTN CurrentCount;
    PSTR DefaultArguments[2];
    CHAR Digit;
    UINTN DigitIndex;
    UINTN InitialSize;
    PSTR *NewArray;
    PSTR NextArgument;
    INT Option;
    PSTR ReadArgument;
    UINTN ReadArgumentSize;
    PSTR ReplaceString;
    UINTN Size;
    UINTN SizeLimit;
    int Status;
    PSTR *Template;
    UINTN TemplateCount;
    INT TotalStatus;

    AllocatedArgumentStart = 1;
    ArgumentIndex = 0;
    Array = NULL;
    ArrayCapacity = 0;
    memset(&Context, 0, sizeof(XARGS_CONTEXT));
    Context.Delimiter = -1;
    Context.Limit = -1;
    NextArgument = NULL;
    ReadArgument = NULL;
    ReplaceString = NULL;
    SizeLimit = 0;
    Status = 0;
    TotalStatus = 0;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             XARGS_OPTIONS_STRING,
                             XargsLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case '0':
            Context.Delimiter = '\0';
            break;

        case 'd':
            Argument = optarg;
            if (*Argument == '\\') {
                Argument += 1;
                Character = *Argument;
                if (Character == '\\') {
                    Character = '\\';

                } else if (Character == 'a') {
                    Character = '\a';

                } else if (Character == 'b') {
                    Character = '\b';

                } else if (Character == 'f') {
                    Character = '\f';

                } else if (Character == 'n') {
                    Character = '\n';

                } else if (Character == 'r') {
                    Character = '\r';

                } else if (Character == 't') {
                    Character = '\t';

                } else if (Character == 'v') {
                    Character = '\v';

                } else if (Character == 'x') {
                    Character = 0;
                    for (DigitIndex = 1; DigitIndex < 3; DigitIndex += 1) {
                        Digit = Argument[DigitIndex];
                        if (isdigit(Digit)) {
                            Digit -= '0';

                        } else if ((Digit >= 'A') && (Digit <= 'F')) {
                            Digit = (Digit - 'A') + 0xA;

                        } else if ((Digit >= 'a') && (Digit <= 'f')) {
                            Digit = (Digit - 'a') + 0xA;

                        } else {
                            break;
                        }

                        Character = (Character * 16) + Digit;
                    }

                } else if ((Character >= '0') && (Character <= '7')) {

                    //
                    // The first digit is already in the character, which is why
                    // the loop only iterates twice for three digits.
                    //

                    Character -= '0';
                    for (DigitIndex = 1; DigitIndex < 3; DigitIndex += 1) {
                        Digit = Argument[DigitIndex];
                        if ((Digit < '0') || (Digit > '7')) {
                            break;
                        }

                        Character = (Character * 8) + (Digit - '0');
                    }

                } else {
                    SwPrintError(0, Argument - 1, "Unknown escape");
                    TotalStatus = 1;
                    goto MainEnd;
                }

                Context.Delimiter = Character;

            } else {
                Context.Delimiter = *Argument;
            }

            break;

        case 'E':
            Context.EndString = optarg;
            break;

        case 'I':
            ReplaceString = optarg;
            Context.Options |= XARGS_OPTION_EXIT | XARGS_OPTION_REPLACE_MODE;
            break;

        case 'L':
            Context.Limit = strtoul(optarg, &AfterScan, 10);
            if (AfterScan == optarg) {
                SwPrintError(0, optarg, "Invalid line limit number");
                TotalStatus = 1;
                goto MainEnd;
            }

            Context.Options |= XARGS_OPTION_LIMIT_LINES;
            Context.Options &= ~XARGS_OPTION_LIMIT_COUNT;
            break;

        case 'n':
            Context.Limit = strtoul(optarg, &AfterScan, 10);
            if (AfterScan == optarg) {
                SwPrintError(0, optarg, "Invalid argument limit number");
                TotalStatus = 1;
                goto MainEnd;
            }

            Context.Options |= XARGS_OPTION_LIMIT_COUNT;
            Context.Options &= ~XARGS_OPTION_LIMIT_LINES;
            break;

        case 'p':
            Context.Options |= XARGS_OPTION_PROMPT | XARGS_OPTION_TRACE;
            break;

        case 's':
            SizeLimit = strtoul(optarg, &AfterScan, 10);
            if (AfterScan == optarg) {
                SwPrintError(0, optarg, "Invalid size limit");
                TotalStatus = 1;
                goto MainEnd;
            }

            break;

        case 't':
            Context.Options |= XARGS_OPTION_TRACE;
            break;

        case 'x':
            Context.Options |= XARGS_OPTION_EXIT;
            break;

        case 'V':
            SwPrintVersion(XARGS_VERSION_MAJOR, XARGS_VERSION_MINOR);
            return 1;

        case 'h':
            printf(XARGS_USAGE);
            return 1;

        default:

            assert(FALSE);

            TotalStatus = 1;
            goto MainEnd;
        }
    }

    if (Context.Limit == 0) {
        SwPrintError(0, NULL, "Limit cannot be zero.");
        TotalStatus = 1;
        goto MainEnd;
    }

    //
    // Figure out the base template, including the utility and initial
    // arguments.
    //

    ArgumentIndex = optind;
    if (ArgumentIndex < ArgumentCount) {
        Template = Arguments + ArgumentIndex;
        TemplateCount = ArgumentCount - ArgumentIndex;

    } else {
        DefaultArguments[0] = XARGS_DEFAULT_UTILITY;
        DefaultArguments[1] = NULL;
        Template = DefaultArguments;
        TemplateCount = 1;
    }

    //
    // Allocate a new argument array.
    //

    ArrayCapacity = XARGS_INITIAL_ARGUMENT_COUNT;
    if (ArrayCapacity < TemplateCount + 1) {
        ArrayCapacity = TemplateCount + 1;
    }

    Array = malloc(ArrayCapacity * sizeof(PSTR));
    if (Array == NULL) {
        Status = ENOMEM;
        goto MainEnd;
    }

    //
    // Replace mode works differently. It takes an argument from standard in
    // and replaces strings in the initial command line arguments.
    //

    if ((Context.Options & XARGS_OPTION_REPLACE_MODE) != 0) {
        Array[0] = Template[0];
        while (TRUE) {
            Status = XargsReadArgument(&Context, &ReadArgument);
            if (Status != 0) {
                goto MainEnd;
            }

            if (ReadArgument == NULL) {
                break;
            }

            for (ArgumentIndex = 1;
                 ArgumentIndex < TemplateCount;
                 ArgumentIndex += 1) {

                Array[ArgumentIndex] = XargsSubstitute(Template[ArgumentIndex],
                                                       ReplaceString,
                                                       ReadArgument);

                if (Array[ArgumentIndex] == NULL) {
                    Status = ENOMEM;
                    goto MainEnd;
                }
            }

            free(ReadArgument);
            ReadArgument = NULL;
            Array[TemplateCount] = NULL;
            if (SizeLimit != 0) {
                Size = XargsGetArgumentsSize(Array, TemplateCount);
                if (Size > SizeLimit) {
                    SwPrintError(0, NULL, "Command too big");
                    TotalStatus = 1;
                    goto MainEnd;
                }
            }

            Status = XargsRunCommand(&Context, Array, TemplateCount);
            if (Status != 0) {
                TotalStatus = Status;
            }

            XargsFreeArrayElements(Array, 1, TemplateCount);
            if (Status == XARGS_EXIT_COMMAND_255) {
                goto MainEnd;
            }
        }

    //
    // Run in normal, non-replace mode.
    //

    } else {

        //
        // Add the template parts, which always stay the same.
        //

        memcpy(Array, Template, TemplateCount * sizeof(PSTR));
        AllocatedArgumentStart = TemplateCount;
        InitialSize = XargsGetArgumentsSize(Array, TemplateCount);
        if ((SizeLimit != 0) && (InitialSize > SizeLimit)) {
            SwPrintError(0, NULL, "Size limit too small for initial arguments");
            TotalStatus = 1;
            goto MainEnd;
        }

        //
        // Loop invoking commands.
        //

        while (Context.AtEnd == FALSE) {
            ArgumentIndex = TemplateCount;
            Size = InitialSize;

            //
            // Loop adding arguments.
            //

            CurrentCount = 0;
            while ((Context.Limit == -1) || (CurrentCount < Context.Limit)) {

                //
                // If there was already an argument read that was too big
                // before, use it now.
                //

                if (NextArgument != NULL) {
                    ReadArgument = NextArgument;
                    NextArgument = NULL;

                } else {
                    Status = XargsReadArgument(&Context, &ReadArgument);
                    if (Status != 0) {
                        SwPrintError(Status, NULL, "Failed to read argument");
                        goto MainEnd;
                    }

                    if (ReadArgument == NULL) {
                        break;
                    }
                }

                if (SizeLimit != 0) {
                    ReadArgumentSize = strlen(ReadArgument) + 1;

                    //
                    // If the argument will never fit in the limit, fail.
                    //

                    if (ReadArgumentSize + InitialSize > SizeLimit) {
                        SwPrintError(0, NULL, "Argument too big");
                        TotalStatus = 1;
                        goto MainEnd;

                    //
                    // If this argument pushes the command over the limit,
                    // skip it for next time.
                    //

                    } else if (Size + ReadArgumentSize > SizeLimit) {

                        //
                        // Fail if requested.
                        //

                        if ((Context.Options & XARGS_OPTION_EXIT) != 0) {
                            SwPrintError(0, NULL, "Argument too big, -x set");
                            TotalStatus = XARGS_EXIT_COMMAND_RUN_FAILURE;
                            goto MainEnd;
                        }

                        NextArgument = ReadArgument;
                        ReadArgument = NULL;
                        break;
                    }

                    Size += ReadArgumentSize;
                }

                //
                // Add the argument to the array, reallocating if needed.
                //

                if (ArgumentIndex + 1 >= ArrayCapacity) {
                    ArrayCapacity *= 2;

                    assert(ArgumentIndex + 1 < ArrayCapacity);

                    NewArray = realloc(Array, ArrayCapacity * sizeof(PSTR));
                    if (NewArray == NULL) {
                        Status = ENOMEM;
                        goto MainEnd;
                    }

                    Array = NewArray;
                }

                Array[ArgumentIndex] = ReadArgument;
                ArgumentIndex += 1;
                ReadArgument = NULL;
                CurrentCount += 1;
            }

            Array[ArgumentIndex] = NULL;
            Status = XargsRunCommand(&Context, Array, ArgumentIndex);
            if (Status != 0) {
                TotalStatus = Status;
            }

            XargsFreeArrayElements(Array, TemplateCount, ArgumentIndex);
            if (Status == XARGS_EXIT_COMMAND_255) {
                goto MainEnd;
            }
        }
    }

MainEnd:
    if (NextArgument != NULL) {

        assert(NextArgument != ReadArgument);

        free(NextArgument);
    }

    if (ReadArgument != NULL) {
        free(ReadArgument);
    }

    if (Array != NULL) {
        XargsFreeArrayElements(Array, AllocatedArgumentStart, ArgumentIndex);
        free(Array);
    }

    if ((TotalStatus == 0) && (Status != 0)) {
        SwPrintError(Status, 0, "error");
        TotalStatus = 1;
    }

    return TotalStatus;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
XargsReadArgument (
    PXARGS_CONTEXT Context,
    PSTR *ArgumentOut
    )

/*++

Routine Description:

    This routine reads a single argument from standard in.

Arguments:

    Context - Supplies a pointer to the application context.

    ArgumentOut - Supplies a pointer where a pointer to an allocated string
        containing the argument will be returned on success. NULL will be
        returned if an EOF condition was found.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PSTR Argument;
    UINTN Capacity;
    INT Character;
    PSTR NewBuffer;
    CHAR Previous;
    INT Quote;
    UINTN Size;
    INT Status;

    Argument = NULL;
    if (Context->AtEnd != FALSE) {
        Status = 0;
        goto ReadArgumentEnd;
    }

    Size = 0;
    Capacity = XARGS_INITIAL_ARGUMENT_SIZE;
    Argument = malloc(Capacity);
    if (Argument == NULL) {
        Status = ENOMEM;
        goto ReadArgumentEnd;
    }

    Previous = 0;
    Quote = 0;
    while (TRUE) {
        Character = fgetc(stdin);
        if (Character == EOF) {
            if (Size == 0) {
                free(Argument);
                Argument = NULL;
            }

            Context->AtEnd = TRUE;
            break;
        }

        //
        // If there's a delimiter, look for that.
        //

        if (Context->Delimiter != -1) {
            if (Character == Context->Delimiter) {
                break;
            }

        //
        // Look for a newline or blank. Ignore leading blanks and newlines.
        //

        } else {

            //
            // If in quote mode, look for the matching end quote.
            //

            if (Quote != 0) {
                if (Quote == '\\') {
                    Quote = 0;

                } else {

                    //
                    // Quotes span blanks, but not newlines.
                    //

                    if (Character == '\n') {
                        SwPrintError(0, NULL, "Unterminated quote");
                        Status = EINVAL;
                        goto ReadArgumentEnd;
                    }

                    if (Character == Quote) {
                        Quote = 0;
                        continue;
                    }
                }

            } else {
                if (((isblank(Character)) &&
                     ((Context->Options & XARGS_OPTION_REPLACE_MODE) == 0) &&
                     ((Context->Options & XARGS_OPTION_LIMIT_LINES) == 0)) ||
                    (Character == '\n')) {

                    if (Size == 0) {
                        continue;

                    } else {
                        break;
                    }

                //
                // If a quote is beginning, don't add the quote character, and
                // go into quote mode.
                //

                } else if ((Character == '\\') ||
                           (Character == '"') ||
                           (Character == '\'')) {

                    Quote = Character;
                    continue;
                }
            }
        }

        //
        // If limited by lines, then newlines that don't have blanks before
        // them break the argument.
        //

        if ((Context->Options & XARGS_OPTION_LIMIT_LINES) != 0) {
            if ((Character == '\n') && (!isblank(Previous))) {
                break;
            }
        }

        //
        // Reallocate if needed, always leaving space for a null terminator
        // too.
        //

        if (Size + 1 >= Capacity) {
            Capacity *= 2;

            assert(Capacity > Size + 1);

            NewBuffer = realloc(Argument, Capacity);
            if (NewBuffer == NULL) {
                Status = ENOMEM;
                goto ReadArgumentEnd;
            }

            Argument = NewBuffer;
        }

        //
        // Add the character to the current argument.
        //

        Argument[Size] = Character;
        Size += 1;
        Previous = Character;
    }

    if (Argument != NULL) {
        Argument[Size] = '\0';

        //
        // Check it against the EOF string.
        //

        if ((Context->EndString != NULL) &&
            (strcmp(Context->EndString, Argument) == 0)) {

            free(Argument);
            Argument = NULL;
            Context->AtEnd = TRUE;
        }
    }

    Status = 0;

ReadArgumentEnd:
    if (Status != 0) {
        if (Argument != NULL) {
            free(Argument);
            Argument = NULL;
        }
    }

    *ArgumentOut = Argument;
    return Status;
}

PSTR
XargsSubstitute (
    PSTR Template,
    PSTR Replace,
    PSTR Replacement
    )

/*++

Routine Description:

    This routine replaces instances of the given string to replace in the
    template with the given replacement.

Arguments:

    Template - Supplies a pointer to the string to make replacements in.

    Replace - Supplies a pointer to the string to replace.

    Replacement - Supplies a pointer to the string to replace it with.

Return Value:

    Returns a pointer to an allocated substitute string on success. The caller
    is responsible for freeing this memory.

    NULL on allocation failure.

--*/

{

    PSTR Current;
    PSTR Found;
    UINTN NewSize;
    PSTR NewString;
    PSTR Remainder;
    UINTN ReplacementSize;
    UINTN ReplaceSize;
    PSTR Result;
    UINTN Size;

    Size = strlen(Template) + 1;
    Result = strdup(Template);
    if (Result == NULL) {
        return NULL;
    }

    ReplacementSize = strlen(Replacement);
    ReplaceSize = strlen(Replace);
    Current = Result;
    while (TRUE) {
        Found = strstr(Current, Replace);
        if (Found == NULL) {
            break;
        }

        NewSize = Size - ReplaceSize + ReplacementSize;
        NewString = malloc(NewSize);
        if (NewString == NULL) {
            free(Result);
            return NULL;
        }

        memcpy(NewString, Result, Found - Result);
        memcpy(NewString + (Found - Result), Replacement, ReplacementSize);
        Remainder = NewString + (Found - Result) + ReplacementSize;
        memcpy(Remainder,
               Found + ReplaceSize,
               Size - ((Found - Result) + ReplaceSize));

        Current = Remainder;
        free(Result);
        Result = NewString;
        Size = NewSize;
    }

    return Result;
}

UINTN
XargsGetArgumentsSize (
    PSTR *Arguments,
    UINTN ArgumentCount
    )

/*++

Routine Description:

    This routine gets the size in bytes of the given arguments array.

Arguments:

    Arguments - Supplies the array of arguments.

    ArgumentCount - Supplies the number of elements in the array.

Return Value:

    Returns the number of bytes in the argument values, including the null
    terminators, but not including the array storage itself.

--*/

{

    UINTN ArgumentIndex;
    UINTN Size;

    Size = 0;
    for (ArgumentIndex = 0; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        Size += strlen(Arguments[ArgumentIndex]) + 1;
    }

    return Size;
}

INT
XargsRunCommand (
    PXARGS_CONTEXT Context,
    PSTR *Arguments,
    UINTN ArgumentCount
    )

/*++

Routine Description:

    This routine runs the given command.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies the argument array to invoke the command with.

    ArgumentCount - Supplies the number of elements in the argument array.

Return Value:

    0 on success.

    Returns an XARGS_EXIT_* value.

--*/

{

    INT ReturnValue;
    INT Status;

    if ((Context->Options & XARGS_OPTION_TRACE) != 0) {
        XargsPrintCommand(Arguments, ArgumentCount);
    }

    if ((Context->Options & XARGS_OPTION_PROMPT) != 0) {
        Status = XargsPrompt();
        if (Status != 0) {
            return 0;
        }
    }

    Status = SwRunCommand(Arguments[0],
                          Arguments,
                          ArgumentCount,
                          0,
                          &ReturnValue);

    if (Status != 0) {
        SwPrintError(Status, Arguments[0], "Unable to run");
        return XARGS_EXIT_COMMAND_RUN_FAILURE;
    }

    if (ReturnValue == 0) {
        return ReturnValue;
    }

    if (ReturnValue == XARGS_EXIT_COMMAND_NOT_FOUND) {
        SwPrintError(0, Arguments[0], "Command not found");
        return ReturnValue;
    }

    if (WIFSIGNALED(ReturnValue)) {
        SwPrintError(0, Arguments[0], "Terminated by signal");
        return XARGS_EXIT_COMMAND_SIGNALED;
    }

    if (WEXITSTATUS(ReturnValue) == 255) {
        SwPrintError(0, Arguments[0], "Returned 255");
        return XARGS_EXIT_COMMAND_255;
    }

    return 1;
}

UINTN
XargsPrintCommand (
    PSTR *Arguments,
    UINTN ArgumentCount
    )

/*++

Routine Description:

    This routine prints the command about to be run.

Arguments:

    Arguments - Supplies the array of arguments.

    ArgumentCount - Supplies the number of elements in the array.

Return Value:

    None.

--*/

{

    UINTN ArgumentIndex;
    UINTN Size;

    Size = 0;
    fprintf(stderr, "%s", Arguments[0]);
    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        fprintf(stderr, " %s", Arguments[ArgumentIndex]);
    }

    fprintf(stderr, "\n");
    return Size;
}

INT
XargsPrompt (
    VOID
    )

/*++

Routine Description:

    This routine prompts the user to enter Y or N.

Arguments:

    None.

Return Value:

    0 if the command should proceed.

    Non-zero if the command should not proceed.

--*/

{

    ssize_t BytesRead;
    CHAR Character;
    INT Result;
    int Terminal;
    CHAR TerminalName[L_ctermid];

    fprintf(stderr, "?...");
    fflush(NULL);
    if (ctermid(TerminalName) == NULL) {
        return 1;
    }

    Terminal = SwOpen(TerminalName, O_RDONLY, 0);
    if (Terminal < 0) {
        return 1;
    }

    Result = 1;
    do {
        BytesRead = read(Terminal, &Character, 1);

    } while ((BytesRead == -1) && (errno == EINTR));

    if (BytesRead <= 1) {
        close(Terminal);
        return 1;
    }

    if ((Character == 'y') || (Character == 'Y')) {
        Result = 0;
    }

    //
    // Read until the next newline.
    //

    while ((Character != '\n') && (BytesRead == 1)) {
        do {
            BytesRead = read(Terminal, &Character, 1);

        } while ((BytesRead == -1) && (errno == EINTR));
    }

    close(Terminal);
    return Result;
}

VOID
XargsFreeArrayElements (
    PSTR *Array,
    UINTN StartIndex,
    UINTN EndIndex
    )

/*++

Routine Description:

    This routine frees the elements in the given array.

Arguments:

    Array - Supplies a pointer to the array to free.

    StartIndex - Supplies the start index to free from, inclusive.

    EndIndex - Supplies the end index to free to, exclusive.

Return Value:

    None.

--*/

{

    UINTN Index;

    for (Index = StartIndex; Index < EndIndex; Index += 1) {
        if (Array[Index] != NULL) {
            free(Array[Index]);
            Array[Index] = NULL;
        }
    }

    return;
}

