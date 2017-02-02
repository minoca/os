/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
// Define the maximum jump distance. This limitation also exists in the
// bytecode because of the argument size to the jump ops.
//

#define CK_MAX_JUMP 0x10000

//
// Define the compiler flags.
//

//
// Set this flag to print errors if compilation fails.
//

#define CK_COMPILE_PRINT_ERRORS 0x00000001

//
// Set this flag to wrap any expression statement in Core.print.
//

#define CK_COMPILE_PRINT_EXPRESSIONS 0x00000002

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
    PCSTR Source,
    UINTN Length,
    LONG Line,
    ULONG Flags
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

    Line - Supplies the line number this code starts on. Supply 1 to start at
        the beginning.

    Flags - Supplies a bitfield of flags governing the behavior of the
        compiler. See CK_COMPILE_* definitions.

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

