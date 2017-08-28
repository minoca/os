/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dlopen.c

Abstract:

    This module implements the performance benchmark tests for the dlopen() and
    dlclose() C library calls.

Author:

    Chris Stevens 6-May-2015

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <stdlib.h>
#include <errno.h>
#include <dlfcn.h>
#include <string.h>

#include "perftest.h"

#include <stdio.h>

//
// ---------------------------------------------------------------- Definitions
//

#define PT_DLOPEN_LIBRARY_NAME "perflib.so"

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
void
(*PPT_LIBRARY_INITIALIZE_ROUTINE) (
    void
    );

/*++

Routine Description:

    This routine initializes the stub performance test library.

Arguments:

    None.

Return Value:

    None.

--*/

typedef
int
(*PPT_IS_LIBRARY_INITIALIZED_ROUTINE) (
    void
    );

/*++

Routine Description:

    This routine determines whether or not the library is initialized.

Arguments:

    None.

Return Value:

    Returns 1 if the library has been initialized, or 0 otherwise.

--*/

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

void
DlopenMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    )

/*++

Routine Description:

    This routine performs the dynamic library open performance benchmark test.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

{

    void *Handle;
    int Initialized;
    unsigned long long Iterations;
    char *LastSlash;
    char *LibraryName;
    size_t LibraryNameLength;
    PPT_IS_LIBRARY_INITIALIZED_ROUTINE PtIsLibraryInitialized;
    PPT_LIBRARY_INITIALIZE_ROUTINE PtLibraryInitialize;
    int Status;

    Iterations = 0;
    Result->Type = PtResultIterations;
    Result->Status = 0;

    //
    // The stub performance library should be sitting in the same directory as
    // the performance test. So, take the program name, strip off the last
    // portion of the path and replace it with the name of the stub library.
    //

    LibraryNameLength = strlen(PT_DLOPEN_LIBRARY_NAME) + strlen(PtProgramPath);
    LibraryNameLength += 1;
    LibraryName = malloc(LibraryNameLength);
    if (LibraryName == NULL) {
        Result->Status = ENOMEM;
        goto MainEnd;
    }

    strncpy(LibraryName, PtProgramPath, LibraryNameLength);
    LastSlash = strrchr(LibraryName, '/');
    if (LastSlash == NULL) {
        LastSlash = strrchr(LibraryName, '\\');
    }

    if (LastSlash == NULL) {
        strncpy(LibraryName, PT_DLOPEN_LIBRARY_NAME, LibraryNameLength);

    } else {
        LastSlash += 1;
        strncpy(LastSlash,
                PT_DLOPEN_LIBRARY_NAME,
                LibraryNameLength - (LastSlash - LibraryName));
    }

    //
    // Start the test. This snaps resource usage and starts the clock ticking.
    //

    Status = PtStartTimedTest(Test->Duration);
    if (Status != 0) {
        Result->Status = errno;
        goto MainEnd;
    }

    //
    // Measure the performance of the dlopen(), dlsym(), and dlclose() C
    // library routines by counting the number of times a library can be opened,
    // called, and closed.
    //

    while (PtIsTimedTestRunning() != 0) {
        Handle = dlopen(LibraryName, RTLD_NOW | RTLD_GLOBAL);
        if (Handle == NULL) {
            fprintf(stderr,
                    "Failed to open %s: %s\n",
                    LibraryName,
                    dlerror());

            Result->Status = ENOENT;
            break;
        }

        PtLibraryInitialize = dlsym(Handle, "PtLibraryInitialize");
        if (PtLibraryInitialize == NULL) {
            Result->Status = ENOSYS;
            dlclose(Handle);
            break;
        }

        PtLibraryInitialize();
        PtIsLibraryInitialized = dlsym(Handle, "PtIsLibraryInitialized");
        if (PtIsLibraryInitialized == NULL) {
            Result->Status = ENOSYS;
            dlclose(Handle);
            break;
        }

        Initialized = PtIsLibraryInitialized();
        if (Initialized == 0) {
            Result->Status = EAGAIN;
            dlclose(Handle);
            break;
        }

        Status = dlclose(Handle);
        if (Status != 0) {
            Result->Status = EBADF;
            break;
        }

        Iterations += 1;
    }

    Status = PtFinishTimedTest(Result);
    if ((Status != 0) && (Result->Status == 0)) {
        Result->Status = errno;
    }

MainEnd:
    if (LibraryName != NULL) {
        free(LibraryName);
    }

    Result->Data.Iterations = Iterations;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

