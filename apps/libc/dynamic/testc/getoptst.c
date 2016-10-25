/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    getoptst.c

Abstract:

    This module implements the getopt tests.

Author:

    Evan Green 22-Aug-2013

Environment:

    Test

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Define this so it doesn't get defined to an import.
//

#define LIBC_API

#include <minoca/lib/types.h>

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//
// ---------------------------------------------------------------- Definitions
//

#define TEST_ARGUMENT_COUNT 11
#define TEST_SHORT_OPTIONS_STRING ":a:bcdef:"

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _TEST_OPTION_CASE {
    PSTR *Arguments;
    INT ArgumentCount;
    INT ExpectedResult;
    INT ExpectedCallCount;
} TEST_OPTION_CASE, *PTEST_OPTION_CASE;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// Define the valid long arguments, including one past the end that should
// never match.
//

struct option TestLongOptions[5] = {
    {"myarg1", required_argument, 0, 'm'},
    {"myag2", optional_argument, 0, 'n'},
    {"diaper", no_argument, 0, 'd'},
    {NULL, 0, 0, 0},
    {"inval", no_argument, 0, 'i'}
};

PSTR TestArguments[TEST_ARGUMENT_COUNT + 1] = {
    "0",
    "--myarg1",
    "--myag2",
    "--diaper",
    "--myag2",
    "--myag2=arg2",
    "--myarg1=myval1",
    "-bcdamyaval",
    "-f",
    "myfarg",
    "--inval",
    NULL
};

//
// Define standard error as the getopt file expects it to be actually
// defined.
//

FILE *stderr = NULL;

//
// ------------------------------------------------------------------ Functions
//

ULONG
TestGetopt (
    VOID
    )

/*++

Routine Description:

    This routine tests the getopt functions.

Arguments:

    None.

Return Value:

    Returns the count of test failures.

--*/

{

    ULONG Failures;
    int LongIndex;
    int Result;

    Failures = 0;
    Result = getopt_long(TEST_ARGUMENT_COUNT,
                         TestArguments,
                         TEST_SHORT_OPTIONS_STRING,
                         TestLongOptions,
                         &LongIndex);

    if ((Result != 'm') || (optind != 3) || (strcmp(optarg, "--myag2") != 0) ||
        (LongIndex != 0)) {

        printf("getopt: Failed. optind is %d.\n", optind);
        Failures += 1;
    }

    Result = getopt_long(TEST_ARGUMENT_COUNT,
                         TestArguments,
                         TEST_SHORT_OPTIONS_STRING,
                         TestLongOptions,
                         &LongIndex);

    if ((Result != 'd') || (optind != 4) || (LongIndex != 2)) {
        printf("getopt: Failed. optind is %d.\n", optind);
        Failures += 1;
    }

    Result = getopt_long(TEST_ARGUMENT_COUNT,
                         TestArguments,
                         TEST_SHORT_OPTIONS_STRING,
                         TestLongOptions,
                         &LongIndex);

    if ((Result != 'n') || (optind != 5) || (optarg != NULL) ||
        (LongIndex != 1)) {

      printf("getopt: Failed. optind is %d.\n", optind);
      Failures += 1;
    }

    Result = getopt_long(TEST_ARGUMENT_COUNT,
                         TestArguments,
                         TEST_SHORT_OPTIONS_STRING,
                         TestLongOptions,
                         &LongIndex);

    if ((Result != 'n') || (optind != 6) || (strcmp(optarg, "arg2") != 0) ||
        (LongIndex != 1)) {

        printf("getopt: Failed. optind is %d.\n", optind);
        Failures += 1;
    }

    Result = getopt_long(TEST_ARGUMENT_COUNT,
                         TestArguments,
                         TEST_SHORT_OPTIONS_STRING,
                         TestLongOptions,
                         NULL);

    if ((Result != 'm') || (optind != 7) || (strcmp(optarg, "myval1") != 0)) {
        printf("getopt: Failed. optind is %d.\n", optind);
        Failures += 1;
    }

    //
    // Now for the little ones: -bcda.
    //

    Result = getopt_long(TEST_ARGUMENT_COUNT,
                         TestArguments,
                         TEST_SHORT_OPTIONS_STRING,
                         TestLongOptions,
                         NULL);

    if ((Result != 'b') || (optind != 7) || (optarg != NULL)) {
        printf("getopt: Failed. optind is %d.\n", optind);
        Failures += 1;
    }

    Result = getopt_long(TEST_ARGUMENT_COUNT,
                         TestArguments,
                         TEST_SHORT_OPTIONS_STRING,
                         TestLongOptions,
                         NULL);

    if ((Result != 'c') || (optind != 7) || (optarg != NULL)) {
        printf("getopt: Failed. optind is %d.\n", optind);
        Failures += 1;
    }

    Result = getopt_long(TEST_ARGUMENT_COUNT,
                         TestArguments,
                         TEST_SHORT_OPTIONS_STRING,
                         TestLongOptions,
                         NULL);

    if ((Result != 'd') || (optind != 7) || (optarg != NULL)) {
        printf("getopt: Failed. optind is %d.\n", optind);
        Failures += 1;
    }

    Result = getopt_long(TEST_ARGUMENT_COUNT,
                         TestArguments,
                         TEST_SHORT_OPTIONS_STRING,
                         TestLongOptions,
                         NULL);

    if ((Result != 'a') || (optind != 8) || (strcmp(optarg, "myaval") != 0)) {
        printf("getopt: Failed. optind is %d.\n", optind);
        Failures += 1;
    }

    Result = getopt_long(TEST_ARGUMENT_COUNT,
                         TestArguments,
                         TEST_SHORT_OPTIONS_STRING,
                         TestLongOptions,
                         NULL);

    if ((Result != 'f') || (optind != 10) || (strcmp(optarg, "myfarg") != 0)) {
        printf("getopt: Failed. optind is %d.\n", optind);
        Failures += 1;
    }

    //
    // Finally, the unknown option.
    //

    opterr = 0;
    Result = getopt_long(TEST_ARGUMENT_COUNT,
                         TestArguments,
                         TEST_SHORT_OPTIONS_STRING,
                         TestLongOptions,
                         NULL);

    if ((Result != '?') || (optind != 11) || (optarg != NULL)) {
        printf("getopt: Failed. optind is %d.\n", optind);
        Failures += 1;
    }

    return Failures;
}

//
// --------------------------------------------------------- Internal Functions
//

