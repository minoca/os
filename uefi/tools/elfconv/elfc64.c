/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    elfc64.c

Abstract:

    This module implements support for converting an ELF64 image to a PE
    image.

Author:

    Evan Green 11-Aug-2017

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
// ------------------------------------------------------ Data Type Definitions
//

typedef
BOOLEAN
(*PELFCONV_SECTION_FILTER_FUNCTION) (
    Elf64_Ehdr *ElfHeader,
    Elf64_Shdr *SectionHeader
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
ElfconvScanSections64 (
    PELFCONV_CONTEXT Context
    );

BOOLEAN
ElfconvWriteSections64 (
    PELFCONV_CONTEXT Context,
    ELFCONV_SECTION_FILTER FilterType
    );

BOOLEAN
ElfconvWriteRelocations64 (
    PELFCONV_CONTEXT Context
    );

BOOLEAN
ElfconvWriteDebug64 (
    PELFCONV_CONTEXT Context
    );

VOID
ElfconvSetImageSize64 (
    PELFCONV_CONTEXT Context
    );

VOID
ElfconvCleanUp64 (
    PELFCONV_CONTEXT Context
    );

BOOLEAN
ElfconvIsTextSection64 (
    Elf64_Ehdr *ElfHeader,
    Elf64_Shdr *SectionHeader
    );

BOOLEAN
ElfconvIsDataSection64 (
    Elf64_Ehdr *ElfHeader,
    Elf64_Shdr *SectionHeader
    );

BOOLEAN
ElfconvIsHiiRsrcSection64 (
    Elf64_Ehdr *ElfHeader,
    Elf64_Shdr *SectionHeader
    );

BOOLEAN
ElfconvIsDebugSection64 (
    Elf64_Ehdr *ElfHeader,
    Elf64_Shdr *SectionHeader
    );

BOOLEAN
ElfconvConvertElfAddress64 (
    PELFCONV_CONTEXT Context,
    UINT64 *Address
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

BOOLEAN
ElfconvInitializeElf64 (
    PELFCONV_CONTEXT Context,
    PELFCONV_FUNCTION_TABLE FunctionTable
    )

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

{

    Elf64_Ehdr *ElfHeader;
    BOOLEAN Result;

    Result = FALSE;
    ElfHeader = Context->InputFile;
    if (Context->InputFileSize < sizeof(Elf64_Ehdr)) {
        if ((ElfHeader->e_ident[EI_CLASS] != ELFCLASS64) ||
            (ElfHeader->e_ident[EI_DATA] != ELFDATA2LSB) ||
            ((ElfHeader->e_type != ET_EXEC) && (ElfHeader->e_type != ET_DYN)) ||
            ((ElfHeader->e_machine != EM_X86_64) &&
             (ElfHeader->e_machine != EM_AARCH64)) ||
            (ElfHeader->e_version != EV_CURRENT)) {

            fprintf(stderr, "ELF Image not valid.\n");
            goto InitializeElf64End;
        }
    }

    Context->CoffSectionsOffset = malloc(ElfHeader->e_shnum * sizeof(UINT32));
    if (Context->CoffSectionsOffset == NULL) {
        goto InitializeElf64End;
    }

    memset(Context->CoffSectionsOffset, 0, ElfHeader->e_shnum * sizeof(UINT32));
    FunctionTable->ScanSections = ElfconvScanSections64;
    FunctionTable->WriteSections = ElfconvWriteSections64;
    FunctionTable->WriteRelocations = ElfconvWriteRelocations64;
    FunctionTable->WriteDebug = ElfconvWriteDebug64;
    FunctionTable->SetImageSize = ElfconvSetImageSize64;
    FunctionTable->CleanUp = ElfconvCleanUp64;
    Result = TRUE;

InitializeElf64End:
    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOLEAN
ElfconvScanSections64 (
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

    UINT64 CoffEntry;
    EFI_IMAGE_DATA_DIRECTORY *DataDirectory;
    EFI_IMAGE_DOS_HEADER *DosHeader;
    Elf64_Ehdr *ElfHeader;
    Elf64_Shdr *ElfSection;
    UINT64 Flags;
    BOOLEAN FoundText;
    EFI_IMAGE_OPTIONAL_HEADER_UNION *NtHeader;
    UINTN SectionIndex;
    CHAR8 *SectionName;
    Elf64_Shdr *StringSection;

    CoffEntry = 0;
    Context->CoffOffset = 0;
    Context->TextOffset = 0;
    Context->CoffOffset = sizeof(EFI_IMAGE_DOS_HEADER) + 0x40;
    Context->NtHeaderOffset = Context->CoffOffset;
    FoundText = FALSE;
    ElfHeader = Context->InputFile;

    assert((ElfHeader->e_machine == EM_X86_64) ||
           (ElfHeader->e_machine == EM_AARCH64));

    Context->CoffOffset += sizeof(EFI_IMAGE_NT_HEADERS64);
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
        if (ElfconvIsTextSection64(ElfHeader, ElfSection) != FALSE) {
            if ((Context->Flags & ELFCONV_OPTION_VERBOSE) != 0) {
                StringSection = ELFCONV_ELF_SECTION(ElfHeader,
                                                    ElfHeader->e_shstrndx);

                SectionName = (CHAR8 *)ElfHeader + StringSection->sh_offset +
                              ElfSection->sh_name;

                printf("Found text section %s: Offset 0x%llx, size 0x%llx.\n",
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

    //
    // Find and wrangle data sections.
    //

    Context->DataOffset = Context->CoffOffset;
    for (SectionIndex = 0;
         SectionIndex < ElfHeader->e_shnum;
         SectionIndex += 1) {

        ElfSection = ELFCONV_ELF_SECTION(ElfHeader, SectionIndex);
        if (ElfconvIsDataSection64(ElfHeader, ElfSection) != FALSE) {
            if ((Context->Flags & ELFCONV_OPTION_VERBOSE) != 0) {
                StringSection = ELFCONV_ELF_SECTION(ElfHeader,
                                                    ElfHeader->e_shstrndx);

                SectionName = (CHAR8 *)ElfHeader + StringSection->sh_offset +
                              ElfSection->sh_name;

                printf("Found data section %s: Offset 0x%llx, size 0x%llx.\n",
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
        if (ElfconvIsHiiRsrcSection64(ElfHeader, ElfSection) != FALSE) {
            if ((Context->Flags & ELFCONV_OPTION_VERBOSE) != 0) {
                StringSection = ELFCONV_ELF_SECTION(ElfHeader,
                                                    ElfHeader->e_shstrndx);

                SectionName = (CHAR8 *)ElfHeader + StringSection->sh_offset +
                              ElfSection->sh_name;

                printf("Found rsrc section %s: Offset 0x%llx, size 0x%llx.\n",
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

    NtHeader->Pe32Plus.Signature = EFI_IMAGE_NT_SIGNATURE;
    switch (ElfHeader->e_machine) {
    case EM_X86_64:
        NtHeader->Pe32Plus.FileHeader.Machine = EFI_IMAGE_MACHINE_X64;
        break;

    case EM_AARCH64:
        NtHeader->Pe32Plus.FileHeader.Machine = EFI_IMAGE_MACHINE_AARCH64;
        break;

    default:

        assert(FALSE);

        return FALSE;
    }

    NtHeader->Pe32Plus.OptionalHeader.Magic = EFI_IMAGE_NT_OPTIONAL_HDR64_MAGIC;
    NtHeader->Pe32Plus.FileHeader.NumberOfSections = ELFCONV_PE_SECTION_COUNT;
    NtHeader->Pe32Plus.FileHeader.TimeDateStamp = (UINT64)time(NULL);
    Context->ImageTimestamp = NtHeader->Pe32Plus.FileHeader.TimeDateStamp;
    NtHeader->Pe32Plus.FileHeader.PointerToSymbolTable = 0;
    NtHeader->Pe32Plus.FileHeader.NumberOfSymbols = 0;
    NtHeader->Pe32Plus.FileHeader.SizeOfOptionalHeader =
                                     sizeof(NtHeader->Pe32Plus.OptionalHeader);

    NtHeader->Pe32Plus.FileHeader.Characteristics =
                                           EFI_IMAGE_FILE_EXECUTABLE_IMAGE |
                                           EFI_IMAGE_FILE_LINE_NUMS_STRIPPED |
                                           EFI_IMAGE_FILE_LOCAL_SYMS_STRIPPED |
                                           EFI_IMAGE_FILE_LARGE_ADDRESS_AWARE;

    NtHeader->Pe32Plus.OptionalHeader.SizeOfCode = Context->DataOffset -
                                                   Context->TextOffset;

    NtHeader->Pe32Plus.OptionalHeader.SizeOfInitializedData =
                               Context->RelocationOffset - Context->DataOffset;

    NtHeader->Pe32Plus.OptionalHeader.SizeOfUninitializedData = 0;
    NtHeader->Pe32Plus.OptionalHeader.AddressOfEntryPoint = CoffEntry;
    NtHeader->Pe32Plus.OptionalHeader.BaseOfCode = Context->TextOffset;
    NtHeader->Pe32Plus.OptionalHeader.ImageBase = 0;
    NtHeader->Pe32Plus.OptionalHeader.SectionAlignment = ELFCONV_COFF_ALIGNMENT;
    NtHeader->Pe32Plus.OptionalHeader.FileAlignment = ELFCONV_COFF_ALIGNMENT;
    NtHeader->Pe32Plus.OptionalHeader.SizeOfImage = 0;
    NtHeader->Pe32Plus.OptionalHeader.SizeOfHeaders = Context->TextOffset;
    NtHeader->Pe32Plus.OptionalHeader.Subsystem = Context->SubsystemType;
    NtHeader->Pe32Plus.OptionalHeader.NumberOfRvaAndSizes =
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
        NtHeader->Pe32Plus.FileHeader.NumberOfSections -= 1;
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
        NtHeader->Pe32Plus.FileHeader.NumberOfSections -= 1;
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

        DataDirectory = &(NtHeader->Pe32Plus.OptionalHeader.DataDirectory[0]);
        DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_RESOURCE].Size =
                            Context->RelocationOffset - Context->HiiRsrcOffset;

        DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress =
                                                        Context->HiiRsrcOffset;

    } else {
        NtHeader->Pe32Plus.FileHeader.NumberOfSections -= 1;
    }

    return TRUE;
}

BOOLEAN
ElfconvWriteSections64 (
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

    VOID *Destination;
    ptrdiff_t Difference;
    Elf64_Ehdr *ElfHeader;
    Elf64_Shdr *ElfSection;
    PELFCONV_SECTION_FILTER_FUNCTION FilterFunction;
    Elf64_Rela *Relocation;
    UINT32 RelocationOffset;
    Elf64_Shdr *RelocationSection;
    UINT32 SectionIndex;
    UINT32 SectionOffset;
    Elf64_Sym *Symbol;
    Elf64_Shdr *SymbolSectionHeader;
    UINT8 *SymbolTable;
    Elf64_Shdr *SymbolTableSection;
    UINT8 *Target;

    ElfHeader = Context->InputFile;

    assert((ElfHeader->e_machine == EM_X86_64) ||
           (ElfHeader->e_machine == EM_AARCH64));

    switch (FilterType) {
    case ElfconvSectionText:
        FilterFunction = ElfconvIsTextSection64;
        break;

    case ElfconvSectionData:
        FilterFunction = ElfconvIsDataSection64;
        break;

    case ElfconvSectionHii:
        FilterFunction = ElfconvIsHiiRsrcSection64;
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
                    printf("Copying section from ELF offset %llx, size %llx to "
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
                    printf("Zeroing COFF offset %lx, size %llx\n",
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
                        (unsigned int)(ElfSection->sh_type));

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

        if ((RelocationSection->sh_type == SHT_REL) ||
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

            Relocation = (Elf64_Rela *)((UINT8 *)ElfHeader +
                                        RelocationSection->sh_offset +
                                        RelocationOffset);

            Symbol = (Elf64_Sym *)(SymbolTable +
                                   (ELF64_R_SYM(Relocation->r_info) *
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

            if (ElfHeader->e_machine == EM_X86_64) {
                switch (ELF64_R_TYPE(Relocation->r_info)) {
                case R_X86_64_NONE:
                    break;

                //
                // This is an absolute relocation. Convert the target from an
                // absolute virtuall address to an absolute COFF address.
                //

                case R_X86_64_64:
                    *(UINT64 *)Target =
                                 *(UINT64 *)Target -
                                 SymbolSectionHeader->sh_addr +
                                 Context->CoffSectionsOffset[Symbol->st_shndx];

                    break;

                //
                // 32-bit absolute relocation.
                //

                case R_X86_64_32:
                    *(UINT32 *)Target =
                        (UINT32)((UINT64)(*(UINT32 *)Target) -
                                 SymbolSectionHeader->sh_addr +
                                 Context->CoffSectionsOffset[Symbol->st_shndx]);

                    break;

                //
                // 32-bit signed absolute relocation.
                //

                case R_X86_64_32S:
                    *(INT32 *)Target =
                        (INT32)((INT64)(*(INT32 *)Target) -
                                SymbolSectionHeader->sh_addr +
                                Context->CoffSectionsOffset[Symbol->st_shndx]);

                    break;

                //
                // This is a PC-relative relocation.
                //

                case R_X86_64_PC32:
                    *(UINT32 *)Target =
                        (UINT32)(*(UINT32 *)Target +
                               (Context->CoffSectionsOffset[Symbol->st_shndx] -
                                SymbolSectionHeader->sh_addr) -
                               (SectionOffset - ElfSection->sh_addr));

                    break;

                default:
                    fprintf(stderr,
                            "Error: Unsupported relocation type %lld.\n",
                            ELF64_R_TYPE(Relocation->r_info));

                    return FALSE;
                }

            } else if (ElfHeader->e_machine == EM_ARM) {
                switch (ELF64_R_TYPE(Relocation->r_info)) {
                case R_AARCH64_ADR_PREL_LO21:
                case R_AARCH64_CONDBR19:
                case R_AARCH64_LD_PREL_LO19:
                    if (Relocation->r_addend != 0) {
                        fprintf(stderr, "Error: Addends not supported.\n");
                        return FALSE;
                    }

                    break;

                case R_AARCH64_CALL26:
                case R_AARCH64_JUMP26:
                    if (Relocation->r_addend != 0) {

                        //
                        // Allow certain relocations to get ignored since they
                        // patch relative to the text section and there's only
                        // one text section.
                        //

                        if (ELF64_ST_TYPE(Symbol->st_info) == STT_SECTION) {
                            break;
                        }

                        fprintf(stderr, "Error: Addends not supported.\n");
                        return FALSE;
                    }

                    break;

                case R_AARCH64_ADR_PREL_PG_HI21:
                case R_AARCH64_ADD_ABS_LO12_NC:
                    fprintf(stderr, "Small memory model not supported.\n");
                    return FALSE;

                case R_AARCH64_ABS64:
                    *(UINT64 *)Target =
                                 *(UINT64 *)Target +
                                 SymbolSectionHeader->sh_addr +
                                 Context->CoffSectionsOffset[Symbol->st_shndx];

                    break;

                case R_AARCH64_ABS32:
                    *(UINT32 *)Target =
                                 *(UINT32 *)Target +
                                 SymbolSectionHeader->sh_addr +
                                 Context->CoffSectionsOffset[Symbol->st_shndx];

                    break;

                default:
                    fprintf(stderr,
                            "Error: Unsupported relocation type %lld.\n",
                            ELF64_R_TYPE(Relocation->r_info));

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
ElfconvWriteRelocations64 (
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

    UINT64 DestinationSectionIndex;
    EFI_IMAGE_DATA_DIRECTORY *Directories;
    EFI_IMAGE_DATA_DIRECTORY *Directory;
    Elf64_Dyn *DynamicSection;
    Elf64_Phdr *DynamicSegment;
    Elf64_Ehdr *ElfHeader;
    UINT64 FixupOffset;
    UINT64 Flags;
    BOOLEAN FoundRelocations;
    EFI_IMAGE_OPTIONAL_HEADER_UNION *NtHeader;
    Elf64_Rela *Relocation;
    UINTN RelocationElementSize;
    UINT64 RelocationIndex;
    UINT64 RelocationOffset;
    Elf64_Shdr *RelocationSectionHeader;
    UINTN RelocationSize;
    BOOLEAN Result;
    Elf64_Shdr *SectionHeader;
    UINT32 SectionIndex;
    UINT64 TargetAddress;
    UINT8 *TargetPointer;
    UINT64 TargetValue;

    ElfHeader = Context->InputFile;

    assert((ElfHeader->e_machine == EM_X86_64) ||
           (ElfHeader->e_machine == EM_AARCH64));

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

        if ((ElfconvIsTextSection64(ElfHeader, SectionHeader) == FALSE) &&
            (ElfconvIsDataSection64(ElfHeader, SectionHeader) == FALSE)) {

            continue;
        }

        FoundRelocations = TRUE;
        for (RelocationIndex = 0;
             RelocationIndex < RelocationSectionHeader->sh_size;
             RelocationIndex += RelocationSectionHeader->sh_entsize) {

            Relocation = (Elf64_Rela *)((UINT8 *)ElfHeader +
                                        RelocationSectionHeader->sh_offset +
                                        RelocationIndex);

            if (ElfHeader->e_machine == EM_X86_64) {
                switch (ELF64_R_TYPE(Relocation->r_info)) {
                case R_X86_64_NONE:
                case R_X86_64_PC32:
                    break;

                //
                // Create a relative relocation entry from the absolute entry.
                //

                case R_X86_64_64:
                    DestinationSectionIndex = RelocationSectionHeader->sh_info;
                    FixupOffset =
                         Context->CoffSectionsOffset[DestinationSectionIndex] +
                         (Relocation->r_offset - SectionHeader->sh_addr);

                    ElfconvCoffAddFixup(Context,
                                        (UINT32)FixupOffset,
                                        EFI_IMAGE_REL_BASED_DIR64);

                    break;

                case R_X86_64_32:
                case R_X86_64_32S:
                    DestinationSectionIndex = RelocationSectionHeader->sh_info;
                    FixupOffset =
                         Context->CoffSectionsOffset[DestinationSectionIndex] +
                         (Relocation->r_offset - SectionHeader->sh_addr);

                    ElfconvCoffAddFixup(Context,
                                        (UINT32)FixupOffset,
                                        EFI_IMAGE_REL_BASED_HIGHLOW);

                    break;

                default:
                    fprintf(stderr,
                            "Error: Unsupported relocation type %lld.\n",
                            ELF64_R_TYPE(Relocation->r_info));

                    return FALSE;
                }

            } else if (ElfHeader->e_machine == EM_ARM) {
                switch (ELF64_R_TYPE(Relocation->r_info)) {

                //
                // PC-relative relocations don't need modification.
                //

                case R_AARCH64_ADR_PREL_LO21:
                case R_AARCH64_CONDBR19:
                case R_AARCH64_LD_PREL_LO19:
                case R_AARCH64_CALL26:
                case R_AARCH64_JUMP26:
                    break;

                case R_AARCH64_ADR_PREL_PG_HI21:
                case R_AARCH64_ADD_ABS_LO12_NC:
                    fprintf(stderr, "Small memory model not supported.\n");
                    return FALSE;

                case R_AARCH64_ABS64:
                    DestinationSectionIndex = RelocationSectionHeader->sh_info;
                    FixupOffset =
                         Context->CoffSectionsOffset[DestinationSectionIndex] +
                         (Relocation->r_offset - SectionHeader->sh_addr);

                    if (!ElfconvCoffAddFixup(Context,
                                             (UINT32)FixupOffset,
                                             EFI_IMAGE_REL_BASED_DIR64)) {

                        return FALSE;
                    }

                    break;

                case R_AARCH64_ABS32:
                    DestinationSectionIndex = RelocationSectionHeader->sh_info;
                    FixupOffset =
                         Context->CoffSectionsOffset[DestinationSectionIndex] +
                         (Relocation->r_offset - SectionHeader->sh_addr);

                    if (!ElfconvCoffAddFixup(Context,
                                             (UINT32)FixupOffset,
                                             EFI_IMAGE_REL_BASED_HIGHLOW)) {

                        return FALSE;
                    }

                    break;

                default:
                    fprintf(stderr,
                            "Error: Unsupported ARM relocation type %lld.\n",
                            ELF64_R_TYPE(Relocation->r_info));

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
    // of SHT_RELA.
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
                  (Elf64_Dyn *)((UINT8 *)ElfHeader + DynamicSegment->p_offset);

            while (DynamicSection->d_tag != DT_NULL) {
                switch (DynamicSection->d_tag) {
                case DT_RELA:
                    RelocationOffset = DynamicSection->d_un.d_val;
                    if ((Context->Flags & ELFCONV_OPTION_VERBOSE) != 0) {
                        printf("Relocation offset %x.\n",
                               (int)RelocationOffset);
                    }

                    Result = ElfconvConvertElfAddress64(Context,
                                                        &RelocationOffset);

                    if (Result == FALSE) {
                        fprintf(stderr,
                                "Error: Failed to convert dynamic relocation "
                                "address %llx to destination image offset.\n",
                                RelocationOffset);

                        return FALSE;
                    }

                    if ((Context->Flags & ELFCONV_OPTION_VERBOSE) != 0) {
                        printf("Adjusted relocation offset %llx.\n",
                               RelocationOffset);
                    }

                    break;

                case DT_RELASZ:
                    RelocationSize = DynamicSection->d_un.d_val;
                    if ((Context->Flags & ELFCONV_OPTION_VERBOSE) != 0) {
                        printf("Relocation size %llx.\n",
                               (UINT64)RelocationSize);
                    }

                    break;

                case DT_RELAENT:
                    RelocationElementSize = DynamicSection->d_un.d_val;
                    if ((Context->Flags & ELFCONV_OPTION_VERBOSE) != 0) {
                        printf("Relocation element size %llx.\n",
                               (UINT64)RelocationElementSize);
                    }

                    break;

                default:
                    break;
                }

                DynamicSection += 1;
            }

            if ((Context->Flags & ELFCONV_OPTION_VERBOSE) != 0) {
                printf("Relocations: PHDR %d (p_offset %llx) Offset %llx "
                       "Size %llx ElemSize %llx\n",
                       SectionIndex,
                       DynamicSegment->p_offset,
                       RelocationOffset,
                       (UINT64)RelocationSize,
                       (UINT64)RelocationElementSize);
            }

            for (RelocationIndex = 0;
                 RelocationIndex < RelocationSize;
                 RelocationIndex += RelocationElementSize) {

                Relocation = (Elf64_Rela *)(Context->CoffFile +
                                            RelocationOffset +
                                            RelocationIndex);

                if (ElfHeader->e_machine == EM_X86_64) {
                    switch (ELF64_R_TYPE(Relocation->r_info)) {

                    //
                    // Relative relocations contain a default VA in them. First
                    // convert that VA into a COFF offset, then create a
                    // relocation for it.
                    //

                    case R_X86_64_RELATIVE:
                        TargetAddress = Relocation->r_offset;
                        Result = ElfconvConvertElfAddress64(Context,
                                                            &TargetAddress);

                        if (Result == FALSE) {
                            fprintf(stderr,
                                    "R_386_RELATIVE target convert failed.\n");

                            return FALSE;
                        }

                        TargetPointer = Context->CoffFile + TargetAddress;
                        TargetValue = *(UINT64 *)TargetPointer;
                        Result = ElfconvConvertElfAddress64(Context,
                                                            &TargetValue);

                        if (Result == FALSE) {
                            printf("Skipping relocation at address %llx that "
                                   "had value %llx that could not be "
                                   "converted in the destination.\n",
                                   TargetAddress,
                                   TargetValue);

                            break;
                        }

                        //
                        // Stick the old value (now an offset in the COFF image)
                        // into the relocation's spot.
                        //

                        *(UINT64 *)TargetPointer = TargetValue;
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
                                "Bad x86-64 dynamic relocation type %lld, offset "
                                "offset %llx, program header index %d.\n",
                                ELF64_R_TYPE(Relocation->r_info),
                                RelocationOffset,
                                SectionIndex);

                        return FALSE;
                    }

                } else if (ElfHeader->e_machine == EM_AARCH64) {

                    //
                    // Implement ARM64 when needed.
                    //

                    switch (ELF64_R_TYPE(Relocation->r_info)) {
                    default:
                        fprintf(stderr,
                                "Bad AA64 dynamic relocation type %lld, offset "
                                "%llx, program header index %d.\n",
                                ELF64_R_TYPE(Relocation->r_info),
                                RelocationOffset,
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

    Directories = &(NtHeader->Pe32Plus.OptionalHeader.DataDirectory[0]);
    Directory = &(Directories[EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC]);
    Directory->Size = Context->CoffOffset - Context->RelocationOffset;
    if (Directory->Size == 0) {
        Directory->VirtualAddress = 0;
        NtHeader->Pe32Plus.FileHeader.NumberOfSections -= 1;

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
ElfconvWriteDebug64 (
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

    Elf64_Ehdr *ElfHeader;
    Elf64_Shdr *ElfSection;
    UINT64 Flags;
    VOID *NewBuffer;
    EFI_IMAGE_OPTIONAL_HEADER_UNION *NtHeader;
    UINT64 SectionCount;
    UINTN SectionIndex;
    CHAR8 *SectionName;
    UINT64 SectionOffset;
    Elf64_Shdr *StringSection;

    ElfHeader = Context->InputFile;

    assert((ElfHeader->e_machine == EM_X86_64) ||
           (ElfHeader->e_machine == EM_AARCH64));

    //
    // Find and wrangle debug sections.
    //

    Context->DataOffset = Context->CoffOffset;
    SectionCount = 0;
    for (SectionIndex = 0;
         SectionIndex < ElfHeader->e_shnum;
         SectionIndex += 1) {

        ElfSection = ELFCONV_ELF_SECTION(ElfHeader, SectionIndex);
        if (ElfconvIsDebugSection64(ElfHeader, ElfSection) == FALSE) {
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
               sizeof(UINT64));

        memcpy(Context->CoffFile + Context->CoffOffset,
               Context->StringTable,
               Context->StringTableSize);

        NtHeader = (EFI_IMAGE_OPTIONAL_HEADER_UNION *)(Context->CoffFile +
                                                       Context->NtHeaderOffset);

        NtHeader->Pe32Plus.FileHeader.PointerToSymbolTable =
                                                           Context->CoffOffset;

        Context->CoffOffset += Context->StringTableSize;
    }

    return TRUE;
}

VOID
ElfconvSetImageSize64 (
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

    NtHeader->Pe32Plus.OptionalHeader.SizeOfImage = Context->CoffOffset;
    return;
}

VOID
ElfconvCleanUp64 (
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
ElfconvIsTextSection64 (
    Elf64_Ehdr *ElfHeader,
    Elf64_Shdr *SectionHeader
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
        (SectionHeader->sh_type != SHT_RELA)) {

        return FALSE;
    }

    if ((SectionHeader->sh_flags & (SHF_WRITE | SHF_ALLOC)) == SHF_ALLOC) {
        return TRUE;
    }

    return FALSE;
}

BOOLEAN
ElfconvIsDataSection64 (
    Elf64_Ehdr *ElfHeader,
    Elf64_Shdr *SectionHeader
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

    if (ElfconvIsHiiRsrcSection64(ElfHeader, SectionHeader) != FALSE) {
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
ElfconvIsHiiRsrcSection64 (
    Elf64_Ehdr *ElfHeader,
    Elf64_Shdr *SectionHeader
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
    Elf64_Shdr *StringSection;

    StringSection = ELFCONV_ELF_SECTION(ElfHeader, ElfHeader->e_shstrndx);
    SectionName = (CHAR8 *)ElfHeader + StringSection->sh_offset +
                  SectionHeader->sh_name;

    if (strcmp(SectionName, ELFCONV_HII_SECTION_NAME) == 0) {
        return TRUE;
    }

    return FALSE;
}

BOOLEAN
ElfconvIsDebugSection64 (
    Elf64_Ehdr *ElfHeader,
    Elf64_Shdr *SectionHeader
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
    Elf64_Shdr *StringSection;

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

BOOLEAN
ElfconvConvertElfAddress64 (
    PELFCONV_CONTEXT Context,
    UINT64 *Address
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

    Elf64_Ehdr *ElfHeader;
    Elf64_Shdr *SectionHeader;
    UINT64 SectionIndex;

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

