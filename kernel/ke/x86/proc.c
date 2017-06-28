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
#include <minoca/kernel/x86defs.h>
#include "../kep.h"

//
// --------------------------------------------------------------------- Macros
//

//
// AMD64 uses GS since the "swapgs" instruction is an important part of the
// syscall mechanism.
//

#if defined(__amd64)

#define PROC_READ32(_Value, _Offset) GS_READ32((_Value), (_Offset))
#define PROC_READN(_Value, _Offset) GS_READN((_Value), (_Offset))

//
// x86 uses FS to keep out of user mode's way for TLS.
//

#else

#define PROC_READ32(_Value, _Offset) FS_READ32((_Value), (_Offset))
#define PROC_READN(_Value, _Offset) FS_READN((_Value), (_Offset))

#endif

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
    PROC_READ32(RunLevel, Offset);
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

    PROC_READN(Block, 0);
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

    PROC_READN(Block, 0);
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
    PROC_READ32(Number, Offset);
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
    UINTN Thread;

    Offset = FIELD_OFFSET(PROCESSOR_BLOCK, RunningThread);
    PROC_READN(Thread, Offset);
    return (PKTHREAD)Thread;
}

//
// --------------------------------------------------------- Internal Functions
//

