/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    core.h

Abstract:

    This header contains definitions for the Chalk core classes.

Author:

    Evan Green 28-May-2016

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

CK_ERROR_TYPE
CkpInitializeCore (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine initialize the Chalk VM, creating and wiring up the root
    classes.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Chalk status.

--*/

