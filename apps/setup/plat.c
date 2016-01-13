/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    plat.c

Abstract:

    This module contains platform specific setup instructions.

Author:

    Evan Green 7-May-2014

Environment:

    User Mode

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

#include "setup.h"

//
// ---------------------------------------------------------------- Definitions
//

#define BIOS_PC_FLAGS                               \
    (SETUP_FLAG_WRITE_BOOT_LBA | SETUP_FLAG_MBR |   \
     SETUP_FLAG_1MB_PARTITION_ALIGNMENT)

#define RASPBERRY_PI_BOOT_IMAGES                    \
    "minoca/system/rpi/rpifw=rpifw\n"               \
    "minoca/system/rpi/config.txt=config.txt\n"     \
    "minoca/system/rpi/fixup.dat=fixup.dat\n"       \
    "minoca/system/rpi/start.elf=start.elf\n"       \
    "minoca/system/rpi/bootcode.bin=bootcode.bin"

#define RASPBERRY_PI_2_BOOT_IMAGES                  \
    "minoca/system/rpi2/rpifw=rpifw\n"              \
    "minoca/system/rpi2/config.txt=config.txt\n"    \
    "minoca/system/rpi2/fixup.dat=fixup.dat\n"      \
    "minoca/system/rpi2/start.elf=start.elf\n"      \
    "minoca/system/rpi2/bootcode.bin=bootcode.bin"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
SetupWriteBootSectorFile (
    PSETUP_CONTEXT Context,
    PVOID Source,
    PSTR SourcePath,
    ULONGLONG DiskOffset,
    BOOL WriteLbaOffset
    );

INT
SetupReadEntireFile (
    PSETUP_CONTEXT Context,
    PVOID Source,
    PSTR SourcePath,
    PVOID *FileContents,
    PULONG FileContentsSize
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the recipes used to install to specific platforms.
//

SETUP_PLATFORM_RECIPE SetupPlatformRecipes[] = {
    {
        SetupPlatformUnknown,
        "unknown",
        "Unknown",
        NULL,
        0,
        NULL,
        NULL,
        NULL,
        NULL
    },
    {
        SetupPlatformBiosPc,
        "pc",
        "Generic PC/AT",
        NULL,
        BIOS_PC_FLAGS,
        "minoca/system/pcat/mbr.bin",
        "minoca/system/pcat/fatboot.bin",
        "system/pcat/loader",
        "minoca/system/pcat/bootman.bin=bootman.bin"
    },
    {
        SetupPlatformEfiPc,
        "efipc",
        "Generic UEFI PC",
        NULL,
        0,
        NULL,
        NULL,
        NULL,
        NULL
    },
    {
        SetupPlatformTiBeagleBoneBlack,
        "beagleboneblack",
        "TI BeagleBone Black",
        "BBBK",
        SETUP_FLAG_MBR | SETUP_FLAG_1MB_PARTITION_ALIGNMENT,
        "minoca/system/bbone/bbonemlo",
        NULL,
        NULL,
        "minoca/system/bbone/bbonefw=bbonefw"
    },
    {
        SetupPlatformTiPandaBoard,
        "pandaboard",
        "TI PandaBoard",
        "PandaBoard",
        SETUP_FLAG_MBR | SETUP_FLAG_1MB_PARTITION_ALIGNMENT,
        "minoca/system/panda/omap4mlo",
        NULL,
        NULL,
        "minoca/system/panda/pandafw=pandafw"
    },
    {
        SetupPlatformTiPandaBoard,
        "pandaboard-es",
        "TI PandaBoard ES",
        "PandaBoard ES",
        SETUP_FLAG_MBR | SETUP_FLAG_1MB_PARTITION_ALIGNMENT,
        "minoca/system/panda/omap4mlo",
        NULL,
        NULL,
        "minoca/system/panda/pandafw=pandafw"
    },
    {
        SetupPlatformRaspberryPi,
        "raspberrypi",
        "Raspberry Pi",
        "Raspberry Pi",
        SETUP_FLAG_MBR,
        NULL,
        NULL,
        NULL,
        RASPBERRY_PI_BOOT_IMAGES
    },
    {
        SetupPlatformRaspberryPi2,
        "raspberrypi2",
        "Raspberry Pi 2",
        "Raspberry Pi 2",
        SETUP_FLAG_MBR,
        NULL,
        NULL,
        NULL,
        RASPBERRY_PI_2_BOOT_IMAGES
    }
};

//
// ------------------------------------------------------------------ Functions
//

BOOL
SetupParsePlatformString (
    PSETUP_CONTEXT Context,
    PSTR PlatformString
    )

/*++

Routine Description:

    This routine converts a platform string into a platform identifier, and
    sets it in the setup context.

Arguments:

    Context - Supplies a pointer to the setup context.

    PlatformString - Supplies a pointer to the string to convert to a
        platform identifier.

Return Value:

    TRUE if the platform name was successfully converted.

    FALSE if the name was invalid.

--*/

{

    ULONG PlatformCount;
    ULONG PlatformIndex;
    PSETUP_PLATFORM_RECIPE Recipe;

    PlatformCount = sizeof(SetupPlatformRecipes) /
                    sizeof(SetupPlatformRecipes[0]);

    for (PlatformIndex = 0; PlatformIndex < PlatformCount; PlatformIndex += 1) {
        Recipe = &(SetupPlatformRecipes[PlatformIndex]);
        if ((strcasecmp(PlatformString, Recipe->ShortName) == 0) ||
            (strcasecmp(PlatformString, Recipe->Description) == 0)) {

            Context->Recipe = Recipe;
            return TRUE;
        }
    }

    return FALSE;
}

VOID
SetupPrintPlatformList (
    VOID
    )

/*++

Routine Description:

    This routine prints the supported platform list.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG PlatformCount;
    ULONG PlatformIndex;
    PSETUP_PLATFORM_RECIPE Recipe;

    printf("Supported platforms:\n");
    PlatformCount = sizeof(SetupPlatformRecipes) /
                    sizeof(SetupPlatformRecipes[0]);

    for (PlatformIndex = 0; PlatformIndex < PlatformCount; PlatformIndex += 1) {
        Recipe = &(SetupPlatformRecipes[PlatformIndex]);
        printf("    %s -- %s\n", Recipe->ShortName, Recipe->Description);
    }

    return;
}

INT
SetupDeterminePlatform (
    PSETUP_CONTEXT Context
    )

/*++

Routine Description:

    This routine finalizes the setup platform recipe to use.

Arguments:

    Context - Supplies a pointer to the setup context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    SETUP_PLATFORM Fallback;
    PSETUP_PLATFORM_RECIPE FallbackRecipe;
    ULONG PlatformCount;
    ULONG PlatformIndex;
    PSTR PlatformName;
    PSETUP_PLATFORM_RECIPE Recipe;
    INT Result;

    PlatformName = NULL;
    Fallback = SetupPlatformInvalid;

    //
    // If the user specified a platform, just use it.
    //

    if (Context->Recipe != NULL) {
        return 0;
    }

    //
    // Ask the OS to detect the current platform.
    //

    Result = SetupOsGetPlatformName(&PlatformName, &Fallback);
    if (Result != 0) {
        if (Result != ENOSYS) {
            fprintf(stderr,
                    "Failed to detect platform: %s\n",
                    strerror(Result));
        }

        goto DeterminePlatformEnd;
    }

    if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
        printf("SMBIOS Platform Name: %s\n", PlatformName);
    }

    FallbackRecipe = NULL;
    PlatformCount = sizeof(SetupPlatformRecipes) /
                    sizeof(SetupPlatformRecipes[0]);

    for (PlatformIndex = 0; PlatformIndex < PlatformCount; PlatformIndex += 1) {
        Recipe = &(SetupPlatformRecipes[PlatformIndex]);
        if (Recipe->Platform == Fallback) {
            FallbackRecipe = Recipe;
        }

        if ((PlatformName != NULL) &&
            (Recipe->SmbiosProductName != NULL) &&
            (strcasecmp(Recipe->SmbiosProductName, PlatformName) == 0)) {

            Context->Recipe = Recipe;
            break;
        }
    }

    if (Context->Recipe == NULL) {
        if (FallbackRecipe == NULL) {
            fprintf(stderr,
                    "Failed to convert platform name %s to recipe.\n",
                    PlatformName);

            Result = EINVAL;
            goto DeterminePlatformEnd;
        }

        Context->Recipe = FallbackRecipe;
    }

    if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
        printf("Platform: %s\n", Context->Recipe->Description);
    }

    Result = 0;

DeterminePlatformEnd:
    if (PlatformName != NULL) {
        free(PlatformName);
    }

    return Result;
}

INT
SetupInstallBootSector (
    PSETUP_CONTEXT Context
    )

/*++

Routine Description:

    This routine writes the MBR and VBR to the disk. This should be done after
    the file system has set up the beginning of the install partition (so that
    it doesn't clobber the VBR being written here).

Arguments:

    Context - Supplies a pointer to the setup context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    ULONGLONG PartitionOffset;
    INT Result;
    BOOL WriteLbaOffset;

    WriteLbaOffset = FALSE;

    //
    // Get the install partition offset, and reset the current offset to zero
    // to access the MBR area.
    //

    PartitionOffset = Context->CurrentPartitionOffset;
    Context->CurrentPartitionOffset = 0;
    Context->CurrentPartitionSize += PartitionOffset;
    if (Context->Recipe->Mbr != NULL) {
        Result = SetupWriteBootSectorFile(Context,
                                          Context->SourceVolume,
                                          Context->Recipe->Mbr,
                                          0,
                                          WriteLbaOffset);

        if (Result != 0) {
            fprintf(stderr, "Failed to write MBR.\n");
            goto InstallBootSectorEnd;
        }
    }

    if (Context->Recipe->Vbr != NULL) {
        if ((Context->Flags & SETUP_FLAG_WRITE_BOOT_LBA) != 0) {
            WriteLbaOffset = TRUE;
        }

        Result = SetupWriteBootSectorFile(Context,
                                          Context->SourceVolume,
                                          Context->Recipe->Vbr,
                                          PartitionOffset,
                                          WriteLbaOffset);

        if (Result != 0) {
            fprintf(stderr, "Failed to write VBR.\n");
            goto InstallBootSectorEnd;
        }
    }

    Result = 0;

InstallBootSectorEnd:

    //
    // Restore the current partition offset.
    //

    Context->CurrentPartitionOffset = PartitionOffset;
    Context->CurrentPartitionSize -= PartitionOffset;
    return Result;
}

INT
SetupInstallPlatformBootFiles (
    PSETUP_CONTEXT Context,
    PVOID BootVolume
    )

/*++

Routine Description:

    This routine writes any custom platform specific boot files to the boot
    volume.

Arguments:

    Context - Supplies a pointer to the setup context.

    BootVolume - Supplies a pointer to the open boot volume.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR Destination;
    PSTR File;
    PSTR Files;
    PSTR NextFile;
    INT Result;

    //
    // Do nothing if there are no platform boot files.
    //

    if (Context->Recipe->BootFiles == NULL) {
        return 0;
    }

    //
    // Make a copy of the string that can be modified.
    //

    Files = strdup(Context->Recipe->BootFiles);
    if (Files == NULL) {
        Result = ENOMEM;
        goto InstallPlatformBootFilesEnd;
    }

    //
    // Loop processing files.
    //

    File = Files;
    while (TRUE) {
        if (*File == '\0') {
            break;
        }

        //
        // Find the end of this file, and terminate the string.
        //

        NextFile = File;
        while ((*NextFile != '\0') && (*NextFile != '\n')) {
            NextFile += 1;
        }

        if (*NextFile == '\n') {
            *NextFile = '\0';
            NextFile += 1;
        }

        //
        // Find the equals sign, which separates the source from the
        // destination.
        //

        Destination = File;
        while ((*Destination != '\0') && (*Destination != '=')) {
            Destination += 1;
        }

        if (*Destination == '=') {
            *Destination = '\0';
            Destination += 1;
            if (*Destination == '\0') {
                Destination = NULL;
            }

        } else {
            Destination = NULL;
        }

        //
        // If no destination was specified, assume it's the same as the source.
        //

        if (Destination == NULL) {
            Destination = File;
        }

        //
        // Update the specified file.
        //

        Result = SetupUpdateFile(Context,
                                 BootVolume,
                                 Context->SourceVolume,
                                 Destination,
                                 File);

        if (Result != 0) {
            fprintf(stderr,
                    "Error: Failed to update platform boot file %s -> %s.\n",
                    File,
                    Destination);

            goto InstallPlatformBootFilesEnd;
        }

        File = NextFile;
    }

InstallPlatformBootFilesEnd:
    if (Files != NULL) {
        free(Files);
    }

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
SetupWriteBootSectorFile (
    PSETUP_CONTEXT Context,
    PVOID Source,
    PSTR SourcePath,
    ULONGLONG DiskOffset,
    BOOL WriteLbaOffset
    )

/*++

Routine Description:

    This routine writes a file's contents out to the boot sector of the disk.

Arguments:

    Context - Supplies a pointer to the applicaton context.

    Source - Supplies a pointer to the open source volume handle.

    SourcePath - Supplies the source path of the file to read.

    DiskOffset - Supplies the offset from the beginning of the disk in blocks
        to write to.

    WriteLbaOffset - Supplies a boolean indicating if the function should
        write the LBA offset into the boot sector at the magic locations.

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
    PUCHAR FileData;
    ULONG FileSize;
    ULONG Offset;
    INT Result;
    ULONGLONG SeekResult;

    Block = NULL;
    FileData = NULL;
    FileSize = 0;
    Result = SetupReadEntireFile(Context,
                                 Source,
                                 SourcePath,
                                 (PVOID *)&FileData,
                                 &FileSize);

    if (Result != 0) {
        fprintf(stderr,
                "Error: Failed to read boot sector file %s: %s.\n",
                SourcePath,
                strerror(Result));

        goto WriteBootFileEnd;
    }

    Block = malloc(SETUP_BLOCK_SIZE);
    if (Block == NULL) {
        Result = ENOMEM;
        goto WriteBootFileEnd;
    }

    //
    // Loop reading, modifying, and writing back sectors.
    //

    SeekResult = SetupPartitionSeek(Context, Context->Disk, DiskOffset);
    if (SeekResult != DiskOffset) {
        Result = errno;
        goto WriteBootFileEnd;
    }

    Offset = 0;
    while (Offset < FileSize) {
        BytesDone = SetupPartitionRead(Context,
                                       Context->Disk,
                                       Block,
                                       SETUP_BLOCK_SIZE);

        if (BytesDone != SETUP_BLOCK_SIZE) {
            fprintf(stderr,
                    "Read only %d of %d bytes.\n",
                    BytesDone,
                    SETUP_BLOCK_SIZE);

            Result = errno;
            if (Result == 0) {
                Result = -1;
            }

            goto WriteBootFileEnd;
        }

        //
        // Merge the boot file contents with what's on the disk. There should
        // be no byte set in both.
        //

        ByteIndex = 0;
        while (ByteIndex < SETUP_BLOCK_SIZE) {
            if (Offset < FileSize) {
                if (FileData[Offset] != 0) {
                    if ((Block[ByteIndex] != 0) &&
                        (FileData[Offset] != Block[ByteIndex])) {

                        fprintf(stderr,
                                "Error: Aborted writing boot file %s, as "
                                "offset 0x%x contains byte 0x%x in the boot "
                                "file, but already contains byte 0x%x on "
                                "disk.\n",
                                SourcePath,
                                Offset,
                                FileData[Offset],
                                Block[ByteIndex]);

                        Result = EIO;
                        goto WriteBootFileEnd;
                    }

                    Block[ByteIndex] = FileData[Offset];
                }
            }

            ByteIndex += 1;
            Offset += 1;
        }

        if (WriteLbaOffset != FALSE) {
            WriteLbaOffset = FALSE;

            assert(Offset == SETUP_BLOCK_SIZE);

            BlockCount = ALIGN_RANGE_UP(FileSize, SETUP_BLOCK_SIZE) /
                         SETUP_BLOCK_SIZE;

            if (BlockCount > MAX_UCHAR) {
                printf("Error: Boot code is too big at %d sectors. Max is "
                       "%d.\n",
                       BlockCount,
                       MAX_UCHAR);

                Result = EIO;
                goto WriteBootFileEnd;
            }

            BlockAddressPointer =
                      (PULONG)(Block + SETUP_BOOT_SECTOR_BLOCK_ADDRESS_OFFSET);

            if (*BlockAddressPointer != 0) {
                fprintf(stderr,
                        "Error: Location for boot sector LBA had %x in it.\n",
                        *BlockAddressPointer);

                Result = EIO;
                goto WriteBootFileEnd;
            }

            assert((ULONG)DiskOffset == DiskOffset);

            *BlockAddressPointer = (ULONG)DiskOffset;
            BootCodeSizePointer = Block + SETUP_BOOT_SECTOR_BLOCK_LENGTH_OFFSET;
            if (*BootCodeSizePointer != 0) {
                fprintf(stderr,
                        "Error: Location for boot sector size had %x in "
                        "it.\n",
                        *BootCodeSizePointer);

                Result = EIO;
                goto WriteBootFileEnd;
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

            goto WriteBootFileEnd;
        }

        BytesDone = SetupPartitionWrite(Context,
                                        Context->Disk,
                                        Block,
                                        SETUP_BLOCK_SIZE);

        if (BytesDone != SETUP_BLOCK_SIZE) {
            fprintf(stderr,
                    "Error: Wrote only %d of %d bytes.\n",
                    BytesDone,
                    SETUP_BLOCK_SIZE);

            Result = errno;
            if (Result == 0) {
                Result = -1;
            }

            goto WriteBootFileEnd;
        }
    }

    Result = 0;
    if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
        printf("Wrote file %s, size %d to boot sector 0x%I64x.\n",
               SourcePath,
               FileSize,
               DiskOffset);
    }

WriteBootFileEnd:
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
    PSTR SourcePath,
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

