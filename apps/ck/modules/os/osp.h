/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    osp.h

Abstract:

    This header contains definitions for the Chalk OS module.

Author:

    Evan Green 28-Aug-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <minoca/lib/chalk.h>

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
CkpOsModuleInit (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine populates the OS module namespace.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

