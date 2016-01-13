/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    proc.c

Abstract:

    This module implements processor-related functionality for the kernel.

Author:

    Evan Green 6-Aug-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include <minoca/x86.h>
#include "../kep.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
RUNLEVEL
KeGetRunLevel (
    VOID
    )

/*++

Routine Description:

    This routine gets the running level for the current processor.

Arguments:

    None.

Return Value:

    Returns the current run level.

--*/

{

    ULONG Offset;
    ULONG RunLevel;

    Offset = FIELD_OFFSET(PROCESSOR_BLOCK, RunLevel);
    GET_PROCESSOR_BLOCK_OFFSET(RunLevel, Offset);
    return RunLevel;
}

PPROCESSOR_BLOCK
KeGetCurrentProcessorBlock (
    VOID
    )

/*++

Routine Description:

    This routine gets the processor state for the currently executing processor.

Arguments:

    None.

Return Value:

    Returns the current processor block.

--*/

{

    PPROCESSOR_BLOCK Block;

    GET_PROCESSOR_BLOCK_OFFSET(Block, 0);
    return Block;
}

PPROCESSOR_BLOCK
KeGetCurrentProcessorBlockForDebugger (
    VOID
    )

/*++

Routine Description:

    This routine gets the processor block for the currently executing
    processor. It is intended to be called only by the debugger.

Arguments:

    None.

Return Value:

    Returns the current processor block.

--*/

{

    PPROCESSOR_BLOCK Block;

    GET_PROCESSOR_BLOCK_OFFSET(Block, 0);
    return Block;
}

ULONG
KeGetCurrentProcessorNumber (
    VOID
    )

/*++

Routine Description:

    This routine gets the processor number for the currently executing
    processor.

Arguments:

    None.

Return Value:

    Returns the current zero-indexed processor number.

--*/

{

    ULONG Number;
    ULONG Offset;

    Offset = FIELD_OFFSET(PROCESSOR_BLOCK, ProcessorNumber);
    GET_PROCESSOR_BLOCK_OFFSET(Number, Offset);
    return Number;
}

PKTHREAD
KeGetCurrentThread (
    VOID
    )

/*++

Routine Description:

    This routine gets the current thread running on this processor.

Arguments:

    None.

Return Value:

    Returns the current run level.

--*/

{

    ULONG Offset;
    ULONG Thread;

    Offset = FIELD_OFFSET(PROCESSOR_BLOCK, RunningThread);
    GET_PROCESSOR_BLOCK_OFFSET(Thread, Offset);
    return (PKTHREAD)Thread;
}

//
// --------------------------------------------------------- Internal Functions
//

