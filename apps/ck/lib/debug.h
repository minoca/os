/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

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

VOID
CkpDebugPrintStackTrace (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine prints a stack trace of the current fiber.

Arguments:

    Vm - Supplies a pointer to the VM.

Return Value:

    None.

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
