/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    hlp.h

Abstract:

    This header contains internal definitions for the hardware layer library.

Author:

    Evan Green 28-Oct-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the amount of time to wait for a system reset to take effect in
// microseconds before moving on.
//

#define RESET_SYSTEM_STALL (5 * MICROSECONDS_PER_SECOND)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Define the kernel services table for hardware modules.
//

extern HARDWARE_MODULE_KERNEL_SERVICES HlHardwareModuleServices;

//
// -------------------------------------------------------- Function Prototypes
//

PVOID
HlpModAllocateMemory (
    UINTN Size,
    ULONG Tag,
    BOOL Device,
    PPHYSICAL_ADDRESS PhysicalAddress
    );

/*++

Routine Description:

    This routine allocates memory from the non-paged pool. This memory will
    never be paged out and can be accessed at any level.

Arguments:

    Size - Supplies the size of the allocation, in bytes.

    Tag - Supplies an identifier to associate with the allocation, useful for
        debugging and leak detection.

    Device - Supplies a boolean indicating if this memory will be accessed by
        a device directly. If TRUE, the memory will be mapped uncached.

    PhysicalAddress - Supplies an optional pointer where the physical address
        of the allocation is returned.

Return Value:

    Returns the allocated memory if successful, or NULL on failure.

--*/

KSTATUS
HlpArchResetSystem (
    SYSTEM_RESET_TYPE ResetType
    );

/*++

Routine Description:

    This routine resets the system.

Arguments:

    ResetType - Supplies the desired reset type. If the desired reset type is
        not supported, a cold reset will be attempted.

Return Value:

    Does not return on success, the system resets.

    STATUS_NOT_SUPPORTED if the system cannot be reset.

    STATUS_UNSUCCESSFUL if the system did not reset.

--*/
