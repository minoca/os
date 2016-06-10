/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    compiler.h

Abstract:

    This header contains definitions for the Chalk bytecode compiler.

Author:

    Evan Green 29-May-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum number of locals that can be accessed in any given
// scope. This limitation exists in the bytecode as well, since the argument
// to load/store local is a byte.
//

#define CK_MAX_LOCALS 256

//
// Define the maximum number of upvalues that can be closed over.
//

#define CK_MAX_UPVALUES 256

//
// Define the maximum number of arguments.
//

#define CK_MAX_ARGUMENTS CK_MAX_LOCALS

//
// Define the maximum number of constants that can exist. This limitation
// exists in the bytecode as well since the argument to a constant op is a 2
// byte value.
//

#define CK_MAX_CONSTANTS 0x10000

//
// Define the maximum size of a method signature string.
//

#define CK_MAX_METHOD_SIGNATURE (CK_MAX_NAME + 8)

//
// Define the maximum jump distance. This limitation also exists in the
// bytecode because of the argument size to the jump ops.
//

#define CK_MAX_JUMP 0x10000

//
// Define a reasonable size for error messages.
//

#define CK_MAX_ERROR_MESSAGE (CK_MAX_NAME + 128)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

PCK_FUNCTION
CkpCompile (
    PCK_VM Vm,
    PCK_MODULE Module,
    PSTR Source,
    UINTN Length,
    BOOL PrintErrors
    );

/*++

Routine Description:

    This routine compiles Chalk source code into bytecode.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Module - Supplies a pointer to the module to compile into.

    Source - Supplies a pointer to the null-terminated source file.

    Length - Supplies the length of the source string, not including the null
        terminator.

    PrintErrors - Supplies a boolean indicating whether or not errors should be
        printed.

Return Value:

    Returns a pointer to a newly compiled function for the module on success.

    NULL on failure, and the virtual machine error will be set to contain more
    information.

--*/

VOID
CkpCompileError (
    PCK_COMPILER Compiler,
    PVOID Token,
    PSTR Format,
    ...
    );

/*++

Routine Description:

    This routine reports a compile error.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Token - Supplies an optional pointer to the token where everything went
        wrong.

    Format - Supplies the printf-style format string of the error.

    ... - Supplies the remaining arguments.

Return Value:

    None.

--*/

