/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fatsup.c

Abstract:

    This module contains internal support routines for the FAT file system
    library.

Author:

    Evan Green 23-Sep-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/fat/fatlib.h>
#include <minoca/lib/fat/fat.h>
#include "fatlibp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define constants used in the linear congruential generator.
//

#define RANDOM_MULTIPLIER 1103515245
#define RANDOM_INCREMENT 12345

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
FatpInitializeDirectory (
    PVOID Volume,
    FILE_ID ParentDirectoryFileId,
    PFAT_DIRECTORY_ENTRY Entry
    );

KSTATUS
FatpCreateDirectoryEntriesForFile (
    PFAT_VOLUME Volume,
    PCSTR FileName,
    ULONG FileNameLength,
    PFAT_DIRECTORY_ENTRY BaseEntry,
    PFAT_DIRECTORY_ENTRY *NewEntries,
    PULONG EntryCount
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Keep the seed for FAT's implementation of pseudo-random numbers. It's dirt
// poor but 1) very fast and 2) doesn't need to be that good.
//

ULONG FatRandomSeed;

//
// Set this to TRUE to be as compatible as possible with other implementations.
// This includes using short names when possible, rather than encoding
// permission information in the short names.
//

BOOL FatCompatibilityMode = FALSE;

//
// Set this to TRUE to maintain the count of free clusters in the FAT FS
// information block. Most OSes don't trust or maintain this value anymore, and
// keeping it up to date generates a lot of extra I/O
//

BOOL FatMaintainFreeClusterCount = FALSE;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
FatpLookupDirectoryEntry (
    PFAT_VOLUME Volume,
    PFAT_DIRECTORY_CONTEXT Directory,
    PCSTR Name,
    ULONG NameLength,
    PFAT_DIRECTORY_ENTRY Entry,
    PULONGLONG EntryOffset
    )

/*++

Routine Description:

    This routine locates the directory entry for the given file or directory.

Arguments:

    Volume - Supplies a pointer to the FAT volume.

    Directory - Supplies a pointer to the directory context for the open file.

    Name - Supplies the name of the file or directory to open.

    NameLength - Supplies the size of the path buffer in bytes, including the
        null terminator.

    Entry - Supplies a pointer where the directory entry will be returned.

    EntryOffset - Supplies an optional pointer where the offset of the returned
        entry will be returned, in bytes, from the beginning of the directory
        file.

Return Value:

    Status code.

--*/

{

    ULONG Cluster;
    ULONG EntriesRead;
    BOOL IsDotEntry;
    ULONGLONG Offset;
    PSTR PotentialName;
    ULONG PotentialNameBufferSize;
    ULONG PotentialNameSize;
    KSTATUS Status;

    Offset = DIRECTORY_CONTENTS_OFFSET;
    PotentialName = NULL;
    if (NameLength <= 1) {
        return STATUS_PATH_NOT_FOUND;
    }

    //
    // Seek to the beginning of the directory.
    //

    Status = FatpDirectorySeek(Directory, Offset);
    if (!KSUCCESS(Status)) {
        goto LookupDirectoryEntryEnd;
    }

    //
    // Allocate a buffer for the name.
    //

    PotentialNameBufferSize = FAT_MAX_LONG_FILE_LENGTH + 1;
    PotentialName = FatAllocatePagedMemory(Volume->Device.DeviceToken,
                                           PotentialNameBufferSize);

    if (PotentialName == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto LookupDirectoryEntryEnd;
    }

    //
    // Loop reading directory entries until a matching one is found or the end
    // is reached.
    //

    while (TRUE) {
        PotentialNameSize = PotentialNameBufferSize;
        Status = FatpReadNextDirectoryEntry(Directory,
                                            NULL,
                                            PotentialName,
                                            &PotentialNameSize,
                                            Entry,
                                            &EntriesRead);

        if (!KSUCCESS(Status)) {
            if (Status == STATUS_END_OF_FILE) {
                Status = STATUS_PATH_NOT_FOUND;
            }

            goto LookupDirectoryEntryEnd;
        }

        Offset += EntriesRead;
        if (PotentialNameSize > NameLength) {
            continue;
        }

        if (RtlAreStringsEqual(Name, PotentialName, NameLength - 1) != FALSE) {

            ASSERT(Offset != 0);

            Offset -= 1;

            //
            // Set the mapping between the file and the directory, except for
            // the . and .. entries. Also, empty files may have a cluster ID of
            // 0, don't save those either.
            //

            IsDotEntry = FALSE;
            if ((Name[0] == '.') &&
                ((Name[1] == '\0') ||
                 ((Name[1] == '.') && (Name[2] == '\0')))) {

                IsDotEntry = TRUE;
            }

            if (IsDotEntry == FALSE) {
                Cluster = (Entry->ClusterHigh << 16) | Entry->ClusterLow;
                if ((Cluster >= FAT_CLUSTER_BEGIN) &&
                    (Cluster < Volume->ClusterBad)) {

                    Status = FatpSetFileMapping(Volume,
                                                Cluster,
                                                Directory->File->SeekTable[0],
                                                Offset);

                    if (!KSUCCESS(Status)) {
                        goto LookupDirectoryEntryEnd;
                    }
                }
            }

            break;
        }
    }

    Status = STATUS_SUCCESS;

LookupDirectoryEntryEnd:
    if (PotentialName != NULL) {
        FatFreePagedMemory(Volume->Device.DeviceToken, PotentialName);
    }

    if (!KSUCCESS(Status)) {
        Offset = 0;
    }

    if (EntryOffset != NULL) {
        *EntryOffset = Offset;
    }

    return Status;
}

KSTATUS
FatpCreateFile (
    PFAT_VOLUME Volume,
    FILE_ID DirectoryFileId,
    PCSTR FileName,
    ULONG FileNameLength,
    PULONGLONG DirectorySize,
    PFILE_PROPERTIES FileProperties
    )

/*++

Routine Description:

    This routine creates a file or directory at the given path. This routine
    fails if the file already exists.

Arguments:

    Volume - Supplies a pointer to the FAT volume.

    DirectoryFileId - Supplies the file ID of the directory to create the file
        in.

    FileName - Supplies the name of the file to create.

    FileNameLength - Supplies the length of the file name buffer, in bytes,
        including the null terminator.

    DirectorySize - Supplies a pointer that receives the updated size of the
        directory.

    FileProperties - Supplies a pointer that on input contains the file
        properties to set on the created file. On output, the new file ID
        will be returned here, and the size and hard link count will be
        initialized.

Return Value:

    Status code.

--*/

{

    UCHAR Attributes;
    FAT_DIRECTORY_ENTRY DirectoryEntry;
    FAT_ENCODED_PROPERTIES EncodedProperties;
    USHORT FatCreationDate;
    USHORT FatCreationTime;
    UCHAR FatCreationTime10ms;
    ULONG FirstCluster;
    KSTATUS Status;

    FirstCluster = Volume->ClusterEnd;

    ASSERT(FileNameLength > 1);
    ASSERT(RtlStringFindCharacter(FileName,
                                  PATH_SEPARATOR,
                                  FileNameLength - 1) == NULL);

    //
    // Figure out what kind of attributes to give the new file.
    //

    Attributes = 0;
    if ((FileProperties->Permissions &
         (FILE_PERMISSION_USER_WRITE | FILE_PERMISSION_GROUP_WRITE |
          FILE_PERMISSION_OTHER_WRITE)) == 0) {

        Attributes |= FAT_READ_ONLY;
    }

    //
    // Symbolic links aren't officially supported by FAT, they can only be
    // created with the additional file properties encoding.
    //

    if ((FatDisableEncodedProperties != FALSE) &&
        (FileProperties->Type == IoObjectSymbolicLink)) {

        Status = STATUS_NOT_SUPPORTED;
        goto CreateFileEnd;
    }

    if (FileProperties->Type == IoObjectRegularDirectory) {
        Attributes |= FAT_SUBDIRECTORY;
    }

    //
    // Initialize most of the directory attributes.
    //

    RtlZeroMemory(&DirectoryEntry, sizeof(FAT_DIRECTORY_ENTRY));
    DirectoryEntry.FileAttributes = Attributes;

    //
    // Write out the file creation time.
    //

    FatpConvertSystemTimeToFatTime(&(FileProperties->StatusChangeTime),
                                   &FatCreationDate,
                                   &FatCreationTime,
                                   &FatCreationTime10ms);

    DirectoryEntry.CreationTime10ms = FatCreationTime10ms;
    DirectoryEntry.CreationTime = FatCreationTime;
    DirectoryEntry.CreationDate = FatCreationDate;
    DirectoryEntry.LastAccessDate = DirectoryEntry.CreationDate;
    DirectoryEntry.LastModifiedDate = DirectoryEntry.CreationDate;
    DirectoryEntry.LastModifiedTime = DirectoryEntry.CreationTime;

    //
    // Allocate a cluster for the new file.
    //

    Status = FatpAllocateCluster(Volume,
                                 Volume->ClusterEnd,
                                 &FirstCluster,
                                 TRUE);

    if (!KSUCCESS(Status)) {
        goto CreateFileEnd;
    }

    //
    // Initialize the directory entry.
    //

    DirectoryEntry.ClusterHigh = (USHORT)((FirstCluster >> 16) & 0xFFFF);
    DirectoryEntry.ClusterLow = (USHORT)(FirstCluster & 0xFFFF);
    if (FatDisableEncodedProperties == FALSE) {
        EncodedProperties.Cluster = FirstCluster;
        EncodedProperties.Owner = FileProperties->UserId;
        EncodedProperties.Group = FileProperties->GroupId;
        if ((EncodedProperties.Owner != FileProperties->UserId) ||
            (EncodedProperties.Group != FileProperties->GroupId)) {

            RtlDebugPrint("FAT: Truncated UID/GID: FILE_PROPERTIES 0x%x "
                          "(ID 0x%I64x UID 0x%x GID 0x%x)\n",
                          FileProperties,
                          FileProperties->FileId,
                          FileProperties->UserId,
                          FileProperties->GroupId);
        }

        EncodedProperties.Permissions = FileProperties->Permissions &
                                        FAT_ENCODED_PROPERTY_PERMISSION_MASK;

        if (FileProperties->Type == IoObjectSymbolicLink) {
            EncodedProperties.Permissions |= FAT_ENCODED_PROPERTY_SYMLINK;
        }

        //
        // Steal the least significant bit of the 10ms creation time for
        // one-second granularity of modification time.
        //

        DirectoryEntry.CreationTime10ms &= ~0x1;
        DirectoryEntry.CreationTime10ms |=
                                    FileProperties->ModifiedTime.Seconds & 0x1;

        FatpWriteEncodedProperties(&DirectoryEntry, &EncodedProperties);
    }

    //
    // Create the directory entry.
    //

    Status = FatpCreateDirectoryEntry(Volume,
                                      DirectoryFileId,
                                      FileName,
                                      FileNameLength,
                                      DirectorySize,
                                      &DirectoryEntry);

    if (!KSUCCESS(Status)) {
        goto CreateFileEnd;
    }

    //
    // If this is a directory, initialize the directory entries.
    //

    if ((Attributes & FAT_SUBDIRECTORY) != 0) {
        Status = FatpInitializeDirectory(Volume,
                                         DirectoryFileId,
                                         &DirectoryEntry);

        if (!KSUCCESS(Status)) {
            goto CreateFileEnd;
        }
    }

    //
    // Fill in the file ID and a couple other properties.
    //

    FileProperties->FileId = FirstCluster;
    FileProperties->HardLinkCount = 1;
    FileProperties->Size = 0;
    FileProperties->BlockSize = Volume->ClusterSize;
    FileProperties->BlockCount = 1;
    Status = STATUS_SUCCESS;

CreateFileEnd:
    if (!KSUCCESS(Status)) {
        if (FirstCluster != Volume->ClusterEnd) {
            FatpFreeClusterChain(Volume, NULL, FirstCluster);
        }
    }

    return Status;
}

KSTATUS
FatpCreateDirectoryEntry (
    PFAT_VOLUME Volume,
    FILE_ID DirectoryFileId,
    PCSTR FileName,
    ULONG FileNameLength,
    PULONGLONG DirectorySize,
    PFAT_DIRECTORY_ENTRY Entry
    )

/*++

Routine Description:

    This routine creates a file or directory entry at the given path. This
    routine fails if the file already exists.

Arguments:

    Volume - Supplies a pointer to the FAT volume.

    DirectoryFileId - Supplies the file ID of the directory to create the file
        in.

    FileName - Supplies the name of the file to create.

    FileNameLength - Supplies the length of the file name buffer, in bytes,
        including the null terminator.

    DirectorySize - Supplies a pointer that receives the updated size of the
        directory.

    Entry - Supplies a pointer to the directory entry information to use for
        the entry (except for the file name which is provided in the previous
        parameter).

Return Value:

    Status code.

--*/

{

    PVOID Directory;
    FAT_DIRECTORY_CONTEXT DirectoryContext;
    BOOL DirectoryContextInitialized;
    FAT_DIRECTORY_ENTRY DirectoryEntry;
    ULONG EntriesRead;
    ULONG EntriesWritten;
    ULONG EntryCount;
    ULONGLONG EntryOffset;
    FAT_DIRECTORY_ENTRY ExistingEntry;
    ULONG FirstCluster;
    PFAT_DIRECTORY_ENTRY NewEntries;
    ULONGLONG Offset;
    ULONGLONG PotentialOffset;
    BOOL SetMapping;
    ULONG SpanCount;
    KSTATUS Status;
    BOOL WriteEndEntry;

    Directory = NULL;
    DirectoryContextInitialized = FALSE;
    *DirectorySize = 0;
    FirstCluster = 0;
    NewEntries = NULL;
    SetMapping = FALSE;

    ASSERT(FileNameLength > 1);
    ASSERT(RtlStringFindCharacter(FileName,
                                  PATH_SEPARATOR,
                                  FileNameLength - 1) == NULL);

    //
    // Open up the directory.
    //

    Status = FatOpenFileId(Volume,
                           DirectoryFileId,
                           IO_ACCESS_READ | IO_ACCESS_WRITE,
                           OPEN_FLAG_DIRECTORY,
                           &Directory);

    if (!KSUCCESS(Status)) {
        goto CreateDirectoryEntryEnd;
    }

    //
    // Initialize the directory context to use for reads and writes.
    //

    FatpInitializeDirectoryContext(&DirectoryContext, Directory);
    DirectoryContextInitialized = TRUE;

    //
    // Fail if the file already exists.
    //

    Status = FatpLookupDirectoryEntry(Volume,
                                      &DirectoryContext,
                                      FileName,
                                      FileNameLength,
                                      &ExistingEntry,
                                      NULL);

    if (KSUCCESS(Status)) {
        Status = STATUS_FILE_EXISTS;
        goto CreateDirectoryEntryEnd;
    }

    //
    // Get the entries that need to be written in.
    //

    Status = FatpCreateDirectoryEntriesForFile(Volume,
                                               FileName,
                                               FileNameLength,
                                               Entry,
                                               &NewEntries,
                                               &EntryCount);

    if (!KSUCCESS(Status)) {
        goto CreateDirectoryEntryEnd;
    }

    ASSERT(EntryCount != 0);

    //
    // Reset to the beginning of the directory file.
    //

    Offset = DIRECTORY_CONTENTS_OFFSET;
    Status = FatpDirectorySeek(&DirectoryContext, Offset);
    if (!KSUCCESS(Status)) {
        goto CreateDirectoryEntryEnd;
    }

    //
    // Look for either enough deleted entries or the ending entry.
    //

    EntryOffset = -1;
    PotentialOffset = -1;
    SpanCount = 0;
    WriteEndEntry = FALSE;
    while (TRUE) {
        Status = FatpReadDirectory(&DirectoryContext,
                                   &DirectoryEntry,
                                   1,
                                   &EntriesRead);

        if (Status == STATUS_END_OF_FILE) {
            WriteEndEntry = TRUE;
            break;

        } else if (!KSUCCESS(Status)) {
            goto CreateDirectoryEntryEnd;
        }

        //
        // If this is the root directory and the end of it was reached, there's
        // no space in the root directory.
        //

        if (EntriesRead == 0) {
            Status = STATUS_VOLUME_FULL;
            goto CreateDirectoryEntryEnd;
        }

        ASSERT(EntriesRead == 1);

        //
        // If the end is found, use it.
        //

        if (DirectoryEntry.DosName[0] == FAT_DIRECTORY_ENTRY_END) {
            EntryOffset = Offset;
            WriteEndEntry = TRUE;
            break;
        }

        //
        // If an erased entry was found, that's also perfect.
        //

        if (DirectoryEntry.DosName[0] == FAT_DIRECTORY_ENTRY_ERASED) {
            if (PotentialOffset == -1) {
                PotentialOffset = Offset;
                SpanCount = 1;

            } else {
                SpanCount += 1;
            }

            if (SpanCount >= EntryCount) {
                EntryOffset = PotentialOffset;
                break;
            }

        //
        // This is a regular entry, so it breaks the span.
        //

        } else {
            PotentialOffset = -1;
            SpanCount = 0;
        }

        Offset += 1;
    }

    //
    // Seek either to the desired entry or to the end. If no entry was found,
    // the file pointer must already be at the end, so there's no need to seek.
    //

    if (EntryOffset != -1) {
        Status = FatpDirectorySeek(&DirectoryContext, EntryOffset);
        if (!KSUCCESS(Status)) {

            ASSERT(Status != STATUS_END_OF_FILE);

            goto CreateDirectoryEntryEnd;
        }

    //
    // If an entry offset is not set, then this better have reached the end of
    // the file.
    //

    } else {

        ASSERT(Status == STATUS_END_OF_FILE);

        EntryOffset = Offset;
    }

    //
    // First create the mapping between the new file and the directory it came
    // from. This is done first because it is easy to roll back.
    //

    FirstCluster = ((ULONG)(Entry->ClusterHigh) << 16) | Entry->ClusterLow;
    Status = FatpSetFileMapping(Volume,
                                FirstCluster,
                                (ULONG)DirectoryFileId,
                                EntryOffset + (EntryCount - 1));

    if (!KSUCCESS(Status)) {
        goto CreateDirectoryEntryEnd;
    }

    SetMapping = TRUE;

    //
    // Write out the new directory entries.
    //

    Status = FatpWriteDirectory(&DirectoryContext,
                                NewEntries,
                                EntryCount,
                                &EntriesWritten);

    if (!KSUCCESS(Status)) {
        goto CreateDirectoryEntryEnd;
    }

    if (EntriesWritten != EntryCount) {
        Status = STATUS_VOLUME_CORRUPT;
        goto CreateDirectoryEntryEnd;
    }

    //
    // If necessary, write out the ending directory entry.
    //

    if (WriteEndEntry != FALSE) {
        RtlZeroMemory(&DirectoryEntry, sizeof(FAT_DIRECTORY_ENTRY));
        Status = FatpWriteDirectory(&DirectoryContext,
                                    &DirectoryEntry,
                                    1,
                                    &EntriesWritten);

        if (!KSUCCESS(Status)) {
            goto CreateDirectoryEntryEnd;
        }

        if (EntriesWritten != 1) {
            Status = STATUS_VOLUME_CORRUPT;
            goto CreateDirectoryEntryEnd;
        }
    }

    //
    // With all the entries written, make sure they are flushed.
    //

    Status = FatpFlushDirectory(&DirectoryContext);
    if (!KSUCCESS(Status)) {
        goto CreateDirectoryEntryEnd;
    }

    *DirectorySize = DirectoryContext.ClusterPosition.FileByteOffset;
    Status = STATUS_SUCCESS;

CreateDirectoryEntryEnd:
    if (!KSUCCESS(Status) && (SetMapping != FALSE)) {
        FatpUnsetFileMapping(Volume, FirstCluster);
    }

    if (DirectoryContextInitialized != FALSE) {

        ASSERT(!KSUCCESS(Status) ||
               ((DirectoryContext.FatFlags & FAT_DIRECTORY_FLAG_DIRTY) == 0));

        FatpDestroyDirectoryContext(&DirectoryContext);
    }

    if (Directory != NULL) {
        FatCloseFile(Directory);
    }

    if (NewEntries != NULL) {
        FatFreePagedMemory(Volume->Device.DeviceToken, NewEntries);
    }

    return Status;
}

KSTATUS
FatpReadNextDirectoryEntry (
    PFAT_DIRECTORY_CONTEXT Directory,
    PIRP Irp,
    PSTR FileName,
    PULONG FileNameLength,
    PFAT_DIRECTORY_ENTRY DirectoryEntry,
    PULONG EntriesRead
    )

/*++

Routine Description:

    This routine reads the next valid directory entry out of the directory.

Arguments:

    Directory - Supplies a pointer to the directory context to use for this
        read.

    Irp - Supplies the optional IRP to use for these device reads and writes.

    FileName - Supplies a pointer where the name of the file will be returned.
        This buffer must be at least FAT_MAX_LONG_FILE_LENGTH + 1 bytes large.

    FileNameLength - Supplies a pointer that on input contains the size of the
        file name buffer. On output, returns the size of the file name in
        bytes including the null terminator.

    DirectoryEntry - Supplies a pointer where the directory entry information
        will be returned.

    EntriesRead - Supplies a pointer where the number of directory entries read
        (valid or not) to get to this valid entry will be returned. The
        official offset of the returned directory entry is this returned value
        minus one.

Return Value:

    Status code.

--*/

{

    ULONG CharacterIndex;
    UCHAR ComputedShortNameChecksum;
    FAT_DIRECTORY_ENTRY Entry;
    ULONG EntryCount;
    BOOL GotCompleteLongName;
    ULONG LocalEntriesRead;
    PFAT_LONG_DIRECTORY_ENTRY LongEntry;
    ULONG NameBufferSize;
    ULONG NameSize;
    LONG NextSequence;
    ULONG RegionIndex;
    UCHAR Sequence;
    UCHAR ShortNameChecksum;
    PUSHORT Source;
    ULONG SourceCount;
    ULONG SourceIndex;
    ULONG SourceSize;
    KSTATUS Status;

    EntryCount = 0;
    GotCompleteLongName = FALSE;
    NameBufferSize = *FileNameLength;
    NextSequence = -1;
    NameSize = 0;
    ShortNameChecksum = 0;
    LongEntry = (PFAT_LONG_DIRECTORY_ENTRY)&Entry;

    //
    // Loop reading directory entries until a valid entry is found.
    //

    while (TRUE) {
        Status = FatpReadDirectory(Directory,
                                   &Entry,
                                   1,
                                   &LocalEntriesRead);

        if (!KSUCCESS(Status)) {
            goto ReadNextDirectoryEntryEnd;
        }

        if (LocalEntriesRead == 0) {
            Status = STATUS_END_OF_FILE;
            goto ReadNextDirectoryEntryEnd;
        }

        //
        // If the read succeeded, then 1 entry should have been read.
        //

        ASSERT(LocalEntriesRead == 1);

        EntryCount += 1;

        //
        // Look to see if it's a long file name.
        //

        if (Entry.FileAttributes == FAT_LONG_FILE_NAME_ATTRIBUTES) {

            //
            // If it's an erased entry, skip it.
            //

            if (LongEntry->SequenceNumber == FAT_DIRECTORY_ENTRY_ERASED) {
                continue;
            }

            //
            // If it's a terminating entry, set everything up. The terminating
            // entry comes first, so there should be more long file name
            // entries on the way.
            //

            if ((LongEntry->SequenceNumber &
                 FAT_LONG_DIRECTORY_ENTRY_END) != 0) {

                Sequence = LongEntry->SequenceNumber &
                           FAT_LONG_DIRECTORY_ENTRY_SEQUENCE_MASK;

                NameSize = Sequence * FAT_CHARACTERS_PER_LONG_NAME_ENTRY;

                ASSERT(NameSize <= FAT_MAX_LONG_FILE_LENGTH);

                if (NameBufferSize < NameSize) {
                    Status = STATUS_BUFFER_TOO_SMALL;
                    goto ReadNextDirectoryEntryEnd;
                }

                ShortNameChecksum = LongEntry->ShortFileNameChecksum;
                NextSequence = Sequence - 1;

            //
            // It's not a terminating entry, it's another in the sequence.
            // Validate it.
            //

            } else {
               Sequence = LongEntry->SequenceNumber &
                          FAT_LONG_DIRECTORY_ENTRY_SEQUENCE_MASK;

                if ((Sequence != NextSequence) ||
                    (LongEntry->ShortFileNameChecksum != ShortNameChecksum)) {

                    NextSequence = -1;
                    continue;
                }

                NextSequence -= 1;
            }

            ASSERT(NextSequence != -1);

            //
            // Add the characters to the destination buffer.
            //

            CharacterIndex = (Sequence - 1) *
                             FAT_CHARACTERS_PER_LONG_NAME_ENTRY;

            for (RegionIndex = 0; RegionIndex < 3; RegionIndex += 1) {
                if (RegionIndex == 0) {
                    Source = LongEntry->Name1;
                    SourceSize = FAT_LONG_DIRECTORY_ENTRY_NAME1_SIZE;

                } else if (RegionIndex == 1) {
                    Source = LongEntry->Name2;
                    SourceSize = FAT_LONG_DIRECTORY_ENTRY_NAME2_SIZE;

                } else {
                    Source = LongEntry->Name3;
                    SourceSize = FAT_LONG_DIRECTORY_ENTRY_NAME3_SIZE;
                }

                for (SourceIndex = 0;
                     SourceIndex < SourceSize;
                     SourceIndex += 1) {

                    FileName[CharacterIndex] =
                                  (CHAR)FAT_READ_INT16(&(Source[SourceIndex]));

                    //
                    // Adjust the size if the file ended early.
                    //

                    if (FileName[CharacterIndex] == '\0') {
                        NameSize = CharacterIndex + 1;
                        break;
                    }

                    CharacterIndex += 1;
                }

                //
                // If the previous loop ended early, cut this outer one short
                // too.
                //

                if (SourceIndex != SourceSize) {
                    break;
                }
            }

        //
        // It's a short 8.3 directory entry.
        //

        } else {

            //
            // If the entry is a volume label or deleted, move on.
            //

            if (((Entry.FileAttributes & FAT_VOLUME_LABEL) != 0) ||
                (Entry.DosName[0] == FAT_DIRECTORY_ENTRY_ERASED)) {

                continue;
            }

            //
            // If it's the last entry, stop.
            //

            if (Entry.DosName[0] == FAT_DIRECTORY_ENTRY_END) {
                NameSize = 0;
                Status = STATUS_END_OF_FILE;
                goto ReadNextDirectoryEntryEnd;
            }

            //
            // If there's a valid long file name that was just read, this
            // should be the short name that corresponds to it. Verify that
            // with the checksum.
            //

            GotCompleteLongName = FALSE;
            if (NextSequence == 0) {
                ComputedShortNameChecksum = FatpChecksumDirectoryEntry(&Entry);
                if (ComputedShortNameChecksum == ShortNameChecksum) {
                    GotCompleteLongName = TRUE;
                }
            }

            //
            // Copy the directory information over.
            //

            RtlCopyMemory(DirectoryEntry,
                          &Entry,
                          sizeof(FAT_DIRECTORY_ENTRY));

            //
            // If the checksum matches, then the filename is already in the
            // buffer. Everything's done.
            //

            if (GotCompleteLongName != FALSE) {
                break;
            }

            //
            // Read the 8.3 name into the file name buffer.
            //

            if (NameBufferSize <
                FAT_FILE_LENGTH + FAT_FILE_EXTENSION_LENGTH + 2) {

                Status = STATUS_BUFFER_TOO_SMALL;
                goto ReadNextDirectoryEntryEnd;
            }

            if (Entry.DosName[0] == FAT_DIRECTORY_ENTRY_E5) {
                Entry.DosName[0] = 0xE5;
            }

            //
            // Skip any spaces 'n' stuff on the end.
            //

            for (SourceCount = FAT_FILE_LENGTH;
                 SourceCount != 0;
                 SourceCount -= 1) {

                if (Entry.DosName[SourceCount - 1] > ' ') {
                    break;
                }
            }

            CharacterIndex = 0;
            for (SourceIndex = 0;
                 SourceIndex < SourceCount;
                 SourceIndex += 1) {

                if (Entry.DosName[SourceIndex] < ' ') {
                    continue;
                }

                FileName[CharacterIndex] = Entry.DosName[SourceIndex];
                if ((Entry.CaseInformation & FAT_CASE_BASENAME_LOWER) != 0) {
                    FileName[CharacterIndex] = RtlConvertCharacterToLowerCase(
                                                     FileName[CharacterIndex]);
                }

                CharacterIndex += 1;
            }

            //
            // If there's an extension, add a dot and the extension. Skip any
            // spaces or junk on the end of the extension.
            //

            for (SourceCount = FAT_FILE_EXTENSION_LENGTH;
                 SourceCount != 0;
                 SourceCount -= 1) {

                if (Entry.DosExtension[SourceCount - 1] > ' ') {
                    break;
                }
            }

            if (Entry.DosExtension[0] != ' ') {
                FileName[CharacterIndex] = '.';
                CharacterIndex += 1;
                for (SourceIndex = 0;
                     SourceIndex < SourceCount;
                     SourceIndex += 1) {

                    if (Entry.DosExtension[SourceIndex] < ' ') {
                        continue;
                    }

                    FileName[CharacterIndex] = Entry.DosExtension[SourceIndex];
                    if ((Entry.CaseInformation &
                         FAT_CASE_EXTENSION_LOWER) != 0) {

                        FileName[CharacterIndex] =
                                      RtlConvertCharacterToLowerCase(
                                                     FileName[CharacterIndex]);
                    }

                    CharacterIndex += 1;
                }
            }

            //
            // Files with zero length names are ignored. Otherwise, something
            // valid was found and should be returned.
            //

            if (CharacterIndex != 0) {
                FileName[CharacterIndex] = '\0';
                CharacterIndex += 1;
                NameSize = CharacterIndex;
                break;
            }
        }
    }

    //
    // Make sure the file name is null terminated.
    //

    ASSERT((NameSize != 0) && (NameSize + 1 < NameBufferSize));

    if (FileName[NameSize - 1] != '\0') {
        FileName[NameSize] = '\0';
        NameSize += 1;
    }

    Status = STATUS_SUCCESS;

ReadNextDirectoryEntryEnd:
    if (!KSUCCESS(Status)) {
        NameSize = 0;
    }

    *FileNameLength = NameSize;
    *EntriesRead = EntryCount;
    return Status;
}

KSTATUS
FatpGetNextCluster (
    PFAT_VOLUME Volume,
    ULONG IoFlags,
    ULONG CurrentCluster,
    PULONG NextCluster
    )

/*++

Routine Description:

    This routine follows the singly linked list of clusters by looking up the
    current entry in the File Allocation Table to determine the index of the
    next cluster.

Arguments:

    Volume - Supplies a pointer to the FAT volume.

    IoFlags - Supplies flags regarding any necessary I/O operations.
        See IO_FLAG_* definitions.

    CurrentCluster - Supplies the cluster to follow.

    NextCluster - Supplies a pointer where the next cluster will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if an invalid cluster was supplied.

    Other error codes on device I/O errors.

--*/

{

    KSTATUS Status;

    //
    // The FAT cache may be larger than the actual FAT. Make sure the cluster
    // is within the bounds of the FAT.
    //

    ASSERT(CurrentCluster < Volume->ClusterCount);

    if (CurrentCluster >= Volume->ClusterCount) {
        Status = STATUS_INVALID_PARAMETER;
        goto GetNextClusterEnd;
    }

    ASSERT(((IoFlags & IO_FLAG_NO_ALLOCATE) == 0) ||
           (FatpFatCacheIsClusterEntryPresent(Volume,
                                              CurrentCluster) != FALSE));

    Status = FatpFatCacheReadClusterEntry(Volume,
                                          FALSE,
                                          CurrentCluster,
                                          NextCluster);

    if (!KSUCCESS(Status)) {
        goto GetNextClusterEnd;
    }

    if (*NextCluster == FAT_CLUSTER_FREE) {
        RtlDebugPrint("FAT: Next cluster of 0 for 0x%x.\n", CurrentCluster);
        *NextCluster = Volume->ClusterEnd;
    }

GetNextClusterEnd:
    return Status;
}

KSTATUS
FatpAllocateCluster (
    PFAT_VOLUME Volume,
    ULONG PreviousCluster,
    PULONG NewCluster,
    BOOL Flush
    )

/*++

Routine Description:

    This routine allocates a free cluster, and chains the cluster so that the
    specified previous cluster points to it.

Arguments:

    Volume - Supplies a pointer to the FAT volume.

    PreviousCluster - Supplies the cluster that should point to the newly
        allocated cluster. Specify FAT32_CLUSTER_END if no previous cluster
        should be updated.

    NewCluster - Supplies a pointer that will receive the new cluster number.

    Flush - Supplies a boolean indicating if the FAT cache should be flushed.
        Supply TRUE here unless multiple clusters are going to be allocated in
        bulk, in which case the caller needs to explicitly flush the FAT
        cache.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if an invalid cluster was supplied.

    STATUS_VOLUME_FULL if no free clusters exist.

    Other error codes on device I/O errors.

--*/

{

    ULONG AllocatedCluster;
    ULONG BlockShift;
    ULONG ClusterCount;
    ULONG ClusterEnd;
    ULONG CurrentCluster;
    PFAT32_INFORMATION_SECTOR Information;
    ULONGLONG InformationBlock;
    PFAT_IO_BUFFER InformationIoBuffer;
    ULONG IoFlags;
    ULONG SearchStart;
    KSTATUS Status;
    ULONG Value;
    PVOID Window;
    PUSHORT Window16;
    PULONG Window32;
    ULONG WindowOffset;
    ULONG WindowSize;

    AllocatedCluster = FAT_CLUSTER_FREE;
    BlockShift = Volume->BlockShift;
    ClusterCount = Volume->ClusterCount;
    InformationIoBuffer = NULL;
    IoFlags = IO_FLAG_FS_DATA | IO_FLAG_FS_METADATA;

    ASSERT((PreviousCluster >= Volume->ClusterBad) ||
           (PreviousCluster < ClusterCount));

    if ((PreviousCluster < Volume->ClusterBad) &&
        (PreviousCluster >= ClusterCount)) {

        return STATUS_INVALID_PARAMETER;
    }

    FatAcquireLock(Volume->Lock);
    if ((Volume->ClusterSearchStart < FAT_CLUSTER_BEGIN) ||
        (Volume->ClusterSearchStart >= ClusterCount)) {

        Volume->ClusterSearchStart = FAT_CLUSTER_BEGIN;
    }

    //
    // Search for a free block. Start just after the last allocated cluster.
    //

    CurrentCluster = Volume->ClusterSearchStart;
    ClusterEnd = ClusterCount;
    SearchStart = CurrentCluster;
    CurrentCluster += 1;
    WindowSize = FAT_WINDOW_INDEX_TO_CLUSTER(Volume, 1);
    WindowOffset = MAX_ULONG;
    while (CurrentCluster != SearchStart) {

        //
        // If this is the end of the FAT, wrap around to the beginning.
        //

        if (CurrentCluster >= ClusterEnd) {
            CurrentCluster = FAT_CLUSTER_BEGIN;
            WindowOffset = MAX_ULONG;
            ClusterEnd = SearchStart;
        }

        //
        // Read the next window if needed.
        //

        if (WindowOffset >= WindowSize) {
            Status = FatpFatCacheGetFatWindow(Volume,
                                              TRUE,
                                              CurrentCluster,
                                              &Window,
                                              &WindowOffset);

            if (!KSUCCESS(Status)) {
                goto AllocateClusterEnd;
            }
        }

        //
        // Scan the whole window.
        //

        if (Volume->Format == Fat12Format) {
            while (CurrentCluster < ClusterEnd) {
                Value = FAT12_READ_CLUSTER(Window, CurrentCluster);
                if (Value == FAT_CLUSTER_FREE) {
                    break;
                }

                CurrentCluster += 1;
            }

        } else if (Volume->Format == Fat16Format) {
            Window16 = Window;
            while ((WindowOffset < WindowSize) &&
                   (CurrentCluster < ClusterEnd) &&
                   (Window16[WindowOffset] != FAT_CLUSTER_FREE)) {

                WindowOffset += 1;
                CurrentCluster += 1;
            }

        } else {
            Window32 = Window;
            while ((WindowOffset < WindowSize) &&
                   (CurrentCluster < ClusterEnd) &&
                   (Window32[WindowOffset] != FAT_CLUSTER_FREE)) {

                WindowOffset += 1;
                CurrentCluster += 1;
            }
        }

        if ((WindowOffset >= WindowSize) || (CurrentCluster >= ClusterEnd)) {
            continue;
        }

        Status = FatpFatCacheWriteClusterEntry(Volume,
                                               CurrentCluster,
                                               Volume->ClusterEnd,
                                               NULL);

        if (!KSUCCESS(Status)) {
            goto AllocateClusterEnd;
        }

        //
        // Mark a cluster as allocated now that it's been written in stone.
        //

        AllocatedCluster = CurrentCluster;
        break;
    }

    //
    // If nothing was found, sadly return.
    //

    if (AllocatedCluster == FAT_CLUSTER_FREE) {
        Status = STATUS_VOLUME_FULL;
        goto AllocateClusterEnd;
    }

    //
    // Update the FS information block saving the new free space and last block
    // allocated.
    //

    if ((FatMaintainFreeClusterCount != FALSE) &&
        (Volume->InformationByteOffset != 0)) {

        InformationIoBuffer = FatAllocateIoBuffer(Volume->Device.DeviceToken,
                                                  Volume->Device.BlockSize);

        if (InformationIoBuffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto AllocateClusterEnd;
        }

        InformationBlock = Volume->InformationByteOffset >> BlockShift;
        Status = FatReadDevice(Volume->Device.DeviceToken,
                               InformationBlock,
                               1,
                               IoFlags,
                               NULL,
                               InformationIoBuffer);

        if (!KSUCCESS(Status)) {
            goto AllocateClusterEnd;
        }

        Information = FatMapIoBuffer(InformationIoBuffer);
        if (Information == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto AllocateClusterEnd;
        }

        Information->LastClusterAllocated = AllocatedCluster;

        ASSERT(Information->FreeClusters != 0);

        if (Information->FreeClusters != 0) {
            Information->FreeClusters -= 1;
        }

        Status = FatWriteDevice(Volume->Device.DeviceToken,
                                InformationBlock,
                                1,
                                IoFlags,
                                NULL,
                                InformationIoBuffer);

        if (!KSUCCESS(Status)) {
            goto AllocateClusterEnd;
        }
    }

    Volume->ClusterSearchStart = AllocatedCluster;

    //
    // Lookup the previous block and update it.
    //

    if ((PreviousCluster != 0) && (PreviousCluster < ClusterCount)) {
        Status = FatpFatCacheWriteClusterEntry(Volume,
                                               PreviousCluster,
                                               AllocatedCluster,
                                               NULL);

        if (!KSUCCESS(Status)) {
            goto AllocateClusterEnd;
        }
    }

    if (Flush != FALSE) {
        Status = FatpFatCacheFlush(Volume, 0);
        if (!KSUCCESS(Status)) {
            goto AllocateClusterEnd;
        }
    }

    Status = STATUS_SUCCESS;

AllocateClusterEnd:
    FatReleaseLock(Volume->Lock);
    if (InformationIoBuffer != NULL) {
        FatFreeIoBuffer(InformationIoBuffer);
    }

    *NewCluster = AllocatedCluster;
    return Status;
}

KSTATUS
FatpFreeClusterChain (
    PFAT_VOLUME Volume,
    PVOID Irp,
    ULONG FirstCluster
    )

/*++

Routine Description:

    This routine marks all clusters in the given list as free.

Arguments:

    Volume - Supplies a pointer to the FAT volume.

    Irp - Supplies an optional pointer to an IRP to use for disk operations.

    FirstCluster - Supplies the first cluster in the list, which will also be
        marked as free.

Return Value:

    Status code.

--*/

{

    ULONG Cluster;
    ULONG ClusterCount;
    PFAT32_INFORMATION_SECTOR Information;
    ULONGLONG InformationBlock;
    PFAT_IO_BUFFER InformationIoBuffer;
    ULONG IoFlags;
    ULONG NextCluster;
    KSTATUS Status;
    ULONG TotalClusters;

    InformationIoBuffer = NULL;
    IoFlags = IO_FLAG_FS_DATA | IO_FLAG_FS_METADATA;
    FatAcquireLock(Volume->Lock);
    TotalClusters = Volume->ClusterCount;
    if ((FirstCluster < FAT_CLUSTER_BEGIN) || (FirstCluster >= TotalClusters)) {
        Status = STATUS_INVALID_PARAMETER;
        goto FreeClusterChainEnd;
    }

    ClusterCount = 0;
    Cluster = FirstCluster;
    while (TRUE) {
        if ((Cluster < FAT_CLUSTER_BEGIN) || (Cluster >= TotalClusters)) {

            //
            // It's not a good sign when the caller is trying to free an
            // invalid cluster. Try to recover by declaring success.
            //

            if (Cluster == FAT_CLUSTER_FREE) {
                RtlDebugPrint("FAT: Freeing cluster 0.\n");

            } else {
                RtlDebugPrint("FAT: Freeing invalid cluster 0x%x, total 0x%x\n",
                              Cluster,
                              TotalClusters);
            }

            Status = STATUS_SUCCESS;
            goto FreeClusterChainEnd;
        }

        //
        // Always allocate from the lowest cluster known to be free.
        //

        if (Cluster < Volume->ClusterSearchStart) {
            Volume->ClusterSearchStart = Cluster;
        }

        Status = FatpFatCacheWriteClusterEntry(Volume,
                                               Cluster,
                                               FAT_CLUSTER_FREE,
                                               &NextCluster);

        if (!KSUCCESS(Status)) {
            goto FreeClusterChainEnd;
        }

        ClusterCount += 1;
        if (NextCluster >= TotalClusters) {
            break;
        }

        Cluster = NextCluster;
    }

    Status = FatpFatCacheFlush(Volume, 0);
    if (!KSUCCESS(Status)) {
        goto FreeClusterChainEnd;
    }

    //
    // Update the FS information block saving the new free space.
    //

    if ((FatMaintainFreeClusterCount != FALSE) &&
        (Volume->InformationByteOffset != 0)) {

        InformationIoBuffer = FatAllocateIoBuffer(Volume->Device.DeviceToken,
                                                  Volume->Device.BlockSize);

        if (InformationIoBuffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto FreeClusterChainEnd;
        }

        InformationBlock = Volume->InformationByteOffset >> Volume->BlockShift;
        Status = FatReadDevice(Volume->Device.DeviceToken,
                               InformationBlock,
                               1,
                               IoFlags,
                               Irp,
                               InformationIoBuffer);

        if (!KSUCCESS(Status)) {
            goto FreeClusterChainEnd;
        }

        Information = FatMapIoBuffer(InformationIoBuffer);
        if (Information == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto FreeClusterChainEnd;
        }

        Information->LastClusterAllocated = Cluster;

        ASSERT(Information->FreeClusters + ClusterCount >
               Information->FreeClusters);

        Information->FreeClusters += ClusterCount;
        Status = FatWriteDevice(Volume->Device.DeviceToken,
                                InformationBlock,
                                1,
                                IoFlags,
                                Irp,
                                InformationIoBuffer);

        if (!KSUCCESS(Status)) {
            goto FreeClusterChainEnd;
        }
    }

FreeClusterChainEnd:
    FatReleaseLock(Volume->Lock);
    if (InformationIoBuffer != NULL) {
        FatFreeIoBuffer(InformationIoBuffer);
    }

    return Status;
}

KSTATUS
FatpIsDirectoryEmpty (
    PFAT_VOLUME Volume,
    FILE_ID DirectoryFileId,
    PBOOL Empty
    )

/*++

Routine Description:

    This routine determines if the given directory is empty or not.

Arguments:

    Volume - Supplies a pointer to the FAT volume.

    DirectoryFileId - Supplies the ID of the directory to query.

    Empty - Supplies a pointer where a boolean will be returned indicating
        whether or not the directory is empty.

Return Value:

    Status code.

--*/

{

    PVOID Directory;
    FAT_DIRECTORY_CONTEXT DirectoryContext;
    BOOL DirectoryContextInitialized;
    BOOL DirectoryEmpty;
    ULONG EntriesRead;
    FAT_DIRECTORY_ENTRY Entry;
    KSTATUS Status;

    Directory = NULL;
    DirectoryContextInitialized = FALSE;
    DirectoryEmpty = TRUE;

    //
    // Open up the directory.
    //

    Status = FatOpenFileId(Volume,
                           DirectoryFileId,
                           IO_ACCESS_READ,
                           OPEN_FLAG_DIRECTORY,
                           &Directory);

    if (!KSUCCESS(Status)) {
        goto IsDirectoryEmptyEnd;
    }

    //
    // Initialize the directory context and seek to the beginning.
    //

    FatpInitializeDirectoryContext(&DirectoryContext, Directory);
    DirectoryContextInitialized = TRUE;
    Status = FatpDirectorySeek(&DirectoryContext, DIRECTORY_CONTENTS_OFFSET);
    if (!KSUCCESS(Status)) {
        goto IsDirectoryEmptyEnd;
    }

    //
    // Loop through reading directory entries.
    //

    while (TRUE) {
        Status = FatpReadDirectory(&DirectoryContext,
                                   &Entry,
                                   1,
                                   &EntriesRead);

        if (Status == STATUS_END_OF_FILE) {
            break;
        }

        if (!KSUCCESS(Status)) {
            goto IsDirectoryEmptyEnd;
        }

        if (EntriesRead == 0) {
            break;
        }

        ASSERT(EntriesRead == 1);

        //
        // Skip this if it has the volume label set. It's either a real
        // volume label or a long entry. If it's a long entry there's sure to
        // be a valid short entry to bump into soon.
        //

        if ((Entry.FileAttributes & FAT_VOLUME_LABEL) != 0) {
            continue;
        }

        if (Entry.DosName[0] == FAT_DIRECTORY_ENTRY_ERASED) {
            continue;
        }

        if (Entry.DosName[0] == FAT_DIRECTORY_ENTRY_END) {
            break;
        }

        //
        // If the entry is dot or dot dot, skip it.
        //

        if (Entry.DosName[0] == '.') {
            if ((Entry.DosName[1] == ' ') ||
                ((Entry.DosName[1] == '.') && (Entry.DosName[2] == ' '))) {

                continue;
            }
        }

        //
        // This doesn't seem to be a bogus entry or the standard . and ..
        // directories, there must really be something in here.
        //

        DirectoryEmpty = FALSE;
        break;
    }

    Status = STATUS_SUCCESS;

IsDirectoryEmptyEnd:
    if (DirectoryContextInitialized != FALSE) {

        ASSERT((DirectoryContext.FatFlags & FAT_DIRECTORY_FLAG_DIRTY) == 0);

        FatpDestroyDirectoryContext(&DirectoryContext);
    }

    if (Directory != NULL) {
        FatCloseFile(Directory);
    }

    *Empty = DirectoryEmpty;
    return Status;
}

CHAR
FatpChecksumDirectoryEntry (
    PFAT_DIRECTORY_ENTRY Entry
    )

/*++

Routine Description:

    This routine returns the checksum of the given fat short directory entry
    based on the file name.

Arguments:

    Entry - Supplies a pointer to the directory entry.

Return Value:

    Returns the checksum of the directory entry.

--*/

{

    ULONG Index;
    UCHAR Sum;

    Sum = 0;
    for (Index = 0; Index < FAT_FILE_LENGTH; Index += 1) {
        Sum = ((Sum & 0x1) << 0x7) + (Sum >> 1) + Entry->DosName[Index];
    }

    for (Index = 0; Index < FAT_FILE_EXTENSION_LENGTH; Index += 1) {
        Sum = ((Sum & 0x1) << 0x7) + (Sum >> 1) + Entry->DosExtension[Index];
    }

    return Sum;
}

KSTATUS
FatpEraseDirectoryEntry (
    PFAT_DIRECTORY_CONTEXT Directory,
    ULONGLONG EntryOffset,
    PBOOL EntryErased
    )

/*++

Routine Description:

    This routine writes over the specified directory entry and any long file
    name entries before it.

Arguments:

    Directory - Supplies a pointer to a context for the directory to be
        modified.

    EntryOffset - Supplies the directory offset of the entry to delete.

    EntryErased - Supplies a pointer that receives a boolean indicating whether
        or not the entry was erased. This can be set to TRUE even if the
        routine returns a failure status.

Return Value:

    Status code.

--*/

{

    UCHAR Checksum;
    ULONG Cluster;
    FAT_DIRECTORY_ENTRY DirectoryEntry;
    ULONG EntriesRead;
    ULONG EntriesWritten;
    BOOL LocalEntryErased;
    KSTATUS Status;

    LocalEntryErased = FALSE;

    //
    // Seek and read in the directory entry.
    //

    Status = FatpDirectorySeek(Directory, EntryOffset);
    if (!KSUCCESS(Status)) {
        goto EraseDirectoryEntryEnd;
    }

    Status = FatpReadDirectory(Directory,
                               &DirectoryEntry,
                               1,
                               &EntriesRead);

    if (!KSUCCESS(Status)) {
        goto EraseDirectoryEntryEnd;
    }

    ASSERT(EntriesRead == 1);

    //
    // Save the checksum for the hunt for long file names.
    //

    Checksum = FatpChecksumDirectoryEntry(&DirectoryEntry);
    Cluster = ((ULONG)(DirectoryEntry.ClusterHigh) << 16) |
              DirectoryEntry.ClusterLow;

    //
    // Write out the erased entry.
    //

    DirectoryEntry.DosName[0] = FAT_DIRECTORY_ENTRY_ERASED;
    DirectoryEntry.FileAttributes = 0;
    DirectoryEntry.ClusterLow = 0;
    DirectoryEntry.ClusterHigh = 0;
    Status = FatpDirectorySeek(Directory, EntryOffset);
    if (!KSUCCESS(Status)) {
        goto EraseDirectoryEntryEnd;
    }

    Status = FatpWriteDirectory(Directory,
                                &DirectoryEntry,
                                1,
                                &EntriesWritten);

    if (!KSUCCESS(Status)) {
        goto EraseDirectoryEntryEnd;
    }

    ASSERT(EntriesWritten == 1);

    //
    // The directory context is now dirty. It has not yet been written to disk.
    //

    ASSERT((Directory->FatFlags & FAT_DIRECTORY_FLAG_DIRTY) != 0);

    Status = FatpPerformLongEntryMaintenance(Directory,
                                             EntryOffset,
                                             Checksum,
                                             (ULONG)-1);

    if ((Directory->FatFlags & FAT_DIRECTORY_FLAG_DIRTY) == 0) {
        LocalEntryErased = TRUE;
    }

    if (!KSUCCESS(Status)) {
        goto EraseDirectoryEntryEnd;
    }

    Status = FatpFlushDirectory(Directory);
    if (!KSUCCESS(Status)) {
        goto EraseDirectoryEntryEnd;
    }

    if ((Directory->FatFlags & FAT_DIRECTORY_FLAG_DIRTY) == 0) {
        LocalEntryErased = TRUE;
    }

    Status = STATUS_SUCCESS;

EraseDirectoryEntryEnd:

    //
    // Unset the mapping if the directory entry was erased.
    //

    if (LocalEntryErased != FALSE) {
        FatpUnsetFileMapping(Directory->File->Volume, Cluster);
    }

    *EntryErased = LocalEntryErased;
    return Status;
}

KSTATUS
FatpFixupDotDot (
    PVOID Volume,
    FILE_ID DirectoryFileId,
    ULONG NewCluster
    )

/*++

Routine Description:

    This routine changes the cluster number for the dot dot entry.

Arguments:

    Volume - Supplies the opaque pointer to the volume.

    DirectoryFileId - Supplies the ID of the directory that needs to be fixed
        up.

    NewCluster - Supplies the new cluster number dot dot should point at.

Return Value:

    Status code.

--*/

{

    FAT_DIRECTORY_CONTEXT DirectoryContext;
    BOOL DirectoryContextInitialized;
    ULONG EntriesRead;
    ULONG EntriesWritten;
    FAT_DIRECTORY_ENTRY Entry;
    PVOID File;
    ULONGLONG Offset;
    KSTATUS Status;

    DirectoryContextInitialized = FALSE;
    File = NULL;
    Status = FatOpenFileId(Volume,
                           DirectoryFileId,
                           IO_ACCESS_READ | IO_ACCESS_WRITE,
                           OPEN_FLAG_DIRECTORY,
                           &File);

    if (!KSUCCESS(Status)) {
        goto FixupDotDotEnd;
    }

    FatpInitializeDirectoryContext(&DirectoryContext, File);
    DirectoryContextInitialized = TRUE;
    Offset = DIRECTORY_CONTENTS_OFFSET;
    Status = FatpDirectorySeek(&DirectoryContext, Offset);
    if (!KSUCCESS(Status)) {
        goto FixupDotDotEnd;
    }

    //
    // Loop reading in directory entries looking for dot dot.
    //

    while (TRUE) {
        Status = FatpReadDirectory(&DirectoryContext,
                                   &Entry,
                                   1,
                                   &EntriesRead);

        if (!KSUCCESS(Status)) {
            goto FixupDotDotEnd;
        }

        if (EntriesRead == 0) {
            Status = STATUS_NOT_FOUND;
            goto FixupDotDotEnd;
        }

        ASSERT(EntriesRead == 1);

        if (((Entry.FileAttributes & FAT_VOLUME_LABEL) != 0) ||
            ((Entry.FileAttributes & FAT_SUBDIRECTORY) == 0)) {

            Offset += 1;
            continue;
        }

        //
        // If this is the dot dot entry, change it.
        //

        if ((Entry.DosName[0] == '.') && (Entry.DosName[1] == '.') &&
            (Entry.DosName[2] == ' ') && (Entry.DosExtension[0] == ' ')) {

            Entry.ClusterHigh = (USHORT)(NewCluster >> 16);
            Entry.ClusterLow = (USHORT)NewCluster;

            //
            // Write the altered cluster out.
            //

            Status = FatpDirectorySeek(&DirectoryContext, Offset);
            if (!KSUCCESS(Status)) {
                goto FixupDotDotEnd;
            }

            Status = FatpWriteDirectory(&DirectoryContext,
                                        &Entry,
                                        1,
                                        &EntriesWritten);

            if (!KSUCCESS(Status)) {
                goto FixupDotDotEnd;
            }

            if (EntriesWritten != 1) {
                Status = STATUS_FILE_CORRUPT;
                goto FixupDotDotEnd;
            }

            //
            // Flush the write if it was successful.
            //

            Status = FatpFlushDirectory(&DirectoryContext);
            if (!KSUCCESS(Status)) {
                goto FixupDotDotEnd;
            }

            break;
        }

        Offset += 1;
    }

    Status = STATUS_SUCCESS;

FixupDotDotEnd:
    if (DirectoryContextInitialized != FALSE) {
        FatpDestroyDirectoryContext(&DirectoryContext);
    }

    if (File != NULL) {
        FatCloseFile(File);
    }

    return Status;
}

KSTATUS
FatpAllocateClusterForEmptyFile (
    PFAT_VOLUME Volume,
    PFAT_DIRECTORY_CONTEXT DirectoryContext,
    ULONG DirectoryFileId,
    PFAT_DIRECTORY_ENTRY Entry,
    ULONGLONG EntryOffset
    )

/*++

Routine Description:

    This routine allocates and writes out a cluster for a given empty file,
    since the starting cluster ID is what uniquely identifies a file.

Arguments:

    Volume - Supplies a pointer to the volume.

    DirectoryContext - Supplies a pointer to the context to be initialized.

    DirectoryFileId - Supplies the file ID of the directory this file resides
        in.

    Entry - Supplies a pointer to the directory entry that needs a cluster.

    EntryOffset - Supplies the offset within the directory where this entry
        resides.

Return Value:

    Status code.

--*/

{

    ULONG Cluster;
    ULONG EntriesWritten;
    UCHAR NewChecksum;
    UCHAR OriginalChecksum;
    ULONG OriginalOffset;
    KSTATUS SeekStatus;
    KSTATUS Status;

    //
    // Save the original position.
    //

    Status = FatpDirectoryTell(DirectoryContext, &OriginalOffset);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Cluster = (Entry->ClusterHigh << 16) | Entry->ClusterLow;

    ASSERT((Cluster < FAT_CLUSTER_BEGIN) || (Cluster > Volume->ClusterBad));

    if (Entry->FileSizeInBytes != 0) {
        RtlDebugPrint("FAT: File size was non-zero but had no cluster.\n");
        Entry->FileSizeInBytes = 0;
    }

    Status = FatpAllocateCluster(Volume,
                                 Volume->ClusterEnd,
                                 &Cluster,
                                 TRUE);

    if (!KSUCCESS(Status)) {
        goto AllocateClusterForEmptyFileEnd;
    }

    OriginalChecksum = FatpChecksumDirectoryEntry(Entry);

    //
    // Now write out the cluster.
    //

    Entry->ClusterHigh = (USHORT)(Cluster << 16);
    Entry->ClusterLow = (USHORT)Cluster;
    Entry->FileSizeInBytes = 0;
    NewChecksum = FatpChecksumDirectoryEntry(Entry);
    Status = FatpDirectorySeek(DirectoryContext, EntryOffset);
    if (!KSUCCESS(Status)) {
        goto AllocateClusterForEmptyFileEnd;
    }

    Status = FatpWriteDirectory(DirectoryContext,
                                Entry,
                                1,
                                &EntriesWritten);

    if (!KSUCCESS(Status)) {
        goto AllocateClusterForEmptyFileEnd;
    }

    if (EntriesWritten != 1) {
        Status = STATUS_FILE_CORRUPT;
        goto AllocateClusterForEmptyFileEnd;
    }

    //
    // Fix up the checksum fields in the long entries, as the short entry
    // changed.
    //

    Status = FatpPerformLongEntryMaintenance(DirectoryContext,
                                             EntryOffset,
                                             OriginalChecksum,
                                             NewChecksum);

    if (!KSUCCESS(Status)) {
        goto AllocateClusterForEmptyFileEnd;
    }

    Status = FatpFlushDirectory(DirectoryContext);
    if (!KSUCCESS(Status)) {
        goto AllocateClusterForEmptyFileEnd;
    }

    Status = FatpSetFileMapping(Volume,
                                Cluster,
                                (ULONG)DirectoryFileId,
                                EntryOffset);

    if (!KSUCCESS(Status)) {
        goto AllocateClusterForEmptyFileEnd;
    }

AllocateClusterForEmptyFileEnd:

    //
    // Restore the original directory context position.
    //

    SeekStatus = FatpDirectorySeek(DirectoryContext, OriginalOffset);
    if (!KSUCCESS(SeekStatus)) {
        if (KSUCCESS(Status)) {
            Status = SeekStatus;
        }
    }

    return Status;
}

KSTATUS
FatpPerformLongEntryMaintenance (
    PFAT_DIRECTORY_CONTEXT Directory,
    ULONGLONG EntryOffset,
    UCHAR Checksum,
    ULONG NewChecksum
    )

/*++

Routine Description:

    This routine operates on long entries preceding the given short directory
    entry (offset), either deleting them or updating their short name checksums.

Arguments:

    Directory - Supplies a pointer to a context for the directory to be
        modified.

    EntryOffset - Supplies the directory offset of the short entry. Long
        entries to modify will be directly behind this entry.

    Checksum - Supplies the checksum of the short directory entry. Long
        entries are expected to contain this checksum.

    NewChecksum - Supplies the new checksum to set in the long entries. If this
        is -1, then the directory entries will be marked erased.

Return Value:

    Status code.

--*/

{

    FAT_DIRECTORY_ENTRY DirectoryEntry;
    ULONG EntriesRead;
    ULONG EntriesWritten;
    PFAT_LONG_DIRECTORY_ENTRY LongEntry;
    UCHAR NextSequence;
    UCHAR Sequence;
    KSTATUS Status;

    //
    // Go backwards and modify any long file name entries associated with
    // this file name. After each directory operation, check to see if the
    // directory context's internal buffer was flushed. If so, consider the
    // first entry erased.
    //

    NextSequence = 1;
    while (EntryOffset > DIRECTORY_CONTENTS_OFFSET) {
        EntryOffset -= 1;
        Status = FatpDirectorySeek(Directory, EntryOffset);
        if (!KSUCCESS(Status)) {
            goto PerformLongEntryMaintenanceEnd;
        }

        Status = FatpReadDirectory(Directory,
                                   &DirectoryEntry,
                                   1,
                                   &EntriesRead);

        if (!KSUCCESS(Status)) {
            goto PerformLongEntryMaintenanceEnd;
        }

        ASSERT(EntriesRead == 1);

        //
        // If this is a long file name entry corresponding to the short name,
        // then back up and modify it.
        //

        LongEntry = (PFAT_LONG_DIRECTORY_ENTRY)&DirectoryEntry;
        Sequence = LongEntry->SequenceNumber;
        if ((DirectoryEntry.FileAttributes == FAT_LONG_FILE_NAME_ATTRIBUTES) &&
            (LongEntry->ShortFileNameChecksum == Checksum) &&
            ((Sequence & FAT_LONG_DIRECTORY_ENTRY_SEQUENCE_MASK) ==
             NextSequence)) {

            Status = FatpDirectorySeek(Directory, EntryOffset);
            if (!KSUCCESS(Status)) {
                goto PerformLongEntryMaintenanceEnd;
            }

            //
            // Delete the long entry if the new checksum is -1.
            //

            if (NewChecksum == (ULONG)-1) {
                DirectoryEntry.DosName[0] = FAT_DIRECTORY_ENTRY_ERASED;
                DirectoryEntry.FileAttributes = 0;
                DirectoryEntry.ClusterLow = 0;
                DirectoryEntry.ClusterHigh = 0;

            //
            // Update the long entry checksum.
            //

            } else {
                LongEntry->ShortFileNameChecksum = NewChecksum;
            }

            Status = FatpWriteDirectory(Directory,
                                        &DirectoryEntry,
                                        1,
                                        &EntriesWritten);

            if (!KSUCCESS(Status)) {
                goto PerformLongEntryMaintenanceEnd;
            }

            ASSERT(EntriesWritten == 1);

            //
            // Stop if that was the last one.
            //

            if ((Sequence & FAT_LONG_DIRECTORY_ENTRY_END) != 0) {
                break;
            }

            NextSequence += 1;

        } else {
            break;
        }
    }

    Status = STATUS_SUCCESS;

PerformLongEntryMaintenanceEnd:
    return Status;
}

VOID
FatpInitializeDirectoryContext (
    PFAT_DIRECTORY_CONTEXT DirectoryContext,
    PFAT_FILE DirectoryFile
    )

/*++

Routine Description:

    This routine initializes the given directory context for the provided FAT
    file.

Arguments:

    DirectoryContext - Supplies a pointer to the context to be initialized.

    DirectoryFile - Supplies a pointer to the FAT file data for the directory.

Return Value:

    None.

--*/

{

    RtlZeroMemory(DirectoryContext, sizeof(FAT_DIRECTORY_CONTEXT));
    DirectoryContext->File = DirectoryFile;
    return;
}

KSTATUS
FatpReadDirectory (
    PFAT_DIRECTORY_CONTEXT Directory,
    PFAT_DIRECTORY_ENTRY Entries,
    ULONG EntryCount,
    PULONG EntriesRead
    )

/*++

Routine Description:

    This routine reads the specified number of directory entries from the given
    directory at its current index.

Arguments:

    Directory - Supplies a pointer to the directory context.

    Entries - Supplies an array of directory entries to read.

    EntryCount - Supplies the number of entries to read into the array.

    EntriesRead - Supplies a pointer that receives the total number of entries
        read.

Return Value:

    Status code.

--*/

{

    UINTN BufferBytesRead;
    UINTN BytesAvailable;
    UINTN BytesToRead;
    ULONG ClusterSize;
    PVOID DeviceToken;
    KSTATUS Status;
    UINTN TotalBytesRead;
    UINTN TotalBytesToRead;

    ASSERT(Entries != NULL);
    ASSERT(EntryCount != 0);

    *EntriesRead = 0;

    //
    // Loop reading directory entries out of the directory context's buffer and
    // more directory buffer regions from disk to fill the buffer as necessary.
    //

    ClusterSize = Directory->File->Volume->ClusterSize;
    TotalBytesRead = 0;
    TotalBytesToRead = EntryCount * sizeof(FAT_DIRECTORY_ENTRY);
    while (TotalBytesRead != TotalBytesToRead) {

        ASSERT(Directory->BufferNextIndex <= ClusterSize);

        if (Directory->ClusterBuffer != NULL) {
            BytesAvailable = ClusterSize - Directory->BufferNextIndex;
            if (BytesAvailable > (TotalBytesToRead - TotalBytesRead)) {
                BytesToRead = TotalBytesToRead - TotalBytesRead;

            } else {
                BytesToRead = BytesAvailable;
            }

            //
            // If there are actually bytes available, read them into the entry
            // buffer.
            //

            if (BytesAvailable != 0) {
                Status = FatCopyIoBufferData(Directory->ClusterBuffer,
                                             (PVOID)Entries + TotalBytesRead,
                                             Directory->BufferNextIndex,
                                             BytesToRead,
                                             FALSE);

                if (!KSUCCESS(Status)) {
                    goto ReadDirectoryEnd;
                }

                Directory->BufferNextIndex += BytesToRead;
                TotalBytesRead += BytesToRead;
            }

            if (TotalBytesRead == TotalBytesToRead) {
                break;
            }

            //
            // Flush the directory's current buffer as more is about to be read.
            //

            Status = FatpFlushDirectory(Directory);
            if (!KSUCCESS(Status)) {
                goto ReadDirectoryEnd;
            }

            FatFreeIoBuffer(Directory->ClusterBuffer);
            Directory->ClusterBuffer = NULL;
        }

        if (Directory->ClusterBuffer == NULL) {
            DeviceToken = Directory->File->Volume->Device.DeviceToken;
            Directory->ClusterBuffer = FatAllocateIoBuffer(DeviceToken,
                                                           ClusterSize);

            if (Directory->ClusterBuffer == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto ReadDirectoryEnd;
            }
        }

        //
        // Read another buffer's worth from the directory file.
        //

        Directory->BufferNextIndex = 0;
        Directory->FatFlags &= ~FAT_DIRECTORY_FLAG_POSITION_AT_END;
        Status = FatReadFile(Directory->File,
                             &(Directory->ClusterPosition),
                             Directory->ClusterBuffer,
                             Directory->File->Volume->ClusterSize,
                             Directory->IoFlags,
                             NULL,
                             &BufferBytesRead);

        //
        // Go to the end on failure, including end of file.
        //

        if (!KSUCCESS(Status)) {
            goto ReadDirectoryEnd;
        }

        if (BufferBytesRead != ClusterSize) {
            Status = STATUS_VOLUME_CORRUPT;
            goto ReadDirectoryEnd;
        }

        Directory->FatFlags |= FAT_DIRECTORY_FLAG_POSITION_AT_END;
    }

    Status = STATUS_SUCCESS;

ReadDirectoryEnd:
    if (!KSUCCESS(Status)) {

        ASSERT((Directory->FatFlags & FAT_DIRECTORY_FLAG_DIRTY) == 0);

        FatFreeIoBuffer(Directory->ClusterBuffer);
        Directory->ClusterBuffer = NULL;
    }

    *EntriesRead = TotalBytesRead / sizeof(FAT_DIRECTORY_ENTRY);
    return Status;
}

KSTATUS
FatpWriteDirectory (
    PFAT_DIRECTORY_CONTEXT Directory,
    PFAT_DIRECTORY_ENTRY Entries,
    ULONG EntryCount,
    PULONG EntriesWritten
    )

/*++

Routine Description:

    This routine writes the given directory entries to the directory at the
    given current offset.

Arguments:

    Directory - Supplies a pointer to the directory context.

    Entries - Supplies an array of directory entries to write.

    EntryCount - Supplies the number of entries to write to the directory.

    EntriesWritten - Supplies a pointer that receives the total number of
        entries written.

Return Value:

    Status code.

--*/

{

    UINTN BufferBytesRead;
    UINTN BytesAvailable;
    UINTN BytesToWrite;
    ULONG ClusterSize;
    PVOID DeviceToken;
    KSTATUS Status;
    UINTN TotalBytesToWrite;
    UINTN TotalBytesWritten;

    ASSERT(Entries != NULL);
    ASSERT(EntryCount != 0);

    *EntriesWritten = 0;

    //
    // Loop writing directory entries to the directory context's buffer and
    // flushing them out to disk as necessary.
    //

    ClusterSize = Directory->File->Volume->ClusterSize;
    TotalBytesToWrite = EntryCount * sizeof(FAT_DIRECTORY_ENTRY);
    TotalBytesWritten = 0;
    while (TotalBytesWritten != TotalBytesToWrite) {
        if (Directory->ClusterBuffer != NULL) {
            BytesAvailable = ClusterSize - Directory->BufferNextIndex;
            if (BytesAvailable > (TotalBytesToWrite - TotalBytesWritten)) {
                BytesToWrite = TotalBytesToWrite - TotalBytesWritten;

            } else {
                BytesToWrite = BytesAvailable;
            }

            ASSERT((Directory->BufferNextIndex + BytesToWrite) <= ClusterSize);

            //
            // If there are bytes to write, write them into the buffer.
            //

            if (BytesToWrite != 0) {
                Status = FatCopyIoBufferData(Directory->ClusterBuffer,
                                             (PVOID)Entries + TotalBytesWritten,
                                             Directory->BufferNextIndex,
                                             BytesToWrite,
                                             TRUE);

                if (!KSUCCESS(Status)) {
                    goto WriteDirectoryEnd;
                }

                Directory->BufferNextIndex += BytesToWrite;
                Directory->FatFlags |= FAT_DIRECTORY_FLAG_DIRTY;
                TotalBytesWritten += BytesToWrite;
            }

            if (TotalBytesWritten == TotalBytesToWrite) {
                break;
            }

            Status = FatpFlushDirectory(Directory);
            if (!KSUCCESS(Status)) {
                goto WriteDirectoryEnd;
            }

            FatFreeIoBuffer(Directory->ClusterBuffer);
            Directory->ClusterBuffer = NULL;
        }

        if (Directory->ClusterBuffer == NULL) {
            DeviceToken = Directory->File->Volume->Device.DeviceToken;
            Directory->ClusterBuffer = FatAllocateIoBuffer(DeviceToken,
                                                           ClusterSize);

            if (Directory->ClusterBuffer == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto WriteDirectoryEnd;
            }
        }

        Directory->BufferNextIndex = 0;
        Directory->FatFlags &= ~FAT_DIRECTORY_FLAG_POSITION_AT_END;
        Status = FatReadFile(Directory->File,
                             &(Directory->ClusterPosition),
                             Directory->ClusterBuffer,
                             ClusterSize,
                             Directory->IoFlags,
                             NULL,
                             &BufferBytesRead);

        if (!KSUCCESS(Status) && (Status != STATUS_END_OF_FILE)) {
            goto WriteDirectoryEnd;
        }

        //
        // If no bytes were read as a result of reaching the end of file,
        // then zero the fat I/O buffer that was allocated above.
        //

        if (BufferBytesRead == 0) {

            ASSERT(Status == STATUS_END_OF_FILE);

            Status = FatZeroIoBuffer(Directory->ClusterBuffer, 0, ClusterSize);
            if (!KSUCCESS(Status)) {
                goto WriteDirectoryEnd;
            }

        } else if (BufferBytesRead != ClusterSize) {
            Status = STATUS_VOLUME_CORRUPT;
            goto WriteDirectoryEnd;

        } else {
            Directory->FatFlags |= FAT_DIRECTORY_FLAG_POSITION_AT_END;
        }
    }

    Status = STATUS_SUCCESS;

WriteDirectoryEnd:
    if (!KSUCCESS(Status)) {
        FatFreeIoBuffer(Directory->ClusterBuffer);
        Directory->ClusterBuffer = NULL;
    }

    *EntriesWritten = TotalBytesWritten / sizeof(FAT_DIRECTORY_ENTRY);
    return Status;
}

KSTATUS
FatpDirectorySeek (
    PFAT_DIRECTORY_CONTEXT Directory,
    ULONG EntryOffset
    )

/*++

Routine Description:

    This routine seeks within the directory to the given entry offset.

Arguments:

    Directory - Supplies a pointer to the directory's context.

    EntryOffset - Supplies the desired entry offset from the beginning of the
        directory.

Return Value:

    Status code.

--*/

{

    UINTN BufferBytesRead;
    ULONG ClusterByteOffset;
    ULONGLONG ClusterEntryOffset;
    ULONG ClusterSize;
    ULONGLONG CurrentEnd;
    ULONGLONG CurrentStart;
    ULONGLONG Destination;
    PVOID DeviceToken;
    ULONGLONG FileByteOffset;
    KSTATUS Status;

    ClusterSize = Directory->File->Volume->ClusterSize;

    ASSERT(EntryOffset >= DIRECTORY_CONTENTS_OFFSET);

    //
    // Determine if the file position can be moved without actually seeking.
    // That is, this is a seek to somewhere else in cluster that's sitting in
    // the buffer.
    //

    if (Directory->ClusterBuffer != NULL) {
        CurrentStart = Directory->ClusterPosition.FileByteOffset;
        if ((Directory->FatFlags & FAT_DIRECTORY_FLAG_POSITION_AT_END) != 0) {
            CurrentStart -= ClusterSize;
        }

        CurrentEnd = CurrentStart + ClusterSize;
        Destination = (EntryOffset - DIRECTORY_CONTENTS_OFFSET) *
                      sizeof(FAT_DIRECTORY_ENTRY);

        if ((Destination >= CurrentStart) && (Destination < CurrentEnd)) {
            Directory->BufferNextIndex = Destination - CurrentStart;
            Status = STATUS_SUCCESS;
            goto DirectorySeekEnd;
        }

        //
        // The seek is moving outside the current buffer. Flush and destroy it.
        //

        Status = FatpFlushDirectory(Directory);
        if (!KSUCCESS(Status)) {
            goto DirectorySeekEnd;
        }

        FatFreeIoBuffer(Directory->ClusterBuffer);
        Directory->ClusterBuffer = NULL;
    }

    //
    // Now seek to the entry offset. If this hits the end of the file, then the
    // next read will fail and any future writes will just extend the file. The
    // buffer position should be at a cluster aligned offset.
    //

    Directory->BufferNextIndex = 0;
    Directory->FatFlags &= ~FAT_DIRECTORY_FLAG_POSITION_AT_END;
    Status = FatFileSeek(Directory->File,
                         NULL,
                         0,
                         SeekCommandFromBeginning,
                         EntryOffset,
                         &(Directory->ClusterPosition));

    if (!KSUCCESS(Status) && (Status != STATUS_END_OF_FILE)) {
        goto DirectorySeekEnd;
    }

    FileByteOffset = Directory->ClusterPosition.FileByteOffset;

    ASSERT((Status != STATUS_END_OF_FILE) ||
           (IS_ALIGNED(FileByteOffset, ClusterSize) != FALSE));

    //
    // Because all the directory buffers are cluster-aligned, this buffer
    // position needs to be aligned down to a cluster boundary. And if it
    // needed alignment, then it needs to be read in and the buffer's next
    // index needs to be set.
    //

    if (IS_ALIGNED(FileByteOffset, ClusterSize) == FALSE) {
        ClusterByteOffset = REMAINDER(FileByteOffset, ClusterSize);
        ClusterEntryOffset = ClusterByteOffset / sizeof(FAT_DIRECTORY_ENTRY);
        Status = FatFileSeek(Directory->File,
                             NULL,
                             0,
                             SeekCommandFromCurrentOffset,
                             (ClusterEntryOffset * -1),
                             &(Directory->ClusterPosition));

        if (!KSUCCESS(Status)) {
            goto DirectorySeekEnd;
        }

        ASSERT(Directory->ClusterBuffer == NULL);

        DeviceToken = Directory->File->Volume->Device.DeviceToken;
        Directory->ClusterBuffer = FatAllocateIoBuffer(DeviceToken,
                                                       ClusterSize);

        if (Directory->ClusterBuffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto DirectorySeekEnd;
        }

        Status = FatReadFile(Directory->File,
                             &(Directory->ClusterPosition),
                             Directory->ClusterBuffer,
                             ClusterSize,
                             Directory->IoFlags,
                             NULL,
                             &BufferBytesRead);

        ASSERT(Status != STATUS_END_OF_FILE);

        if (!KSUCCESS(Status)) {
            goto DirectorySeekEnd;
        }

        if (BufferBytesRead != ClusterSize) {
            Status = STATUS_VOLUME_CORRUPT;
            goto DirectorySeekEnd;
        }

        Directory->BufferNextIndex = ClusterByteOffset;
        Directory->FatFlags |= FAT_DIRECTORY_FLAG_POSITION_AT_END;
    }

DirectorySeekEnd:
    if (!KSUCCESS(Status)) {

        ASSERT((Directory->FatFlags & FAT_DIRECTORY_FLAG_DIRTY) == 0);

        FatFreeIoBuffer(Directory->ClusterBuffer);
        Directory->ClusterBuffer = NULL;
    }

    return Status;
}

KSTATUS
FatpDirectoryTell (
    PFAT_DIRECTORY_CONTEXT Directory,
    PULONG Offset
    )

/*++

Routine Description:

    This routine returns the current offset of the given directory context.

Arguments:

    Directory - Supplies a pointer to the directory context to query.

    Offset - Supplies a pointer where the current offset in entries will be
        returned on success.

Return Value:

    Status code.

--*/

{

    ULONG ClusterSize;
    ULONGLONG CurrentOffset;

    ClusterSize = Directory->File->Volume->ClusterSize;
    CurrentOffset = Directory->ClusterPosition.FileByteOffset;
    if (Directory->ClusterBuffer != NULL) {
        if ((Directory->FatFlags & FAT_DIRECTORY_FLAG_POSITION_AT_END) != 0) {
            CurrentOffset -= ClusterSize;
        }
    }

    CurrentOffset += Directory->BufferNextIndex;

    ASSERT(((ULONG)CurrentOffset % sizeof(FAT_DIRECTORY_ENTRY)) == 0);

    *Offset = ((ULONG)CurrentOffset / sizeof(FAT_DIRECTORY_ENTRY)) +
              DIRECTORY_CONTENTS_OFFSET;

    return STATUS_SUCCESS;
}

KSTATUS
FatpFlushDirectory (
    PFAT_DIRECTORY_CONTEXT Directory
    )

/*++

Routine Description:

    This routine flushes writes accumulated in a directory context.

Arguments:

    Directory - Supplies a pointer to the directory context to flush.

Return Value:

    Status code.

--*/

{

    UINTN BytesWritten;
    ULONGLONG ClusterEntryOffset;
    ULONG ClusterSize;
    KSTATUS Status;

    if ((Directory->FatFlags & FAT_DIRECTORY_FLAG_DIRTY) == 0) {
        Status = STATUS_SUCCESS;
        goto FlushDirectoryEnd;
    }

    ASSERT(Directory->ClusterBuffer != NULL);

    //
    // If the buffer position is at the end of the cluster, then seek backwards
    // to the beginning.
    //

    ClusterSize = Directory->File->Volume->ClusterSize;
    if ((Directory->FatFlags & FAT_DIRECTORY_FLAG_POSITION_AT_END) != 0) {
        ClusterEntryOffset = ClusterSize / sizeof(FAT_DIRECTORY_ENTRY);
        Status = FatFileSeek(Directory->File,
                             NULL,
                             0,
                             SeekCommandFromCurrentOffset,
                             (ClusterEntryOffset * -1),
                             &(Directory->ClusterPosition));

         if (!KSUCCESS(Status)) {
             goto FlushDirectoryEnd;
         }

        Directory->FatFlags &= ~FAT_DIRECTORY_FLAG_POSITION_AT_END;
    }

    Status = FatWriteFile(Directory->File,
                          &(Directory->ClusterPosition),
                          Directory->ClusterBuffer,
                          ClusterSize,
                          Directory->IoFlags,
                          NULL,
                          &BytesWritten);

    if (!KSUCCESS(Status)) {
        goto FlushDirectoryEnd;
    }

    if (BytesWritten != ClusterSize) {
        Status = STATUS_VOLUME_CORRUPT;
        goto FlushDirectoryEnd;
    }

    //
    // This is safe to unset the dirty flag after the write because all
    // directory writes hold an exclusive lock.
    //

    Directory->FatFlags |= FAT_DIRECTORY_FLAG_POSITION_AT_END;
    Directory->FatFlags &= ~FAT_DIRECTORY_FLAG_DIRTY;

FlushDirectoryEnd:
    return Status;
}

VOID
FatpDestroyDirectoryContext (
    PFAT_DIRECTORY_CONTEXT DirectoryContext
    )

/*++

Routine Description:

    This routine destroys any allocations stored in the directory context.

Arguments:

    DirectoryContext - Supplies a pointer to a directory context.

Return Value:

    None.

--*/

{

    if (DirectoryContext->ClusterBuffer != NULL) {
        FatFreeIoBuffer(DirectoryContext->ClusterBuffer);
    }

    return;
}

VOID
FatpConvertSystemTimeToFatTime (
    PSYSTEM_TIME SystemTime,
    PUSHORT Date,
    PUSHORT Time,
    PUCHAR Time10ms
    )

/*++

Routine Description:

    This routine converts a system time value to a FAT date and time.

Arguments:

    SystemTime - Supplies an pointer to the system time to convert. If this
        parameter is NULL, then the current system time will be used.

    Date - Supplies an optional pointer where the FAT date will be stored.

    Time - Supplies an optional pointer where the FAT time will be stored.

    Time10ms - Supplies an optional pointer where the remainder of the time in
        10 millisecond units will be returned.

Return Value:

    None.

--*/

{

    CALENDAR_TIME CalendarTime;
    SYSTEM_TIME CurrentTime;
    USHORT FatDate;
    UCHAR FatFineTime;
    USHORT FatTime;
    KSTATUS Status;

    FatDate = 0;
    FatTime = 0;
    FatFineTime = 0;
    if (SystemTime == NULL) {
        FatGetCurrentSystemTime(&CurrentTime);
        Status = RtlSystemTimeToGmtCalendarTime(&CurrentTime, &CalendarTime);

    } else {
        Status = RtlSystemTimeToGmtCalendarTime(SystemTime, &CalendarTime);
    }

    if (!KSUCCESS(Status)) {
        goto ConvertSystemTimeToFatTimeEnd;
    }

    FatDate = (((CalendarTime.Year - FAT_EPOCH_YEAR) << FAT_DATE_YEAR_SHIFT) &
               FAT_DATE_YEAR_MASK) |
              (((CalendarTime.Month + 1) << FAT_DATE_MONTH_SHIFT) &
               FAT_DATE_MONTH_MASK) |
              (CalendarTime.Day & FAT_DATE_DAY_MASK);

    FatTime = ((CalendarTime.Hour << FAT_TIME_HOUR_SHIFT) &
               FAT_TIME_HOUR_MASK) |
              ((CalendarTime.Minute << FAT_TIME_MINUTE_SHIFT) &
               FAT_TIME_MINUTE_MASK) |
              ((CalendarTime.Second / 2) & FAT_TIME_SECOND_OVER_TWO_MASK);

    FatFineTime = ((CalendarTime.Second & 0x1) * FAT_10MS_PER_SECOND) +
                  CalendarTime.Nanosecond / FAT_NANOSECONDS_PER_10MS;

    //
    // In encoded mode, reserve the least significant bit of the creation time,
    // making the granularity 20ms instead of 10.
    //

    if (FatDisableEncodedProperties == FALSE) {
        FatFineTime &= ~0x1;
    }

ConvertSystemTimeToFatTimeEnd:
    if (Date != NULL) {
        *Date = FatDate;
    }

    if (Time != NULL) {
        *Time = FatTime;
    }

    if (Time10ms != NULL) {
        *Time10ms = FatFineTime;
    }

    return;
}

VOID
FatpConvertFatTimeToSystemTime (
    USHORT Date,
    USHORT Time,
    CHAR Time10ms,
    PSYSTEM_TIME SystemTime
    )

/*++

Routine Description:

    This routine converts a FAT time value into a system time value.

Arguments:

    Date - Supplies the FAT date portion.

    Time - Supplies the FAT time portion.

    Time10ms - Supplies the fine-grained time portion.

    SystemTime - Supplies a pointer where the system time will be returned.

Return Value:

    None.

--*/

{

    CALENDAR_TIME CalendarTime;
    KSTATUS Status;

    RtlZeroMemory(&CalendarTime, sizeof(CALENDAR_TIME));
    CalendarTime.Year = ((Date & FAT_DATE_YEAR_MASK) >> FAT_DATE_YEAR_SHIFT) +
                        FAT_EPOCH_YEAR;

    CalendarTime.Month =
                    ((Date & FAT_DATE_MONTH_MASK) >> FAT_DATE_MONTH_SHIFT) - 1;

    CalendarTime.Day = Date & FAT_DATE_DAY_MASK;
    CalendarTime.Hour = (Time & FAT_TIME_HOUR_MASK) >> FAT_TIME_HOUR_SHIFT;
    CalendarTime.Minute = (Time & FAT_TIME_MINUTE_MASK) >>
                          FAT_TIME_MINUTE_SHIFT;

    CalendarTime.Second = (Time & FAT_TIME_SECOND_OVER_TWO_MASK) * 2;

    //
    // In encoded mode, reserve the least significant bit of the creation time,
    // making the granularity 20ms instead of 10.
    //

    if (FatDisableEncodedProperties == FALSE) {
        Time10ms &= ~0x1;
    }

    CalendarTime.Second += Time10ms / FAT_10MS_PER_SECOND;
    Time10ms %= FAT_10MS_PER_SECOND;
    CalendarTime.Nanosecond = Time10ms * FAT_NANOSECONDS_PER_10MS;
    Status = RtlCalendarTimeToSystemTime(&CalendarTime, SystemTime);
    if (!KSUCCESS(Status)) {
        RtlZeroMemory(SystemTime, sizeof(SYSTEM_TIME));
    }

    return;
}

VOID
FatpReadEncodedProperties (
    PFAT_DIRECTORY_ENTRY Entry,
    PFAT_ENCODED_PROPERTIES Properties
    )

/*++

Routine Description:

    This routine converts the file name portion of a short form directory
    entry into a FAT encoded file properties structure. This format is
    non-standard and doesn't translate to other operating systems.

Arguments:

    Entry - Supplies a pointer to the directory entry to convert.

    Properties - Supplies a pointer where the encoded properties are returned.

Return Value:

    None.

--*/

{

    UINTN ByteIndex;
    PUCHAR Bytes;
    PUCHAR EntryBytes;

    Bytes = (PUCHAR)Properties;
    EntryBytes = (PUCHAR)Entry;

    //
    // The first 8 decoded bytes are constructed from the lower 7 bits of each
    // file byte, and the high bits come from the ninth byte. The first byte is
    // a bit special because if the value comes out to 0xE5, FAT demands that
    // it transform into a different byte (as E5 means erased).
    //

    Bytes[0] = EntryBytes[0] & 0x7F;
    if (EntryBytes[0] == FAT_DIRECTORY_ENTRY_E5) {
        Bytes[0] = 0xE5 & 0x7F;
    }

    for (ByteIndex = 1; ByteIndex < 8; ByteIndex += 1) {
        Bytes[ByteIndex] = (EntryBytes[ByteIndex] & 0x7F) |
                           ((EntryBytes[8] << ByteIndex) & 0x80);
    }

    //
    // The last two bytes come from the lower 7 bits of bytes 10 and 11. This
    // means that the permissions have only 14 bits to work with. One of those
    // bits goes to the high bit for byte 0, so it's really only 13.
    //

    Properties->Permissions = (EntryBytes[9] & 0x7F) |
                              (((USHORT)(EntryBytes[10] & 0x7F)) << 7);

    if ((Properties->Permissions & FAT_ENCODED_PROPERTY_BYTE0_BIT7) != 0) {
        Bytes[0] |= 0x80;
    }

    return;
}

VOID
FatpWriteEncodedProperties (
    PFAT_DIRECTORY_ENTRY Entry,
    PFAT_ENCODED_PROPERTIES Properties
    )

/*++

Routine Description:

    This routine converts encoded properties into a FAT short filename.

Arguments:

    Entry - Supplies a pointer where the encoded properties will be returned.
        Only the file name and extension are modified.

    Properties - Supplies a pointer to the properties to set.

Return Value:

    None.

--*/

{

    UINTN ByteIndex;
    PUCHAR Bytes;
    PUCHAR EntryBytes;

    Bytes = (PUCHAR)Properties;
    EntryBytes = (PUCHAR)Entry;

    //
    // In FAT, 0x80 through 0xFF are valid file name characters. Pack the lower
    // 7 bits of each of the first 8 bytes into these characters, with the
    // high bits of bytes 1 through 8 in the ninth byte. The high bit for byte
    // 0 is off in byte 11.
    //

    EntryBytes[0] = Bytes[0] | 0x80;
    if (EntryBytes[0] == FAT_DIRECTORY_ENTRY_ERASED) {
        EntryBytes[0] = FAT_DIRECTORY_ENTRY_E5;
    }

    if ((Bytes[0] & 0x80) != 0) {
        Properties->Permissions |= FAT_ENCODED_PROPERTY_BYTE0_BIT7;

    } else {
        Properties->Permissions &= ~FAT_ENCODED_PROPERTY_BYTE0_BIT7;
    }

    EntryBytes[8] = 0x80;
    for (ByteIndex = 1; ByteIndex < 8; ByteIndex += 1) {
        EntryBytes[ByteIndex] = Bytes[ByteIndex] | 0x80;
        if ((Bytes[ByteIndex] & 0x80) != 0) {
            EntryBytes[8] |= 0x80 >> ByteIndex;
        }
    }

    EntryBytes[9] = (UCHAR)(Properties->Permissions) | 0x80;
    EntryBytes[10] = (UCHAR)(Properties->Permissions >> 7) | 0x80;
    return;
}

ULONG
FatpGetRandomNumber (
    VOID
    )

/*++

Routine Description:

    This routine returns a random number.

Arguments:

    None.

Return Value:

    Returns a random 32-bit value.

--*/

{

    SYSTEM_TIME CurrentTime;
    ULONG Value;

    FatGetCurrentSystemTime(&CurrentTime);
    Value = (FatRandomSeed ^ (ULONG)(CurrentTime.Seconds) ^
             (ULONG)(CurrentTime.Seconds >> 32) ^ CurrentTime.Nanoseconds) *
            RANDOM_MULTIPLIER +
            RANDOM_INCREMENT;

    FatRandomSeed = Value;
    return Value;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
FatpInitializeDirectory (
    PVOID Volume,
    FILE_ID ParentDirectoryFileId,
    PFAT_DIRECTORY_ENTRY Entry
    )

/*++

Routine Description:

    This routine initializes a new directory file.

Arguments:

    Volume - Supplies the token identifying the volume.

    ParentDirectoryFileId - Supplies the file ID of the parent directory.

    Entry - Supplies a pointer to the directory entry for this directory.

Return Value:

    Status code.

--*/

{

    ULONG Cluster;
    PVOID Directory;
    FAT_DIRECTORY_CONTEXT DirectoryContext;
    BOOL DirectoryContextInitialized;
    FAT_DIRECTORY_ENTRY DirectoryEntries[2];
    ULONG EntriesWritten;
    PFAT_VOLUME FatVolume;
    ULONG NameIndex;
    KSTATUS Status;

    Directory = NULL;
    DirectoryContextInitialized = FALSE;
    FatVolume = Volume;
    Cluster = ((ULONG)(Entry->ClusterHigh) << 16) | Entry->ClusterLow;
    Status = FatOpenFileId(Volume,
                           Cluster,
                           IO_ACCESS_READ | IO_ACCESS_WRITE,
                           OPEN_FLAG_DIRECTORY,
                           &Directory);

    if (!KSUCCESS(Status)) {
        goto InitializeDirectoryEnd;
    }

    //
    // Create the initial directory file with the dot and dot dot
    // directory entries. Start with the dot entry.
    //

    RtlCopyMemory(&(DirectoryEntries[0]), Entry, sizeof(FAT_DIRECTORY_ENTRY));
    DirectoryEntries[0].FileAttributes = FAT_SUBDIRECTORY;
    DirectoryEntries[0].CaseInformation = 0;
    for (NameIndex = 0; NameIndex < FAT_FILE_LENGTH; NameIndex += 1) {
        DirectoryEntries[0].DosName[NameIndex] = ' ';
    }

    for (NameIndex = 0; NameIndex < FAT_FILE_EXTENSION_LENGTH; NameIndex += 1) {
        DirectoryEntries[0].DosExtension[NameIndex] = ' ';
    }

    DirectoryEntries[0].DosName[0] = '.';
    DirectoryEntries[0].FileAttributes = FAT_SUBDIRECTORY;

    //
    // Now create dot dot, which is a modified version of dot. The cluster ID
    // is set to zero if its parent is the root directory.
    //

    RtlCopyMemory(&(DirectoryEntries[1]),
                  &(DirectoryEntries[0]),
                  sizeof(FAT_DIRECTORY_ENTRY));

    DirectoryEntries[1].DosName[1] = '.';
    if (ParentDirectoryFileId == FatVolume->RootDirectoryCluster) {
        DirectoryEntries[1].ClusterHigh = 0;
        DirectoryEntries[1].ClusterLow = 0;

    } else {
        DirectoryEntries[1].ClusterHigh =
                              (USHORT)((ParentDirectoryFileId >> 16) & 0xFFFF);

        DirectoryEntries[1].ClusterLow =
                                      (USHORT)(ParentDirectoryFileId & 0xFFFF);
    }

    //
    // Write all three out at once.
    //

    FatpInitializeDirectoryContext(&DirectoryContext, Directory);
    DirectoryContextInitialized = TRUE;
    Status = FatpDirectorySeek(&DirectoryContext, DIRECTORY_CONTENTS_OFFSET);
    if (!KSUCCESS(Status)) {
        goto InitializeDirectoryEnd;
    }

    Status = FatpWriteDirectory(&DirectoryContext,
                                DirectoryEntries,
                                2,
                                &EntriesWritten);

    if (!KSUCCESS(Status)) {
        goto InitializeDirectoryEnd;
    }

    if (EntriesWritten != 2) {
        Status = STATUS_VOLUME_CORRUPT;
        goto InitializeDirectoryEnd;
    }

    //
    // Zero out the remainder of the cluster.
    //

    FatZeroIoBuffer(DirectoryContext.ClusterBuffer,
                    sizeof(FAT_DIRECTORY_ENTRY) * 2,
                    FatVolume->ClusterSize - (sizeof(FAT_DIRECTORY_ENTRY) * 2));

    //
    // Flush the directory.
    //

    Status = FatpFlushDirectory(&DirectoryContext);
    if (!KSUCCESS(Status)) {
        goto InitializeDirectoryEnd;
    }

    Status = STATUS_SUCCESS;

InitializeDirectoryEnd:
    if (DirectoryContextInitialized != FALSE) {
        FatpDestroyDirectoryContext(&DirectoryContext);
    }

    if (Directory != NULL) {
        FatCloseFile(Directory);
    }

    return Status;
}

KSTATUS
FatpCreateDirectoryEntriesForFile (
    PFAT_VOLUME Volume,
    PCSTR FileName,
    ULONG FileNameLength,
    PFAT_DIRECTORY_ENTRY BaseEntry,
    PFAT_DIRECTORY_ENTRY *NewEntries,
    PULONG EntryCount
    )

/*++

Routine Description:

    This routine creates the set of directory entries needed to represent the
    given file.

Arguments:

    Volume - Supplies a pointer to the FAT volume.

    FileName - Supplies the name of the file to create.

    FileNameLength - Supplies the length of the file name buffer, in bytes,
        including the null terminator.

    BaseEntry - Supplies a pointer to the directory entry that contains most
        of the information for the file (creation time, attributes, etc). The
        name however comes from the supplied parameter.

    NewEntries - Supplies a pointer where the array of entries will be returned.
        The caller is responsible for freeing this memory.

    EntryCount - Supplies a pointer where the number of entries in the array
        will be returned on success.

Return Value:

    Status code.

--*/

{

    USHORT Character;
    ULONG CharacterIndex;
    PUSHORT Destination;
    ULONG DestinationIndex;
    ULONG DestinationSize;
    PFAT_DIRECTORY_ENTRY Entries;
    ULONG EntriesNeeded;
    PFAT_DIRECTORY_ENTRY Entry;
    ULONG EntryIndex;
    BOOL IsCurrentLower;
    BOOL IsLower;
    BOOL IsLowerValid;
    PSTR LastDot;
    ULONG LongEntriesNeeded;
    PFAT_LONG_DIRECTORY_ENTRY LongEntry;
    BOOL LongEntryNeeded;
    PCHAR Name;
    ULONG RegionIndex;
    UCHAR ShortCharacter;
    UCHAR ShortNameChecksum;
    KSTATUS Status;

    Entries = NULL;
    EntriesNeeded = 1;

    //
    // Stop now if this name is too long.
    //

    if (FileNameLength > FAT_MAX_LONG_FILE_LENGTH) {
        Status = STATUS_NAME_TOO_LONG;
        goto CreateDirectoryEntriesForFileEnd;
    }

    if (FileNameLength != 0) {
        FileNameLength -= 1;
    }

    if (FileNameLength == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Determine if the whole thing can fit in an 8.3 entry.
    //

    while (FileName[FileNameLength - 1] == '\0') {
        FileNameLength -= 1;
    }

    LastDot = NULL;
    LongEntryNeeded = TRUE;
    if (((Volume->Flags & FAT_VOLUME_FLAG_COMPATIBILITY_MODE) != 0) ||
        (FatCompatibilityMode != FALSE)) {

        LastDot = RtlStringFindCharacterRight(FileName, '.', FileNameLength);
        if (LastDot != NULL) {
            if ((FileNameLength - ((UINTN)LastDot + 1 - (UINTN)FileName)) <=
                FAT_FILE_EXTENSION_LENGTH) {

                if (((UINTN)LastDot - (UINTN)FileName) <= FAT_FILE_LENGTH) {
                    LongEntryNeeded = FALSE;
                }
            }

        } else if (FileNameLength <= FAT_FILE_LENGTH) {
            LongEntryNeeded = FALSE;
        }
    }

    //
    // If the case varies in the name or the extension (treated separately)
    // then a long entry will be needed anyway to preserve the case.
    //

    if (LongEntryNeeded == FALSE) {
        IsLowerValid = FALSE;
        for (CharacterIndex = 0;
             CharacterIndex < FAT_FILE_LENGTH;
             CharacterIndex += 1) {

            if (FileName[CharacterIndex] == '\0') {
                break;
            }

            if ((LastDot != NULL) && (FileName + CharacterIndex == LastDot)) {
                break;
            }

            if (!RtlIsCharacterAlphabetic(FileName[CharacterIndex])) {
                continue;
            }

            //
            // Record the case of the first character and continue.
            //

            IsCurrentLower = RtlIsCharacterLowerCase(FileName[CharacterIndex]);
            if (IsLowerValid == FALSE) {
                IsLower = IsCurrentLower;
                IsLowerValid = TRUE;

            //
            // If the current character's case does not match that of the first
            // character, then a long entry is needed.
            //

            } else if (IsCurrentLower != IsLower) {
                LongEntryNeeded = TRUE;
                break;
            }
        }

        if ((LongEntryNeeded == FALSE) && (LastDot != NULL)) {
            IsLowerValid = FALSE;
            for (CharacterIndex = 0;
                 CharacterIndex < FAT_FILE_EXTENSION_LENGTH;
                 CharacterIndex += 1) {

                if (((UINTN)LastDot - (UINTN)FileName + CharacterIndex + 1) >=
                    FileNameLength) {

                    break;
                }

                if (LastDot[CharacterIndex + 1] == '\0') {
                    break;
                }

                if (!RtlIsCharacterAlphabetic(LastDot[CharacterIndex + 1])) {
                    continue;
                }

                //
                // Record the case of the first character and continue.
                //

                IsCurrentLower = RtlIsCharacterLowerCase(
                                                  LastDot[CharacterIndex + 1]);

                if (IsLowerValid == FALSE) {
                    IsLower = IsCurrentLower;
                    IsLowerValid = TRUE;

                //
                // If the current character's case does not match that of the
                // first character, then a long entry is needed.
                //

                } else if (IsCurrentLower != IsLower) {
                    LongEntryNeeded = TRUE;
                    break;
                }
            }
        }
    }

    //
    // If there's a long entry needed, then figure out how many of them are
    // required.
    //

    LongEntriesNeeded = 0;
    if (LongEntryNeeded != FALSE) {
        LongEntriesNeeded = FileNameLength / FAT_CHARACTERS_PER_LONG_NAME_ENTRY;
        if ((FileNameLength % FAT_CHARACTERS_PER_LONG_NAME_ENTRY) != 0) {
            LongEntriesNeeded += 1;
        }

        EntriesNeeded += LongEntriesNeeded;
    }

    //
    // Allocate the entries.
    //

    Entries = FatAllocatePagedMemory(
                                  Volume->Device.DeviceToken,
                                  EntriesNeeded * sizeof(FAT_DIRECTORY_ENTRY));

    if (Entries == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateDirectoryEntriesForFileEnd;
    }

    //
    // Create the short entry.
    //

    Entry = &(Entries[EntriesNeeded - 1]);
    RtlCopyMemory(Entry, BaseEntry, sizeof(FAT_DIRECTORY_ENTRY));
    Name = (PCHAR)(Entry->DosName);
    if (LongEntryNeeded != FALSE) {

        //
        // If not using encoded properties, create a random valid short name.
        //

        if (FatDisableEncodedProperties != FALSE) {
            for (CharacterIndex = 0;
                 CharacterIndex < FAT_NAME_SIZE;
                 CharacterIndex += 1) {

                Name[CharacterIndex] = FatpGetRandomNumber() | 0x80;
            }
        }

    //
    // There is no long entry, as it fits in a short entry.
    //

    } else {
        RtlSetMemory(Name, ' ', FAT_NAME_SIZE);

        //
        // Copy the file name in.
        //

        for (CharacterIndex = 0;
             CharacterIndex < FAT_FILE_LENGTH;
             CharacterIndex += 1) {

            if (FileName[CharacterIndex] == '\0') {
                break;
            }

            if ((LastDot != NULL) && (FileName + CharacterIndex == LastDot)) {
                break;
            }

            //
            // Only upper case characters are allows in short file names. It
            // was validated above that the case is at least consistent, but
            // convert any lower case characters to upper case and record the
            // fact.
            //

            ShortCharacter = FileName[CharacterIndex];
            if (RtlIsCharacterLowerCase(ShortCharacter)) {
                ShortCharacter = RtlConvertCharacterToUpperCase(ShortCharacter);
                Entry->CaseInformation |= FAT_CASE_BASENAME_LOWER;
            }

            Entry->DosName[CharacterIndex] = ShortCharacter;
        }

        if (LastDot != NULL) {
            for (CharacterIndex = 0;
                 CharacterIndex < FAT_FILE_EXTENSION_LENGTH;
                 CharacterIndex += 1) {

                if (((UINTN)LastDot - (UINTN)FileName + CharacterIndex + 1) >=
                    FileNameLength) {

                    break;
                }

                if (LastDot[CharacterIndex + 1] == '\0') {
                    break;
                }

                //
                // Only upper case characters are allows in short file name
                // extension. It was validated above that the case is at least
                // consistent, but convert any lower case characters to upper
                // case and record the fact.
                //

                ShortCharacter = LastDot[CharacterIndex + 1];
                if (RtlIsCharacterLowerCase(ShortCharacter)) {
                    ShortCharacter = RtlConvertCharacterToUpperCase(
                                                               ShortCharacter);

                    Entry->CaseInformation |= FAT_CASE_EXTENSION_LOWER;
                }

                Entry->DosExtension[CharacterIndex] = ShortCharacter;
            }
        }

    }

    if ((UCHAR)(Name[0]) == FAT_DIRECTORY_ENTRY_ERASED) {
        Name[0] = FAT_DIRECTORY_ENTRY_E5;
    }

    ShortNameChecksum = FatpChecksumDirectoryEntry(Entry);

    //
    // Fill out all the long entries. The long entries are written in reverse
    // order (ie the last long entry comes first).
    //

    for (EntryIndex = 0; EntryIndex < LongEntriesNeeded; EntryIndex += 1) {
        CharacterIndex = ((LongEntriesNeeded - 1) - EntryIndex) *
                         FAT_CHARACTERS_PER_LONG_NAME_ENTRY;

        ASSERT(CharacterIndex < FileNameLength);

        LongEntry = (PFAT_LONG_DIRECTORY_ENTRY)&(Entries[EntryIndex]);
        LongEntry->SequenceNumber = LongEntriesNeeded - EntryIndex;
        if (EntryIndex == 0) {
            LongEntry->SequenceNumber |= FAT_LONG_DIRECTORY_ENTRY_END;
        }

        LongEntry->FileAttributes = FAT_LONG_FILE_NAME_ATTRIBUTES;
        LongEntry->Type = 0;
        LongEntry->ShortFileNameChecksum = ShortNameChecksum;
        LongEntry->Cluster = 0;

        //
        // Copy the characters in across the three disjoint regions where
        // characters reside in the structure.
        //

        for (RegionIndex = 0; RegionIndex < 3; RegionIndex += 1) {
            if (RegionIndex == 0) {
                Destination = LongEntry->Name1;
                DestinationSize = FAT_LONG_DIRECTORY_ENTRY_NAME1_SIZE;

            } else if (RegionIndex == 1) {
                Destination = LongEntry->Name2;
                DestinationSize = FAT_LONG_DIRECTORY_ENTRY_NAME2_SIZE;

            } else {
                Destination = LongEntry->Name3;
                DestinationSize = FAT_LONG_DIRECTORY_ENTRY_NAME3_SIZE;
            }

            for (DestinationIndex = 0;
                 DestinationIndex < DestinationSize;
                 DestinationIndex += 1) {

                if (CharacterIndex < FileNameLength) {
                    Character = FileName[CharacterIndex];

                } else if (CharacterIndex == FileNameLength) {
                    Character = '\0';

                } else {
                    Character = MAX_USHORT;
                }

                FAT_WRITE_INT16(&(Destination[DestinationIndex]), Character);
                CharacterIndex += 1;
            }
        }
    }

    Status = STATUS_SUCCESS;

CreateDirectoryEntriesForFileEnd:
    if (!KSUCCESS(Status)) {
        if (Entries != NULL) {
            FatFreePagedMemory(Volume->Device.DeviceToken, Entries);
            Entries = NULL;
        }

        EntriesNeeded = 0;
    }

    *NewEntries = Entries;
    *EntryCount = EntriesNeeded;
    return Status;
}

