/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    getopt.h

Abstract:

    This header contains definitions for the non-standard getopt functions that
    parse command line options and support long arguments.

Author:

    Evan Green 21-Aug-2013

--*/

#ifndef _GETOPT_H
#define _GETOPT_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Definitions for the has_arg field of the option structure.
//

#define no_argument 0
#define required_argument 1
#define optional_argument 2

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the option structure used to define a single long
    command line option.

Members:

    name - Stores a pointer to the null terminated string containing the name
        of the long option.

    has_arg - Stores a flag. Valid values are no_argument if the long option
        does not take an argument, required_argument if the long option must
        take an argument, or optional_argument if the long option can either
        take or not take an argument.

    flag - Stores a pointer where a value should be set. If this is NULL,
        then getopt_long returns the value member. Otherwise, getopt_long
        returns 0, and this member points to a variable of type int which is
        set to the value member if the option is found (and left unchanged if
        the option is not encountered).

    val - Stores the value to either return or set in the flag pointer.

--*/

struct option {
    const char *name;
    int has_arg;
    int *flag;
    int val;
};

//
// -------------------------------------------------------------------- Globals
//

//
// Define the global that points to the argument if the getopt function finds
// an option that takes an argument.
//

LIBC_API extern char *optarg;

//
// Define the global that contains the index of the next argument to be
// processed by the getopt function.
//

LIBC_API extern int optind;

//
// Define the global that controls whether or not an error message is printed
// to standrad error when the getopt function detects an error. The user can
// set this to 0 to disable such messages.
//

LIBC_API extern int opterr;

//
// Define the global that is set to the unknown option if an option is passed
// in the arguments that is not in the options string during a call to getopt.
//

LIBC_API extern int optopt;

//
// Define the global that can be used to reset the option system so that it
// can be called with a different array or called repeatedly on the same array.
// Setting optind to zero has the same effect as setting optreset to non-zero.
//

LIBC_API extern int optreset;

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
getopt_long (
    int ArgumentCount,
    char *const Arguments[],
    const char *ShortOptions,
    const struct option *LongOptions,
    int *LongIndex
    );

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

LIBC_API
int
getopt_long_only (
    int ArgumentCount,
    char *const Arguments[],
    const char *ShortOptions,
    const struct option *LongOptions,
    int *LongIndex
    );

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

#ifdef __cplusplus

}

#endif
#endif

