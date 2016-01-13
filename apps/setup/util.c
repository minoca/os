/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    util.c

Abstract:

    This module implements utility functions for the setup program.

Author:

    Evan Green 10-Apr-2014

Environment:

    User

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

#define SETUP_FILE_BUFFER_SIZE (1024 * 512)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

PSTR SetupPartitionDescriptions[] = {
    "Invalid",
    "",
    "Unknown",
    "Empty",
    "FAT12",
    "FAT16",
    "Extended",
    "NTFS",
    "FAT32",
    "FAT32L",
    "FAT16",
    "ExtendedLba",
    "WinRE",
    "Plan9",
    "Hurd",
    "Minoca",
    "Minix",
    "Minix",
    "Linux Swap",
    "Linux",
    "LinuxExtended",
    "LinuxLVM",
    "BSD",
    "FreeBSD",
    "OpenBSD",
    "NeXTStep",
    "MacOSX",
    "NetBSD",
    "MacOSXBoot",
    "HFS",
    "EFIGPT",
    "EFISystem",
};

//
// ------------------------------------------------------------------ Functions
//

PSETUP_DESTINATION
SetupCreateDestination (
    SETUP_DESTINATION_TYPE Type,
    PSTR Path,
    DEVICE_ID DeviceId
    )

/*++

Routine Description:

    This routine creates a setup destination structure.

Arguments:

    Type - Supplies the destination type.

    Path - Supplies an optional pointer to the path. A copy of this string will
        be made.

    DeviceId - Supplies an optional device ID.

Return Value:

    Returns a pointer to the newly created destination on success.

    NULL on allocation failure.

--*/

{

    PSETUP_DESTINATION Destination;

    Destination = malloc(sizeof(SETUP_DESTINATION));
    if (Destination == NULL) {
        return NULL;
    }

    memset(Destination, 0, sizeof(SETUP_DESTINATION));
    Destination->Type = Type;
    Destination->DeviceId = DeviceId;
    if (Path != NULL) {
        Destination->Path = strdup(Path);
        if (Destination->Path == NULL) {
            free(Destination);
            return NULL;
        }
    }

    return Destination;
}

VOID
SetupDestroyDestination (
    PSETUP_DESTINATION Destination
    )

/*++

Routine Description:

    This routine destroys a setup destination structure.

Arguments:

    Destination - Supplies a pointer to the destination structure to free.

Return Value:

    None.

--*/

{

    if (Destination->Path != NULL) {
        free(Destination->Path);
    }

    free(Destination);
    return;
}

VOID
SetupDestroyDeviceDescriptions (
    PSETUP_PARTITION_DESCRIPTION Devices,
    ULONG DeviceCount
    )

/*++

Routine Description:

    This routine destroys an array of device descriptions.

Arguments:

    Devices - Supplies a pointer to the array to destroy.

    DeviceCount - Supplies the number of elements in the array.

Return Value:

    None.

--*/

{

    ULONG Index;

    for (Index = 0; Index < DeviceCount; Index += 1) {
        if (Devices[Index].Destination != NULL) {
            SetupDestroyDestination(Devices[Index].Destination);
        }
    }

    free(Devices);
    return;
}

VOID
SetupPrintDeviceDescription (
    PSETUP_PARTITION_DESCRIPTION Device,
    BOOL PrintHeader
    )

/*++

Routine Description:

    This routine prints a device description.

Arguments:

    Device - Supplies a pointer to the device description.

    PrintHeader - Supplies a boolean indicating if the column descriptions
        should be printed.

Return Value:

    None.

--*/

{

    PSTR DeviceType;
    ULONG DiskId;
    ULONGLONG Offset;
    CHAR OffsetString[20];
    CHAR PartitionFlavor;
    ULONG PartitionId;
    PSTR PartitionScheme;
    PSTR PartitionTypeString;
    ULONGLONG Size;
    CHAR SizeString[20];
    CHAR System;

    DeviceType = "Partition";
    PartitionFlavor = ' ';
    System = ' ';
    PartitionScheme = "";
    if (Device->Partition.PartitionFormat == PartitionFormatGpt) {
        PartitionScheme = "GPT";

    } else if (Device->Partition.PartitionFormat == PartitionFormatMbr) {
        PartitionScheme = "MBR";
    }

    if ((Device->Partition.Flags & PARTITION_FLAG_RAW_DISK) != 0) {
        DeviceType = "Disk";

    } else if ((Device->Partition.Flags & PARTITION_FLAG_BOOT) != 0) {
        PartitionFlavor = 'B';

    } else if ((Device->Partition.Flags & PARTITION_FLAG_EXTENDED) != 0) {
        PartitionFlavor = 'E';

    } else if ((Device->Partition.Flags & PARTITION_FLAG_LOGICAL) != 0) {
        PartitionFlavor = 'L';
    }

    if ((Device->Flags & SETUP_DEVICE_FLAG_SYSTEM) != 0) {
        System = 'S';
    }

    RtlCopyMemory(&DiskId, &(Device->Partition.DiskId[0]), sizeof(ULONG));
    if (Device->Partition.PartitionFormat == PartitionFormatMbr) {
        RtlCopyMemory(&PartitionId,
                      &(Device->Partition.PartitionId[sizeof(ULONG)]),
                      sizeof(ULONG));

    } else {
        RtlCopyMemory(&PartitionId,
                      &(Device->Partition.PartitionId[0]),
                      sizeof(ULONG));
    }

    PartitionTypeString = "";
    if (Device->Partition.PartitionType <
        sizeof(SetupPartitionDescriptions) /
        sizeof(SetupPartitionDescriptions[0])) {

        PartitionTypeString =
                   SetupPartitionDescriptions[Device->Partition.PartitionType];
    }

    Offset = Device->Partition.FirstBlock * Device->Partition.BlockSize;
    SetupPrintSize(OffsetString, sizeof(OffsetString), Offset);
    Size = ((Device->Partition.LastBlock + 1) * Device->Partition.BlockSize) -
           Offset;

    SetupPrintSize(SizeString, sizeof(SizeString), Size);
    if (PrintHeader != FALSE) {
        printf("    DiskId   PartID   DevType   Fmt    Type          Offset "
               "Size   Path\n");

        printf("    --------------------------------------------------------"
               "--------------------\n");
    }

    printf("    %08X %08X %-9s %3s %c%c %-13s %-6s %-6s ",
           DiskId,
           PartitionId,
           DeviceType,
           PartitionScheme,
           PartitionFlavor,
           System,
           PartitionTypeString,
           OffsetString,
           SizeString);

    SetupPrintDestination(Device->Destination);
    printf("\n");
    return;
}

ULONG
SetupPrintSize (
    PCHAR String,
    ULONG StringSize,
    ULONGLONG Value
    )

/*++

Routine Description:

    This routine prints a formatted size a la 5.8M (M for megabytes).

Arguments:

    String - Supplies a pointer to the string buffer to print to.

    StringSize - Supplies the total size of the string buffer in bytes.

    Value - Supplies the value in bytes to print.

Return Value:

    Returns the number of bytes successfully converted.

--*/

{

    ULONG Size;
    CHAR Suffix;

    Suffix = 'B';
    if (Value > 1024) {
        Suffix = 'K';
        Value = (Value * 10) / 1024;
        if (Value / 10 >= 1024) {
            Suffix = 'M';
            Value /= 1024;
            if (Value / 10 >= 1024) {
                Suffix = 'G';
                Value /= 1024;
                if (Value / 10 >= 1024) {
                    Suffix = 'T';
                    Value /= 1024;
                }
            }
        }
    }

    if (Suffix == 'B') {
        Size = snprintf(String, StringSize, "%d", (ULONG)Value);

    } else {
        if (Value < 100) {
            Size = snprintf(String,
                            StringSize,
                            "%d.%d%c",
                            (ULONG)Value / 10,
                            (ULONG)Value % 10,
                            Suffix);

        } else {
            Size = snprintf(String,
                            StringSize,
                            "%d%c",
                            (ULONG)Value / 10,
                            Suffix);
        }
    }

    return Size;
}

VOID
SetupPrintDestination (
    PSETUP_DESTINATION Destination
    )

/*++

Routine Description:

    This routine prints a destination structure.

Arguments:

    Destination - Supplies a pointer to the destination.

Return Value:

    None.

--*/

{

    if (Destination->Path != NULL) {
        printf("%s", Destination->Path);

    } else {
        printf("Device 0x%I64X", Destination->DeviceId);
    }

    return;
}

PSETUP_DESTINATION
SetupParseDestination (
    SETUP_DESTINATION_TYPE DestinationType,
    PSTR Argument
    )

/*++

Routine Description:

    This routine converts a string argument into a destination. Device ID
    destinations can start with "0x", and everything else is treated as a
    path. An empty string is not valid.

Arguments:

    DestinationType - Supplies the destination type.

    Argument - Supplies the string argument.

Return Value:

    Returns a pointer to a newly created destination on success. The caller
    is responsible for destroying this structure.

    NULL if the argument is not valid.

--*/

{

    PSTR AfterScan;
    ULONGLONG DeviceId;

    if (strlen(Argument) == 0) {
        return NULL;
    }

    if ((Argument[0] == '0') &&
        ((Argument[1] == 'x') || (Argument[1] == 'X'))) {

        DeviceId = strtoull(Argument, &AfterScan, 16);
        if (strlen(AfterScan) != 0) {
            return NULL;
        }

        return SetupCreateDestination(DestinationType, NULL, DeviceId);
    }

    return SetupCreateDestination(DestinationType, Argument, 0);
}

PSTR
SetupAppendPaths (
    PSTR Path1,
    PSTR Path2
    )

/*++

Routine Description:

    This routine appends two paths to one another.

Arguments:

    Path1 - Supplies a pointer to the first path.

    Path2 - Supplies a pointer to the second path.

Return Value:

    Returns a pointer to a newly created combined path on success. The caller
    is responsible for freeing this new path.

    NULL on allocation failure.

--*/

{

    size_t Length1;
    size_t Length2;
    PSTR NewPath;

    if (Path1 == NULL) {
        return strdup(Path2);
    }

    Length1 = strlen(Path1);
    Length2 = strlen(Path2);
    NewPath = malloc(Length1 + Length2 + 2);
    if (NewPath == NULL) {
        return NULL;
    }

    strcpy(NewPath, Path1);
    if ((Length1 != 0) && (Path1[Length1 - 1] != '/')) {
        NewPath[Length1] = '/';
        Length1 += 1;
    }

    strcpy(NewPath + Length1, Path2);
    return NewPath;
}

INT
SetupUpdateFile (
    PSETUP_CONTEXT Context,
    PVOID Destination,
    PVOID Source,
    PSTR DestinationPath,
    PSTR SourcePath
    )

/*++

Routine Description:

    This routine copies the given path from the source to the destination if
    the destination is older than the source. If the source is a directory, the
    contents of that directory are recursively copied to the destination
    (regardless of the age of the files inside the directory).

Arguments:

    Context - Supplies a pointer to the applicaton context.

    Destination - Supplies a pointer to the open destination volume
        handle.

    Source - Supplies a pointer to the open source volume handle.

    DestinationPath - Supplies a pointer to the path of the file to create at
        the destination.

    SourcePath - Supplies the source path of the copy.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PVOID DestinationFile;
    time_t DestinationModificationDate;
    INT Result;
    PVOID SourceFile;
    time_t SourceModificationDate;

    DestinationFile = NULL;

    //
    // Open up the source to get its modification date.
    //

    SourceFile = SetupFileOpen(Source, SourcePath, O_RDONLY, 0);
    if (SourceFile == NULL) {
        fprintf(stderr, "Failed to open source file %s.\n", SourcePath);
        Result = errno;
        if (Result == 0) {
            Result = -1;
        }

        goto UpdateFileEnd;
    }

    Result = SetupFileFileStat(SourceFile, NULL, &SourceModificationDate, NULL);
    if (Result != 0) {
        goto UpdateFileEnd;
    }

    SetupFileClose(SourceFile);
    SourceFile = NULL;

    //
    // Open up the destination.
    //

    DestinationFile = SetupFileOpen(Destination, DestinationPath, O_RDONLY, 0);

    //
    // If the destination does not exist, copy it.
    //

    if (DestinationFile == NULL) {
        Result = SetupCopyFile(Context,
                               Destination,
                               Source,
                               DestinationPath,
                               SourcePath);

        goto UpdateFileEnd;
    }

    Result = SetupFileFileStat(DestinationFile,
                               NULL,
                               &DestinationModificationDate,
                               NULL);

    if (Result != 0) {
        goto UpdateFileEnd;
    }

    SetupFileClose(DestinationFile);
    DestinationFile = NULL;
    if (DestinationModificationDate < SourceModificationDate) {
        Result = SetupCopyFile(Context,
                               Destination,
                               Source,
                               DestinationPath,
                               SourcePath);

        goto UpdateFileEnd;
    }

    if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
        printf("Not updating %s\n", DestinationPath);
    }

    Result = 0;

UpdateFileEnd:
    if (SourceFile != NULL) {
        SetupFileClose(SourceFile);
    }

    if (DestinationFile != NULL) {
        SetupFileClose(DestinationFile);
    }

    return Result;
}

INT
SetupCopyFile (
    PSETUP_CONTEXT Context,
    PVOID Destination,
    PVOID Source,
    PSTR DestinationPath,
    PSTR SourcePath
    )

/*++

Routine Description:

    This routine copies the given path from the source to the destination. If
    the source is a directory, the contents of that directory are recursively
    copied to the destination.

Arguments:

    Context - Supplies a pointer to the applicaton context.

    Destination - Supplies a pointer to the open destination volume
        handle.

    Source - Supplies a pointer to the open source volume handle.

    DestinationPath - Supplies a pointer to the path of the file to create at
        the destination.

    SourcePath - Supplies the source path of the copy.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR AppendedDestinationPath;
    PSTR AppendedSourcePath;
    PVOID Buffer;
    PVOID DestinationFile;
    PSTR DirectoryEntry;
    PSTR Enumeration;
    ULONGLONG FileSize;
    PSTR LinkTarget;
    INT LinkTargetSize;
    mode_t Mode;
    time_t ModificationDate;
    INT Result;
    ssize_t Size;
    ssize_t SizeWritten;
    PVOID SourceFile;

    AppendedDestinationPath = NULL;
    AppendedSourcePath = NULL;
    Buffer = NULL;
    DestinationFile = NULL;
    Enumeration = NULL;
    LinkTarget = NULL;
    SourceFile = SetupFileOpen(Source, SourcePath, O_RDONLY | O_NOFOLLOW, 0);
    if (SourceFile == NULL) {

        //
        // If the file could not be opened, maybe it's a symbolic link. Try to
        // read that.
        //

        Result = SetupFileReadLink(Source,
                                   SourcePath,
                                   &LinkTarget,
                                   &LinkTargetSize);

        if (Result != 0) {
            fprintf(stderr, "Failed to open source file %s.\n", SourcePath);
            Result = errno;
            if (Result == 0) {
                Result = -1;
            }

            goto CopyFileEnd;
        }

        //
        // Try to create a link in the target.
        //

        Result = SetupFileSymlink(Destination,
                                  DestinationPath,
                                  LinkTarget,
                                  LinkTargetSize);

        //
        // If not possible, fall back to copying the file.
        //

        if (Result == 0) {
            goto CopyFileEnd;
        }

        fprintf(stderr,
                "Failed to create symbolic link at %s, copying instead.\n",
                DestinationPath);

        SourceFile = SetupFileOpen(Source, LinkTarget, O_RDONLY, 0);
        if (SourceFile == NULL) {
            fprintf(stderr,
                    "Failed to open source file link %s.\n",
                    LinkTarget);

            Result = errno;
            if (Result == 0) {
                Result = -1;
            }

            goto CopyFileEnd;
        }
    }

    Result = SetupFileFileStat(SourceFile, &FileSize, &ModificationDate, &Mode);
    if (Result != 0) {
        goto CopyFileEnd;
    }

    //
    // If this is a directory, create a directory in the destination.
    //

    if (S_ISDIR(Mode) != 0) {
        if (S_IRGRP == 0) {
            if ((Mode & S_IRUSR) != 0) {
                Mode |= FILE_PERMISSION_GROUP_READ |
                        FILE_PERMISSION_OTHER_READ |
                        FILE_PERMISSION_USER_EXECUTE |
                        FILE_PERMISSION_GROUP_EXECUTE |
                        FILE_PERMISSION_OTHER_EXECUTE;
            }
        }

        Result = SetupFileCreateDirectory(Destination,
                                          DestinationPath,
                                          Mode);

        if (Result != 0) {
            fprintf(stderr,
                    "Failed to create destination directory %s.\n",
                    DestinationPath);

            goto CopyFileEnd;
        }

        Result = SetupFileEnumerateDirectory(Source, SourcePath, &Enumeration);
        if (Result != 0) {
            fprintf(stderr, "Failed to enumerate directory %s.\n", SourcePath);
            goto CopyFileEnd;
        }

        //
        // Loop over every file in the directory.
        //

        DirectoryEntry = Enumeration;
        while (*DirectoryEntry != '\0') {
            AppendedDestinationPath = SetupAppendPaths(DestinationPath,
                                                       DirectoryEntry);

            if (AppendedDestinationPath == NULL) {
                Result = ENOMEM;
                goto CopyFileEnd;
            }

            AppendedSourcePath = SetupAppendPaths(SourcePath, DirectoryEntry);
            if (AppendedSourcePath == NULL) {
                Result = ENOMEM;
                goto CopyFileEnd;
            }

            //
            // Recurse and copy each file inside the directory.
            //

            Result = SetupCopyFile(Context,
                                   Destination,
                                   Source,
                                   AppendedDestinationPath,
                                   AppendedSourcePath);

            if (Result != 0) {
                fprintf(stderr,
                        "Failed to copy %s.\n",
                        AppendedDestinationPath);

                goto CopyFileEnd;
            }

            free(AppendedDestinationPath);
            free(AppendedSourcePath);
            AppendedDestinationPath = NULL;
            AppendedSourcePath = NULL;
            DirectoryEntry += strlen(DirectoryEntry) + 1;
        }

        //
        // Set directory permissions.
        //

        Result = SetupFileSetAttributes(Destination,
                                        DestinationPath,
                                        ModificationDate,
                                        Mode);

        if (Result != 0) {
            fprintf(stderr,
                    "Failed to set mode on directory %s.\n",
                    DestinationPath);

            goto CopyFileEnd;
        }

    //
    // This is a regular file, so open it up in the destination and copy it in
    // chunks.
    //

    } else {
        Buffer = malloc(SETUP_FILE_BUFFER_SIZE);
        if (Buffer == NULL) {
            Result = ENOMEM;
            goto CopyFileEnd;
        }

        if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
            printf("Copying %s -> %s\n", SourcePath, DestinationPath);
        }

        DestinationFile = SetupFileOpen(Destination,
                                        DestinationPath,
                                        O_CREAT | O_TRUNC | O_RDWR,
                                        Mode);

        if (DestinationFile == NULL) {
            fprintf(stderr, "Failed to create destination file %s.\n");
            goto CopyFileEnd;
        }

        //
        // Loop copying chunks.
        //

        while (FileSize != 0) {
            Size = SETUP_FILE_BUFFER_SIZE;
            if (Size > FileSize) {
                Size = FileSize;
            }

            Size = SetupFileRead(SourceFile, Buffer, Size);
            if (Size <= 0) {
                break;
            }

            SizeWritten = SetupFileWrite(DestinationFile, Buffer, Size);
            if (SizeWritten != Size) {
                fprintf(stderr,
                        "Failed to write to file %s.\n",
                        DestinationPath);

                Result = errno;
                goto CopyFileEnd;
            }

            FileSize -= Size;
        }

        //
        // Set file permissions.
        //

        SetupFileClose(DestinationFile);
        DestinationFile = NULL;
        Result = SetupFileSetAttributes(Destination,
                                        DestinationPath,
                                        ModificationDate,
                                        Mode);

        if (Result != 0) {
            fprintf(stderr,
                    "Failed to set mode on file %s, ModData %I64x Mode %x, "
                    "Result %d\n",
                    DestinationPath,
                    (ULONGLONG)ModificationDate,
                    Mode,
                    Result);

            goto CopyFileEnd;
        }
    }

CopyFileEnd:
    if (AppendedDestinationPath != NULL) {
        free(AppendedDestinationPath);
    }

    if (AppendedSourcePath != NULL) {
        free(AppendedSourcePath);
    }

    if (DestinationFile != NULL) {
        SetupFileClose(DestinationFile);
    }

    if (SourceFile != NULL) {
        SetupFileClose(SourceFile);
    }

    if (Enumeration != NULL) {
        free(Enumeration);
    }

    if (Buffer != NULL) {
        free(Buffer);
    }

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

