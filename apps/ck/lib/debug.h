/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    debug.h

Abstract:

    This header contains debug definitions for the Chalk interpreter.

Author:

    Evan Green 22-Jun-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

CK_VALUE
CkpCreateStackTrace (
    PCK_VM Vm,
    UINTN Skim
    );

/*++

Routine Description:

    This routine creates a stack trace object from the current fiber.

Arguments:

    Vm - Supplies a pointer to the VM.

    Skim - Supplies the number of most recently called functions not to include
        in the stack trace. This is usually 0 for exceptions created in C and
        1 for exceptions created in Chalk.

Return Value:

    Returns a list of lists containing the stack trace. The first element is
    the least recently called. Each elements contains a list of 3 elements:
    the module name, the function name, and the line number.

    CK_NULL_VALUE on allocation failure.

--*/

VOID
CkpDumpCode (
    PCK_VM Vm,
    PCK_FUNCTION Function
    );

/*++

Routine Description:

    This routine prints the bytecode assembly for the given function.

Arguments:

    Vm - Supplies a pointer to the VM.

    Function - Supplies a pointer to the function containing the bytecode.

Return Value:

    None.

--*/

VOID
CkpDumpStack (
    PCK_VM Vm,
    PCK_FIBER Fiber
    );

/*++

Routine Description:

    This routine prints the current contents of the stack for the most recent
    call frame.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Fiber - Supplies a pointer to the fiber.

Return Value:

    None.

--*/

INTN
CkpDumpInstruction (
    PCK_VM Vm,
    PCK_FUNCTION Function,
    UINTN Offset,
    PLONG LastLine
    );

/*++

Routine Description:

    This routine prints the bytecode for a single instruction.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Function - Supplies a pointer to the function.

    Offset - Supplies the offset into the function code to print from.

    LastLine - Supplies an optional pointer where the last line number printed
        is given on input. On output, returns the line number of this
        instruction.

Return Value:

    Returns the length of this instruction.

    -1 if there are no more instructions.

--*/

INT
CkpGetLineForOffset (
    PCK_FUNCTION Function,
    UINTN CodeOffset
    );

/*++

Routine Description:

    This routine determines what line the given bytecode offset is on.

Arguments:

    Function - Supplies a pointer to the function containing the bytecode.

    CodeOffset - Supplies the offset whose line number is desired.

Return Value:

    Returns the line number the offset in question.

    -1 if no line number information could be found.

--*/

VOID
CkpDebugPrint (
    PCK_VM Vm,
    PSTR Message,
    ...
    );

/*++

Routine Description:

    This routine prints something to the output for the debug code.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Message - Supplies the printf-style format message to print.

    ... - Supplies the remainder of the arguments.

Return Value:

    None.

--*/
