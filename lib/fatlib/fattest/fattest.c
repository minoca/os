/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fattest.c

Abstract:

    This module implements the FAT file system test program.

Author:

    Evan Green 9-Oct-2012

Environment:

    Test

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/lib/fat/fat.h>

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

//
// ---------------------------------------------------------------- Definitions
//

#define OUTPUT_IMAGE    "testfat.test"
#define TEST_FILE_NAME  "testfile.pag"
#define TEST_FILE_SIZE (1024 * 1024 * 8)
#define BLOCK_ITERATIONS 10000
#define BLOCK_SIZE 4096

#define USAGE_STRING    \
    "Testfat.exe will test the FAT file system implementation.\n\n" \
    "Usage: Testfat.exe [-v]\n\n" \
    "    -v  Verbose mode\n\n" \

#define SECTOR_SIZE            512

//
// Disk geometry.
//

#define DISK_SECTORS_PER_TRACK 63
#define DISK_TRACKS_PER_HEAD 1
#define DISK_HEADS 16

//
// --------------------------------------------------------------------- Macros
//

#define VPRINT(_Format, _Args...)      \
    {                                  \
                                       \
        if (FatTestVerbose != FALSE) { \
            printf(_Format, ## _Args); \
        }                              \
    }

#define DPRINT(_Format, _Args...)      \
    {                                  \
                                       \
        if (FatTestDebug != FALSE) {   \
            printf(_Format, ## _Args); \
        }                              \
    }

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
FormatDisk (
    FILE *File,
    ULONG BlockSize,
    ULONGLONG BlockCount,
    PVOID *VolumeToken
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a global indicating whether to print out lots of information or not.
//

BOOL FatTestVerbose = FALSE;
BOOL FatTestDebug = FALSE;

//
// Store the size of one block on the device.
//

extern ULONG FatBlockSize;

//
// ------------------------------------------------------ Data Type Definitions
//

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

    PSTR Argument;
    ULONG BlockIndex;
    UINTN BytesRead;
    UINTN BytesWritten;
    ULONG DesiredAccess;
    FILE_PROPERTIES DirectoryProperties;
    ULONGLONG DirectorySize;
    ULONG DiskSize;
    FAT_SEEK_INFORMATION FatSeekInformation;
    PULONG FileBuffer;
    PFAT_IO_BUFFER FileIoBuffer;
    PVOID FileToken;
    ULONG FillIndex;
    ULONG Iteration;
    ULONGLONG NewDirectorySize;
    ULONG OpenFlags;
    FILE *OutputFile;
    PULONG PageBuffer;
    PFAT_IO_BUFFER PageIoBuffer;
    ULONG PageValue;
    FILE_PROPERTIES Properties;
    BOOL Result;
    KSTATUS Status;
    BOOL VerifyFailed;
    PVOID VolumeToken;

    FileBuffer = NULL;
    FileIoBuffer = NULL;
    PageBuffer = NULL;
    PageIoBuffer = NULL;
    Result = FALSE;
    srand(time(NULL));

    //
    // Process the command line options
    //

    while ((ArgumentCount > 1) && (Arguments[1][0] == '-')) {
        Argument = &(Arguments[1][1]);
        if (strcmp(Argument, "v") == 0) {
            FatTestVerbose = TRUE;

        } else if (strcmp(Argument, "d") == 0) {
            FatTestVerbose = TRUE;
            FatTestDebug = TRUE;

        } else {
            printf("%s: Invalid option\n\n%s", Argument, USAGE_STRING);
            return 1;
        }

        ArgumentCount -= 1;
        Arguments += 1;
    }

    //
    // Start by opening the output file.
    //

    OutputFile = fopen(OUTPUT_IMAGE, "wb+");
    if (OutputFile == NULL) {
        printf("Unable to open output file \"%s\" for write.\n",
               OUTPUT_IMAGE);

        goto MainEnd;
    }

    //
    // Create a 15MB disk, approximately.
    //

    DiskSize = 15 * (63 * 32);
    VPRINT("Formatting disk of size %d.\n", DiskSize * 512);
    Status = FormatDisk(OutputFile, 512, DiskSize, &VolumeToken);
    if (!KSUCCESS(Status)) {
        printf("Error: Could not format image. Status = %d.\n", Status);
        goto MainEnd;
    }

    //
    // Look up the root directory.
    //

    VPRINT("Opening root directory.\n");
    RtlZeroMemory(&DirectoryProperties, sizeof(FILE_PROPERTIES));
    Status = FatLookup(VolumeToken, TRUE, 0, NULL, 0, &DirectoryProperties);
    if (!KSUCCESS(Status)) {
        printf("Error: Could not look up root directory. Status = %d.\n",
               Status);

        goto MainEnd;
    }

    //
    // Create the test file.
    //

    VPRINT("Creating Test File\n");
    RtlZeroMemory(&Properties, sizeof(FILE_PROPERTIES));
    Properties.Type = IoObjectRegularFile;
    Properties.Permissions = FILE_PERMISSION_USER_READ |
                             FILE_PERMISSION_USER_WRITE;

    Properties.HardLinkCount = 1;
    Status = FatCreate(VolumeToken,
                       DirectoryProperties.FileId,
                       TEST_FILE_NAME,
                       sizeof(TEST_FILE_NAME),
                       &NewDirectorySize,
                       &Properties);

    if (!KSUCCESS(Status)) {
        printf("Error: Unable to create file %s. Status %d.\n",
               TEST_FILE_NAME,
               Status);

        goto MainEnd;
    }

    DirectorySize = DirectoryProperties.Size;
    if (NewDirectorySize > DirectorySize) {
        DirectoryProperties.Size = NewDirectorySize;
        FatWriteFileProperties(VolumeToken, &DirectoryProperties, 0);
    }

    //
    // Now open that created file.
    //

    DesiredAccess = IO_ACCESS_READ | IO_ACCESS_WRITE;
    OpenFlags = OPEN_FLAG_CREATE;
    Status = FatOpenFileId(VolumeToken,
                           Properties.FileId,
                           DesiredAccess,
                           OpenFlags,
                           &FileToken);

    if (!KSUCCESS(Status)) {
        printf("Error: Unable to open %s (ID %lld) in the output image."
               "Status %d\n",
               TEST_FILE_NAME,
               Properties.FileId,
               Status);

        goto MainEnd;
    }

    //
    // Allocate an 8MB buffer, zero it, and write it into the file.
    //

    FileIoBuffer = FatAllocateIoBuffer(NULL, TEST_FILE_SIZE);
    if (FileIoBuffer == NULL) {
        printf("Error: Unable to allocate 8MB buffer.\n");
        goto MainEnd;
    }

    FileBuffer = FatMapIoBuffer(FileIoBuffer);
    if (FileBuffer == NULL) {
        printf("Error: Unable to map 8MB buffer.\n");
        goto MainEnd;
    }

    RtlZeroMemory(&FatSeekInformation, sizeof(FAT_SEEK_INFORMATION));
    memset(FileBuffer, 0, TEST_FILE_SIZE);
    Status = FatWriteFile(FileToken,
                          &FatSeekInformation,
                          FileIoBuffer,
                          TEST_FILE_SIZE,
                          0,
                          NULL,
                          &BytesWritten);

    if ((!KSUCCESS(Status)) || (BytesWritten != TEST_FILE_SIZE)) {
        printf("Error: %lu bytes were written to file \"%s\", but the "
               "original file size is %d. Status = %d.\n",
               BytesWritten,
               TEST_FILE_NAME,
               TEST_FILE_SIZE,
               Status);

        goto MainEnd;
    }

    //
    // Do a bunch of random page writes, writing out a pattern that can be
    // identified.
    //

    PageIoBuffer = FatAllocateIoBuffer(NULL, BLOCK_SIZE);
    if (PageIoBuffer == NULL) {
        printf("Error: Unable to allocate page buffer.\n");
        goto MainEnd;
    }

    PageBuffer = FatMapIoBuffer(PageIoBuffer);
    if (PageBuffer == NULL) {
        printf("Error: Unable to map page buffer.\n");
        goto MainEnd;
    }

    VPRINT("Doing %d writes (. = 500)\n", BLOCK_ITERATIONS);
    for (Iteration = 0; Iteration < BLOCK_ITERATIONS; Iteration += 1) {
        if ((Iteration != 0) && ((Iteration % 500) == 0)) {
            VPRINT(".");
        }

        //
        // Pick a random block to use.
        //

        BlockIndex = rand() % (TEST_FILE_SIZE / BLOCK_SIZE);
        PageValue = (BlockIndex << 16) | Iteration;
        DPRINT("Block %08x, Value %08x\n", BlockIndex, PageValue);
        Status = FatFileSeek(FileToken,
                             NULL,
                             0,
                             SeekCommandFromBeginning,
                             (BlockIndex * BLOCK_SIZE),
                             &FatSeekInformation);

        if (!KSUCCESS(Status)) {
            printf("Error: Could not seek to offset 0x%x.\n",
                   BlockIndex * BLOCK_SIZE);

            goto MainEnd;
        }

        //
        // Fill the page with a magical value.
        //

        for (FillIndex = 0;
             FillIndex < (BLOCK_SIZE / sizeof(ULONG));
             FillIndex += 1) {

            PageBuffer[FillIndex] = PageValue;
        }

        //
        // Write it out to the file.
        //

        Status = FatWriteFile(FileToken,
                              &FatSeekInformation,
                              PageIoBuffer,
                              BLOCK_SIZE,
                              0,
                              NULL,
                              &BytesWritten);

        if ((!KSUCCESS(Status)) || (BytesWritten != BLOCK_SIZE)) {
            printf("Error: %lu bytes were written to file \"%s\", but the "
                   "block size is %d. Status = %d.\n",
                   BytesWritten,
                   TEST_FILE_NAME,
                   BLOCK_SIZE,
                   Status);

            goto MainEnd;
        }

        //
        // Immediately read it back.
        //

        Status = FatFileSeek(FileToken,
                             NULL,
                             0,
                             SeekCommandFromBeginning,
                             (BlockIndex * BLOCK_SIZE),
                             &FatSeekInformation);

        if (!KSUCCESS(Status)) {
            printf("Error: Could not seek to offset 0x%x.\n",
                   BlockIndex * BLOCK_SIZE);

            goto MainEnd;
        }

        Status = FatReadFile(FileToken,
                             &FatSeekInformation,
                             PageIoBuffer,
                             BLOCK_SIZE,
                             0,
                             NULL,
                             &BytesRead);

        if ((!KSUCCESS(Status)) || (BytesRead != BLOCK_SIZE)) {
            printf("Attempting to read block %x immediately after writing it "
                   "read %lu bytes, status %d.\n",
                   BlockIndex,
                   BytesRead,
                   Status);

            goto MainEnd;
        }

        //
        // Compare to what it should be.
        //

        VerifyFailed = FALSE;
        for (FillIndex = 0;
             FillIndex < (BLOCK_SIZE / sizeof(ULONG));
             FillIndex += 1) {

            if (PageBuffer[FillIndex] != PageValue) {
                printf("Error: Immediately after writing block %d, offset %lu "
                       "had %x in it instead of %x\n",
                       BlockIndex,
                       (long)FillIndex * sizeof(ULONG),
                       PageBuffer[FillIndex],
                       PageValue);

                VerifyFailed = TRUE;
            }
        }

        if (VerifyFailed != FALSE) {
            goto MainEnd;
        }
    }

    FatCloseFile(FileToken);
    Result = TRUE;

MainEnd:
    if (FileIoBuffer != NULL) {
        FatFreeIoBuffer(FileIoBuffer);
    }

    if (PageIoBuffer != NULL) {
        FatFreeIoBuffer(PageIoBuffer);
    }

    if (Result == FALSE) {
        return 1;
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
FormatDisk (
    FILE *File,
    ULONG BlockSize,
    ULONGLONG BlockCount,
    PVOID *VolumeToken
    )

/*++

Routine Description:

    This routine initializes, formats, and mounts a disk image.

Arguments:

    File - Supplies an open handle to the output file, the disk image to be
        created.

    BlockSize - Supplies the size of one sector on the disk.

    BlockCount - Supplies the total number of blocks on the disk.

    VolumeToken - Supplies a pointer where the token identifying the volume
        will be returned.

Return Value:

    Status code.

--*/

{

    BLOCK_DEVICE_PARAMETERS BlockParameters;
    KSTATUS Status;

    RtlZeroMemory(&BlockParameters, sizeof(BLOCK_DEVICE_PARAMETERS));
    BlockParameters.DeviceToken = File;
    BlockParameters.BlockSize = BlockSize;
    BlockParameters.BlockCount = BlockCount;
    FatBlockSize = BlockSize;

    //
    // Write the last byte on the disk to ensure that a file of that size is
    // created.
    //

    fseek(File, (BlockSize * BlockCount) - 1, SEEK_SET);
    fputc('\0', File);

    //
    // Format the drive using the FAT32 file system.
    //

    Status = FatFormat(&BlockParameters, 0, 0);
    if (!KSUCCESS(Status)) {
        printf("Error: Unable to format image. Status = %d.\n", Status);
        return Status;
    }

    //
    // Mount the disk.
    //

    Status = FatMount(&BlockParameters, 0, VolumeToken);
    if (!KSUCCESS(Status)) {
        printf("Error: Unable to mount freshly formatted image. Status = %d.\n",
               Status);
    }

    return Status;
}

VOID
KdPrintWithArgumentList (
    PCSTR Format,
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

    return malloc(Size);
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

    free(Allocation);
    return;
}

