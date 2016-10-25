/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rsdp.c

Abstract:

    This module contains support for finding the ACPI RSDP pointer on PC-AT
    compatible systems.

Author:

    Evan Green 29-Oct-2012

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/fw/smbios.h>
#include "firmware.h"
#include "bios.h"

//
// ---------------------------------------------------------------- Definitions
//

#define SMBIOS_SEARCH_START 0xF0000
#define SMBIOS_SEARCH_END 0x100000
#define SMBIOS_SEARCH_INCREMENT 0x10

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PVOID
FwpPcatSearchForRsdp (
    PVOID Address,
    ULONGLONG Length
    );

BOOL
FwpPcatChecksumTable (
    PVOID Address,
    ULONG Length
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

PVOID
FwPcatFindRsdp (
    VOID
    )

/*++

Routine Description:

    This routine attempts to find the ACPI RSDP table pointer on a PC-AT
    compatible system. It looks in the first 1k of the EBDA (Extended BIOS Data
    Area), as well as between the ranges 0xE0000 and 0xFFFFF. This routine
    must be run in physical mode.

Arguments:

    None.

Return Value:

    Returns a pointer to the RSDP table on success.

    NULL on failure.

--*/

{

    PUSHORT EbdaLocationPointer;
    PVOID EbdaPointer;
    PVOID RsdpPointer;

    //
    // Locate the EBDA, whose address is written into a specific offset.
    //

    EbdaLocationPointer = (PUSHORT)EBDA_POINTER_ADDRESS;
    EbdaPointer = (PVOID)(UINTN)*EbdaLocationPointer;

    //
    // Search the first 1k of the EBDA for the RSDP pointer.
    //

    RsdpPointer = FwpPcatSearchForRsdp(EbdaPointer, 1024);
    if (RsdpPointer != NULL) {
        return RsdpPointer;
    }

    //
    // Search the hardcoded range from 0xE0000 to 0xFFFFF.
    //

    RsdpPointer = FwpPcatSearchForRsdp(RSDP_SEARCH_ADDRESS, RSDP_SEARCH_LENGTH);
    if (RsdpPointer != NULL) {
        return RsdpPointer;
    }

    RsdpPointer = FwpPcatSearchForRsdp((PVOID)((UINTN)EbdaPointer << 4), 1024);
    if (RsdpPointer != NULL) {
        return RsdpPointer;
    }

    return NULL;
}

PVOID
FwPcatFindSmbiosTable (
    VOID
    )

/*++

Routine Description:

    This routine attempts to find the SMBIOS table entry point structure.

Arguments:

    None.

Return Value:

    Returns a pointer to the SMBIOS entry point structure on success.

    NULL on failure.

--*/

{

    PVOID Intermediate;
    UCHAR Length;
    BOOL Match;
    ULONG Offset;
    PSMBIOS_ENTRY_POINT Table;

    //
    // On PC/AT systems, the SMBIOS table entry point resides somewhere between
    // 0xF0000 and 0x100000, aligned to a 16 byte boundary.
    //

    Table = (PSMBIOS_ENTRY_POINT)SMBIOS_SEARCH_START;
    while (Table < (PSMBIOS_ENTRY_POINT)SMBIOS_SEARCH_END) {
        if (Table->AnchorString == SMBIOS_ANCHOR_STRING_VALUE) {
            Length = Table->EntryPointLength;

            //
            // Check the checksum.
            //

            if (FwpPcatChecksumTable(Table, Length) != FALSE) {

                //
                // Also verify and checksum the second part of the table.
                //

                Match = RtlCompareMemory(Table->IntermediateAnchor,
                                         SMBIOS_INTERMEDIATE_ANCHOR,
                                         SMBIOS_INTERMEDIATE_ANCHOR_SIZE);

                if (Match != FALSE) {
                    Offset = FIELD_OFFSET(SMBIOS_ENTRY_POINT,
                                          IntermediateAnchor);

                    Length = sizeof(SMBIOS_ENTRY_POINT) - Offset;
                    Intermediate = (((PVOID)Table) + Offset);

                    //
                    // If this also checksums, then the table really is here.
                    //

                    if (FwpPcatChecksumTable(Intermediate, Length) != FALSE) {
                        return Table;
                    }
                }
            }
        }

        //
        // Move up 16 bytes to the next candidate position.
        //

        Table = (PSMBIOS_ENTRY_POINT)(((PVOID)Table) + SMBIOS_SEARCH_INCREMENT);
    }

    return NULL;
}

//
// --------------------------------------------------------- Internal Functions
//

PVOID
FwpPcatSearchForRsdp (
    PVOID Address,
    ULONGLONG Length
    )

/*++

Routine Description:

    This routine searches the given range for the RSDP table.

Arguments:

    Address - Supplies the starting address to search for the RSDP. This
        address must be 16 byte aligned.

    Length - Supplies the number of bytes to search.

Return Value:

    Returns a pointer to the RSDP table on success.

    NULL on failure.

--*/

{

    PULONGLONG CurrentAddress;
    BOOL GoodChecksum;

    CurrentAddress = Address;
    while (Length >= sizeof(ULONGLONG)) {
        if (*CurrentAddress == RSDP_SIGNATURE) {
            GoodChecksum = FwpPcatChecksumTable(CurrentAddress,
                                                RSDP_CHECKSUM_SIZE);

            if (GoodChecksum != FALSE) {
                return CurrentAddress;
            }
        }

        //
        // Advance by 16 bytes.
        //

        CurrentAddress = (PULONGLONG)((PBYTE)CurrentAddress + 16);
    }

    return NULL;
}

BOOL
FwpPcatChecksumTable (
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

