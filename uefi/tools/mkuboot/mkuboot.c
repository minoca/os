/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mkuboot.c

Abstract:

    This module implements the utility to create U-Boot images.

Author:

    Chris Stevens 2-Jul-2015

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>

#include "uefifw.h"
#include "uboot.h"

//
// --------------------------------------------------------------------- Macros
//

#define MupByteSwap32(_Value) \
   ((((_Value) & 0xFF000000) >> 24) | (((_Value) & 0x00FF0000) >>  8) | \
    (((_Value) & 0x0000FF00) <<  8) | (((_Value) & 0x000000FF) << 24))

//
// ---------------------------------------------------------------- Definitions
//

#define MKUBOOT_VERSION_MAJOR 1
#define MKUBOOT_VERSION_MINOR 0

#define MKUBOOT_USAGE                                                          \
    "Usage: mkuboot [-c] [-a arch] [-f format] [-e entry_point] "              \
    "[-l load_address] -o image file\n"                                        \
    "Mkuboot creates a bootable U-Boot image based off of the given file.\n"   \
    "Options are:\n"                                                           \
    "  -a, --arch=arch -- Specify the architecture of the image file. \n"      \
    "      Valid values are arm and x86.\n"                                    \
    "  -c, --create -- Create the output even if it already exists.\n"         \
    "  -e, --entry=entry_point -- Specify the hexidecimal value of the data\n" \
    "      file's entry point.\n"                                              \
    "  -f, --format=format -- Specify the output format. Valid values \n"      \
    "      are fit and legacy. Legacy is the default.\n"                       \
    "  -l, --load=address -- Specify the hexidecimal load address for \n"      \
    "      data file.\n"                                                       \
    "  -o, --output=image -- Specify the output image name.\n"                 \
    "  -v, --verbose -- Output more information.\n"                            \
    "  file -- Specify the image to use for creating the U-Boot image.\n"      \
    "  --help -- Print this help text and exit.\n"                             \
    "  --version -- Print the application version information and exit.\n\n"   \

#define MKUBOOT_OPTIONS_STRING "a:ce:f:l:o:vhV"

#define MKUBOOT_OPTION_VERBOSE       0x00000001
#define MKUBOOT_OPTION_CREATE_ALWAYS 0x00000002

#define MKUBOOT_ARCHITECTURE_ARM 1
#define MKUBOOT_ARCHITECTURE_X86 2

//
// Define the default description to use for the FIT image.
//

#define MKUBOOT_DEFAULT_FIT_DESCRIPTION "Minoca U-Boot Firmware Image."
#define MKUBOOT_DEFAULT_FIT_DEVICE_TREE_DESCRIPTION "Empty Device Tree."

//
// Define the default kernel, device tree, and configuration names.
//

#define MKUBOOT_DEFAULT_FIT_KERNEL_NAME "kernel@1"
#define MKUBOOT_DEFAULT_FIT_DEVICE_TREE_NAME "fdt@1"
#define MKUBOOT_DEFAULT_FIT_CONFIGURATION_NAME "config@1"

//
// Define the alignment for a U-Boot fit image. Empirically, it is smaller than
// 4K, but be on the safe side.
//

#define MKUBOOT_FIT_ALIGNMENT 4096

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _MKUBOOT_PROPERTY {
    MkUBootPropertyDescription,
    MkUBootPropertyTimestamp,
    MkUBootPropertyData,
    MkUBootPropertyType,
    MkUBootPropertyArchitecture,
    MkUBootPropertyOs,
    MkUBootPropertyCompression,
    MkUBootPropertyLoadAddress,
    MkUBootPropertyEntryPoint,
    MkUBootPropertyDefault,
    MkUBootPropertyKernel,
    MkUBootPropertyCount
} MKUBOOT_PROPERTY, *PMKUBOOT_PROPERTY;

typedef enum _MKUBOOT_FORMAT {
    MkUBootFormatInvalid,
    MkUBootFormatLegacy,
    MkUBootFormatFit
} MKUBOOT_FORMAT, *PMKUBOOT_FORMAT;

/*++

Structure Description:

    This structure defines the context used by mkuboot while creating an image.

Members:

    InputFileName - Stores a null-terminated path to the input file.

    OutputFileName - Store a null-terminated path to the output file.

    InputFileBuffer - Stores a pointer to the input files contents.

    InputFileSize - Stores the size of the input file, in bytes.

    OutputFile - Stores a pointer to the opened output file stream.

    Options - Stores a bitmask of options. See MKUBOOT_OPTION_* for definitions.

    Architecture - Stores the architecture of the input file.

    LoadAddress - Stores the data load address to store in the U-Boot image.

    EntryPoint - Stores the data entry point to store in the U-Boot image.

--*/

typedef struct _MKUBOOT_CONTEXT {
    CHAR8 *InputFileName;
    CHAR8 *OutputFileName;
    void *InputFileBuffer;
    UINT32 InputFileSize;
    FILE *OutputFile;
    UINT32 Options;
    UINT32 Architecture;
    UINT32 LoadAddress;
    UINT32 EntryPoint;
} MKUBOOT_CONTEXT, *PMKUBOOT_CONTEXT;

/*++

Structure Description:

    This structure defines the set of information needed to convert a U-Boot
    property type to it's name and string name offset.

Members:

    Property - Stores the property type value.

    Name - Stores a null-terminated string representing the name of the
        property.

    Offset - Stores the property's offset into the dictionary.

--*/

typedef struct _MKUBOOT_PROPERTY_ENTRY {
    MKUBOOT_PROPERTY Property;
    CHAR8 *Name;
    UINT32 Offset;
} MKUBOOT_PROPERTY_ENTRY, *PMKUBOOT_PROPERTY_ENTRY;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// Borrow the CRC32 routine from EFI core. This program can't link in Rtl
// because there are naming conflicts with some EFI names (like INTN).
//

EFIAPI
EFI_STATUS
EfiCoreCalculateCrc32 (
    VOID *Data,
    UINTN DataSize,
    UINT32 *Crc32
    );

int
MupOpenFiles (
    PMKUBOOT_CONTEXT Context
    );

void
MupCloseFiles (
    PMKUBOOT_CONTEXT Context
    );

int
MupCreateLegacyImage (
    PMKUBOOT_CONTEXT Context
    );

int
MupCreateFitImage (
    PMKUBOOT_CONTEXT Context
    );

int
MupCreateStringsDictionary (
    void **Strings,
    UINT32 *StringsSize
    );

void
MupDestroyStringsDictionary (
    void *Strings
    );

int
MupWriteFitStructures (
    PMKUBOOT_CONTEXT Context
    );

int
MupWriteNodeStart (
    PMKUBOOT_CONTEXT Context,
    CHAR8 *Name
    );

int
MupWriteProperty (
    PMKUBOOT_CONTEXT Context,
    MKUBOOT_PROPERTY Property,
    void *Data,
    UINT32 DataSize
    );

//
// -------------------------------------------------------------------- Globals
//

struct option MkUBootLongOptions[] = {
    {"address", required_argument, 0, 'a'},
    {"create", no_argument, 0, 'c'},
    {"entry", required_argument, 0, 'e'},
    {"format", required_argument, 0, 'f'},
    {"output", required_argument, 0, 'o'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {"verbose", no_argument, 0, 'v'},
    {NULL, 0, 0, 0},
};

//
// Store the set of property entries whose names need to be written to the
// strings dictionary.
//

MKUBOOT_PROPERTY_ENTRY MkUBootProperties[MkUBootPropertyCount] = {
    {MkUBootPropertyDescription, UBOOT_FIT_PROPERTY_DESCRIPTION, 0},
    {MkUBootPropertyTimestamp, UBOOT_FIT_PROPERTY_TIMESTAMP, 0},
    {MkUBootPropertyData, UBOOT_FIT_PROPERTY_DATA, 0},
    {MkUBootPropertyType, UBOOT_FIT_PROPERTY_TYPE, 0},
    {MkUBootPropertyArchitecture, UBOOT_FIT_PROPERTY_ARCHITECTURE, 0},
    {MkUBootPropertyOs, UBOOT_FIT_PROPERTY_OS, 0},
    {MkUBootPropertyCompression, UBOOT_FIT_PROPERTY_COMPRESSION, 0},
    {MkUBootPropertyLoadAddress, UBOOT_FIT_PROPERTY_LOAD_ADDRESS, 0},
    {MkUBootPropertyEntryPoint, UBOOT_FIT_PROPERTY_ENTRY_POINT, 0},
    {MkUBootPropertyDefault, UBOOT_FIT_PROPERTY_DEFAULT, 0},
    {MkUBootPropertyKernel, UBOOT_FIT_PROPERTY_KERNEL, 0},
};

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

    This routine is the main entry point for the program. It collects the
    options passed to it, and creates the output UBoot image.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    CHAR8 *AfterScan;
    CHAR8 *Argument;
    int ArgumentIndex;
    MKUBOOT_CONTEXT Context;
    MKUBOOT_FORMAT Format;
    int Option;
    int Result;

    srand(time(NULL));
    memset(&Context, 0, sizeof(MKUBOOT_CONTEXT));
    Format = MkUBootFormatLegacy;
    Context.Architecture = MKUBOOT_ARCHITECTURE_X86;

    //
    // Process the command line options
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             MKUBOOT_OPTIONS_STRING,
                             MkUBootLongOptions,
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
            Argument = optarg;
            if (strcasecmp(Argument, "arm") == 0) {
                Context.Architecture = MKUBOOT_ARCHITECTURE_ARM;

            } else if (strcasecmp(Argument, "x86") == 0) {
                Context.Architecture = MKUBOOT_ARCHITECTURE_X86;

            } else {
                fprintf(stderr,
                        "mkuboot: Invalid architecture '%s'.\n",
                        Argument);

                Result = 1;
                goto MainEnd;
            }

            break;

        case 'c':
            Context.Options |= MKUBOOT_OPTION_CREATE_ALWAYS;
            break;

        case 'e':
            Argument = optarg;
            errno = 0;
            Context.EntryPoint = (UINT32)strtoul(Argument, &AfterScan, 16);
            if ((Context.EntryPoint == 0) && (errno != 0)) {
                fprintf(stderr, "mkuboot: Invalid entry point '%s'.\n",
                        Argument);

                Result = 1;
                goto MainEnd;
            }

            break;

        case 'f':
            Argument = optarg;
            if (strcasecmp(Argument, "legacy") == 0) {
                Format = MkUBootFormatLegacy;

            } else if (strcasecmp(Argument, "fit") == 0) {
                Format = MkUBootFormatFit;

            } else {
                fprintf(stderr,
                        "mkuboot: Invalid disk format '%s'.\n",
                        Argument);

                Result = 1;
                goto MainEnd;
            }

            break;

        case 'l':
            Argument = optarg;
            errno = 0;
            Context.LoadAddress = (UINT32)strtoul(Argument, &AfterScan, 16);
            if ((Context.LoadAddress == 0) && (errno != 0)) {
                fprintf(stderr, "mkuboot: Invalid load address '%s'.\n",
                        Argument);

                Result = 1;
                goto MainEnd;
            }

            break;

        case 'o':
            Context.OutputFileName = optarg;
            break;

        case 'v':
            Context.Options |= MKUBOOT_OPTION_VERBOSE;
            break;

        case 'V':
            printf("mkuboot version %d.%d.\n",
                   MKUBOOT_VERSION_MAJOR,
                   MKUBOOT_VERSION_MINOR);

            return 1;

        case 'h':
            printf(MKUBOOT_USAGE);
            return 1;

        default:

            assert(FALSE);

            Result = 1;
            goto MainEnd;
        }
    }

    //
    // Make sure an output image file was specified.
    //

    if (Context.OutputFileName == NULL) {
        fprintf(stderr,
                "mkuboot: An output image must be specified with -o.\n");

        Result = 1;
        goto MainEnd;
    }

    //
    // The last remaining argument should be the file to convert into the
    // U-Boot image.
    //

    ArgumentIndex = optind;
    if ((ArgumentIndex > ArgumentCount) ||
        (ArgumentCount - ArgumentIndex) != 1) {

        fprintf(stderr, "mkuboot: An input file must be specified.\n");
        Result = 1;
        goto MainEnd;
    }

    Context.InputFileName = Arguments[ArgumentIndex];

    //
    // Open the input and output files now stored in the context.
    //

    Result = MupOpenFiles(&Context);
    if (Result != 0) {
        goto MainEnd;
    }

    //
    // Create the U-Boot image based on the requested format type.
    //

    switch (Format) {
    case MkUBootFormatLegacy:
        Result = MupCreateLegacyImage(&Context);
        if (Result != 0) {
            fprintf(stderr, "mkuboot: Failed to create image.\n");
            Result = 1;
            goto MainEnd;
        }

        break;

    case MkUBootFormatFit:
        Result = MupCreateFitImage(&Context);
        if (Result != 0) {
            fprintf(stderr, "mkuboot: Failed to create image.\n");
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
    MupCloseFiles(&Context);
    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

int
MupOpenFiles (
    PMKUBOOT_CONTEXT Context
    )

/*++

Routine Description:

    This routine opens the input and output files in the given context.

Arguments:

    Context - Supplies a pointer to the make U-Boot context.

Return Value:

    Returns 0 on success. Non-zero on failure.

--*/

{

    size_t BytesRead;
    FILE *InputFile;
    UINT8 *InputFileBuffer;
    UINT32 InputFileSize;
    CHAR8 *OpenMode;
    FILE *OutputFile;
    int Result;

    InputFile = NULL;
    InputFileBuffer = NULL;
    OutputFile = NULL;
    Result = 0;

    //
    // Open the input file to get its size.
    //

    InputFile = fopen(Context->InputFileName, "rb");
    if (InputFile == NULL) {
        fprintf(stderr,
                "mkuboot: Unable to open input file \"%s\" for read: %s.\n",
                Context->InputFileName,
                strerror(errno));

        Result = 1;
        goto OpenFilesEnd;
    }

    //
    // Get the size of the input file image and allocate a buffer to hold it.
    //

    fseek(InputFile, 0, SEEK_END);
    InputFileSize = (UINT32)ftell(InputFile);
    fseek(InputFile, 0, SEEK_SET);
    InputFileBuffer = malloc(InputFileSize);
    if (InputFileBuffer == NULL) {
        fprintf(stderr,
                "mkuboot: Failed to allocate memory for input file \"%s\".",
                Context->InputFileName);

        Result = 1;
        goto OpenFilesEnd;
    }

    //
    // Read the input file.
    //

    BytesRead = fread(InputFileBuffer, 1, InputFileSize, InputFile);
    if (BytesRead != InputFileSize) {
        fprintf(stderr,
                "mkuboot: Unable to read \"%s\". Read %ld bytes, expected "
                "%d.\n",
                Context->InputFileName,
                (long)BytesRead,
                InputFileSize);

        Result = 1;
        goto OpenFilesEnd;
    }

    //
    // Open the output file for write.
    //

    OpenMode = "rb+";
    if ((Context->Options & MKUBOOT_OPTION_CREATE_ALWAYS) != 0) {
        OpenMode = "wb+";
    }

    OutputFile = fopen(Context->OutputFileName, OpenMode);
    if (OutputFile == NULL) {
        fprintf(stderr,
                "mkuboot: Unable to open output file \"%s\" for write: %s.\n",
                Context->OutputFileName,
                strerror(errno));

        Result = 1;
        goto OpenFilesEnd;
    }

    //
    // Update the context with the collected data.
    //

    Context->InputFileBuffer = InputFileBuffer;
    Context->InputFileSize = InputFileSize;
    Context->OutputFile = OutputFile;

OpenFilesEnd:
    if (Result != 0) {
        if (InputFileBuffer != NULL) {
            free(InputFileBuffer);
        }

        if (OutputFile != NULL) {
            fclose(OutputFile);
        }
    }

    if (InputFile != NULL) {
        fclose(InputFile);
    }

    return Result;
}

void
MupCloseFiles (
    PMKUBOOT_CONTEXT Context
    )

/*++

Routine Description:

    This routine closes the input and output files in the given context.

Arguments:

    Context - Supplies a pointer to the make U-Boot context.

Return Value:

    None.

--*/

{

    if (Context->OutputFile != NULL) {
        fclose(Context->OutputFile);
        Context->OutputFile = NULL;
    }

    if (Context->InputFileBuffer != NULL) {
        free(Context->InputFileBuffer);
        Context->InputFileBuffer = NULL;
    }

    Context->InputFileSize = 0;
    return;
}

int
MupCreateLegacyImage (
    PMKUBOOT_CONTEXT Context
    )

/*++

Routine Description:

    This routine creates a legacy U-Boot image out of the given input file and
    writes it to the output file.

Arguments:

    Context - Supplies a pointer to the U-Boot image creation context.

Return Value:

    Returns 0 on success. Non-zero on failure.

--*/

{

    size_t BytesWritten;
    int Result;
    UBOOT_HEADER UBootHeader;

    Result = 0;

    //
    // Fill out the U-Boot header.
    //

    memset(&UBootHeader, 0, sizeof(UBOOT_HEADER));
    UBootHeader.Magic = MupByteSwap32(UBOOT_MAGIC);
    UBootHeader.CreationTimestamp = MupByteSwap32(time(NULL));
    UBootHeader.DataSize = MupByteSwap32(Context->InputFileSize);
    UBootHeader.DataLoadAddress = MupByteSwap32(Context->LoadAddress);
    UBootHeader.EntryPoint = MupByteSwap32(Context->EntryPoint);
    EfiCoreCalculateCrc32(Context->InputFileBuffer,
                          Context->InputFileSize,
                          &(UBootHeader.DataCrc32));

    UBootHeader.DataCrc32 = MupByteSwap32(UBootHeader.DataCrc32);
    UBootHeader.OperatingSystem = UBOOT_OS_LINUX;
    UBootHeader.Architecture = UBOOT_ARCHITECTURE_ARM;
    UBootHeader.ImageType = UBOOT_IMAGE_KERNEL;
    UBootHeader.CompressionType = UBOOT_COMPRESSION_NONE;
    strncpy((CHAR8 *)&UBootHeader.ImageName,
            Context->InputFileName,
            UBOOT_MAX_NAME);

    EfiCoreCalculateCrc32(&UBootHeader,
                          sizeof(UBOOT_HEADER),
                          &(UBootHeader.HeaderCrc32));

    UBootHeader.HeaderCrc32 = MupByteSwap32(UBootHeader.HeaderCrc32);

    //
    // Write out the U-Boot header.
    //

    BytesWritten = fwrite(&UBootHeader,
                          1,
                          sizeof(UBOOT_HEADER),
                          Context->OutputFile);

    if (BytesWritten != sizeof(UBOOT_HEADER)) {
        fprintf(stderr,
                "mkuboot: Needed to write %d byte U-Boot header, wrote %ld "
                "bytes.\n",
                (int)sizeof(UBOOT_HEADER),
                (long)BytesWritten);

        Result = 1;
        goto CreateLegacyImageEnd;
    }

    //
    // Write the input file to the data section of the output file.
    //

    BytesWritten = fwrite(Context->InputFileBuffer,
                          1,
                          Context->InputFileSize,
                          Context->OutputFile);

    if (BytesWritten != Context->InputFileSize) {
        Result = 1;
        goto CreateLegacyImageEnd;
    }

CreateLegacyImageEnd:
    return Result;
}

int
MupCreateFitImage (
    PMKUBOOT_CONTEXT Context
    )

/*++

Routine Description:

    This routine creates a FIT U-Boot image out of the given input file and
    writes it to the output file.

Arguments:

    Context - Supplies a pointer to the U-Boot image creation context.

Return Value:

    Returns 0 on success. Non-zero on failure.

--*/

{

    size_t BytesWritten;
    UBOOT_FIT_MEMORY_RESERVE_MAP MemoryReserveMap;
    UINT32 MemoryReserveMapOffset;
    long Offset;
    int Result;
    void *Strings;
    UINT32 StringsOffset;
    UINT32 StringsSize;
    UINT32 StructuresOffset;
    UINT32 StructuresSize;
    UINT32 TotalSize;
    UBOOT_FIT_HEADER UBootFitHeader;

    Strings = NULL;

    //
    // Create the strings dictionary. This initialized the global properties
    // array so that it holds the correct string offsets for each property's
    // string.
    //

    Result = MupCreateStringsDictionary(&Strings, &StringsSize);
    if (Result != 0) {
        fprintf(stderr, "mkuboot: Failed to create strings dictionary.\n");
        goto CreateFitImageEnd;
    }

    //
    // Reserve space in the image for the U-Boot FIT header.
    //

    BytesWritten = 0;
    while (BytesWritten < sizeof(UBOOT_FIT_HEADER)) {
        fputc('\0', Context->OutputFile);
        BytesWritten += 1;
    }

    //
    // Write out the reserve memory map.
    //

    Offset = ftell(Context->OutputFile);
    if (Offset < 0) {
        fprintf(stderr, "mkuboot: Failed to seek.\n");
        Result = 1;
        goto CreateFitImageEnd;
    }

    MemoryReserveMapOffset = (UINT32)Offset;
    MemoryReserveMap.BaseAddress = 0;
    MemoryReserveMap.Size = 0;
    BytesWritten = fwrite(&MemoryReserveMap,
                          1,
                          sizeof(UBOOT_FIT_MEMORY_RESERVE_MAP),
                          Context->OutputFile);

    if (BytesWritten != sizeof(UBOOT_FIT_MEMORY_RESERVE_MAP)) {
        fprintf(stderr, "mkuboot: Failed to write reserve memory map.\n");
        Result = 1;
        goto CreateFitImageEnd;
    }

    //
    // Write the structure data out to the file.
    //

    Offset = ftell(Context->OutputFile);
    if (Offset < 0) {
        fprintf(stderr, "mkuboot: Failed to seek.\n");
        Result = 1;
        goto CreateFitImageEnd;
    }

    StructuresOffset = (UINT32)Offset;
    Result = MupWriteFitStructures(Context);
    if (Result != 0) {
        fprintf(stderr, "mkuboot: Failed to write FIT structures.\n");
        goto CreateFitImageEnd;
    }

    Offset = ftell(Context->OutputFile);
    if (Offset < 0) {
        fprintf(stderr, "mkuboot: Failed to seek.\n");
        Result = 1;
        goto CreateFitImageEnd;
    }

    StringsOffset = (UINT32)Offset;
    StructuresSize = StringsOffset - StructuresOffset;

    //
    // Write the strings out to the file.
    //

    BytesWritten = fwrite(Strings, 1, StringsSize, Context->OutputFile);
    if (BytesWritten != StringsSize) {
        fprintf(stderr,
                "mkuboot: Failed to write FIT strings. Write %ld bytes, "
                "expected %d.\n",
                (long)BytesWritten,
                StringsSize);

        Result = 1;
        goto CreateFitImageEnd;
    }

    Offset = ftell(Context->OutputFile);
    if (Offset < 0) {
        fprintf(stderr, "mkuboot: Failed to seek.\n");
        Result = 1;
        goto CreateFitImageEnd;
    }

    TotalSize = (UINT32)Offset;

    //
    // Align the total size up to the alignment value.
    //

    while ((TotalSize & (MKUBOOT_FIT_ALIGNMENT - 1)) != 0) {
        fputc('\0', Context->OutputFile);
        TotalSize += 1;
    }

    //
    // Fill out the U-Boot FIT header.
    //

    memset(&UBootFitHeader, 0, sizeof(UBOOT_FIT_HEADER));
    UBootFitHeader.Magic = MupByteSwap32(UBOOT_FIT_MAGIC);
    UBootFitHeader.TotalSize = MupByteSwap32(TotalSize);
    UBootFitHeader.StructuresOffset = MupByteSwap32(StructuresOffset);
    UBootFitHeader.StringsOffset = MupByteSwap32(StringsOffset);
    UBootFitHeader.MemoryReserveMapOffset =
                                         MupByteSwap32(MemoryReserveMapOffset);

    UBootFitHeader.Version = MupByteSwap32(UBOOT_FIT_VERSION);
    UBootFitHeader.LastCompatibleVersion =
                              MupByteSwap32(UBOOT_FIT_LAST_COMPATIBLE_VERSION);

    UBootFitHeader.BootCpuId = 0;
    UBootFitHeader.StringsSize = MupByteSwap32(StringsSize);
    UBootFitHeader.StructuresSize = MupByteSwap32(StructuresSize);

    //
    // Write out the U-Boot FIT header back at the beginning of the file.
    //

    Result = fseek(Context->OutputFile, 0, SEEK_SET);
    if (Result != 0) {
        Result = 1;
        goto CreateFitImageEnd;
    }

    BytesWritten = fwrite(&UBootFitHeader,
                          1,
                          sizeof(UBOOT_FIT_HEADER),
                          Context->OutputFile);

    if (BytesWritten != sizeof(UBOOT_FIT_HEADER)) {
        fprintf(stderr,
                "mkuboot: Needed to write %d byte U-Boot FIT header, "
                "wrote %ld bytes.\n",
                (int)sizeof(UBOOT_FIT_HEADER),
                (long)BytesWritten);

        Result = 1;
        goto CreateFitImageEnd;
    }

CreateFitImageEnd:
    if (Strings != NULL) {
        MupDestroyStringsDictionary(Strings);
    }

    return Result;
}

int
MupCreateStringsDictionary (
    void **Strings,
    UINT32 *StringsSize
    )

/*++

Routine Description:

    This routine creates the strings dictionary for a U-Boot FIT image.

Arguments:

    Strings - Supplies a pointer that receives the string dictionary.

    StringsSize - Supplies a pointer that receives the total size of the string
        dictionary, in bytes.

Return Value:

    None.

--*/

{

    void *Buffer;
    int Index;
    size_t Length;
    UINT32 Offset;
    PMKUBOOT_PROPERTY_ENTRY PropertyEntry;
    int Result;
    UINT32 Size;

    //
    // Calculate the total size of the dictionary. For now, only the property
    // strings are included.
    //

    Size = 0;
    for (Index = 0; Index < MkUBootPropertyCount; Index += 1) {
        PropertyEntry = &(MkUBootProperties[Index]);
        Length = strlen(PropertyEntry->Name) + 1;
        Size += Length;
    }

    Buffer = malloc(Size);
    if (Buffer == NULL) {
        Result = 1;
        goto CreateStringsDictionaryEnd;
    }

    //
    // Copy the strings, including null terminators, into the buffer. While
    // doing so, fill in the offsets for the properties.
    //

    Offset = 0;
    for (Index = 0; Index < MkUBootPropertyCount; Index += 1) {
        PropertyEntry = &(MkUBootProperties[Index]);
        PropertyEntry->Offset = Offset;
        Length = strlen(PropertyEntry->Name) + 1;
        memcpy(Buffer + Offset, PropertyEntry->Name, Length);
        Offset += Length;
    }

    Result = 0;

CreateStringsDictionaryEnd:
    if (Result != 0) {
        if (Buffer != NULL) {
            free(Buffer);
            Buffer = NULL;
        }

        Size = 0;
    }

    *Strings = Buffer;
    *StringsSize = Size;
    return Result;
}

void
MupDestroyStringsDictionary (
    void *Strings
    )

/*++

Routine Description:

    This routine destroys the given strings dictionary.

Arguments:

    Strings - Supplies a pointer to the string dictionary to destroy.

Return Value:

    None.

--*/

{

    free(Strings);
    return;
}

int
MupWriteFitStructures (
    PMKUBOOT_CONTEXT Context
    )

/*++

Routine Description:

    This routine writes all of the structures out a U-Boot FIT image. From the
    context it writes the input file as the one an only kernel for the default
    configuration. It also creates an empty flat device tree required by U-Boot.

Arguments:

    Context - Supplies a pointer to the make U-Boot context.

Return Value:

    Returns 0 on success. Non-zero on failure.

--*/

{

    UINT32 Address;
    size_t BytesWritten;
    UINT32 EndNodeTag;
    UINT32 EndTag;
    size_t Length;
    int Result;
    CHAR8 *String;
    UINT32 Timestamp;

    EndTag = MupByteSwap32(UBOOT_FIT_TAG_END);
    EndNodeTag = MupByteSwap32(UBOOT_FIT_TAG_NODE_END);

    //
    // Write the root node.
    //

    Result = MupWriteNodeStart(Context, UBOOT_FIT_NODE_ROOT);
    if (Result != 0) {
        goto FitWriteStructuresEnd;
    }

    //
    // Write the root's timestamp.
    //

    Timestamp = MupByteSwap32(time(NULL));
    Result = MupWriteProperty(Context,
                              MkUBootPropertyTimestamp,
                              &Timestamp,
                              sizeof(UINT32));

    if (Result != 0) {
        goto FitWriteStructuresEnd;
    }

    //
    // Write the root description property.
    //

    Length = strlen(MKUBOOT_DEFAULT_FIT_DESCRIPTION) + 1;
    Result = MupWriteProperty(Context,
                              MkUBootPropertyDescription,
                              MKUBOOT_DEFAULT_FIT_DESCRIPTION,
                              Length);

    if (Result != 0) {
        goto FitWriteStructuresEnd;
    }

    //
    // Write the images node.
    //

    Result = MupWriteNodeStart(Context, UBOOT_FIT_NODE_IMAGES);
    if (Result != 0) {
        goto FitWriteStructuresEnd;
    }

    //
    // Write the kernel node, including the data, type, os type, compression,
    // load address, and entry point.
    //

    Result = MupWriteNodeStart(Context, MKUBOOT_DEFAULT_FIT_KERNEL_NAME);
    if (Result != 0) {
        goto FitWriteStructuresEnd;
    }

    Result = MupWriteProperty(Context,
                              MkUBootPropertyData,
                              Context->InputFileBuffer,
                              Context->InputFileSize);

    if (Result != 0) {
        goto FitWriteStructuresEnd;
    }

    if (Context->LoadAddress != 0) {
        String = UBOOT_IMAGE_STRING_KERNEL;

    } else {
        String = UBOOT_IMAGE_STRING_KERNEL_NO_LOAD;
    }

    Length = strlen(String) + 1;
    Result = MupWriteProperty(Context, MkUBootPropertyType, String, Length);
    if (Result != 0) {
        goto FitWriteStructuresEnd;
    }

    switch (Context->Architecture) {
    case MKUBOOT_ARCHITECTURE_ARM:
        String = UBOOT_ARCHITECTURE_STRING_ARM;
        break;

    case MKUBOOT_ARCHITECTURE_X86:
        String = UBOOT_ARCHITECTURE_STRING_X86;
        break;

    default:
        fprintf(stderr, "mkuboot: Invalid architecture.\n");
        Result = 1;
        goto FitWriteStructuresEnd;
    }

    Length = strlen(String) + 1;
    Result = MupWriteProperty(Context,
                              MkUBootPropertyArchitecture,
                              String,
                              Length);

    if (Result != 0) {
        goto FitWriteStructuresEnd;
    }

    Length = strlen(UBOOT_OS_STRING_LINUX) + 1;
    Result = MupWriteProperty(Context,
                              MkUBootPropertyOs,
                              UBOOT_OS_STRING_LINUX,
                              Length);

    if (Result != 0) {
        goto FitWriteStructuresEnd;
    }

    Length = strlen(UBOOT_COMPRESSION_STRING_NONE) + 1;
    Result = MupWriteProperty(Context,
                              MkUBootPropertyCompression,
                              UBOOT_COMPRESSION_STRING_NONE,
                              Length);

    if (Result != 0) {
        goto FitWriteStructuresEnd;
    }

    Address = MupByteSwap32(Context->LoadAddress);
    Result = MupWriteProperty(Context,
                              MkUBootPropertyLoadAddress,
                              &Address,
                              sizeof(UINT32));

    if (Result != 0) {
        goto FitWriteStructuresEnd;
    }

    Address = MupByteSwap32(Context->EntryPoint);
    Result = MupWriteProperty(Context,
                              MkUBootPropertyEntryPoint,
                              &Address,
                              sizeof(UINT32));

    if (Result != 0) {
        goto FitWriteStructuresEnd;
    }

    //
    // Write the kernel's node end tag.
    //

    BytesWritten = fwrite(&EndNodeTag, 1, sizeof(UINT32), Context->OutputFile);
    if (BytesWritten != sizeof(UINT32)) {
        Result = 1;
        goto FitWriteStructuresEnd;
    }

    //
    // Write the Flat Device Tree's node, including a description, empty data
    // set, and a type property.
    //

    Result = MupWriteNodeStart(Context, MKUBOOT_DEFAULT_FIT_DEVICE_TREE_NAME);
    if (Result != 0) {
        goto FitWriteStructuresEnd;
    }

    Length = strlen(MKUBOOT_DEFAULT_FIT_DEVICE_TREE_DESCRIPTION) + 1;
    Result = MupWriteProperty(Context,
                              MkUBootPropertyDescription,
                              MKUBOOT_DEFAULT_FIT_DEVICE_TREE_DESCRIPTION,
                              Length);

    if (Result != 0) {
        goto FitWriteStructuresEnd;
    }

    Result = MupWriteProperty(Context, MkUBootPropertyData, NULL, 0);
    if (Result != 0) {
        goto FitWriteStructuresEnd;
    }

    Length = strlen(UBOOT_IMAGE_STRING_FLAT_DEVICE_TREE) + 1;
    Result = MupWriteProperty(Context,
                              MkUBootPropertyType,
                              UBOOT_IMAGE_STRING_FLAT_DEVICE_TREE,
                              Length);

    if (Result != 0) {
        goto FitWriteStructuresEnd;
    }

    //
    // Write the device tree's node end tag, and the image node's end tag.
    //

    BytesWritten = fwrite(&EndNodeTag, 1, sizeof(UINT32), Context->OutputFile);
    if (BytesWritten != sizeof(UINT32)) {
        Result = 1;
        goto FitWriteStructuresEnd;
    }

    BytesWritten = fwrite(&EndNodeTag, 1, sizeof(UINT32), Context->OutputFile);
    if (BytesWritten != sizeof(UINT32)) {
        Result = 1;
        goto FitWriteStructuresEnd;
    }

    //
    // Write the configurations node.
    //

    Result = MupWriteNodeStart(Context, UBOOT_FIT_NODE_CONFIGURATIONS);
    if (Result != 0) {
        goto FitWriteStructuresEnd;
    }

    //
    // Write the default configuration property.
    //

    Length = strlen(MKUBOOT_DEFAULT_FIT_CONFIGURATION_NAME) + 1;
    Result = MupWriteProperty(Context,
                              MkUBootPropertyDefault,
                              MKUBOOT_DEFAULT_FIT_CONFIGURATION_NAME,
                              Length);

    if (Result != 0) {
        goto FitWriteStructuresEnd;
    }

    //
    // Create the default configuration's node.
    //

    Result = MupWriteNodeStart(Context, MKUBOOT_DEFAULT_FIT_CONFIGURATION_NAME);
    if (Result != 0) {
        goto FitWriteStructuresEnd;
    }

    Length = strlen(MKUBOOT_DEFAULT_FIT_KERNEL_NAME) + 1;
    Result = MupWriteProperty(Context,
                              MkUBootPropertyKernel,
                              MKUBOOT_DEFAULT_FIT_KERNEL_NAME,
                              Length);

    if (Result != 0) {
        goto FitWriteStructuresEnd;
    }

    //
    // End both the default configuration node and the configuration node.
    //

    BytesWritten = fwrite(&EndNodeTag, 1, sizeof(UINT32), Context->OutputFile);
    if (BytesWritten != sizeof(UINT32)) {
        Result = 1;
        goto FitWriteStructuresEnd;
    }

    BytesWritten = fwrite(&EndNodeTag, 1, sizeof(UINT32), Context->OutputFile);
    if (BytesWritten != sizeof(UINT32)) {
        Result = 1;
        goto FitWriteStructuresEnd;
    }

    //
    // Write the final node end tag for the root and the end tag.
    //

    BytesWritten = fwrite(&EndNodeTag, 1, sizeof(UINT32), Context->OutputFile);
    if (BytesWritten != sizeof(UINT32)) {
        Result = 1;
        goto FitWriteStructuresEnd;
    }

    BytesWritten = fwrite(&EndTag, 1, sizeof(UINT32), Context->OutputFile);
    if (BytesWritten != sizeof(UINT32)) {
        Result = 1;
        goto FitWriteStructuresEnd;
    }

FitWriteStructuresEnd:
    return Result;
}

int
MupWriteNodeStart (
    PMKUBOOT_CONTEXT Context,
    CHAR8 *Name
    )

/*++

Routine Description:

    This routine writes a FIT image start node with the given name to the
    context's output file.

Arguments:

    Context - Supplies a pointer to the make U-Boot context.

    Name - Supplies the name of the node.

Return Value:

    Returns 0 on success. Non-zero on failure.

--*/

{

    size_t BytesWritten;
    size_t NameLength;
    PUBOOT_FIT_NODE Node;
    UINT32 NodeSize;
    int Result;

    NameLength = strlen(Name) + 1;
    NodeSize = sizeof(UBOOT_FIT_NODE) + NameLength;
    NodeSize = ALIGN_VALUE(NodeSize, UBOOT_FIT_TAG_ALIGNMENT);
    Node = malloc(NodeSize);
    if (Node == NULL) {
        Result = 1;
        goto WriteNodeStartEnd;
    }

    memset(Node, 0, NodeSize);
    Node->Tag = MupByteSwap32(UBOOT_FIT_TAG_NODE_START);
    memcpy((Node + 1), Name, NameLength);
    BytesWritten = fwrite(Node, 1, NodeSize, Context->OutputFile);
    if (BytesWritten != NodeSize) {
        Result = 1;
        goto WriteNodeStartEnd;
    }

    Result = 0;

WriteNodeStartEnd:
    if (Node != NULL) {
        free(Node);
    }

    return Result;
}

int
MupWriteProperty (
    PMKUBOOT_CONTEXT Context,
    MKUBOOT_PROPERTY Property,
    void *Data,
    UINT32 DataSize
    )

/*++

Routine Description:

    This routine writes a FIT property tag to the context's output file using
    the given data.

Arguments:

    Context - Supplies a pointer to the make U-Boot context.

    Property - Supplies the property to be written.

    Data - Supplies a pointer to the data to write for the property.

    DataSize - Supplies the size of the data to write.

Return Value:

    Returns 0 on success. Non-zero on failure.

--*/

{

    size_t BytesWritten;
    PUBOOT_FIT_PROPERTY FitProperty;
    UINT32 FitPropertySize;
    int Result;
    UINT32 StringOffset;

    FitPropertySize = sizeof(UBOOT_FIT_PROPERTY) + DataSize;
    FitPropertySize = ALIGN_VALUE(FitPropertySize, UBOOT_FIT_TAG_ALIGNMENT);
    FitProperty = malloc(FitPropertySize);
    if (FitProperty == NULL) {
        Result = 1;
        goto WritePropertyEnd;
    }

    memset(FitProperty, 0, FitPropertySize);
    FitProperty->Tag = MupByteSwap32(UBOOT_FIT_TAG_PROPERTY);
    FitProperty->Size = MupByteSwap32(DataSize);
    StringOffset = MupByteSwap32(MkUBootProperties[Property].Offset);
    FitProperty->StringOffset = StringOffset;
    if (DataSize != 0) {
        memcpy((FitProperty + 1), Data, DataSize);
    }

    BytesWritten = fwrite(FitProperty, 1, FitPropertySize, Context->OutputFile);
    if (BytesWritten != FitPropertySize) {
        Result = 1;
        goto WritePropertyEnd;
    }

    Result = 0;

WritePropertyEnd:
    if (FitProperty != NULL) {
        free(FitProperty);
    }

    return Result;
}

