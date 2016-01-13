/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    runlevel.c

Abstract:

    This module handles run level management.

Author:

    Evan Green 27-Aug-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include "kep.h"

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

//
// --------------------------------------------------------- Internal Functions
//

ULONG
KeGetActiveProcessorCount (
    VOID
    )

/*++

Routine Description:

    This routine gets the number of processors currently running in the
    system.

Arguments:

    None.

Return Value:

    Returns the number of active processors currently in the system.

--*/

{

    return KeActiveProcessorCount;
}

KERNEL_API
RUNLEVEL
KeRaiseRunLevel (
    RUNLEVEL RunLevel
    )

/*++

Routine Description:

    This routine raises the running level of the current processor to the given
    level.

Arguments:

    RunLevel - Supplies the new running level of the current processor.

Return Value:

    Returns the old running level of the processor.

--*/

{

    return HlRaiseRunLevel(RunLevel);
}

KERNEL_API
VOID
KeLowerRunLevel (
    RUNLEVEL RunLevel
    )

/*++

Routine Description:

    This routine lowers the running level of the current processor to the given
    level.

Arguments:

    RunLevel - Supplies the new running level of the current processor.

Return Value:

    None.

--*/

{

    return HlLowerRunLevel(RunLevel);
}

