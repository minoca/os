/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    archsupc.c

Abstract:

    This module contains architecture-specific support functions for the kernel
    memory manager.

Author:

    Evan Green 11-Jun-2017

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/x64.h>
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

ULONG MmDataCacheLineSize = 1;

extern CHAR MmpUserModeMemoryReturn;

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

    InstructionPointer = (PVOID)(UINTN)(TrapFrame->Rip);
    if ((InstructionPointer >= (PVOID)MmpCopyUserModeMemory) &&
        (InstructionPointer < (PVOID)&MmpUserModeMemoryReturn)) {

        TrapFrame->Rip = (UINTN)&MmpUserModeMemoryReturn;
        TrapFrame->Rax = FALSE;
        return TRUE;
    }

    return FALSE;
}

//
// --------------------------------------------------------- Internal Functions
//

