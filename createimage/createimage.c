/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    createimage.c

Abstract:

    This module implements the createimage program, which creates a flat disk
    image from the boot block, kernel, and user programs.

Author:

    Evan Green 20-Jun-2012

Environment:

    Development

--*/

//
// ------------------------------------------------------------------- Includes
//

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <minoca/kernel.h>
#include <minoca/fat.h>
#include <minoca/partlib.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "createimage.h"

//
// ---------------------------------------------------------------- Definitions
//

#define CREATEIMAGE_VERSION_MAJOR 1
#define CREATEIMAGE_VERSION_MINOR 0

#define OUTPUT_IMAGE "image"
#define CREATEIMAGE_USAGE                                                      \
    "Usage: createimage [-achiv] [-o file] [-m mbr] [-y file] [-s size] "      \
    "[-r num,file] [-f format] [-p partition] [file...]\n"                     \
    "Createimage creates a bootable image based off of the given files.\n"     \
    "Options are:\n"                                                           \
    "  -a, --align-partitions -- Align partitions to 1MB. If not set, \n"      \
    "      partitions are only sector aligned.\n"                              \
    "  -b, --boot=num -- Set the given partition as the boot partition.\n"     \
    "  -c, --create -- Create the output even if it already exists.\n"         \
    "  -D, --debug=index -- Enable debugging in the output image, and \n"      \
    "      specifies the target device device index. Specify 0 to use the \n"  \
    "      first available debug device.\n"                                    \
    "  -E, --efi -- Set the loader path to EFI, even on MBR disks.\n"          \
    "  -f, --format=format -- Specify the output format. Valid values \n"      \
    "      are flat, vmdk, and vhd.\n"                                         \
    "  -g, --gpt -- Create a GPT formatted disk.\n"                            \
    "  -i, --ignore-missing -- Skip missing image files.\n"                    \
    "  -k, --kernel-command=line -- Specify the kernel command line.\n"        \
    "  -m, --mbr=file -- Specify an MBR file. The contents of this file \n"    \
    "      will be merged with the beginning of the disk.\n"                   \
    "  -n, --install=num -- Install to the given partition number.\n"          \
    "  -o, --output=file -- Specify the output file name.\n"                   \
    "  -p, --partition=format -- Specify the partition formatting. The \n"     \
    "      format is <type><offset>:<size>[*][:system_id]. See the \n"         \
    "      explanation of this format below.\n"                                \
    "  -r  --raw=num,file -- Specify a file to write at the beginning of \n"   \
    "      the partition indicated by the number.\n"                           \
    "  -s, --size=size -- Specify the size of the image in megabytes. If \n"   \
    "      not specified, a reasonable size will be estimated.\n"              \
    "  -S  --boot-short-names -- Specifies that short file names should be\n"  \
    "      allowed when creating the boot partition.\n"                        \
    "  -v, --verbose -- Output more information.\n"                            \
    "  -x, --vbr=file -- Specify a VBR file. The contents of this file will \n"\
    "      be merged with the beginning of the boot partition.\n"              \
    "  -y, --boot-file=file -- Specify a file to go on the boot partition.\n"  \
    "  -z  --min-size=min-size -- Specify the minimum size of the image in \n" \
    "      megabytes. If not specified, a reasonable size will be estimated.\n"\
    "  file -- Specify the files and directories to add to the image.\n"       \
    "  --help -- Print this help text and exit.\n"                             \
    "  --version -- Print the application version information and exit.\n\n"   \
    "The partition format takes the form <type>[offset]:[size][*][:type].\n"   \
    "Valid partition types are:\n"                                             \
    "  p -- Primary partition\n"                                               \
    "  e -- Extended partition\n"                                              \
    "  l -- Logical partition\n"                                               \
    "  b -- Blank partition (unallocated space)\n"                             \
    "For GPT formatted disks (the -g option), p is the only valid option.\n"   \
    "The offset and size parameters can be a byte count, or can have \n"       \
    "suffixes of K, M, G, or T for kilobytes, megabytes, gigabytes, and \n"    \
    "terabytes. If the offset is omitted, the next available space will be \n" \
    "used. If the size is omitted, an appropriate size will be estimated.\n"   \
    "The optional * indicates that this partition is bootable. This is \n"     \
    "ignored for GPT disks.\n"                                                 \
    "The type field can either be a numeric system ID byte, or one of the \n"  \
    "following characters:\n"                                                  \
    "  d -- FAT12 partition\n"                                                 \
    "  e -- EFI system partition\n"                                            \
    "  m -- Minoca partition\n"                                                \
    "  f -- FAT16 partition\n"                                                 \
    "  F -- FAT32 partition\n"                                                 \
    "Example: -p p512K:50M*:F,p: -- This creates a 50 megabyte boot \n"        \
    "partition at offset 512K, followed by a primary partition right after \n" \
    "it with a default size.\n"

#define CREATEIMAGE_OPTIONS_STRING "ab:cD:Ef:gik:m:n:o:p:r:s:Svx:y:z:"

#define SECTOR_SIZE 512

//
// Define the well known offsets of the boot sector where its LBA and size are
// stored.
//

#define BOOT_SECTOR_BLOCK_ADDRESS_OFFSET 0x5C
#define BOOT_SECTOR_BLOCK_LENGTH_OFFSET 0x60

//
// Define the amount of extra space that is added to the disk size for file
// system structures and general slop.
//

#define DISK_SIZE_FUDGE_NUMERATOR 3
#define DISK_SIZE_FUDGE_DENOMINATOR 2

#define HEAP_BEGIN_GUARD 0xABCD1234
#define HEAP_BEGIN_FREE  0xF4EEEEEE
#define HEAP_END_GUARD   0xEFDCBA98

#define ELF_MAGIC 0x464C457F
#define IMAGE_DOS_SIGNATURE 0x5A4D
#define SCRIPT_SHEBANG 0x2123

//
// Define away symbolic link functions on Windows.
//

#ifdef _WIN32

#define S_ISLNK(_Mode) 0
#define readlink(_Path, _Buffer, _BufferSize) (-1)
#define lstat stat
#define S_IXGRP 0

#endif

#define CREATEIMAGE_SYMLINK_SIZE 512

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
CreateImage (
    PCREATEIMAGE_CONTEXT Context,
    ULONG FileCount,
    PCHAR *Files
    );

BOOL
EstimateVolumeSize (
    PSTR *Files,
    ULONG FileCount,
    PULONGLONG Megabytes
    );

BOOL
EstimateItemSize (
    PSTR Path,
    PULONGLONG Size,
    PULONGLONG FileCount
    );

KSTATUS
WriteFileToDisk (
    PCREATEIMAGE_CONTEXT Context,
    PSTR FileName,
    ULONGLONG BlockAddress,
    BOOL WriteBlockAddress
    );

KSTATUS
InitializeDisk (
    FILE *File,
    ULONG BlockSize,
    ULONGLONG PartitionSize,
    BOOL Format,
    BOOL WritePartitionData,
    BOOL BootAllowShortFileNames,
    PCI_VOLUME *BootVolume,
    PCI_VOLUME *InstallVolume
    );

BOOL
AddItemToImage (
    PCI_VOLUME Volume,
    PSTR Path
    );

BOOL
AddRelativeItemToImage (
    PCI_VOLUME Volume,
    PSTR Prefix,
    PSTR Path
    );

BOOL
AddFileContentsToImage (
    PCI_VOLUME Volume,
    PSTR Path,
    PVOID FileContents,
    ULONGLONG FileSize,
    mode_t FileMode,
    time_t ModifiedTime,
    time_t AccessTime
    );

BOOL
AddVhdFooter (
    FILE *File,
    ULONGLONG BlockCount
    );

ULONG
CalculateVhdChecksum (
    PUCHAR Data,
    ULONG Size
    );

BOOL
MemoryMapFile (
    PCHAR Path,
    ULONGLONG FileSize,
    PVOID *Buffer
    );

BOOL
CipOpen (
    PCI_VOLUME Volume,
    PSTR Path,
    BOOL Create,
    BOOL Directory,
    PCI_HANDLE *Handle
    );

BOOL
CipPerformIo (
    PCI_HANDLE Handle,
    BOOL Write,
    PVOID Buffer,
    UINTN Size,
    PUINTN BytesCompleted
    );

PSTR
AppendPaths (
    PSTR Path1,
    PSTR Path2
    );

VOID
ConvertUnixTimeToSystemTime (
    PSYSTEM_TIME SystemTime,
    time_t UnixTime
    );

//
// -------------------------------------------------------------------- Globals
//

struct option CiLongOptions[] = {
    {"align-partitions", no_argument, 0, 'a'},
    {"boot", required_argument, 0, 'b'},
    {"create", no_argument, 0, 'c'},
    {"debug", required_argument, 0, 'D'},
    {"efi", no_argument, 0, 'E'},
    {"gpt", no_argument, 0, 'g'},
    {"ignore-missing", no_argument, 0, 'i'},
    {"kernel-command", required_argument, 0, 'k'},
    {"mbr", required_argument, 0, 'm'},
    {"install", required_argument, 0, 'n'},
    {"output", required_argument, 0, 'o'},
    {"size", required_argument, 0, 's'},
    {"boot-short-names", no_argument, 0, 'S'},
    {"min-size", required_argument, 0, 'z'},
    {"format", required_argument, 0, 'f'},
    {"partition", required_argument, 0, 'p'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {"verbose", no_argument, 0, 'v'},
    {"vbr", no_argument, 0, 'x'},
    {"boot-file", required_argument, 0, 'y'},
    {NULL, 0, 0, 0},
};

CREATEIMAGE_CONTEXT CiContext;
BOOL CiHeapChecking = FALSE;

PSTR CiExecutableSuffixes[] = {
    ".sh",
    ".py"
};

//
// ------------------------------------------------------------------ Functions
//

INT
main (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the program. It collects the
    options passed to it, and creates the output image.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR AfterScan;
    PCHAR Argument;
    INT ArgumentIndex;
    PVOID NewBuffer;
    size_t NewSize;
    INT Option;
    CREATEIMAGE_RAW_FILE RawFile;
    INT Result;
    KSTATUS Status;

    srand(time(NULL));
    memset(&CiContext, 0, sizeof(CREATEIMAGE_CONTEXT));
    CiContext.Output = OUTPUT_IMAGE;
    CiContext.Format = CreateimageFormatFlat;
    CiContext.BootPartitionNumber = -1;
    CiContext.InstallPartitionNumber = -1;
    Status = CiInitializePartitionSupport(&CiContext);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Process the command line options
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             CREATEIMAGE_OPTIONS_STRING,
                             CiLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Result = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'a':
            CiContext.Options |= CREATEIMAGE_OPTION_ALIGN_PARTITIONS;
            break;

        case 'b':
            Argument = optarg;
            CiContext.BootPartitionNumber = strtoul(Argument, &AfterScan, 0);
            if (AfterScan == Argument) {
                printf("Invalid partition number %s.\n", Argument);
                return 1;
            }

            break;

        case 'c':
            CiContext.Options |= CREATEIMAGE_OPTION_CREATE_ALWAYS;
            break;

        case 'D':
            Argument = optarg;
            CiContext.Options |= CREATEIMAGE_OPTION_TARGET_DEBUG;
            CiContext.DebugDeviceIndex = strtoul(Argument, &AfterScan, 10);
            if (AfterScan == Argument) {
                printf("Invalid debug device index (integer required): %s.\n",
                       Argument);

                return 1;
            }

            break;

        case 'E':
            CiContext.Options |= CREATEIMAGE_OPTION_EFI;
            break;

        //
        // Enabling GPT formatted disks turn on EFI automatically.
        //

        case 'g':
            CiContext.Options |= CREATEIMAGE_OPTION_GPT |
                                 CREATEIMAGE_OPTION_EFI;

            break;

        case 'i':
            CiContext.Options |= CREATEIMAGE_OPTION_IGNORE_MISSING;
            break;

        case 'k':
            CiContext.KernelCommandLine = optarg;
            break;

        case 'm':
            CiContext.MbrFile = optarg;
            break;

        case 'n':
            Argument = optarg;
            CiContext.InstallPartitionNumber = strtoul(Argument, &AfterScan, 0);
            if (AfterScan == Argument) {
                printf("Invalid partition number %s.\n", Argument);
                return 1;
            }

            break;

        case 'o':
            CiContext.Output = optarg;
            break;

        case 'p':
            Status = CiParsePartitionLayout(&CiContext, optarg);
            if (!KSUCCESS(Status)) {
                printf("Unable to parse partition layout %s.\n", optarg);
                return 1;
            }

            break;

        case 'r':
            Argument = optarg;
            RawFile.PartitionNumber = strtoul(Argument, &AfterScan, 0);
            if (AfterScan == Argument) {
                printf("Invalid raw file partition number %s.\n", Argument);
                return 1;
            }

            if (*AfterScan != ',') {
                printf("Invalid raw file partition format %s.\n", Argument);
                return 1;
            }

            RawFile.FileName = AfterScan + 1;
            NewSize = (CiContext.RawFileCount + 1) *
                      sizeof(CREATEIMAGE_RAW_FILE);

            NewBuffer = realloc(CiContext.RawFiles, NewSize);
            if (NewBuffer == NULL) {
                Result = 1;
                goto MainEnd;
            }

            CiContext.RawFiles = NewBuffer;
            memcpy(&(CiContext.RawFiles[CiContext.RawFileCount]),
                   &RawFile,
                   sizeof(CREATEIMAGE_RAW_FILE));

            CiContext.RawFileCount += 1;
            break;

        case 's':
            Argument = optarg;
            CiContext.DiskSize = (strtoull(Argument, &AfterScan, 0) * _1MB) /
                                 CREATEIMAGE_SECTOR_SIZE;

            if (AfterScan == Argument) {
                printf("Invalid image size '%s'.\n", Argument);
                return 1;
            }

            break;

        case 'S':
            CiContext.Options |= CREATEIMAGE_OPTION_BOOT_ALLOW_SHORT_FILE_NAMES;
            break;

        case 'z':
            Argument = optarg;
            CiContext.ImageMinimumSizeMegabytes = strtoul(Argument,
                                                          &AfterScan,
                                                          0);

            if (AfterScan == Argument) {
                printf("Invalid image minimum size '%s'.\n", Argument);
                return 1;
            }

            break;

        case 'f':
            Argument = optarg;
            if (strcasecmp(Argument, "flat") == 0) {
                CiContext.Format = CreateimageFormatFlat;

            } else if (strcasecmp(Argument, "vmdk") == 0) {
                CiContext.Format = CreateimageFormatVmdk;

            } else if (strcasecmp(Argument, "vhd") == 0) {
                CiContext.Format = CreateimageFormatVhd;

            } else {
                fprintf(stderr,
                        "createimage: Invalid disk format '%s'.\n",
                        Argument);

                Result = 1;
                goto MainEnd;
            }

            break;

        case 'x':
            CiContext.VbrFile = optarg;
            break;

        case 'y':
            Argument = optarg;
            NewBuffer = realloc(CiContext.BootFiles,
                                (CiContext.BootFileCount + 1) * sizeof(PVOID));

            if (NewBuffer == NULL) {
                Result = 1;
                goto MainEnd;
            }

            CiContext.BootFiles = NewBuffer;
            CiContext.BootFiles[CiContext.BootFileCount] = Argument;
            CiContext.BootFileCount += 1;
            break;

        case 'v':
            CiContext.Options |= CREATEIMAGE_OPTION_VERBOSE;
            break;

        case 'V':
            printf("createimage version %d.%d.%d.\n",
                   CREATEIMAGE_VERSION_MAJOR,
                   CREATEIMAGE_VERSION_MINOR,
                   REVISION);

            return 1;

        case 'h':
            printf(CREATEIMAGE_USAGE);
            return 1;

        default:

            assert(FALSE);

            Result = 1;
            goto MainEnd;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    //
    // Unless set directly via the command line, set the minimum image size
    // based on the environment variable.
    //

    if (CiContext.ImageMinimumSizeMegabytes == 0) {
        Argument = getenv("CI_MIN_IMAGE_SIZE");
        if (Argument != NULL) {
            CiContext.ImageMinimumSizeMegabytes = strtoul(Argument,
                                                          &AfterScan,
                                                          0);

            if (AfterScan == Argument) {
                printf("Invalid CI_MIN_IMAGE_SIZE '%s'.\n", Argument);
                return 1;
            }

            if ((CiContext.Options & CREATEIMAGE_OPTION_VERBOSE) != 0) {
                printf("Setting min image size to %d from "
                       "CI_MIN_IMAGE_SIZE.\n",
                       CiContext.ImageMinimumSizeMegabytes);
            }
        }
    }

    switch (CiContext.Format) {
    case CreateimageFormatFlat:
    case CreateimageFormatVmdk:
    case CreateimageFormatVhd:
        Result = CreateImage(&CiContext,
                             ArgumentCount - ArgumentIndex,
                             Arguments + ArgumentIndex);

        if (Result == FALSE) {
            fprintf(stderr, "createimage: Failed to create image.\n");
            Result = 1;
            goto MainEnd;
        }

        break;

    default:
        fprintf(stderr, "Unknown image format.\n");
        Result = 1;
        goto MainEnd;
    }

    Result = 0;

MainEnd:
    CiDestroyPartitionSupport(&CiContext);
    return Result;
}

PVOID
CiMalloc (
    size_t AllocationSize
    )

/*++

Routine Description:

    This routine allocates from the heap.

Arguments:

    AllocationSize - Supplies the size of the allocation in bytes.

Return Value:

    Returns a pointer to the allocation on success.

    NULL on failure.

--*/

{

    PULONG Allocation;
    PULONG End;

    if (CiHeapChecking == FALSE) {
        return malloc(AllocationSize);
    }

    Allocation = malloc(AllocationSize + 12);
    if (Allocation == NULL) {
        return NULL;
    }

    *Allocation = AllocationSize;
    *(Allocation + 1) = HEAP_BEGIN_GUARD;
    End = (PULONG)((PUCHAR)(Allocation + 2) + AllocationSize);
    *End = HEAP_END_GUARD;
    return Allocation + 2;
}

VOID
CiFree (
    PVOID Allocation
    )

/*++

Routine Description:

    This routine frees an allocation from the heap.

Arguments:

    Allocation - Supplies a pointer to the allocation.

Return Value:

    Returns a pointer to the allocation on success.

    NULL on failure.

--*/

{

    PULONG Check;
    PULONG End;

    if (CiHeapChecking == FALSE) {
        free(Allocation);
        return;
    }

    if (Allocation == NULL) {
        return;
    }

    Check = (PULONG)Allocation - 2;
    if (*(Check + 1) != HEAP_BEGIN_GUARD) {
        fprintf(stderr,
                "Heap allocation %x underwrote: Was %x, should be %x.\n",
                Allocation,
                *(Check + 1),
                HEAP_BEGIN_GUARD);

        assert(FALSE);
    }

    End = (PULONG)((PUCHAR)(Check + 2) + *Check);
    if (*End != HEAP_END_GUARD) {
        fprintf(stderr,
                "Heap allocation %x overwrite: Was %x, should be %x.\n",
                Allocation,
                *End,
                HEAP_END_GUARD);

        assert(FALSE);
    }

    *(Check + 1) = HEAP_BEGIN_FREE;
    free(Check);
    return;
}

PSTR
CiCopyString (
    PSTR String
    )

/*++

Routine Description:

    This routine allocates a copy of the string.

Arguments:

    String - Supplies a pointer to the string to copy.

Return Value:

    Returns a pointer to the allocation on success.

    NULL on failure.

--*/

{

    size_t Length;
    PSTR NewString;

    if (CiHeapChecking == FALSE) {
        return strdup(String);
    }

    Length = strlen(String) + 1;
    NewString = CiMalloc(Length);
    if (NewString == NULL) {
        return NULL;
    }

    memcpy(NewString, String, Length);
    return NewString;
}

BOOL
CiOpen (
    PCI_VOLUME Volume,
    PSTR Path,
    BOOL Create,
    PCI_HANDLE *Handle
    )

/*++

Routine Description:

    This routine opens a file on the target image.

Arguments:

    Volume - Supplies the mounted volume token.

    Path - Supplies a pointer to the complete path within the volume to open.

    Create - Supplies a boolean indicating if the file should be created if it
        does not exist. If this is TRUE and the file does exist, the call will
        fail.

    Handle - Supplies a pointer where the handle will be returned on success.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    return CipOpen(Volume, Path, Create, FALSE, Handle);
}

BOOL
CiCreateDirectory (
    PCI_VOLUME Volume,
    PSTR Path
    )

/*++

Routine Description:

    This routine creates a directory on the target image.

Arguments:

    Volume - Supplies the mounted volume token.

    Path - Supplies a pointer to the complete path within the volume to open.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PCI_HANDLE Handle;
    BOOL Result;

    Result = CipOpen(Volume, Path, TRUE, TRUE, &Handle);
    if (Result == FALSE) {
        fprintf(stderr, "createimage: Cannot create directory.\n");
        return FALSE;
    }

    CiClose(Handle);
    return TRUE;
}

VOID
CiClose (
    PCI_HANDLE Handle
    )

/*++

Routine Description:

    This routine closes an open handle on the target image.

Arguments:

    Handle - Supplies a pointer to the open handle.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    if (Handle == NULL) {
        return;
    }

    if (Handle->FileSystemHandle != NULL) {
        Status = FatWriteFileProperties(Handle->Volume,
                                        &(Handle->Properties),
                                        0);

        if (!KSUCCESS(Status)) {
            fprintf(stderr,
                    "createimage: Unable to write file properties: %x\n",
                    Status);
        }

        FatCloseFile(Handle->FileSystemHandle);
    }

    CiFree(Handle);
    return;
}

BOOL
CiRead (
    PCI_HANDLE Handle,
    PVOID Buffer,
    UINTN Size,
    PUINTN BytesCompleted
    )

/*++

Routine Description:

    This routine reads from a file on the target image.

Arguments:

    Handle - Supplies the open handle to the file.

    Buffer - Supplies the buffer where the read data will be returned.

    Size - Supplies the number of bytes to read.

    BytesCompleted - Supplies a pointer where the number of bytes read will be
        returned on success.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    return CipPerformIo(Handle, FALSE, Buffer, Size, BytesCompleted);
}

BOOL
CiWrite (
    PCI_HANDLE Handle,
    PVOID Buffer,
    UINTN Size,
    PUINTN BytesCompleted
    )

/*++

Routine Description:

    This routine writes to a file on the target image.

Arguments:

    Handle - Supplies the open handle to the file.

    Buffer - Supplies the buffer containing the data to write.

    Size - Supplies the number of bytes to write.

    BytesCompleted - Supplies a pointer where the number of bytes written will
        be returned on success.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    return CipPerformIo(Handle, TRUE, Buffer, Size, BytesCompleted);
}

BOOL
CiSetFileProperties (
    PCI_HANDLE Handle,
    IO_OBJECT_TYPE Type,
    FILE_PERMISSIONS Permissions,
    time_t ModificationTime,
    time_t AccessTime
    )

/*++

Routine Description:

    This routine sets the properties on the open file handle.

Arguments:

    Handle - Supplies the open handle to the file.

    Type - Supplies the file type.

    Permissions - Supplies the file permissions.

    ModificationTime - Supplies the modification time to set in the file.

    AccessTime - Supplies the last access time to set in the file.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    Handle->Properties.Type = Type;
    Handle->Properties.Permissions = Permissions;
    if (ModificationTime != 0) {
        ConvertUnixTimeToSystemTime(&(Handle->Properties.ModifiedTime),
                                    ModificationTime);
    }

    if (AccessTime != 0) {
        ConvertUnixTimeToSystemTime(&(Handle->Properties.AccessTime),
                                    AccessTime);
    }

    return TRUE;
}

KSTATUS
CiOpenVolume (
    PCREATEIMAGE_CONTEXT Context,
    PPARTITION_INFORMATION Partition,
    ULONG BlockSize,
    BOOL Format,
    BOOL AllowShortFileNames,
    PCI_VOLUME *Volume
    )

/*++

Routine Description:

    This routine opens a handle to a volume.

Arguments:

    Context - Supplies a pointer to the application context.

    Partition - Supplies a pointer to the partition to open. Supply NULL to
        open the disk directly.

    BlockSize - Supplies the size of a block on the disk.

    Format - Supplies a boolean indicating whether or not to format the
        partition or just open it.

    AllowShortFileNames - Supplies a boolean indicating whether or not to allow
        the creation of short file names on the volume.

    Volume - Supplies a pointer where a pointer to the volume will be returned
        on success.

Return Value:

    Status code.

--*/

{

    BLOCK_DEVICE_PARAMETERS BlockParameters;
    ULONG MountFlags;
    PCI_VOLUME NewVolume;
    KSTATUS Status;

    RtlZeroMemory(&BlockParameters, sizeof(BLOCK_DEVICE_PARAMETERS));
    if (Partition == NULL) {
        BlockParameters.BlockCount = Context->DiskSize;

    } else {
        BlockParameters.BlockCount =
                                 Partition->EndOffset - Partition->StartOffset;
    }

    NewVolume = CiMalloc(sizeof(CI_VOLUME));
    if (NewVolume == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto OpenVolumeEnd;
    }

    RtlZeroMemory(NewVolume, sizeof(CI_VOLUME));
    NewVolume->Context = Context;
    NewVolume->Partition = Partition;
    BlockParameters.DeviceToken = NewVolume;
    BlockParameters.BlockSize = BlockSize;
    if (Format != FALSE) {
        Status = FatFormat(&BlockParameters, 0, 0);
        if (!KSUCCESS(Status)) {
            goto OpenVolumeEnd;
        }
    }

    MountFlags = 0;
    if (AllowShortFileNames != FALSE) {
        MountFlags |= FAT_MOUNT_FLAG_COMPATIBILITY_MODE;
    }

    Status = FatMount(&BlockParameters,
                      MountFlags,
                      &(NewVolume->FileSystemHandle));

    if (!KSUCCESS(Status)) {
        goto OpenVolumeEnd;
    }

    Status = STATUS_SUCCESS;

OpenVolumeEnd:
    if (!KSUCCESS(Status)) {
        if (NewVolume != NULL) {
            CiFree(NewVolume);
            NewVolume = NULL;
        }
    }

    *Volume = NewVolume;
    return Status;
}

VOID
CiCloseVolume (
    PCI_VOLUME Volume
    )

/*++

Routine Description:

    This routine closes a handle to a volume.

Arguments:

    Volume - Supplies a pointer to the volume to close.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    Status = FatUnmount(Volume->FileSystemHandle);

    assert(KSUCCESS(Status));

    CiFree(Volume);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
CreateImage (
    PCREATEIMAGE_CONTEXT Context,
    ULONG FileCount,
    PCHAR *Files
    )

/*++

Routine Description:

    This routine creates an image that can be copied directly on to a hard
    disk or floppy.

Arguments:

    Context - Supplies a pointer to the application context.

    FileCount - Supplies the number of files to put into the image.

    Files - Supplies an array of strings that represent the file names of
           the files to put into the image.

Return Value:

    TRUE on success.

    FALSE otherwise.

--*/

{

    ULONGLONG BlockAddress;
    BOOL BootAllowShortFileNames;
    PCI_VOLUME BootVolume;
    ULONG FileIndex;
    PCHAR FileName;
    BOOL FormatDisk;
    PCI_VOLUME InstallVolume;
    ULONGLONG MainPartitionBlockCount;
    PSTR OpenMode;
    FILE *OutputFile;
    ULONG OutputLength;
    BOOL Result;
    KSTATUS Status;
    ULONGLONG VmdkCylinders;
    FILE *VmdkFile;
    PSTR VmdkFileName;
    ULONG VmdkFileNameSize;
    ULONG VmdkLongContentId[2];
    UCHAR VmdkUuid[8];
    ULONG VmdkUuidIndex;
    ULONGLONG VolumeSizeMegabytes;
    BOOL WriteBootSectorOffset;
    BOOL WritePartitionData;

    BootVolume = NULL;
    InstallVolume = NULL;
    VmdkFile = NULL;
    VmdkFileName = NULL;

    //
    // Start by opening the output file.
    //

    FormatDisk = FALSE;
    WritePartitionData = FALSE;
    OpenMode = "rb+";
    if ((Context->Options & CREATEIMAGE_OPTION_CREATE_ALWAYS) != 0) {
        FormatDisk = TRUE;
        WritePartitionData = TRUE;
        OpenMode = "wb+";
    }

    BootAllowShortFileNames = FALSE;
    if ((Context->Options &
         CREATEIMAGE_OPTION_BOOT_ALLOW_SHORT_FILE_NAMES) != 0) {

        BootAllowShortFileNames = TRUE;
    }

    OutputFile = fopen(Context->Output, OpenMode);
    if (OutputFile == NULL) {
        printf("Unable to open output file \"%s\" for write.\n",
               Context->Output);

        Status = STATUS_UNSUCCESSFUL;
        goto CreateImageEnd;
    }

    Context->OutputFile = OutputFile;

    //
    // Determine the size of all files that will be put on the disk. Estimate
    // no matter what to get the complete file count.
    //

    Result = EstimateVolumeSize(Files, FileCount, &VolumeSizeMegabytes);
    if (Result == FALSE) {
        printf("Failed to estimate volume size.\n");
        Status = STATUS_UNSUCCESSFUL;
        goto CreateImageEnd;
    }

    if ((Context->ImageMinimumSizeMegabytes != 0) &&
        (VolumeSizeMegabytes < Context->ImageMinimumSizeMegabytes)) {

        VolumeSizeMegabytes = Context->ImageMinimumSizeMegabytes;
    }

    //
    // If the volume is bigger than the disk, chop the volume down to the disk
    // size minus 2MB.
    //

    MainPartitionBlockCount = VolumeSizeMegabytes *
                              _1MB / CREATEIMAGE_SECTOR_SIZE;

    if ((Context->DiskSize != 0) &&
        (MainPartitionBlockCount >= Context->DiskSize)) {

        MainPartitionBlockCount =
                      Context->DiskSize - (2 * _1MB / CREATEIMAGE_SECTOR_SIZE);
    }

    //
    // Create the disk.
    //

    Status = InitializeDisk(OutputFile,
                            CREATEIMAGE_SECTOR_SIZE,
                            MainPartitionBlockCount,
                            FormatDisk,
                            WritePartitionData,
                            BootAllowShortFileNames,
                            &BootVolume,
                            &InstallVolume);

    if (!KSUCCESS(Status)) {
        printf("Error: Could not initialize disk. Status = 0x%x.\n", Status);
        goto CreateImageEnd;
    }

    //
    // Set the boot volume equal to the install volume if none was specified.
    //

    if (BootVolume == NULL) {
        BootVolume = InstallVolume;
    }

    //
    // Write out the MBR code. If it's a partitionless disk, then write out
    // the boot code offset (as it's a VBR that's being written to the MBR).
    //

    if (Context->MbrFile != NULL) {
        WriteBootSectorOffset = FALSE;
        if (Context->PartitionContext.PartitionCount == 0) {
            WriteBootSectorOffset = TRUE;
        }

        Status = WriteFileToDisk(Context,
                                 Context->MbrFile,
                                 0,
                                 WriteBootSectorOffset);

        if (!KSUCCESS(Status)) {
            printf("Error: Failed to write MBR: 0x%08x.\n", Status);
            goto CreateImageEnd;
        }
    }

    //
    // Write out the VBR code.
    //

    if (Context->VbrFile != NULL) {
        if (Context->BootPartition == NULL) {
            printf("Error: VBR was specified on a partitionless system.\n");
            Status = STATUS_INVALID_CONFIGURATION;
            goto CreateImageEnd;
        }

        BlockAddress = 0;
        Status = PartTranslateIo(Context->BootPartition,
                                 &BlockAddress,
                                 NULL);

        if (!KSUCCESS(Status)) {
            goto CreateImageEnd;
        }

        Status = WriteFileToDisk(Context, Context->VbrFile, BlockAddress, TRUE);
        if (!KSUCCESS(Status)) {
            printf("Error: Failed to write VBR: %x.\n", Status);
            goto CreateImageEnd;
        }
    }

    //
    // Write out all other files.
    //

    for (FileIndex = 0; FileIndex < FileCount; FileIndex += 1) {
        FileName = Files[FileIndex];
        Result = AddItemToImage(InstallVolume, FileName);
        if (Result == FALSE) {
            fprintf(stderr, "createimage: Failed to add '%s'.\n", FileName);
            Status = STATUS_UNSUCCESSFUL;
            goto CreateImageEnd;
        }
    }

    //
    // Write out the boot files.
    //

    for (FileIndex = 0; FileIndex < Context->BootFileCount; FileIndex += 1) {
        FileName = Context->BootFiles[FileIndex];
        Result = AddItemToImage(BootVolume, FileName);
        if (Result == FALSE) {
            fprintf(stderr,
                    "createimage: Failed to add boot file '%s'.\n",
                    FileName);

            Status = STATUS_UNSUCCESSFUL;
            goto CreateImageEnd;
        }
    }

    //
    // Create the boot configuration file.
    //

    if (Context->PartitionContext.PartitionCount != 0) {
        Status = CiCreateBootConfigurationFile(BootVolume, Context);
        if (!KSUCCESS(Status)) {
            goto CreateImageEnd;
        }
    }

    //
    // Write out the raw files to the beginning of the requested partitions.
    //

    for (FileIndex = 0; FileIndex < Context->RawFileCount; FileIndex += 1) {
        FileName = Context->RawFiles[FileIndex].FileName;
        BlockAddress = 0;
        Status = PartTranslateIo(Context->RawFiles[FileIndex].Partition,
                                 &BlockAddress,
                                 NULL);

        if (!KSUCCESS(Status)) {
            goto CreateImageEnd;
        }

        Status = WriteFileToDisk(Context, FileName, BlockAddress, FALSE);
        if (!KSUCCESS(Status)) {
            printf("Error: Failed to write raw file: %x.\n", Status);
            goto CreateImageEnd;
        }
    }

    //
    // Add the VHD footer if requested.
    //

    if (Context->Format == CreateimageFormatVhd) {
        Result = AddVhdFooter(OutputFile, Context->DiskSize);
        if (Result == FALSE) {
            fprintf(stderr, "createimage: Failed to add VHD Footer.\n");
            Status = STATUS_UNSUCCESSFUL;
            goto CreateImageEnd;
        }

    //
    // Create the VMDK file if requested.
    //

    } else if (Context->Format == CreateimageFormatVmdk) {
        OutputLength = strlen(Context->Output);
        VmdkFileNameSize = OutputLength + sizeof(".vmdk") + 1;
        VmdkFileName = CiMalloc(VmdkFileNameSize);
        if (VmdkFileName == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CreateImageEnd;
        }

        snprintf(VmdkFileName, VmdkFileNameSize, "%s.vmdk", Context->Output);

        //
        // VMDKs are written with unix-style line endings even on Windows, so
        // open the stream in binary mode.
        //

        VmdkFile = fopen(VmdkFileName, "wb");
        if (VmdkFile == NULL) {
            fprintf(stderr,
                    "creatimage: Unable to open %s: %s.\n",
                    VmdkFileName,
                    strerror(errno));

            Status = STATUS_UNSUCCESSFUL;
            goto CreateImageEnd;
        }

        //
        // Calculate the number of cylinders in this disk, and generate random
        // IDs for this disk.
        //

        VmdkCylinders = Context->DiskSize / (16 * 63);
        if ((Context->DiskSize % (16 * 63)) != 0) {
            VmdkCylinders += 1;
        }

        VmdkLongContentId[0] = (rand() ^ rand() << 16);
        VmdkLongContentId[1] = (rand() ^ rand() << 16);
        for (VmdkUuidIndex = 0; VmdkUuidIndex < 8; VmdkUuidIndex += 1) {
            VmdkUuid[VmdkUuidIndex] = rand();
        }

        //
        // Write out the text file.
        //

        fprintf(VmdkFile,
                VMDK_FORMAT_STRING,
                Context->DiskSize,
                Context->Output,
                VmdkLongContentId[0],
                VmdkLongContentId[1],
                VmdkUuid[0],
                VmdkUuid[1],
                VmdkUuid[2],
                VmdkUuid[3],
                VmdkUuid[4],
                VmdkUuid[5],
                VmdkUuid[6],
                VmdkUuid[7],
                VmdkCylinders);
    }

    printf("\nWrote %s, %I64d MB, %I64d files.\n",
           Context->Output,
           (Context->DiskSize * CREATEIMAGE_SECTOR_SIZE) / _1MB,
           Context->FileCount);

    Status = STATUS_SUCCESS;

CreateImageEnd:
    if ((BootVolume != NULL) && (BootVolume != InstallVolume)) {
        CiCloseVolume(BootVolume);
    }

    if (InstallVolume != NULL) {
        CiCloseVolume(InstallVolume);
    }

    if (OutputFile != NULL) {
        fclose(OutputFile);
        Context->OutputFile = NULL;
    }

    if (VmdkFileName != NULL) {
        CiFree(VmdkFileName);
    }

    if (VmdkFile != NULL) {
        fclose(VmdkFile);
    }

    if (!KSUCCESS(Status)) {
        return FALSE;
    }

    return TRUE;
}

BOOL
EstimateVolumeSize (
    PSTR *Files,
    ULONG FileCount,
    PULONGLONG Megabytes
    )

/*++

Routine Description:

    This routine creates an estimate for the volume size given an array of
    items that will be added to it.

Arguments:

    Files - Supplies an array of files or directories.

    FileCount - Supplies the number of elements in the array.

    Megabytes - Supplies a pointer where the disk size in megabytes will be
        returned on success.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG FileIndex;
    ULONGLONG ItemFileCount;
    BOOL Result;
    ULONGLONG Size;
    ULONGLONG TotalFileCount;

    Size = 0;
    TotalFileCount = 0;
    for (FileIndex = 0; FileIndex < FileCount; FileIndex += 1) {
        Result = EstimateItemSize(Files[FileIndex], &Size, &ItemFileCount);
        if (Result == FALSE) {
            goto EstimateDiskSizeEnd;
        }

        TotalFileCount += ItemFileCount;
    }

    CiContext.FileCount = TotalFileCount;

    //
    // Add a fudge factor for file system metadata.
    //

    Result = TRUE;
    Size = (Size * DISK_SIZE_FUDGE_NUMERATOR) / DISK_SIZE_FUDGE_DENOMINATOR;
    if (Size == 0) {
        Result = FALSE;
    }

EstimateDiskSizeEnd:
    if (Result == FALSE) {
        Size = 0;
    }

    *Megabytes = ALIGN_RANGE_UP(Size, _1MB) / _1MB;
    return Result;
}

BOOL
EstimateItemSize (
    PSTR Path,
    PULONGLONG Size,
    PULONGLONG FileCount
    )

/*++

Routine Description:

    This routine estimates the size of the given file or directory.

Arguments:

    Path - Supplies a pointer to the string containing the path.

    Size - Supplies a pointer where the size will be returned on success. The
        amount needed will be added to the value passed in here, not replaced.

    FileCount - Supplies a pointer where the number of files will be returned.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSTR AppendedPath;
    ULONGLONG Count;
    DIR *Directory;
    struct dirent *Entry;
    ULONGLONG EntryFileCount;
    INT Result;
    struct stat Stat;

    Count = 0;
    Directory = NULL;
    Result = stat(Path, &Stat);
    if (Result < 0) {
        if (((errno == ENOENT) || (errno == 0)) &&
            (CiContext.Options & CREATEIMAGE_OPTION_IGNORE_MISSING) != 0) {

            Result = TRUE;

        } else {
            fprintf(stderr,
                    "createimage: Unable to stat %s: %s.\n",
                    Path,
                    strerror(errno));

            Result = FALSE;
        }

        goto EstimateItemSizeEnd;
    }

    Result = TRUE;
    if (S_ISREG(Stat.st_mode)) {
        *Size += Stat.st_size;
        Count += 1;

    } else if (S_ISDIR(Stat.st_mode)) {
        Directory = opendir(Path);
        if (Directory == NULL) {
            fprintf(stderr,
                    "createimage: Unable to open directory %s: %s.\n",
                    Path,
                    strerror(errno));

            Result = FALSE;
            goto EstimateItemSizeEnd;
        }

        //
        // Loop reading directory entries.
        //

        while (TRUE) {
            errno = 0;
            Entry = readdir(Directory);
            if (Entry == NULL) {
                Result = TRUE;
                if (errno != 0) {
                    fprintf(stderr,
                            "createimage: Unable to read directory %s: %s.\n",
                            Path,
                            strerror(errno));

                    Result = FALSE;
                    goto EstimateItemSizeEnd;
                }

                break;
            }

            //
            // Skip the . and .. directories.
            //

            if ((strcmp(Entry->d_name, ".") == 0) ||
                (strcmp(Entry->d_name, "..") == 0)) {

                continue;
            }

            //
            // Create an appended path.
            //

            AppendedPath = AppendPaths(Path, Entry->d_name);
            if (AppendedPath == NULL) {
                Result = FALSE;
                goto EstimateItemSizeEnd;
            }

            //
            // Recurse into the next entry.
            //

            Result = EstimateItemSize(AppendedPath, Size, &EntryFileCount);
            CiFree(AppendedPath);
            if (Result == FALSE) {
                goto EstimateItemSizeEnd;
            }

            Count += EntryFileCount;
        }
    }

EstimateItemSizeEnd:
    if (Directory != NULL) {
        closedir(Directory);
    }

    *FileCount = Count;
    return Result;
}

KSTATUS
WriteFileToDisk (
    PCREATEIMAGE_CONTEXT Context,
    PSTR FileName,
    ULONGLONG BlockAddress,
    BOOL WriteBlockAddress
    )

/*++

Routine Description:

    This routine writes the total size of the given file to disk.

Arguments:

    Context - Supplies a pointer to the createimage context.

    FileName - Supplies a pointer to the name of the file containing the
        contents to write to disk.

    BlockAddress - Supplies the block address of the sector at which to start
        the write.

    WriteBlockAddress - Supplies a boolean indicating if the block address and
        size of the boot code should be written into the boot code.

Return Value:

    Status code.

--*/

{

    PULONG BlockAddressPointer;
    ULONG BlockCount;
    ULONG BlockSize;
    PUCHAR BootCodeSizePointer;
    ULONG BufferSize;
    ULONG Byte;
    ULONG BytesRead;
    ULONG BytesWritten;
    PUCHAR DiskData;
    FILE *DiskFile;
    PUCHAR FileData;
    PVOID MappedFile;
    ULONGLONG Offset;
    INT Result;
    struct stat Stat;
    KSTATUS Status;

    BlockSize = Context->PartitionContext.BlockSize;

    assert(BlockSize != 0);

    DiskFile = Context->OutputFile;
    DiskData = NULL;
    MappedFile = NULL;

    //
    // Load the file.
    //

    Result = stat(FileName, &Stat);
    if (Result != 0) {
        printf("Error: Unable to stat %s: %s.\n", FileName, strerror(errno));
        Status = STATUS_UNSUCCESSFUL;
        goto WriteFileToDisk;
    }

    BufferSize = Stat.st_size;
    if (BufferSize == 0) {
        Status = STATUS_SUCCESS;
        goto WriteFileToDisk;
    }

    if (MemoryMapFile(FileName, BufferSize, &MappedFile) == FALSE) {
        printf("Error: Unable to read %s.\n", FileName);
        Status = STATUS_UNSUCCESSFUL;
        goto WriteFileToDisk;
    }

    FileData = (PUCHAR)MappedFile;

    //
    // Write in the offset and size if requested. This is used when writing
    // boot code to disk.
    //

    if (WriteBlockAddress != FALSE) {
        if (BlockAddress > MAX_ULONG) {
            printf("Error: Boot code is too high at sector 0x%I64x.\n",
                   BlockAddress);

            Status = STATUS_INVALID_CONFIGURATION;
            goto WriteFileToDisk;
        }

        BlockCount = ALIGN_RANGE_UP(BufferSize, CREATEIMAGE_SECTOR_SIZE) /
                     CREATEIMAGE_SECTOR_SIZE;

        if (BlockCount > MAX_UCHAR) {
            printf("Error: Boot code is too big at %d sectors. Max is %d.\n",
                   BlockCount,
                   MAX_UCHAR);

            Status = STATUS_BUFFER_OVERRUN;
            goto WriteFileToDisk;
        }

        BlockAddressPointer =
                         (PULONG)(FileData + BOOT_SECTOR_BLOCK_ADDRESS_OFFSET);

        if (*BlockAddressPointer != 0) {
            printf("Error: Location for boot sector LBA had %x in it.\n",
                   *BlockAddressPointer);

            Status = STATUS_FILE_CORRUPT;
            goto WriteFileToDisk;
        }

        *BlockAddressPointer = (ULONG)BlockAddress;
        BootCodeSizePointer = FileData + BOOT_SECTOR_BLOCK_LENGTH_OFFSET;
        if (*BootCodeSizePointer != 0) {
            printf("Error: Location for boot sector size had %x in "
                   "it.\n",
                   *BootCodeSizePointer);

            Status = STATUS_FILE_CORRUPT;
            goto WriteFileToDisk;
        }

        *BootCodeSizePointer = (UCHAR)BlockCount;
    }

    //
    // Read in the disk blocks that are already there.
    //

    DiskData = CiMalloc(BufferSize);
    if (DiskData == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto WriteFileToDisk;
    }

    Offset = BlockAddress * BlockSize;
    fseek(DiskFile, Offset, SEEK_SET);
    BytesRead = fread(DiskData, 1, BufferSize, DiskFile);
    if (BytesRead != BufferSize) {
        printf("Error: Unable to read from output image!\n");
        Status = STATUS_END_OF_FILE;
        goto WriteFileToDisk;
    }

    //
    // Merge the boot sector and the current contents of the disk. Complain if
    // both have a non-zero byte there.
    //

    for (Byte = 0; Byte < BufferSize; Byte += 1) {
        if ((DiskData[Byte] != 0) && (FileData[Byte] != 0) &&
            (DiskData[Byte] != FileData[Byte])) {

            printf("Warning: Byte %d has contents both on the disk and in "
                   "%s. Disk has 0x%02x (%c), boot code has 0x%02x (%c).\n",
                   Byte,
                   FileName,
                   DiskData[Byte],
                   DiskData[Byte],
                   FileData[Byte],
                   FileData[Byte]);
        }

        if (FileData[Byte] != 0) {
            DiskData[Byte] = FileData[Byte];
        }
    }

    //
    // Write the completed data out to disk.
    //

    fseek(DiskFile, Offset, SEEK_SET);
    BytesWritten = fwrite(DiskData, 1, BufferSize, DiskFile);
    if (BytesWritten != BufferSize) {
        printf("Error writing to output image.\n");
        Status = STATUS_UNSUCCESSFUL;
        goto WriteFileToDisk;
    }

    Status = STATUS_SUCCESS;

WriteFileToDisk:
    if (MappedFile != NULL) {
        CiFree(MappedFile);
    }

    if (DiskData != NULL) {
        CiFree(DiskData);
    }

    return Status;
}

KSTATUS
InitializeDisk (
    FILE *File,
    ULONG BlockSize,
    ULONGLONG PartitionSize,
    BOOL Format,
    BOOL WritePartitionData,
    BOOL BootAllowShortFileNames,
    PCI_VOLUME *BootVolume,
    PCI_VOLUME *InstallVolume
    )

/*++

Routine Description:

    This routine initializes, potentially formats, and mounts a disk image.

Arguments:

    File - Supplies an open handle to the output file, the disk image to be
        created.

    BlockSize - Supplies the size of one sector on the disk.

    PartitionSize - Supplies the size of the install partition, if partition
        information is provided.

    Format - Supplies a boolean indicating whether or not the disk should be
        formatted.

    WritePartitionData - Supplies a boolean indicating whether the partition
        information should be written or not.

    BootAllowShortFileNames - Supplies a boolean indicating whether the boot
        partition should be mounted to allow the creation of short file names.

    BootVolume - Supplies a pointer where a pointer to the open boot volume
        will be returned. This may be NULL.

    InstallVolume - Supplies a pointer where a pointer to the open install
        volume will be returned.

Return Value:

    Status code.

--*/

{

    BOOL InstallAllowShortFileNames;
    KSTATUS Status;

    *BootVolume = NULL;
    *InstallVolume = NULL;
    if (WritePartitionData != FALSE) {

        //
        // Make sure there's at least a sector there.
        //

        fseeko64(File, BlockSize, SEEK_SET);
        if (fgetc(File) == EOF) {
            fputc('\0', File);
        }

        //
        // Write the partition information.
        //

        Status = CiWritePartitionLayout(&CiContext, PartitionSize);
        if (!KSUCCESS(Status)) {
            printf("Error: Failed to write the partition layout.\n");
            return STATUS_UNSUCCESSFUL;
        }

        //
        // Write the last byte on the disk to ensure that a file of that size is
        // created.
        //

        fseeko64(File, (BlockSize * CiContext.DiskSize) - 1, SEEK_SET);

        assert(ftello64(File) == (BlockSize * CiContext.DiskSize) - 1);

        if (fgetc(File) == EOF) {
            fputc('\0', File);
        }
    }

    Status = CiBindToPartitions(&CiContext, CiContext.DiskSize);
    if (!KSUCCESS(Status)) {
        printf("Error: Unable to bind to partition: %x.\n", Status);
        return Status;
    }

    //
    // If there is no separate boot partition, then the install partition acts
    // as the boot partition.
    //

    InstallAllowShortFileNames = FALSE;
    if ((CiContext.BootPartition == CiContext.InstallPartition) ||
        (CiContext.BootPartition == NULL)) {

        InstallAllowShortFileNames = BootAllowShortFileNames;
    }

    Status = CiOpenVolume(&CiContext,
                          CiContext.InstallPartition,
                          BlockSize,
                          Format,
                          InstallAllowShortFileNames,
                          InstallVolume);

    if (!KSUCCESS(Status)) {
        goto InitializeDiskEnd;
    }

    if ((CiContext.BootPartition != CiContext.InstallPartition) &&
        (CiContext.BootPartition != NULL)) {

        Status = CiOpenVolume(&CiContext,
                              CiContext.BootPartition,
                              BlockSize,
                              Format,
                              BootAllowShortFileNames,
                              BootVolume);

        if (!KSUCCESS(Status)) {
            goto InitializeDiskEnd;
        }
    }

InitializeDiskEnd:
    if (!KSUCCESS(Status)) {
        if (*BootVolume != NULL) {
            CiCloseVolume(*BootVolume);
            *BootVolume = NULL;
        }

        if (*InstallVolume != NULL) {
            CiCloseVolume(*InstallVolume);
            *InstallVolume = NULL;
        }
    }

    return Status;
}

BOOL
AddItemToImage (
    PCI_VOLUME Volume,
    PSTR Path
    )

/*++

Routine Description:

    This routine writes a file or directory out to the disk image.

Arguments:

    Volume - Supplies the mounted volume token.

    Path - Supplies the path of the file or directory to add.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSTR FileName;
    PSTR FileNameCopy;
    PSTR Prefix;
    INT Result;

    Result = FALSE;

    //
    // Split the item into a filename and host directory path.
    //

    FileNameCopy = CiCopyString(Path);
    if (FileNameCopy == NULL) {
        goto AddItemToImageEnd;
    }

    FileName = basename(FileNameCopy);
    Prefix = dirname(FileNameCopy);
    if ((FileName == NULL) || (Prefix == NULL)) {
        fprintf(stderr,
                "createimage: Unable to split path '%s', got '%s' and '%s'.\n",
                FileNameCopy,
                FileName,
                Prefix);

       goto AddItemToImageEnd;
    }

    Result = AddRelativeItemToImage(Volume, Prefix, FileName);
    if (Result == FALSE) {
        goto AddItemToImageEnd;
    }

AddItemToImageEnd:
    if (FileNameCopy != NULL) {
        CiFree(FileNameCopy);
    }

    return Result;
}

BOOL
AddRelativeItemToImage (
    PCI_VOLUME Volume,
    PSTR Prefix,
    PSTR Path
    )

/*++

Routine Description:

    This routine writes a file or directory out to the disk image, stripping
    the given prefix off the host.

Arguments:

    Volume - Supplies the mounted volume token.

    Prefix - Supplies the host-only base of the path to add.

    Path - Supplies the common portion of the path to add.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSTR AppendedPath;
    PSTR CompleteHostPath;
    DIR *Directory;
    struct dirent *Entry;
    PVOID FileBuffer;
    PSTR LinkBuffer;
    INT Result;
    struct stat Stat;

    AppendedPath = NULL;
    Directory = NULL;
    FileBuffer = NULL;
    LinkBuffer = NULL;

    //
    // Combine the two strings to get the complete host path.
    //

    CompleteHostPath = AppendPaths(Prefix, Path);
    if (CompleteHostPath == NULL) {
        Result = FALSE;
        goto AddRelativeItemToImageEnd;
    }

    errno = 0;
    Result = lstat(CompleteHostPath, &Stat);
    if (Result < 0) {
        if (((errno == ENOENT) || (errno == 0)) &&
            ((CiContext.Options & CREATEIMAGE_OPTION_IGNORE_MISSING) != 0)) {

            if ((CiContext.Options & CREATEIMAGE_OPTION_VERBOSE) != 0) {
                printf("createimage: Skipping non-existant file '%s'.\n", Path);
            }

            Result = TRUE;

        } else {
            fprintf(stderr,
                    "createimage: unable to stat '%s': %s.\n",
                    Path,
                    strerror(errno));

            Result = FALSE;
        }

        goto AddRelativeItemToImageEnd;
    }

    Result = TRUE;

    //
    // Write the file out if it's a regular file.
    //

    if (S_ISREG(Stat.st_mode)) {
        Result = MemoryMapFile(CompleteHostPath, Stat.st_size, &FileBuffer);
        if (Result == FALSE) {
            goto AddRelativeItemToImageEnd;
        }

        Result = AddFileContentsToImage(Volume,
                                        Path,
                                        FileBuffer,
                                        Stat.st_size,
                                        Stat.st_mode,
                                        Stat.st_mtime,
                                        Stat.st_atime);

        if (Result == FALSE) {
            goto AddRelativeItemToImageEnd;
        }

    } else if (S_ISLNK(Stat.st_mode)) {
        LinkBuffer = CiMalloc(CREATEIMAGE_SYMLINK_SIZE);
        if (LinkBuffer == NULL) {
            Result = FALSE;
            goto AddRelativeItemToImageEnd;
        }

        Result = readlink(CompleteHostPath,
                          LinkBuffer,
                          CREATEIMAGE_SYMLINK_SIZE);

        if (Result < 0) {
            fprintf(stderr,
                    "createimage: Cannot read link %s: %s\n",
                    CompleteHostPath,
                    strerror(errno));

            Result = FALSE;
            goto AddRelativeItemToImageEnd;
        }

        Result = AddFileContentsToImage(Volume,
                                        Path,
                                        LinkBuffer,
                                        Stat.st_size,
                                        Stat.st_mode,
                                        Stat.st_mtime,
                                        Stat.st_atime);

        if (Result == FALSE) {
            goto AddRelativeItemToImageEnd;
        }

    } else if (S_ISDIR(Stat.st_mode)) {
        Directory = opendir(CompleteHostPath);
        if (Directory == NULL) {
            fprintf(stderr,
                    "createimage: Unable to open directory %s: %s.\n",
                    Path,
                    strerror(errno));

            Result = FALSE;
            goto AddRelativeItemToImageEnd;
        }

        Result = CiCreateDirectory(Volume, Path);
        if (Result == FALSE) {
            fprintf(stderr,
                    "createimage: Unable to create directory %s in target "
                    "image.\n",
                    Path);

            Result = FALSE;
            goto AddRelativeItemToImageEnd;
        }

        if ((CiContext.Options & CREATEIMAGE_OPTION_VERBOSE) != 0) {
            printf("%8s %s\n", "<dir>", Path);
        }

        //
        // Loop creating directory entries.
        //

        while (TRUE) {
            errno = 0;
            Entry = readdir(Directory);
            if (Entry == NULL) {
                Result = TRUE;
                if (errno != 0) {
                    fprintf(stderr,
                            "createimage: Unable to read directory %s: %s.\n",
                            Path,
                            strerror(errno));

                    Result = FALSE;
                    goto AddRelativeItemToImageEnd;
                }

                break;
            }

            //
            // Skip the . and .. directories.
            //

            if ((strcmp(Entry->d_name, ".") == 0) ||
                (strcmp(Entry->d_name, "..") == 0)) {

                continue;
            }

            //
            // Create an appended path.
            //

            AppendedPath = AppendPaths(Path, Entry->d_name);
            if (AppendedPath == NULL) {
                Result = FALSE;
                goto AddRelativeItemToImageEnd;
            }

            //
            // Recurse into the next entry.
            //

            Result = AddRelativeItemToImage(Volume, Prefix, AppendedPath);
            CiFree(AppendedPath);
            if (Result == FALSE) {
                goto AddRelativeItemToImageEnd;
            }
        }
    }

AddRelativeItemToImageEnd:
    if (LinkBuffer != NULL) {
        CiFree(LinkBuffer);
    }

    if (CompleteHostPath != NULL) {
        CiFree(CompleteHostPath);
    }

    if (FileBuffer != NULL) {
        CiFree(FileBuffer);
    }

    if (Directory != NULL) {
        closedir(Directory);
    }

    return Result;
}

BOOL
AddFileContentsToImage (
    PCI_VOLUME Volume,
    PSTR Path,
    PVOID FileContents,
    ULONGLONG FileSize,
    mode_t FileMode,
    time_t ModifiedTime,
    time_t AccessTime
    )

/*++

Routine Description:

    This routine adds the given file contents to the target image.

Arguments:

    Volume - Supplies the mounted volume token.

    Path - Supplies the target path.

    FileContents - Supplies the contents of the buffer.

    FileSize - Supplies the number of bytes in the file and buffer.

    FileMode - Supplies the file permission information.

    ModifiedTime - Supplies the modification time of the file.

    AccessTime - Supplies the access time of the file.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    UINTN BytesCompleted;
    FILE_PERMISSIONS FilePermissions;
    IO_OBJECT_TYPE FileType;
    PULONG FirstValue;
    PCI_HANDLE Handle;
    ULONG HumanSize;
    CHAR HumanSizeSuffix;
    INT PathLength;
    ULONG Percent;
    ULONG PreviousPercent;
    INT Result;
    INT SuffixCount;
    INT SuffixIndex;
    INT SuffixLength;

    Handle = NULL;

    //
    // Print the banner for this file.
    //

    HumanSize = FileSize;
    HumanSizeSuffix = 0;
    if (HumanSize > 1024) {
        if (HumanSize > _1MB) {

            //
            // Deal with gigabytes.
            //

            if (HumanSize > 1024 * (ULONG)_1MB) {
                HumanSize = HumanSize / ((1024 * _1MB) / 10);
                HumanSizeSuffix = 'G';

            //
            // Deal with megabytes.
            //

            } else {
                HumanSize = HumanSize / (_1MB / 10);
                HumanSizeSuffix = 'M';
            }

        //
        // Deal with kilobytes.
        //

        } else {
            HumanSize = (HumanSize  * 10) / 1024;
            HumanSizeSuffix = 'K';
        }
    }

    //
    // If there's a suffix, then print out a decimal point if it's less than
    // 10 (so it would print 7.4K, but not 744.4K. If there is no suffix, print
    // the raw byte count.
    //

    if ((CiContext.Options & CREATEIMAGE_OPTION_VERBOSE) != 0) {
        if (HumanSizeSuffix != 0) {
            if (HumanSize < (10 * 10)) {
                printf("%5d.%d%c %s\n",
                       HumanSize / 10,
                       HumanSize % 10,
                       HumanSizeSuffix,
                       Path);

            } else {
                printf("%7d%c %s\n", HumanSize / 10, HumanSizeSuffix, Path);
            }

        } else {
            printf("%8d %s\n", HumanSize, Path);
        }

    } else if (Volume->Partition == CiContext.InstallPartition) {

        //
        // Print out a little percentage indicator for fun.
        //

        PreviousPercent = CiContext.FilesWritten * 100ULL / CiContext.FileCount;
        CiContext.FilesWritten += 1;
        Percent = CiContext.FilesWritten * 100ULL / CiContext.FileCount;
        while (PreviousPercent != Percent) {
            PreviousPercent += 1;
            if ((PreviousPercent % 10) == 0) {
                printf("%d", PreviousPercent / 10);

            } else {
                printf(".");
            }
        }
    }

    //
    // Open and write the file in.
    //

    Result = CiOpen(Volume, Path, TRUE, &Handle);
    if (Result == FALSE) {
        fprintf(stderr,
                "createimage: Unable to open %s in target image.\n",
                Path);

        goto AddFileContentsToImageEnd;
    }

    Result = CiWrite(Handle, FileContents, FileSize, &BytesCompleted);
    if ((Result == FALSE) || (BytesCompleted != FileSize)) {
        fprintf(stderr,
                "createimage: Unable to write %s in target image.\n",
                Path);

        Result = FALSE;
        goto AddFileContentsToImageEnd;
    }

    //
    // Figure out the file permissions.
    //

    FilePermissions = FileMode & FILE_PERMISSION_MASK;
    FileType = IoObjectRegularFile;
    if (S_ISLNK(FileMode)) {
        FileType = IoObjectSymbolicLink;

    } else if (!S_ISREG(FileMode)) {
        fprintf(stderr, "createimage: Unknown file type: mode %x\n", FileMode);
    }

    //
    // Try to guess whether or not the file is executable if it's not already
    // marked as such. Only do this on systems that don't have Unix permissions.
    //

    if ((S_IXGRP == 0) && ((FileMode & S_IXUSR) == 0)) {
        FirstValue = FileContents;

        //
        // If it starts with a magic value, then it's executable.
        //

        if ((FileSize > sizeof(ULONG)) &&
            ((*FirstValue == ELF_MAGIC) ||
             ((*FirstValue & 0x0000FFFF) == IMAGE_DOS_SIGNATURE) ||
              (*FirstValue & 0x0000FFFF) == SCRIPT_SHEBANG)) {

            FilePermissions |= FILE_PERMISSION_USER_EXECUTE |
                               FILE_PERMISSION_GROUP_EXECUTE |
                               FILE_PERMISSION_OTHER_EXECUTE;

        //
        // See if the file name ends in any known executable suffixes.
        //

        } else {
            SuffixCount = sizeof(CiExecutableSuffixes) /
                          sizeof(CiExecutableSuffixes[0]);

            PathLength = strlen(Path);
            for (SuffixIndex = 0; SuffixIndex < SuffixCount; SuffixIndex += 1) {
                SuffixLength = strlen(CiExecutableSuffixes[SuffixIndex]);
                if (PathLength >= SuffixLength) {
                    if (strcmp(Path + PathLength - SuffixLength,
                               CiExecutableSuffixes[SuffixIndex]) == 0) {

                        FilePermissions |= FILE_PERMISSION_USER_EXECUTE |
                                           FILE_PERMISSION_GROUP_EXECUTE |
                                           FILE_PERMISSION_OTHER_EXECUTE;

                        break;
                    }
                }
            }
        }
    }

    Result = CiSetFileProperties(Handle,
                                 FileType,
                                 FilePermissions,
                                 ModifiedTime,
                                 AccessTime);

    if (Result == FALSE) {
        goto AddFileContentsToImageEnd;
    }

    Result = TRUE;

AddFileContentsToImageEnd:
    if (Handle != NULL) {
        CiClose(Handle);
    }

    return Result;
}

BOOL
AddVhdFooter (
    FILE *File,
    ULONGLONG BlockCount
    )

/*++

Routine Description:

    This routine adds a VHD footer to the end of the file.

Arguments:

    File - Supplies an open file handle to the image.

    BlockCount - Supplies the number of blocks on the disk. The VHD footer
        will be written at this offset (times the sector size).

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    size_t BytesWritten;
    ULONG Cylinders;
    ULONG CylinderTimesHeads;
    ULONG Heads;
    INT Result;
    ULONG SectorsPerTrack;
    ULONGLONG TotalSectors;
    ULONG UniqueIdIndex;
    VHD_FOOTER VhdFooter;

    //
    // Create a footer that defines a fixed disk.
    //

    memset(&VhdFooter, 0, sizeof(VHD_FOOTER));
    VhdFooter.Cookie = VHD_COOKIE;
    VhdFooter.Features = RtlByteSwapUlong(VHD_FEATURES_DEFAULT);
    VhdFooter.FileFormatVersion = RtlByteSwapUlong(VHD_FILE_FORMAT_VERSION);
    VhdFooter.DataOffset = VHD_FIXED_DISK_DATA_OFFSET;
    VhdFooter.Timestamp =
                        RtlByteSwapUlong(time(NULL) - VHD_TIME_TO_EPOCH_DELTA);

    VhdFooter.CreatorApplication = VHD_CREATOR_ID;
    VhdFooter.CreatorVersion = VHD_VERSION(CREATEIMAGE_VERSION_MAJOR,
                                           CREATEIMAGE_VERSION_MINOR);

    VhdFooter.CreatorVersion = RtlByteSwapUlong(VhdFooter.CreatorVersion);
    VhdFooter.CreatorHostOs = RtlByteSwapUlong(VHD_HOST_OS);
    VhdFooter.OriginalSize =
                    RtlByteSwapUlonglong(BlockCount * CREATEIMAGE_SECTOR_SIZE);

    VhdFooter.CurrentSize = VhdFooter.OriginalSize;
    VhdFooter.DiskType = RtlByteSwapUlong(VHD_DISK_TYPE_FIXED);
    for (UniqueIdIndex = 0;
         UniqueIdIndex < sizeof(VhdFooter.UniqueId);
         UniqueIdIndex += 1) {

        VhdFooter.UniqueId[UniqueIdIndex] = rand();
    }

    //
    // Compute the disk geometry as defined in the VHD spec.
    //

    TotalSectors = BlockCount;
    if (TotalSectors > 0xFFFF * 16 * 255) {
        TotalSectors = 0xFFFF * 16 * 255;
    }

    if (TotalSectors >= 0xFFFF * 16 * 63) {
        SectorsPerTrack = 255;
        Heads = 16;
        CylinderTimesHeads = TotalSectors / SectorsPerTrack;

    } else {
        SectorsPerTrack = 17;
        CylinderTimesHeads = TotalSectors / SectorsPerTrack;
        Heads = (CylinderTimesHeads + 1023) / 1024;
        if (Heads < 4) {
            Heads = 4;
        }

        if ((CylinderTimesHeads >= (Heads * 1024)) || (Heads > 16)) {
            SectorsPerTrack = 31;
            Heads = 16;
            CylinderTimesHeads = TotalSectors / SectorsPerTrack;
        }

        if (CylinderTimesHeads >= (Heads * 1024)) {
            SectorsPerTrack = 63;
            Heads = 16;
            CylinderTimesHeads = TotalSectors / SectorsPerTrack;
        }
    }

    Cylinders = CylinderTimesHeads / Heads;
    VhdFooter.DiskGeometry = VHD_DISK_GEOMETRY(Cylinders,
                                               Heads,
                                               SectorsPerTrack);

    VhdFooter.Checksum =
                  CalculateVhdChecksum((PUCHAR)&VhdFooter, sizeof(VHD_FOOTER));

    VhdFooter.Checksum = RtlByteSwapUlong(VhdFooter.Checksum);

    //
    // Seek to the end of the disk and write the blocks out.
    //

    Result = fseeko64(File, BlockCount * CREATEIMAGE_SECTOR_SIZE, SEEK_SET);
    if (Result != 0) {
        fprintf(stderr,
                "createimage: Failed to seek to %I64x.\n",
                BlockCount * SECTOR_SIZE);

        return FALSE;
    }

    assert(sizeof(VHD_FOOTER) == CREATEIMAGE_SECTOR_SIZE);

    BytesWritten = fwrite(&VhdFooter, 1, sizeof(VHD_FOOTER), File);
    if (BytesWritten != sizeof(VHD_FOOTER)) {
        fprintf(stderr,
                "createimage: Wrote only %d of %d bytes.\n",
                BytesWritten,
                sizeof(VHD_FOOTER));

        return FALSE;
    }

    return TRUE;
}

ULONG
CalculateVhdChecksum (
    PUCHAR Data,
    ULONG Size
    )

/*++

Routine Description:

    This routine computes the VHD checksum of a buffer, which is just the
    one's complement sum of all the bytes.

Arguments:

    Data - Supplies a pointer to the data to checksum.

    Size - Supplies the number of bytes in the buffer.

Return Value:

    Returns the VHD checksum field.

--*/

{

    ULONG Index;
    ULONG Sum;

    Sum = 0;
    for (Index = 0; Index < Size; Index += 1) {
        Sum += Data[Index];
    }

    return ~Sum;
}

BOOL
MemoryMapFile (
    PCHAR Path,
    ULONGLONG FileSize,
    PVOID *Buffer
    )

/*++

Routine Description:

    This routine maps the contents of the given file into memory.

Arguments:

    Path - Supplies a pointer to the string containing the host path to the
        file.

    FileSize - Supplies the size of the file in bytes.

    Buffer - Supplies a pointer to where the newly allocated buffer will be
        returned. It is the caller's responsibility to free this buffer.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    LONG BytesRead;
    FILE *File;
    PVOID MappedFile;
    BOOL Result;

    File = NULL;
    MappedFile = NULL;
    Result = FALSE;
    if (FileSize > MAX_UINTN) {
        goto MemoryMapFileEnd;
    }

    File = fopen(Path, "rb");
    if (File == NULL) {
        fprintf(stderr,
                "createimage: unable to open '%s': %s.\n",
                Path,
                strerror(errno));

        goto MemoryMapFileEnd;
    }

    MappedFile = CiMalloc(FileSize);
    if (MappedFile == NULL) {
        goto MemoryMapFileEnd;
    }

    BytesRead = fread(MappedFile, 1, FileSize, File);
    if (BytesRead != FileSize) {
        printf("Unable to read %d bytes, actually read %d.\n",
               FileSize,
               BytesRead);

        goto MemoryMapFileEnd;
    }

    Result = TRUE;

MemoryMapFileEnd:
    if (File != NULL) {
        fclose(File);
    }

    if (Result == FALSE) {
        if (MappedFile != NULL) {
            CiFree(MappedFile);
            MappedFile = NULL;
        }
    }

    *Buffer = MappedFile;
    return Result;
}

BOOL
CipOpen (
    PCI_VOLUME Volume,
    PSTR Path,
    BOOL Create,
    BOOL Directory,
    PCI_HANDLE *Handle
    )

/*++

Routine Description:

    This routine opens a file or directory on the target image.

Arguments:

    Volume - Supplies the mounted volume token.

    Path - Supplies a pointer to the complete path within the volume to open.

    Create - Supplies a boolean indicating if the file should be created if it
        does not exist. If this is TRUE and the file does exist, the call will
        fail.

    Directory - Supplies a boolean indicating whether or not to open or create
        a directory.

    Handle - Supplies a pointer where the handle will be returned on success.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSTR Component;
    FILE_PROPERTIES DirectoryProperties;
    ULONGLONG DirectorySize;
    FILE_PROPERTIES FileProperties;
    ULONGLONG NewDirectorySize;
    PCI_HANDLE NewHandle;
    CHAR OriginalCharacter;
    PSTR PathCopy;
    INT Result;
    PSTR Search;
    KSTATUS Status;

    NewHandle = NULL;
    PathCopy = NULL;
    Result = FALSE;

    //
    // Open up the root directory.
    //

    Status = FatLookup(Volume->FileSystemHandle,
                       TRUE,
                       0,
                       NULL,
                       0,
                       &DirectoryProperties);

    if (!KSUCCESS(Status)) {
        fprintf(stderr,
                "createimage: Unable to lookup root directory: %x.\n",
                Status);

        goto OpenEnd;
    }

    //
    // Create a copy of the path to play with.
    //

    PathCopy = CiCopyString(Path);
    if (PathCopy == NULL) {
        goto OpenEnd;
    }

    //
    // Replace any backslashes with forward slashes.
    //

    Search = PathCopy;
    while (*Search != '\0') {
        if (*Search == '\\') {
            *Search = '/';
        }

        Search += 1;
    }

    //
    // Remove any trailing backslashes.
    //

    while ((Search != PathCopy) && (*(Search - 1) == '/')) {
        *(Search - 1) = '\0';
        Search -= 1;
    }

    if (strlen(PathCopy) == 0) {
        fprintf(stderr,
                "createimage: Path '%s' consists of only slashes.\n",
                PathCopy);

        goto OpenEnd;
    }

    //
    // Loop opening up directories until the actual path is found.
    //

    Component = PathCopy;
    while (*Component != '\0') {

        //
        // Find the first non-separator character.
        //

        while (*Component == '/') {
            Component += 1;
        }

        //
        // Find the next separator or end of the string.
        //

        Search = Component;
        while ((*Search != '\0') && (*Search != '/')) {
            Search += 1;
        }

        OriginalCharacter = *Search;
        *Search = '\0';
        Status = FatLookup(Volume->FileSystemHandle,
                           FALSE,
                           DirectoryProperties.FileId,
                           Component,
                           strlen(Component) + 1,
                           &DirectoryProperties);

        //
        // If this is not the last component or create is FALSE, then lookups
        // need to always succeed.
        //

        if ((OriginalCharacter == '/') || (Create == FALSE)) {

            //
            // If creating something and a path along the way doesn't exist,
            // create it.
            //

            if ((Status == STATUS_PATH_NOT_FOUND) && (Create != FALSE)) {
                memset(&FileProperties, 0, sizeof(FILE_PROPERTIES));
                FileProperties.Type = IoObjectRegularDirectory;
                FileProperties.Permissions = CREATEIMAGE_DEFAULT_PERMISSIONS |
                                             FILE_PERMISSION_ALL_EXECUTE;

                FatGetCurrentSystemTime(&(FileProperties.StatusChangeTime));

                assert(strlen(Component) != 0);

                Status = FatCreate(Volume->FileSystemHandle,
                                   DirectoryProperties.FileId,
                                   Component,
                                   strlen(Component) + 1,
                                   &NewDirectorySize,
                                   &FileProperties);

                if (!KSUCCESS(Status)) {
                    fprintf(stderr,
                            "createimage: Cannot create '%s': Status %x.\n",
                            Path,
                            Status);

                    goto OpenEnd;
                }

                READ_INT64_SYNC(&(DirectoryProperties.FileSize),
                                &DirectorySize);

                if (NewDirectorySize > DirectorySize) {
                    WRITE_INT64_SYNC(&(DirectoryProperties.FileSize),
                                     NewDirectorySize);

                    Status = FatWriteFileProperties(Volume->FileSystemHandle,
                                                    &DirectoryProperties,
                                                    0);

                    if (!KSUCCESS(Status)) {
                        goto OpenEnd;
                    }
                }

                RtlCopyMemory(&DirectoryProperties,
                              &FileProperties,
                              sizeof(FILE_PROPERTIES));

            } else if (!KSUCCESS(Status)) {
                fprintf(stderr,
                        "createimage: Failed to lookup component '%s' of "
                        "path '%s'. Status %x.\n",
                        Component,
                        Path,
                        Status);

                goto OpenEnd;
            }

        //
        // This is the last component and create is TRUE, so this had better
        // not succeed.
        //

        } else {
            if (KSUCCESS(Status)) {
                fprintf(stderr,
                        "createimage: Cannot create '%s': File exists.\n",
                        Path);

                goto OpenEnd;
            }
        }

        if (OriginalCharacter == '\0') {
            break;
        }

        //
        // Restore the character and advance to the end of this component.
        //

        *Search = OriginalCharacter;
        Component = Search;
    }

    //
    // For creates, create the file now.
    //

    if (Create != FALSE) {
        memset(&FileProperties, 0, sizeof(FILE_PROPERTIES));
        FileProperties.Type = IoObjectRegularFile;
        FileProperties.Permissions = CREATEIMAGE_DEFAULT_PERMISSIONS;
        if (Directory != FALSE) {
            FileProperties.Type = IoObjectRegularDirectory;
            FileProperties.Permissions |= FILE_PERMISSION_ALL_EXECUTE;
        }

        FatGetCurrentSystemTime(&(FileProperties.StatusChangeTime));

        assert(strlen(Component) != 0);

        Status = FatCreate(Volume->FileSystemHandle,
                           DirectoryProperties.FileId,
                           Component,
                           strlen(Component) + 1,
                           &NewDirectorySize,
                           &FileProperties);

        if (!KSUCCESS(Status)) {
            fprintf(stderr,
                    "createimage: Cannot create '%s': Status %x.\n",
                    Path,
                    Status);

            goto OpenEnd;
        }

        READ_INT64_SYNC(&(DirectoryProperties.FileSize), &DirectorySize);
        if (NewDirectorySize > DirectorySize) {
            WRITE_INT64_SYNC(&(DirectoryProperties.FileSize), NewDirectorySize);
            FatWriteFileProperties(Volume->FileSystemHandle,
                                   &DirectoryProperties,
                                   0);
        }

    //
    // This is just a regular open, verify that the directory-ness agrees.
    //

    } else {
        memcpy(&FileProperties, &DirectoryProperties, sizeof(FILE_PROPERTIES));
        if (Directory != FALSE) {
            if (FileProperties.Type != IoObjectRegularDirectory) {
                fprintf(stderr,
                        "createimage: Cannot open '%s': Not a directory.\n",
                        Path);

                goto OpenEnd;
            }

        } else {
            if (FileProperties.Type != IoObjectRegularFile) {
                fprintf(stderr,
                        "createimage: Cannot open '%s': Not a regular file.\n",
                        Path);

                goto OpenEnd;
            }
        }
    }

    //
    // Create the handle.
    //

    NewHandle = CiMalloc(sizeof(CI_HANDLE));
    if (NewHandle == NULL) {
        goto OpenEnd;
    }

    memset(NewHandle, 0, sizeof(CI_HANDLE));
    NewHandle->Volume = Volume->FileSystemHandle;
    Status = FatOpenFileId(Volume->FileSystemHandle,
                           FileProperties.FileId,
                           IO_ACCESS_READ | IO_ACCESS_WRITE,
                           0,
                           &(NewHandle->FileSystemHandle));

    if (!KSUCCESS(Status)) {
        fprintf(stderr,
                "createimage: Cannot open '%s': Status %x.\n",
                Path,
                Status);

        goto OpenEnd;
    }

    memcpy(&(NewHandle->Properties), &FileProperties, sizeof(FILE_PROPERTIES));
    Result = TRUE;

OpenEnd:
    if (PathCopy == NULL) {
        CiFree(PathCopy);
    }

    if (Result == FALSE) {
        if (NewHandle != NULL) {

            assert(NewHandle->FileSystemHandle == NULL);

            CiFree(NewHandle);
            NewHandle = NULL;
        }
    }

    *Handle = NewHandle;
    return Result;
}

BOOL
CipPerformIo (
    PCI_HANDLE Handle,
    BOOL Write,
    PVOID Buffer,
    UINTN Size,
    PUINTN BytesCompleted
    )

/*++

Routine Description:

    This routine reads from or writes to a file on the target image.

Arguments:

    Handle - Supplies the open handle to the file.

    Write - Supplies a boolean indicating if this is a read (FALSE) or write
        (TRUE).

    Buffer - Supplies the buffer to read or write.

    Size - Supplies the number of bytes to read or write.

    BytesCompleted - Supplies a pointer where the number of bytes read or
        written will be returned on success.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    FAT_SEEK_INFORMATION FatSeekInformation;
    ULONGLONG FileSize;
    PFAT_IO_BUFFER IoBuffer;
    INT Result;
    KSTATUS Status;

    Result = FALSE;
    IoBuffer = FatCreateIoBuffer(Buffer, Size);
    if (IoBuffer == NULL) {
        goto PerformIoEnd;
    }

    RtlZeroMemory(&FatSeekInformation, sizeof(FAT_SEEK_INFORMATION));
    Status = FatFileSeek(Handle->FileSystemHandle,
                         NULL,
                         0,
                         SeekCommandFromBeginning,
                         Handle->Position,
                         &FatSeekInformation);

    if (!KSUCCESS(Status)) {
        goto PerformIoEnd;
    }

    if (Write != FALSE) {
        Status = FatWriteFile(Handle->FileSystemHandle,
                              &FatSeekInformation,
                              IoBuffer,
                              Size,
                              0,
                              NULL,
                              BytesCompleted);

        if ((!KSUCCESS(Status)) || (*BytesCompleted != Size)) {
            printf("createimage: Failed to write %d bytes. Wrote "
                   "%d with status %x.\n",
                   Size,
                   *BytesCompleted,
                   Status);

            goto PerformIoEnd;
        }

    } else {
        Status = FatReadFile(Handle->FileSystemHandle,
                             &FatSeekInformation,
                             IoBuffer,
                             Size,
                             0,
                             NULL,
                             BytesCompleted);

        if (!KSUCCESS(Status)) {
            printf("createimage: Failed to read %d bytes. Read "
                   "%d with status %x.\n",
                   Size,
                   *BytesCompleted,
                   Status);

            goto PerformIoEnd;
        }
    }

    Handle->Position += *BytesCompleted;
    READ_INT64_SYNC(&(Handle->Properties.FileSize), &FileSize);
    if (Handle->Position > FileSize) {
        WRITE_INT64_SYNC(&(Handle->Properties.FileSize), Handle->Position);
    }

    FatGetCurrentSystemTime(&(Handle->Properties.AccessTime));
    if (Write != FALSE) {
        Handle->Properties.ModifiedTime = Handle->Properties.AccessTime;
    }

    Result = TRUE;

PerformIoEnd:
    if (IoBuffer != NULL) {
        FatFreeIoBuffer(IoBuffer);
    }

    return Result;
}

PSTR
AppendPaths (
    PSTR Path1,
    PSTR Path2
    )

/*++

Routine Description:

    This routine creates a concatenated string of "Path1/Path2".

Arguments:

    Path1 - Supplies a pointer to the prefix of the combined path.

    Path2 - Supplies a pointer to the path to append.

Return Value:

    Returns a pointer to the appended path on success. The caller is
    responsible for freeing this memory.

    NULL on failure.

--*/

{

    PSTR AppendedPath;
    size_t Offset;
    size_t Path1Length;
    size_t Path2Length;
    BOOL SlashNeeded;

    assert((Path1 != NULL) && (Path2 != NULL));

    Path1Length = strlen(Path1);
    Path2Length = strlen(Path2);
    SlashNeeded = TRUE;
    if ((Path1Length == 0) || (Path1[Path1Length - 1] == '/') ||
        (Path1[Path1Length - 1] == '\\')) {

        SlashNeeded = FALSE;
    }

    AppendedPath = CiMalloc(Path1Length + Path2Length + 2);
    if (AppendedPath == NULL) {
        return NULL;
    }

    Offset = 0;
    if (Path1Length != 0) {
        memcpy(AppendedPath, Path1, Path1Length);
        Offset += Path1Length;
    }

    if (SlashNeeded != FALSE) {
        AppendedPath[Offset] = '/';
        Offset += 1;
    }

    strcpy(AppendedPath + Offset, Path2);
    return AppendedPath;
}

VOID
ConvertUnixTimeToSystemTime (
    PSYSTEM_TIME SystemTime,
    time_t UnixTime
    )

/*++

Routine Description:

    This routine converts the given time_t value into a system time structure.
    Fractional seconds in the system time structure are set to zero.

Arguments:

    SystemTime - Supplies a pointer to the system time structure to initialize.

    UnixTime - Supplies the time to set.

Return Value:

    None.

--*/

{

    SystemTime->Seconds = UnixTime - SYSTEM_TIME_TO_EPOCH_DELTA;
    SystemTime->Nanoseconds = 0;
    return;
}

VOID
KdPrintWithArgumentList (
    PSTR Format,
    va_list ArgumentList
    )

/*++

Routine Description:

    This routine prints a string to the debugger. Currently the maximum length
    string is a little less than one debug packet.

Arguments:

    Format - Supplies a pointer to the printf-like format string.

    ArgumentList - Supplies a pointer to the initialized list of arguments
        required for the format string.

Return Value:

    None.

--*/

{

    vfprintf(stderr, Format, ArgumentList);
    return;
}

ULONG
MmPageSize (
    VOID
    )

/*++

Routine Description:

    This routine returns the size of a page of memory.

Arguments:

    None.

Return Value:

    Returns the size of one page of memory (ie the minimum mapping granularity).

--*/

{

    return 0x1000;
}

KERNEL_API
PVOID
MmAllocatePool (
    POOL_TYPE PoolType,
    UINTN Size,
    ULONG Tag
    )

/*++

Routine Description:

    This routine allocates memory from a kernel pool.

Arguments:

    PoolType - Supplies the type of pool to allocate from. Valid choices are:

        PoolTypeNonPaged - This type of memory will never be paged out. It is a
        scarce resource, and should only be allocated if paged pool is not
        an option. This memory is marked no-execute.

        PoolTypePaged - This is normal memory that may be transparently paged if
        memory gets tight. The caller may not touch paged pool at run-levels at
        or above dispatch, and is not suitable for DMA (as its physical address
        may change unexpectedly.) This pool type should be used for most normal
        allocations. This memory is marked no-execute.

    Size - Supplies the size of the allocation, in bytes.

    Tag - Supplies an identifier to associate with the allocation, useful for
        debugging and leak detection.

Return Value:

    Returns the allocated memory if successful, or NULL on failure.

--*/

{

    return CiMalloc(Size);
}

KERNEL_API
VOID
MmFreePool (
    POOL_TYPE PoolType,
    PVOID Allocation
    )

/*++

Routine Description:

    This routine frees memory allocated from a kernel pool.

Arguments:

    PoolType - Supplies the type of pool the memory was allocated from. This
        must agree with the type of pool the allocation originated from, or
        the system will become unstable.

    Allocation - Supplies a pointer to the allocation to free. This pointer
        may not be referenced after this function completes.

Return Value:

    None.

--*/

{

    CiFree(Allocation);
    return;
}

