/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    steps.c

Abstract:

    This module implements the major steps in installing the OS.

Author:

    Evan Green 20-Oct-2015

Environment:

    Setup

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "setup.h"
#include "sconf.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the default factor to multiply system memory by to get the page file
// size.
//

#define SETUP_DEFAULT_PAGE_FILE_NUMERATOR 2
#define SETUP_DEFAULT_PAGE_FILE_DENOMINATOR 1
#define SETUP_MAX_PAGE_FILE_DISK_DIVISOR 10

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
SetupDeterminePageFileSize (
    PSETUP_CONTEXT Context
    );

INT
SetupWriteBootDriversFile (
    PSETUP_CONTEXT Context,
    PVOID DestinationVolume
    );

INT
SetupExecuteCopy (
    PSETUP_CONTEXT Context,
    PVOID DestinationVolume,
    PSETUP_COPY Command
    );

INT
SetupWriteBootSectorFile (
    PSETUP_CONTEXT Context,
    PSETUP_COPY Command,
    BOOL WriteLbaOffset,
    BOOL Clobber
    );

INT
SetupReadEntireFile (
    PSETUP_CONTEXT Context,
    PVOID Source,
    PCSTR SourcePath,
    PVOID *FileContents,
    PULONG FileContentsSize
    );

PSETUP_PARTITION_CONFIGURATION
SetupGetPartition (
    PSETUP_CONTEXT Context,
    ULONG Flag
    );

PVOID
SetupGetSourceVolume (
    PSETUP_CONTEXT Context,
    ULONG SourceVolume
    );

//
// -------------------------------------------------------------------- Globals
//

UCHAR SetupZeroDiskIdentifier[DISK_IDENTIFIER_SIZE] = {0};
UCHAR SetupZeroPartitionIdentifier[PARTITION_IDENTIFIER_SIZE] = {0};

//
// ------------------------------------------------------------------ Functions
//

INT
SetupInstallToDisk (
    PSETUP_CONTEXT Context
    )

/*++

Routine Description:

    This routine installs the OS onto an open disk.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PVOID BootVolume;
    BOOL CompatibilityMode;
    ULONG Index;
    PSETUP_PARTITION_CONFIGURATION Partition;
    ULONG PartitionCount;
    LONGLONG PreviousOffset;
    ULONGLONG PreviousSize;
    INT Result;

    BootVolume = NULL;
    PreviousOffset = Context->CurrentPartitionOffset;
    PreviousSize = Context->CurrentPartitionSize;

    //
    // Write the partition structures.
    //

    Result = SetupFormatDisk(Context);
    if (Result != 0) {
        fprintf(stderr, "Failed to format disk.\n");
        goto InstallToDiskEnd;
    }

    //
    // Loop installing all files to all partitions.
    //

    Partition = Context->Configuration->Disk.Partitions;
    PartitionCount = Context->Configuration->Disk.PartitionCount;
    for (Index = 0; Index < PartitionCount; Index += 1) {
        Result = SetupInstallToPartition(Context, Partition);
        if (Result != 0) {
            goto InstallToDiskEnd;
        }

        Partition += 1;
    }

    //
    // Write the MBR if there is one.
    //

    if (Context->Configuration->Disk.Mbr.Source != NULL) {
        Context->CurrentPartitionOffset = 0;
        Context->CurrentPartitionSize =
                             Context->Configuration->Disk.Partitions[0].Offset;

        Result = SetupWriteBootSectorFile(Context,
                                          &(Context->Configuration->Disk.Mbr),
                                          FALSE,
                                          FALSE);

        if (Result != 0) {
            fprintf(stderr, "Failed to write MBR.\n");
            goto InstallToDiskEnd;
        }
    }

    //
    // Open up the boot volume and write out the new boot entries.
    //

    Partition = SetupGetPartition(Context, SETUP_PARTITION_FLAG_BOOT);
    if (Partition != NULL) {
        CompatibilityMode = FALSE;
        if ((Partition->Flags & SETUP_PARTITION_FLAG_COMPATIBILITY_MODE) != 0) {
            CompatibilityMode = TRUE;
        }

        Context->CurrentPartitionOffset = Partition->Offset / SETUP_BLOCK_SIZE;
        Context->CurrentPartitionSize = Partition->Size / SETUP_BLOCK_SIZE;
        BootVolume = SetupVolumeOpen(Context,
                                     Context->DiskPath,
                                     SetupVolumeFormatIfIncompatible,
                                     CompatibilityMode);

        if (BootVolume == NULL) {
            fprintf(stderr, "Error: Failed to open boot volume.\n");
            Result = -1;
            goto InstallToDiskEnd;
        }

        Result = SetupUpdateBootEntries(Context, BootVolume);
        if (Result != 0) {
            fprintf(stderr, "Error: Failed to update boot entries.\n");
            goto InstallToDiskEnd;
        }
    }

InstallToDiskEnd:
    if (BootVolume != NULL) {
        SetupVolumeClose(Context, BootVolume);
        BootVolume = NULL;
    }

    Context->CurrentPartitionOffset = PreviousOffset;
    Context->CurrentPartitionSize = PreviousSize;
    return Result;
}

INT
SetupInstallToPartition (
    PSETUP_CONTEXT Context,
    PSETUP_PARTITION_CONFIGURATION PartitionConfiguration
    )

/*++

Routine Description:

    This routine perform the required installation steps for a particular
    partition.

Arguments:

    Context - Supplies a pointer to the application context.

    PartitionConfiguration - Supplies a pointer to the partition data to
        install.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    BOOL Clobber;
    BOOL CompatibilityMode;
    PSETUP_DESTINATION Destination;
    PARTITION_DEVICE_INFORMATION PartitionInformation;
    INT Result;
    LONGLONG SeekResult;
    PVOID Volume;
    BOOL WriteLbaOffset;

    Volume = NULL;

    //
    // If no partition was specified, get the system partition. If that fails,
    // just get the first partition.
    //

    if (PartitionConfiguration == NULL) {
        PartitionConfiguration = SetupGetPartition(Context,
                                                   SETUP_PARTITION_FLAG_SYSTEM);

        if (PartitionConfiguration == NULL) {
            if (Context->Configuration->Disk.PartitionCount == 0) {
                fprintf(stderr, "Error: no partitions.\n");
                return ENOENT;
            }

            PartitionConfiguration =
                                 &(Context->Configuration->Disk.Partitions[0]);
        }
    }

    //
    // Open up the partition. If there's already a disk, then set the offset
    // to the partition offset.
    //

    if (Context->Disk != NULL) {
        Destination = Context->DiskPath;
        Context->CurrentPartitionOffset =
                             PartitionConfiguration->Offset / SETUP_BLOCK_SIZE;

        Context->CurrentPartitionSize =
                               PartitionConfiguration->Size / SETUP_BLOCK_SIZE;

        SeekResult = SetupPartitionSeek(Context, Context->Disk, 0);
        if (SeekResult != 0) {
            fprintf(stderr, "Failed to seek to install partition.\n");
            Result = -1;
            goto InstallToPartitionEnd;
        }

    //
    // No device has been opened, so open up the partition directly.
    //

    } else {
        Destination = Context->PartitionPath;
        memset(&PartitionInformation, 0, sizeof(PartitionInformation));
        Context->Disk = SetupPartitionOpen(Context,
                                           Destination,
                                           &PartitionInformation);

        if (Context->Disk == NULL) {
            Result = errno;
            fprintf(stderr, "Failed to open partition: %s.\n", strerror(errno));
            goto InstallToPartitionEnd;
        }

        Context->CurrentPartitionOffset = 0;
        Context->CurrentPartitionSize = PartitionInformation.LastBlock -
                                        PartitionInformation.FirstBlock + 1;

        PartitionConfiguration->Offset = PartitionInformation.FirstBlock *
                                         PartitionInformation.BlockSize;

        PartitionConfiguration->Size = Context->CurrentPartitionSize *
                                       SETUP_BLOCK_SIZE;

        memcpy(&(PartitionConfiguration->PartitionId),
               PartitionInformation.PartitionId,
               PARTITION_IDENTIFIER_SIZE);

        memcpy(&(Context->PartitionContext.DiskIdentifier),
               PartitionInformation.DiskId,
               DISK_IDENTIFIER_SIZE);
    }

    assert(Destination != NULL);

    if (PartitionConfiguration->CopyCommandCount != 0) {
        CompatibilityMode = FALSE;
        if ((PartitionConfiguration->Flags &
             SETUP_PARTITION_FLAG_COMPATIBILITY_MODE) != 0) {

            CompatibilityMode = TRUE;
        }

        Volume = SetupVolumeOpen(Context,
                                 Destination,
                                 SetupVolumeFormatAlways,
                                 CompatibilityMode);

        if (Volume == NULL) {
            Result = -1;
            goto InstallToPartitionEnd;
        }

        Result = SetupInstallFiles(Context, Volume, PartitionConfiguration);
        if (Result != 0) {
            goto InstallToPartitionEnd;
        }
    }

    //
    // Write the VBR if there is one.
    //

    Clobber = TRUE;
    if ((PartitionConfiguration->Flags & SETUP_PARTITION_FLAG_MERGE_VBR) != 0) {
        Clobber = FALSE;
    }

    if (PartitionConfiguration->Vbr.Source != NULL) {
        WriteLbaOffset = FALSE;
        if ((PartitionConfiguration->Flags &
             SETUP_PARTITION_FLAG_WRITE_VBR_LBA) != 0) {

            WriteLbaOffset = TRUE;
        }

        Result = SetupWriteBootSectorFile(Context,
                                          &(PartitionConfiguration->Vbr),
                                          WriteLbaOffset,
                                          Clobber);

        if (Result != 0) {
            fprintf(stderr, "Failed to write VBR.\n");
            goto InstallToPartitionEnd;
        }
    }

    Result = 0;

InstallToPartitionEnd:
    if (Volume != NULL) {
        SetupVolumeClose(Context, Volume);
    }

    //
    // Only close the partition if this routine opened it.
    //

    if ((Context->Disk != NULL) && (Destination != Context->DiskPath)) {
        SetupPartitionClose(Context, Context->Disk);
        Context->Disk = NULL;
    }

    return Result;
}

INT
SetupInstallToDirectory (
    PSETUP_CONTEXT Context
    )

/*++

Routine Description:

    This routine installs the OS onto a directory, copying only system
    partition files.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT Result;
    PSETUP_PARTITION_CONFIGURATION SystemPartition;
    PVOID Volume;

    SystemPartition = SetupGetPartition(Context, SETUP_PARTITION_FLAG_SYSTEM);

    assert(SystemPartition != NULL);

    Volume = SetupVolumeOpen(Context,
                             Context->DirectoryPath,
                             SetupVolumeFormatNever,
                             FALSE);

    if (Volume == NULL) {
        return -1;
    }

    Result = SetupInstallFiles(Context, Volume, SystemPartition);
    SetupVolumeClose(Context, Volume);
    return Result;
}

INT
SetupInstallFiles (
    PSETUP_CONTEXT Context,
    PVOID DestinationVolume,
    PSETUP_PARTITION_CONFIGURATION Partition
    )

/*++

Routine Description:

    This routine installs to the given volume.

Arguments:

    Context - Supplies a pointer to the applicaton context.

    DestinationVolume - Supplies a pointer to the open destination volume
        handle.

    Partition - Supplies a pointer to the partition configuration, which
        contains the files to copy.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSETUP_COPY CopyCommand;
    ULONG CopyCommandCount;
    ULONG Index;
    PVOID PageFile;
    ULONGLONG PageFileSize;
    INT Result;

    PageFile = NULL;
    CopyCommand = Partition->CopyCommands;
    CopyCommandCount = Partition->CopyCommandCount;
    for (Index = 0; Index < CopyCommandCount; Index += 1) {
        Result = SetupExecuteCopy(Context, DestinationVolume, CopyCommand);
        if (Result != 0) {
            goto InstallFilesEnd;
        }

        CopyCommand += 1;
    }

    if ((Partition->Flags & SETUP_PARTITION_FLAG_SYSTEM) != 0) {
        Result = SetupWriteBootDriversFile(Context, DestinationVolume);
        if (Result != 0) {
            fprintf(stderr, "Failed to write boot drivers file.\n");
            goto InstallFilesEnd;
        }

        //
        // Compute the page file size if it has not already been specified.
        //

        if (Context->PageFileSize == -1ULL) {
            SetupDeterminePageFileSize(Context);
        }

        //
        // Create a page file if needed.
        //

        if (Context->PageFileSize != 0) {
            PageFileSize = Context->PageFileSize * _1MB;

            //
            // Watch out for file system limitations on max file size.
            // TODO: Max file size is file system specific, not hardcoded.
            //

            if (PageFileSize > MAX_ULONG) {
                PageFileSize = MAX_ULONG;
            }

            if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
                printf("Creating %lldMB page file...", PageFileSize / _1MB);
                fflush(stdout);
            }

            PageFile = SetupFileOpen(DestinationVolume,
                                     SETUP_PAGE_FILE_PATH,
                                     O_RDWR | O_CREAT,
                                     0);

            if (PageFile == NULL) {
                fprintf(stderr, "Warning: Failed to create page file.\n");
                goto InstallFilesEnd;
            }

            Result = SetupFileFileTruncate(PageFile, PageFileSize);
            SetupFileClose(PageFile);
            PageFile = NULL;
            if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
                printf("Done.\n");
            }

            if (Result != 0) {
                fprintf(stderr, "Warning: Failed to set page file size.\n");
                Result = 0;
            }
        }
    }

InstallFilesEnd:
    if (PageFile != NULL) {
        SetupFileClose(PageFile);
    }

    return Result;
}

INT
SetupUpdateBootVolume (
    PSETUP_CONTEXT Context,
    PVOID BootVolume
    )

/*++

Routine Description:

    This routine updates the boot volume, copying the boot files and updating
    the boot entries.

Arguments:

    Context - Supplies a pointer to the applicaton context.

    BootVolume - Supplies a pointer to the open boot volume handle.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSETUP_PARTITION_CONFIGURATION BootPartition;
    INT Status;

    BootPartition = SetupGetPartition(Context, SETUP_PARTITION_FLAG_BOOT);
    if ((BootPartition != NULL) &&
        ((BootPartition->Flags & SETUP_PARTITION_FLAG_SYSTEM) == 0)) {

        Status = SetupInstallFiles(Context, BootVolume, BootPartition);
        if (Status != 0) {
            fprintf(stderr, "Error: Failed to install boot volume files.\n");
            return Status;
        }
    }

    Status = SetupUpdateBootEntries(Context, BootVolume);
    if (Status != 0) {
        return Status;
    }

    return 0;
}

INT
SetupUpdateBootEntries (
    PSETUP_CONTEXT Context,
    PVOID BootVolume
    )

/*++

Routine Description:

    This routine writes out the new boot entries for the installed image.

Arguments:

    Context - Supplies a pointer to the applicaton context.

    BootVolume - Supplies a pointer to the open boot volume handle.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    BOOT_CONFIGURATION_CONTEXT BootConfiguration;
    BOOL BootConfigurationInitialized;
    PCSTR BootDataPath;
    PBOOT_ENTRY BootEntries;
    PBOOT_ENTRY BootEntry;
    ULONG BootEntryCount;
    PVOID Buffer;
    ssize_t BytesComplete;
    int CompareResult;
    PVOID Destination;
    ULONG EntryIndex;
    ULONGLONG FileSize;
    UINTN Index;
    PBOOT_ENTRY NewBootEntry;
    PVOID NewBuffer;
    size_t NewSize;
    mode_t Permissions;
    INT Result;
    KSTATUS Status;
    PSETUP_PARTITION_CONFIGURATION SystemPartition;

    memset(&BootConfiguration, 0, sizeof(BOOT_CONFIGURATION_CONTEXT));
    BootConfigurationInitialized = FALSE;
    Destination = NULL;
    NewBootEntry = NULL;

    //
    // Make sure the appropriate directories exist.
    //

    Permissions = S_IRUSR | S_IWUSR | S_IXUSR;

    //
    // The install partition information had better be valid.
    //

    SystemPartition = SetupGetPartition(Context, SETUP_PARTITION_FLAG_SYSTEM);
    if (SystemPartition == NULL) {
        Result = EINVAL;
        goto UpdateBootVolumeEnd;
    }

    //
    // Initialize the boot configuration library support.
    //

    BootConfiguration.AllocateFunction = (PBOOT_CONFIGURATION_ALLOCATE)malloc;
    BootConfiguration.FreeFunction = (PBOOT_CONFIGURATION_FREE)free;
    Status = BcInitializeContext(&BootConfiguration);
    if (!KSUCCESS(Status)) {
        fprintf(stderr, "BcInitializeContext Error: %d\n", Status);
        Result = -1;
        goto UpdateBootVolumeEnd;
    }

    BootConfigurationInitialized = TRUE;

    //
    // Attempt to open up the boot configuration data.
    //

    BootDataPath = Context->Configuration->BootDataPath;
    if (BootDataPath == NULL) {
        BootDataPath = BOOT_CONFIGURATION_ABSOLUTE_PATH;
    }

    Destination = SetupFileOpen(BootVolume,
                                BootDataPath,
                                O_RDONLY,
                                0);

    if (Destination != NULL) {
        if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
            printf("Reading existing boot configuration.\n");
        }

        //
        // The file exists. Read it in.
        //

        Result = SetupFileFileStat(Destination, &FileSize, NULL, NULL);
        if (Result != 0) {
            goto UpdateBootVolumeEnd;
        }

        Status = STATUS_NOT_FOUND;
        if (FileSize != 0) {
            Buffer = malloc(FileSize);
            if (Buffer == NULL) {
                goto UpdateBootVolumeEnd;
            }

            BytesComplete = SetupFileRead(Destination, Buffer, FileSize);
            if (BytesComplete != FileSize) {
                fprintf(stderr, "Failed to read boot configuration file.\n");
                goto UpdateBootVolumeEnd;
            }

            BootConfiguration.FileData = Buffer;
            BootConfiguration.FileDataSize = FileSize;
            Buffer = NULL;

            //
            // Read in and parse the boot configuration data. If it is
            // invalid, create a brand new default configuration.
            //

            Status = BcReadBootConfigurationFile(&BootConfiguration);
            if (!KSUCCESS(Status)) {
                fprintf(stderr,
                        "Failed to read boot configuration data: %d.\n",
                        Status);
            }
        }

        //
        // If the file size is zero or could not be read, create a default
        // configuration.
        //

        if (!KSUCCESS(Status)) {
            Status = BcCreateDefaultBootConfiguration(
                                      &BootConfiguration,
                                      Context->PartitionContext.DiskIdentifier,
                                      SystemPartition->PartitionId);

            if (!KSUCCESS(Status)) {
                fprintf(stderr,
                        "Failed to create default boot configuration: %d\n",
                        Status);

                Result = -1;
                goto UpdateBootVolumeEnd;
            }
        }

        SetupFileClose(Destination);
        Destination = NULL;

    //
    // There is no boot configuration data. Create a new one.
    //

    } else {
        if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
            printf("Creating initial boot configuration.\n");
        }

        Status = BcCreateDefaultBootConfiguration(
                                      &BootConfiguration,
                                      Context->PartitionContext.DiskIdentifier,
                                      SystemPartition->PartitionId);

        if (!KSUCCESS(Status)) {
            fprintf(stderr,
                    "BcCreateDefaultBootConfiguration Error: %x\n",
                    Status);

            Result = -1;
            goto UpdateBootVolumeEnd;
        }
    }

    BootEntries = Context->Configuration->BootEntries;
    BootEntryCount = Context->Configuration->BootEntryCount;
    for (EntryIndex = 0; EntryIndex < BootEntryCount; EntryIndex += 1) {
        NewBootEntry = BcCopyBootEntry(&BootConfiguration,
                                       &(BootEntries[EntryIndex]));

        if (NewBootEntry == NULL) {
            Result = ENOMEM;
            goto UpdateBootVolumeEnd;
        }

        //
        // Mark new boot entries so that the loop below doesn't replace them,
        // even if several of them point to the same partition ID.
        //

        NewBootEntry->Id = -1;
        if ((Context->Flags & SETUP_FLAG_INSTALL_DEBUG) != 0) {
            NewBootEntry->Flags |= BOOT_ENTRY_FLAG_DEBUG;
            if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
                printf("Enabled debug mode.\n");
            }
        }

        if ((Context->Flags & SETUP_FLAG_INSTALL_BOOT_DEBUG) != 0) {
            NewBootEntry->Flags |= BOOT_ENTRY_FLAG_BOOT_DEBUG;
            if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
                printf("Enabled boot debug mode.\n");
            }
        }

        //
        // If the disk ID or partition ID are blank, fill them in with the
        // system disk and partition IDs.
        //

        assert(BOOT_DISK_ID_SIZE == DISK_IDENTIFIER_SIZE);

        CompareResult = memcmp(NewBootEntry->DiskId,
                               SetupZeroDiskIdentifier,
                               DISK_IDENTIFIER_SIZE);

        if (CompareResult == 0) {
            memcpy(NewBootEntry->DiskId,
                   Context->PartitionContext.DiskIdentifier,
                   DISK_IDENTIFIER_SIZE);
        }

        assert(BOOT_PARTITION_ID_SIZE == PARTITION_IDENTIFIER_SIZE);

        CompareResult = memcmp(NewBootEntry->PartitionId,
                               SetupZeroPartitionIdentifier,
                               PARTITION_IDENTIFIER_SIZE);

        if (CompareResult == 0) {
            memcpy(NewBootEntry->PartitionId,
                   SystemPartition->PartitionId,
                   PARTITION_IDENTIFIER_SIZE);
        }

        //
        // Look for a boot entry with this partition ID to replace. Skip ones
        // that are already new.
        //

        for (Index = 0;
             Index < BootConfiguration.BootEntryCount;
             Index += 1) {

            BootEntry = BootConfiguration.BootEntries[Index];
            if (BootEntry->Id == (ULONG)-1) {
                continue;
            }

            CompareResult = memcmp(BootEntry->PartitionId,
                                   NewBootEntry->PartitionId,
                                   BOOT_PARTITION_ID_SIZE);

            if (CompareResult == 0) {
                if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
                    printf("Replacing boot entry %d: %s.\n",
                           BootEntry->Id,
                           BootEntry->Name);
                }

                BootConfiguration.BootEntries[Index] = NewBootEntry;
                BcDestroyBootEntry(&BootConfiguration, BootEntry);
                break;
            }
        }

        //
        // If there was no previous entry pointing at this partition,
        // add it to the end of the list.
        //

        if (Index == BootConfiguration.BootEntryCount) {
            NewSize = (BootConfiguration.BootEntryCount + 1) *
                      sizeof(PBOOT_ENTRY);

            NewBuffer = realloc(BootConfiguration.BootEntries, NewSize);
            if (NewBuffer == NULL) {
                Result = ENOMEM;
                goto UpdateBootVolumeEnd;
            }

            BootConfiguration.BootEntries = NewBuffer;
            BootConfiguration.BootEntries[BootConfiguration.BootEntryCount] =
                                                                  NewBootEntry;

            BootConfiguration.BootEntryCount += 1;
        }

        if (EntryIndex == 0) {
            BootConfiguration.GlobalConfiguration.DefaultBootEntry =
                                                                  NewBootEntry;
        }

        NewBootEntry = NULL;
    }

    //
    // Serialize the boot configuration data.
    //

    Status = BcWriteBootConfigurationFile(&BootConfiguration);
    if (!KSUCCESS(Status)) {
        fprintf(stderr,
                "Error: Failed to serialize boot configuration data: %d.\n",
                Status);

        Result = -1;
        goto UpdateBootVolumeEnd;
    }

    if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
        printf("Writing boot configuration data.\n");
    }

    //
    // Open and write the data.
    //

    SetupCreateDirectories(Context, BootVolume, BootDataPath);
    Destination = SetupFileOpen(BootVolume,
                                BootDataPath,
                                O_RDWR | O_CREAT | O_TRUNC,
                                Permissions);

    if (Destination == NULL) {
        fprintf(stderr,
                "Error: Failed to open %s for writing.\n",
                BOOT_CONFIGURATION_ABSOLUTE_PATH);

        Result = errno;
        if (Result == 0) {
            Result = -1;
        }

        goto UpdateBootVolumeEnd;
    }

    BytesComplete = SetupFileWrite(Destination,
                                   BootConfiguration.FileData,
                                   BootConfiguration.FileDataSize);

    if (BytesComplete != BootConfiguration.FileDataSize) {
        fprintf(stderr,
                "Error: Failed to write boot configuration data.\n");

        Result = -1;
        goto UpdateBootVolumeEnd;
    }

    SetupFileClose(Destination);
    Destination = NULL;

UpdateBootVolumeEnd:
    if (BootConfigurationInitialized != FALSE) {
        if (NewBootEntry != NULL) {
            BcDestroyBootEntry(&BootConfiguration, NewBootEntry);
        }

        BcDestroyContext(&BootConfiguration);
    }

    if (Destination != NULL) {
        SetupFileClose(Destination);
    }

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
SetupDeterminePageFileSize (
    PSETUP_CONTEXT Context
    )

/*++

Routine Description:

    This routine determines the size of the page file to create.

Arguments:

    Context - Supplies a pointer to the applicaton context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    ULONGLONG InstallPartitionSize;
    ULONGLONG PageFileSize;
    INT Result;
    ULONGLONG SystemMemory;
    PSETUP_PARTITION_CONFIGURATION SystemPartition;

    assert(Context->PageFileSize == -1ULL);

    //
    // On failure, don't make a page file.
    //

    Context->PageFileSize = 0;
    Result = SetupOsGetSystemMemorySize(&SystemMemory);
    if (Result != 0) {
        return Result;
    }

    PageFileSize = (SystemMemory * SETUP_DEFAULT_PAGE_FILE_NUMERATOR) /
                   SETUP_DEFAULT_PAGE_FILE_DENOMINATOR;

    if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
        printf("System memory %lldMB, Page File size %lldMB.\n",
               SystemMemory,
               PageFileSize);
    }

    SystemPartition = SetupGetPartition(Context, SETUP_PARTITION_FLAG_SYSTEM);
    if (SystemPartition == NULL) {
        return 0;
    }

    InstallPartitionSize = SystemPartition->Size;
    InstallPartitionSize /= _1MB;
    if ((InstallPartitionSize != 0) &&
        (PageFileSize >
         (InstallPartitionSize / SETUP_MAX_PAGE_FILE_DISK_DIVISOR))) {

        PageFileSize = InstallPartitionSize / SETUP_MAX_PAGE_FILE_DISK_DIVISOR;
        if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
            printf("Clipping page file to %lldMB, as install partition is "
                   "only %lldMB.\n",
                   PageFileSize,
                   InstallPartitionSize);
        }
    }

    Context->PageFileSize = PageFileSize;
    return 0;
}

INT
SetupWriteBootDriversFile (
    PSETUP_CONTEXT Context,
    PVOID DestinationVolume
    )

/*++

Routine Description:

    This routine writes the boot drivers file out to the system volume.

Arguments:

    Context - Supplies a pointer to the applicaton context.

    DestinationVolume - Supplies a pointer to the open destination volume
        handle.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSETUP_CONFIGURATION Configuration;
    PSTR Contents;
    UINTN ContentsSize;
    INT Status;

    Contents = NULL;
    Configuration = Context->Configuration;
    if (Configuration->BootDriversPath == NULL) {
        return 0;
    }

    Status = SetupConvertStringArrayToLines(Configuration->BootDrivers,
                                            &Contents,
                                            &ContentsSize);

    if (Status != 0) {
        return Status;
    }

    assert(ContentsSize != 0);

    Status = SetupCreateAndWriteFile(Context,
                                     DestinationVolume,
                                     Configuration->BootDriversPath,
                                     Contents,
                                     ContentsSize - 1);

    free(Contents);
    return Status;
}

INT
SetupExecuteCopy (
    PSETUP_CONTEXT Context,
    PVOID DestinationVolume,
    PSETUP_COPY Command
    )

/*++

Routine Description:

    This routine executes a copy command.

Arguments:

    Context - Supplies a pointer to the applicaton context.

    DestinationVolume - Supplies a pointer to the open destination volume
        handle.

    Command - Supplies the copy command, which may direct this routine to
        copy a single file, recursive directory, or a list of files and/or
        directories.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR AppendedDestination;
    PSTR AppendedSource;
    PCSTR File;
    UINTN Index;
    PVOID Source;
    INT Status;

    //
    // Do nothing if the source is empty.
    //

    if ((Command->Source == NULL) || (Command->Source[0] == '\0')) {
        return 0;
    }

    Source = SetupGetSourceVolume(Context, Command->SourceVolume);
    if (Source == NULL) {
        return EINVAL;
    }

    if (Command->Files == NULL) {
        Status = SetupCopyFile(Context,
                               DestinationVolume,
                               Source,
                               Command->Destination,
                               Command->Source,
                               Command->Flags);

        return Status;
    }

    Index = 0;
    while (Command->Files[Index] != NULL) {
        File = Command->Files[Index];
        Index += 1;
        AppendedDestination = SetupAppendPaths(Command->Destination, File);
        if (AppendedDestination == NULL) {
            return ENOMEM;
        }

        AppendedSource = SetupAppendPaths(Command->Source, File);
        if (AppendedSource == NULL) {
            free(AppendedDestination);
            return ENOMEM;
        }

        Status = SetupCopyFile(Context,
                               DestinationVolume,
                               Source,
                               AppendedDestination,
                               AppendedSource,
                               Command->Flags);

        if (Status != 0) {
            fprintf(stderr,
                    "Failed to copy %s -> %s: %s.\n",
                    AppendedSource,
                    AppendedDestination,
                    strerror(Status));
        }

        free(AppendedDestination);
        free(AppendedSource);
        if (Status != 0) {
            return Status;
        }
    }

    return 0;
}

INT
SetupWriteBootSectorFile (
    PSETUP_CONTEXT Context,
    PSETUP_COPY Command,
    BOOL WriteLbaOffset,
    BOOL Clobber
    )

/*++

Routine Description:

    This routine writes a file's contents out to the boot sector of the disk.

Arguments:

    Context - Supplies a pointer to the applicaton context.

    Command - Supplies a pointer to the copy command to execute.

    WriteLbaOffset - Supplies a boolean indicating if the function should
        write the LBA offset into the boot sector at the magic locations.

    Clobber - Supplies a boolean indicating whether to ignore previous
        disagreeing contents on the disk (TRUE) or to fail if writing the
        boot sector file overwrites some other data (FALSE).

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PUCHAR Block;
    PULONG BlockAddressPointer;
    ULONGLONG BlockCount;
    PUCHAR BootCodeSizePointer;
    ULONG ByteIndex;
    ssize_t BytesDone;
    LONGLONG DiskOffset;
    PUCHAR FileData;
    ULONG FileSize;
    ULONG Offset;
    INT Result;
    LONGLONG SeekResult;
    PVOID Source;

    Block = NULL;
    FileData = NULL;
    FileSize = 0;
    Source = SetupGetSourceVolume(Context, Command->SourceVolume);
    if (Source == NULL) {
        return EINVAL;
    }

    Result = SetupReadEntireFile(Context,
                                 Source,
                                 Command->Source,
                                 (PVOID *)&FileData,
                                 &FileSize);

    if (Result != 0) {
        fprintf(stderr,
                "Error: Failed to read boot sector file %s: %s.\n",
                Command->Source,
                strerror(Result));

        goto WriteBootSectorFileEnd;
    }

    Block = malloc(SETUP_BLOCK_SIZE);
    if (Block == NULL) {
        Result = ENOMEM;
        goto WriteBootSectorFileEnd;
    }

    //
    // Loop reading, modifying, and writing back sectors.
    //

    DiskOffset = Command->Offset / SETUP_BLOCK_SIZE;

    assert((DiskOffset * SETUP_BLOCK_SIZE) == Command->Offset);

    SeekResult = SetupPartitionSeek(Context, Context->Disk, DiskOffset);
    if (SeekResult != DiskOffset) {
        Result = errno;
        goto WriteBootSectorFileEnd;
    }

    Offset = 0;
    while (Offset < FileSize) {

        //
        // If clobbering, just write the contents over what's there.
        //

        if (Clobber != FALSE) {
            if (FileSize - Offset >= SETUP_BLOCK_SIZE) {
                memcpy(Block, FileData + Offset, SETUP_BLOCK_SIZE);
                Offset += SETUP_BLOCK_SIZE;

            } else {
                memset(Block, 0, SETUP_BLOCK_SIZE);
                memcpy(Block, FileData + Offset, FileSize - Offset);
                Offset = FileSize;
            }

        //
        // If not clobbering, carefully merge the bytes with what's already on
        // disk.
        //

        } else {
            BytesDone = SetupPartitionRead(Context,
                                           Context->Disk,
                                           Block,
                                           SETUP_BLOCK_SIZE);

            if (BytesDone != SETUP_BLOCK_SIZE) {
                fprintf(stderr,
                        "Read only %ld of %d bytes.\n",
                        (long)BytesDone,
                        SETUP_BLOCK_SIZE);

                Result = errno;
                if (Result == 0) {
                    Result = -1;
                }

                goto WriteBootSectorFileEnd;
            }

            //
            // Merge the boot file contents with what's on the disk. There
            // should be no byte set in both.
            //

            ByteIndex = 0;
            while (ByteIndex < SETUP_BLOCK_SIZE) {
                if ((Offset < FileSize) && (FileData[Offset] != 0)) {
                    if ((Block[ByteIndex] != 0) &&
                        (FileData[Offset] != Block[ByteIndex])) {

                        fprintf(stderr,
                                "Error: Aborted writing boot file %s, as "
                                "offset 0x%x contains byte 0x%x in the boot "
                                "file, but already contains byte 0x%x on "
                                "disk.\n",
                                Command->Source,
                                Offset,
                                FileData[Offset],
                                Block[ByteIndex]);

                        Result = EIO;
                        goto WriteBootSectorFileEnd;
                    }

                    Block[ByteIndex] = FileData[Offset];
                }

                ByteIndex += 1;
                Offset += 1;
            }
        }

        if (WriteLbaOffset != FALSE) {
            WriteLbaOffset = FALSE;

            assert(Offset == SETUP_BLOCK_SIZE);

            BlockCount = ALIGN_RANGE_UP(FileSize, SETUP_BLOCK_SIZE) /
                         SETUP_BLOCK_SIZE;

            if (BlockCount > MAX_UCHAR) {
                printf("Error: Boot code is too big at %lld sectors. Max is "
                       "%d.\n",
                       BlockCount,
                       MAX_UCHAR);

                Result = EIO;
                goto WriteBootSectorFileEnd;
            }

            BlockAddressPointer =
                      (PULONG)(Block + SETUP_BOOT_SECTOR_BLOCK_ADDRESS_OFFSET);

            if (*BlockAddressPointer != 0) {
                fprintf(stderr,
                        "Error: Location for boot sector LBA had %x in it.\n",
                        *BlockAddressPointer);

                Result = EIO;
                goto WriteBootSectorFileEnd;
            }

            assert((ULONG)DiskOffset == DiskOffset);

            *BlockAddressPointer = (ULONG)DiskOffset +
                                   Context->CurrentPartitionOffset;

            BootCodeSizePointer = Block + SETUP_BOOT_SECTOR_BLOCK_LENGTH_OFFSET;
            if (*BootCodeSizePointer != 0) {
                fprintf(stderr,
                        "Error: Location for boot sector size had %x in "
                        "it.\n",
                        *BootCodeSizePointer);

                Result = EIO;
                goto WriteBootSectorFileEnd;
            }

            *BootCodeSizePointer = (UCHAR)BlockCount;
        }

        //
        // Go back to that block and write it out.
        //

        SeekResult = SetupPartitionSeek(
                                 Context,
                                 Context->Disk,
                                 DiskOffset + (Offset / SETUP_BLOCK_SIZE) - 1);

        if (SeekResult != DiskOffset + (Offset / SETUP_BLOCK_SIZE) - 1) {
            fprintf(stderr, "Error: Seek failed.\n");
            Result = errno;
            if (Result == 0) {
                Result = -1;
            }

            goto WriteBootSectorFileEnd;
        }

        BytesDone = SetupPartitionWrite(Context,
                                        Context->Disk,
                                        Block,
                                        SETUP_BLOCK_SIZE);

        if (BytesDone != SETUP_BLOCK_SIZE) {
            fprintf(stderr,
                    "Error: Wrote only %ld of %d bytes.\n",
                    (long)BytesDone,
                    SETUP_BLOCK_SIZE);

            Result = errno;
            if (Result == 0) {
                Result = -1;
            }

            goto WriteBootSectorFileEnd;
        }
    }

    Result = 0;
    if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
        printf("Wrote file %s, size %d to boot sector 0x%llx.\n",
               Command->Source,
               FileSize,
               DiskOffset);
    }

WriteBootSectorFileEnd:
    if (Block != NULL) {
        free(Block);
    }

    if (FileData != NULL) {
        free(FileData);
    }

    return Result;
}

INT
SetupReadEntireFile (
    PSETUP_CONTEXT Context,
    PVOID Source,
    PCSTR SourcePath,
    PVOID *FileContents,
    PULONG FileContentsSize
    )

/*++

Routine Description:

    This routine reads a file's contents into memory.

Arguments:

    Context - Supplies a pointer to the applicaton context.

    Source - Supplies a pointer to the open source volume handle.

    SourcePath - Supplies the source path of the file to read.

    FileContents - Supplies a pointer where a pointer to the file data will be
        returned on success. The caller is responsible for freeing this
        memory when done.

    FileContentsSize - Supplies a pointer where the size of the file in bytes
        will be returned on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PVOID Buffer;
    ULONGLONG FileSize;
    mode_t Mode;
    time_t ModificationDate;
    INT Result;
    ssize_t Size;
    PVOID SourceFile;

    FileSize = 0;
    Buffer = NULL;
    SourceFile = SetupFileOpen(Source, SourcePath, O_RDONLY, 0);
    if (SourceFile == NULL) {
        fprintf(stderr, "Failed to open source file %s.\n", SourcePath);
        Result = errno;
        if (Result == 0) {
            Result = -1;
        }

        goto ReadEntireFileEnd;
    }

    Result = SetupFileFileStat(SourceFile, &FileSize, &ModificationDate, &Mode);
    if (Result != 0) {
        goto ReadEntireFileEnd;
    }

    //
    // If this is a directory, create a directory in the destination.
    //

    if (S_ISDIR(Mode) != 0) {
        fprintf(stderr,
                "Error: Setup tried to read in file %s but got a directory.\n",
                SourcePath);

        Result = EISDIR;
        goto ReadEntireFileEnd;
    }

    //
    // This is a regular file, so open it up and read it.
    //

    Buffer = malloc(FileSize);
    if (Buffer == NULL) {
        Result = ENOMEM;
        goto ReadEntireFileEnd;
    }

    Size = SetupFileRead(SourceFile, Buffer, FileSize);
    if (Size != FileSize) {
        fprintf(stderr,
                "Failed to read in file %s, got %d of %d bytes.\n",
                SourcePath,
                (ULONG)Size,
                (ULONG)FileSize);

        Result = errno;
        goto ReadEntireFileEnd;
    }

    Result = 0;

ReadEntireFileEnd:
    if (SourceFile != NULL) {
        SetupFileClose(SourceFile);
    }

    if (Result != 0) {
        if (Buffer != NULL) {
            free(Buffer);
        }

        FileSize = 0;
    }

    *FileContents = Buffer;

    assert((ULONG)FileSize == FileSize);

    *FileContentsSize = FileSize;
    return Result;
}

PSETUP_PARTITION_CONFIGURATION
SetupGetPartition (
    PSETUP_CONTEXT Context,
    ULONG Flag
    )

/*++

Routine Description:

    This routine is used to retrieve a given partition, usually either the boot
    or system partition.

Arguments:

    Context - Supplies a pointer to the application context.

    Flag - Supplies the partition flag to search for. See
        SETUP_PARTITION_FLAG_* definitions.

Return Value:

    Returns a pointer to the desired partition on success.

    NULL if no matching partition could be found.

--*/

{

    ULONG Count;
    ULONG Index;
    PSETUP_PARTITION_CONFIGURATION Partition;

    Partition = Context->Configuration->Disk.Partitions;
    Count = Context->Configuration->Disk.PartitionCount;
    for (Index = 0; Index < Count; Index += 1) {
        if ((Partition->Flags & Flag) != 0) {
            return Partition;
        }

        Partition += 1;
    }

    return NULL;
}

PVOID
SetupGetSourceVolume (
    PSETUP_CONTEXT Context,
    ULONG SourceVolume
    )

/*++

Routine Description:

    This routine gets the source volume given the source volume index.

Arguments:

    Context - Supplies a pointer to the application context.

    SourceVolume - Supplies the source volume index.

Return Value:

    Returns a pointer to the open volume on success.

    NULL if the source volume index is invalid.

--*/

{

    PVOID Source;

    if (SourceVolume == 0) {
        Source = Context->SourceVolume;

    } else if (SourceVolume == (ULONG)-1L) {
        Source = Context->HostFileSystem;

    } else {
        fprintf(stderr,
                "Error: Invalid source volume %d.\n",
                SourceVolume);

        Source = NULL;
    }

    return Source;
}

