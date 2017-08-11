/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    elfconv.h

Abstract:

    This header contains internal definitions for the ELF conversion program.

Author:

    Evan Green 7-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "peimage.h"
#include "elfimage.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro aligns a value up to the COFF alignment.
//

#define ELFCONV_COFF_ALIGN(_Value) \
    (((_Value) + (ELFCONV_COFF_ALIGNMENT - 1)) & ~(ELFCONV_COFF_ALIGNMENT - 1))

//
// ---------------------------------------------------------------- Definitions
//

//
// Set this flag to print additional information.
//

#define ELFCONV_OPTION_VERBOSE 0x00000001

//
// Define the name of the HII .rsrc section.
//

#define ELFCONV_HII_SECTION_NAME ".hii"

//
// Define the alignment used throughout the COFF file.
//

#define ELFCONV_COFF_ALIGNMENT 0x20

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _ELFCONV_SECTION_FILTER {
    ElfconvSectionInvalid,
    ElfconvSectionText,
    ElfconvSectionData,
    ElfconvSectionHii,
    ElfconvSectionStabs,
} ELFCONV_SECTION_FILTER, *PELFCONV_SECTION_FILTER;

/*++

Structure Description:

    This structure defines the application context information for the ElfConv
    utility.

Members:

    Flags - Stores a bitfield of flags. See ELFCONV_OPTION_* definition.

    OutputName - Stores the name of the output image.

    InputName - Stores the name of the input image.

    SubsystemType - Stores the desired subsystem type.

    CoffSectionsOffset - Stores the COFF section offset buffer.

    CoffOffset - Stores the current offset in the COFF file.

    NtHeaderOffset - Stores the offset of the NT header in the COFF file.

    TableOffset - Stores the offset of the section table in the COFF file.

    TextOffset - Stores the offset of the text section in the COFF file.

    DataOffset - Stores the offset of the data section in the COFF file.

    HiiRsrcOffset - Stores the offset of the .rsrc HII section in the COFF file.

    RelocationOffset - Stores the offset of the relocation information in the
        COFF file.

    CoffFile - Store the COFF output file buffer.

    Timestamp - Stores the image timestamp.

    CoffBaseRelocation - Stores a pointer to the first relocation entry for the
        current relocation page.

    CoffNextRelocation - Stores a pointer to the next available relocation
        entry.

    StringTable - Stores a pointer to the string table, needed for sections
        that are larger than 8 characters.

    StringTableSize - Stores the size of the string table in bytes.

--*/

typedef struct _ELFCONV_CONTEXT {
    UINT32 Flags;
    CHAR8 *OutputName;
    CHAR8 *InputName;
    CHAR8 SubsystemType;
    VOID *InputFile;
    UINT32 InputFileSize;
    UINT32 *CoffSectionsOffset;
    UINT32 CoffOffset;
    UINT32 NtHeaderOffset;
    UINT32 TableOffset;
    UINT32 TextOffset;
    UINT32 DataOffset;
    UINT32 HiiRsrcOffset;
    UINT32 RelocationOffset;
    UINT8 *CoffFile;
    UINT32 ImageTimestamp;
    EFI_IMAGE_BASE_RELOCATION *CoffBaseRelocation;
    UINT16 *CoffNextRelocation;
    CHAR8 *StringTable;
    UINT32 StringTableSize;
} ELFCONV_CONTEXT, *PELFCONV_CONTEXT;

typedef
BOOLEAN
(*PELFCONV_SCAN_SECTIONS) (
    PELFCONV_CONTEXT Context
    );

/*++

Routine Description:

    This routine scans the ELF sections in preparation for conversion.

Arguments:

    Context - Supplies a pointer to the conversion context.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

typedef
BOOLEAN
(*PELFCONV_WRITE_SECTIONS) (
    PELFCONV_CONTEXT Context,
    ELFCONV_SECTION_FILTER FilterType
    );

/*++

Routine Description:

    This routine writes ELF sections to the PE image buffer.

Arguments:

    Context - Supplies a pointer to the conversion context.

    FilterType - Supplies the section filter to apply.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

typedef
BOOLEAN
(*PELFCONV_WRITE_RELOCATIONS) (
    PELFCONV_CONTEXT Context
    );

/*++

Routine Description:

    This routine writes the relocation section for the new PE image.

Arguments:

    Context - Supplies a pointer to the conversion context.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

typedef
BOOLEAN
(*PELFCONV_WRITE_DEBUG) (
    PELFCONV_CONTEXT Context
    );

/*++

Routine Description:

    This routine writes debug sections into the new PE image.

Arguments:

    Context - Supplies a pointer to the conversion context.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

typedef
VOID
(*PELFCONV_SET_IMAGE_SIZE) (
    PELFCONV_CONTEXT Context
    );

/*++

Routine Description:

    This routine sets the final PE image size.

Arguments:

    Context - Supplies a pointer to the conversion context.

Return Value:

    None.

--*/

typedef
VOID
(*PELFCONV_CLEAN_UP) (
    PELFCONV_CONTEXT Context
    );

/*++

Routine Description:

    This routine finalizes the new PE image and performs clean-up actions.

Arguments:

    Context - Supplies a pointer to the conversion context.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure defines the function table for converting an ELF image to
    a PE image.

Members:

    ScanSections - Stores a pointer to a function used to scan the ELF image
        sections.

    WriteSections - Stores a pointer to a function used to write the PE
        image sections.

    WriteRelocations - Stores a pointer to a function used to write the
        relocation information into the PE image.

    WriteDebug - Stores a pointer to a function used to write debug information
        into the PE image.

    SetImageSize - Stores a pointer to a function used to set the final image
        size.

    CleanUp - Stores a pointer to a function called to clean up the
        conversion state and finalize the PE image.

--*/

typedef struct _ELFCONV_FUNCTION_TABLE {
    PELFCONV_SCAN_SECTIONS ScanSections;
    PELFCONV_WRITE_SECTIONS WriteSections;
    PELFCONV_WRITE_RELOCATIONS WriteRelocations;
    PELFCONV_WRITE_DEBUG WriteDebug;
    PELFCONV_SET_IMAGE_SIZE SetImageSize;
    PELFCONV_CLEAN_UP CleanUp;
} ELFCONV_FUNCTION_TABLE, *PELFCONV_FUNCTION_TABLE;

//
// -------------------------------------------------------------------- Globals
//

extern CHAR8 *ElfconvDebugSections[];

//
// -------------------------------------------------------- Function Prototypes
//

//
// Common ELF functions
//

VOID
ElfconvSetHiiResourceHeader (
    UINT8 *HiiBinData,
    UINT32 OffsetToFile
    );

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

VOID
ElfconvCreateSectionHeader (
    PELFCONV_CONTEXT Context,
    CHAR8 *Name,
    UINT32 Offset,
    UINT32 Size,
    UINT32 Flags
    );

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

BOOLEAN
ElfconvCoffAddFixup (
    PELFCONV_CONTEXT Context,
    UINT32 Offset,
    UINT8 Type
    );

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

VOID
ElfconvCoffAddFixupEntry (
    PELFCONV_CONTEXT Context,
    UINT16 Value
    );

/*++

Routine Description:

    This routine adds a relocation entry to the current COFF location.

Arguments:

    Context - Supplies a pointer to the application context.

    Value - Supplies the relocation value to add.

Return Value:

    None.

--*/

BOOLEAN
ElfconvInitializeElf32 (
    PELFCONV_CONTEXT Context,
    PELFCONV_FUNCTION_TABLE FunctionTable
    );

/*++

Routine Description:

    This routine attempts to bind an ELF conversion context to an ELF32 image.

Arguments:

    Context - Supplies a pointer to the ELF context.

    FunctionTable - Supplies a pointer to the ELF conversion functions on
        success.

Return Value:

    TRUE on success.

    FALSE if the image is not a valid ELF32 image.

--*/

BOOLEAN
ElfconvInitializeElf64 (
    PELFCONV_CONTEXT Context,
    PELFCONV_FUNCTION_TABLE FunctionTable
    );

/*++

Routine Description:

    This routine attempts to bind an ELF conversion context to an ELF64 image.

Arguments:

    Context - Supplies a pointer to the ELF context.

    FunctionTable - Supplies a pointer to the ELF conversion functions on
        success.

Return Value:

    TRUE on success.

    FALSE if the image is not a valid ELF64 image.

--*/
