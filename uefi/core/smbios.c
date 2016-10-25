/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    smbios.c

Abstract:

    This module implements support for adding SMBIOS tables to the firmware.

Author:

    Evan Green 7-May-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"
#include <minoca/fw/smbios.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
EfipSmbiosChecksumTable (
    VOID *Buffer,
    UINTN Size,
    UINTN ChecksumOffset
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the SMBIOS entry point structure.
//

VOID *EfiSmbiosEntryPoint;
UINTN EfiSmbiosAllocationSize;
UINTN EfiSmbiosPageCount;

EFI_GUID EfiSmbiosTableGuid = EFI_SMBIOS_TABLE_GUID;

SMBIOS_ENTRY_POINT EfiSmbiosEntryPointTemplate = {
    SMBIOS_ANCHOR_STRING_VALUE,
    0,
    0x1F,
    2,
    8,
    0,
    0,
    {0},
    {'_', 'D', 'M', 'I', '_'},
    0,
    0,
    0,
    0,
    0x28
};

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
EFI_STATUS
EfiSmbiosDriverEntry (
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable
    )

/*++

Routine Description:

    This routine is the entry point into the SMBIOS driver.

Arguments:

    ImageHandle - Supplies the driver image handle.

    SystemTable - Supplies a pointer to the EFI system table.

Return Value:

    EFI status code.

--*/

{

    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfiSmbiosAddStructure (
    VOID *Table,
    ...
    )

/*++

Routine Description:

    This routine adds an SMBIOS structure to the SMBIOS table.

Arguments:

    Table - Supplies a pointer to the table to add. A copy of this data will be
        made. The length of the table must be correctly filled in.

    ... - Supplies an array of pointers to strings to add to the end of the
        table. This list must be terminated with a NULL.

Return Value:

    EFI_SUCCESS on success.

    EFI_INSUFFICIENT_RESOURCES if a memory allocation failed.

--*/

{

    EFI_PHYSICAL_ADDRESS Allocation;
    UINTN AllocationSize;
    CHAR8 *Argument;
    VA_LIST ArgumentList;
    UINTN ChecksumOffset;
    CHAR8 *CurrentPointer;
    PSMBIOS_ENTRY_POINT EntryPoint;
    UINTN Offset;
    UINTN PageCount;
    EFI_STATUS Status;
    UINTN StringLength;
    UINTN StringsLength;
    UINTN StructureSize;
    PSMBIOS_HEADER TableHeader;

    //
    // Loop through the arguments once to count the string lengths.
    //

    StringsLength = 1;
    VA_START(ArgumentList, Table);
    Argument = VA_ARG(ArgumentList, CHAR8 *);
    while (Argument != NULL) {
        StringsLength += RtlStringLength(Argument) + 1;
        Argument = VA_ARG(ArgumentList, CHAR8 *);
    }

    VA_END(ArgumentList);
    if (StringsLength < 2) {
        StringsLength = 2;
    }

    //
    // Compute the total length of the new table.
    //

    TableHeader = Table;
    StructureSize = TableHeader->Length + StringsLength;
    AllocationSize = EfiSmbiosAllocationSize + StructureSize;
    if (EfiSmbiosAllocationSize == 0) {
        AllocationSize += sizeof(SMBIOS_ENTRY_POINT);
    }

    PageCount = EFI_SIZE_TO_PAGES(AllocationSize);

    //
    // Allocate more pages if needed.
    //

    if (PageCount > EfiSmbiosPageCount) {
        Status = EfiAllocatePages(AllocateAnyPages,
                                  EfiACPIReclaimMemory,
                                  PageCount,
                                  &Allocation);

        if (EFI_ERROR(Status)) {
            return Status;
        }

        //
        // If this is the first table, copy the template over.
        //

        if (EfiSmbiosAllocationSize == 0) {
            EfiCopyMem((VOID *)(UINTN)Allocation,
                       &EfiSmbiosEntryPointTemplate,
                       sizeof(SMBIOS_ENTRY_POINT));

            EfipSmbiosChecksumTable(
                                  (VOID *)(UINTN)Allocation,
                                  EfiSmbiosEntryPointTemplate.EntryPointLength,
                                  OFFSET_OF(SMBIOS_ENTRY_POINT, Checksum));

            EfiSmbiosAllocationSize = sizeof(SMBIOS_ENTRY_POINT);

        //
        // Copy the existing tables, and free the old memory.
        //

        } else {
            EfiCopyMem((VOID *)(UINTN)Allocation,
                       EfiSmbiosEntryPoint,
                       EfiSmbiosAllocationSize);

            EfiFreePages((EFI_PHYSICAL_ADDRESS)(UINTN)EfiSmbiosEntryPoint,
                         EfiSmbiosPageCount);
        }

        EfiSmbiosEntryPoint = (VOID *)(UINTN)Allocation;
        EfiSmbiosPageCount = PageCount;

        //
        // Install the new table.
        //

        Status = EfiInstallConfigurationTable(&EfiSmbiosTableGuid,
                                              EfiSmbiosEntryPoint);

        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    //
    // Copy the new structure onto the end.
    //

    RtlCopyMemory(EfiSmbiosEntryPoint + EfiSmbiosAllocationSize,
                  TableHeader,
                  TableHeader->Length);

    StringsLength = 0;
    CurrentPointer = EfiSmbiosEntryPoint +
                     EfiSmbiosAllocationSize +
                     TableHeader->Length;

    VA_START(ArgumentList, Table);
    Argument = VA_ARG(ArgumentList, CHAR8 *);
    while (Argument != NULL) {
        StringLength = RtlStringLength(Argument) + 1;
        EfiCopyMem(CurrentPointer, Argument, StringLength);
        StringsLength += StringLength;
        CurrentPointer += StringLength;
        Argument = VA_ARG(ArgumentList, CHAR8 *);
    }

    VA_END(ArgumentList);
    *CurrentPointer = '\0';
    CurrentPointer += 1;
    StringsLength += 1;
    if (StringsLength < 2) {
        *CurrentPointer = '\0';
        StringsLength += 1;
    }

    //
    // Update the header.
    //

    EfiSmbiosAllocationSize += StructureSize;
    EntryPoint = EfiSmbiosEntryPoint;
    EntryPoint->NumberOfStructures += 1;
    if (EntryPoint->MaxStructureSize < StructureSize) {
        EntryPoint->MaxStructureSize = StructureSize;
    }

    EntryPoint->StructureTableLength = EfiSmbiosAllocationSize -
                                       sizeof(SMBIOS_ENTRY_POINT);

    EntryPoint->StructureTableAddress =
                                   (UINT32)(UINTN)(EfiSmbiosEntryPoint +
                                                   sizeof(SMBIOS_ENTRY_POINT));

    Offset = OFFSET_OF(SMBIOS_ENTRY_POINT, IntermediateAnchor);
    ChecksumOffset = OFFSET_OF(SMBIOS_ENTRY_POINT, IntermediateChecksum) -
                     Offset;

    EfipSmbiosChecksumTable(EfiSmbiosEntryPoint + Offset,
                            sizeof(SMBIOS_ENTRY_POINT) - Offset,
                            ChecksumOffset);

    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
EfipSmbiosChecksumTable (
    VOID *Buffer,
    UINTN Size,
    UINTN ChecksumOffset
    )

/*++

Routine Description:

    This routine checksums the SMBIOS entry point.

Arguments:

    Buffer - Supplies a pointer to the table to checksum.

    Size - Supplies the size of the table in bytes.

    ChecksumOffset - Supplies the offset of the 8 bit checksum field.

Return Value:

    None.

--*/

{

    UINT8 *Pointer;
    UINT8 Sum;

    Sum = 0;
    Pointer = Buffer;
    Pointer[ChecksumOffset] = 0;
    while (Size != 0) {
        Sum = (UINT8)(Sum + *Pointer);
        Pointer += 1;
        Size -= 1;
    }

    Pointer = Buffer;
    Pointer[ChecksumOffset] = (UINT8)(0xFF - Sum + 1);
    return;
}

