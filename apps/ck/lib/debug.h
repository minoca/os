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
