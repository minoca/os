/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
#include <time.h>

#include "setup.h"
#include "sconf.h"

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
        printf("Device 0x%llX", Destination->DeviceId);
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

    if (*Argument == '\0') {
        return NULL;
    }

    if ((Argument[0] == '0') &&
        ((Argument[1] == 'x') || (Argument[1] == 'X'))) {

        DeviceId = strtoull(Argument, &AfterScan, 16);
        if (*AfterScan != '\0') {
            return NULL;
        }

        return SetupCreateDestination(DestinationType, NULL, DeviceId);
    }

    return SetupCreateDestination(DestinationType, Argument, 0);
}

PSTR
SetupAppendPaths (
    PCSTR Path1,
    PCSTR Path2
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
SetupConvertStringArrayToLines (
    PCSTR *StringArray,
    PSTR *ResultBuffer,
    PUINTN ResultBufferSize
    )

/*++

Routine Description:

    This routine converts a null-terminated array of strings into a single
    buffer where each element is separated by a newline.

Arguments:

    StringArray - Supplies a pointer to the array of strings. The array must be
        terminated by a NULL entry.

    ResultBuffer - Supplies a pointer where a string will be returned
        containing all the lines. The caller is responsible for freeing this
        buffer.

    ResultBufferSize - Supplies a pointer where size of the buffer in bytes
        will be returned, including the null terminator.

Return Value:

    0 on success.

    ENOMEM on allocation failure.

--*/

{

    UINTN AllocationSize;
    PCSTR *Array;
    PSTR Buffer;
    PSTR Current;
    size_t Length;

    AllocationSize = 1;
    Array = StringArray;
    while (*Array != NULL) {
        AllocationSize += strlen(*Array) + 1;
        Array += 1;
    }

    Buffer = malloc(AllocationSize);
    if (Buffer == NULL) {
        return ENOMEM;
    }

    Current = Buffer;
    Array = StringArray;
    while (*Array != NULL) {
        Length = strlen(*Array);
        memcpy(Current, *Array, Length);
        Current[Length] = '\n';
        Current += Length + 1;
        Array += 1;
    }

    *Current = '\0';
    *ResultBuffer = Buffer;
    *ResultBufferSize = AllocationSize;
    return 0;
}

INT
SetupCopyFile (
    PSETUP_CONTEXT Context,
    PVOID Destination,
    PVOID Source,
    PCSTR DestinationPath,
    PCSTR SourcePath,
    ULONG Flags
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

    Flags - Supplies a bitfield of flags governing the operation. See
        SETUP_COPY_FLAG_* definitions.

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
    mode_t ExistingMode;
    time_t ExistingModificationDate;
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
    FileSize = 0;
    Enumeration = NULL;
    LinkTarget = NULL;
    Mode = 0;
    ModificationDate = 0;
    Result = -1;
    SourceFile = SetupFileOpen(Source, SourcePath, O_RDONLY | O_NOFOLLOW, 0);

    //
    // Some OSes don't allow opening of directories. If the source open failed
    // and the error is that it's a directory, then handle that.
    //

    if ((SourceFile == NULL) && (errno == EISDIR)) {
        Mode = S_IFDIR | FILE_PERMISSION_USER_ALL | FILE_PERMISSION_GROUP_ALL |
               FILE_PERMISSION_OTHER_READ |
               FILE_PERMISSION_OTHER_EXECUTE;

        ModificationDate = time(NULL);

    } else if (SourceFile == NULL) {

        //
        // If the file could not be opened, maybe it's a symbolic link. Try to
        // read that.
        //

        Result = SetupFileReadLink(Source,
                                   SourcePath,
                                   &LinkTarget,
                                   &LinkTargetSize);

        if (Result != 0) {
            Result = errno;

            //
            // Forgive optional copies if they don't exist.
            //

            if ((Result == ENOENT) &&
                ((Flags & SETUP_COPY_FLAG_OPTIONAL) != 0)) {

                Result = 0;

            } else {
                fprintf(stderr,
                        "Failed to open source file %s: %s\n",
                        SourcePath,
                        strerror(Result));

                if (Result == 0) {
                    Result = -1;
                }
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

    if (SourceFile != NULL) {
        Result = SetupFileFileStat(SourceFile,
                                   &FileSize,
                                   &ModificationDate,
                                   &Mode);

        if (Result != 0) {
            goto CopyFileEnd;
        }
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

        //
        // Attempt to create the destination directory at once. If it fails,
        // perhaps the directories leading up to it must be created.
        //

        Result = SetupFileCreateDirectory(Destination,
                                          DestinationPath,
                                          Mode);

        if (Result != 0) {
            Result = SetupCreateDirectories(Context,
                                            Destination,
                                            DestinationPath);

            if (Result == 0) {
                Result = SetupFileCreateDirectory(Destination,
                                                  DestinationPath,
                                                  Mode);
            }
        }

        if (Result != 0) {
            Result = errno;
            fprintf(stderr,
                    "Failed to create destination directory %s.\n",
                    DestinationPath);

            if (Result == 0) {
                Result = -1;
            }

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
                                   AppendedSourcePath,
                                   Flags);

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

        //
        // If this is an update operation, first try to open up the destination
        // to see if it is newer than the source.
        //

        if ((Flags & SETUP_COPY_FLAG_UPDATE) != 0) {
            DestinationFile = SetupFileOpen(Destination,
                                            DestinationPath,
                                            O_RDONLY | O_NOFOLLOW,
                                            0);

            if (DestinationFile != NULL) {
                Result = SetupFileFileStat(DestinationFile,
                                           NULL,
                                           &ExistingModificationDate,
                                           &ExistingMode);

                SetupFileClose(DestinationFile);
                DestinationFile = NULL;
                if (Result != 0) {
                    goto CopyFileEnd;
                }

                //
                // If the existing one is the same type of file and is at least
                // as new, don't update it.
                //

                if ((ExistingModificationDate >= ModificationDate) &&
                    (((ExistingMode ^ Mode) & S_IFMT) == 0)) {

                    if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
                        printf("Skipping %s -> %s\n",
                               SourcePath,
                               DestinationPath);
                    }

                    Result = 0;
                    goto CopyFileEnd;
                }
            }
        }

        //
        // Some OSes don't have an executable bit, but still need to be able
        // to install executables onto OSes that do. Probe around a bit to see
        // if the file should be set executable.
        //

        if ((Mode & FILE_PERMISSION_ALL_EXECUTE) == 0) {
            SetupFileDetermineExecuteBit(SourceFile, SourcePath, &Mode);
        }

        Buffer = malloc(SETUP_FILE_BUFFER_SIZE);
        if (Buffer == NULL) {
            Result = ENOMEM;
            goto CopyFileEnd;
        }

        if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
            printf("Copying %s -> %s\n", SourcePath, DestinationPath);
        }

        SetupCreateDirectories(Context, Destination, DestinationPath);
        DestinationFile = SetupFileOpen(Destination,
                                        DestinationPath,
                                        O_CREAT | O_TRUNC | O_RDWR | O_NOFOLLOW,
                                        Mode);

        if (DestinationFile == NULL) {
            Result = errno;
            fprintf(stderr,
                    "Failed to create destination file %s.\n",
                    DestinationPath);

            if (Result == 0) {
                Result = -1;
            }

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
                if (Size < 0) {
                    Result = errno;
                    if (Result == 0) {
                        Result = EINVAL;
                    }

                    goto CopyFileEnd;
                }

                break;
            }

            SizeWritten = SetupFileWrite(DestinationFile, Buffer, Size);
            if (SizeWritten != Size) {
                fprintf(stderr,
                        "Failed to write to file %s.\n",
                        DestinationPath);

                Result = errno;
                if (Result == 0) {
                    Result = errno;
                }

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
                    "Failed to set mode on file %s, ModData %llx Mode %x, "
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

INT
SetupCreateAndWriteFile (
    PSETUP_CONTEXT Context,
    PVOID Destination,
    PCSTR DestinationPath,
    PVOID Contents,
    ULONG ContentsSize
    )

/*++

Routine Description:

    This routine creates a file and writes the given contents out to it.

Arguments:

    Context - Supplies a pointer to the applicaton context.

    Destination - Supplies a pointer to the open destination volume
        handle.

    DestinationPath - Supplies a pointer to the path of the file to create at
        the destination.

    Contents - Supplies the buffer containing the file contents to write.

    ContentsSize - Supplies the size of the buffer in bytes.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PVOID DestinationFile;
    mode_t Mode;
    INT Status;
    size_t TotalWritten;
    ssize_t Written;

    Status = 0;
    Mode = FILE_PERMISSION_USER_READ |
           FILE_PERMISSION_USER_WRITE |
           FILE_PERMISSION_GROUP_READ |
           FILE_PERMISSION_GROUP_WRITE |
           FILE_PERMISSION_OTHER_READ;

    if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
        printf("Creating %s\n", DestinationPath);
    }

    SetupCreateDirectories(Context, Destination, DestinationPath);
    DestinationFile = SetupFileOpen(Destination,
                                    DestinationPath,
                                    O_CREAT | O_TRUNC | O_RDWR | O_NOFOLLOW,
                                    Mode);

    if (DestinationFile == NULL) {
        fprintf(stderr,
                "Failed to create destination file %s.\n",
                DestinationPath);

        goto CreateAndWriteFileEnd;
    }

    //
    // Loop copying chunks.
    //

    TotalWritten = 0;
    while (TotalWritten != ContentsSize) {
        Written = SetupFileWrite(DestinationFile,
                                 Contents + TotalWritten,
                                 ContentsSize - TotalWritten);

        if (Written <= 0) {
            fprintf(stderr,
                    "Failed to write %s.\n",
                    DestinationPath);

            Status = errno;
            goto CreateAndWriteFileEnd;
        }

        TotalWritten += Written;
    }

CreateAndWriteFileEnd:
    if (DestinationFile != NULL) {
        SetupFileClose(DestinationFile);
    }

    return Status;
}

INT
SetupCreateDirectories (
    PSETUP_CONTEXT Context,
    PVOID Volume,
    PCSTR Path
    )

/*++

Routine Description:

    This routine creates directories up to but not including the final
    component of the given path.

Arguments:

    Context - Supplies a pointer to the applicaton context.

    Volume - Supplies a pointer to the open destination volume handle.

    Path - Supplies the full file path. The file itself won't be created, but
        all directories leading up to it will. If the path ends in a slash,
        all components will be created.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR Copy;
    ULONG Mode;
    PSTR Next;
    INT Status;

    Copy = strdup(Path);
    if (Copy == NULL) {
        return ENOMEM;
    }

    Status = 0;
    Mode = FILE_PERMISSION_USER_READ |
           FILE_PERMISSION_USER_WRITE |
           FILE_PERMISSION_USER_EXECUTE |
           FILE_PERMISSION_GROUP_READ |
           FILE_PERMISSION_GROUP_WRITE |
           FILE_PERMISSION_GROUP_EXECUTE |
           FILE_PERMISSION_OTHER_READ |
           FILE_PERMISSION_OTHER_EXECUTE;

    //
    // Get past any leading slashes.
    //

    Next = Copy;
    while (*Next == '/') {
        Next += 1;
    }

    while (TRUE) {
        while ((*Next != '\0') && (*Next != '/')) {
            Next += 1;
        }

        //
        // If the next character is the ending one, then this was the last
        // component. Don't create a directory for it.
        //

        if (*Next == '\0') {
            break;
        }

        //
        // Terminate the string and create the directory.
        //

        *Next = '\0';
        Status = SetupFileCreateDirectory(Volume, Copy, Mode);
        if (Status != 0) {
            fprintf(stderr,
                    "Error: Cannot create directories for path %s: %s.\n",
                    Copy,
                    strerror(Status));

            goto CreateDirectoriesEnd;
        }

        *Next = '/';
        while (*Next == '/') {
            Next += 1;
        }
    }

CreateDirectoriesEnd:
    if (Copy != NULL) {
        free(Copy);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

