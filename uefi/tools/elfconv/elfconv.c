/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    elfconv.c

Abstract:

    This module implements a UEFI build utility that converts an ELF image
    into a PE image.

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
#include "elfconv.h"

//
// ---------------------------------------------------------------- Definitions
//

#define ELFCONV_VERSION_MAJOR 1
#define ELFCONV_VERSION_MINOR 0

#define ELFCONV_USAGE                                                          \
    "Usage: ElfConv [options] [files...]\n"                                    \
    "The ElfConv utility takes an ELF file as input and produces a PE image.\n"\
    "Dynamic linking is not supported. Valid options are:\n"                   \
    "  -o, --output=File -- Specify the output image name. The default is \n"  \
    "      the name of the input image followed by .efi\n"                     \
    "  -t, --type=type -- Specify the EFI subsystem type. Valid values are \n" \
    "      efiapp, efibootdriver, efiruntimedriver, efidriver, or a \n"        \
    "      numeric value.\n"                                                   \
    "  -v, --verbose -- Print extra information.\n"                            \
    "  --help -- Print this help and exit.\n"                                  \
    "  --version -- Print version information and exit.\n"                     \

#define ELFCONV_OPTIONS_STRING "o:t:v"

#define ELFCONV_RELOCATION_EXPANSION_SIZE (2 * 0x1000)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

int
ElfconvLoadFile (
    PELFCONV_CONTEXT Context
    );

BOOLEAN
ElfconvIsElfHeader (
    VOID *File,
    UINTN FileSize
    );

//
// -------------------------------------------------------------------- Globals
//

struct option ElfconvLongOptions[] = {
    {"output", required_argument, 0, 'o'},
    {"type", required_argument, 0, 't'},
    {"verbose", no_argument, 0, 'v'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

CHAR8 *ElfconvDebugSections[] = {
    ".stab",
    ".stabstr",
    ".debug_aranges",
    ".debug_info",
    ".debug_abbrev",
    ".debug_frame",
    ".debug_line",
    ".debug_str",
    ".debug_loc",
    ".debug_ranges",
    ".debug_macinfo",
    ".debug_pubtypes",
    ".eh_frame",
    NULL
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

    This routine is the main entry point into the ElfConv utility, which
    converts and ELF binary into an EFI PE binary.

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
    ssize_t BytesWritten;
    ELFCONV_CONTEXT Context;
    CHAR8 *DefaultOutputName;
    UINT8 ElfClass;
    ELFCONV_FUNCTION_TABLE FunctionTable;
    size_t InputNameLength;
    int Option;
    FILE *Output;
    BOOLEAN Result;
    int Status;

    memset(&Context, 0, sizeof(ELFCONV_CONTEXT));
    Context.SubsystemType = -1;
    memset(&FunctionTable, 0, sizeof(ELFCONV_FUNCTION_TABLE));
    DefaultOutputName = NULL;
    Output = NULL;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             ELFCONV_OPTIONS_STRING,
                             ElfconvLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto mainEnd;
        }

        switch (Option) {
        case 't':
            if (strcasecmp(optarg, "efiapp") == 0) {
                Context.SubsystemType = EFI_IMAGE_SUBSYSTEM_EFI_APPLICATION;

            } else if (strcasecmp(optarg, "efibootdriver") == 0) {
                Context.SubsystemType =
                                   EFI_IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER;

            } else if (strcasecmp(optarg, "efiruntimedriver") == 0) {
                Context.SubsystemType = EFI_IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER;

            } else if (strcasecmp(optarg, "saldriver") == 0) {
                Context.SubsystemType = EFI_IMAGE_SUBSYSTEM_SAL_RUNTIME_DRIVER;

            } else {
                Context.SubsystemType = strtoul(optarg, &AfterScan, 0);
                if (AfterScan == optarg) {
                    fprintf(stderr,
                            "Error: Invalid PE subsystem type: %s.\n",
                            optarg);

                    Status = EINVAL;
                    goto mainEnd;
                }
            }

            break;

        case 'o':
            Context.OutputName = optarg;
            break;

        case 'v':
            Context.Flags |= ELFCONV_OPTION_VERBOSE;
            break;

        case 'V':
            printf("ElfConv version %d.%d\n",
                   ELFCONV_VERSION_MAJOR,
                   ELFCONV_VERSION_MINOR);

            return 1;

        case 'h':
            printf(ELFCONV_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto mainEnd;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex != ArgumentCount - 1) {
        fprintf(stderr, "ElfConv takes exactly one (non-option) argument.\n");
        Status = EINVAL;
        goto mainEnd;
    }

    Context.InputName = Arguments[ArgumentIndex];
    if (Context.OutputName == NULL) {
        InputNameLength = strlen(Context.InputName);
        DefaultOutputName = malloc(InputNameLength + sizeof(".efi"));
        if (DefaultOutputName == NULL) {
            Status = ENOMEM;
            goto mainEnd;
        }

        strcpy(DefaultOutputName, Context.InputName);
        strcpy(DefaultOutputName + InputNameLength, ".efi");
        Context.OutputName = DefaultOutputName;
    }

    if (Context.SubsystemType == -1) {
        fprintf(stderr, "Error: -t is a required argument.\n");
        Status = EINVAL;
        goto mainEnd;
    }

    Status = ElfconvLoadFile(&Context);
    if (Status != 0) {
        goto mainEnd;
    }

    if (ElfconvIsElfHeader(Context.InputFile, Context.InputFileSize) == FALSE) {
        fprintf(stderr,
                "Error: %s does not appear to be and ELF image.\n",
                Context.InputName);

        Status = EINVAL;
        goto mainEnd;
    }

    ElfClass = ((UINT8 *)(Context.InputFile))[EI_CLASS];
    if (ElfClass == ELFCLASS32) {
        if (ElfconvInitializeElf32(&Context, &FunctionTable) == FALSE) {
            Status = EINVAL;
            goto mainEnd;
        }

    } else if (ElfClass == ELFCLASS64) {
        if (ElfconvInitializeElf64(&Context, &FunctionTable) == FALSE) {
            Status = EINVAL;
            goto mainEnd;
        }

    } else {
        fprintf(stderr, "Error: Unrecogized ei_class %d.\n", ElfClass);
        Status = EINVAL;
        goto mainEnd;
    }

    Status = EINVAL;

    //
    // Perform an initial pass and set up the destination image.
    //

    Result = FunctionTable.ScanSections(&Context);
    if (Result == FALSE) {
        fprintf(stderr, "Error: Failed to scan sections.\n");
        goto mainEnd;
    }

    //
    // Write and relocate individual section types.
    //

    Result = FunctionTable.WriteSections(&Context, ElfconvSectionText);
    if (Result == FALSE) {
        fprintf(stderr, "Error: Failed to write text section.\n");
        goto mainEnd;
    }

    Result = FunctionTable.WriteSections(&Context, ElfconvSectionData);
    if (Result == FALSE) {
        fprintf(stderr, "Error: Failed to write data section.\n");
        goto mainEnd;
    }

    Result = FunctionTable.WriteSections(&Context, ElfconvSectionHii);
    if (Result == FALSE) {
        fprintf(stderr, "Error: Failed to write HII section.\n");
        goto mainEnd;
    }

    //
    // Translate and write the relocation information.
    //

    Result = FunctionTable.WriteRelocations(&Context);
    if (Result == FALSE) {
        fprintf(stderr, "Error: Failed to translate and write relocations.\n");
        goto mainEnd;
    }

    //
    // Write out the debug information.
    //

    Result = FunctionTable.WriteDebug(&Context);
    if (Result == FALSE) {
        fprintf(stderr, "Error: Failed to write debug data.\n");
        goto mainEnd;
    }

    FunctionTable.SetImageSize(&Context);

    //
    // Write out the new file buffer.
    //

    assert(Context.OutputName != NULL);

    Output = fopen(Context.OutputName, "wb");
    if (Output == NULL) {
        Status = errno;
        fprintf(stderr,
                "Error: Failed to open output %s: %s.\n",
                Context.OutputName,
                strerror(Status));

        goto mainEnd;
    }

    assert((Context.CoffFile != NULL) &&
           (Context.CoffOffset != 0));

    BytesWritten = fwrite(Context.CoffFile, 1, Context.CoffOffset, Output);
    if ((BytesWritten != Context.CoffOffset) || (ferror(Output) != 0)) {
        Status = errno;
        if (Status == 0) {
            errno = EIO;
        }

        fprintf(stderr,
                "Error: Failed to write %s: %s.\n",
                Context.OutputName,
                strerror(Status));

        goto mainEnd;
    }

    Status = 0;

mainEnd:
    if (Output != NULL) {
        fclose(Output);
    }

    if (FunctionTable.CleanUp != NULL) {
        FunctionTable.CleanUp(&Context);
    }

    if (DefaultOutputName != NULL) {
        free(DefaultOutputName);
    }

    if (Context.InputFile != NULL) {
        free(Context.InputFile);
    }

    if (Context.CoffFile != NULL) {
        free(Context.CoffFile);
    }

    if (Context.StringTable != NULL) {
        free(Context.StringTable);
    }

    if ((Context.Flags & ELFCONV_OPTION_VERBOSE) != 0) {
        printf("ElfConv %s returning %d: %s.\n",
               Context.InputName,
               Status,
               strerror(Status));
    }

    return Status;
}

VOID
ElfconvSetHiiResourceHeader (
    UINT8 *HiiBinData,
    UINT32 OffsetToFile
    )

/*++

Routine Description:

    This routine sets up the HII resource data in the destination image.

Arguments:

    HiiBinData - Supplies a pointer to the raw resource data.

    OffsetToFile - Supplies the offset within the file where the data is going
        to reside.

Return Value:

    None.

--*/

{

    UINT32 Index;
    VOID *NextEntry;
    UINT32 Offset;
    EFI_IMAGE_RESOURCE_DATA_ENTRY *ResourceDataEntry;
    EFI_IMAGE_RESOURCE_DIRECTORY *ResourceDirectory;
    EFI_IMAGE_RESOURCE_DIRECTORY_ENTRY *ResourceDirectoryEntry;
    EFI_IMAGE_RESOURCE_DIRECTORY_STRING *ResourceDirectoryString;

    //
    // Fill in the resource section entry.
    //

    ResourceDirectory = (EFI_IMAGE_RESOURCE_DIRECTORY *)HiiBinData;
    ResourceDirectoryEntry =
                 (EFI_IMAGE_RESOURCE_DIRECTORY_ENTRY *)(ResourceDirectory + 1);

    for (Index = 0;
         Index < ResourceDirectory->NumberOfNamedEntries;
         Index += 1) {

        if (ResourceDirectoryEntry->u1.s.NameIsString != 0) {
            Offset = ResourceDirectoryEntry->u1.s.NameOffset;
            ResourceDirectoryString =
                  (EFI_IMAGE_RESOURCE_DIRECTORY_STRING *)(HiiBinData + Offset);

            if ((ResourceDirectoryString->Length == 3) &&
                (ResourceDirectoryString->String[0] == L'H') &&
                (ResourceDirectoryString->String[1] == L'I') &&
                (ResourceDirectoryString->String[2] == L'I')) {

                //
                // Resource Type "HII" was found.
                //

                if (ResourceDirectoryEntry->u2.s.DataIsDirectory) {

                    //
                    // Move to next level - resource Name.
                    //

                    Offset = ResourceDirectoryEntry->u2.s.OffsetToDirectory;
                    ResourceDirectory =
                         (EFI_IMAGE_RESOURCE_DIRECTORY *)(HiiBinData + Offset);

                    NextEntry = ResourceDirectory + 1;
                    ResourceDirectoryEntry = NextEntry;
                    if (ResourceDirectoryEntry->u2.s.DataIsDirectory) {

                        //
                        // Move to next level - resource Language.
                        //

                        Offset = ResourceDirectoryEntry->u2.s.OffsetToDirectory;
                        ResourceDirectory =
                            (EFI_IMAGE_RESOURCE_DIRECTORY *)(HiiBinData +
                                                             Offset);

                        NextEntry = ResourceDirectory + 1;
                        ResourceDirectoryEntry = NextEntry;
                    }
                }

                //
                // Now it ought to be resource Data. Update its "offset to
                // data" value.
                //

                if (ResourceDirectoryEntry->u2.s.DataIsDirectory != 0) {
                    Offset = ResourceDirectoryEntry->u2.OffsetToData;
                    ResourceDataEntry =
                        (EFI_IMAGE_RESOURCE_DATA_ENTRY *)(HiiBinData + Offset);

                    ResourceDataEntry->OffsetToData =
                                ResourceDataEntry->OffsetToData + OffsetToFile;

                    break;
                }
            }
        }

        ResourceDirectoryEntry += 1;
    }

    return;
}

VOID
ElfconvCreateSectionHeader (
    PELFCONV_CONTEXT Context,
    CHAR8 *Name,
    UINT32 Offset,
    UINT32 Size,
    UINT32 Flags
    )

/*++

Routine Description:

    This routine initializes a PE section header in the output file buffer.

Arguments:

    Context - Supplies a pointer to the conversion context.

    Name - Supplies a pointer to a string containing the name of the section.

    Offset - Supplies the file offset to the start of the section.

    Size - Supplies the size of the section in bytes.

    Flags - Supplies the section flags.

Return Value:

    None.

--*/

{

    EFI_IMAGE_SECTION_HEADER *Header;
    UINT32 Length;
    UINT32 NewSize;
    CHAR8 *NewTable;

    Header =
        (EFI_IMAGE_SECTION_HEADER *)(Context->CoffFile + Context->TableOffset);

    Length = strlen(Name) + 1;
    if (Length > EFI_IMAGE_SIZEOF_SHORT_NAME) {

        //
        // Create a string table entry for it. The first 4 bytes of the string
        // table are its size.
        //

        NewSize = Context->StringTableSize + Length;
        if (Context->StringTableSize == 0) {
            NewSize += sizeof(UINT32);
            Context->StringTableSize = sizeof(UINT32);
        }

        NewTable = realloc(Context->StringTable, NewSize);
        if (NewTable == NULL) {

            assert(FALSE);

            return;
        }

        memcpy(NewTable + Context->StringTableSize, Name, Length);
        Context->StringTable = NewTable;
        snprintf((INT8 *)(Header->Name),
                 EFI_IMAGE_SIZEOF_SHORT_NAME,
                 "/%d",
                 Context->StringTableSize);

        Context->StringTableSize += Length;

    } else {
        strncpy((INT8 *)(Header->Name), Name, EFI_IMAGE_SIZEOF_SHORT_NAME);
    }

    Header->Misc.VirtualSize = Size;
    Header->VirtualAddress = Offset;
    Header->SizeOfRawData = Size;
    Header->PointerToRawData = Offset;
    Header->PointerToRelocations = 0;
    Header->PointerToLinenumbers = 0;
    Header->NumberOfRelocations = 0;
    Header->NumberOfLinenumbers = 0;
    Header->Characteristics = Flags;
    Context->TableOffset += sizeof(EFI_IMAGE_SECTION_HEADER);
    if ((Context->Flags & ELFCONV_OPTION_VERBOSE) != 0) {
        printf("Creating section %s VA 0x%x, SizeOfRawData 0x%x, "
               "PointerToRawData 0x%x, Characteristics 0x%x.\n",
               Name,
               Header->VirtualAddress,
               Header->SizeOfRawData,
               Header->PointerToRawData,
               Header->Characteristics);
    }

    return;
}

BOOLEAN
ElfconvCoffAddFixup (
    PELFCONV_CONTEXT Context,
    UINT32 Offset,
    UINT8 Type
    )

/*++

Routine Description:

    This routine adds a COFF relocation to the destination image buffer.

Arguments:

    Context - Supplies a pointer to the conversion context.

    Offset - Supplies the file offset to the relocation.

    Type - Supplies the relocation type.

Return Value:

    TRUE on success.

    FALSE on allocation failure.

--*/

{

    UINTN Difference;
    VOID *NewBuffer;
    UINTN NewSize;

    //
    // Create a new page entry if no relocations have been added or this one is
    // on a different page.
    //

    if ((Context->CoffBaseRelocation == NULL) ||
        (Context->CoffBaseRelocation->VirtualAddress !=
         (Offset & ~0x00000FFF))) {

        if (Context->CoffBaseRelocation != NULL) {

            //
            // Add a null entry, then pad for alignment.
            //

            ElfconvCoffAddFixupEntry(Context, 0);
            if ((Context->CoffOffset % 4) != 0) {
                ElfconvCoffAddFixupEntry(Context, 0);
            }
        }

        NewSize = Context->CoffOffset +
                  sizeof(EFI_IMAGE_BASE_RELOCATION) +
                  ELFCONV_RELOCATION_EXPANSION_SIZE;

        NewBuffer = realloc(Context->CoffFile, NewSize);
        if (NewBuffer == NULL) {
            return FALSE;
        }

        Context->CoffFile = NewBuffer;
        Difference = NewSize - Context->CoffOffset;
        memset(Context->CoffFile + Context->CoffOffset, 0, Difference);
        Context->CoffBaseRelocation =
                            (EFI_IMAGE_BASE_RELOCATION *)(Context->CoffFile +
                                                          Context->CoffOffset);

        Context->CoffBaseRelocation->VirtualAddress = Offset & ~0x00000FFF;
        Context->CoffBaseRelocation->SizeOfBlock =
                                             sizeof(EFI_IMAGE_BASE_RELOCATION);

        Context->CoffNextRelocation =
                                   (UINT16 *)(Context->CoffBaseRelocation + 1);

        Context->CoffOffset += sizeof(EFI_IMAGE_BASE_RELOCATION);
    }

    ElfconvCoffAddFixupEntry(Context,
                             (UINT16)((Type << 12) | (Offset & 0xFFF)));

    return TRUE;
}

VOID
ElfconvCoffAddFixupEntry (
    PELFCONV_CONTEXT Context,
    UINT16 Value
    )

/*++

Routine Description:

    This routine adds a relocation entry to the current COFF location.

Arguments:

    Context - Supplies a pointer to the application context.

    Value - Supplies the relocation value to add.

Return Value:

    None.

--*/

{

    *(Context->CoffNextRelocation) = Value;
    Context->CoffNextRelocation += 1;
    Context->CoffBaseRelocation->SizeOfBlock += sizeof(UINT16);
    Context->CoffOffset += sizeof(UINT16);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

int
ElfconvLoadFile (
    PELFCONV_CONTEXT Context
    )

/*++

Routine Description:

    This routine loads the input file into memory.

Arguments:

    Context - Supplies a pointer to the conversion context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    CHAR8 *Buffer;
    ssize_t BytesRead;
    FILE *File;
    struct stat Stat;
    int Status;

    Buffer = NULL;
    File = fopen(Context->InputName, "rb");
    if (File == NULL) {
        Status = errno;
        fprintf(stderr,
                "Error: Failed to open %s: %s.\n",
                Context->InputName,
                strerror(Status));

        goto LoadFileEnd;
    }

    Status = stat(Context->InputName, &Stat);
    if (Status != 0) {
        Status = errno;
        goto LoadFileEnd;
    }

    if (Stat.st_size > 0xFFFFFFFF) {
        Status = ERANGE;
        fprintf(stderr, "Error: File too big.\n");
        goto LoadFileEnd;
    }

    Buffer = malloc(Stat.st_size);
    if (Buffer == NULL) {
        fprintf(stderr,
                "Error: Failed to allocate 0x%llx for input file buffer.\n",
                (UINT64)Stat.st_size);

        Status = ENOMEM;
        goto LoadFileEnd;
    }

    BytesRead = fread(Buffer, 1, Stat.st_size, File);
    if (BytesRead != Stat.st_size) {
        Status = errno;
        if (Status == 0) {
            Status = EIO;
        }

        fprintf(stderr,
                "Error: Failed to read input file %s: %s.\n",
                Context->InputName,
                strerror(Status));

        goto LoadFileEnd;
    }

    Context->InputFile = Buffer;
    Context->InputFileSize = Stat.st_size;
    Buffer = 0;
    Status = 0;

LoadFileEnd:
    if (File != NULL) {
        fclose(File);
    }

    if (Buffer != NULL) {
        free(Buffer);
    }

    return Status;
}

BOOLEAN
ElfconvIsElfHeader (
    VOID *File,
    UINTN FileSize
    )

/*++

Routine Description:

    This routine determines if the given file starts with a valid ELF header.

Arguments:

    File - Supplies a pointer to the file buffer.

    FileSize - Supplies the size of the file in bytes.

Return Value:

    TRUE if the given file is ELF.

    FALSE if the file is not ELF or is too small.

--*/

{

    INT8 *Buffer;

    if (FileSize < EI_PAD) {
        return FALSE;
    }

    Buffer = File;
    if ((Buffer[EI_MAG0] == ELFMAG0) && (Buffer[EI_MAG1] == ELFMAG1) &&
        (Buffer[EI_MAG2] == ELFMAG2) && (Buffer[EI_MAG3] == ELFMAG3)) {

        return TRUE;
    }

    return FALSE;
}
