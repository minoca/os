/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    elfc32.c

Abstract:

    This module implements support for converting an ELF32 image to a PE
    image.

Author:

    Evan Green 7-Mar-2014

Environment:

    Build

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "uefifw.h"
#include "elfconv.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro aligns a value up to the COFF alignment.
//

#define ELFCONV_COFF_ALIGN(_Value) \
    (((_Value) + (ELFCONV_COFF_ALIGNMENT - 1)) & ~(ELFCONV_COFF_ALIGNMENT - 1))

//
// This macro returns a pointer to the base of the section header array given
// a pointer to the ELF header.
//

#define ELFCONV_SECTION_BASE(_ElfHeader) \
    ((VOID *)(_ElfHeader) + (_ElfHeader)->e_shoff)

//
// This macro returns a pointer to the requested ELF section given the ELF
// file header.
//

#define ELFCONV_ELF_SECTION(_ElfHeader, _SectionIndex)  \
    ((VOID *)ELFCONV_SECTION_BASE(_ElfHeader) +         \
     ((_SectionIndex) * (_ElfHeader)->e_shentsize))

//
// This macro returns a pointer to the base of the program header array given
// a pointer to the ELF header.
//

#define ELFCONV_PROGRAM_HEADER_BASE(_ElfHeader) \
    ((VOID *)(_ElfHeader) + (_ElfHeader)->e_phoff)

//
// This macro returns a pointer to the requested ELF program header given the
// ELF file header.
//

#define ELFCONV_ELF_PROGRAM_HEADER(_ElfHeader, _ProgramHeaderIndex) \
    ((VOID *)ELFCONV_PROGRAM_HEADER_BASE(_ElfHeader) +              \
     ((_ProgramHeaderIndex) * (_ElfHeader)->e_phentsize))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of sections assumed in the PE image.
//

#define ELFCONV_PE_SECTION_COUNT 16

//
// Define the alignment used throughout the COFF file.
//

#define ELFCONV_COFF_ALIGNMENT 0x20

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
BOOLEAN
(*PELFCONV_SECTION_FILTER_FUNCTION) (
    Elf32_Ehdr *ElfHeader,
    Elf32_Shdr *SectionHeader
    );

/*++

Routine Description:

    This routine filters ELF sections based on certain criteria.

Arguments:

    ElfHeader - Supplies a pointer to the ELF file header.

    SectionHeader - Supplies a pointer to the section header in question.

Return Value:

    TRUE if the section matches the filter.

    FALSE if the section does not match the filter.

--*/

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOLEAN
ElfconvScanSections32 (
    PELFCONV_CONTEXT Context
    );

BOOLEAN
ElfconvWriteSections32 (
    PELFCONV_CONTEXT Context,
    ELFCONV_SECTION_FILTER FilterType
    );

BOOLEAN
ElfconvWriteRelocations32 (
    PELFCONV_CONTEXT Context
    );

BOOLEAN
ElfconvWriteDebug32 (
    PELFCONV_CONTEXT Context
    );

VOID
ElfconvSetImageSize32 (
    PELFCONV_CONTEXT Context
    );

VOID
ElfconvCleanUp32 (
    PELFCONV_CONTEXT Context
    );

BOOLEAN
ElfconvIsTextSection (
    Elf32_Ehdr *ElfHeader,
    Elf32_Shdr *SectionHeader
    );

BOOLEAN
ElfconvIsDataSection (
    Elf32_Ehdr *ElfHeader,
    Elf32_Shdr *SectionHeader
    );

BOOLEAN
ElfconvIsHiiRsrcSection (
    Elf32_Ehdr *ElfHeader,
    Elf32_Shdr *SectionHeader
    );

BOOLEAN
ElfconvIsDebugSection (
    Elf32_Ehdr *ElfHeader,
    Elf32_Shdr *SectionHeader
    );

VOID
ElfconvThumbMovtImmediatePatch (
    UINT16 *Instruction,
    UINT16 Address
    );

BOOLEAN
ElfconvConvertElfAddress (
    PELFCONV_CONTEXT Context,
    UINT32 *Address
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

BOOLEAN
ElfconvInitializeElf32 (
    PELFCONV_CONTEXT Context,
    PELFCONV_FUNCTION_TABLE FunctionTable
    )

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

{

    Elf32_Ehdr *ElfHeader;
    BOOLEAN Result;

    Result = FALSE;
    ElfHeader = Context->InputFile;
    if (Context->InputFileSize < sizeof(Elf32_Ehdr)) {
        if ((ElfHeader->e_ident[EI_CLASS] != ELFCLASS32) ||
            (ElfHeader->e_ident[EI_DATA] != ELFDATA2LSB) ||
            ((ElfHeader->e_type != ET_EXEC) && (ElfHeader->e_type != ET_DYN)) ||
            ((ElfHeader->e_machine != EM_386) &&
             (ElfHeader->e_machine != EM_ARM)) ||
            (ElfHeader->e_version != EV_CURRENT)) {

            fprintf(stderr, "ELF Image not valid.\n");
            goto InitializeElf32End;
        }
    }

    Context->CoffSectionsOffset = malloc(ElfHeader->e_shnum * sizeof(UINT32));
    if (Context->CoffSectionsOffset == NULL) {
        goto InitializeElf32End;
    }

    memset(Context->CoffSectionsOffset, 0, ElfHeader->e_shnum * sizeof(UINT32));
    FunctionTable->ScanSections = ElfconvScanSections32;
    FunctionTable->WriteSections = ElfconvWriteSections32;
    FunctionTable->WriteRelocations = ElfconvWriteRelocations32;
    FunctionTable->WriteDebug = ElfconvWriteDebug32;
    FunctionTable->SetImageSize = ElfconvSetImageSize32;
    FunctionTable->CleanUp = ElfconvCleanUp32;
    Result = TRUE;

InitializeElf32End:
    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOLEAN
ElfconvScanSections32 (
    PELFCONV_CONTEXT Context
    )

/*++

Routine Description:

    This routine scans the ELF sections and sets up the PE image.

Arguments:

    Context - Supplies a pointer to the ELF context.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    UINT32 CoffEntry;
    EFI_IMAGE_DOS_HEADER *DosHeader;
    Elf32_Ehdr *ElfHeader;
    Elf32_Shdr *ElfSection;
    UINT32 Flags;
    BOOLEAN FoundText;
    EFI_IMAGE_OPTIONAL_HEADER_UNION *NtHeader;
    UINTN SectionIndex;
    CHAR8 *SectionName;
    Elf32_Shdr *StringSection;

    CoffEntry = 0;
    Context->CoffOffset = 0;
    Context->TextOffset = 0;
    Context->CoffOffset = sizeof(EFI_IMAGE_DOS_HEADER) + 0x40;
    Context->NtHeaderOffset = Context->CoffOffset;
    FoundText = FALSE;
    ElfHeader = Context->InputFile;

    assert((ElfHeader->e_machine == EM_386) ||
           (ElfHeader->e_machine == EM_ARM));

    Context->CoffOffset += sizeof(EFI_IMAGE_NT_HEADERS32);
    Context->TableOffset = Context->CoffOffset;
    Context->CoffOffset += ELFCONV_PE_SECTION_COUNT *
                           sizeof(EFI_IMAGE_SECTION_HEADER);

    //
    // Find and wrangle any text sections.
    //

    Context->CoffOffset = ELFCONV_COFF_ALIGN(Context->CoffOffset);
    for (SectionIndex = 0;
         SectionIndex < ElfHeader->e_shnum;
         SectionIndex += 1) {

        ElfSection = ELFCONV_ELF_SECTION(ElfHeader, SectionIndex);
        if (ElfconvIsTextSection(ElfHeader, ElfSection) != FALSE) {
            if ((Context->Flags & ELFCONV_OPTION_VERBOSE) != 0) {
                StringSection = ELFCONV_ELF_SECTION(ElfHeader,
                                                    ElfHeader->e_shstrndx);

                SectionName = (CHAR8 *)ElfHeader + StringSection->sh_offset +
                              ElfSection->sh_name;

                printf("Found text section %s: Offset 0x%x, size 0x%x.\n",
                       SectionName,
                       ElfSection->sh_offset,
                       ElfSection->sh_size);
            }

            //
            // If the alignment field is valid, align the COFF offset up.
            //

            if ((ElfSection->sh_addralign != 0) &&
                (ElfSection->sh_addralign != 1)) {

                if ((ElfSection->sh_addr &
                     (ElfSection->sh_addralign - 1)) == 0) {

                    Context->CoffOffset = ALIGN_VALUE(Context->CoffOffset,
                                                      ElfSection->sh_addralign);

                } else if ((ElfSection->sh_addr % ElfSection->sh_addralign) !=
                           (Context->CoffOffset % ElfSection->sh_addralign)) {

                    fprintf(stderr, "Error: Unsupported section alignment.\n");
                    return FALSE;
                }
            }

            //
            // Relocate the entry point.
            //

            if ((ElfHeader->e_entry >= ElfSection->sh_addr) &&
                (ElfHeader->e_entry <
                 ElfSection->sh_addr + ElfSection->sh_size)) {

                CoffEntry = Context->CoffOffset +
                            ElfHeader->e_entry - ElfSection->sh_addr;
            }

            if (FoundText == FALSE) {
                Context->TextOffset = Context->CoffOffset;
                FoundText = TRUE;
            }

            Context->CoffSectionsOffset[SectionIndex] = Context->CoffOffset;
            Context->CoffOffset += ElfSection->sh_size;
        }
    }

    if (FoundText == FALSE) {
        fprintf(stderr, "Error: Failed to find a text section.\n");
        return FALSE;
    }

    if (ElfHeader->e_machine != EM_ARM) {
        Context->CoffOffset = ELFCONV_COFF_ALIGN(Context->CoffOffset);
    }

    //
    // Find and wrangle data sections.
    //

    Context->DataOffset = Context->CoffOffset;
    for (SectionIndex = 0;
         SectionIndex < ElfHeader->e_shnum;
         SectionIndex += 1) {

        ElfSection = ELFCONV_ELF_SECTION(ElfHeader, SectionIndex);
        if (ElfconvIsDataSection(ElfHeader, ElfSection) != FALSE) {
            if ((Context->Flags & ELFCONV_OPTION_VERBOSE) != 0) {
                StringSection = ELFCONV_ELF_SECTION(ElfHeader,
                                                    ElfHeader->e_shstrndx);

                SectionName = (CHAR8 *)ElfHeader + StringSection->sh_offset +
                              ElfSection->sh_name;

                printf("Found data section %s: Offset 0x%x, size 0x%x.\n",
                       SectionName,
                       ElfSection->sh_offset,
                       ElfSection->sh_size);
            }

            //
            // If the alignment field is valid, align the COFF offset up.
            //

            if ((ElfSection->sh_addralign != 0) &&
                (ElfSection->sh_addralign != 1)) {

                if ((ElfSection->sh_addr &
                     (ElfSection->sh_addralign - 1)) == 0) {

                    Context->CoffOffset = ALIGN_VALUE(Context->CoffOffset,
                                                      ElfSection->sh_addralign);

                } else if ((ElfSection->sh_addr % ElfSection->sh_addralign) !=
                           (Context->CoffOffset % ElfSection->sh_addralign)) {

                    fprintf(stderr, "Error: Unsupported section alignment.\n");
                    return FALSE;
                }
            }

            Context->CoffSectionsOffset[SectionIndex] = Context->CoffOffset;
            Context->CoffOffset += ElfSection->sh_size;
        }
    }

    Context->CoffOffset = ELFCONV_COFF_ALIGN(Context->CoffOffset);

    //
    // Find and wrangle HII .rsrc section.
    //

    Context->HiiRsrcOffset = Context->CoffOffset;
    for (SectionIndex = 0;
         SectionIndex < ElfHeader->e_shnum;
         SectionIndex += 1) {

        ElfSection = ELFCONV_ELF_SECTION(ElfHeader, SectionIndex);
        if (ElfconvIsHiiRsrcSection(ElfHeader, ElfSection) != FALSE) {
            if ((Context->Flags & ELFCONV_OPTION_VERBOSE) != 0) {
                StringSection = ELFCONV_ELF_SECTION(ElfHeader,
                                                    ElfHeader->e_shstrndx);

                SectionName = (CHAR8 *)ElfHeader + StringSection->sh_offset +
                              ElfSection->sh_name;

                printf("Found rsrc section %s: Offset 0x%x, size 0x%x.\n",
                       SectionName,
                       ElfSection->sh_offset,
                       ElfSection->sh_size);
            }

            //
            // If the alignment field is valid, align the COFF offset up.
            //

            if ((ElfSection->sh_addralign != 0) &&
                (ElfSection->sh_addralign != 1)) {

                if ((ElfSection->sh_addr &
                     (ElfSection->sh_addralign - 1)) == 0) {

                    Context->CoffOffset = ALIGN_VALUE(Context->CoffOffset,
                                                      ElfSection->sh_addralign);

                } else if ((ElfSection->sh_addr % ElfSection->sh_addralign) !=
                           (Context->CoffOffset % ElfSection->sh_addralign)) {

                    fprintf(stderr, "Error: Unsupported section alignment.\n");
                    return FALSE;
                }
            }

            if (ElfSection->sh_size != 0) {
                Context->CoffSectionsOffset[SectionIndex] = Context->CoffOffset;
                Context->CoffOffset += ElfSection->sh_size;
                Context->CoffOffset = ELFCONV_COFF_ALIGN(Context->CoffOffset);
                ElfconvSetHiiResourceHeader(
                                    (UINT8 *)ElfHeader + ElfSection->sh_offset,
                                    Context->HiiRsrcOffset);

                break;
            }
        }
    }

    Context->RelocationOffset = Context->CoffOffset;

    //
    // Allocate the base COFF file. This will be expanded later for relocations.
    //

    Context->CoffFile = malloc(Context->CoffOffset);
    if (Context->CoffFile == NULL) {
        return FALSE;
    }

    memset(Context->CoffFile, 0, Context->CoffOffset);

    //
    // Fill in the headers.
    //

    DosHeader = (EFI_IMAGE_DOS_HEADER *)(Context->CoffFile);
    DosHeader->e_magic = EFI_IMAGE_DOS_SIGNATURE;
    DosHeader->e_lfanew = Context->NtHeaderOffset;
    NtHeader = (EFI_IMAGE_OPTIONAL_HEADER_UNION *)(Context->CoffFile +
                                                   Context->NtHeaderOffset);

    NtHeader->Pe32.Signature = EFI_IMAGE_NT_SIGNATURE;
    switch (ElfHeader->e_machine) {
    case EM_386:
        NtHeader->Pe32.FileHeader.Machine = EFI_IMAGE_MACHINE_IA32;
        break;

    case EM_ARM:
        NtHeader->Pe32.FileHeader.Machine = EFI_IMAGE_MACHINE_ARMTHUMB_MIXED;
        break;

    default:

        assert(FALSE);

        return FALSE;
    }

    NtHeader->Pe32.OptionalHeader.Magic = EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    NtHeader->Pe32.FileHeader.NumberOfSections = ELFCONV_PE_SECTION_COUNT;
    NtHeader->Pe32.FileHeader.TimeDateStamp = (UINT32)time(NULL);
    Context->ImageTimestamp = NtHeader->Pe32.FileHeader.TimeDateStamp;
    NtHeader->Pe32.FileHeader.PointerToSymbolTable = 0;
    NtHeader->Pe32.FileHeader.NumberOfSymbols = 0;
    NtHeader->Pe32.FileHeader.SizeOfOptionalHeader =
                                         sizeof(NtHeader->Pe32.OptionalHeader);

    NtHeader->Pe32.FileHeader.Characteristics =
                                           EFI_IMAGE_FILE_EXECUTABLE_IMAGE |
                                           EFI_IMAGE_FILE_LINE_NUMS_STRIPPED |
                                           EFI_IMAGE_FILE_LOCAL_SYMS_STRIPPED |
                                           EFI_IMAGE_FILE_32BIT_MACHINE;

    NtHeader->Pe32.OptionalHeader.SizeOfCode = Context->DataOffset -
                                               Context->TextOffset;

    NtHeader->Pe32.OptionalHeader.SizeOfInitializedData =
                               Context->RelocationOffset - Context->DataOffset;

    NtHeader->Pe32.OptionalHeader.SizeOfUninitializedData = 0;
    NtHeader->Pe32.OptionalHeader.AddressOfEntryPoint = CoffEntry;
    NtHeader->Pe32.OptionalHeader.BaseOfCode = Context->TextOffset;
    NtHeader->Pe32.OptionalHeader.BaseOfData = Context->DataOffset;
    NtHeader->Pe32.OptionalHeader.ImageBase = 0;
    NtHeader->Pe32.OptionalHeader.SectionAlignment = ELFCONV_COFF_ALIGNMENT;
    NtHeader->Pe32.OptionalHeader.FileAlignment = ELFCONV_COFF_ALIGNMENT;
    NtHeader->Pe32.OptionalHeader.SizeOfImage = 0;
    NtHeader->Pe32.OptionalHeader.SizeOfHeaders = Context->TextOffset;
    NtHeader->Pe32.OptionalHeader.Subsystem = Context->SubsystemType;
    NtHeader->Pe32.OptionalHeader.NumberOfRvaAndSizes =
                                         EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES;

    //
    // Create the section headers.
    //

    if (Context->DataOffset > Context->TextOffset) {
        Flags = EFI_IMAGE_SCN_CNT_CODE |
                EFI_IMAGE_SCN_MEM_EXECUTE |
                EFI_IMAGE_SCN_MEM_READ;

        ElfconvCreateSectionHeader(Context,
                                   ".text",
                                   Context->TextOffset,
                                   Context->DataOffset - Context->TextOffset,
                                   Flags);

    } else {
        NtHeader->Pe32.FileHeader.NumberOfSections -= 1;
    }

    if (Context->HiiRsrcOffset > Context->DataOffset) {
        Flags = EFI_IMAGE_SCN_CNT_INITIALIZED_DATA |
                EFI_IMAGE_SCN_MEM_WRITE |
                EFI_IMAGE_SCN_MEM_READ;

        ElfconvCreateSectionHeader(Context,
                                   ".data",
                                   Context->DataOffset,
                                   Context->HiiRsrcOffset - Context->DataOffset,
                                   Flags);

    } else {
        NtHeader->Pe32.FileHeader.NumberOfSections -= 1;
    }

    if (Context->RelocationOffset > Context->HiiRsrcOffset) {
        Flags = EFI_IMAGE_SCN_CNT_INITIALIZED_DATA |
                EFI_IMAGE_SCN_MEM_READ;

        ElfconvCreateSectionHeader(
                            Context,
                            ".rsrc",
                            Context->HiiRsrcOffset,
                            Context->RelocationOffset - Context->HiiRsrcOffset,
                            Flags);

    } else {
        NtHeader->Pe32.FileHeader.NumberOfSections -= 1;
    }

    return TRUE;
}

BOOLEAN
ElfconvWriteSections32 (
    PELFCONV_CONTEXT Context,
    ELFCONV_SECTION_FILTER FilterType
    )

/*++

Routine Description:

    This routine writes certain sections to the output image.

Arguments:

    Context - Supplies a pointer to the ELF context.

    FilterType - Supplies the type of sections to write.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    UINT16 Address;
    UINT32 AddressValue;
    VOID *Destination;
    ptrdiff_t Difference;
    Elf32_Ehdr *ElfHeader;
    Elf32_Shdr *ElfSection;
    PELFCONV_SECTION_FILTER_FUNCTION FilterFunction;
    Elf32_Rel *Relocation;
    UINT32 RelocationOffset;
    Elf32_Shdr *RelocationSection;
    UINT32 SectionIndex;
    UINT32 SectionOffset;
    Elf32_Sym *Symbol;
    Elf32_Shdr *SymbolSectionHeader;
    UINT8 *SymbolTable;
    Elf32_Shdr *SymbolTableSection;
    UINT8 *Target;

    ElfHeader = Context->InputFile;

    assert((ElfHeader->e_machine == EM_386) ||
           (ElfHeader->e_machine == EM_ARM));

    switch (FilterType) {
    case ElfconvSectionText:
        FilterFunction = ElfconvIsTextSection;
        break;

    case ElfconvSectionData:
        FilterFunction = ElfconvIsDataSection;
        break;

    case ElfconvSectionHii:
        FilterFunction = ElfconvIsHiiRsrcSection;
        break;

    default:

        assert(FALSE);

        return FALSE;
    }

    //
    // Copy the contents of the eligible sections.
    //

    for (SectionIndex = 0;
         SectionIndex < ElfHeader->e_shnum;
         SectionIndex += 1) {

        ElfSection = ELFCONV_ELF_SECTION(ElfHeader, SectionIndex);
        if (FilterFunction(ElfHeader, ElfSection) != FALSE) {
            Destination = Context->CoffFile +
                          Context->CoffSectionsOffset[SectionIndex];

            switch (ElfSection->sh_type) {
            case SHT_PROGBITS:
            case SHT_DYNAMIC:
            case SHT_DYNSYM:
            case SHT_REL:
            case SHT_RELA:
                if ((Context->Flags & ELFCONV_OPTION_VERBOSE) != 0) {
                    Difference = Destination - (VOID *)(Context->CoffFile);
                    printf("Copying section from ELF offset %x, size %x to "
                           "COFF offset %lx.\n",
                           ElfSection->sh_offset,
                           ElfSection->sh_size,
                           (long)Difference);

                }

                memcpy(Destination,
                       (UINT8 *)ElfHeader + ElfSection->sh_offset,
                       ElfSection->sh_size);

                break;

            case SHT_NOBITS:
                if ((Context->Flags & ELFCONV_OPTION_VERBOSE) != 0) {
                    Difference = Destination - (VOID *)(Context->CoffFile);
                    printf("Zeroing COFF offset %lx, size %x",
                           (long)Difference,
                           ElfSection->sh_size);
                }

                memset(Destination, 0, ElfSection->sh_size);
                break;

            case SHT_SYMTAB:
            case SHT_STRTAB:
            case SHT_HASH:
                break;

            default:
                fprintf(stderr,
                        "Warning: Unknown section type %x.\n",
                        ElfSection->sh_type);

                break;
            }
        }
    }

    //
    // Now apply relocations.
    //

    for (SectionIndex = 0;
         SectionIndex < ElfHeader->e_shnum;
         SectionIndex += 1) {

        RelocationSection = ELFCONV_ELF_SECTION(ElfHeader, SectionIndex);

        //
        // Skip everything except relocation sections.
        //

        if ((RelocationSection->sh_type != SHT_REL) &&
            (RelocationSection->sh_type != SHT_RELA)) {

            continue;
        }

        //
        // This is a relocation section. Extract section information that the
        // relocations apply to.
        //

        ElfSection = ELFCONV_ELF_SECTION(ElfHeader, RelocationSection->sh_info);
        SectionOffset = Context->CoffSectionsOffset[SectionIndex];

        //
        // Skip the section if the filter function doesn't match or they're
        // addend relocations.
        //

        if ((RelocationSection->sh_type != SHT_REL) ||
            (FilterFunction(ElfHeader, ElfSection) == FALSE)) {

            continue;
        }

        //
        // Get the symbol table.
        //

        SymbolTableSection = ELFCONV_ELF_SECTION(ElfHeader,
                                                 RelocationSection->sh_link);

        SymbolTable = (UINT8 *)ElfHeader + SymbolTableSection->sh_offset;

        //
        // Process all relocation entries for this section.
        //

        for (RelocationOffset = 0;
             RelocationOffset < RelocationSection->sh_size;
             RelocationOffset += RelocationSection->sh_entsize) {

            Relocation = (Elf32_Rel *)((UINT8 *)ElfHeader +
                                       RelocationSection->sh_offset +
                                       RelocationOffset);

            Symbol = (Elf32_Sym *)(SymbolTable +
                                   (ELF32_R_SYM(Relocation->r_info) *
                                    SymbolTableSection->sh_entsize));

            //
            // Skip absolute symbols.
            //

            if (Symbol->st_shndx == SHN_ABS) {
                continue;
            }

            if ((Symbol->st_shndx == SHN_UNDEF) ||
                (Symbol->st_shndx > ElfHeader->e_shnum)) {

                Difference = (VOID *)Relocation - (VOID *)ElfHeader;
                fprintf(stderr,
                        "Error: Invalid symbol definition %x, "
                        "relocation section %d, offset %lx.\n",
                        Symbol->st_shndx,
                        SectionIndex,
                        (long)Difference);

                return FALSE;
            }

            SymbolSectionHeader = ELFCONV_ELF_SECTION(ElfHeader,
                                                      Symbol->st_shndx);

            //
            // Convert the relocation data to a pointer into the COFF file.
            // r_offset is the virtual address of the storage unit to be
            // relocated. sh_addr is the virtual address for the base of the
            // section.
            //

            Target = Context->CoffFile + SectionOffset +
                     (Relocation->r_offset - ElfSection->sh_addr);

            //
            // The relocation types are machine dependent.
            //

            if (ElfHeader->e_machine == EM_386) {
                switch (ELF32_R_TYPE(Relocation->r_info)) {
                case R_386_NONE:
                    break;

                //
                // This is an absolute relocation. Convert the target from an
                // absolute virtuall address to an absolute COFF address.
                //

                case R_386_32:
                    *(UINT32 *)Target =
                                 *(UINT32 *)Target -
                                 SymbolSectionHeader->sh_addr +
                                 Context->CoffSectionsOffset[Symbol->st_shndx];

                    break;

                //
                // This is a relative relocation: Symbol - PC + Addend.
                //

                case R_386_PC32:
                    *(UINT32 *)Target =
                               *(UINT32 *)Target +
                               (Context->CoffSectionsOffset[Symbol->st_shndx] -
                                SymbolSectionHeader->sh_addr) -
                               (SectionOffset - ElfSection->sh_addr);

                    break;

                default:
                    fprintf(stderr,
                            "Error: Unsupported relocation type %d.\n",
                            ELF32_R_TYPE(Relocation->r_info));

                    return FALSE;
                }

            } else if (ElfHeader->e_machine == EM_ARM) {
                switch (ELF32_R_TYPE(Relocation->r_info)) {

                //
                // PC-relative relocations don't need modification.
                //

                case R_ARM_RBASE:
                case R_ARM_PC24:
                case R_ARM_REL32:
                case R_ARM_XPC25:
                case R_ARM_THM_PC22:
                case R_ARM_THM_JUMP19:
                case R_ARM_CALL:
                case R_ARM_JMP24:
                case R_ARM_THM_JUMP24:
                case R_ARM_PREL31:
                case R_ARM_MOVW_PREL_NC:
                case R_ARM_MOVT_PREL:
                case R_ARM_THM_MOVW_PREL_NC:
                case R_ARM_THM_MOVT_PREL:
                case R_ARM_THM_JMP6:
                case R_ARM_THM_ALU_PREL_11_0:
                case R_ARM_THM_PC12:
                case R_ARM_REL32_NOI:
                case R_ARM_ALU_PC_G0_NC:
                case R_ARM_ALU_PC_G0:
                case R_ARM_ALU_PC_G1_NC:
                case R_ARM_ALU_PC_G1:
                case R_ARM_ALU_PC_G2:
                case R_ARM_LDR_PC_G1:
                case R_ARM_LDR_PC_G2:
                case R_ARM_LDRS_PC_G0:
                case R_ARM_LDRS_PC_G1:
                case R_ARM_LDRS_PC_G2:
                case R_ARM_LDC_PC_G0:
                case R_ARM_LDC_PC_G1:
                case R_ARM_LDC_PC_G2:
                case R_ARM_GOT_PREL:
                case R_ARM_THM_JUMP11:
                case R_ARM_THM_JUMP8:
                case R_ARM_TLS_GD32:
                case R_ARM_TLS_LDM32:
                case R_ARM_TLS_IE32:
                case R_ARM_GOT_BREL:
                case R_ARM_BASE_PREL:
                    break;

                //
                // MOVW relocates the lower 16 bits of the address.
                //

                case R_ARM_THM_MOVW_ABS_NC:
                    Address =
                        (UINT16)(Symbol->st_value -
                                 SymbolSectionHeader->sh_addr +
                                 Context->CoffSectionsOffset[Symbol->st_shndx]);

                    ElfconvThumbMovtImmediatePatch((UINT16 *)Target, Address);
                    break;

                //
                // MOVT relocates the upper 16 bits of the address.
                //

                case R_ARM_THM_MOVT_ABS:
                    AddressValue =
                         (Symbol->st_value -
                          SymbolSectionHeader->sh_addr +
                          Context->CoffSectionsOffset[Symbol->st_shndx]) >> 16;

                    Address = (UINT16)AddressValue;
                    ElfconvThumbMovtImmediatePatch((UINT16 *)Target, Address);
                    break;

                //
                // This is a 32 bit absolute relocation.
                //

                case R_ARM_ABS32:
                case R_ARM_RABS32:
                    *(UINT32 *)Target =
                                 *(UINT32 *)Target -
                                 SymbolSectionHeader->sh_addr +
                                 Context->CoffSectionsOffset[Symbol->st_shndx];

                    break;

                default:
                    fprintf(stderr,
                            "Error: Unsupported relocation type %d.\n",
                            ELF32_R_TYPE(Relocation->r_info));

                    return FALSE;
                }

            //
            // The machine type is unknown. Execution should have failed before
            // this point.
            //

            } else {

                assert(FALSE);

                return FALSE;
            }
        }
    }

    return TRUE;
}

BOOLEAN
ElfconvWriteRelocations32 (
    PELFCONV_CONTEXT Context
    )

/*++

Routine Description:

    This routine converts the ELF relocations into PE relocations and writes
    them into the output PE file buffer.

Arguments:

    Context - Supplies a pointer to the ELF context.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    UINT32 DestinationSectionIndex;
    EFI_IMAGE_DATA_DIRECTORY *Directories;
    EFI_IMAGE_DATA_DIRECTORY *Directory;
    Elf32_Dyn *DynamicSection;
    Elf32_Phdr *DynamicSegment;
    Elf32_Ehdr *ElfHeader;
    UINT32 FixupOffset;
    UINT32 Flags;
    BOOLEAN FoundRelocations;
    UINTN MovwOffset;
    EFI_IMAGE_OPTIONAL_HEADER_UNION *NtHeader;
    Elf32_Rel *Relocation;
    UINTN RelocationElementSize;
    UINT32 RelocationIndex;
    UINT32 RelocationOffset;
    Elf32_Shdr *RelocationSectionHeader;
    UINTN RelocationSize;
    BOOLEAN Result;
    Elf32_Shdr *SectionHeader;
    UINT32 SectionIndex;
    UINT32 TargetAddress;
    UINT8 *TargetPointer;
    UINT32 TargetValue;

    ElfHeader = Context->InputFile;

    assert((ElfHeader->e_machine == EM_386) ||
           (ElfHeader->e_machine == EM_ARM));

    MovwOffset = 0;

    //
    // Loop across all sections looking for relocation sections.
    //

    FoundRelocations = FALSE;
    for (SectionIndex = 0;
         SectionIndex < ElfHeader->e_shnum;
         SectionIndex += 1) {

        RelocationSectionHeader = ELFCONV_ELF_SECTION(ElfHeader, SectionIndex);

        //
        // Skip things that aren't relocation sections.
        //

        if ((RelocationSectionHeader->sh_type != SHT_REL) &&
            (RelocationSectionHeader->sh_type != SHT_RELA)) {

            continue;
        }

        SectionHeader = ELFCONV_ELF_SECTION(ElfHeader,
                                            RelocationSectionHeader->sh_info);

        if ((Context->Flags & ELFCONV_OPTION_VERBOSE) != 0) {
            printf("Found relocation section (index %d).\n", SectionIndex);
        }

        //
        // Skip sections that are neither text nor data.
        //

        if ((ElfconvIsTextSection(ElfHeader, SectionHeader) == FALSE) &&
            (ElfconvIsDataSection(ElfHeader, SectionHeader) == FALSE)) {

            continue;
        }

        FoundRelocations = TRUE;
        for (RelocationIndex = 0;
             RelocationIndex < RelocationSectionHeader->sh_size;
             RelocationIndex += RelocationSectionHeader->sh_entsize) {

            Relocation = (Elf32_Rel *)((UINT8 *)ElfHeader +
                                       RelocationSectionHeader->sh_offset +
                                       RelocationIndex);

            if (ElfHeader->e_machine == EM_386) {
                switch (ELF32_R_TYPE(Relocation->r_info)) {
                case R_386_NONE:
                case R_386_PC32:
                    break;

                //
                // Create a relative relocation entry from the absolute entry.
                //

                case R_386_32:
                    DestinationSectionIndex = RelocationSectionHeader->sh_info;
                    FixupOffset =
                         Context->CoffSectionsOffset[DestinationSectionIndex] +
                         (Relocation->r_offset - SectionHeader->sh_addr);

                    ElfconvCoffAddFixup(Context,
                                        FixupOffset,
                                        EFI_IMAGE_REL_BASED_HIGHLOW);

                    break;

                default:
                    fprintf(stderr,
                            "Error: Unsupported relocation type %d.\n",
                            ELF32_R_TYPE(Relocation->r_info));

                    return FALSE;
                }

            } else if (ElfHeader->e_machine == EM_ARM) {
                switch (ELF32_R_TYPE(Relocation->r_info)) {

                //
                // PC-relative relocations don't need modification.
                //

                case R_ARM_RBASE:
                case R_ARM_PC24:
                case R_ARM_REL32:
                case R_ARM_XPC25:
                case R_ARM_THM_PC22:
                case R_ARM_THM_JUMP19:
                case R_ARM_CALL:
                case R_ARM_JMP24:
                case R_ARM_THM_JUMP24:
                case R_ARM_PREL31:
                case R_ARM_MOVW_PREL_NC:
                case R_ARM_MOVT_PREL:
                case R_ARM_THM_MOVW_PREL_NC:
                case R_ARM_THM_MOVT_PREL:
                case R_ARM_THM_JMP6:
                case R_ARM_THM_ALU_PREL_11_0:
                case R_ARM_THM_PC12:
                case R_ARM_REL32_NOI:
                case R_ARM_ALU_PC_G0_NC:
                case R_ARM_ALU_PC_G0:
                case R_ARM_ALU_PC_G1_NC:
                case R_ARM_ALU_PC_G1:
                case R_ARM_ALU_PC_G2:
                case R_ARM_LDR_PC_G1:
                case R_ARM_LDR_PC_G2:
                case R_ARM_LDRS_PC_G0:
                case R_ARM_LDRS_PC_G1:
                case R_ARM_LDRS_PC_G2:
                case R_ARM_LDC_PC_G0:
                case R_ARM_LDC_PC_G1:
                case R_ARM_LDC_PC_G2:
                case R_ARM_GOT_PREL:
                case R_ARM_THM_JUMP11:
                case R_ARM_THM_JUMP8:
                case R_ARM_TLS_GD32:
                case R_ARM_TLS_LDM32:
                case R_ARM_TLS_IE32:
                case R_ARM_GOT_BREL:
                case R_ARM_BASE_PREL:
                    break;

                //
                // MOVW relocates the lower 16 bits of the address.
                //

                case R_ARM_THM_MOVW_ABS_NC:
                    DestinationSectionIndex = RelocationSectionHeader->sh_info;
                    FixupOffset =
                         Context->CoffSectionsOffset[DestinationSectionIndex] +
                         (Relocation->r_offset - SectionHeader->sh_addr);

                    Result = ElfconvCoffAddFixup(
                                               Context,
                                               FixupOffset,
                                               EFI_IMAGE_REL_BASED_ARM_MOV32T);

                    if (Result == FALSE) {
                        return FALSE;
                    }

                    //
                    // PE/COFF tracks MOVW/MOVT relocations as a single
                    // 64-bit instruction. Track this address so that an error
                    // can be logged if the next relocation is not a MOVT.
                    //

                    MovwOffset = FixupOffset;
                    break;

                //
                // MOVT relocates the upper 16 bits of the address. This is
                // already handled by the MOVW fixup.
                //

                case R_ARM_THM_MOVT_ABS:
                    DestinationSectionIndex = RelocationSectionHeader->sh_info;
                    FixupOffset =
                         Context->CoffSectionsOffset[DestinationSectionIndex] +
                         (Relocation->r_offset - SectionHeader->sh_addr);

                    if (FixupOffset != MovwOffset + 4) {
                        fprintf(stderr,
                                "Error: PE requires MOVW+MOVT instruction "
                                "sequences together.\n");

                        return FALSE;
                    }

                    break;

                //
                // This is a 32 bit absolute relocation.
                //

                case R_ARM_ABS32:
                case R_ARM_RABS32:
                    DestinationSectionIndex = RelocationSectionHeader->sh_info;
                    FixupOffset =
                         Context->CoffSectionsOffset[DestinationSectionIndex] +
                         (Relocation->r_offset - SectionHeader->sh_addr);

                    Result = ElfconvCoffAddFixup(Context,
                                                 FixupOffset,
                                                 EFI_IMAGE_REL_BASED_HIGHLOW);

                    if (Result == FALSE) {
                        return FALSE;
                    }

                    break;

                default:
                    fprintf(stderr,
                            "Error: Unsupported ARM relocation type %d.\n",
                            ELF32_R_TYPE(Relocation->r_info));

                    return FALSE;
                }

            } else {

                assert(FALSE);

                return FALSE;
            }
        }
    }

    //
    // If relocations were not found, try again looking for PT_DYNAMIC instead
    // of SHT_REL.
    //

    if (FoundRelocations == FALSE) {
        for (SectionIndex = 0;
             SectionIndex < ElfHeader->e_phnum;
             SectionIndex += 1) {

            RelocationElementSize = 0;
            RelocationSize = 0;
            RelocationOffset = 0;
            DynamicSegment =
                           ELFCONV_ELF_PROGRAM_HEADER(ElfHeader, SectionIndex);

            if (DynamicSegment->p_type != PT_DYNAMIC) {
                continue;
            }

            if ((Context->Flags & ELFCONV_OPTION_VERBOSE) != 0) {
                printf("Found dynamic section (index %d)\n", SectionIndex);
            }

            DynamicSection =
                  (Elf32_Dyn *)((UINT8 *)ElfHeader + DynamicSegment->p_offset);

            while (DynamicSection->d_tag != DT_NULL) {
                switch (DynamicSection->d_tag) {
                case DT_REL:
                    RelocationOffset = DynamicSection->d_un.d_val;
                    if ((Context->Flags & ELFCONV_OPTION_VERBOSE) != 0) {
                        printf("Relocation offset %x.\n",
                               (int)RelocationOffset);
                    }

                    Result = ElfconvConvertElfAddress(Context,
                                                      &RelocationOffset);

                    if (Result == FALSE) {
                        fprintf(stderr,
                                "Error: Failed to convert dynamic relocation "
                                "address %x to destination image offset.\n",
                                RelocationOffset);

                        return FALSE;
                    }

                    if ((Context->Flags & ELFCONV_OPTION_VERBOSE) != 0) {
                        printf("Adjusted relocation offset %x.\n",
                               RelocationOffset);
                    }

                    break;

                case DT_RELSZ:
                    RelocationSize = DynamicSection->d_un.d_val;
                    if ((Context->Flags & ELFCONV_OPTION_VERBOSE) != 0) {
                        printf("Relocation size %x.\n",
                               (UINT32)RelocationSize);
                    }

                    break;

                case DT_RELENT:
                    RelocationElementSize = DynamicSection->d_un.d_val;
                    if ((Context->Flags & ELFCONV_OPTION_VERBOSE) != 0) {
                        printf("Relocation element size %x.\n",
                               (UINT32)RelocationElementSize);
                    }

                    break;

                default:
                    break;
                }

                DynamicSection += 1;
            }

            if ((Context->Flags & ELFCONV_OPTION_VERBOSE) != 0) {
                printf("Relocations: PHDR %d (p_offset %x) Offset %x Size %x "
                       "ElemSize %x\n",
                       SectionIndex,
                       DynamicSegment->p_offset,
                       RelocationOffset,
                       (UINT32)RelocationSize,
                       (UINT32)RelocationElementSize);
            }

            for (RelocationIndex = 0;
                 RelocationIndex < RelocationSize;
                 RelocationIndex += RelocationElementSize) {

                Relocation = (Elf32_Rel *)(Context->CoffFile +
                                           RelocationOffset +
                                           RelocationIndex);

                if (ElfHeader->e_machine == EM_386) {
                    switch (ELF32_R_TYPE(Relocation->r_info)) {

                    //
                    // Relative relocations contain a default VA in them. First
                    // convert that VA into a COFF offset, then create a
                    // relocation for it. If the value conversion failed, it's
                    // probably a relocation off in a stabs section, so ignore
                    // it.
                    //

                    case R_386_RELATIVE:
                        TargetAddress = Relocation->r_offset;
                        Result = ElfconvConvertElfAddress(Context,
                                                          &TargetAddress);

                        if (Result == FALSE) {
                            fprintf(stderr,
                                    "R_386_RELATIVE target convert failed.\n");

                            return FALSE;
                        }

                        TargetPointer = Context->CoffFile + TargetAddress;
                        TargetValue = *(UINT32 *)TargetPointer;
                        Result = ElfconvConvertElfAddress(Context,
                                                          &TargetValue);

                        if (Result == FALSE) {
                            if ((Context->Flags &
                                 ELFCONV_OPTION_VERBOSE) != 0) {

                                printf("Skipping relocation at address %x that "
                                       "had value %x that could not be "
                                       "converted in the destination.\n",
                                       TargetAddress,
                                       TargetValue);
                            }

                            break;
                        }

                        //
                        // Stick the old value (now an offset in the COFF image)
                        // into the relocation's spot.
                        //

                        *(UINT32 *)TargetPointer = TargetValue;
                        Result = ElfconvCoffAddFixup(
                                                  Context,
                                                  TargetAddress,
                                                  EFI_IMAGE_REL_BASED_HIGHLOW);

                        if (Result == FALSE) {
                            return FALSE;
                        }

                        break;

                    default:
                        fprintf(stderr,
                                "Bad 386 dynamic relocation type %d, offset "
                                "%x, program header index %d.\n",
                                ELF32_R_TYPE(Relocation->r_info),
                                (UINT32)RelocationOffset,
                                SectionIndex);

                        return FALSE;
                    }

                } else if (ElfHeader->e_machine == EM_ARM) {
                    switch (ELF32_R_TYPE(Relocation->r_info)) {
                    case R_ARM_RBASE:
                        break;

                    //
                    // Relative relocations contain a default VA in them. First
                    // convert that VA into a COFF offset, then create a
                    // relocation for it. If the value conversion failed, it's
                    // probably a relocation off in a stabs section, so ignore
                    // it.
                    //

                    case R_ARM_RELATIVE:
                        TargetAddress = Relocation->r_offset;
                        Result = ElfconvConvertElfAddress(Context,
                                                          &TargetAddress);

                        if (Result == FALSE) {
                            fprintf(stderr,
                                    "R_ARM_RELATIVE target convert failed.\n");

                            return FALSE;
                        }

                        TargetPointer = Context->CoffFile + TargetAddress;
                        TargetValue = *(UINT32 *)TargetPointer;
                        Result = ElfconvConvertElfAddress(Context,
                                                          &TargetValue);

                        if (Result == FALSE) {
                            if ((Context->Flags &
                                 ELFCONV_OPTION_VERBOSE) != 0) {

                                printf("Skipping relocation at address %x that "
                                       "had value %x that could not be "
                                       "converted in the destination.\n",
                                       TargetAddress,
                                       TargetValue);
                            }

                            break;
                        }

                        //
                        // Stick the old value (now an offset in the COFF image)
                        // into the relocation's spot.
                        //

                        *(UINT32 *)TargetPointer = TargetValue;
                        Result = ElfconvCoffAddFixup(
                                                  Context,
                                                  TargetAddress,
                                                  EFI_IMAGE_REL_BASED_HIGHLOW);

                        if (Result == FALSE) {
                            return FALSE;
                        }

                        break;

                    default:
                        fprintf(stderr,
                                "Bad ARM dynamic relocation type %d, offset "
                                "%x, program header index %d.\n",
                                ELF32_R_TYPE(Relocation->r_info),
                                (UINT32)RelocationOffset,
                                SectionIndex);

                        return FALSE;
                    }

                } else {

                    assert(FALSE);

                    return FALSE;
                }
            }
        }
    }

    //
    // Pad the page entry out by adding extra entries.
    //

    while ((Context->CoffOffset & (ELFCONV_COFF_ALIGNMENT - 1)) != 0) {
        ElfconvCoffAddFixupEntry(Context, 0);
    }

    //
    // Create the relocation section.
    //

    NtHeader = (EFI_IMAGE_OPTIONAL_HEADER_UNION *)(Context->CoffFile +
                                                   Context->NtHeaderOffset);

    Directories = &(NtHeader->Pe32.OptionalHeader.DataDirectory[0]);
    Directory = &(Directories[EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC]);
    Directory->Size = Context->CoffOffset - Context->RelocationOffset;
    if (Directory->Size == 0) {
        Directory->VirtualAddress = 0;
        NtHeader->Pe32.FileHeader.NumberOfSections -= 1;

    } else {
        Directory->VirtualAddress = Context->RelocationOffset;
        Flags = EFI_IMAGE_SCN_CNT_INITIALIZED_DATA |
                EFI_IMAGE_SCN_MEM_DISCARDABLE |
                EFI_IMAGE_SCN_MEM_READ;

        ElfconvCreateSectionHeader(
                              Context,
                              ".reloc",
                              Context->RelocationOffset,
                              Context->CoffOffset - Context->RelocationOffset,
                              Flags);
    }

    return TRUE;
}

BOOLEAN
ElfconvWriteDebug32 (
    PELFCONV_CONTEXT Context
    )

/*++

Routine Description:

    This routine writes out the debug sections.

Arguments:

    Context - Supplies a pointer to the ELF context.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    Elf32_Ehdr *ElfHeader;
    Elf32_Shdr *ElfSection;
    UINT32 Flags;
    VOID *NewBuffer;
    EFI_IMAGE_OPTIONAL_HEADER_UNION *NtHeader;
    UINT32 SectionCount;
    UINTN SectionIndex;
    CHAR8 *SectionName;
    UINT32 SectionOffset;
    Elf32_Shdr *StringSection;

    ElfHeader = Context->InputFile;

    assert((ElfHeader->e_machine == EM_386) ||
           (ElfHeader->e_machine == EM_ARM));

    //
    // Find and wrangle debug sections.
    //

    Context->DataOffset = Context->CoffOffset;
    SectionCount = 0;
    for (SectionIndex = 0;
         SectionIndex < ElfHeader->e_shnum;
         SectionIndex += 1) {

        ElfSection = ELFCONV_ELF_SECTION(ElfHeader, SectionIndex);
        if (ElfconvIsDebugSection(ElfHeader, ElfSection) == FALSE) {
            continue;
        }

        SectionOffset = Context->CoffOffset;
        Context->CoffSectionsOffset[SectionIndex] = SectionOffset;
        Context->CoffOffset += ElfSection->sh_size;
        NewBuffer = realloc(Context->CoffFile, Context->CoffOffset);
        if (NewBuffer == NULL) {
            return FALSE;
        }

        Context->CoffFile = NewBuffer;

        //
        // Create the section.
        //

        StringSection = ELFCONV_ELF_SECTION(ElfHeader, ElfHeader->e_shstrndx);
        SectionName = (CHAR8 *)ElfHeader + StringSection->sh_offset +
                      ElfSection->sh_name;

        Flags = EFI_IMAGE_SCN_MEM_READ | EFI_IMAGE_SCN_MEM_DISCARDABLE;
        ElfconvCreateSectionHeader(Context,
                                   SectionName,
                                   SectionOffset,
                                   ElfSection->sh_size,
                                   Flags);

        memcpy(Context->CoffFile + SectionOffset,
               (UINT8 *)ElfHeader + ElfSection->sh_offset,
               ElfSection->sh_size);

        SectionCount += 1;
    }

    //
    // Also write out the string table at this point.
    //

    if (Context->StringTable != NULL) {
        NewBuffer = realloc(Context->CoffFile,
                            Context->CoffOffset + Context->StringTableSize);

        if (NewBuffer == NULL) {
            return FALSE;
        }

        Context->CoffFile = NewBuffer;

        //
        // Set the final size in the string table, then copy the string table
        // to the image.
        //

        memcpy(Context->StringTable,
               &(Context->StringTableSize),
               sizeof(UINT32));

        memcpy(Context->CoffFile + Context->CoffOffset,
               Context->StringTable,
               Context->StringTableSize);

        NtHeader = (EFI_IMAGE_OPTIONAL_HEADER_UNION *)(Context->CoffFile +
                                                       Context->NtHeaderOffset);

        NtHeader->Pe32.FileHeader.PointerToSymbolTable = Context->CoffOffset;
        Context->CoffOffset += Context->StringTableSize;
    }

    return TRUE;
}

VOID
ElfconvSetImageSize32 (
    PELFCONV_CONTEXT Context
    )

/*++

Routine Description:

    This routine sets the final image size.

Arguments:

    Context - Supplies a pointer to the conversion context.

Return Value:

    None.

--*/

{

    EFI_IMAGE_OPTIONAL_HEADER_UNION *NtHeader;

    NtHeader = (EFI_IMAGE_OPTIONAL_HEADER_UNION *)(Context->CoffFile +
                                                   Context->NtHeaderOffset);

    NtHeader->Pe32.OptionalHeader.SizeOfImage = Context->CoffOffset;
    return;
}

VOID
ElfconvCleanUp32 (
    PELFCONV_CONTEXT Context
    )

/*++

Routine Description:

    This routine performs final cleanup actions.

Arguments:

    Context - Supplies a pointer to the conversion context.

Return Value:

    None.

--*/

{

    if (Context->CoffSectionsOffset != NULL) {
        free(Context->CoffSectionsOffset);
        Context->CoffSectionsOffset = NULL;
    }

    return;
}

BOOLEAN
ElfconvIsTextSection (
    Elf32_Ehdr *ElfHeader,
    Elf32_Shdr *SectionHeader
    )

/*++

Routine Description:

    This routine determines if the given section is a text section.

Arguments:

    ElfHeader - Supplies a pointer to the ELF file header.

    SectionHeader - Supplies a pointer to the section header in question.

Return Value:

    TRUE if it is a text section.

    FALSE if it is not a text section.

--*/

{

    if ((SectionHeader->sh_type != SHT_PROGBITS) &&
        (SectionHeader->sh_type != SHT_REL)) {

        return FALSE;
    }

    if ((SectionHeader->sh_flags & (SHF_WRITE | SHF_ALLOC)) == SHF_ALLOC) {
        return TRUE;
    }

    return FALSE;
}

BOOLEAN
ElfconvIsDataSection (
    Elf32_Ehdr *ElfHeader,
    Elf32_Shdr *SectionHeader
    )

/*++

Routine Description:

    This routine determines if the given section is a data section.

Arguments:

    ElfHeader - Supplies a pointer to the ELF file header.

    SectionHeader - Supplies a pointer to the section header in question.

Return Value:

    TRUE if it is a data section.

    FALSE if it is not a data section.

--*/

{

    if (ElfconvIsHiiRsrcSection(ElfHeader, SectionHeader) != FALSE) {
        return FALSE;
    }

    if ((SectionHeader->sh_type != SHT_PROGBITS) &&
        (SectionHeader->sh_type != SHT_NOBITS) &&
        (SectionHeader->sh_type != SHT_DYNAMIC)) {

        return FALSE;
    }

    if ((SectionHeader->sh_flags & (SHF_WRITE | SHF_ALLOC)) ==
        (SHF_WRITE | SHF_ALLOC)) {

        return TRUE;
    }

    return FALSE;
}

BOOLEAN
ElfconvIsHiiRsrcSection (
    Elf32_Ehdr *ElfHeader,
    Elf32_Shdr *SectionHeader
    )

/*++

Routine Description:

    This routine determines if the given section is a HII .rsrc section.

Arguments:

    ElfHeader - Supplies a pointer to the ELF file header.

    SectionHeader - Supplies a pointer to the section header in question.

Return Value:

    TRUE if it is a HII rsrc section.

    FALSE if it is not the HII rsrc section.

--*/

{

    CHAR8 *SectionName;
    Elf32_Shdr *StringSection;

    StringSection = ELFCONV_ELF_SECTION(ElfHeader, ElfHeader->e_shstrndx);
    SectionName = (CHAR8 *)ElfHeader + StringSection->sh_offset +
                  SectionHeader->sh_name;

    if (strcmp(SectionName, ELFCONV_HII_SECTION_NAME) == 0) {
        return TRUE;
    }

    return FALSE;
}

BOOLEAN
ElfconvIsDebugSection (
    Elf32_Ehdr *ElfHeader,
    Elf32_Shdr *SectionHeader
    )

/*++

Routine Description:

    This routine determines if the given section is a stabs or stabs string
    section.

Arguments:

    ElfHeader - Supplies a pointer to the ELF file header.

    SectionHeader - Supplies a pointer to the section header in question.

Return Value:

    TRUE if it is a HII rsrc section.

    FALSE if it is not the HII rsrc section.

--*/

{

    CHAR8 **DebugSection;
    CHAR8 *SectionName;
    Elf32_Shdr *StringSection;

    StringSection = ELFCONV_ELF_SECTION(ElfHeader, ElfHeader->e_shstrndx);
    SectionName = (CHAR8 *)ElfHeader + StringSection->sh_offset +
                  SectionHeader->sh_name;

    DebugSection = ElfconvDebugSections;
    while (*DebugSection != NULL) {
        if (strcmp(SectionName, *DebugSection) == 0) {
            return TRUE;
        }

        DebugSection += 1;
    }

    return FALSE;
}

VOID
ElfconvThumbMovtImmediatePatch (
    UINT16 *Instruction,
    UINT16 Address
    )

/*++

Routine Description:

    This routine updates an ARM MOVT or MOVW immediate instruction.

Arguments:

    Instruction - Supplies a pointer to the ARM MOVT or MOVW immediate
        instruction.

    Address - Supplies the new address to patch into the instruction.

Return Value:

    None.

--*/

{

    UINT16 Patch;

    //
    // Patch the first 16-bit chunk of the instruction.
    //

    Patch = (Address >> 12) & 0x000F;
    if ((Address & (1 << 11)) != 0) {
        Patch |= 1 << 10;
    }

    *Instruction = (*Instruction & ~0x040F) | Patch;

    //
    // Patch the second 16-bit chunk of the instruction.
    //

    Patch = Address & 0x000000FF;
    Patch |= (Address << 4) & 0x00007000;
    Instruction += 1;
    *Instruction = (*Instruction & 0x70FF) | Patch;
    return;
}

BOOLEAN
ElfconvConvertElfAddress (
    PELFCONV_CONTEXT Context,
    UINT32 *Address
    )

/*++

Routine Description:

    This routine converts a memory address in the ELF image into an offset
    within the COFF file.

Arguments:

    Context - Supplies a pointer to the conversion context.

    Address - Supplies a pointer that on input contains the address to convert.
        On output, this will contain the COFF offset.

Return Value:

    TRUE on success.

    FALSE if the address is not valid.

--*/

{

    Elf32_Ehdr *ElfHeader;
    Elf32_Shdr *SectionHeader;
    UINT32 SectionIndex;

    ElfHeader = Context->InputFile;

    //
    // Loop through all the sections looking for the one that contains this
    // address.
    //

    for (SectionIndex = 0;
         SectionIndex < ElfHeader->e_shnum;
         SectionIndex += 1) {

        SectionHeader = ELFCONV_ELF_SECTION(ElfHeader, SectionIndex);

        //
        // If the section header contains the address, then return the
        // equivalent offset in the COFF section.
        //

        if ((SectionHeader->sh_addr <= *Address) &&
            (SectionHeader->sh_addr + SectionHeader->sh_size > *Address)) {

            if (Context->CoffSectionsOffset[SectionIndex] == 0) {
                return FALSE;
            }

            *Address = *Address - SectionHeader->sh_addr +
                       Context->CoffSectionsOffset[SectionIndex];

            return TRUE;
        }
    }

    return FALSE;
}

