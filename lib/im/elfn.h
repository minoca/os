/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    elfn.h

Abstract:

    This header contains definitions from "native" type names to 32-bit or
    64-bit ELF definitions, depending on which one is selected.

Author:

    Evan Green 8-Apr-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#if defined(WANT_ELF64)

//
// Define macros
//

#define ELF_GET_RELOCATION_SYMBOL ELF64_GET_RELOCATION_SYMBOL
#define ELF_GET_RELOCATION_TYPE ELF64_GET_RELOCATION_TYPE
#define ELF_RELOCATION_INFORMATION ELF64_RELOCATION_INFORMATION

//
// Define other defines
//

#define ELF_WORD_SIZE_SHIFT ELF64_WORD_SIZE_SHIFT
#define ELF_WORD_SIZE_MASK ELF64_WORD_SIZE_MASK

#define ImageElfNative ImageElf64

//
// Define integer types
//

#define ELF_ADDR ELF64_ADDR
#define ELF_HALF ELF64_HALF
#define ELF_OFF ELF64_OFF
#define ELF_SWORD ELF64_SWORD
#define ELF_WORD ELF64_SWORD
#define ELF_XWORD ELF64_XWORD
#define ELF_SXWORD ELF64_SXWORD

#define PELF_ADDR PELF64_ADDR
#define PELF_HALF PELF64_HALF
#define PELF_OFF PELF64_OFF
#define PELF_SWORD PELF64_SWORD
#define PELF_WORD PELF64_SWORD
#define PELF_XWORD PELF64_XWORD
#define PELF_SXWORD PELF64_SXWORD

//
// Define structures
//

#define ELF_HEADER ELF64_HEADER
#define ELF_SECTION_HEADER ELF64_SECTION_HEADER
#define ELF_PROGRAM_HEADER ELF64_PROGRAM_HEADER
#define ELF_RELOCATION_ENTRY ELF64_RELOCATION_ENTRY
#define ELF_RELOCATION_ADDEND_ENTRY ELF64_RELOCATION_ADDEND_ENTRY
#define ELF_SYMBOL ELF64_SYMBOL
#define ELF_DYNAMIC_ENTRY ELF64_DYNAMIC_ENTRY

#define PELF_HEADER PELF64_HEADER
#define PELF_SECTION_HEADER PELF64_SECTION_HEADER
#define PELF_PROGRAM_HEADER PELF64_PROGRAM_HEADER
#define PELF_RELOCATION_ENTRY PELF64_RELOCATION_ENTRY
#define PELF_RELOCATION_ADDEND_ENTRY PELF64_RELOCATION_ADDEND_ENTRY
#define PELF_SYMBOL PELF64_SYMBOL
#define PELF_DYNAMIC_ENTRY PELF64_DYNAMIC_ENTRY

//
// Define functions
//

#define ImpElfOpenLibrary ImpElf64OpenLibrary
#define ImpElfGetImageSize ImpElf64GetImageSize
#define ImpElfLoadImage ImpElf64LoadImage
#define ImpElfAddImage ImpElf64AddImage
#define ImpElfUnloadImage ImpElf64UnloadImage
#define ImpElfGetHeader ImpElf64GetHeader
#define ImpElfGetSection ImpElf64GetSection
#define ImpElfLoadAllImports ImpElf64LoadAllImports
#define ImpElfRelocateImages ImpElf64RelocateImages
#define ImpElfRelocateSelf ImpElf64RelocateSelf
#define ImpElfGetSymbolByName ImpElf64GetSymbolByName
#define ImpElfGetSymbolByAddress ImpElf64GetSymbolByAddress
#define ImpElfResolvePltEntry ImpElf64ResolvePltEntry

#else

//
// Define macros
//

#define ELF_GET_RELOCATION_SYMBOL ELF32_GET_RELOCATION_SYMBOL
#define ELF_GET_RELOCATION_TYPE ELF32_GET_RELOCATION_TYPE
#define ELF_RELOCATION_INFORMATION ELF32_RELOCATION_INFORMATION

//
// Define other defines
//

#define ELF_WORD_SIZE_SHIFT ELF32_WORD_SIZE_SHIFT
#define ELF_WORD_SIZE_MASK ELF32_WORD_SIZE_MASK

#define ImageElfNative ImageElf32

//
// Define integer types
//

#define ELF_ADDR ELF32_ADDR
#define ELF_HALF ELF32_HALF
#define ELF_OFF ELF32_OFF
#define ELF_SWORD ELF32_SWORD
#define ELF_WORD ELF32_SWORD
#define ELF_XWORD ELF32_WORD
#define ELF_SXWORD ELF32_SWORD

#define PELF_ADDR PELF32_ADDR
#define PELF_HALF PELF32_HALF
#define PELF_OFF PELF32_OFF
#define PELF_SWORD PELF32_SWORD
#define PELF_WORD PELF32_SWORD
#define PELF_XWORD PELF32_WORD
#define PELF_SXWORD PELF32_SWORD

//
// Define structures
//

#define ELF_HEADER ELF32_HEADER
#define ELF_SECTION_HEADER ELF32_SECTION_HEADER
#define ELF_PROGRAM_HEADER ELF32_PROGRAM_HEADER
#define ELF_RELOCATION_ENTRY ELF32_RELOCATION_ENTRY
#define ELF_RELOCATION_ADDEND_ENTRY ELF32_RELOCATION_ADDEND_ENTRY
#define ELF_SYMBOL ELF32_SYMBOL
#define ELF_DYNAMIC_ENTRY ELF32_DYNAMIC_ENTRY

#define PELF_HEADER PELF32_HEADER
#define PELF_SECTION_HEADER PELF32_SECTION_HEADER
#define PELF_PROGRAM_HEADER PELF32_PROGRAM_HEADER
#define PELF_RELOCATION_ENTRY PELF32_RELOCATION_ENTRY
#define PELF_RELOCATION_ADDEND_ENTRY PELF32_RELOCATION_ADDEND_ENTRY
#define PELF_SYMBOL PELF32_SYMBOL
#define PELF_DYNAMIC_ENTRY PELF32_DYNAMIC_ENTRY

//
// Define functions
//

#define ImpElfOpenLibrary ImpElf32OpenLibrary
#define ImpElfGetImageSize ImpElf32GetImageSize
#define ImpElfLoadImage ImpElf32LoadImage
#define ImpElfAddImage ImpElf32AddImage
#define ImpElfUnloadImage ImpElf32UnloadImage
#define ImpElfGetHeader ImpElf32GetHeader
#define ImpElfGetSection ImpElf32GetSection
#define ImpElfLoadAllImports ImpElf32LoadAllImports
#define ImpElfRelocateImages ImpElf32RelocateImages
#define ImpElfRelocateSelf ImpElf32RelocateSelf
#define ImpElfGetSymbolByName ImpElf32GetSymbolByName
#define ImpElfGetSymbolByAddress ImpElf32GetSymbolByAddress
#define ImpElfResolvePltEntry ImpElf32ResolvePltEntry

#endif

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

