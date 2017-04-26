/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    init.c

Abstract:

    This module implements C library initialization that is statically linked
    into every application.

Author:

    Evan Green 4-Mar-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/minocaos.h>
#include <stdlib.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
VOID
(*PSTATIC_CONSTRUCTOR_DESTRUCTOR) (
    VOID
    );

/*++

Routine Description:

    This routine defines the prototype for functions in the .preinit_array,
    .init_array, or .fini_array.

Arguments:

    None.

Return Value:

    None.

--*/

//
// ----------------------------------------------- Internal Function Prototypes
//

static
VOID
ClpCallDestructors (
    VOID
    );

//
// Define a prototype for the standard main function, whose implementation is
// up to the programmer.
//

int
main (
    );

//
// Define functions emitted by the compiler for static constructors and
// destructors.
//

void
_init (
    void
    );

void
_fini (
    void
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define symbols in the linker script for accessing preinit_array, init_array,
// and fini_array.
//

extern void *__preinit_array_start;
extern void *__preinit_array_end;
extern void *__init_array_start;
extern void *__init_array_end;
extern void *__fini_array_start;
extern void *__fini_array_end;

//
// ------------------------------------------------------------------ Functions
//

VOID
ClApplicationInitialize (
    PPROCESS_ENVIRONMENT Environment
    )

/*++

Routine Description:

    This routine initializes the C application.

Arguments:

    Environment - Supplies a pointer to the environment information.

Return Value:

    None.

--*/

{

    PSTATIC_CONSTRUCTOR_DESTRUCTOR CurrentFunction;
    void **CurrentPointer;
    INT Result;

    atexit(ClpCallDestructors);

    //
    // Call the elements in the .preinit_array first.
    //

    CurrentPointer = &__preinit_array_start;
    while (CurrentPointer < &__preinit_array_end) {
        CurrentFunction = *CurrentPointer;
        CurrentFunction();
        CurrentPointer += 1;
    }

    //
    // Now call the _init routine, followed by the .init_array.
    //

    _init();
    CurrentPointer = &__init_array_start;
    while (CurrentPointer < &__init_array_end) {
        CurrentFunction = *CurrentPointer;
        CurrentFunction();
        CurrentPointer += 1;
    }

    //
    // Launch out to the application.
    //

    Result = main(Environment->ArgumentCount,
                  Environment->Arguments,
                  Environment->Environment);

    exit(Result);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

static
VOID
ClpCallDestructors (
    VOID
    )

/*++

Routine Description:

    This routine calls the destructor functions.

Arguments:

    Environment - Supplies a pointer to the environment information.

Return Value:

    None.

--*/

{

    PSTATIC_CONSTRUCTOR_DESTRUCTOR CurrentFunction;
    void **CurrentPointer;

    //
    // Call the fini_array functions in reverse order, and then the _fini
    // function.
    //

    if (&__fini_array_start != &__fini_array_end) {
        CurrentPointer = &__fini_array_end - 1;
        while (CurrentPointer >= &__fini_array_start) {
            CurrentFunction = *CurrentPointer;
            CurrentFunction();
            CurrentPointer -= 1;
        }
    }

    _fini();
    return;
}

