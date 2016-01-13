/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    archsupc.c

Abstract:

    This module implements ARMv6 processor architecture features.

Author:

    Chris Stevens 19-Mar-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include <minoca/arm.h>
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

extern CHAR MmpUserModeMemoryReturn;

//
// ------------------------------------------------------------------ Functions
//

BOOL
MmpGetInstructionCacheType (
    VOID
    )

/*++

Routine Description:

    This routine returns a boolean indicating whether or not the instruction
    cache is virtually indexed.

Arguments:

    None.

Return Value:

    TRUE if the instruction cache is virtually indexed.

    FALSE if the instruction cache is physically indexed (and presumably
    physically tagged).

--*/

{

    //
    // ARMv6 is always high maintenance.
    //

    return TRUE;
}

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

