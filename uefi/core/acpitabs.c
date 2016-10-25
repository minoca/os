/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    acpitabs.c

Abstract:

    This module implements support for getting ACPI tables from the EFI system
    table.

Author:

    Chris Stevens 3-May-2016

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"
#include <minoca/fw/acpitabs.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PRSDP
EfipGetRsdp (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
VOID *
EfiGetAcpiTable (
    UINT32 Signature,
    VOID *PreviousTable
    )

/*++

Routine Description:

    This routine attempts to find an ACPI description table with the given
    signature. This routine does not validate the checksum of the table.

Arguments:

    Signature - Supplies the signature of the desired table.

    PreviousTable - Supplies an optional pointer to the table to start the
        search from.

Return Value:

    Returns a pointer to the beginning of the header to the table if the table
    was found, or NULL if the table could not be located.

--*/

{

    PRSDP Rsdp;
    PRSDT Rsdt;
    UINTN RsdtIndex;
    PULONG RsdtTableEntry;
    PDESCRIPTION_HEADER Table;
    UINTN TableCount;
    UINTN TableIndex;

    Rsdp = EfipGetRsdp();
    if (Rsdp == NULL) {
        return NULL;
    }

    Rsdt = (PRSDT)(Rsdp->RsdtAddress);
    if (Rsdt == NULL) {
        return NULL;
    }

    TableCount = (Rsdt->Header.Length - sizeof(DESCRIPTION_HEADER)) /
                 sizeof(UINT32);

    RsdtTableEntry = (PULONG)&(Rsdt->Entries);

    //
    // Search the list of pointers, but do it backwards. This runs on the
    // assumption that if there are two tables in the firmware, the later one
    // is the better one.
    //

    for (TableIndex = 0; TableIndex < TableCount; TableIndex += 1) {
        RsdtIndex = TableCount - TableIndex - 1;
        Table = (PDESCRIPTION_HEADER)(UINTN)(RsdtTableEntry[RsdtIndex]);
        if (Table == NULL) {
            continue;
        }

        if (PreviousTable != NULL) {
            if (Table == PreviousTable) {
                PreviousTable = NULL;
            }

            continue;
        }

        if (Table->Signature == Signature) {
            return Table;
        }
    }

    return NULL;
}

//
// --------------------------------------------------------- Internal Functions
//

PRSDP
EfipGetRsdp (
    VOID
    )

/*++

Routine Description:

    This routine attempts to find the RSDP in the EFI system table.

Arguments:

    None.

Return Value:

    Returns a pointer to the RSDP on success.

    NULL on failure.

--*/

{

    BOOLEAN Match;
    EFI_CONFIGURATION_TABLE *Table;
    UINTN TableCount;
    UINTN TableIndex;

    TableCount = EfiSystemTable->NumberOfTableEntries;
    for (TableIndex = 0; TableIndex < TableCount; TableIndex += 1) {
        Table = &(EfiSystemTable->ConfigurationTable[TableIndex]);
        Match = EfiCoreCompareGuids(&(Table->VendorGuid), &EfiAcpiTableGuid);
        if (Match != FALSE) {
            return Table->VendorTable;
        }

        Match = EfiCoreCompareGuids(&(Table->VendorGuid), &EfiAcpiTable1Guid);
        if (Match != FALSE) {
            return Table->VendorTable;
        }
    }

    return NULL;
}

