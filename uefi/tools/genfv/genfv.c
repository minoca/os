/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    genfv.c

Abstract:

    This module implements the UEFI build tool for creating an FFS firmware
    volume out of one or more FFS files.

Author:

    Evan Green 7-Mar-2014

Environment:

    Build

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "uefifw.h"
#include "efiffs.h"
#include "fwvol.h"

//
// ---------------------------------------------------------------- Definitions
//

#define GENFV_VERSION_MAJOR 1
#define GENFV_VERSION_MINOR 0

#define GENFV_USAGE                                                            \
    "Usage: GenFv [options] [files...]\n"                                      \
    "The GenFv utility takes one or more FFS files produced by the GenFFS "    \
    "utility and combines them into a single FFS firmware volume.\nValid "     \
    "option are:\n"                                                            \
    "  -a, --attributes=value -- Specify the firmware volume attributes.\n"    \
    "  -b, --block-size=size -- Specify the block size. If not supplied, 512 " \
    "is assumed.\n"                                                            \
    "  -c, --block-count=count -- Specify the number of blocks in the \n"      \
    "      volume. If not supplied, the volume will be sized to fit the \n"    \
    "      files it contains.\n"                                               \
    "  -o, --output=File -- Specify the output image name.\n"                  \
    "  -v, --verbose -- Print extra information.\n"                            \
    "  --help -- Print this help and exit.\n"                                  \
    "  --version -- Print version information and exit.\n"                     \

#define GENFV_OPTIONS_STRING "b:c:o:v"

//
// Set this flag to print additional information.
//

#define GENFV_OPTION_VERBOSE 0x00000001

//
// Set this flag if a large file was seen.
//

#define GENFV_OPTION_LARGE_FILE 0x00000002

#define GENFV_DEFAULT_BLOCK_SIZE 512
#define GENFV_DEFAULT_ATTRIBUTES                    \
    (EFI_FVB_READ_STATUS | EFI_FVB2_ALIGNMENT_8 |   \
     EFI_FVB_MEMORY_MAPPED | EFI_FVB2_WEAK_ALIGNMENT)

#define GENFV_DEFAULT_OUTPUT_NAME "fwvol"

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the application context information for the GenFV
    utility.

Members:

    Flags - Stores a bitfield of flags. See GENFV_OPTION_* definition.

    OutputName - Stores the name of the output image.

    BlockSize - Stores the block size of the device.

    BlockCount - Stores the number of blocks in the device.

    Attributes - Stores the firmware volume attributes.

    FileCount - Stores the number of input files.

    Files - Stores the array of input files.

    MaxAlignment - Stores the highest alignment value seen.

--*/

typedef struct _GENFV_CONTEXT {
    UINT32 Flags;
    CHAR8 *OutputName;
    UINT32 BlockSize;
    UINT64 BlockCount;
    UINT32 Attributes;
    UINTN FileCount;
    CHAR8 **Files;
    UINT32 MaxAlignment;
} GENFV_CONTEXT, *PGENFV_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

int
GenfvCreateVolume (
    PGENFV_CONTEXT Context
    );

int
GenfvAddFile (
    PGENFV_CONTEXT Context,
    VOID *Buffer,
    UINTN BufferSize,
    UINTN *Offset,
    UINTN FileIndex
    );

int
GenfvAddPadFile (
    PGENFV_CONTEXT Context,
    VOID *Buffer,
    UINTN BufferSize,
    UINTN *Offset,
    UINTN NewOffset
    );

int
GenfvGetFileSize (
    CHAR8 *File,
    UINT64 *Size
    );

UINT8
GenfvCalculateChecksum8 (
    UINT8 *Buffer,
    UINTN Size
    );

UINT16
GenfvCalculateChecksum16 (
    UINT16 *Buffer,
    UINTN Size
    );

UINT32
GenfvReadAlignment (
    EFI_FFS_FILE_HEADER *Header
    );

BOOLEAN
GenfvCompareGuids (
    EFI_GUID *FirstGuid,
    EFI_GUID *SecondGuid
    );

//
// -------------------------------------------------------------------- Globals
//

struct option GenfvLongOptions[] = {
    {"attributes", required_argument, 0, 'a'},
    {"block-size", required_argument, 0, 'b'},
    {"block-count", required_argument, 0, 'c'},
    {"output", required_argument, 0, 'o'},
    {"verbose", no_argument, 0, 'v'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

EFI_GUID GenfvFfsVolumeTopGuid = EFI_FFS_VOLUME_TOP_FILE_GUID;
EFI_GUID GenfvFfsFileSystem2Guid = EFI_FIRMWARE_FILE_SYSTEM2_GUID;
EFI_GUID GenfvFfsFileSystem3Guid = EFI_FIRMWARE_FILE_SYSTEM2_GUID;

//
// ------------------------------------------------------------------ Functions
//

int
main (
    int ArgumentCount,
    CHAR8 **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point into the GenFV utility, which creates
    a firmware volume from FFS files.

Arguments:

    ArgumentCount - Supplies the number of command line parameters.

    Arguments - Supplies the array of pointers to parameter strings.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    CHAR8 *AfterScan;
    int ArgumentIndex;
    GENFV_CONTEXT Context;
    int Option;
    int Status;

    memset(&Context, 0, sizeof(GENFV_CONTEXT));
    Context.Attributes = GENFV_DEFAULT_ATTRIBUTES;
    Context.OutputName = GENFV_DEFAULT_OUTPUT_NAME;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             GENFV_OPTIONS_STRING,
                             GenfvLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = EINVAL;
            goto mainEnd;
        }

        switch (Option) {
        case 'a':
            Context.Attributes = strtol(optarg, &AfterScan, 0);
            if (AfterScan == optarg) {
                fprintf(stderr, "Error: Invalid FV attributes: %s.\n", optarg);
                Status = 1;
                goto mainEnd;
            }

            break;

        case 'b':
            Context.BlockSize = strtoul(optarg, &AfterScan, 0);
            if (AfterScan == optarg) {
                fprintf(stderr, "Error: Invalid FV block size: %s.\n", optarg);
                Status = EINVAL;
                goto mainEnd;
            }

            if ((Context.BlockSize & (Context.BlockSize - 1)) != 0) {
                fprintf(stderr, "Error: Block size must be a power of two.\n");
                Status = EINVAL;
                goto mainEnd;
            }

            break;

        case 'c':
            Context.BlockCount = strtoull(optarg, &AfterScan, 0);
            if (AfterScan == optarg) {
                fprintf(stderr, "Error: Invalid block count: %s.\n", optarg);
                Status = EINVAL;
                goto mainEnd;
            }

            break;

        case 'o':
            Context.OutputName = optarg;
            break;

        case 'v':
            Context.Flags |= GENFV_OPTION_VERBOSE;
            break;

        case 'V':
            printf("GenFv version %d.%d\n",
                   GENFV_VERSION_MAJOR,
                   GENFV_VERSION_MINOR);

            return 1;

        case 'h':
            printf(GENFV_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = EINVAL;
            goto mainEnd;
        }
    }

    if (Context.BlockSize == 0) {
        Context.BlockSize = GENFV_DEFAULT_BLOCK_SIZE;
    }

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    Context.FileCount = ArgumentCount - ArgumentIndex;
    Context.Files = &(Arguments[ArgumentIndex]);
    Status = GenfvCreateVolume(&Context);
    if (Status != 0) {
        goto mainEnd;
    }

mainEnd:
    if (Status != 0) {
        fprintf(stderr, "GenFV failed: %s.\n", strerror(Status));
        return 1;
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

int
GenfvCreateVolume (
    PGENFV_CONTEXT Context
    )

/*++

Routine Description:

    This routine creates a firmware volume from the given context.

Arguments:

    Context - Supplies the application context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    UINT32 AlignmentValue;
    VOID *Buffer;
    UINTN BufferSize;
    ssize_t BytesWritten;
    UINTN CurrentOffset;
    UINTN FileIndex;
    EFI_FIRMWARE_VOLUME_HEADER *Header;
    UINT32 MaxAlignment;
    FILE *Output;
    int Status;

    Buffer = NULL;
    Output = NULL;

    assert(Context->BlockSize != 0);

    //
    // If a block count was specified, use that directly. Otherwise, run
    // through once to compute the needed size of the buffer.
    //

    if (Context->BlockCount != 0) {
        CurrentOffset = Context->BlockCount * Context->BlockSize;

    } else {
        BufferSize = 0;

        //
        // At the beginning of the volume is the header, plus the block map
        // which consists of a single entry and a terminator. The volume header
        // already has a single entry in it, so just one more is needed for the
        // terminator.
        //

        CurrentOffset = sizeof(EFI_FIRMWARE_VOLUME_HEADER) +
                        sizeof(EFI_FV_BLOCK_MAP_ENTRY);

        CurrentOffset = ALIGN_VALUE(CurrentOffset, 8);
        for (FileIndex = 0; FileIndex < Context->FileCount; FileIndex += 1) {
            Status = GenfvAddFile(Context,
                                  Buffer,
                                  BufferSize,
                                  &CurrentOffset,
                                  FileIndex);

            if (Status != 0) {
                goto CreateVolumeEnd;
            }
        }

        //
        // Align up to the block size.
        //

        CurrentOffset = ALIGN_VALUE(CurrentOffset, Context->BlockSize);
        Context->BlockCount = CurrentOffset / Context->BlockSize;
    }

    BufferSize = CurrentOffset;

    //
    // Allocate the image buffer.
    //

    Buffer = malloc(BufferSize);
    if (Buffer == NULL) {
        fprintf(stderr,
                "Error: Failed to allocate image buffer, size 0x%lx.\n",
                (long)CurrentOffset);

        Status = ENOMEM;
        goto CreateVolumeEnd;
    }

    memset(Buffer, 0, BufferSize);
    if (BufferSize <
        sizeof(EFI_FIRMWARE_VOLUME_HEADER) + sizeof(EFI_FV_BLOCK_MAP_ENTRY)) {

        fprintf(stderr, "Error: The image is way too tiny.\n");
        Status = ERANGE;
        goto CreateVolumeEnd;
    }

    Header = Buffer;
    if ((Context->Flags & GENFV_OPTION_LARGE_FILE) != 0) {
        memcpy(&(Header->FileSystemGuid),
               &GenfvFfsFileSystem3Guid,
               sizeof(EFI_GUID));

    } else {
        memcpy(&(Header->FileSystemGuid),
               &GenfvFfsFileSystem2Guid,
               sizeof(EFI_GUID));
    }

    //
    // Compute the maximum alignment value.
    //

    AlignmentValue = 0;
    MaxAlignment = Context->MaxAlignment;
    while (MaxAlignment > 1) {
        AlignmentValue += 1;
        MaxAlignment >>= 1;
    }

    //
    // Initialize the firmware volume header.
    //

    Header->Length = Context->BlockCount * Context->BlockSize;
    Header->Signature = EFI_FVH_SIGNATURE;
    Header->Attributes = Context->Attributes |
                         ((AlignmentValue << 16) & 0xFFFF0000);

    Header->HeaderLength = sizeof(EFI_FIRMWARE_VOLUME_HEADER) +
                           sizeof(EFI_FV_BLOCK_MAP_ENTRY);

    Header->ExtHeaderOffset = 0;
    Header->Revision = EFI_FVH_REVISION;
    Header->BlockMap[0].BlockCount = Context->BlockCount;
    Header->BlockMap[0].BlockLength = Context->BlockSize;
    Header->BlockMap[1].BlockCount = 0;
    Header->BlockMap[1].BlockLength = 0;
    Header->Checksum = GenfvCalculateChecksum16((UINT16 *)Header,
                                                Header->HeaderLength);

    //
    // Add all the files to the image.
    //

    CurrentOffset = Header->HeaderLength;
    for (FileIndex = 0; FileIndex < Context->FileCount; FileIndex += 1) {
        Status = GenfvAddFile(Context,
                              Buffer,
                              BufferSize,
                              &CurrentOffset,
                              FileIndex);

        if (Status != 0) {
            goto CreateVolumeEnd;
        }
    }

    //
    // Create the output file and write out the image.
    //

    Output = fopen(Context->OutputName, "wb");
    if (Output == NULL) {
        Status = errno;
        fprintf(stderr,
                "Error: Failed to open output %s: %s.\n",
                Context->OutputName,
                strerror(Status));

        goto CreateVolumeEnd;
    }

    BytesWritten = fwrite(Buffer, 1, BufferSize, Output);
    if (BytesWritten != BufferSize) {
        Status = errno;
        if (Status == 0) {
            Status = EIO;
        }

        fprintf(stderr,
                "Error: Failed to write %s: %s.\n",
                Context->OutputName,
                strerror(Status));

        goto CreateVolumeEnd;
    }

CreateVolumeEnd:
    if (Buffer != NULL) {
        free(Buffer);
    }

    if (Output != NULL) {
        fclose(Output);
    }

    return Status;
}

int
GenfvAddFile (
    PGENFV_CONTEXT Context,
    VOID *Buffer,
    UINTN BufferSize,
    UINTN *Offset,
    UINTN FileIndex
    )

/*++

Routine Description:

    This routine adds a file to the buffer containing the working firmware
    volume image.

Arguments:

    Context - Supplies the application context.

    Buffer - Supplies an optional pointer to the buffer to add. If NULL, only
        the size will be computed.

    BufferSize - Supplies the total size of the supplied buffer.

    Offset - Supplies a pointer that on input contains the current offset into
        the buffer. On output this will be advanced past the portion used by
        this file, even if no bufer was supplied.

    FileIndex - Supplies the index of the file to add in the context's file
        array.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    UINTN Alignment;
    ssize_t BytesRead;
    FILE *File;
    EFI_FFS_FILE_HEADER *FileHeader;
    CHAR8 *FileName;
    UINT64 FileSize;
    EFI_FFS_FILE_HEADER2 Header;
    UINTN HeaderSize;
    UINT8 HeaderSum;
    UINTN PaddedOffset;
    int Status;
    UINTN TopFileEnd;

    assert(FileIndex < Context->FileCount);

    FileName = Context->Files[FileIndex];
    File = fopen(FileName, "rb");
    if (File == NULL) {
        fprintf(stderr,
                "Error: Failed to open %s: %s.\n",
                FileName,
                strerror(errno));

        Status = errno;
        goto AddFileEnd;
    }

    Status = GenfvGetFileSize(FileName, &FileSize);
    if (Status != 0) {
        goto AddFileEnd;
    }

    HeaderSize = sizeof(EFI_FFS_FILE_HEADER);
    if (FileSize > MAX_FFS_SIZE) {
        HeaderSize = sizeof(EFI_FFS_FILE_HEADER2);
        Context->Flags |= GENFV_OPTION_LARGE_FILE;
    }

    BytesRead = fread(&Header, 1, HeaderSize, File);
    if (BytesRead < HeaderSize) {
        fprintf(stderr,
                "Error: Only read %ld bytes of %s.\n",
                (long)BytesRead,
                FileName);

        Status = EINVAL;
        goto AddFileEnd;
    }

    //
    // Verify the FFS file checksum in case a bozo tried to slip in a non-FFS
    // file.
    //

    Header.State = 0;
    HeaderSum = Header.IntegrityCheck.Checksum.Header;
    Header.IntegrityCheck.Checksum16 = 0;
    if (HeaderSum != GenfvCalculateChecksum8((UINT8 *)&Header, HeaderSize)) {
        fprintf(stderr,
                "Error: %s does not appear to be a valid FFS file. "
                "Did you use GenFFS to create it?\n", FileName);

        Status = EINVAL;
        goto AddFileEnd;
    }

    //
    // Get the alignment and add a pad file if needed.
    //

    Alignment = GenfvReadAlignment((EFI_FFS_FILE_HEADER *)&Header);
    if (Alignment > Context->MaxAlignment) {
        Context->MaxAlignment = Alignment;
    }

    //
    // Things get trickier for a volume top file.
    //

    if (GenfvCompareGuids(&(Header.Name), &GenfvFfsVolumeTopGuid) != FALSE) {
        if (FileIndex != Context->FileCount - 1) {
            fprintf(stderr,
                    "Error: A volume top file (%s) must be the last file.\n",
                    FileName);

            Status = EINVAL;
            goto AddFileEnd;
        }

        //
        // The volume top file must align propertly. If there's a block count
        // set, then the file start must align.
        //

        if (Context->BlockCount != 0) {
            PaddedOffset = (Context->BlockCount * Context->BlockSize) -
                           FileSize;

            if ((PaddedOffset % Alignment) != 0) {
                fprintf(stderr,
                        "Error: Volume top file is size 0x%llx, which "
                        "conflicts with its required alignment of 0x%x.\n",
                        FileSize,
                        (UINT32)Alignment);

                Status = EINVAL;
                goto AddFileEnd;
            }

        //
        // If there's no block count set, align this file out to its desired
        // alignment, then verify that the file ends on a block boundary.
        //

        } else {
            PaddedOffset = *Offset;
            if (((*Offset + HeaderSize) % Alignment) != 0) {
                PaddedOffset = (*Offset + HeaderSize +
                                sizeof(EFI_FFS_FILE_HEADER) + Alignment) &
                               ~(Alignment - 1);

                PaddedOffset -= HeaderSize;
            }

            TopFileEnd = PaddedOffset + HeaderSize + FileSize;
            if ((TopFileEnd % Context->BlockSize) != 0) {
                fprintf(stderr,
                        "Error: Volume top file is size 0x%llx, which "
                        "conflicts with its required alignment of 0x%x.\n",
                        FileSize,
                        (UINT32)Alignment);

                Status = EINVAL;
                goto AddFileEnd;
            }
        }

        //
        // Create a pad file if needed.
        //

        if (PaddedOffset != *Offset) {
            Status = GenfvAddPadFile(Context,
                                     Buffer,
                                     BufferSize,
                                     Offset,
                                     PaddedOffset);

            if (Status != 0) {
                goto AddFileEnd;
            }
        }

    //
    // This is not a volume top file. Pad out to its required alignment
    // if necessary.
    //

    } else {
        if (((*Offset + HeaderSize) % Alignment) != 0) {
            PaddedOffset = (*Offset + HeaderSize + sizeof(EFI_FFS_FILE_HEADER) +
                            Alignment) &
                           ~(Alignment - 1);

            PaddedOffset -= HeaderSize;
            Status = GenfvAddPadFile(Context,
                                     Buffer,
                                     BufferSize,
                                     Offset,
                                     PaddedOffset);

            if (Status != 0) {
                goto AddFileEnd;
            }
        }
    }

    //
    // Read the file in.
    //

    if (BufferSize != 0) {
        if ((Context->Flags & GENFV_OPTION_VERBOSE) != 0) {
            printf("Adding file %s at offset %lx, size %llx\n",
                   FileName,
                   (long)*Offset,
                   FileSize);
        }

        fseek(File, 0, SEEK_SET);
        if (*Offset + FileSize > BufferSize) {
            Status = ERANGE;
            goto AddFileEnd;
        }

        BytesRead = fread(Buffer + *Offset, 1, FileSize, File);
        if (BytesRead != FileSize) {
            fprintf(stderr, "Error: Failed to read from %s.\n", FileName);
            Status = EIO;
            goto AddFileEnd;
        }

        //
        // Flip the state bits if the erase polarity is one.
        //

        FileHeader = (EFI_FFS_FILE_HEADER *)(Buffer + *Offset);
        if ((Context->Attributes & EFI_FVB_ERASE_POLARITY) != 0) {
            FileHeader->State = ~(FileHeader->State);
        }
    }

    *Offset += FileSize;

    //
    // Align up to 8-bytes.
    //

    *Offset = ALIGN_VALUE(*Offset, 8);
    if ((BufferSize != 0) && (*Offset > BufferSize)) {
        Status = ERANGE;
        goto AddFileEnd;
    }

    Status = 0;

AddFileEnd:
    if (File != NULL) {
        fclose(File);
    }

    return Status;
}

int
GenfvAddPadFile (
    PGENFV_CONTEXT Context,
    VOID *Buffer,
    UINTN BufferSize,
    UINTN *Offset,
    UINTN NewOffset
    )

/*++

Routine Description:

    This routine adds a pad file to the image.

Arguments:

    Context - Supplies the application context.

    Buffer - Supplies an optional pointer to the buffer to add. If NULL, only
        the size will be computed.

    BufferSize - Supplies the total size of the supplied buffer.

    Offset - Supplies a pointer that on input contains the current offset into
        the buffer. On output this will be advanced past the portion used by
        this file, even if no bufer was supplied.

    NewOffset - Supplies the offset the pad file should extend to.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    UINTN HeaderSize;
    EFI_FFS_FILE_HEADER2 *PadFile;
    UINTN PadFileSize;

    if (BufferSize != 0) {
        if (NewOffset > BufferSize) {
            return ERANGE;
        }

        memset(Buffer + *Offset, 0, NewOffset - *Offset);
        PadFile = Buffer + *Offset;
        PadFile->Type = EFI_FV_FILETYPE_FFS_PAD;
        PadFile->Attributes = 0;
        PadFileSize = (NewOffset - *Offset) - sizeof(EFI_FFS_FILE_HEADER);
        if ((Context->Flags & GENFV_OPTION_VERBOSE) != 0) {
            printf("Creating pad file at 0x%lx, Size %lx to new offset "
                   "0x%lx.\n",
                   *Offset,
                   PadFileSize,
                   NewOffset);
        }

        if (PadFileSize > MAX_FFS_SIZE) {
            HeaderSize = sizeof(EFI_FFS_FILE_HEADER2);
            PadFileSize = (NewOffset - *Offset) - HeaderSize;
            PadFile->ExtendedSize = PadFileSize;
            Context->Flags |= GENFV_OPTION_LARGE_FILE;

        } else {
            HeaderSize = sizeof(EFI_FFS_FILE_HEADER);
            PadFile->Size[0] = (UINT8)(PadFileSize & 0xFF);
            PadFile->Size[1] = (UINT8)((PadFileSize & 0xFF00) >> 8);
            PadFile->Size[2] = (UINT8)((PadFileSize & 0xFF0000) >> 16);
        }

        PadFile->IntegrityCheck.Checksum.Header =
                         GenfvCalculateChecksum8((UINT8 *)PadFile, HeaderSize);

        PadFile->IntegrityCheck.Checksum.File = FFS_FIXED_CHECKSUM;
        PadFile->State = EFI_FILE_HEADER_CONSTRUCTION |
                         EFI_FILE_HEADER_VALID |
                         EFI_FILE_DATA_VALID;

        if ((Context->Attributes & EFI_FVB_ERASE_POLARITY) != 0) {
            PadFile->State = ~(PadFile->State);
        }
    }

    *Offset = NewOffset;
    return 0;
}

int
GenfvGetFileSize (
    CHAR8 *File,
    UINT64 *Size
    )

/*++

Routine Description:

    This routine returns the size of the given file.

Arguments:

    File - Supplies a pointer to a string containing the path to the file to
        get the size of.

    Size - Supplies a pointer where the size of the file will be returned on
        success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    int Result;
    struct stat Stat;

    Result = stat(File, &Stat);
    if (Result != 0) {
        return Result;
    }

    *Size = Stat.st_size;
    return Result;
}

UINT8
GenfvCalculateChecksum8 (
    UINT8 *Buffer,
    UINTN Size
    )

/*++

Routine Description:

    This routine calculates the checksum of the bytes in the given buffer.

Arguments:

    Buffer - Supplies a pointer to a buffer containing byte data.

    Size - Supplies the size of the buffer.

Return Value:

    Returns the 8-bit checksum of each byte in the buffer.

--*/

{

    UINTN Index;
    UINT8 Sum;

    Sum = 0;
    for (Index = 0; Index < Size; Index += 1) {
        Sum = (UINT8)(Sum + Buffer[Index]);
    }

    return 0x100 - Sum;
}

UINT16
GenfvCalculateChecksum16 (
    UINT16 *Buffer,
    UINTN Size
    )

/*++

Routine Description:

    This routine calculates the 16-bit checksum of the bytes in the given
    buffer.

Arguments:

    Buffer - Supplies a pointer to a buffer containing byte data.

    Size - Supplies the size of the buffer, in bytes.

Return Value:

    Returns the 16-bit checksum of the buffer words.

--*/

{

    UINTN Index;
    UINT16 Sum;

    Size = Size / sizeof(UINT16);
    Sum = 0;
    for (Index = 0; Index < Size; Index += 1) {
        Sum = (UINT16)(Sum + Buffer[Index]);
    }

    return 0x10000 - Sum;
}

UINT32
GenfvReadAlignment (
    EFI_FFS_FILE_HEADER *Header
    )

/*++

Routine Description:

    This routine returns the alignment requirement from a FFS file header.

Arguments:

    Header - Supplies a pointer to the header.

Return Value:

    Returns the alignment, in bytes.

--*/

{

    switch ((Header->Attributes & FFS_ATTRIB_DATA_ALIGNMENT) >> 3) {

    //
    // 1 byte alignment.
    //

    case 0:
        return 1 << 0;

    //
    // 16 byte alignment.
    //

    case 1:
        return 1 << 4;

    //
    // 128 byte alignment.
    //

    case 2:
        return 1 << 7;

    //
    // 512 byte alignment.
    //

    case 3:
        return 1 << 9;

    //
    // 1K byte alignment.
    //

    case 4:
        return 1 << 10;

    //
    // 4K byte alignment.
    //

    case 5:
        return 1 << 12;

    //
    // 32K byte alignment.
    //

    case 6:
        return 1 << 15;

    default:
        break;
    }

    assert(FALSE);

    return 0;
}

BOOLEAN
GenfvCompareGuids (
    EFI_GUID *FirstGuid,
    EFI_GUID *SecondGuid
    )

/*++

Routine Description:

    This routine compares two GUIDs.

Arguments:

    FirstGuid - Supplies a pointer to the first GUID.

    SecondGuid - Supplies a pointer to the second GUID.

Return Value:

    TRUE if the GUIDs are equal.

    FALSE if the GUIDs are different.

--*/

{

    UINT32 *FirstPointer;
    UINT32 *SecondPointer;

    //
    // Compare GUIDs 32 bits at a time.
    //

    FirstPointer = (UINT32 *)FirstGuid;
    SecondPointer = (UINT32 *)SecondGuid;
    if ((FirstPointer[0] == SecondPointer[0]) &&
        (FirstPointer[1] == SecondPointer[1]) &&
        (FirstPointer[2] == SecondPointer[2]) &&
        (FirstPointer[3] == SecondPointer[3])) {

        return TRUE;
    }

    return FALSE;
}

