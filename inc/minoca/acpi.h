/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    acpi.h

Abstract:

    This header contains definitions for the kernel's implementation of support
    for the Advanced Configuration and Power Interface (ACPI).

Author:

    Evan Green 4-Aug-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/acpitabs.h>

//
// ---------------------------------------------------------------- Definitions
//

#define ACPI_ALLOCATION_TAG 0x49504341 // 'IPCA'

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

extern PVOID AcpiTables;
extern ULONG AcpiTablesSize;

//
// -------------------------------------------------------- Function Prototypes
//

KERNEL_API
PVOID
AcpiFindTable (
    ULONG Signature,
    PVOID PreviousTable
    );

/*++

Routine Description:

    This routine attempts to find an ACPI description table with the given
    signature. This routine can also be used to find the SMBIOS table.

Arguments:

    Signature - Supplies the signature of the desired table.

    PreviousTable - Supplies a pointer to the table to start the search from.

Return Value:

    Returns a pointer to the beginning of the header to the table if the table
    was found, or NULL if the table could not be located.

--*/

