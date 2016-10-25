/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    cfgtable.c

Abstract:

    This module implements the install configuration table UEFI service.

Author:

    Evan Green 25-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_CONFIGURATION_TABLE_EXPANSION_SIZE 0x10

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
// Store the size of the configuration table array allocation.
//

UINTN EfiSystemTableAllocationSize = 0;

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
EFI_STATUS
EfiCoreInstallConfigurationTable (
    EFI_GUID *Guid,
    VOID *Table
    )

/*++

Routine Description:

    This routine adds, updates, or removes a configuration table entry from the
    EFI System Table.

Arguments:

    Guid - Supplies a pointer to the GUID for the entry to add, update, or
        remove.

    Table - Supplies a pointer to the configuration table for the entry to add,
        update, or remove. This may be NULL.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if an attempt was made to delete a nonexistant entry.

    EFI_INVALID_PARAMETER if the GUID is NULL.

    EFI_OUT_OF_RESOURCES if an allocation failed.

--*/

{

    EFI_CONFIGURATION_TABLE *ConfigurationTable;
    UINTN Index;
    BOOLEAN Match;
    UINTN NewSize;
    UINTN RemainderSize;

    if (Guid == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    ConfigurationTable = EfiSystemTable->ConfigurationTable;

    //
    // Search all the tables for an entry that matches this one.
    //

    for (Index = 0; Index < EfiSystemTable->NumberOfTableEntries; Index += 1) {
        Match = EfiCoreCompareGuids(
                      Guid,
                      &(EfiSystemTable->ConfigurationTable[Index].VendorGuid));

        if (Match != FALSE) {
            break;
        }
    }

    //
    // If a match was found, then this is either a modify or a delete operation.
    //

    if (Index < EfiSystemTable->NumberOfTableEntries) {
        if (Table != NULL) {
            EfiSystemTable->ConfigurationTable[Index].VendorTable = Table;

            //
            // Signal a configuration table change.
            //

            EfipCoreNotifySignalList(Guid);
            return EFI_SUCCESS;
        }

        EfiSystemTable->NumberOfTableEntries -= 1;
        RemainderSize = (EfiSystemTable->NumberOfTableEntries - Index) *
                        sizeof(EFI_CONFIGURATION_TABLE);

        EfiCopyMem(&(ConfigurationTable[Index]),
                   &(EfiSystemTable->ConfigurationTable[Index + 1]),
                   RemainderSize);

    //
    // No matching GUID was found, this is an add operation.
    //

    } else {
        if (Table == NULL) {
            return EFI_NOT_FOUND;
        }

        Index = EfiSystemTable->NumberOfTableEntries;
        if ((Index * sizeof(EFI_CONFIGURATION_TABLE)) >=
            EfiSystemTableAllocationSize) {

            NewSize = EfiSystemTableAllocationSize +
                      (EFI_CONFIGURATION_TABLE_EXPANSION_SIZE *
                       sizeof(EFI_CONFIGURATION_TABLE));

            ConfigurationTable = EfiCoreAllocateRuntimePool(NewSize);
            if (ConfigurationTable == NULL) {
                return EFI_OUT_OF_RESOURCES;
            }

            if (EfiSystemTable->ConfigurationTable != NULL) {

                ASSERT(EfiSystemTableAllocationSize != 0);

                EfiCopyMem(ConfigurationTable,
                           EfiSystemTable->ConfigurationTable,
                           Index * sizeof(EFI_CONFIGURATION_TABLE));

                EfiFreePool(EfiSystemTable->ConfigurationTable);
            }

            EfiSystemTable->ConfigurationTable = ConfigurationTable;
            EfiSystemTableAllocationSize = NewSize;
        }

        //
        // Fill in the new entry.
        //

        EfiCopyMem(&(ConfigurationTable[Index].VendorGuid),
                   Guid,
                   sizeof(EFI_GUID));

        ConfigurationTable[Index].VendorTable = Table;
        EfiSystemTable->NumberOfTableEntries += 1;
    }

    EfiCoreCalculateTableCrc32(&(EfiSystemTable->Hdr));
    EfipCoreNotifySignalList(Guid);
    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

