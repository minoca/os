/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/arm.h>
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

    return ArDereferenceProcessorBlock(FIELD_OFFSET(PROCESSOR_BLOCK, RunLevel));
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

    return ArGetProcessorBlockRegister();
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

    return ArGetProcessorBlockRegisterForDebugger();
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

    ULONG ProcessorNumber;

    ProcessorNumber = (ULONG)ArDereferenceProcessorBlock(
                               FIELD_OFFSET(PROCESSOR_BLOCK, ProcessorNumber));

    return ProcessorNumber;
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

    PKTHREAD Thread;

    Thread = (PVOID)ArDereferenceProcessorBlock(
                                 FIELD_OFFSET(PROCESSOR_BLOCK, RunningThread));

    return Thread;
}

//
// --------------------------------------------------------- Internal Functions
//

