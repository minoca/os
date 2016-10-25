/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    tables.c

Abstract:

    This module implements support for working with ACPI tables.

Author:

    Evan Green 4-Aug-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/fw/smbios.h>
#include <minoca/kernel/bootload.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
AcpipChecksumTable (
    PVOID Address,
    ULONG Length
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

PFIRMWARE_TABLE_DIRECTORY AcpiFirmwareTables = NULL;

//
// ------------------------------------------------------------------ Functions
//

VOID
AcpiInitializePreDebugger (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine pre-initializes ACPI to the extent that the debugger requires
    it. This routine is *undebuggable* as it is called before debug services
    are online.

Arguments:

    Parameters - Supplies the kernel parameter block coming from the loader.

Return Value:

    None.

--*/

{

    //
    // If parametes are supplied, initialize very basic support for accessing
    // firmware tables. MM is not available at this point, so the tables
    // returned should only be used temporarily.
    //

    if (Parameters != NULL) {
        if (AcpiFirmwareTables == NULL) {
            AcpiFirmwareTables = Parameters->FirmwareTables;
        }
    }

    return;
}

KSTATUS
AcpiInitialize (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine initializes ACPI.

Arguments:

    Parameters - Supplies the kernel parameter block coming from the loader.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    UINTN Length;
    PVOID NewTable;
    PSMBIOS_ENTRY_POINT SmbiosTable;
    KSTATUS Status;
    PDESCRIPTION_HEADER Table;
    PVOID *TableEntry;
    ULONG TableIndex;

    //
    // Make a non-paged pool copy of the firmware table directory, as this
    // is boot allocated memory and will disappear at some point.
    //

    AllocationSize = sizeof(FIRMWARE_TABLE_DIRECTORY) +
                  (Parameters->FirmwareTables->TableCount * sizeof(PVOID));

    AcpiFirmwareTables = MmAllocateNonPagedPool(AllocationSize,
                                                ACPI_ALLOCATION_TAG);

    if (AcpiFirmwareTables == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeEnd;
    }

    RtlCopyMemory(AcpiFirmwareTables,
                  Parameters->FirmwareTables,
                  AllocationSize);

    //
    // Copy the tables in preparation for destroying boot regions.
    //

    TableEntry = (PVOID *)(AcpiFirmwareTables + 1);
    for (TableIndex = 0;
         TableIndex < AcpiFirmwareTables->TableCount;
         TableIndex += 1) {

        Table = (PDESCRIPTION_HEADER)(TableEntry[TableIndex]);

        //
        // Skip the FACS table, as it contains the firwmare lock that
        // cannot be moved. It is assumed this table is allocated in
        // firmware permanent memory.
        //

        if (Table->Signature == FACS_SIGNATURE) {
            continue;
        }

        //
        // The SMBIOS table is jammed in this array as well, but doesn't
        // conform to the ACPI table header structure.
        //

        if (Table->Signature == SMBIOS_ANCHOR_STRING_VALUE) {
            SmbiosTable = (PSMBIOS_ENTRY_POINT)Table;
            Length = sizeof(SMBIOS_ENTRY_POINT) +
                     SmbiosTable->StructureTableLength;

        } else {
            Length = Table->Length;
        }

        NewTable = MmAllocateNonPagedPool(Length, ACPI_ALLOCATION_TAG);
        if (NewTable == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializeEnd;
        }

        RtlCopyMemory(NewTable, Table, Length);
        TableEntry[TableIndex] = NewTable;
    }

    Status = STATUS_SUCCESS;

InitializeEnd:
    return Status;
}

KERNEL_API
PVOID
AcpiFindTable (
    ULONG Signature,
    PVOID PreviousTable
    )

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

{

    PDESCRIPTION_HEADER Table;
    PVOID *TableEntry;
    LONG TableIndex;

    if (AcpiFirmwareTables == NULL) {
        return NULL;
    }

    //
    // Search the list of pointers, but do it backwards. This runs on the
    // assumption that if there are two tables in the firmware, the later one
    // is the better one. It also allows the test tables to override existing
    // firmware tables.
    //

    TableEntry = (PVOID *)(AcpiFirmwareTables + 1);
    for (TableIndex = AcpiFirmwareTables->TableCount - 1;
         TableIndex >= 0;
         TableIndex -= 1) {

        Table = (PDESCRIPTION_HEADER)(TableEntry[TableIndex]);

        //
        // If a previous table was supplied and was not yet found, don't really
        // look.
        //

        if (PreviousTable != NULL) {
            if (Table == PreviousTable) {
                PreviousTable = NULL;
            }

            continue;
        }

        if (Table->Signature == Signature) {

            //
            // The SMBIOS table doesn't conform to the ACPI table spec, so just
            // return it without computing an (incorrect) checksum.
            //

            if (Signature == SMBIOS_ANCHOR_STRING_VALUE) {
                return Table;
            }

            if (AcpipChecksumTable(Table, Table->Length) != FALSE) {
                return Table;
            }
        }
    }

    return NULL;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
AcpipChecksumTable (
    PVOID Address,
    ULONG Length
    )

/*++

Routine Description:

    This routine sums all of the bytes in a given table to determine if its
    checksum is correct. The checksum is set such that all the bytes in the
    table sum to a value of 0.

Arguments:

    Address - Supplies the address of the table to checksum.

    Length - Supplies the length of the table, in bytes.

Return Value:

    TRUE if all bytes in the table correctly sum to 0.

    FALSE if the bytes don't properly sum to 0.

--*/

{

    PBYTE CurrentByte;
    BYTE Sum;

    CurrentByte = Address;
    Sum = 0;
    while (Length != 0) {
        Sum += *CurrentByte;
        CurrentByte += 1;
        Length -= 1;
    }

    if (Sum == 0) {
        return TRUE;
    }

    return FALSE;
}

