/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    vmsys.c

Abstract:

    This module implements the default support functions needed to wire a
    Chalk interpreter up to the rest of the system in the default configuration.

Author:

    Evan Green 28-May-2016

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <stdio.h>
#include <stdlib.h>

#include "chalkp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define some default garbage collection parameters.
//

#define CK_INITIAL_HEAP_DEFAULT (1024 * 1024 * 10)
#define CK_MINIMUM_HEAP_DEFAULT (1024 * 1024)
#define CK_HEAP_GROWTH_DEFAULT 50

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PVOID
CkpDefaultReallocate (
    PVOID Allocation,
    UINTN NewSize
    );

PSTR
CkpDefaultLoadModule (
    PCK_VM Vm,
    PSTR ModuleName
    );

VOID
CkpDefaultWrite (
    PCK_VM Vm,
    PSTR String
    );

VOID
CkpDefaultError (
    PCK_VM Vm,
    CK_ERROR_TYPE ErrorType,
    PSTR Module,
    INT Line,
    PSTR Message
    );

//
// -------------------------------------------------------------------- Globals
//

CK_CONFIGURATION CkDefaultConfiguration = {
    CkpDefaultReallocate,
    CkpDefaultLoadModule,
    CkpDefaultWrite,
    CkpDefaultError,
    CK_INITIAL_HEAP_DEFAULT,
    CK_MINIMUM_HEAP_DEFAULT,
    CK_HEAP_GROWTH_DEFAULT
};

//
// ------------------------------------------------------------------ Functions
//

PVOID
CkpDefaultReallocate (
    PVOID Allocation,
    UINTN NewSize
    )

/*++

Routine Description:

    This routine contains the default Chalk reallocate routine, which wires up
    to the C library realloc function.

Arguments:

    Allocation - Supplies an optional pointer to the allocation to resize or
        free. If NULL, then this routine will allocate new memory.

    NewSize - Supplies the size of the desired allocation. If this is 0 and the
        allocation parameter is non-null, the given allocation will be freed.
        Otherwise it will be resized to requested size.

Return Value:

    Returns a pointer to the allocation on success.

    NULL on allocation failure, or in the case the memory is being freed.

--*/

{

    return realloc(Allocation, NewSize);
}

PSTR
CkpDefaultLoadModule (
    PCK_VM Vm,
    PSTR ModuleName
    )

/*++

Routine Description:

    This routine is called to load a new Chalk module.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ModuleName - Supplies the name of the module to load.

Return Value:

    Returns a pointer to a string containing the source code of the module.
    This memory should be allocated from the heap, and will be freed by Chalk.

--*/

{

    //
    // TODO: Implement LoadModule.
    //

    return NULL;
}

VOID
CkpDefaultWrite (
    PCK_VM Vm,
    PSTR String
    )

/*++

Routine Description:

    This routine is called to print text in Chalk.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    String - Supplies a pointer to the string to print. This routine should not
        modify or free this string.

Return Value:

    None.

--*/

{

    printf("%s", String);
    return;
}

VOID
CkpDefaultError (
    PCK_VM Vm,
    CK_ERROR_TYPE ErrorType,
    PSTR Module,
    INT Line,
    PSTR Message
    )

/*++

Routine Description:

    This routine when the Chalk interpreter experiences an error.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ErrorType - Supplies the type of error occurring.

    Module - Supplies a pointer to the module the error occurred in.

    Line - Supplies the line number the error occurred on.

    Message - Supplies a pointer to a string describing the error.

Return Value:

    None.

--*/

{

    if (Message == NULL) {
        Message = "";
    }

    if (Module == NULL) {
        Module = "<none>";
    }

    switch (ErrorType) {
    case CkErrorNoMemory:
        fprintf(stderr, "%s:%d: Allocation failure\n", Module, Line);
        break;

    case CkErrorStackTrace:
        fprintf(stderr, "  in %s at %s:%d\n", Message, Module, Line);
        break;

    case CkErrorCompile:
    case CkErrorRuntime:
    default:
        fprintf(stderr, "%s:%d: %s.\n", Module, Line, Message);
        break;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

