/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    archcomc.c

Abstract:

    This module implements architecture specific C support routines common to
    ARMv6 and ARMv7.

Author:

    Evan Green 16-Sep-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/arm.h>
#include "../mmp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// Define the address to jump to if a fault occurred in one of the user mode
// memory access routines.
//

extern CHAR MmpUserModeMemoryReturn;

//
// Define cache line sizes for the CPU L1 caches.
//

ULONG MmDataCacheLineSize;
ULONG MmInstructionCacheLineSize;

//
// Store whether or not the instruction caches are virtually indexed. If it is,
// then whenever a mapping is changed that may be executable, it needs to be
// invalidated in the instruction cache.
//

BOOL MmVirtuallyIndexedInstructionCache;

//
// ------------------------------------------------------------------ Functions
//

BOOL
MmpCheckUserModeCopyRoutines (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine determines if a given fault occurred inside a user mode memory
    manipulation function, and adjusts the instruction pointer if so.

Arguments:

    TrapFrame - Supplies a pointer to the state of the machine when the page
        fault occurred.

Return Value:

    None.

--*/

{

    PVOID InstructionPointer;

    InstructionPointer = (PVOID)(TrapFrame->Pc);
    if ((InstructionPointer >= (PVOID)MmpCopyUserModeMemory) &&
        (InstructionPointer < (PVOID)&MmpUserModeMemoryReturn)) {

        TrapFrame->Pc = (UINTN)&MmpUserModeMemoryReturn;
        TrapFrame->R0 = FALSE;
        return TRUE;
    }

    return FALSE;
}

//
// --------------------------------------------------------- Internal Functions
//

