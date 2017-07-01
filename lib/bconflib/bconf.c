/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    bconf.c

Abstract:

    This module implements the Boot Configuration Library.

Author:

    Evan Green 20-Feb-2014

Environment:

    Any

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/lib/bconf.h>
#include "bconfp.h"

//
// --------------------------------------------------------------------- Macros
//

//
// Define some macros for memory allocation.
//

#define BcAllocate(_Context, _Size) (_Context)->AllocateFunction(_Size)
#define BcFree(_Context, _Memory) (_Context)->FreeFunction(_Memory)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the initial string table allocation size.
//

#define INITIAL_BOOT_CONFIGURATION_STRING_TABLE_SIZE 1024

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
BcpParseBootEntry (
    PBOOT_CONFIGURATION_CONTEXT Context,
    ULONG EntryIndex,
    PBOOT_ENTRY *Entry
    );

VOID
BcpDestroyBootEntries (
    PBOOT_CONFIGURATION_CONTEXT Context
    );

KSTATUS
BcpValidateHeader (
    PBOOT_CONFIGURATION_CONTEXT Context
    );

KSTATUS
BcpReadString (
    PBOOT_CONFIGURATION_CONTEXT Context,
    ULONG StringOffset,
    PCSTR *String
    );

KSTATUS
BcpAddToStringTable (
    PBOOT_CONFIGURATION_CONTEXT Context,
    PCSTR String,
    PULONG StringIndex,
    PVOID *StringTable,
    PULONG StringTableSize,
    PULONG StringTableCapacity
    );

PSTR
BcpCopyString (
    PBOOT_CONFIGURATION_CONTEXT Context,
    PCSTR String
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
BcInitializeContext (
    PBOOT_CONFIGURATION_CONTEXT Context
    )

/*++

Routine Description:

    This routine initializes the given boot configuration context.

Arguments:

    Context - Supplies a pointer to the context to initialize. The caller must
        have filled in the allocate and free functions, optionally filled in
        the file data, and zeroed the rest of the structure.

Return Value:

    Status code.

--*/

{

    if ((Context->AllocateFunction == NULL) ||
        (Context->FreeFunction == NULL)) {

        ASSERT(FALSE);

        return STATUS_INVALID_PARAMETER;
    }

    Context->BootEntries = NULL;
    Context->BootEntryCount = 0;
    return STATUS_SUCCESS;
}

VOID
BcDestroyContext (
    PBOOT_CONFIGURATION_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys the given boot configuration context. It will free
    all resources contained in the structure, including the file data.

Arguments:

    Context - Supplies a pointer to the context to destroy.

Return Value:

    None.

--*/

{

    BcpDestroyBootEntries(Context);
    if (Context->FileData != NULL) {
        BcFree(Context, Context->FileData);
        Context->FileData = NULL;
    }

    Context->FileDataSize = 0;
    return;
}

VOID
BcDestroyBootEntry (
    PBOOT_CONFIGURATION_CONTEXT Context,
    PBOOT_ENTRY Entry
    )

/*++

Routine Description:

    This routine destroys the given boot entry, freeing all its resources.

Arguments:

    Context - Supplies a pointer to the context containing the entry.

    Entry - Supplies a pointer to the entry to destroy.

Return Value:

    None.

--*/

{

    if (Entry->Name != NULL) {
        BcFree(Context, (PSTR)(Entry->Name));
    }

    if (Entry->LoaderArguments != NULL) {
        BcFree(Context, (PSTR)(Entry->LoaderArguments));
    }

    if (Entry->KernelArguments != NULL) {
        BcFree(Context, (PSTR)(Entry->KernelArguments));
    }

    if (Entry->LoaderPath != NULL) {
        BcFree(Context, (PSTR)(Entry->LoaderPath));
    }

    if (Entry->KernelPath != NULL) {
        BcFree(Context, (PSTR)(Entry->KernelPath));
    }

    if (Entry->SystemPath != NULL) {
        BcFree(Context, (PSTR)(Entry->SystemPath));
    }

    BcFree(Context, Entry);
    return;
}

KSTATUS
BcReadBootConfigurationFile (
    PBOOT_CONFIGURATION_CONTEXT Context
    )

/*++

Routine Description:

    This routine parses the boot configuration file out into boot entries that
    can be manipulated by consumers of this library.

Arguments:

    Context - Supplies a pointer to the context. The file data and file data
        size must have been filled in by the caller.

Return Value:

    Status code.

--*/

{

    PBOOT_ENTRY *EntryArray;
    ULONG EntryIndex;
    PBOOT_CONFIGURATION_HEADER Header;
    KSTATUS Status;

    EntryArray = NULL;
    Header = NULL;

    //
    // Destroy any previous boot entries.
    //

    BcpDestroyBootEntries(Context);
    if ((Context->FileData == NULL) || (Context->FileDataSize == 0)) {
        Status = STATUS_NOT_INITIALIZED;
        goto ReadBootConfigurationFileEnd;
    }

    //
    // Validate the header (which checksums the whole file as well).
    //

    Status = BcpValidateHeader(Context);
    if (!KSUCCESS(Status)) {
        goto ReadBootConfigurationFileEnd;
    }

    Header = Context->FileData;
    Context->GlobalConfiguration.Key = Header->Key;
    Context->GlobalConfiguration.DefaultBootEntry = NULL;
    Context->GlobalConfiguration.BootOnce = NULL;
    Context->GlobalConfiguration.Timeout = Header->Timeout;
    EntryArray = BcAllocate(Context, sizeof(PVOID) * Header->EntryCount);
    if (EntryArray == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ReadBootConfigurationFileEnd;
    }

    RtlZeroMemory(EntryArray, sizeof(PVOID) * Header->EntryCount);
    for (EntryIndex = 0; EntryIndex < Header->EntryCount; EntryIndex += 1) {
        Status = BcpParseBootEntry(Context,
                                   EntryIndex,
                                   &(EntryArray[EntryIndex]));

        if (!KSUCCESS(Status)) {
            goto ReadBootConfigurationFileEnd;
        }
    }

    //
    // Set the new array of boot entries.
    //

    Context->BootEntries = EntryArray;
    Context->BootEntryCount = Header->EntryCount;
    Status = STATUS_SUCCESS;

ReadBootConfigurationFileEnd:
    if (!KSUCCESS(Status)) {
        if (EntryArray != NULL) {
            for (EntryIndex = 0;
                 EntryIndex < Header->EntryCount;
                 EntryIndex += 1) {

                BcDestroyBootEntry(Context, EntryArray[EntryIndex]);
            }

            BcFree(Context, EntryArray);
            EntryArray = NULL;
        }
    }

    return Status;
}

KSTATUS
BcWriteBootConfigurationFile (
    PBOOT_CONFIGURATION_CONTEXT Context
    )

/*++

Routine Description:

    This routine writes the boot entries into the file buffer.

Arguments:

    Context - Supplies a pointer to the context. If there is existing file
        data it will be freed, and new file data will be allocated.

Return Value:

    Status code.

--*/

{

    UINTN AllocationSize;
    PBOOT_ENTRY Entry;
    ULONG EntryIndex;
    PBOOT_CONFIGURATION_ENTRY FileEntries;
    PBOOT_CONFIGURATION_ENTRY FileEntry;
    PBOOT_CONFIGURATION_HEADER FinalHeader;
    BOOT_CONFIGURATION_HEADER Header;
    PVOID NewFileData;
    KSTATUS Status;
    ULONG StringIndex;
    PVOID StringTable;
    ULONG StringTableCapacity;
    ULONG StringTableSize;

    FileEntries = NULL;
    NewFileData = NULL;
    StringTable = NULL;
    StringTableCapacity = 0;
    StringTableSize = 0;

    //
    // Initialize the header.
    //

    RtlZeroMemory(&Header, sizeof(BOOT_CONFIGURATION_HEADER));
    Header.DefaultEntry = -1;
    Header.BootOnce = -1;

    //
    // Create the boot entries array.
    //

    AllocationSize = Context->BootEntryCount * sizeof(BOOT_CONFIGURATION_ENTRY);
    FileEntries = BcAllocate(Context, AllocationSize);
    if (FileEntries == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto WriteBootConfigurationFileEnd;
    }

    RtlZeroMemory(FileEntries, AllocationSize);

    //
    // Create each of the boot entries, adding strings to the table along the
    // way.
    //

    for (EntryIndex = 0;
         EntryIndex < Context->BootEntryCount;
         EntryIndex += 1) {

        Entry = Context->BootEntries[EntryIndex];
        FileEntry = &(FileEntries[EntryIndex]);

        //
        // Re-number the boot entry ID numbers. Also save this new ID back into
        // the boot entry.
        //

        FileEntry->Id = EntryIndex + 1;
        Entry->Id = FileEntry->Id;
        FileEntry->Flags = Entry->Flags;
        FileEntry->DebugDevice = Entry->DebugDevice;
        RtlCopyMemory(&(FileEntry->DiskId),
                      &(Entry->DiskId),
                      BOOT_DISK_ID_SIZE);

        RtlCopyMemory(&(FileEntry->PartitionId),
                      &(Entry->PartitionId),
                      BOOT_PARTITION_ID_SIZE);

        Status = BcpAddToStringTable(Context,
                                     Entry->Name,
                                     &StringIndex,
                                     &StringTable,
                                     &StringTableSize,
                                     &StringTableCapacity);

        if (!KSUCCESS(Status)) {
            goto WriteBootConfigurationFileEnd;
        }

        FileEntry->Name = StringIndex;
        Status = BcpAddToStringTable(Context,
                                     Entry->LoaderArguments,
                                     &StringIndex,
                                     &StringTable,
                                     &StringTableSize,
                                     &StringTableCapacity);

        if (!KSUCCESS(Status)) {
            goto WriteBootConfigurationFileEnd;
        }

        FileEntry->LoaderArguments = StringIndex;
        Status = BcpAddToStringTable(Context,
                                     Entry->KernelArguments,
                                     &StringIndex,
                                     &StringTable,
                                     &StringTableSize,
                                     &StringTableCapacity);

        if (!KSUCCESS(Status)) {
            goto WriteBootConfigurationFileEnd;
        }

        FileEntry->KernelArguments = StringIndex;
        Status = BcpAddToStringTable(Context,
                                     Entry->LoaderPath,
                                     &StringIndex,
                                     &StringTable,
                                     &StringTableSize,
                                     &StringTableCapacity);

        if (!KSUCCESS(Status)) {
            goto WriteBootConfigurationFileEnd;
        }

        FileEntry->LoaderPath = StringIndex;
        Status = BcpAddToStringTable(Context,
                                     Entry->KernelPath,
                                     &StringIndex,
                                     &StringTable,
                                     &StringTableSize,
                                     &StringTableCapacity);

        if (!KSUCCESS(Status)) {
            goto WriteBootConfigurationFileEnd;
        }

        FileEntry->KernelPath = StringIndex;
        Status = BcpAddToStringTable(Context,
                                     Entry->SystemPath,
                                     &StringIndex,
                                     &StringTable,
                                     &StringTableSize,
                                     &StringTableCapacity);

        if (!KSUCCESS(Status)) {
            goto WriteBootConfigurationFileEnd;
        }

        FileEntry->SystemPath = StringIndex;

        //
        // If this is the default or boot once entries, fill in the ID now.
        //

        if (Context->GlobalConfiguration.DefaultBootEntry == Entry) {
            Header.DefaultEntry = FileEntry->Id;
        }

        if (Context->GlobalConfiguration.BootOnce == Entry) {
            Header.BootOnce = FileEntry->Id;
        }
    }

    Header.Magic = BOOT_CONFIGURATION_HEADER_MAGIC;
    Header.Version = BOOT_CONFIGURATION_VERSION;
    Header.Key = Context->GlobalConfiguration.Key + 1;
    Header.EntriesOffset = sizeof(BOOT_CONFIGURATION_HEADER);
    Header.EntrySize = sizeof(BOOT_CONFIGURATION_ENTRY);
    Header.EntryCount = Context->BootEntryCount;
    Header.StringsOffset = Header.EntriesOffset +
                           (Header.EntrySize * Header.EntryCount);

    Header.StringsSize = StringTableSize;
    Header.TotalSize = Header.StringsOffset + Header.StringsSize;
    Header.Timeout = Context->GlobalConfiguration.Timeout;

    //
    // Allocate and write out the new file data.
    //

    NewFileData = BcAllocate(Context, Header.TotalSize);
    if (NewFileData == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto WriteBootConfigurationFileEnd;
    }

    RtlCopyMemory(NewFileData, &Header, sizeof(BOOT_CONFIGURATION_HEADER));
    RtlCopyMemory(NewFileData + Header.EntriesOffset,
                  FileEntries,
                  Header.EntrySize * Header.EntryCount);

    RtlCopyMemory(NewFileData + Header.StringsOffset,
                  StringTable,
                  Header.StringsSize);

    //
    // Compute the CRC32 over the entire buffer.
    //

    FinalHeader = NewFileData;
    FinalHeader->Crc32 = RtlComputeCrc32(0, NewFileData, Header.TotalSize);

    //
    // Free the old file data if there was any, and install this new data.
    //

    if (Context->FileData != NULL) {
        BcFree(Context, Context->FileData);
    }

    Context->FileData = NewFileData;
    Context->FileDataSize = Header.TotalSize;
    NewFileData = NULL;
    Status = STATUS_SUCCESS;

WriteBootConfigurationFileEnd:
    if (NewFileData != NULL) {
        BcFree(Context, NewFileData);
    }

    if (FileEntries != NULL) {
        BcFree(Context, FileEntries);
    }

    if (StringTable != NULL) {
        BcFree(Context, StringTable);
    }

    return Status;
}

KSTATUS
BcCreateDefaultBootConfiguration (
    PBOOT_CONFIGURATION_CONTEXT Context,
    UCHAR DiskId[BOOT_DISK_ID_SIZE],
    UCHAR PartitionId[BOOT_PARTITION_ID_SIZE]
    )

/*++

Routine Description:

    This routine sets up the boot configuration data, creating a single default
    entry.

Arguments:

    Context - Supplies a pointer to the boot configuration context.

    DiskId - Supplies the disk ID of the boot entry.

    PartitionId - Supplies the partition ID of the boot entry.

Return Value:

    Returns a pointer to the new boot entry on success.

    NULL on allocation failure.

--*/

{

    PBOOT_ENTRY *Entries;
    KSTATUS Status;

    //
    // Create a new boot entry array, and then create a single default boot
    // entry.
    //

    Status = STATUS_INSUFFICIENT_RESOURCES;
    Entries = BcAllocate(Context, sizeof(PVOID));
    if (Entries == NULL) {
        goto CreateDefaultBootConfigurationEnd;
    }

    Entries[0] = BcCreateDefaultBootEntry(Context, NULL, DiskId, PartitionId);
    if (Entries[0] == NULL) {
        goto CreateDefaultBootConfigurationEnd;
    }

    BcpDestroyBootEntries(Context);
    Context->GlobalConfiguration.DefaultBootEntry = Entries[0];
    Context->GlobalConfiguration.BootOnce = NULL;
    Context->GlobalConfiguration.Timeout = BOOT_DEFAULT_TIMEOUT;
    Context->BootEntries = Entries;
    Context->BootEntryCount = 1;
    Entries = NULL;
    Status = STATUS_SUCCESS;

CreateDefaultBootConfigurationEnd:
    if (Entries != NULL) {
        if (Entries[0] != NULL) {
            BcDestroyBootEntry(Context, Entries[0]);
        }

        BcFree(Context, Entries);
    }

    return Status;
}

PBOOT_ENTRY
BcCreateDefaultBootEntry (
    PBOOT_CONFIGURATION_CONTEXT Context,
    PSTR Name,
    UCHAR DiskId[BOOT_DISK_ID_SIZE],
    UCHAR PartitionId[BOOT_PARTITION_ID_SIZE]
    )

/*++

Routine Description:

    This routine creates a new boot entry with the default values.

Arguments:

    Context - Supplies a pointer to the boot configuration context.

    Name - Supplies an optional pointer to a string containing the name of the
        entry. A copy of this string will be made. If no name is supplied, a
        default name will be used.

    DiskId - Supplies the disk ID of the boot entry.

    PartitionId - Supplies the partition ID of the boot entry.

Return Value:

    Returns a pointer to the new boot entry on success.

    NULL on allocation failure.

--*/

{

    PBOOT_ENTRY Entry;
    KSTATUS Status;

    Status = STATUS_INSUFFICIENT_RESOURCES;
    Entry = BcAllocate(Context, sizeof(BOOT_ENTRY));
    if (Entry == NULL) {
        goto CreateDefaultBootEntryEnd;
    }

    RtlZeroMemory(Entry, sizeof(BOOT_ENTRY));
    Entry->Id = 0;
    RtlCopyMemory(Entry->DiskId, DiskId, sizeof(Entry->DiskId));
    RtlCopyMemory(Entry->PartitionId, PartitionId, sizeof(Entry->PartitionId));
    if (Name != NULL) {
        Entry->Name = BcpCopyString(Context, Name);

    } else {
        Entry->Name = BcpCopyString(Context, BOOT_DEFAULT_NAME);
    }

    if (Entry->Name == NULL) {
        goto CreateDefaultBootEntryEnd;
    }

    Entry->LoaderPath = BcpCopyString(Context, BOOT_DEFAULT_LOADER_PATH);
    if (Entry->LoaderPath == NULL) {
        goto CreateDefaultBootEntryEnd;
    }

    Entry->KernelPath = BcpCopyString(Context, BOOT_DEFAULT_KERNEL_PATH);
    if (Entry->KernelPath == NULL) {
        goto CreateDefaultBootEntryEnd;
    }

    Entry->SystemPath = BcpCopyString(Context, BOOT_DEFAULT_SYSTEM_PATH);
    if (Entry->SystemPath == NULL) {
        goto CreateDefaultBootEntryEnd;
    }

    Status = STATUS_SUCCESS;

CreateDefaultBootEntryEnd:
    if (!KSUCCESS(Status)) {
        if (Entry != NULL) {
            BcDestroyBootEntry(Context, Entry);
            Entry = NULL;
        }
    }

    return Entry;
}

PBOOT_ENTRY
BcCopyBootEntry (
    PBOOT_CONFIGURATION_CONTEXT Context,
    PBOOT_ENTRY Source
    )

/*++

Routine Description:

    This routine creates a new boot entry based on an existing one.

Arguments:

    Context - Supplies a pointer to the boot configuration context.

    Source - Supplies a pointer to the boot entry to copy.

Return Value:

    Returns a pointer to the new boot entry on success.

    NULL on allocation failure.

--*/

{

    PBOOT_ENTRY Entry;
    KSTATUS Status;
    PCSTR String;

    Status = STATUS_INSUFFICIENT_RESOURCES;
    Entry = BcAllocate(Context, sizeof(BOOT_ENTRY));
    if (Entry == NULL) {
        goto CreateDefaultBootEntryEnd;
    }

    RtlZeroMemory(Entry, sizeof(BOOT_ENTRY));
    Entry->Id = Source->Id;
    RtlCopyMemory(Entry->DiskId, Source->DiskId, sizeof(Entry->DiskId));
    RtlCopyMemory(Entry->PartitionId,
                  Source->PartitionId,
                  sizeof(Entry->PartitionId));

    Entry->Flags = Source->Flags;
    Entry->DebugDevice = Source->DebugDevice;
    String = Source->Name;
    if (String == NULL) {
        String = BOOT_DEFAULT_NAME;
    }

    Entry->Name = BcpCopyString(Context, String);
    if (Entry->Name == NULL) {
        goto CreateDefaultBootEntryEnd;
    }

    String = Source->LoaderArguments;
    if (String != NULL) {
        Entry->LoaderArguments = BcpCopyString(Context, String);
        if (Entry->LoaderArguments == NULL) {
            goto CreateDefaultBootEntryEnd;
        }
    }

    String = Source->KernelArguments;
    if (String != NULL) {
        Entry->KernelArguments = BcpCopyString(Context, String);
        if (Entry->KernelArguments == NULL) {
            goto CreateDefaultBootEntryEnd;
        }
    }

    String = Source->LoaderPath;
    if (String == NULL) {
        String = BOOT_DEFAULT_LOADER_PATH;
    }

    Entry->LoaderPath = BcpCopyString(Context, String);
    if (Entry->LoaderPath == NULL) {
        goto CreateDefaultBootEntryEnd;
    }

    String = Source->KernelPath;
    if (String == NULL) {
        String = BOOT_DEFAULT_KERNEL_PATH;
    }

    Entry->KernelPath = BcpCopyString(Context, String);
    if (Entry->KernelPath == NULL) {
        goto CreateDefaultBootEntryEnd;
    }

    String = Source->SystemPath;
    if (String == NULL) {
        String = BOOT_DEFAULT_SYSTEM_PATH;
    }

    Entry->SystemPath = BcpCopyString(Context, String);
    if (Entry->SystemPath == NULL) {
        goto CreateDefaultBootEntryEnd;
    }

    Status = STATUS_SUCCESS;

CreateDefaultBootEntryEnd:
    if (!KSUCCESS(Status)) {
        if (Entry != NULL) {
            BcDestroyBootEntry(Context, Entry);
            Entry = NULL;
        }
    }

    return Entry;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
BcpParseBootEntry (
    PBOOT_CONFIGURATION_CONTEXT Context,
    ULONG EntryIndex,
    PBOOT_ENTRY *Entry
    )

/*++

Routine Description:

    This routine parses a single boot entry.

Arguments:

    Context - Supplies a pointer to the boot configuration context.

    EntryIndex - Supplies the entry number to parse.

    Entry - Supplies a pointer where a pointer to the entry will be returned
        on success.

Return Value:

    Status code.

--*/

{

    PBOOT_CONFIGURATION_ENTRY FileEntry;
    PBOOT_CONFIGURATION_HEADER Header;
    PBOOT_ENTRY NewEntry;
    KSTATUS Status;

    NewEntry = BcAllocate(Context, sizeof(BOOT_ENTRY));
    if (NewEntry == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ParseBootEntryEnd;
    }

    RtlZeroMemory(NewEntry, sizeof(BOOT_ENTRY));
    Header = Context->FileData;
    FileEntry = Context->FileData + Header->EntriesOffset +
                (EntryIndex * Header->EntrySize);

    NewEntry->Id = FileEntry->Id;
    NewEntry->Flags = FileEntry->Flags;
    NewEntry->DebugDevice = FileEntry->DebugDevice;
    RtlCopyMemory(&(NewEntry->DiskId), &(FileEntry->DiskId), BOOT_DISK_ID_SIZE);
    RtlCopyMemory(&(NewEntry->PartitionId),
                  &(FileEntry->PartitionId),
                  BOOT_PARTITION_ID_SIZE);

    Status = BcpReadString(Context, FileEntry->Name, &(NewEntry->Name));
    if (!KSUCCESS(Status)) {
        goto ParseBootEntryEnd;
    }

    Status = BcpReadString(Context,
                           FileEntry->LoaderArguments,
                           &(NewEntry->LoaderArguments));

    if (!KSUCCESS(Status)) {
        goto ParseBootEntryEnd;
    }

    Status = BcpReadString(Context,
                           FileEntry->KernelArguments,
                           &(NewEntry->KernelArguments));

    if (!KSUCCESS(Status)) {
        goto ParseBootEntryEnd;
    }

    Status = BcpReadString(Context,
                           FileEntry->LoaderPath,
                           &(NewEntry->LoaderPath));

    if (!KSUCCESS(Status)) {
        goto ParseBootEntryEnd;
    }

    Status = BcpReadString(Context,
                           FileEntry->KernelPath,
                           &(NewEntry->KernelPath));

    if (!KSUCCESS(Status)) {
        goto ParseBootEntryEnd;
    }

    Status = BcpReadString(Context,
                           FileEntry->SystemPath,
                           &(NewEntry->SystemPath));

    if (!KSUCCESS(Status)) {
        goto ParseBootEntryEnd;
    }

    //
    // If the IDs match for the default boot entry or boot once entry, set
    // those pointers now.
    //

    if (Header->DefaultEntry == FileEntry->Id) {
        Context->GlobalConfiguration.DefaultBootEntry = NewEntry;
    }

    if (Header->BootOnce == FileEntry->Id) {
        Context->GlobalConfiguration.BootOnce = NewEntry;
    }

    Status = STATUS_SUCCESS;

ParseBootEntryEnd:
    if (!KSUCCESS(Status)) {
        if (NewEntry != NULL) {
            BcDestroyBootEntry(Context, NewEntry);
            NewEntry = NULL;
        }
    }

    *Entry = NewEntry;
    return Status;
}

VOID
BcpDestroyBootEntries (
    PBOOT_CONFIGURATION_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys the boot entries in the given context.

Arguments:

    Context - Supplies a pointer to the context.

Return Value:

    None.

--*/

{

    PBOOT_ENTRY Entry;
    ULONG EntryIndex;

    for (EntryIndex = 0;
         EntryIndex < Context->BootEntryCount;
         EntryIndex += 1) {

        Entry = Context->BootEntries[EntryIndex];
        if (Entry != NULL) {
            BcDestroyBootEntry(Context, Entry);
        }
    }

    if (Context->BootEntries != NULL) {
        BcFree(Context, Context->BootEntries);
    }

    Context->BootEntries = NULL;
    Context->BootEntryCount = 0;
    return;
}

KSTATUS
BcpValidateHeader (
    PBOOT_CONFIGURATION_CONTEXT Context
    )

/*++

Routine Description:

    This routine performs sanity checks on the boot configuration file to make
    sure it's valid.

Arguments:

    Context - Supplies a pointer to the context. The file data must be filled
        in.

Return Value:

    Status code.

--*/

{

    ULONG ComputedCrc;
    PBOOT_CONFIGURATION_HEADER Header;
    ULONG HeaderCrc;
    PUCHAR LastCharacter;
    ULONG MaxEntries;
    KSTATUS Status;

    Status = STATUS_FILE_CORRUPT;
    if (Context->FileDataSize < sizeof(BOOT_CONFIGURATION_HEADER)) {
        goto ValidateHeaderEnd;
    }

    //
    // Check that the header is there.
    //

    Header = Context->FileData;
    if ((Header->Magic != BOOT_CONFIGURATION_HEADER_MAGIC) ||
        (Header->TotalSize < sizeof(BOOT_CONFIGURATION_HEADER))) {

        goto ValidateHeaderEnd;
    }

    //
    // Make sure the size reported is at least as big as the data buffer here.
    //

    if (Header->TotalSize > Context->FileDataSize) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto ValidateHeaderEnd;
    }

    //
    // Sanity check the offsets.
    //

    if ((Header->EntrySize >= Header->TotalSize) ||
        (Header->EntriesOffset >= Header->TotalSize) ||
        (Header->StringsOffset >= Header->TotalSize) ||
        (Header->StringsSize > Header->TotalSize) ||
        (Header->StringsOffset + Header->StringsSize > Header->TotalSize) ||
        (Header->StringsSize == 0)) {

        goto ValidateHeaderEnd;
    }

    //
    // Perform an estimate on the maximum entries that could possibly fit to
    // weed out multiplication overflow issues below.
    //

    MaxEntries = Header->TotalSize / Header->EntrySize;
    if (Header->EntryCount >= MaxEntries) {
        goto ValidateHeaderEnd;
    }

    //
    // Now that it's at least reasonable, check the actual bounds.
    //

    if ((Header->EntriesOffset + (Header->EntryCount * Header->EntrySize)) >
        Header->TotalSize) {

        goto ValidateHeaderEnd;
    }

    //
    // Ensure the last character of the string table is a terminator.
    //

    LastCharacter = (PUCHAR)Header + Header->StringsOffset +
                    Header->StringsSize - 1;

    if (*LastCharacter != '\0') {
        Status = STATUS_INVALID_SEQUENCE;
        goto ValidateHeaderEnd;
    }

    //
    // Compute the CRC of the whole file.
    //

    HeaderCrc = Header->Crc32;
    Header->Crc32 = 0;
    ComputedCrc = RtlComputeCrc32(0, Header, Header->TotalSize);
    Header->Crc32 = HeaderCrc;
    if (ComputedCrc != HeaderCrc) {
        Status = STATUS_CHECKSUM_MISMATCH;
        goto ValidateHeaderEnd;
    }

    //
    // Lookin real good.
    //

    Status = STATUS_SUCCESS;

ValidateHeaderEnd:
    return Status;
}

KSTATUS
BcpReadString (
    PBOOT_CONFIGURATION_CONTEXT Context,
    ULONG StringOffset,
    PCSTR *String
    )

/*++

Routine Description:

    This routine reads a string out of the given string table.

Arguments:

    Context - Supplies a pointer to the boot configuration context.

    StringOffset - Supplies the offset into the string table.

    String - Supplies a pointer where a pointer to a copy of the string will be
        returned on success. The caller is responsible for freeing this memory
        when finished.

Return Value:

    Status code.

--*/

{

    PBOOT_CONFIGURATION_HEADER Header;
    PSTR TableString;

    *String = NULL;
    Header = Context->FileData;
    if (StringOffset >= Header->StringsSize) {
        return STATUS_BUFFER_OVERRUN;
    }

    TableString = Context->FileData + Header->StringsOffset + StringOffset;
    *String = BcpCopyString(Context, TableString);
    if (*String == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    return STATUS_SUCCESS;
}

KSTATUS
BcpAddToStringTable (
    PBOOT_CONFIGURATION_CONTEXT Context,
    PCSTR String,
    PULONG StringIndex,
    PVOID *StringTable,
    PULONG StringTableSize,
    PULONG StringTableCapacity
    )

/*++

Routine Description:

    This routine adds a string to the given string table.

Arguments:

    Context - Supplies a pointer to the boot configuration context.

    String - Supplies a pointer to the string to add.

    StringIndex - Supplies a pointer where the index of the given string in
        the string table will be returned.

    StringTable - Supplies a pointer that on input contains the string table.
        This may get reallocated if the string table needs to expand.

    StringTableSize - Supplies a pointer that on input contains the size of the
        string table in bytes. On output, this value will be updated to contain
        the new size of the string table with this new string added.

    StringTableCapacity - Supplies a pointer that on input contains the size of
        the string table allocation. This may be updated if the string table
        is reallocated.

Return Value:

    Status code.

--*/

{

    ULONG Length;
    PUCHAR NewBuffer;
    ULONG NewCapacity;

    if (String == NULL) {
        String = "";
    }

    Length = RtlStringLength(String);

    //
    // Reallocate if needed.
    //

    if (*StringTableSize + Length + 1 > *StringTableCapacity) {
        if (*StringTableCapacity == 0) {
            NewCapacity = INITIAL_BOOT_CONFIGURATION_STRING_TABLE_SIZE;

        } else {
            NewCapacity = *StringTableCapacity * 2;
        }

        while (NewCapacity < *StringTableSize + Length + 1) {

            ASSERT(NewCapacity * 2 > NewCapacity);

            NewCapacity *= 2;
        }

        NewBuffer = BcAllocate(Context, NewCapacity);
        if (NewBuffer == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        //
        // If the string table is just being created, populate it with the
        // empty string. Otherwise, copy the previous contents over.
        //

        if (*StringTableSize == 0) {
            *NewBuffer = '\0';
            *StringTableSize = 1;

        } else {
            RtlCopyMemory(NewBuffer, *StringTable, *StringTableSize);
            BcFree(Context, *StringTable);
        }

        *StringTableCapacity = NewCapacity;
        *StringTable = NewBuffer;
    }

    //
    // If the length is zero, reuse the empty string. Otherwise, add this
    // onto the end.
    //

    if (Length != 0) {
        *StringIndex = *StringTableSize;
        RtlCopyMemory(*StringTable + *StringTableSize, String, Length + 1);
        *StringTableSize += Length + 1;

    } else {
        *StringIndex = 0;
    }

    return STATUS_SUCCESS;
}

PSTR
BcpCopyString (
    PBOOT_CONFIGURATION_CONTEXT Context,
    PCSTR String
    )

/*++

Routine Description:

    This routine allocates and copies the given string.

Arguments:

    Context - Supplies a pointer to the boot configuration context.

    String - Supplies a pointer to the string to copy.

Return Value:

    Returns a pointer to a newly allocated copy of the given string. The caller
    is responsible for freeing this memory when finished with it.

    NULL on allocation failure.

--*/

{

    PSTR Copy;
    UINTN Length;

    Length = RtlStringLength(String);
    Copy = BcAllocate(Context, Length + 1);
    if (Copy == NULL) {
        return NULL;
    }

    RtlCopyMemory(Copy, String, Length + 1);
    return Copy;
}

