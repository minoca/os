/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    getopt.c

Abstract:

    This module implements support for the getopt family of functions, which is
    made for parsing command line arguments.

Author:

    Evan Green 21-Aug-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define string for the environment variable used to enforce correct POSIX
// behavior.
//

#define GET_OPTION_CORRECT_POSIX_ENVIRONMENT_VARIABLE "POSIXLY_CORRECT"

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _GET_OPTION_ERROR_TYPE {
    GetOptionErrorNone,
    GetOptionErrorMissingArgument,
    GetOptionErrorUnknownOption,
    GetOptionErrorAmbiguousOption,
    GetOptionErrorNoArgumentExpected,
} GET_OPTION_ERROR_TYPE, *PGET_OPTION_ERROR_TYPE;

/*++

Structure Description:

    This structure stores the context needed to report a getopt error.

Members:

    Type - Stores the error type that occurred.

    Option - Stores the short option character that caused the error.

    LongOption - Stores a pointer to the long option string that caused the
        error.

    CommandName - Stores the first command line argument: the command name.

--*/

typedef struct _GET_OPTION_ERROR {
    GET_OPTION_ERROR_TYPE Type;
    CHAR Option;
    const char *LongOption;
    PSTR CommandName;
} GET_OPTION_ERROR, *PGET_OPTION_ERROR;

//
// ----------------------------------------------- Internal Function Prototypes
//

int
ClpGetOption (
    int ArgumentCount,
    char *const Arguments[],
    const char *Options,
    const struct option *LongOptions,
    int *LongIndex,
    BOOL ShortLongOptions
    );

int
ClpGetShortOption (
    int ArgumentCount,
    char *const Arguments[],
    const char *Options,
    const struct option *LongOptions,
    int *LongIndex,
    PGET_OPTION_ERROR Error
    );

int
ClpGetLongOption (
    int ArgumentCount,
    char *const Arguments[],
    const char *Argument,
    const char *Options,
    const struct option *LongOptions,
    int *LongIndex,
    PGET_OPTION_ERROR Error
    );

int
ClpMatchLongOption (
    const char *Argument,
    const struct option *Options,
    PGET_OPTION_ERROR Error
    );

size_t
ClpMatchLongOptionString (
    const char *Argument,
    const char *OptionName
    );

void
ClpPrintGetOptionError (
    PGET_OPTION_ERROR Error
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the global that points to the argument if the getopt function finds
// an option that takes an argument.
//

LIBC_API char *optarg;

//
// Define the global that contains the index of the next argument to be
// processed by the getopt function.
//

LIBC_API int optind = 1;

//
// Define the global that controls whether or not an error message is printed
// to standrad error when the getopt function detects an error. The user can
// set this to 0 to disable such messages.
//

LIBC_API int opterr = 1;

//
// Define the global that is set to the unknown option if an option is passed
// in the arguments that is not in the options string during a call to getopt.
//

LIBC_API int optopt;

//
// Define the global that can be used to reset the option system so that it
// can be called with a different array or called repeatedly on the same array.
// Setting optind to zero has the same effect as setting optreset to non-zero.
//

LIBC_API int optreset;

//
// Define a variable containing the next character index to look in the
// current argument in the getopt function.
//

int ClNextOptionCharacter = 1;

//
// Define a copy of the optind variable that is used to detect if the user
// tried to reset the getopt state.
//

int ClOptionIndexCopy;

//
// Define a variable containing the first non-option string encountered.
//

char *ClFirstNonOption;

//
// Define a variable used to store the index of the option end delimiter "--".
//

int ClOptionEndIndex;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
int
getopt (
    int ArgumentCount,
    char *const Arguments[],
    const char *Options
    )

/*++

Routine Description:

    This routine parses command line arguments, successively returning each
    passed in argument. This routine is neither reentrant nor thread safe.

Arguments:

    ArgumentCount - Supplies the argument count from main.

    Arguments - Supplies the argument array from main.

    Options - Supplies a pointer to a null terminated string containing the set
        of accepted options. Each character represents an allowed option. If
        a character is followed by a colon ':', then that option takes an
        argument. If an option with an argument is found and there are more
        characters in the current string, then the remainder of that string
        will be returned. Otherwise, the next argument will be returned. If
        there is no next argument, that's considered an error.

Return Value:

    Returns the next option character on success. The global variable optind
    will be updated to reflect the index of the next argument to be processed.
    It will be initialied by the system to 1. If the option takes an argument,
    the global variable optarg will be set to point at that argument.

    Returns '?' if the option found is not in the recognized option string. The
    optopt global variable will be set to the unrecognized option that resulted
    in this condition. The '?' character is also returned if the options string
    does not begin with a colon ':' and a required argument is not found. If the
    opterr global variable has not been set to 0 by the user, then an error
    will be printed to standard error.

    Returns ':' if the options string begins with a colon and a required
    argument is missing. If the opterr global variable has not been set to 0 by
    the user, then an error will be printed to standard error.

    -1 if a non-option was encountered. In this case the optind global variable
    will be set to the first non-option argument.

--*/

{

    int Result;

    Result = ClpGetOption(ArgumentCount,
                          Arguments,
                          Options,
                          NULL,
                          NULL,
                          FALSE);

    return Result;
}

LIBC_API
int
getopt_long (
    int ArgumentCount,
    char *const Arguments[],
    const char *ShortOptions,
    const struct option *LongOptions,
    int *LongIndex
    )

/*++

Routine Description:

    This routine works just like the getopt function (see that for details),
    except it allow allows long options of the form --option=argument or
    --option argument.

Arguments:

    ArgumentCount - Supplies the argument count from main.

    Arguments - Supplies the argument array from main.

    ShortOptions - Supplies the short option string. This parameter works the
        same way as the Options string of getopt.

    LongOptions - Supplies a pointer to an array of long options. The array
        must be terminated with a NULLed out option structure. Long option
        names can be abbreviated in the argument list provided that the
        abbreviation is unique.

    LongIndex - Supplies an optional pointer that returns the index into the
        long options array of the long option that matched.

Return Value:

    Returns the same set of values as the getopt function. If a long option
    masked, then either 0 or the value set inside the long option is returned
    depending on the flag member of the long option.

--*/

{

    int Result;

    Result = ClpGetOption(ArgumentCount,
                          Arguments,
                          ShortOptions,
                          LongOptions,
                          LongIndex,
                          FALSE);

    return Result;
}

LIBC_API
int
getopt_long_only (
    int ArgumentCount,
    char *const Arguments[],
    const char *ShortOptions,
    const struct option *LongOptions,
    int *LongIndex
    )

/*++

Routine Description:

    This routine works just like the getopt_long function except it allows
    long arguments to have only one dash at the beginning instead of two
    (ie -option instead of --option). If an argument does not match for long
    options of either --option or -option, the short options will be tried.

Arguments:

    ArgumentCount - Supplies the argument count from main.

    Arguments - Supplies the argument array from main.

    ShortOptions - Supplies the short option string. This parameter works the
        same way as the Options string of getopt.

    LongOptions - Supplies a pointer to an array of long options. The array
        must be terminated with a NULLed out option structure. Long option
        names can be abbreviated in the argument list provided that the
        abbreviation is unique.

    LongIndex - Supplies an optional pointer that returns the index into the
        long options array of the long option that matched.

Return Value:

    Returns the same set of values as the getopt function. If a long option
    masked, then either 0 or the value set inside the long option is returned
    depending on the flag member of the long option.

--*/

{

    int Result;

    Result = ClpGetOption(ArgumentCount,
                          Arguments,
                          ShortOptions,
                          LongOptions,
                          LongIndex,
                          TRUE);

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

int
ClpGetOption (
    int ArgumentCount,
    char *const Arguments[],
    const char *Options,
    const struct option *LongOptions,
    int *LongIndex,
    BOOL ShortLongOptions
    )

/*++

Routine Description:

    This routine parses command line arguments for the getopt routines.

Arguments:

    ArgumentCount - Supplies the argument count from main.

    Arguments - Supplies the argument array from main.

    Options - Supplies the short options string, which defines the set of legal
        short options.

    LongOptions - Supplies a pointer to an array of long option structures
        containing the long options.

    LongIndex - Supplies an optional pointer that if supplied will return the
        index into the long options array of the found long option.

    ShortLongOptions - Supplies a boolean indicating if long options starting
        with only one dash should be recognized.

Return Value:

    Returns the option character or 0 if another option was found.

    Returns '?' if an unknown option was found, or a required argument is
    missing and the options string does not start with a colon.

    Returns ':' if a required argument to an option is missing and the options
    string starts with a colon.

    -1 if no more options were found.

--*/

{

    PSTR Argument;
    GET_OPTION_ERROR Error;
    BOOL FailNonOptions;
    ULONG Index;
    int Result;
    BOOL ReturnNonOptions;

    FailNonOptions = FALSE;
    ReturnNonOptions = FALSE;
    optarg = NULL;
    memset(&Error, 0, sizeof(Error));
    if (ArgumentCount == 0) {
        return -1;
    }

    Error.CommandName = Arguments[0];
    if ((optind <= 0) || (optreset != 0) || (ClOptionEndIndex <= 0) ||
        (ClOptionEndIndex >= ArgumentCount)) {

        optind = 1;
        optreset = 0;
        ClOptionIndexCopy = 1;
        ClNextOptionCharacter = 1;
        ClFirstNonOption = NULL;
        optarg = NULL;
        ClOptionEndIndex = ArgumentCount - 1;
    }

    //
    // Reset if the caller tried to manipulate the option index.
    //

    if (ClOptionIndexCopy != optind) {
        ClOptionIndexCopy = optind;
        ClNextOptionCharacter = 1;
        ClOptionEndIndex = ArgumentCount - 1;
        ClFirstNonOption = NULL;
    }

    //
    // Don't go off the end of the array.
    //

    if (optind >= ArgumentCount) {
        Result = -1;
        goto GetOptionEnd;
    }

    //
    // If there are short options and the first character is a -, then return
    // non-options as a option with value 1. If there are short options and
    // the first character is +, then fail if a non-option is encountered.
    //

    if (Options != NULL) {
        if (*Options == '-') {
            ReturnNonOptions = TRUE;
            Options += 1;

        } else if (*Options == '+') {
            FailNonOptions = TRUE;
            Options += 1;
        }
    }

    //
    // Check to see if the POSIXLY_CORRECT environment variable is present,
    // dictating that non-options should fail.
    //

    if (getenv(GET_OPTION_CORRECT_POSIX_ENVIRONMENT_VARIABLE) != NULL) {
        FailNonOptions = TRUE;
    }

    Argument = Arguments[optind];

    assert(Argument != NULL);

    //
    // Loop while the argument doesn't start with a '-', or is just a single
    // dash.
    //

    while ((*Argument != '-') || (*(Argument + 1) == '\0')) {
        ClNextOptionCharacter = 1;
        if (ReturnNonOptions != FALSE) {
            optarg = Argument;
            optind += 1;
            Result = 1;
            goto GetOptionEnd;

        } else if (FailNonOptions != FALSE) {
            Result = -1;
            goto GetOptionEnd;
        }

        //
        // If the first argument is reached again, it's time to give up.
        //

        if (Argument == ClFirstNonOption) {
            Result = -1;
            goto GetOptionEnd;
        }

        //
        // This non-option needs to be put at the end of the option list.
        // Shift everything down and try again with the new argument. Make sure
        // that the non-option gets put before any arguments that come after
        // the "--" delimiter.
        //
        // N.B. For legacy reasons - to match default C library behavior that
        //      pre-dates 'const' - turn a blind eye to the fact that the
        //      Arguments parameter is an array of constant pointers.
        //

        for (Index = optind; Index < ClOptionEndIndex; Index += 1) {
            if ((ClFirstNonOption == NULL) &&
                (strcmp(Arguments[Index + 1], "--") == 0)) {

                ClOptionEndIndex = Index + 1;
            }

            *((PSTR *)&(Arguments[Index])) = Arguments[Index + 1];
        }

        //
        // If this is the first time an argument has been thrown at the end,
        // remember it.
        //

        assert(Index < ArgumentCount);

        *((PSTR *)&(Arguments[Index])) = Argument;
        if (ClFirstNonOption == NULL) {
            ClFirstNonOption = Argument;
        }

        //
        // Get the new argument in the index position.
        //

        Argument = Arguments[optind];
    }

    if ((Argument[1] == '-') && (Argument[2] == '\0')) {
        optind += 1;
        Result = -1;
        goto GetOptionEnd;
    }

    assert(ClNextOptionCharacter <= strlen(Argument));

    //
    // If there are long options, try to parse them if 1) the argument starts
    // with -- or 2) It's a long_only call and either a) there are multiple
    // characters in the argument or b) it's not in the short arguments string.
    //

    if (LongOptions != NULL) {
        if ((Argument[1] == '-') ||
            ((ShortLongOptions != FALSE) &&
             ((Argument[2] != '\0') ||
              (strchr(Options, Argument[ClNextOptionCharacter]) == NULL)))) {

            if ((ClNextOptionCharacter == 1) && (Argument[1] == '-')) {
                ClNextOptionCharacter += 1;
            }

            Result = ClpGetLongOption(ArgumentCount,
                                      Arguments,
                                      Argument + ClNextOptionCharacter,
                                      Options,
                                      LongOptions,
                                      LongIndex,
                                      &Error);

            if (Result != '?') {
                ClNextOptionCharacter = 1;
                goto GetOptionEnd;
            }

            if ((Error.Type != GetOptionErrorUnknownOption) ||
                (ShortLongOptions == FALSE)) {

                goto GetOptionEnd;
            }

            optind -= 1;
        }
    }

    //
    // Try for a short option.
    //

    Result = ClpGetShortOption(ArgumentCount,
                               Arguments,
                               Options,
                               LongOptions,
                               LongIndex,
                               &Error);

GetOptionEnd:
    if ((Error.Type != GetOptionErrorNone) &&
        (opterr != 0) && ((Options == NULL) || (*Options != ':'))) {

        ClpPrintGetOptionError(&Error);
    }

    ClOptionIndexCopy = optind;
    return Result;
}

int
ClpGetShortOption (
    int ArgumentCount,
    char *const Arguments[],
    const char *Options,
    const struct option *LongOptions,
    int *LongIndex,
    PGET_OPTION_ERROR Error
    )

/*++

Routine Description:

    This routine attempts to parse a single short option at the given location.

Arguments:

    ArgumentCount - Supplies the number of arguments in the array.

    Arguments - Supplies the array of arguments.

    Options - Supplies the short options string, which defines the set of legal
        short options.

    LongOptions - Supplies a pointer to an array of long option structures
        containing the long options.

    LongIndex - Supplies an optional pointer that if supplied will return the
        index into the long options array of the found long option.

    Error - Supplies a pointer where the error information is returned.

Return Value:

    Returns the values appropriate to return from the getopt functions.

--*/

{

    const char *Argument;
    int Option;
    BOOL StartsWithColon;

    assert(optind < ArgumentCount);
    assert(ClNextOptionCharacter != 0);

    Argument = Arguments[optind] + ClNextOptionCharacter;

    assert(*Argument != '\0');

    StartsWithColon = FALSE;
    if (*Options == ':') {
        StartsWithColon = TRUE;
        Options += 1;
    }

    //
    // Loop over every acceptable option.
    //

    while (*Options != '\0') {

        //
        // Keep looking if they're not equal.
        //

        if ((!isalnum(*Options)) || (*Argument != *Options)) {
            Options += 1;
            continue;
        }

        //
        // They're equal, look to see if the next character is a colon.
        //

        Option = *Options;
        Options += 1;
        ClNextOptionCharacter += 1;
        Argument += 1;

        //
        // If the option is W and it's followed by a semicolon, then treat
        // -W foo as the long option --foo.
        //

        if ((Option == 'W') && (*Options == ';') && (LongOptions != NULL)) {
            Options += 1;
            ClNextOptionCharacter = 1;

            //
            // Use either the remainder of the argument or the next argument
            // as the long option.
            //

            if (*Argument == '\0') {
                optind += 1;
                if (optind >= ArgumentCount) {
                    optopt = Option;
                    Error->Option = Option;
                    Error->Type = GetOptionErrorMissingArgument;
                    return '?';
                }

                Argument = Arguments[optind];
            }

            Option = ClpGetLongOption(ArgumentCount,
                                      Arguments,
                                      Argument,
                                      Options,
                                      LongOptions,
                                      LongIndex,
                                      Error);

            return Option;
        }

        //
        // If no argument is required, then work here is done.
        //

        if (*Options != ':') {

            //
            // If the next character of the argument is the terminator, then
            // up the index and reset the option character.
            //

            if (*Argument == '\0') {
                ClNextOptionCharacter = 1;
                optind += 1;
            }

            goto GetShortOptionEnd;
        }

        Options += 1;

        //
        // An argument is required or optional. If the next character of the
        // argument is not null, then the argument is the remainder.
        //

        ClNextOptionCharacter = 1;
        if (*Argument != '\0') {
            optarg = (char *)Argument;
            optind += 1;
            goto GetShortOptionEnd;
        }

        //
        // If the argument is optional, then the only chance for an argument
        // was the remainder of the current argument. Bail out now with no
        // argument.
        //

        if (*Options == ':') {
            optind += 1;
            goto GetShortOptionEnd;
        }

        //
        // It must be in the next argument. If there is no next argument, that's
        // a problem.
        //

        if (optind >= ArgumentCount - 1) {
            optind += 1;
            optopt = Option;
            if (StartsWithColon != FALSE) {
                Option = ':';
                goto GetShortOptionEnd;
            }

            Error->Option = Option;
            Error->Type = GetOptionErrorMissingArgument;
            Option = '?';
            goto GetShortOptionEnd;
        }

        optind += 1;
        optarg = Arguments[optind];
        optind += 1;
        Error->Type = GetOptionErrorNone;
        goto GetShortOptionEnd;
    }

    //
    // The argument doesn't match any of the acceptable options.
    //

    optopt = *Argument;
    if (StartsWithColon == FALSE) {
        Error->Option = *Argument;
        Error->Type = GetOptionErrorUnknownOption;
    }

    //
    // Advance to the next option, which may require advancing the index.
    //

    Argument += 1;
    if (*Argument == '\0') {
        optind += 1;
        ClNextOptionCharacter = 1;

    } else {
        ClNextOptionCharacter += 1;
    }

    Option = '?';

GetShortOptionEnd:

    //
    // Clear the error if all is well. Returning ':' is not well, but no error
    // should be printed, so it's effectively the same as a success case.
    //

    if ((Option != -1) && (Option != '?')) {
        Error->Type = GetOptionErrorNone;
    }

    return Option;
}

int
ClpGetLongOption (
    int ArgumentCount,
    char *const Arguments[],
    const char *Argument,
    const char *Options,
    const struct option *LongOptions,
    int *LongIndex,
    PGET_OPTION_ERROR Error
    )

/*++

Routine Description:

    This routine parses attempts to parse a long command line option.

Arguments:

    ArgumentCount - Supplies the argument count from main.

    Arguments - Supplies the argument array from main.

    Argument - Supplies the argument to read, with any leading dashes stripped
        away.

    Options - Supplies the short options string, which defines the set of legal
        short options.

    LongOptions - Supplies a pointer to an array of long option structures
        containing the long options.

    LongIndex - Supplies an optional pointer that if supplied will return the
        index into the long options array of the found long option.

    Error - Supplies a pointer where the error information will be returned.

Return Value:

    Returns value appropriate to be returned to the getopt functions.

--*/

{

    PSTR Equals;
    const struct option *Option;
    int OptionIndex;

    if (LongOptions == NULL) {
        return -1;
    }

    assert(optind < ArgumentCount);

    //
    // The two valid forms are --option argument or --option=argument. Look for
    // an equals to terminate the option name.
    //

    Equals = strchr(Argument, '=');

    //
    // Get the long option.
    //

    OptionIndex = ClpMatchLongOption(Argument, LongOptions, Error);
    if (OptionIndex == -1) {
        optind += 1;
        optopt = 0;
        return '?';
    }

    Option = &(LongOptions[OptionIndex]);
    optind += 1;
    if (LongIndex != NULL) {
        *LongIndex = OptionIndex;
    }

    //
    // Get the argument.
    //

    if (Option->has_arg != no_argument) {

        //
        // If there's an equals, then take the argument as the part after the
        // equals.
        //

        if (Equals != NULL) {
            optarg = Equals + 1;

        //
        // Continue processing if the argument is required. If the argument is
        // optional, then an argument can only be accepted with an equals sign,
        // meaning work here is done.
        //

        } else if (Option->has_arg == required_argument) {

            //
            // If the argument is required and there isn't one, then that's a
            // problem.
            //

            if (optind >= ArgumentCount) {
                optopt = Option->val;
                if (*Options == ':') {
                    return ':';
                }

                Error->LongOption = Option->name;
                Error->Type = GetOptionErrorMissingArgument;
                return '?';

            //
            // Otherwise, use the next argument.
            //

            } else {
                optarg = Arguments[optind];
                optind += 1;
            }
        }

    //
    // No argument is expected. Fail if there is one.
    //

    } else {
        if (Equals != NULL) {
            if (*Options != ':') {
                optopt = Option->val;
                Error->LongOption = Option->name;
                Error->Type = GetOptionErrorNoArgumentExpected;
            }

            return '?';
        }
    }

    //
    // If the flag is non-null, then set *flag to the value. Otherwise, return
    // the value.
    //

    if (Option->flag == NULL) {
        return Option->val;
    }

    *(Option->flag) = Option->val;
    return 0;
}

int
ClpMatchLongOption (
    const char *Argument,
    const struct option *Options,
    PGET_OPTION_ERROR Error
    )

/*++

Routine Description:

    This routine attempts to unambiguously match an option.

Arguments:

    Argument - Supplies a pointer to the argument string, without any leading
        dashes.

    Options - Supplies the option array.

    Error - Supplies a pointer where the error will be returned on failure.

Return Value:

    Returns an index of a matching option.

    -1 if no option unambiguously matches.

--*/

{

    size_t MatchCount;
    const struct option *Option;
    int OptionIndex;
    size_t RunnerUpCount;
    size_t WinnerCount;
    int WinnerIndex;

    WinnerCount = 0;
    WinnerIndex = -1;
    RunnerUpCount = 0;

    //
    // Loop through looking for the best option and the second best option.
    //

    Option = Options;
    OptionIndex = 0;
    while (Option->name != NULL) {

        //
        // Determine how many characters match in this option, and update the
        // new winner and runner up.
        //

        MatchCount = ClpMatchLongOptionString(Argument, Option->name);
        if (MatchCount >= WinnerCount) {

            //
            // If this match is as good as the winner and the values are the
            // same, then don't update the runner up, as these options are
            // considered as one.
            //

            if ((MatchCount != WinnerCount) ||
                (WinnerIndex == -1) ||
                (Options[WinnerIndex].flag != Option->flag) ||
                (Options[WinnerIndex].val != Option->val)) {

                RunnerUpCount = WinnerCount;
            }

            WinnerCount = MatchCount;
            WinnerIndex = OptionIndex;

            //
            // If the option matches exactly, then use it.
            //

            if ((Option->name[MatchCount] == '\0') &&
                ((Argument[MatchCount] == '\0') ||
                 (Argument[MatchCount] == '='))) {

                RunnerUpCount = -1;
                break;
            }

        } else if (MatchCount > RunnerUpCount) {
            RunnerUpCount = MatchCount;
        }

        Option += 1;
        OptionIndex += 1;
    }

    //
    // If the winner doesn't match any characters, this is an unknown option.
    //

    if (WinnerCount == 0) {
        Error->Type = GetOptionErrorUnknownOption;
        Error->LongOption = Argument;
        WinnerIndex = -1;

    //
    // If the winner and the runner up match the same number of characters,
    // the result is ambiguous.
    //

    } else if (WinnerCount == RunnerUpCount) {
        Error->Type = GetOptionErrorAmbiguousOption;
        Error->LongOption = Argument;
        WinnerIndex = -1;

    } else {
        Error->Type = GetOptionErrorNone;
    }

    return WinnerIndex;
}

size_t
ClpMatchLongOptionString (
    const char *Argument,
    const char *OptionName
    )

/*++

Routine Description:

    This routine returns the number of characters that match between the two
    strings.

Arguments:

    Argument - Supplies a pointer to the argument, which is either null
        terminated or terminated with an equals. Dashes should have already
        been stripped.

    OptionName - Supplies a pointer to the option name.

Return Value:

    Returns the number of characters that match.

--*/

{

    size_t MatchCount;

    MatchCount = 0;
    while (TRUE) {

        //
        // If the option name ended, stop. If the argument name keeps going,
        // then it doesn't match at all.
        //

        if (*OptionName == '\0') {
            if ((*Argument != '\0') && (*Argument != '=')) {
                return 0;
            }

            break;

        //
        // If the argument ended, stop.
        //

        } else if ((*Argument == '\0') || (*Argument == '=')) {
            break;
        }

        //
        // If they disagree somewhere they definitely don't match.
        //

        if (*Argument != *OptionName) {
            return 0;
        }

        MatchCount += 1;
        OptionName += 1;
        Argument += 1;
    }

    return MatchCount;
}

void
ClpPrintGetOptionError (
    PGET_OPTION_ERROR Error
    )

/*++

Routine Description:

    This routine prints an error to standard error for the getopt functions if
    the user has not set the opterr variable to 0.

Arguments:

    Error - Supplies a pointer to the error information.

Return Value:

    None.

--*/

{

    PSTR BaseName;
    PSTR Copy;
    PSTR Dashes;
    PSTR Equals;
    PSTR ErrorString;
    PSTR OptionString;
    CHAR ShortOption[2];

    Copy = NULL;
    BaseName = basename(Error->CommandName);
    if (Error->LongOption != NULL) {
        Copy = strdup(Error->LongOption);
        Equals = strchr(Copy, '=');
        if (Equals != NULL) {
            *Equals = '\0';
        }

        OptionString = Copy;
        Dashes = "--";

    } else {
        ShortOption[0] = Error->Option;
        ShortOption[1] = '\0';
        Dashes = "-";
        OptionString = ShortOption;
    }

    switch (Error->Type) {
    case GetOptionErrorMissingArgument:
        ErrorString = "%s: Option %s%s requires an argument.\n";
        break;

    case GetOptionErrorUnknownOption:
        ErrorString = "%s: Unknown option %s%s.\n";
        break;

    case GetOptionErrorAmbiguousOption:
        ErrorString = "%s: Option %s%s is ambiguous.\n";
        break;

    case GetOptionErrorNoArgumentExpected:
        ErrorString = "%s: Option %s%s does not take an argument.\n";
        break;

    default:

        assert(FALSE);

        ErrorString = "%s: An unknown error occurred with %s%s.\n";
    }

    fprintf(stderr, ErrorString, BaseName, Dashes, OptionString);
    if (Copy != NULL) {
        free(Copy);
    }

    return;
}

