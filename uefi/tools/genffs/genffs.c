/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    genffs.c

Abstract:

    This module implements a build tool that generates one EFI FFS file.

Author:

    Evan Green 6-Mar-2014

Environment:

    Build

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "uefifw.h"
#include "efiffs.h"
#include "peimage.h"

//
// --------------------------------------------------------------------- Macros
//

#define GFFS_LOG_ERROR(...)                   \
    fprintf(stderr, __VA_ARGS__); GffsErrorOccurred = TRUE;

#define GFFS_LOG_VERBOSE(...)                   \
    if (GffsDebugLevel >= LOG_LEVEL_VERBOSE) {  \
        fprintf(stderr, __VA_ARGS__);           \
    }

#define GFFS_LOG_DEBUG(...)                     \
    if (GffsDebugLevel >= LOG_LEVEL_DEBUG) {    \
        fprintf(stderr, __VA_ARGS__);           \
    }

//
// ---------------------------------------------------------------- Definitions
//

#define GFFS_STATUS_SUCCESS  0
#define GFFS_STATUS_WARNING  1
#define GFFS_STATUS_ERROR    2

#define UTILITY_NAME            "GenFfs"
#define UTILITY_MAJOR_VERSION 0
#define UTILITY_MINOR_VERSION 1

#define MAXIMUM_INPUT_FILE_COUNT     10

#define LOG_LEVEL_QUIET     0
#define LOG_LEVEL_DEFAULT   1
#define LOG_LEVEL_VERBOSE   2
#define LOG_LEVEL_DEBUG     3

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

EFI_STATUS
GffsGetSectionContents (
    CHAR8 **InputFileName,
    UINT32 *InputFileAlignment,
    UINT8 *InputFileSectionType,
    UINT32 InputFileCount,
    UINT8 *FileBuffer,
    UINT32 *BufferLength,
    UINT32 *MaxAlignment
    );

UINT8
GffsStringToType (
    CHAR8 *String
    );

UINT8
GffsStringToSectionType (
    CHAR8 *String
    );

EFI_STATUS
GffsStringtoAlignment (
    CHAR8  *AlignBuffer,
    UINT32 *AlignNumber
    );

EFI_STATUS
GffsStringToGuid (
    CHAR8 *AsciiGuidBuffer,
    EFI_GUID *GuidBuffer
    );

VOID
GffsConvertAsciiStringToUnicode (
    CHAR8 *String,
    CHAR16 *UnicodeString
    );

VOID
GffsCreateRandomGuid (
    EFI_GUID *Guid
    );

UINT8
GffsCalculateChecksum8 (
    UINT8 *Buffer,
    UINTN Size
    );

UINT8
GffsCalculateSum8 (
    UINT8 *Buffer,
    UINTN Size
    );

BOOLEAN
GffsCompareGuids (
    EFI_GUID *FirstGuid,
    EFI_GUID *SecondGuid
    );

VOID
GffsPrintVersion (
    VOID
    );

VOID
GffsPrintUsage (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

CHAR8 *GffsFileTypes[] = {
    NULL,
    "EFI_FV_FILETYPE_RAW",
    "EFI_FV_FILETYPE_FREEFORM",
    "EFI_FV_FILETYPE_SECURITY_CORE",
    "EFI_FV_FILETYPE_PEI_CORE",
    "EFI_FV_FILETYPE_DXE_CORE",
    "EFI_FV_FILETYPE_PEIM",
    "EFI_FV_FILETYPE_DRIVER",
    "EFI_FV_FILETYPE_COMBINED_PEIM_DRIVER",
    "EFI_FV_FILETYPE_APPLICATION",
    "EFI_FV_FILETYPE_SMM",
    "EFI_FV_FILETYPE_FIRMWARE_VOLUME_IMAGE",
    "EFI_FV_FILETYPE_COMBINED_SMM_DXE",
    "EFI_FV_FILETYPE_SMM_CORE"
};

CHAR8 *GffsFileSectionTypes[] = {
    NULL,
    "EFI_SECTION_COMPRESSION",
    "EFI_SECTION_GUID_DEFINED",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "EFI_SECTION_PE32",
    "EFI_SECTION_PIC",
    "EFI_SECTION_TE",
    "EFI_SECTION_DXE_DEPEX",
    "EFI_SECTION_VERSION",
    "EFI_SECTION_USER_INTERFACE",
    "EFI_SECTION_COMPATIBILITY16",
    "EFI_SECTION_FIRMWARE_VOLUME_IMAGE",
    "EFI_SECTION_FREEFORM_SUBTYPE_GUID",
    "EFI_SECTION_RAW",
    NULL,
    "EFI_SECTION_PEI_DEPEX",
    "EFI_SECTION_SMM_DEPEX"
};

CHAR8 *GffsAlignmentStrings[] = {
    "1", "2", "4", "8", "16", "32", "64", "128", "256", "512",
    "1K", "2K", "4K", "8K", "16K", "32K", "64K"
};

CHAR8 *GffsValidAlignmentStrings[] = {
    "8", "16", "128", "512", "1K", "4K", "32K", "64K"
};

UINT32 GffsValidAlignments[] = {0, 8, 16, 128, 512, 1024, 4096, 32768, 65536};

EFI_GUID GffsZeroGuid = {0};

UINTN GffsDebugLevel = LOG_LEVEL_DEFAULT;
BOOLEAN GffsErrorOccurred = FALSE;

//
// ------------------------------------------------------------------ Functions
//

int
main (
    int ArgumentCount,
    CHAR8 *Arguments[]
    )

/*++

Routine Description:

    This routine is the main entry point into the GenFFS utility, which creates
    a single FFS file from an input file.

Arguments:

    ArgumentCount - Supplies the number of command line parameters.

    Arguments - Supplies the array of pointers to parameter strings.

Return Value:

    GFFS_STATUS_SUCCESS - Utility exits successfully.

    GFFS_STATUS_ERROR - Some error occurred during execution.

--*/

{

    CHAR8 *AfterScan;
    UINT32 Alignment;
    EFI_FFS_FILE_ATTRIBUTES Attributes;
    UINT32 DefaultFileAlignment;
    UINT8 DefaultFileSectionType;
    FILE *FfsFile;
    EFI_FFS_FILE_HEADER2 FfsFileHeader;
    UINT8 *FileBuffer;
    EFI_GUID FileGuid;
    UINT32 FileSize;
    EFI_FV_FILETYPE FileType;
    UINT32 HeaderSize;
    UINT32 Index;
    UINT32 *InputFileAlignment;
    UINT32 InputFileCount;
    CHAR8 **InputFileName;
    UINT8 *InputFileSectionType;
    UINT64 LogLevel;
    UINT32 MaxAlignment;
    VOID *NewBuffer;
    UINTN NewSize;
    CHAR8 *OutputFileName;
    EFI_STATUS Status;

    LogLevel = 0;
    Index = 0;
    Attributes = 0;
    Alignment = 0;
    DefaultFileSectionType = 0;
    DefaultFileAlignment = 0;
    FileType = EFI_FV_FILETYPE_ALL;
    OutputFileName = NULL;
    InputFileCount = 0;
    InputFileName = NULL;
    InputFileAlignment = NULL;
    InputFileSectionType = NULL;
    FileBuffer = NULL;
    FileSize = 0;
    MaxAlignment = 1;
    FfsFile = NULL;
    Status = EFI_SUCCESS;
    srand(time(NULL) ^ getpid());
    memset(&FileGuid, 0, sizeof(EFI_GUID));
    if (ArgumentCount == 1) {
        GFFS_LOG_ERROR("Missing options.\n");
        GffsPrintUsage();
        return GFFS_STATUS_ERROR;
    }

    //
    // Parse the arguments.
    //

    ArgumentCount -= 1;
    Arguments += 1;
    if ((strcasecmp(Arguments[0], "-h") == 0) ||
        (strcasecmp(Arguments[0], "--help") == 0)) {

        GffsPrintVersion();
        GffsPrintUsage();
        return GFFS_STATUS_SUCCESS;
    }

    if (strcasecmp(Arguments[0], "--version") == 0) {
        GffsPrintVersion();
        return GFFS_STATUS_SUCCESS;
    }

    while (ArgumentCount > 0) {
        if ((strcasecmp(Arguments[0], "-t") == 0) ||
            (strcasecmp(Arguments[0], "--filetype") == 0)) {

            if ((Arguments[1] == NULL) || Arguments[1][0] == '-') {
                GFFS_LOG_ERROR("file type is missing for -t option");
                goto mainEnd;
            }

            FileType = GffsStringToType(Arguments[1]);
            if (FileType == EFI_FV_FILETYPE_ALL) {
                GFFS_LOG_ERROR("%s is not a valid file type.\n", Arguments[1]);
                goto mainEnd;
            }

            ArgumentCount -= 2;
            Arguments += 2;
            continue;
        }

        if ((strcasecmp(Arguments[0], "-o") == 0) ||
            (strcasecmp(Arguments[0], "--outputfile") == 0)) {

            if ((Arguments[1] == NULL) || (Arguments[1][0] == '-')) {
                GFFS_LOG_ERROR("Output file is missing for -o options.\n");
                goto mainEnd;
            }

            OutputFileName = Arguments[1];
            ArgumentCount -= 2;
            Arguments += 2;
            continue;
        }

        if ((strcasecmp(Arguments[0], "-g") == 0) ||
            (strcasecmp(Arguments[0], "--fileguid") == 0)) {

            Status = GffsStringToGuid(Arguments[1], &FileGuid);
            if (EFI_ERROR(Status)) {
                GFFS_LOG_ERROR("Invalid option value %s = %s.\n",
                               Arguments[0],
                               Arguments[1]);

                goto mainEnd;
            }

            ArgumentCount -= 2;
            Arguments += 2;
            continue;
        }

        if ((strcasecmp(Arguments[0], "-x") == 0) ||
            (strcasecmp(Arguments[0], "--fixed") == 0)) {

            Attributes |= FFS_ATTRIB_FIXED;
            ArgumentCount -= 1;
            Arguments += 1;
            continue;
        }

        if ((strcasecmp(Arguments[0], "-s") == 0) ||
            (strcasecmp(Arguments[0], "--checksum") == 0)) {

            Attributes |= FFS_ATTRIB_CHECKSUM;
            ArgumentCount -= 1;
            Arguments += 1;
            continue;
        }

        if ((strcasecmp(Arguments[0], "-a") == 0) ||
            (strcasecmp(Arguments[0], "--align") == 0)) {

            if ((Arguments[1] == NULL) || (Arguments[1][0] == '-')) {
                GFFS_LOG_ERROR("Align value is missing for -a option.\n");
                goto mainEnd;
            }

            for (Index = 0;
                 Index < sizeof(GffsValidAlignmentStrings) / sizeof(CHAR8 *);
                 Index += 1) {

                if (strcasecmp(Arguments[1],
                               GffsValidAlignmentStrings[Index]) == 0) {

                    break;
                }
            }

            if (Index == sizeof(GffsValidAlignmentStrings) / sizeof(CHAR8 *)) {
                if ((strcasecmp(Arguments[1], "1") == 0) ||
                    (strcasecmp(Arguments[1], "2") == 0) ||
                    (strcasecmp(Arguments[1], "4") == 0)) {

                    //
                    // 1, 2, 4 byte alignment same to 8 byte alignment
                    //

                    Index = 0;

                } else {
                    GFFS_LOG_ERROR(
                            "Invalid option %s = %s.\n",
                            Arguments[0],
                            Arguments[1]);

                    goto mainEnd;
                }
            }

            Alignment = Index;
            ArgumentCount -= 2;
            Arguments += 2;
            continue;
        }

        if ((strcasecmp(Arguments[0], "-n") == 0) ||
            (strcasecmp(Arguments[0], "--sectionalign") == 0)) {

            Arguments += 1;
            ArgumentCount -= 1;
            if (ArgumentCount == 0) {
                GFFS_LOG_ERROR("Error: -n requires an argument.\n");
                goto mainEnd;
            }

            Status = GffsStringtoAlignment(Arguments[0],
                                           &DefaultFileAlignment);

            if (EFI_ERROR(Status)) {
                GFFS_LOG_ERROR("Invalid default alignment.\n");
                goto mainEnd;
            }

            Arguments += 1;
            ArgumentCount -= 1;
            continue;
        }

        if ((strcasecmp(Arguments[0], "-r") == 0) ||
            (strcasecmp(Arguments[0], "--sectiontype") == 0)) {

            Arguments += 1;
            ArgumentCount -= 1;
            if (ArgumentCount == 0) {
                GFFS_LOG_ERROR("Error: -n requires an argument.\n");
                goto mainEnd;
            }

            DefaultFileSectionType = GffsStringToSectionType(Arguments[0]);
            if (DefaultFileSectionType == 0) {
                GFFS_LOG_ERROR("Invalid section type %s.\n",
                               Arguments[0]);

                goto mainEnd;
            }

            Arguments += 1;
            ArgumentCount -= 1;
            continue;
        }

        if ((strcasecmp(Arguments[0], "-v") == 0) ||
            (strcasecmp(Arguments[0], "--verbose") == 0)) {

            GffsDebugLevel = LOG_LEVEL_VERBOSE;
            GFFS_LOG_VERBOSE("Verbose output Mode Set!\n");
            ArgumentCount -= 1;
            Arguments += 1;
            continue;
        }

        if ((strcasecmp(Arguments[0], "-q") == 0) ||
            (strcasecmp(Arguments[0], "--quiet") == 0)) {

            GffsDebugLevel = LOG_LEVEL_QUIET;
            ArgumentCount -= 1;
            Arguments += 1;
            continue;
        }

        if ((strcasecmp(Arguments[0], "-d") == 0) ||
            (strcasecmp(Arguments[0], "--debug") == 0)) {

            LogLevel = strtoul(Arguments[1], &AfterScan, 0);
            if (AfterScan == Arguments[1]) {
                GFFS_LOG_ERROR(
                        "Invalid option value %s = %s.\n",
                        Arguments[0],
                        Arguments[1]);

                goto mainEnd;
            }

            if (LogLevel > 9) {
                GFFS_LOG_ERROR(
                        "Debug Level range is 0-9, current input level is %d",
                        (int)LogLevel);

                goto mainEnd;
            }

            GffsDebugLevel = LOG_LEVEL_DEBUG;
            GFFS_LOG_DEBUG("Debug Output Mode Level %s is set!\n",
                           Arguments[1]);

            ArgumentCount -= 2;
            Arguments += 2;
            continue;
        }

        if ((strcasecmp(Arguments[0], "-i") == 0) ||
            (strcasecmp(Arguments[0], "--sectionfile") == 0) ||
            (Arguments[0][0] != '-')) {

            if (Arguments[0][0] == '-') {
                Arguments += 1;
                ArgumentCount -= 1;
            }

            //
            // Get the input file name and its alignment.
            //

            if ((ArgumentCount == 0) || (Arguments[0][0] == '-')) {
                GFFS_LOG_ERROR("Input section file is missing for -i "
                               "option.\n");

                goto mainEnd;
            }

            //
            // Allocate Input file name buffer and its alignment buffer.
            //

            if (InputFileCount % MAXIMUM_INPUT_FILE_COUNT == 0) {
                NewSize = (InputFileCount + MAXIMUM_INPUT_FILE_COUNT) *
                          sizeof(CHAR8 *);

                NewBuffer = (CHAR8 **)realloc(InputFileName, NewSize);
                if (NewBuffer == NULL) {
                    GffsErrorOccurred = TRUE;
                    goto mainEnd;
                }

                InputFileName = NewBuffer;
                memset(&(InputFileName[InputFileCount]),
                       0,
                       (MAXIMUM_INPUT_FILE_COUNT * sizeof(CHAR8 *)));

                NewSize = (InputFileCount + MAXIMUM_INPUT_FILE_COUNT) *
                          sizeof(UINT32);

                NewBuffer = (UINT32 *)realloc(InputFileAlignment, NewSize);
                if (NewBuffer == NULL) {
                    GffsErrorOccurred = TRUE;
                    goto mainEnd;
                }

                InputFileAlignment = NewBuffer;
                memset(&(InputFileAlignment[InputFileCount]),
                       0,
                       (MAXIMUM_INPUT_FILE_COUNT * sizeof(UINT32)));

                NewSize = (InputFileCount + MAXIMUM_INPUT_FILE_COUNT) *
                          sizeof(UINT8);

                NewBuffer = (UINT8 *)realloc(InputFileSectionType, NewSize);
                if (NewBuffer == NULL) {
                    GffsErrorOccurred = TRUE;
                    goto mainEnd;
                }

                InputFileSectionType = NewBuffer;
                memset(&(InputFileSectionType[InputFileCount]),
                       0,
                       (MAXIMUM_INPUT_FILE_COUNT * sizeof(UINT8)));
            }

            InputFileName[InputFileCount] = Arguments[0];
            ArgumentCount -= 1;
            Arguments += 1;

            //
            // Assign the default alignment and type.
            //

            InputFileAlignment[InputFileCount] = DefaultFileAlignment;
            InputFileSectionType[InputFileCount] = DefaultFileSectionType;
            if (ArgumentCount <= 0) {
                InputFileCount += 1;
                break;
            }

            //
            // Process a section file alignment requirement.
            //

            if ((strcasecmp(Arguments[0], "-n") == 0) ||
                (strcasecmp(Arguments[0], "--sectionalign") == 0)) {

                Status = GffsStringtoAlignment(
                                        Arguments[1],
                                        &(InputFileAlignment[InputFileCount]));

                if (EFI_ERROR(Status)) {
                    GFFS_LOG_ERROR("Invalid option value %s = %s.\n",
                                   Arguments[0],
                                   Arguments[1]);

                    goto mainEnd;
                }

                ArgumentCount -= 2;
                Arguments += 2;
            }

            //
            // Process a section type.
            //

            if ((strcasecmp(Arguments[0], "-r") == 0) ||
                (strcasecmp(Arguments[0], "--sectiontype") == 0)) {

                InputFileSectionType[InputFileCount] =
                                        GffsStringToSectionType(Arguments[1]);

                if (InputFileSectionType[InputFileCount] == 0) {
                    GFFS_LOG_ERROR("Invalid section type %s.\n",
                                   Arguments[1]);

                    goto mainEnd;
                }

                ArgumentCount -= 2;
                Arguments += 2;
            }

            InputFileCount += 1;
            continue;
        }

        GFFS_LOG_ERROR("Unknown option %s.\n", Arguments[0]);
        goto mainEnd;
    }

    GFFS_LOG_VERBOSE("%s tool start.", UTILITY_NAME);

    //
    // Check the complete input paramters.
    //

    if (FileType == EFI_FV_FILETYPE_ALL) {
        GFFS_LOG_ERROR("Missing option filetype.\n");
        goto mainEnd;
    }

    if (GffsCompareGuids(&FileGuid, &GffsZeroGuid) != FALSE) {
        GFFS_LOG_VERBOSE("Creating random GUID for the file.\n");
        GffsCreateRandomGuid(&FileGuid);
    }

    if (InputFileCount == 0) {
        GFFS_LOG_ERROR("Missing option input files.\n");
        goto mainEnd;
    }

    //
    // Output input parameter information
    //

    GFFS_LOG_VERBOSE("Fv File type is %s\n", GffsFileTypes[FileType]);
    GFFS_LOG_VERBOSE("Output file name is %s\n", OutputFileName);
    GFFS_LOG_VERBOSE("FFS File Guid is "
                     "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X\n",
                     (unsigned)FileGuid.Data1,
                     FileGuid.Data2,
                     FileGuid.Data3,
                     FileGuid.Data4[0],
                     FileGuid.Data4[1],
                     FileGuid.Data4[2],
                     FileGuid.Data4[3],
                     FileGuid.Data4[4],
                     FileGuid.Data4[5],
                     FileGuid.Data4[6],
                     FileGuid.Data4[7]);

    if ((Attributes & FFS_ATTRIB_FIXED) != 0) {
        GFFS_LOG_VERBOSE("FFS File has the fixed file attribute\n");
    }

    if ((Attributes & FFS_ATTRIB_CHECKSUM) != 0) {
        GFFS_LOG_VERBOSE("FFS File requires the checksum of the whole file\n");
    }

    GFFS_LOG_VERBOSE("FFS file alignment is %s\n",
                     GffsValidAlignmentStrings[Alignment]);

    for (Index = 0; Index < InputFileCount; Index += 1) {
        if (InputFileAlignment[Index] == 0) {
            InputFileAlignment[Index] = 1;
        }

        GFFS_LOG_VERBOSE("the %dth input section name is %s and section "
                         "alignment s %u\n",
                         Index,
                         InputFileName[Index],
                         (unsigned)InputFileAlignment[Index]);
    }

    //
    // Calculate the size of all input section files.
    //

    Status = GffsGetSectionContents(InputFileName,
                                    InputFileAlignment,
                                    InputFileSectionType,
                                    InputFileCount,
                                    FileBuffer,
                                    &FileSize,
                                    &MaxAlignment);

    if (Status == EFI_BUFFER_TOO_SMALL) {
        FileBuffer = (UINT8 *)malloc(FileSize);
        if (FileBuffer == NULL) {
            goto mainEnd;
        }

        memset(FileBuffer, 0, FileSize);

        //
        // Read all input file contents into a buffer.
        //

        Status = GffsGetSectionContents(InputFileName,
                                        InputFileAlignment,
                                        InputFileSectionType,
                                        InputFileCount,
                                        FileBuffer,
                                        &FileSize,
                                        &MaxAlignment);
    }

    if (EFI_ERROR(Status)) {
        goto mainEnd;
    }

    //
    // Create the Ffs file header.
    //

    memset(&FfsFileHeader, 0, sizeof(EFI_FFS_FILE_HEADER2));
    memcpy(&(FfsFileHeader.Name), &FileGuid, sizeof(EFI_GUID));
    FfsFileHeader.Type = FileType;

    //
    // Update the FFS Alignment based on the max alignment required by input
    // section files.
    //

    GFFS_LOG_VERBOSE("the max alignment of all input sections is %u\n",
                     (unsigned)MaxAlignment);

    for (Index = 0;
         Index < sizeof(GffsValidAlignments) / sizeof(UINT32) - 1;
         Index += 1) {

        if ((MaxAlignment > GffsValidAlignments[Index]) &&
            (MaxAlignment <= GffsValidAlignments[Index + 1])) {

            break;
        }
    }

    if (Alignment < Index) {
        Alignment = Index;
    }

    GFFS_LOG_VERBOSE("the alignment of the generated FFS file is %u\n",
                     (unsigned)GffsValidAlignments[Alignment + 1]);

    //
    // Now the file size includes the EFI_FFS_FILE_HEADER.
    //

    if (FileSize + sizeof(EFI_FFS_FILE_HEADER) >= MAX_FFS_SIZE) {
        HeaderSize = sizeof(EFI_FFS_FILE_HEADER2);
        FileSize += sizeof(EFI_FFS_FILE_HEADER2);
        FfsFileHeader.ExtendedSize = FileSize;
        Attributes |= FFS_ATTRIB_LARGE_FILE;

    } else {
        HeaderSize = sizeof(EFI_FFS_FILE_HEADER);
        FileSize += sizeof(EFI_FFS_FILE_HEADER);
        FfsFileHeader.Size[0] = (UINT8)(FileSize & 0xFF);
        FfsFileHeader.Size[1] = (UINT8)((FileSize & 0xFF00) >> 8);
        FfsFileHeader.Size[2] = (UINT8)((FileSize & 0xFF0000) >> 16);
    }

    GFFS_LOG_VERBOSE("the size of the generated FFS file is %u bytes\n",
                     (unsigned)FileSize);

    FfsFileHeader.Attributes =
                      (EFI_FFS_FILE_ATTRIBUTES)(Attributes | (Alignment << 3));

    //
    // Fill in checksums and state, these must be zero for checksumming
    //
    // FileHeader.IntegrityCheck.Checksum.Header = 0;
    // FileHeader.IntegrityCheck.Checksum.File = 0;
    // FileHeader.State = 0;
    //

    assert((FfsFileHeader.IntegrityCheck.Checksum.Header == 0) &&
           (FfsFileHeader.IntegrityCheck.Checksum.File == 0) &&
           (FfsFileHeader.State == 0));

    FfsFileHeader.IntegrityCheck.Checksum.Header = GffsCalculateChecksum8(
                                                       (UINT8 *)&FfsFileHeader,
                                                       HeaderSize);

    if ((FfsFileHeader.Attributes & FFS_ATTRIB_CHECKSUM) != 0) {

        //
        // The Ffs header checksum is zero, so just calculate the CRC on the
        // FFS body.
        //

        FfsFileHeader.IntegrityCheck.Checksum.File = GffsCalculateChecksum8(
                                                        FileBuffer,
                                                        FileSize - HeaderSize);

    } else {
        FfsFileHeader.IntegrityCheck.Checksum.File = FFS_FIXED_CHECKSUM;
    }

    FfsFileHeader.State = EFI_FILE_HEADER_CONSTRUCTION |
                          EFI_FILE_HEADER_VALID | EFI_FILE_DATA_VALID;

    if (OutputFileName == NULL) {
        GFFS_LOG_ERROR("Error: output file was not specified.\n");
        goto mainEnd;
    }

    FfsFile = fopen(OutputFileName, "wb");
    if (FfsFile == NULL) {
        GFFS_LOG_ERROR("Error opening output file %s.\n", OutputFileName);
        goto mainEnd;
    }

    //
    // Write the header.
    //

    fwrite(&FfsFileHeader, 1, HeaderSize, FfsFile);

    //
    // Write the data.
    //

    fwrite(FileBuffer, 1, FileSize - HeaderSize, FfsFile);
    fclose(FfsFile);

mainEnd:
    if (InputFileName != NULL) {
        free(InputFileName);
    }

    if (InputFileAlignment != NULL) {
        free(InputFileAlignment);
    }

    if (InputFileSectionType != NULL) {
        free(InputFileSectionType);
    }

    if (FileBuffer != NULL) {
        free(FileBuffer);
    }

    //
    // If any errors were reported via the standard error reporting
    // routines, then the status has been saved. Get the value and
    // return it to the caller.
    //

    GFFS_LOG_VERBOSE("%s tool done with return code is 0x%x.\n",
                     UTILITY_NAME,
                     GffsErrorOccurred);

    return GffsErrorOccurred;
}

//
// --------------------------------------------------------- Internal Functions
//

EFI_STATUS
GffsGetSectionContents (
    CHAR8 **InputFileName,
    UINT32 *InputFileAlignment,
    UINT8 *InputFileSectionType,
    UINT32 InputFileCount,
    UINT8 *FileBuffer,
    UINT32 *BufferLength,
    UINT32 *MaxAlignment
    )

/*++

Routine Description:

    This routine gets the contents of all the given section files.

Arguments:

    InputFileName - Supplies the name of the input file.

    InputFileAlignment - Supplies an array of alignments for each input file.

    InputFileSectionType - Supplies an array of section types for each input
        file.

    InputFileCount - Supplies the number of input files. This should be at
        least 1.

    FileBuffer - Supplies buffer where the data will be returned.

    BufferLength - Supplies a pointer that on input contains the size of the
        file buffer. On output, this is the actual length of the data.

    MaxAlignment - Supplies the max alignment required by all the input file
        datas.

Return Value:

    EFI_SUCCESS on successful return.

    EFI_INVALID_PARAMETER if the input file count is less than 1 or buffer
    length pointer is NULL.

    EFI_ABORTED if unable to open input file.

    EFI_BUFFER_TOO_SMALL if the file buffer is not enough to contain all file
    data.

--*/

{

    UINT32 FileSize;
    UINT32 HeaderSize;
    UINT32 Index;
    FILE *InFile;
    size_t ItemsRead;
    EFI_COMMON_SECTION_HEADER2 *SectionHeader;
    UINT32 SectionSize;
    UINT32 Size;
    EFI_VERSION_SECTION *VersionSection;

    Size = 0;

    //
    // Go through the array of file names and copy their contents to the output
    // buffer.
    //

    for (Index = 0; Index < InputFileCount; Index += 1) {

        //
        // Make sure the section ends on a DWORD boundary.
        //

        while ((Size & 0x03) != 0) {
            Size += 1;
        }

        //
        // Get the Max alignment of all input file data.
        //

        if (*MaxAlignment < InputFileAlignment[Index]) {
            *MaxAlignment = InputFileAlignment[Index];
        }

        InFile = NULL;
        if (InputFileAlignment[Index] != 1) {
            GFFS_LOG_ERROR("Error: File alignment is not supported.\n");
            return EFI_UNSUPPORTED;
        }

        //
        // Compute the header size.
        //

        switch (InputFileSectionType[Index]) {
        case 0:
            GFFS_LOG_ERROR("Error: File %s missing section type.\n",
                           InputFileName[Index]);

            return EFI_UNSUPPORTED;

        case EFI_SECTION_COMPRESSION:
        case EFI_SECTION_GUID_DEFINED:
            GFFS_LOG_ERROR("Error: Encapsulation sections not supported.\n");
            return EFI_UNSUPPORTED;

        case EFI_SECTION_PE32:
        case EFI_SECTION_PIC:
        case EFI_SECTION_TE:
        case EFI_SECTION_DXE_DEPEX:
        case EFI_SECTION_COMPATIBILITY16:
        case EFI_SECTION_FIRMWARE_VOLUME_IMAGE:
        case EFI_SECTION_FREEFORM_SUBTYPE_GUID:
        case EFI_SECTION_RAW:
        case EFI_SECTION_PEI_DEPEX:

            //
            // Open the file and read its contents.
            //

            InFile = fopen(InputFileName[Index], "rb");
            if (InFile == NULL) {
                GFFS_LOG_ERROR("Error opening file %s.\n",
                               InputFileName[Index]);

                return EFI_ABORTED;
            }

            fseek(InFile, 0, SEEK_END);
            FileSize = ftell(InFile);
            fseek(InFile, 0, SEEK_SET);
            GFFS_LOG_DEBUG("the input section name is %s and the size is %u "
                           "bytes.\n",
                           InputFileName[Index],
                           (unsigned)FileSize);

            if (FileSize >= MAX_FFS_SIZE) {
                HeaderSize = sizeof(EFI_COMMON_SECTION_HEADER2);

            } else {
                HeaderSize = sizeof(EFI_COMMON_SECTION_HEADER);
            }

            //
            // Write the section header out to the buffer.
            //

            if ((FileBuffer != NULL) && (Size + HeaderSize <= *BufferLength)) {
                SectionHeader =
                             (EFI_COMMON_SECTION_HEADER2 *)(FileBuffer + Size);

                SectionSize = FileSize + HeaderSize;
                if (FileSize >= MAX_FFS_SIZE) {
                    SectionHeader->Elements.ExtendedSize = FileSize;

                } else {
                    SectionHeader->AsUint32 = SectionSize & 0x00FFFFFF;
                }

                SectionHeader->Elements.Type = InputFileSectionType[Index];
            }

            Size += HeaderSize;

            //
            // Write the file contents out to the buffer.
            //

            if ((FileSize > 0) && (FileBuffer != NULL) &&
                ((Size + FileSize) <= *BufferLength)) {

                ItemsRead = fread(FileBuffer + Size,
                                  (size_t)FileSize,
                                  1,
                                  InFile);

                if (ItemsRead != 1) {
                    GFFS_LOG_ERROR("Error reading file %s.\n",
                                   InputFileName[Index]);

                    fclose(InFile);
                    return EFI_ABORTED;
                }
            }

            fclose(InFile);
            Size += FileSize;
            break;

        //
        // For version sections, the file name is actually the version
        // information.
        //

        case EFI_SECTION_VERSION:
            HeaderSize = sizeof(EFI_VERSION_SECTION);
            FileSize = (strlen(InputFileName[Index]) + 1) * sizeof(CHAR16);
            SectionSize = HeaderSize + FileSize;
            if ((FileBuffer != NULL) && (Size + HeaderSize <= *BufferLength)) {
                VersionSection = (EFI_VERSION_SECTION *)(FileBuffer + Size);
                VersionSection->CommonHeader.AsUint32 =
                                                      SectionSize & 0x00FFFFFF;

                VersionSection->CommonHeader.Elements.Type =
                                                           EFI_SECTION_VERSION;

                VersionSection->BuildNumber = strtoul(InputFileName[Index],
                                                      NULL,
                                                      0);
            }

            Size += HeaderSize;
            if ((FileSize > 0) && (FileBuffer != NULL) &&
                ((Size + FileSize) <= *BufferLength)) {

                GffsConvertAsciiStringToUnicode(InputFileName[Index],
                                                (CHAR16 *)(FileBuffer + Size));
            }

            Size += FileSize;
            break;

        //
        // For user interface sections, the file name is the value of the
        // section.
        //

        case EFI_SECTION_USER_INTERFACE:
            HeaderSize = sizeof(EFI_COMMON_SECTION_HEADER);
            FileSize = (strlen(InputFileName[Index]) + 1) * sizeof(CHAR16);
            SectionSize = HeaderSize + FileSize;
            if ((FileBuffer != NULL) && (Size + HeaderSize <= *BufferLength)) {
                SectionHeader =
                             (EFI_COMMON_SECTION_HEADER2 *)(FileBuffer + Size);

                SectionHeader->AsUint32 = SectionSize & 0x00FFFFFF;
                SectionHeader->Elements.Type = InputFileSectionType[Index];
            }

            Size += HeaderSize;
            if ((FileSize > 0) && (FileBuffer != NULL) &&
                ((Size + FileSize) <= *BufferLength)) {

                GffsConvertAsciiStringToUnicode(InputFileName[Index],
                                                (CHAR16 *)(FileBuffer + Size));
            }

            Size += FileSize;
            break;

        default:
            GFFS_LOG_ERROR("Error: Unsupported section type %d.\n",
                           InputFileSectionType[Index]);

            return EFI_UNSUPPORTED;
        }
    }

    //
    // Set the actual length of the data.
    //

    if (Size > *BufferLength) {
        *BufferLength = Size;
        return EFI_BUFFER_TOO_SMALL;
    }

    *BufferLength = Size;
    return EFI_SUCCESS;
}

UINT8
GffsStringToType (
    CHAR8 *String
    )

/*++

Routine Description:

    This routine converts a file type string to a file type value. The value
    EFI_FV_FILETYPE_ALL indicates that an unrecognized file type was specified.

Arguments:

    String - Supplies the file type string.

Return Value:

    Returns the file type value.

--*/

{

    UINTN Index;

    if (String == NULL) {
        return EFI_FV_FILETYPE_ALL;
    }

    for (Index = 0;
         Index < sizeof(GffsFileTypes) / sizeof(CHAR8 *);
         Index += 1) {

        if ((GffsFileTypes[Index] != NULL) &&
            (strcasecmp(String, GffsFileTypes[Index]) == 0)) {

            return Index;
        }
    }

    return EFI_FV_FILETYPE_ALL;
}

UINT8
GffsStringToSectionType (
    CHAR8 *String
    )

/*++

Routine Description:

    This routine converts a file section type string to a file section value.

Arguments:

    String - Supplies the file section string.

Return Value:

    Returns the file section value.

--*/

{

    UINTN Index;

    if (String == NULL) {
        return EFI_FV_FILETYPE_ALL;
    }

    for (Index = 0;
         Index < sizeof(GffsFileSectionTypes) / sizeof(CHAR8 *);
         Index += 1) {

        if ((GffsFileSectionTypes[Index] != NULL) &&
            (strcasecmp(String, GffsFileSectionTypes[Index]) == 0)) {

            return Index;
        }
    }

    return 0;
}

EFI_STATUS
GffsStringtoAlignment (
    CHAR8  *AlignBuffer,
    UINT32 *AlignNumber
    )

/*++

Routine Description:

    This routine converts an alignment string to an alignment value in the
    range of 1 to 64K.

Arguments:

    AlignBuffer - Supplies a pointer to the alignment string.

    AlignNumber - Supplies a pointer where the alignment value will be returned
        on success.

Return Value:

    EFI_SUCCESS if a value was converted successfully.

    EFI_INVALID_PARAMETER if the alignment string is invalid or align value is
    not in scope.

--*/

{

    UINT32 Index;

    if (AlignBuffer == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    for (Index = 0;
         Index < sizeof(GffsAlignmentStrings) / sizeof(CHAR8 *);
         Index += 1) {

        if (strcasecmp(AlignBuffer, GffsAlignmentStrings[Index]) == 0) {
            *AlignNumber = 1 << Index;
            return EFI_SUCCESS;
        }
    }

    return EFI_INVALID_PARAMETER;
}

EFI_STATUS
GffsStringToGuid (
    CHAR8 *AsciiGuidBuffer,
    EFI_GUID *GuidBuffer
    )

/*++

Routine Description:

    This routine converts a string to an EFI_GUID.  The string must be in the
    xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx format.

Arguments:

    AsciiGuidBuffer - Supplies a pointer to ascii string.

    GuidBuffer - Supplies a pointer where the GUID will be returned on
        success.

Return Value:

    EFI_ABORTED if the string could not be converted.

    EFI_SUCCESS if the string was successfully converted.

    EFI_INVALID_PARAMETER if the input parameter is not valid.

--*/

{

    unsigned Data1;
    unsigned Data2;
    unsigned Data3;
    unsigned Data4[8];
    INT32 Index;

    if (AsciiGuidBuffer == NULL || GuidBuffer == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    //
    // Check that the Guid Format is strictly
    // xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    //

    for (Index = 0;
         (AsciiGuidBuffer[Index] != '\0') && (Index < 37);
         Index += 1) {

        if ((Index == 8) || (Index == 13) || (Index == 18) || (Index == 23)) {
            if (AsciiGuidBuffer[Index] != '-') {
                break;
            }

        } else {
            if (((AsciiGuidBuffer[Index] >= '0') &&
                 (AsciiGuidBuffer[Index] <= '9')) ||
                ((AsciiGuidBuffer[Index] >= 'a') &&
                 (AsciiGuidBuffer[Index] <= 'f')) ||
                ((AsciiGuidBuffer[Index] >= 'A') &&
                 (AsciiGuidBuffer[Index] <= 'F'))) {

                continue;

            } else {
                break;
            }
        }
    }

    if ((Index < 36) || (AsciiGuidBuffer[36] != '\0')) {
        GFFS_LOG_ERROR("Incorrect GUID \"%s\"\n"
                       "Correct Format \"xxxxxxxx-xxxx-xxxx-xxxx-"
                       "xxxxxxxxxxxx\".\n",
                       AsciiGuidBuffer);

        return EFI_ABORTED;
    }

    //
    // Scan the guid string into the buffer.
    //

    Index = sscanf(AsciiGuidBuffer,
                   "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                   &Data1,
                   &Data2,
                   &Data3,
                   &Data4[0],
                   &Data4[1],
                   &Data4[2],
                   &Data4[3],
                   &Data4[4],
                   &Data4[5],
                   &Data4[6],
                   &Data4[7]);

    //
    // Verify the correct number of items were scanned.
    //

    if (Index != 11) {
        GFFS_LOG_ERROR("Incorrect GUID \"%s\"\n"
                       "Correct Format \"xxxxxxxx-xxxx-xxxx-xxxx-"
                       "xxxxxxxxxxxx\".\n",
                       AsciiGuidBuffer);

        return EFI_ABORTED;
    }

    //
    // Copy the data into the output GUID.
    //

    GuidBuffer->Data1 = (UINT32)Data1;
    GuidBuffer->Data2 = (UINT16)Data2;
    GuidBuffer->Data3 = (UINT16)Data3;
    GuidBuffer->Data4[0] = (UINT8)Data4[0];
    GuidBuffer->Data4[1] = (UINT8)Data4[1];
    GuidBuffer->Data4[2] = (UINT8)Data4[2];
    GuidBuffer->Data4[3] = (UINT8)Data4[3];
    GuidBuffer->Data4[4] = (UINT8)Data4[4];
    GuidBuffer->Data4[5] = (UINT8)Data4[5];
    GuidBuffer->Data4[6] = (UINT8)Data4[6];
    GuidBuffer->Data4[7] = (UINT8)Data4[7];
    return EFI_SUCCESS;
}

VOID
GffsConvertAsciiStringToUnicode (
    CHAR8 *String,
    CHAR16 *UnicodeString
    )

/*++

Routine Description:

    This routine converts and ASCII string into a unicode string.

Arguments:

    String - Supplies a pointer to the ASCII string.

    UnicodeString - Supplies a pointer to the Unicode string.

Return Value:

    None.

--*/

{

    while (*String != '\0') {
        *UnicodeString = (CHAR16)*String;
        String += 1;
        UnicodeString += 1;
    }

    *UnicodeString = L'\0';
}

VOID
GffsCreateRandomGuid (
    EFI_GUID *Guid
    )

/*++

Routine Description:

    This routine creates a random GUID.

Arguments:

    Guid - Supplies a pointer where the GUID will be returned.

Return Value:

    None.

--*/

{

    UINT16 *Buffer;
    UINTN BufferSize;
    int Value;

    Buffer = (UINT16 *)Guid;
    BufferSize = sizeof(EFI_GUID) / sizeof(UINT16);
    while (BufferSize != 0) {
        Value = rand();
        *Buffer = Value;
        Buffer += 1;
        BufferSize -= 1;
    }

    return;
}

UINT8
GffsCalculateChecksum8 (
    UINT8 *Buffer,
    UINTN Size
    )

/*++

Routine Description:

    This routine calculates the value needed for a valid UINT8 checksum.

Arguments:

    Buffer - Supplies a pointer to a buffer containing byte data.

    Size - Supplies the size of the buffer.

Return Value:

    Returns the 8-bit checksum of the field.

--*/

{

    return (UINT8)(0x100 - GffsCalculateSum8(Buffer, Size));
}

UINT8
GffsCalculateSum8 (
    UINT8 *Buffer,
    UINTN Size
    )

/*++

Routine Description:

    This routine calculates the sum of the bytes in the given buffer.

Arguments:

    Buffer - Supplies a pointer to a buffer containing byte data.

    Size - Supplies the size of the buffer.

Return Value:

    Returns the 8-bit sum of each byte in the buffer.

--*/

{

    UINTN Index;
    UINT8 Sum;

    Sum = 0;
    for (Index = 0; Index < Size; Index += 1) {
        Sum = (UINT8)(Sum + Buffer[Index]);
    }

    return Sum;
}

BOOLEAN
GffsCompareGuids (
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

VOID
GffsPrintVersion (
    VOID
    )

/*++

Routine Description:

    This routine prints the utility version information.

Arguments:

    None.

Return Value:

    None.

--*/

{

    fprintf(stdout,
            "%s Version %d.%d\n",
            UTILITY_NAME,
            UTILITY_MAJOR_VERSION,
            UTILITY_MINOR_VERSION);

    return;
}

VOID
GffsPrintUsage (
    VOID
    )

/*++

Routine Description:

    This routine prints application usage information.

Arguments:

    None.

Return Value:

    None.

--*/

{

    fprintf(stdout,
            "\n%s Creates a single FFS file from one or more input files.\n",
            UTILITY_NAME);

    fprintf(stdout, "\nUsage: %s [options] [files...]\n\n", UTILITY_NAME);
    fprintf(stdout, "Options:\n");
    fprintf(stdout,
            "  -r SectionType, --sectiontype SectionType\n"
            "Define the section type of the input file just specified. \n"
            "Valid values are EFI_SECTION_COMPRESSION, \n"
            "EFI_SECTION_GUID_DEFINED, EFI_SECTION_PE32, EFI_SECTION_PIC, \n"
            "EFI_SECTION_TE, EFI_SECTION_DXE_DEPEX, \n"
            "EFI_SECTION_COMPATIBILITY16, EFI_SECTION_USER_INTERFACE, \n"
            "EFI_SECTION_VERSION, EFI_SECTION_FIRMWARE_VOLUME_IMAGE, \n"
            "EFI_SECTION_RAW, EFI_SECTION_FREEFORM_SUBTYPE_GUID, \n"
            "EFI_SECTION_PEI_DEPEX, EFI_SECTION_SMM_DEPEX.\n\n"
            "  -o FileName, --outputfile FileName\n"
            "File is FFS file to be created.\n"
            "  -t Type, --filetype Type\n"
            "Type is one FV file type defined in PI spec, which is\n"
            "EFI_FV_FILETYPE_RAW, EFI_FV_FILETYPE_FREEFORM,\n"
            "EFI_FV_FILETYPE_SECURITY_CORE, EFI_FV_FILETYPE_PEIM,\n"
            "EFI_FV_FILETYPE_PEI_CORE, EFI_FV_FILETYPE_DXE_CORE,\n"
            "EFI_FV_FILETYPE_DRIVER, EFI_FV_FILETYPE_APPLICATION,\n"
            "EFI_FV_FILETYPE_COMBINED_PEIM_DRIVER,\n"
            "EFI_FV_FILETYPE_SMM, EFI_FV_FILETYPE_SMM_CORE,\n"
            "EFI_FV_FILETYPE_COMBINED_SMM_DXE, \n"
            "EFI_FV_FILETYPE_FIRMWARE_VOLUME_IMAGE.\n\n"
            "  -g FileGuid, --fileguid FileGuid\n"
            "FileGuid is one module guid.\n"
            "Its format is xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx\n"
            "  -x, --fixed           Indicates that the file may not be moved\n"
            "from its present location.\n"
            "  -s, --checksum        Indicates to calculate file checksum.\n"
            "  -a FileAlign, --align FileAlign\n"
            "FileAlign points to file alignment, which only support\n"
            "the following align: 1,2,4,8,16,128,512,1K,4K,32K,64K\n"
            "  -i SectionFile, --sectionfile SectionFile\n"
            "Section file will be contained in this FFS file.\n"
            "  -n SectionAlign, --sectionalign SectionAlign\n"
            "SectionAlign points to section alignment, which support\n"
            "the alignment scope 1~64K. It is specified together\n"
            "with sectionfile to point its alignment in FFS file.\n"
            "  -v, --verbose         Turn on verbose output with informational "
            "messages.\n"
            "  -q, --quiet           Disable all messages except key message "
            "and fatal error\n"
            "  -d, --debug level     Enable debug messages, at input debug "
            "level.\n"
            "  --version             Show program's version number "
            "and exit.\n"
            "  -h, --help            Show this help message and exit.\n");

    return;
}

