/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    keinit.h

Abstract:

    This header contains private definitions for the Kernel Executive
    initialization.

Author:

    Evan Green 30-Jan-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/bootload.h>

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

KSTATUS
KepArchInitialize (
    PKERNEL_INITIALIZATION_BLOCK Parameters,
    ULONG Phase
    );

/*++

Routine Description:

    This routine performs architecture-specific initialization for the kernel
    executive.

Arguments:

    Parameters - Supplies a pointer to the kernel initialization parameters
        from the loader.

    Phase - Supplies the initialization phase.

Return Value:

    Status code.

--*/

KSTATUS
KepInitializeSystemResources (
    PKERNEL_INITIALIZATION_BLOCK Parameters,
    ULONG Phase
    );

/*++

Routine Description:

    This routine initializes the system resource manager.

Arguments:

    Parameters - Supplies a pointer to the kernel initialization block.

    Phase - Supplies the phase. Valid values are 0 and 1.

Return Value:

    Status code.

--*/

PSYSTEM_RESOURCE_HEADER
KepGetSystemResource (
    SYSTEM_RESOURCE_TYPE ResourceType,
    BOOL Acquire
    );

/*++

Routine Description:

    This routine attempts to find an unacquired system resource of the given
    type.

Arguments:

    ResourceType - Supplies the type of builtin resource to acquire.

    Acquire - Supplies a boolean indicating if the resource should be acquired
        or not.

Return Value:

    Returns a pointer to a resource of the given type on success.

    NULL on failure.

--*/

KSTATUS
KepInitializeCrashDumpSupport (
    VOID
    );

/*++

Routine Description:

    This routine initializes system crash dump support.

Arguments:

    None.

Return Value:

    Status code.

--*/

