/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    acpi.c

Abstract:

    This module implements support for installing ACPI tables into the EFI
    system table.

Author:

    Evan Green 25-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"
#include "efiimg.h"
#include "fwvol.h"
#include "fv2.h"
#include <minoca/fw/acpitabs.h>
#include <minoca/uefi/guid/acpi.h>

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_ACPI_TABLE_ENTRY_MAGIC 0x62544145 // 'bTAE'

#define EFI_ACPI_TABLE_EXPANSION_COUNT 0x10

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _EFI_ACPI_COMMON_HEADER {
  UINT32 Signature;
  UINT32 Length;
} EFI_ACPI_COMMON_HEADER, *PEFI_ACPI_COMMON_HEADER;

typedef struct _EFI_ACPI_TABLE_ENTRY {
    UINT32 Magic;
    LIST_ENTRY ListEntry;
    EFI_ACPI_COMMON_HEADER *Table;
    EFI_PHYSICAL_ADDRESS PageAddress;
    UINTN NumberOfPages;
    UINTN Handle;
} EFI_ACPI_TABLE_ENTRY, *PEFI_ACPI_TABLE_ENTRY;

typedef struct _EFI_ACPI_CONTEXT {
    LIST_ENTRY TableList;
    UINTN CurrentHandle;
    UINTN TableCount;
    UINTN TableCapacity;
    PFADT Fadt;
    PFACS Facs;
    PRSDP Rsdp;
    PRSDT Rsdt;
    PXSDT Xsdt;
    PDESCRIPTION_HEADER Dsdt;
} EFI_ACPI_CONTEXT, *PEFI_ACPI_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

EFI_STATUS
EfipLocateFirmwareVolumeWithAcpiTables (
    EFI_FIRMWARE_VOLUME2_PROTOCOL **Instance
    );

EFI_STATUS
EfipAcpiInitializeTables (
    VOID
    );

EFI_STATUS
EfipSetAcpiTable (
    VOID *Table,
    BOOLEAN Checksum,
    UINTN *Handle
    );

EFI_STATUS
EfipAddAcpiTableToList (
    VOID *Table,
    BOOLEAN Checksum,
    UINTN *Handle
    );

EFI_STATUS
EfipRemoveAcpiTableFromList (
    UINTN Handle
    );

VOID
EfipAcpiDeleteTable (
    PEFI_ACPI_TABLE_ENTRY Table
    );

EFI_STATUS
EfipAcpiRemoveTableFromRsdt (
    PEFI_ACPI_TABLE_ENTRY Table,
    UINTN *TableCount,
    PDESCRIPTION_HEADER Rsdt,
    PDESCRIPTION_HEADER Xsdt
    );

EFI_STATUS
EfipAcpiFindTableByHandle (
    UINTN Handle,
    PLIST_ENTRY ListHead,
    PEFI_ACPI_TABLE_ENTRY *FoundEntry
    );

EFI_STATUS
EfipReallocateAcpiTableBuffer (
    VOID
    );

EFI_STATUS
EfipAcpiPublishTables (
    VOID
    );

VOID
EfipAcpiChecksumCommonTables (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

EFI_GUID EfiAcpiTable1Guid = EFI_ACPI_10_TABLE_GUID;
EFI_GUID EfiAcpiTableGuid = EFI_ACPI_20_TABLE_GUID;
EFI_GUID EfiAcpiTableStorageFileGuid = EFI_ACPI_TABLE_STORAGE_FILE_GUID;

//
// Define the master EFI ACPI context.
//

EFI_ACPI_CONTEXT EfiAcpiContext;

//
// Define default values to stick in the table header. These can be overridden
// by the platform. Note that once the FADT is installed, the values from that
// header will overwrite values installed here.
//

CHAR8 *EfiAcpiDefaultOemId = "Minoca";
UINT64 EfiAcpiDefaultOemTableId;
UINT32 EfiAcpiDefaultOemRevision;
UINT32 EfiAcpiDefaultCreatorId;
UINT32 EfiAcpiDefaultCreatorRevision;

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
EFI_STATUS
EfiAcpiDriverEntry (
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable
    )

/*++

Routine Description:

    This routine is the entry point into the ACPI driver.

Arguments:

    ImageHandle - Supplies the driver image handle.

    SystemTable - Supplies a pointer to the EFI system table.

Return Value:

    EFI status code.

--*/

{

    EFI_ACPI_COMMON_HEADER *CurrentTable;
    EFI_FIRMWARE_VOLUME2_PROTOCOL *FirmwareVolume;
    UINT32 FirmwareVolumeStatus;
    INTN Instance;
    UINTN Size;
    EFI_STATUS Status;
    UINTN TableHandle;
    UINTN TableSize;

    CurrentTable = NULL;
    Instance = 0;
    TableHandle = 0;
    Status = EfipAcpiInitializeTables();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfipLocateFirmwareVolumeWithAcpiTables(&FirmwareVolume);
    if (EFI_ERROR(Status)) {
        return EFI_SUCCESS;
    }

    while (Status == EFI_SUCCESS) {
        Status = FirmwareVolume->ReadSection(FirmwareVolume,
                                             &EfiAcpiTableStorageFileGuid,
                                             EFI_SECTION_RAW,
                                             Instance,
                                             (VOID **)&CurrentTable,
                                             &Size,
                                             &FirmwareVolumeStatus);

        if (!EFI_ERROR(Status)) {
            TableHandle = 0;
            TableSize = ((EFI_ACPI_COMMON_HEADER *)CurrentTable)->Length;

            ASSERT(Size >= TableSize);

            EfiAcpiChecksumTable(CurrentTable,
                                 TableSize,
                                 OFFSET_OF(DESCRIPTION_HEADER, Checksum));

            Status = EfiAcpiInstallTable(CurrentTable, TableSize, &TableHandle);
            EfiFreePool(CurrentTable);
            if (EFI_ERROR(Status)) {
                return EFI_ABORTED;
            }

            Instance += 1;
            CurrentTable = NULL;
        }
    }

    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfiAcpiInstallTable (
    VOID *AcpiTableBuffer,
    UINTN AcpiTableBufferSize,
    UINTN *TableKey
    )

/*++

Routine Description:

    This routine installs an ACPI table into the RSDT/XSDT.

Arguments:

    AcpiTableBuffer - Supplies a pointer to the buffer containing the ACPI
        table to insert.

    AcpiTableBufferSize - Supplies the size in bytes of the ACPI table buffer.

    TableKey - Supplies a pointer where a key will be returned that refers
        to the table.

Return Value:

    EFI status code.

--*/

{

    EFI_STATUS Status;
    VOID *TableCopy;

    ASSERT(EfiAcpiContext.TableList.Next != NULL);

    if ((AcpiTableBuffer == NULL) || (TableKey == NULL) ||
        (((EFI_ACPI_COMMON_HEADER *)AcpiTableBuffer)->Length !=
         AcpiTableBufferSize)) {

        return EFI_INVALID_PARAMETER;
    }

    Status = EfiAllocatePool(EfiBootServicesData,
                             AcpiTableBufferSize,
                             (VOID **)&TableCopy);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfiCopyMem(TableCopy, AcpiTableBuffer, AcpiTableBufferSize);
    *TableKey = 0;
    Status = EfipSetAcpiTable(TableCopy, TRUE, TableKey);
    if (!EFI_ERROR(Status)) {
        Status = EfipAcpiPublishTables();
    }

    EfiFreePool(TableCopy);
    return Status;
}

EFIAPI
EFI_STATUS
EfiAcpiUninstallTable (
    UINTN TableKey
    )

/*++

Routine Description:

    This routine uninstalls a previously install ACPI table.

Arguments:

    TableKey - Supplies the key returned when the table was installed.

Return Value:

    EFI status code.

--*/

{

    EFI_STATUS Status;

    ASSERT(EfiAcpiContext.TableList.Next != NULL);

    Status = EfipSetAcpiTable(NULL, FALSE, &TableKey);
    if (!EFI_ERROR(Status)) {
        Status = EfipAcpiPublishTables();
    }

    if (EFI_ERROR(Status)) {
        return EFI_NOT_FOUND;
    }

    return EFI_SUCCESS;
}

EFIAPI
VOID
EfiAcpiChecksumTable (
    VOID *Buffer,
    UINTN Size,
    UINTN ChecksumOffset
    )

/*++

Routine Description:

    This routine checksums an ACPI table.

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

//
// --------------------------------------------------------- Internal Functions
//

EFI_STATUS
EfipLocateFirmwareVolumeWithAcpiTables (
    EFI_FIRMWARE_VOLUME2_PROTOCOL **Instance
    )

/*++

Routine Description:

    This routine returns the first instance of the firmware volume protocol
    that contains an ACPI table storage file.

Arguments:

    Instance - Supplies a pointer where a pointer to the instance will be
        returned on success.

Return Value:

    EFI status code.

--*/

{

    EFI_FV_FILE_ATTRIBUTES Attributes;
    EFI_FV_FILETYPE FileType;
    UINT32 FirmwareVolumeStatus;
    EFI_HANDLE *HandleBuffer;
    UINTN HandleCount;
    UINTN Index;
    UINTN Size;
    EFI_STATUS Status;
    EFI_FIRMWARE_VOLUME2_PROTOCOL *Volume;

    FirmwareVolumeStatus = 0;

    //
    // Get all the firmware volume handles.
    //

    Status = EfiLocateHandleBuffer(ByProtocol,
                                   &EfiFirmwareVolume2ProtocolGuid,
                                   NULL,
                                   &HandleCount,
                                   &HandleBuffer);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Loop through all the firmware volume handles looking for an ACPI
    // table storage file.
    //

    for (Index = 0; Index < HandleCount; Index += 1) {
        Status = EfiHandleProtocol(HandleBuffer[Index],
                                   &EfiFirmwareVolume2ProtocolGuid,
                                   (VOID **)&Volume);

        ASSERT(!EFI_ERROR(Status));

        //
        // See if it has an ACPI storage file.
        //

        Status = Volume->ReadFile(Volume,
                                  &EfiAcpiTableStorageFileGuid,
                                  NULL,
                                  &Size,
                                  &FileType,
                                  &Attributes,
                                  &FirmwareVolumeStatus);

        if (Status == EFI_SUCCESS) {
            *Instance = Volume;
            break;
        }
    }

    EfiFreePool(HandleBuffer);
    return Status;
}

EFI_STATUS
EfipAcpiInitializeTables (
    VOID
    )

/*++

Routine Description:

    This routine creates the initial RSDP and XSDT tables.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    PEFI_ACPI_CONTEXT Context;
    UINT64 CurrentData;
    EFI_PHYSICAL_ADDRESS PageAddress;
    UINT8 *Pointer;
    UINTN RsdpTableSize;
    EFI_STATUS Status;
    UINTN TotalSize;

    Context = &EfiAcpiContext;
    INITIALIZE_LIST_HEAD(&(Context->TableList));
    Context->CurrentHandle = 1;
    RsdpTableSize = sizeof(RSDP);

    //
    // Allocate space for the initial RSDP below 4GB for 32-bit OSes.
    //

    PageAddress = 0xFFFFFFFF;
    Status = EfiAllocatePages(AllocateMaxAddress,
                              EfiACPIReclaimMemory,
                              EFI_SIZE_TO_PAGES(RsdpTableSize),
                              &PageAddress);

    if (EFI_ERROR(Status)) {
        return EFI_OUT_OF_RESOURCES;
    }

    Pointer = (UINT8 *)(UINTN)PageAddress;
    EfiSetMem(Pointer, RsdpTableSize, 0);
    Context->Rsdp = (PRSDP)Pointer;

    //
    // Allocate space for the RSDT and XSDT below 4GB, again for historical
    // reasons.
    //

    Context->TableCapacity = EFI_ACPI_TABLE_EXPANSION_COUNT;
    TotalSize = sizeof(DESCRIPTION_HEADER) +
                (Context->TableCapacity * sizeof(UINT32)) +
                sizeof(DESCRIPTION_HEADER) +
                (Context->TableCapacity * sizeof(UINT64));

    PageAddress = 0xFFFFFFFF;
    Status = EfiAllocatePages(AllocateMaxAddress,
                              EfiACPIReclaimMemory,
                              EFI_SIZE_TO_PAGES(TotalSize),
                              &PageAddress);

    if (EFI_ERROR(Status)) {
        EfiFreePages((EFI_PHYSICAL_ADDRESS)(UINTN)(Context->Rsdp),
                     EFI_SIZE_TO_PAGES(RsdpTableSize));

        return EFI_OUT_OF_RESOURCES;
    }

    Pointer = (UINT8 *)(UINTN)PageAddress;
    EfiSetMem(Pointer, TotalSize, 0);
    Context->Rsdt = (PRSDT)Pointer;
    Pointer += sizeof(DESCRIPTION_HEADER) +
               (Context->TableCapacity * sizeof(UINT32));

    Context->Xsdt = (PXSDT)Pointer;

    //
    // Initialize the RSDP.
    //

    CurrentData = RSDP_SIGNATURE;
    EfiCopyMem(&(Context->Rsdp->Signature), &CurrentData, sizeof(UINT64));
    EfiCopyMem(&(Context->Rsdp->OemId),
               EfiAcpiDefaultOemId,
               sizeof(Context->Rsdp->OemId));

    Context->Rsdp->Revision = ACPI_20_RSDP_REVISION;
    Context->Rsdp->RsdtAddress = (UINT32)(UINTN)(Context->Rsdt);
    Context->Rsdp->Length = sizeof(RSDP);
    CurrentData = (UINT64)(UINTN)(Context->Xsdt);
    EfiCopyMem(&(Context->Rsdp->XsdtAddress), &CurrentData, sizeof(UINT64));
    EfiSetMem(Context->Rsdp->Reserved, sizeof(Context->Rsdp->Reserved), 0);

    //
    // Initialize the RSDT. Reserve the first entry for the FADT.
    //

    Context->TableCount = 1;
    Context->Rsdt->Header.Length = sizeof(DESCRIPTION_HEADER) + sizeof(UINT32);
    Context->Rsdt->Header.Signature = RSDT_SIGNATURE;
    Context->Rsdt->Header.Revision = ACPI_30_RSDT_REVISION;
    EfiCopyMem(&(Context->Rsdt->Header.OemId),
               EfiAcpiDefaultOemId,
               sizeof(Context->Rsdt->Header.OemId));

    CurrentData = EfiAcpiDefaultOemTableId;
    EfiCopyMem(&(Context->Rsdt->Header.OemTableId),
               &CurrentData,
               sizeof(UINT64));

    Context->Rsdt->Header.OemRevision = EfiAcpiDefaultOemRevision;
    Context->Rsdt->Header.CreatorId = EfiAcpiDefaultCreatorId;
    Context->Rsdt->Header.CreatorRevision = EfiAcpiDefaultCreatorRevision;

    //
    // Initialize the XSDT, again reserving the first entry for the FADT.
    //

    Context->TableCount = 1;
    Context->Rsdt->Header.Length = sizeof(DESCRIPTION_HEADER) + sizeof(UINT64);
    Context->Rsdt->Header.Signature = RSDT_SIGNATURE;
    Context->Rsdt->Header.Revision = ACPI_30_XSDT_REVISION;
    EfiCopyMem(&(Context->Rsdt->Header.OemId),
               EfiAcpiDefaultOemId,
               sizeof(Context->Rsdt->Header.OemId));

    CurrentData = EfiAcpiDefaultOemTableId;
    EfiCopyMem(&(Context->Rsdt->Header.OemTableId),
               &CurrentData,
               sizeof(UINT64));

    Context->Rsdt->Header.OemRevision = EfiAcpiDefaultOemRevision;
    Context->Rsdt->Header.CreatorId = EfiAcpiDefaultCreatorId;
    Context->Rsdt->Header.CreatorRevision = EfiAcpiDefaultCreatorRevision;
    EfipAcpiChecksumCommonTables();
    return EFI_SUCCESS;
}

EFI_STATUS
EfipSetAcpiTable (
    VOID *Table,
    BOOLEAN Checksum,
    UINTN *Handle
    )

/*++

Routine Description:

    This routine adds, removes, or updates ACPI tables. If the address is
    not NULL and the handle is NULL, the table is added. If both the address
    and the handle is not NULL, the table is updated. If the address is NULL
    and the handle is not, the table is deleted.

Arguments:

    Table - Supplies an optional pointer to the table to intall. If not
        supplied, the table is deleted.

    Checksum - Supplies a boolean indicating if the checksum should be
        recomputed.

    Handle - Supplies a pointer to the handle of the table.

Return Value:

    EFI status code.

--*/

{

    UINTN SavedHandle;
    EFI_STATUS Status;

    ASSERT(Handle != NULL);

    //
    // If there is no handle, add the table.
    //

    if (*Handle == 0) {
        if (Table == NULL) {
            return EFI_INVALID_PARAMETER;
        }

        Status = EfipAddAcpiTableToList(Table, Checksum, Handle);

    //
    // There is a handle, update or delete the table.
    //

    } else {
        if (Table != NULL) {
            Status = EfipRemoveAcpiTableFromList(*Handle);
            if (EFI_ERROR(Status)) {
                return EFI_ABORTED;
            }

            //
            // Set the handle to replace the table at the same handle.
            //

            SavedHandle = EfiAcpiContext.CurrentHandle;
            EfiAcpiContext.CurrentHandle = *Handle;
            Status = EfipAddAcpiTableToList(Table, Checksum, Handle);
            EfiAcpiContext.CurrentHandle = SavedHandle;

        } else {
            Status = EfipRemoveAcpiTableFromList(*Handle);
        }
    }

    if (EFI_ERROR(Status)) {
        return EFI_ABORTED;
    }

    return EFI_SUCCESS;
}

EFI_STATUS
EfipAddAcpiTableToList (
    VOID *Table,
    BOOLEAN Checksum,
    UINTN *Handle
    )

/*++

Routine Description:

    This routine adds an ACPI table to the table list. It detects the FACS,
    allocates the correct type of memory, and properly aligns the table.

Arguments:

    Table - Supplies a pointer to the table to intall.

    Checksum - Supplies a boolean indicating if the checksum should be
        recomputed.

    Handle - Supplies a pointer to the handle of the table.

Return Value:

    EFI status code.

--*/

{

    BOOLEAN AddToRsdt;
    UINT64 Buffer64;
    UINT32 *RsdtEntry;
    EFI_STATUS Status;
    PEFI_ACPI_TABLE_ENTRY TableEntry;
    UINT32 TableSignature;
    UINT32 TableSize;
    UINT32 *XsdtEntry;

    ASSERT((Table != NULL) && (Handle != NULL));

    AddToRsdt = TRUE;
    TableEntry = EfiCoreAllocateBootPool(sizeof(EFI_ACPI_TABLE_ENTRY));
    if (TableEntry == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    TableEntry->Magic = EFI_ACPI_TABLE_ENTRY_MAGIC;
    TableSignature = ((EFI_ACPI_COMMON_HEADER *)Table)->Signature;
    TableSize = ((EFI_ACPI_COMMON_HEADER *)Table)->Length;

    //
    // Allocate a buffer for the table. All tables are allocated in the lower
    // 32 bits for backwards compatibility with 32-bit OSes.
    //

    TableEntry->PageAddress = 0xFFFFFFFF;
    TableEntry->NumberOfPages = EFI_SIZE_TO_PAGES(TableSize);

    //
    // The FACS is aligned on a 64-byte boundary and must be ACPI NVS memory.
    //

    if (TableSignature == FACS_SIGNATURE) {

        ASSERT((EFI_PAGE_SIZE % 64) == 0);

        Status = EfiAllocatePages(AllocateMaxAddress,
                                  EfiACPIMemoryNVS,
                                  TableEntry->NumberOfPages,
                                  &(TableEntry->PageAddress));

    //
    // Everything else is just ACPI reclaim memory.
    //

    } else {
        Status = EfiAllocatePages(AllocateMaxAddress,
                                  EfiACPIReclaimMemory,
                                  TableEntry->NumberOfPages,
                                  &(TableEntry->PageAddress));
    }

    if (EFI_ERROR(Status)) {
        EfiFreePool(TableEntry);
        return EFI_OUT_OF_RESOURCES;
    }

    TableEntry->Table = (VOID *)(UINTN)(TableEntry->PageAddress);
    EfiCopyMem(TableEntry->Table, Table, TableSize);
    TableEntry->Handle = EfiAcpiContext.CurrentHandle;
    EfiAcpiContext.CurrentHandle += 1;
    *Handle = TableEntry->Handle;

    //
    // Update some pointers depending on the table signature.
    //

    switch (TableSignature) {
    case FADT_SIGNATURE:
        AddToRsdt = FALSE;
        if (EfiAcpiContext.Fadt != NULL) {
            EfiFreePages(TableEntry->PageAddress, TableEntry->NumberOfPages);
            EfiFreePool(TableEntry);
            return EFI_ABORTED;
        }

        EfiAcpiContext.Fadt = (PFADT)(TableEntry->Table);
        if (EfiAcpiContext.Facs <= (PFACS)0xFFFFFFFF) {
            EfiAcpiContext.Fadt->FirmwareControlAddress =
                                          (UINT32)(UINTN)(EfiAcpiContext.Facs);

        } else {
            Buffer64 = (UINTN)(EfiAcpiContext.Facs);
            EfiCopyMem(&(EfiAcpiContext.Fadt->XFirmwareControl),
                       &Buffer64,
                       sizeof(UINT64));
        }

        EfiAcpiContext.Fadt->DsdtAddress = (UINT32)(UINTN)(EfiAcpiContext.Dsdt);
        Buffer64 = (UINTN)(EfiAcpiContext.Dsdt);
        EfiCopyMem(&(EfiAcpiContext.Fadt->XDsdt), &Buffer64, sizeof(UINT64));

        //
        // Copy the RSDP information to match the FADT OEM information.
        //

        ASSERT(EfiAcpiContext.Rsdp != NULL);

        EfiCopyMem(&(EfiAcpiContext.Rsdp->OemId),
                   &(EfiAcpiContext.Fadt->Header.OemId),
                   sizeof(EfiAcpiContext.Fadt->Header.OemId));

        //
        // Copy the RSDT OEM information to match the FADT.
        //

        ASSERT(EfiAcpiContext.Rsdt != NULL);

        EfiCopyMem(&(EfiAcpiContext.Rsdt->Header.OemId),
                   &(EfiAcpiContext.Fadt->Header.OemId),
                   sizeof(EfiAcpiContext.Fadt->Header.OemId));

        EfiCopyMem(&(EfiAcpiContext.Rsdt->Header.OemTableId),
                   &(EfiAcpiContext.Fadt->Header.OemTableId),
                   sizeof(EfiAcpiContext.Fadt->Header.OemTableId));

        EfiAcpiContext.Rsdt->Header.OemRevision =
                                       EfiAcpiContext.Fadt->Header.OemRevision;

        //
        // Copy over the XSDT OEM information to match the FADT as well.
        //

        ASSERT(EfiAcpiContext.Xsdt != NULL);

        EfiCopyMem(&(EfiAcpiContext.Xsdt->Header.OemId),
                   &(EfiAcpiContext.Fadt->Header.OemId),
                   sizeof(EfiAcpiContext.Fadt->Header.OemId));

        EfiCopyMem(&(EfiAcpiContext.Xsdt->Header.OemTableId),
                   &(EfiAcpiContext.Fadt->Header.OemTableId),
                   sizeof(EfiAcpiContext.Fadt->Header.OemTableId));

        EfiAcpiContext.Xsdt->Header.OemRevision =
                                       EfiAcpiContext.Fadt->Header.OemRevision;

        if (Checksum != FALSE) {
            EfiAcpiChecksumTable(TableEntry->Table,
                                 TableEntry->Table->Length,
                                 OFFSET_OF(DESCRIPTION_HEADER, Checksum));
        }

        break;

    case FACS_SIGNATURE:
        if (EfiAcpiContext.Facs != NULL) {
            EfiFreePages(TableEntry->PageAddress, TableEntry->NumberOfPages);
            EfiFreePool(TableEntry);
            return EFI_ABORTED;
        }

        //
        // The FACS is referenced by the FADT and is not part of the RSDT.
        //

        AddToRsdt = FALSE;
        EfiAcpiContext.Facs = (PFACS)(TableEntry->Table);
        if (EfiAcpiContext.Fadt != NULL) {
            if (EfiAcpiContext.Facs <= (PFACS)0xFFFFFFFF) {
                EfiAcpiContext.Fadt->FirmwareControlAddress =
                                            (UINT32)(UINTN)(TableEntry->Table);

            } else {
                Buffer64 = (UINTN)(TableEntry->Table);
                EfiCopyMem(&(EfiAcpiContext.Fadt->XFirmwareControl),
                           &Buffer64,
                           sizeof(UINT64));
            }

            //
            // Checksum the FADT.
            //

            EfiAcpiChecksumTable(EfiAcpiContext.Fadt,
                                 EfiAcpiContext.Fadt->Header.Length,
                                 OFFSET_OF(DESCRIPTION_HEADER, Checksum));
        }

        break;

    case DSDT_SIGNATURE:
        if (EfiAcpiContext.Dsdt != NULL) {
            EfiFreePages(TableEntry->PageAddress, TableEntry->NumberOfPages);
            EfiFreePool(TableEntry);
            return EFI_ABORTED;
        }

        //
        // The DSDT is referenced by the FADT and is not part of the RSDT.
        //

        AddToRsdt = FALSE;
        EfiAcpiContext.Dsdt = (PDESCRIPTION_HEADER)TableEntry->Table;
        if (EfiAcpiContext.Fadt != NULL) {
            EfiAcpiContext.Fadt->DsdtAddress =
                                          (UINT32)(UINTN)(EfiAcpiContext.Dsdt);

            Buffer64 = (UINT64)(UINTN)(EfiAcpiContext.Dsdt);
            EfiCopyMem(&(EfiAcpiContext.Fadt->XDsdt),
                       &Buffer64,
                       sizeof(UINT64));

            //
            // Checksum the FADT.
            //

            EfiAcpiChecksumTable(EfiAcpiContext.Fadt,
                                 EfiAcpiContext.Fadt->Header.Length,
                                 OFFSET_OF(DESCRIPTION_HEADER, Checksum));
        }

        break;

    //
    // The average joe table.
    //

    default:
        if (Checksum != FALSE) {
            EfiAcpiChecksumTable(TableEntry->Table,
                                 TableEntry->Table->Length,
                                 OFFSET_OF(DESCRIPTION_HEADER, Checksum));
        }

        break;
    }

    //
    // Add the table to the global list.
    //

    INSERT_BEFORE(&(TableEntry->ListEntry), &(EfiAcpiContext.TableList));

    //
    // Add this to the RSDT/XSDT.
    //

    if (AddToRsdt != FALSE) {
        if (EfiAcpiContext.TableCount >= EfiAcpiContext.TableCapacity) {
            Status = EfipReallocateAcpiTableBuffer();
            if (EFI_ERROR(Status)) {

                ASSERT(FALSE);

                return EFI_OUT_OF_RESOURCES;
            }
        }

        RsdtEntry = (UINT32 *)((UINT8 *)(EfiAcpiContext.Rsdt) +
                               sizeof(DESCRIPTION_HEADER) +
                               (EfiAcpiContext.TableCount *
                                sizeof(UINT32)));

        XsdtEntry = (VOID *)((UINT8 *)(EfiAcpiContext.Xsdt) +
                             sizeof(DESCRIPTION_HEADER) +
                             EfiAcpiContext.TableCount *
                             sizeof(UINT64));

        *RsdtEntry = (UINT32)(UINTN)(TableEntry->Table);
        EfiAcpiContext.Rsdt->Header.Length += sizeof(UINT32);
        Buffer64 = (UINTN)(TableEntry->Table);
        EfiCopyMem(XsdtEntry, &Buffer64, sizeof(UINT64));
        EfiAcpiContext.Xsdt->Header.Length += sizeof(UINT64);
        EfiAcpiContext.TableCount += 1;
    }

    EfipAcpiChecksumCommonTables();
    return EFI_SUCCESS;
}

EFI_STATUS
EfipRemoveAcpiTableFromList (
    UINTN Handle
    )

/*++

Routine Description:

    This routine removes the table with the given handle.

Arguments:

    Handle - Supplies the handle of the table to remove.

Return Value:

    EFI Status code.

--*/

{

    EFI_STATUS Status;
    PEFI_ACPI_TABLE_ENTRY TableEntry;

    TableEntry = NULL;
    Status = EfipAcpiFindTableByHandle(Handle,
                                       &(EfiAcpiContext.TableList),
                                       &TableEntry);

    if (EFI_ERROR(Status)) {
        return EFI_NOT_FOUND;
    }

    EfipAcpiDeleteTable(TableEntry);
    return EFI_SUCCESS;
}

VOID
EfipAcpiDeleteTable (
    PEFI_ACPI_TABLE_ENTRY Table
    )

/*++

Routine Description:

    This routine removes the given table from the ACPI list.

Arguments:

    Table - Supplies a pointer to the table entry to remove.

Return Value:

    EFI Status code.

--*/

{

    BOOLEAN RemoveFromRsdt;
    UINT32 TableSignature;

    RemoveFromRsdt = TRUE;

    ASSERT(Table->Table != NULL);

    TableSignature = Table->Table->Signature;
    if ((TableSignature == FACS_SIGNATURE) ||
        (TableSignature == DSDT_SIGNATURE)) {

        RemoveFromRsdt = FALSE;
    }

    if (TableSignature == FADT_SIGNATURE) {
        RemoveFromRsdt = FALSE;
    }

    if (RemoveFromRsdt != FALSE) {

        ASSERT((EfiAcpiContext.Rsdt != NULL) && (EfiAcpiContext.Xsdt != NULL));

        EfipAcpiRemoveTableFromRsdt(Table,
                                    &(EfiAcpiContext.TableCount),
                                    &(EfiAcpiContext.Rsdt->Header),
                                    &(EfiAcpiContext.Xsdt->Header));
    }

    switch (TableSignature) {
    case FADT_SIGNATURE:
        EfiAcpiContext.Fadt = NULL;
        break;

    case FACS_SIGNATURE:
        EfiAcpiContext.Facs = NULL;
        if (EfiAcpiContext.Fadt != NULL) {
            EfiAcpiContext.Fadt->FirmwareControlAddress = 0;
            EfiSetMem(&(EfiAcpiContext.Fadt->XFirmwareControl),
                      sizeof(UINT64),
                      0);

            EfiAcpiChecksumTable(EfiAcpiContext.Fadt,
                                 EfiAcpiContext.Fadt->Header.Length,
                                 OFFSET_OF(DESCRIPTION_HEADER, Checksum));
        }

        break;

    case DSDT_SIGNATURE:
        EfiAcpiContext.Dsdt = NULL;
        if (EfiAcpiContext.Fadt != NULL) {
            EfiAcpiContext.Fadt->DsdtAddress = 0;
            EfiSetMem(&(EfiAcpiContext.Fadt->XDsdt), sizeof(UINT64), 0);
            EfiAcpiChecksumTable(EfiAcpiContext.Fadt,
                                 EfiAcpiContext.Fadt->Header.Length,
                                 OFFSET_OF(DESCRIPTION_HEADER, Checksum));
        }

        break;

    default:
        break;
    }

    //
    // Remove and free the table entry.
    //

    EfiFreePages(Table->PageAddress, Table->NumberOfPages);
    LIST_REMOVE(&(Table->ListEntry));
    Table->Magic = 0;
    EfiFreePool(Table);
    return;
}

EFI_STATUS
EfipAcpiRemoveTableFromRsdt (
    PEFI_ACPI_TABLE_ENTRY Table,
    UINTN *TableCount,
    PDESCRIPTION_HEADER Rsdt,
    PDESCRIPTION_HEADER Xsdt
    )

/*++

Routine Description:

    This routine removes the given table from the RSDT and optionally from the
    XSDT.

Arguments:

    Table - Supplies a pointer to the table entry to remove.

    TableCount - Supplies a pointer to the current number of tables. This value
        will be updated.

    Rsdt - Supplies a pointer to the RSDT to remove the table from.

    Xsdt - Supplies an optional pointer to the XSDT to remove the table from.

Return Value:

    EFI Status code.

--*/

{

    UINTN Index;
    UINT32 *RsdtEntry;
    UINT64 Table64;
    VOID *XsdtEntry;

    for (Index = 0; Index < *TableCount; Index += 1) {
        RsdtEntry = (UINT32 *)((UINT8 *)Rsdt +
                               sizeof(DESCRIPTION_HEADER) +
                               (Index * sizeof(UINT32)));

        XsdtEntry = NULL;
        Table64 = 0;
        if (Xsdt != NULL) {
            XsdtEntry = (UINT64 *)((UINT8 *)Xsdt +
                                   sizeof(DESCRIPTION_HEADER) +
                                   (Index * sizeof(UINT64)));

            EfiCopyMem(&Table64, XsdtEntry, sizeof(UINT64));
        }

        //
        // Fix up the table if this is the right entry.
        //

        if ((*RsdtEntry == (UINT32)(UINTN)(Table->Table)) &&
            ((Xsdt == NULL) || (Table64 == (UINT64)(UINTN)(Table->Table)))) {

            EfiCopyMem(RsdtEntry,
                       RsdtEntry + 1,
                       (*TableCount - Index) * sizeof(UINT32));

            Rsdt->Length -= sizeof(UINT32);
            if (Xsdt != NULL) {
                EfiCopyMem(XsdtEntry,
                           (UINT64 *)XsdtEntry + 1,
                           (*TableCount - Index * sizeof(UINT64)));

                Xsdt->Length -= sizeof(UINT64);
            }

            break;

        //
        // Watch out for the end of the list, fail if the table wasn't found.
        //

        } else if (Index + 1 == *TableCount) {
            return EFI_INVALID_PARAMETER;
        }
    }

    EfiAcpiChecksumTable(Rsdt,
                         Rsdt->Length,
                         OFFSET_OF(DESCRIPTION_HEADER, Checksum));

    if (Xsdt != NULL) {
        EfiAcpiChecksumTable(Xsdt,
                             Xsdt->Length,
                             OFFSET_OF(DESCRIPTION_HEADER, Checksum));
    }

    *TableCount -= 1;
    return EFI_SUCCESS;
}

EFI_STATUS
EfipAcpiFindTableByHandle (
    UINTN Handle,
    PLIST_ENTRY ListHead,
    PEFI_ACPI_TABLE_ENTRY *FoundEntry
    )

/*++

Routine Description:

    This routine finds the table entry with the given handle.

Arguments:

    Handle - Supplies the handle of the table to find.

    ListHead - Supplies a pointer to the head of the list of table entries.

    FoundEntry - Supplies a pointer where a pointer to the found entry will be
        returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if no table with the given handle exists.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PEFI_ACPI_TABLE_ENTRY TableEntry;

    CurrentEntry = ListHead->Next;
    while (CurrentEntry != ListHead) {
        TableEntry = LIST_VALUE(CurrentEntry, EFI_ACPI_TABLE_ENTRY, ListEntry);

        ASSERT(TableEntry->Magic == EFI_ACPI_TABLE_ENTRY_MAGIC);

        if (TableEntry->Handle == Handle) {
            *FoundEntry = TableEntry;
            return EFI_SUCCESS;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    return EFI_NOT_FOUND;
}

EFI_STATUS
EfipReallocateAcpiTableBuffer (
    VOID
    )

/*++

Routine Description:

    This routine reallocates the RSDT and XSDT table arrays.

Arguments:

    None.

Return Value:

    EFI Status code.

--*/

{

    UINTN CopySize;
    UINT64 CurrentData;
    UINTN NewTableCount;
    EFI_ACPI_CONTEXT Original;
    EFI_PHYSICAL_ADDRESS PageAddress;
    UINT8 *Pointer;
    EFI_STATUS Status;
    UINTN TotalSize;

    EfiCopyMem(&Original, &EfiAcpiContext, sizeof(EFI_ACPI_CONTEXT));
    NewTableCount = Original.TableCapacity + EFI_ACPI_TABLE_EXPANSION_COUNT;
    TotalSize = sizeof(DESCRIPTION_HEADER) + (NewTableCount * sizeof(UINT32)) +
                sizeof(DESCRIPTION_HEADER) + (NewTableCount * sizeof(UINT64));

    //
    // Allocate memory in the lower 4GB.
    //

    PageAddress = 0xFFFFFFFF;
    Status = EfiAllocatePages(AllocateMaxAddress,
                              EfiACPIReclaimMemory,
                              EFI_SIZE_TO_PAGES(TotalSize),
                              &PageAddress);

    if (EFI_ERROR(Status)) {
        return EFI_OUT_OF_RESOURCES;
    }

    Pointer = (UINT8 *)(UINTN)PageAddress;
    EfiSetMem(Pointer, TotalSize, 0);
    EfiAcpiContext.Rsdt = (PRSDT)Pointer;
    Pointer += sizeof(DESCRIPTION_HEADER) + (NewTableCount * sizeof(UINT32));
    EfiAcpiContext.Xsdt = (PXSDT)Pointer;

    //
    // Update the RSDP to point to the new RSDT and XSDT.
    //

    ASSERT(EfiAcpiContext.Rsdp != NULL);

    EfiAcpiContext.Rsdp->RsdtAddress = (UINT32)(UINTN)(EfiAcpiContext.Rsdt);
    CurrentData = (UINTN)(EfiAcpiContext.Xsdt);
    EfiCopyMem(&(EfiAcpiContext.Rsdp->XsdtAddress),
               &CurrentData,
               sizeof(UINT64));

    //
    // Copy the original structures to the new buffer.
    //

    CopySize = sizeof(DESCRIPTION_HEADER) +
               (Original.TableCount * sizeof(UINT32));

    EfiCopyMem(EfiAcpiContext.Rsdt, Original.Rsdt, CopySize);
    CopySize = sizeof(DESCRIPTION_HEADER) +
               (Original.TableCount * sizeof(UINT64));

    EfiCopyMem(EfiAcpiContext.Xsdt, Original.Xsdt, CopySize);

    //
    // Free the original buffer.
    //

    TotalSize = sizeof(DESCRIPTION_HEADER) +
                (Original.TableCapacity * sizeof(UINT32)) +
                sizeof(DESCRIPTION_HEADER) +
                (Original.TableCapacity * sizeof(UINT64));

    EfiFreePages((EFI_PHYSICAL_ADDRESS)(UINTN)(Original.Rsdt),
                 EFI_SIZE_TO_PAGES(TotalSize));

    EfiAcpiContext.TableCapacity = NewTableCount;
    return EFI_SUCCESS;
}

EFI_STATUS
EfipAcpiPublishTables (
    VOID
    )

/*++

Routine Description:

    This routine installs the ACPI tables as an EFI configuration table.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 *RsdtEntry;
    EFI_STATUS Status;
    UINT64 Value64;
    VOID *XsdtEntry;

    if (EfiAcpiContext.Fadt != NULL) {
        RsdtEntry = (UINT32 *)((UINT8 *)(EfiAcpiContext.Rsdt) +
                               sizeof(DESCRIPTION_HEADER));

        *RsdtEntry = (UINT32)(UINTN)(EfiAcpiContext.Fadt);
        XsdtEntry = (VOID *)((UINT8 *)(EfiAcpiContext.Xsdt) +
                             sizeof(DESCRIPTION_HEADER));

        Value64 = (UINT64)(UINTN)(EfiAcpiContext.Fadt);
        EfiCopyMem(XsdtEntry, &Value64, sizeof(UINT64));
    }

    EfipAcpiChecksumCommonTables();
    Status = EfiInstallConfigurationTable(&EfiAcpiTableGuid,
                                          EfiAcpiContext.Rsdp);

    if (EFI_ERROR(Status)) {
        return EFI_ABORTED;
    }

    return EFI_SUCCESS;
}

VOID
EfipAcpiChecksumCommonTables (
    VOID
    )

/*++

Routine Description:

    This routine recomputes the checksums on everyone's favorite ACPI tables.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINTN Offset;

    Offset = OFFSET_OF(DESCRIPTION_HEADER, Checksum);
    EfiAcpiChecksumTable(EfiAcpiContext.Rsdp,
                         OFFSET_OF(RSDP, Length),
                         OFFSET_OF(RSDP, Checksum));

    EfiAcpiChecksumTable(EfiAcpiContext.Rsdp,
                         sizeof(RSDP),
                         OFFSET_OF(RSDP, ExtendedChecksum));

    EfiAcpiChecksumTable(EfiAcpiContext.Rsdt,
                         EfiAcpiContext.Rsdt->Header.Length,
                         Offset);

    EfiAcpiChecksumTable(EfiAcpiContext.Xsdt,
                         EfiAcpiContext.Xsdt->Header.Length,
                         Offset);

    return;
}

