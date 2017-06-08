/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    elf.h

Abstract:

    This header contains definitions for the ELF file format.

Author:

    Evan Green 13-Oct-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// These macros manipulate the bind and type information from a symbol's
// information field.
//

#define ELF_GET_SYMBOL_BIND(_Information) ((_Information) >> 4)
#define ELF_GET_SYMBOL_TYPE(_Information) ((_Information) & 0xF)
#define ELF_SYMBOL_INFORMATION(_Bind, _Type) \
    (((_Bind) << 4) + ((_Type) & 0x0F))

//
// These macros manipulate the symbol and type fields in a relocation entry's
// information in 32-bit ELF images.
//

#define ELF32_GET_RELOCATION_SYMBOL(_Information) ((_Information) >> 8)
#define ELF32_GET_RELOCATION_TYPE(_Information) ((_Information) & 0xFF)
#define ELF32_RELOCATION_INFORMATION(_Symbol, _Type) \
    (((_Symbol) << 8) + ((_Type) & 0xFF))

//
// These macros manipulate the symbol and type fields in a relocation entry's
// information in 64-bit ELF images.
//

#define ELF64_GET_RELOCATION_SYMBOL(_Information) ((_Information) >> 32)
#define ELF64_GET_RELOCATION_TYPE(_Information) ((_Information) & 0xFFFFFFFF)
#define ELF64_RELOCATION_INFORMATION(_Symbol, _Type) \
    (((_Symbol) << 32) + ((_Type) & 0xFFFFFFFF))

//
// ---------------------------------------------------------------- Definitions
//

#define ELF32_IDENTIFICATION_LENGTH 16
#define ELF64_IDENTIFICATION_LENGTH 16

#define ELF_MAGIC0 0x7F
#define ELF_MAGIC1 'E'
#define ELF_MAGIC2 'L'
#define ELF_MAGIC3 'F'
#define ELF_MAGIC 0x464C457F

#define ELF_CLASS_OFFSET 4
#define ELF_ENDIANNESS_OFFSET 5
#define ELF_VERSION_OFFSET 6
#define ELF_OS_ABI_OFFSET 7
#define ELF_ABI_VERSION_OFFSET 8

#define ELF_IMAGE_RELOCATABLE 1
#define ELF_IMAGE_EXECUTABLE 2
#define ELF_IMAGE_SHARED_OBJECT 3
#define ELF_IMAGE_CORE 4

#define ELF_MACHINE_I386 3
#define ELF_MACHINE_ARM 40
#define ELF_MACHINE_X86_64 62
#define ELF_MACHINE_AARCH64 183

#define ELF_LITTLE_ENDIAN 0x1
#define ELF_BIG_ENDIAN 0x2

#define ELF_32BIT 0x1
#define ELF_64BIT 0x2

#define ELF_SECTION_TYPE_NULL 0
#define ELF_SECTION_TYPE_PROGRAM 1
#define ELF_SECTION_TYPE_SYMBOLS 2
#define ELF_SECTION_TYPE_STRINGS 3
#define ELF_SECTION_TYPE_RELOCATIONS_ADDENDS 4
#define ELF_SECTION_TYPE_HASH_TABLE 5
#define ELF_SECTION_TYPE_DYNAMIC_LINK 6
#define ELF_SECTION_TYPE_NOTE 7
#define ELF_SECTION_TYPE_NO_BITS 8
#define ELF_SECTION_TYPE_RELOCATIONS_NO_ADDENDS 9
#define ELF_SECTION_TYPE_SHLIB 10
#define ELF_SECTION_TYPE_DYNAMIC_SYMBOLS 11
#define ELF_SECTION_TYPE_OS_LOW 0x60000000
#define ELF_SECTION_TYPE_OS_HIGH 0x6FFFFFFF
#define ELF_SECTION_TYPE_PROCESSOR_LOW 0x70000000
#define ELF_SECTION_TYPE_PROCESSOR_HIGH 0x7FFFFFFF
#define ELF_SECTION_TYPE_USER_LOW 0x80000000
#define ELF_SECTION_TYPE_USER_HIGH 0x8FFFFFFF

#define ELF_SECTION_UNDEFINED 0
#define ELF_SECTION_RESERVED_LOW 0xFF00
#define ELF_SECTION_ABSOLUTE 0xFFF1
#define ELF_SECTION_COMMON 0xFFF2
#define ELF_SECTION_RESERVED_HIGH 0xFFFF

#define ELF_SECTION_FLAG_WRITABLE 0x1
#define ELF_SECTION_FLAG_LOAD 0x2
#define ELF_SECTION_FLAG_EXECUTABLE 0x4
#define ELF_SECTION_FLAG_OS_MASK 0x0F000000
#define ELF_SECTION_FLAG_PROCESSOR_MASK 0xF0000000

#define ELF_SEGMENT_TYPE_NULL 0
#define ELF_SEGMENT_TYPE_LOAD 1
#define ELF_SEGMENT_TYPE_DYNAMIC 2
#define ELF_SEGMENT_TYPE_INTERPRETER 3
#define ELF_SEGMENT_TYPE_NOTE 4
#define ELF_SEGMENT_TYPE_SHLIB 5
#define ELF_SEGMENT_TYPE_PROGRAM_HEADER 6
#define ELF_SEGMENT_TYPE_TLS 7
#define ELF_SEGMENT_PROCESSOR_LOW 0x70000000
#define ELF_SEGMENT_PROCESSOR_HIGH 0x7FFFFFFF

#define ELF_PROGRAM_HEADER_FLAG_EXECUTE 0x00000001
#define ELF_PROGRAM_HEADER_FLAG_WRITE 0x00000002
#define ELF_PROGRAM_HEADER_FLAG_READ 0x00000004

#define ELF_DYNAMIC_NULL 0
#define ELF_DYNAMIC_NEEDED 1
#define ELF_DYNAMIC_PLT_REL_SIZE 2
#define ELF_DYNAMIC_PLT_GOT 3
#define ELF_DYNAMIC_HASH_TABLE 4
#define ELF_DYNAMIC_STRING_TABLE 5
#define ELF_DYNAMIC_SYMBOL_TABLE 6
#define ELF_DYNAMIC_RELA_TABLE 7
#define ELF_DYNAMIC_RELA_TABLE_SIZE 8
#define ELF_DYNAMIC_RELA_ENTRY_SIZE 9
#define ELF_DYNAMIC_STRING_TABLE_SIZE 10
#define ELF_DYNAMIC_SYMBOL_ENTRY_SIZE 11
#define ELF_DYNAMIC_INIT 12
#define ELF_DYNAMIC_FINI 13
#define ELF_DYNAMIC_LIBRARY_NAME 14
#define ELF_DYNAMIC_RPATH 15
#define ELF_DYNAMIC_SYMBOLIC 16
#define ELF_DYNAMIC_REL_TABLE 17
#define ELF_DYNAMIC_REL_TABLE_SIZE 18
#define ELF_DYNAMIC_REL_ENTRY_SIZE 19
#define ELF_DYNAMIC_PLT_RELOCATION_TYPE 20
#define ELF_DYNAMIC_DEBUG 21
#define ELF_DYNAMIC_TEXT_RELOCATIONS 22
#define ELF_DYNAMIC_JUMP_RELOCATIONS 23
#define ELF_DYNAMIC_BIND_NOW 24
#define ELF_DYNAMIC_INIT_ARRAY 25
#define ELF_DYNAMIC_FINI_ARRAY 26
#define ELF_DYNAMIC_INIT_ARRAY_SIZE 27
#define ELF_DYNAMIC_FINI_ARRAY_SIZE 28
#define ELF_DYNAMIC_RUN_PATH 29
#define ELF_DYNAMIC_FLAGS 30
#define ELF_DYNAMIC_PREINIT_ARRAY 32
#define ELF_DYNAMIC_PREINIT_ARRAY_SIZE 33
#define ELF_DYNAMIC_GNU_HASH_TABLE 0x6FFFFEF5
#define ELF_DYNAMIC_PROCESSOR_LOW 0x70000000
#define ELF_DYNAMIC_PROCESSOR_HIGH 0x7FFFFFFF

#define ELF32_WORD_SIZE_SHIFT 5
#define ELF32_WORD_SIZE_MASK ((1 << ELF32_WORD_SIZE_SHIFT) - 1)

#define ELF64_WORD_SIZE_SHIFT 6
#define ELF64_WORD_SIZE_MASK ((1 << ELF64_WORD_SIZE_SHIFT) - 1)

//
// Define ELF dynamic flags.
//

//
// This flag indicates that the $ORIGIN subsitution string may be used.
//

#define ELF_DYNAMIC_FLAG_ORIGIN 0x01

//
// This flag indicates that symbol searches should start from the image itself,
// then start from the executable if the symbol was not found in this image.
//

#define ELF_DYNAMIC_FLAG_SYMBOLIC 0x02

//
// This flag indicates that one or more relocations might require modifying a
// non-writable segment.
//

#define ELF_DYNAMIC_FLAG_TEXT_RELOCATIONS 0x04

//
// This flag indicates that the dynamic linker should process all relocations
// for this object before transferring control to the program. The presence of
// this flag takes precedence over the lazy flag passed to the dynamic load
// routine.
//

#define ELF_DYNAMIC_FLAG_BIND_NOW 0x08

//
// This flag indicates that the image uses the static TLS model, and attempts
// to load this file dynamically should be blocked.
//

#define ELF_DYNAMIC_FLAG_STATIC_TLS 0x10

//
// ------------------------------------------------------ Data Type Definitions
//

typedef ULONG ELF32_ADDR, *PELF32_ADDR;
typedef USHORT ELF32_HALF, *PELF32_HALF;
typedef ULONG ELF32_OFF, *PELF32_OFF;
typedef LONG ELF32_SWORD, *PELF32_SWORD;
typedef ULONG ELF32_WORD, *PELF32_WORD;

typedef ULONGLONG ELF64_ADDR, *PELF64_ADDR;
typedef ULONGLONG ELF64_OFF, *PELF64_OFF;
typedef USHORT ELF64_HALF, *PELF64_HALF;
typedef ULONG ELF64_WORD, *PELF64_WORD;
typedef LONG ELF64_SWORD, *PELF64_SWORD;
typedef ULONGLONG ELF64_XWORD, *PELF64_XWORD;
typedef LONGLONG ELF64_SXWORD, *PELF64_SXWORD;

typedef enum _ELF_SYMBOL_BIND_TYPE {
    ElfBindLocal = 0,
    ElfBindGlobal = 1,
    ElfBindWeak = 2,
} ELF_SYMBOL_BIND_TYPE, *PELF_SYMBOL_BIND_TYPE;

typedef enum _ELF_SYMBOL_TYPE {
    ElfSymbolNone = 0,
    ElfSymbolObject = 1,
    ElfSymbolFunction = 2,
    ElfSymbolSection = 3,
    ElfSymbolFile = 4,
    ElfSymbolCommon = 5,
    ElfSymbolTls = 6,
} ELF_SYMBOL_TYPE, *PELF_SYMBOL_TYPE;

typedef enum _ELF_386_RELOCATION_TYPE {
    Elf386RelocationNone        = 0,
    Elf386Relocation32          = 1,
    Elf386RelocationPc32        = 2,
    Elf386RelocationGot32       = 3,
    Elf386RelocationPlt32       = 4,
    Elf386RelocationCopy        = 5,
    Elf386RelocationGlobalData  = 6,
    Elf386RelocationJumpSlot    = 7,
    Elf386RelocationRelative    = 8,
    Elf386RelocationGotOffset   = 9,
    Elf386RelocationGotPc       = 10,
    Elf386RelocationTlsTpOff    = 14,
    Elf386RelocationTlsDtpMod32 = 35,
    Elf386RelocationTlsDtpOff32 = 36,
    Elf386RelocationTlsTpOff32  = 37,
} ELF_386_RELOCATION_TYPE, *PELF_386_RELOCATION_TYPE;

typedef enum _ELF_X86_64_RELOCATION_TYPE {
    ElfX64RelocationNone = 0,
    ElfX64Relocation64 = 1,
    ElfX64RelocationPc32 = 2,
    ElfX64RelocationGot32 = 3,
    ElfX64RelocationPlt32 = 4,
    ElfX64RelocationCopy = 5,
    ElfX64RelocationGlobalData = 6,
    ElfX64RelocationJumpSlot = 7,
    ElfX64RelocationRelative = 8,
    ElfX64RelocationGotPcRelative = 9,
    ElfX64Relocation32 = 10,
    ElfX64Relocation32S = 11,
    ElfX64Relocation16 = 12,
    ElfX64RelocationPc16 = 13,
    ElfX64Relocation8 = 14,
    ElfX64RelocationPc8 = 15,
    ElfX64RelocationDtpMod64 = 16,
    ElfX64RelocationDtpOff64 = 17,
    ElfX64RelocationTpOff64 = 18,
    ElfX64RelocationTlsGd = 19,
    ElfX64RelocationTlsLd = 20,
    ElfX64RelocationDtpOff32 = 21,
    ElfX64RelocationGotTpOff = 22,
    ElfX64RelocationTpOff32 = 23,
    ElfX64RelocationPc64 = 24,
    ElfX64RelocationGotOff64 = 25,
    ElfX64RelocationGotPc32 = 26,
    ElfX64RelocationSize32 = 32,
    ElfX64RelocationSize64 = 33,
    ElfX64RelocationGotPc32TlsDesc = 34,
    ElfX64RelocationTlsDescCall = 35,
    ElfX64RelocationTlsDesc = 36,
    ElfX64RelocationIRelative = 37,
    ElfX64RelocationRelative64 = 38,
} ELF_X86_64_RELOCATION_TYPE, *PELF_X86_64_RELOCATION_TYPE;

typedef enum _ELF_ARM_RELOCATION_TYPE {
    ElfArmRelocationNone        = 0,
    ElfArmRelocationAbsolute32  = 2,
    ElfArmRelocationTlsDtpMod32 = 17,
    ElfArmRelocationTlsDtpOff32 = 18,
    ElfArmRelocationTlsTpOff32  = 19,
    ElfArmRelocationCopy        = 20,
    ElfArmRelocationGlobalData  = 21,
    ElfArmRelocationJumpSlot    = 22,
    ElfArmRelocationRelative    = 23,
    ElfArmRelocationGotOffset   = 24,
    ElfArmRelocationGotPc       = 25,
    ElfArmRelocationGot32       = 26,
    ElfArmRelocationPlt32       = 27
} ELF_ARM_RELOCATION_TYPE, *PELF_ARM_RELOCATION_TYPE;

//
// 32-bit structures
//

/*++

Structure Description:

    This structure stores the main file header for an ELF image. It is located
    at offset 0 in the file and stores the locations of other ELF headers.

Members:

    Identification - Stores a magic number identifying the file as an ELF file,
        as well as several other pieces of information such as the file version,
        ELF version, endianness, etc.

    ImageType - Stores the type of file this is (relocatable, executable,
        shared object, etc.)

    Machine - Stores the machine architecture of the code in this image.

    Version - Stores the version of the ELF format.

    EntryPoint - Stores the virtual address where the system should transfer
        control to once the image is loaded.

    ProgramHeaderOffset - Stores the offset within the file to the first
        program header.

    SectionHeaderOffset - Stores the offset within the file to the first
        section header.

    Flags - Stores processor-specific flags associated with the file.

    ElfHeaderSize - Stores the size of this header, in bytes.

    ProgramHeaderSize - Stores the size of one program header, in bytes.

    ProgramHeaderCount - Stores the number of program headers in the file.

    SectionHeaderSize - Stores the size of one section header, in bytes.

    SectionHeaderCount - Stores the number of section headers in the file.

    StringSectionIndex - Stores the section header table index of the entry
        associated with the section name string table.

--*/

typedef struct _ELF32_HEADER {
    UCHAR Identification[ELF32_IDENTIFICATION_LENGTH];
    ELF32_HALF ImageType;
    ELF32_HALF Machine;
    ELF32_WORD Version;
    ELF32_ADDR EntryPoint;
    ELF32_OFF ProgramHeaderOffset;
    ELF32_OFF SectionHeaderOffset;
    ELF32_WORD Flags;
    ELF32_HALF ElfHeaderSize;
    ELF32_HALF ProgramHeaderSize;
    ELF32_HALF ProgramHeaderCount;
    ELF32_HALF SectionHeaderSize;
    ELF32_HALF SectionHeaderCount;
    ELF32_HALF StringSectionIndex;
} PACKED ELF32_HEADER, *PELF32_HEADER;

/*++

Structure Description:

    This structure stores information about a section of data in an ELF32 file.

Members:

    NameOffset - Stores the index into the string table section, giving the
        location of a null-terminated string of the name of the section.

    Type - Stores a description of the section's contents and semantics. See
        the ELF_SECTION_TYPE_* definitions.

    Flags - Stores miscellaneous attributes of the section. See the
        ELF_SECTION_FLAG_* definitions.

    VirtualAddress - Stores the virtual address of the beginning of this
        section.

    Offset - Stores the byte offset from the beginning of the file to the first
        byte in the section. If the section type is NOBITS, and this field
        locates the conceptual placement in the file.

    Size - Stores the size of the section in bytes both in memory an on disk
        (unless the type is NOBITS, in which case the file contains no bytes).

    Link - Stores a section header table index link, whose interpretation
        depends on the section type.

    Information - Stores extra information, whose interpretation depends on the
        section type.

    Alignment - Stores the alignment constraints of the section. Only 0 and
        positive powers of 2 are supported. Values of 0 or 1 indicate no
        alignment constraints.

    EntrySize - Stores the size of one entry, if the section holds a table of
        fixed size entries, such as a symbol table. This field contains 0 if
        the section does not hold a table of fixed size entries.

--*/

typedef struct _ELF32_SECTION_HEADER {
    ELF32_WORD NameOffset;
    ELF32_WORD SectionType;
    ELF32_WORD Flags;
    ELF32_ADDR VirtualAddress;
    ELF32_OFF Offset;
    ELF32_WORD Size;
    ELF32_WORD Link;
    ELF32_WORD Information;
    ELF32_WORD Alignment;
    ELF32_WORD EntrySize;
} PACKED ELF32_SECTION_HEADER, *PELF32_SECTION_HEADER;

/*++

Structure Description:

    This structure stores information about a program segment, used to load
    ELF32 files. Segments are distinct from sections. One segment may contain
    multiple sections.

Members:

    Type - Stores what type of segment this array element describes. See
        ELF_SEGMENT_TYPE_* definitions.

    Offset - Stores the offset from the beginning of the file, in bytes, at
        which the first byte of the segment resides.

    VirtualAddress - Stores the virtual address at which the first byte of the
        segment resides in memory.

    PhysicalAddress - Stores the physical address at which the segment resides.
        This field is almost always the same as the virtual address.

    FileSize - Stores the number of bytes in the file image of the segment. It
        may be zero.

    MemorySize - Stores the number of bytes in the memory image of the segment.
        It may be zero.

    Flags - Stores flags regarding this segment. See ELF_PROGRAM_HEADER_FLAG_*
        definitions.

    Alignment - Stores the power of two alignment requirement for the segment.
        Values of 0 and 1 mean that no alignment is required.

--*/

typedef struct _ELF32_PROGRAM_HEADER {
    ELF32_WORD Type;
    ELF32_OFF Offset;
    ELF32_ADDR VirtualAddress;
    ELF32_ADDR PhysicalAddress;
    ELF32_WORD FileSize;
    ELF32_WORD MemorySize;
    ELF32_WORD Flags;
    ELF32_WORD Alignment;
} PACKED ELF32_PROGRAM_HEADER, *PELF32_PROGRAM_HEADER;

/*++

Structure Description:

    This structure stores information about a relocation entry.

Members:

    Offset - Stores the location at which to apply the relocation action. For
        an executable or shared file object, the value is the virtual address
        of the storage unit affected by the relocation.

    Information - Stores both the symbol table index with respect to which
        relocation must be made, and the type of relocation to apply.

--*/

typedef struct _ELF32_RELOCATION_ENTRY {
    ELF32_ADDR Offset;
    ELF32_WORD Information;
} PACKED ELF32_RELOCATION_ENTRY, *PELF32_RELOCATION_ENTRY;

/*++

Structure Description:

    This structure stores information about a relocation entry with an addend.

Members:

    Offset - Stores the location at which to apply the relocation action. For
        an executable or shared file object, the value is the virtual address
        of the storage unit affected by the relocation.

    Information - Stores both the symbol table index with respect to which
        relocation must be made, and the type of relocation to apply.

    Addend - Stores a constant addend used to compute the value to be stored in
        the relocatable field.

--*/

typedef struct _ELF32_RELOCATION_ADDEND_ENTRY {
    ELF32_ADDR Offset;
    ELF32_WORD Information;
    ELF32_SWORD Addend;
} PACKED ELF32_RELOCATION_ADDEND_ENTRY, *PELF32_RELOCATION_ADDEND_ENTRY;

/*++

Structure Description:

    This structure stores a symbol entry in the ELF format.

Members:

    NameOffset - Stores an offset into the string table where the name of the
        symbol is stored.

    Value - Stores the value or address of the symbol.

    Size - Stores a size associated with the symbol. For data symbols, this
        store the size of the type. This member holds 0 if the symbol is an
        unknown size.

    Information - Stores the symbols type and binding attributes.

    Other - Stores 0 and has no defined meaning.

    SectionIndex - Stores the section index of the section related to this
        symbol. All symbols are defined in the context of a section.

--*/

typedef struct _ELF32_SYMBOL {
    ELF32_WORD NameOffset;
    ELF32_ADDR Value;
    ELF32_WORD Size;
    UCHAR Information;
    UCHAR Other;
    ELF32_HALF SectionIndex;
} PACKED ELF32_SYMBOL, *PELF32_SYMBOL;

/*++

Structure Description:

    This structure stores a single entry located in a dynamic section

Members:

    Tag - Stores the type of entry and the interpretation of the Value. See
        ELF_DYNAMIC_* definitions.

    Value - Stores the entry value, whose meaning varies with the type.

--*/

typedef struct _ELF32_DYNAMIC_ENTRY {
    ELF32_SWORD Tag;
    ELF32_WORD Value;
} PACKED ELF32_DYNAMIC_ENTRY, *PELF32_DYNAMIC_ENTRY;

//
// 64-bit structures
//

/*++

Structure Description:

    This structure stores the main file header for an ELF image. It is located
    at offset 0 in the file and stores the locations of other ELF headers.

Members:

    Identification - Stores a magic number identifying the file as an ELF file,
        as well as several other pieces of information such as the file version,
        ELF version, endianness, etc.

    ImageType - Stores the type of file this is (relocatable, executable,
        shared object, etc.)

    Machine - Stores the machine architecture of the code in this image.

    Version - Stores the version of the ELF format.

    EntryPoint - Stores the virtual address where the system should transfer
        control to once the image is loaded.

    ProgramHeaderOffset - Stores the offset within the file to the first
        program header.

    SectionHeaderOffset - Stores the offset within the file to the first
        section header.

    Flags - Stores processor-specific flags associated with the file.

    ElfHeaderSize - Stores the size of this header, in bytes.

    ProgramHeaderSize - Stores the size of one program header, in bytes.

    ProgramHeaderCount - Stores the number of program headers in the file.

    SectionHeaderSize - Stores the size of one section header, in bytes.

    SectionHeaderCount - Stores the number of section headers in the file.

    StringSectionIndex - Stores the section header table index of the entry
        associated with the section name string table.

--*/

typedef struct _ELF64_HEADER {
    UCHAR Identification[ELF64_IDENTIFICATION_LENGTH];
    ELF64_HALF ImageType;
    ELF64_HALF Machine;
    ELF64_WORD Version;
    ELF64_ADDR EntryPoint;
    ELF64_OFF ProgramHeaderOffset;
    ELF64_OFF SectionHeaderOffset;
    ELF64_WORD Flags;
    ELF64_HALF ElfHeaderSize;
    ELF64_HALF ProgramHeaderSize;
    ELF64_HALF ProgramHeaderCount;
    ELF64_HALF SectionHeaderSize;
    ELF64_HALF SectionHeaderCount;
    ELF64_HALF StringSectionIndex;
} PACKED ELF64_HEADER, *PELF64_HEADER;

/*++

Structure Description:

    This structure stores information about a section of data in an ELF32 file.

Members:

    NameOffset - Stores the index into the string table section, giving the
        location of a null-terminated string of the name of the section.

    Type - Stores a description of the section's contents and semantics. See
        the ELF_SECTION_TYPE_* definitions.

    Flags - Stores miscellaneous attributes of the section. See the
        ELF_SECTION_FLAG_* definitions.

    VirtualAddress - Stores the virtual address of the beginning of this
        section.

    Offset - Stores the byte offset from the beginning of the file to the first
        byte in the section. If the section type is NOBITS, and this field
        locates the conceptual placement in the file.

    Size - Stores the size of the section in bytes both in memory an on disk
        (unless the type is NOBITS, in which case the file contains no bytes).

    Link - Stores a section header table index link, whose interpretation
        depends on the section type.

    Information - Stores extra information, whose interpretation depends on the
        section type.

    Alignment - Stores the alignment constraints of the section. Only 0 and
        positive powers of 2 are supported. Values of 0 or 1 indicate no
        alignment constraints.

    EntrySize - Stores the size of one entry, if the section holds a table of
        fixed size entries, such as a symbol table. This field contains 0 if
        the section does not hold a table of fixed size entries.

--*/

typedef struct _ELF64_SECTION_HEADER {
    ELF64_WORD NameOffset;
    ELF64_WORD SectionType;
    ELF64_XWORD Flags;
    ELF64_ADDR VirtualAddress;
    ELF64_OFF Offset;
    ELF64_XWORD Size;
    ELF64_WORD Link;
    ELF64_WORD Information;
    ELF64_XWORD Alignment;
    ELF64_XWORD EntrySize;
} PACKED ELF64_SECTION_HEADER, *PELF64_SECTION_HEADER;

/*++

Structure Description:

    This structure stores information about a program segment, used to load
    ELF64 files. Segments are distinct from sections. One segment may contain
    multiple sections.

Members:

    Type - Stores what type of segment this array element describes. See
        ELF_SEGMENT_TYPE_* definitions.

    Flags - Stores flags regarding this segment. See ELF_PROGRAM_HEADER_FLAG_*
        definitions.

    Offset - Stores the offset from the beginning of the file, in bytes, at
        which the first byte of the segment resides.

    VirtualAddress - Stores the virtual address at which the first byte of the
        segment resides in memory.

    PhysicalAddress - Stores the physical address at which the segment resides.
        This field is almost always the same as the virtual address.

    FileSize - Stores the number of bytes in the file image of the segment. It
        may be zero.

    MemorySize - Stores the number of bytes in the memory image of the segment.
        It may be zero.

    Alignment - Stores the power of two alignment requirement for the segment.
        Values of 0 and 1 mean that no alignment is required.

--*/

typedef struct _ELF64_PROGRAM_HEADER {
    ELF64_WORD Type;
    ELF64_WORD Flags;
    ELF64_OFF Offset;
    ELF64_ADDR VirtualAddress;
    ELF64_ADDR PhysicalAddress;
    ELF64_XWORD FileSize;
    ELF64_XWORD MemorySize;
    ELF64_XWORD Alignment;
} PACKED ELF64_PROGRAM_HEADER, *PELF64_PROGRAM_HEADER;

/*++

Structure Description:

    This structure stores information about a relocation entry.

Members:

    Offset - Stores the location at which to apply the relocation action. For
        an executable or shared file object, the value is the virtual address
        of the storage unit affected by the relocation.

    Information - Stores both the symbol table index with respect to which
        relocation must be made, and the type of relocation to apply.

--*/

typedef struct _ELF64_RELOCATION_ENTRY {
    ELF64_ADDR Offset;
    ELF64_XWORD Information;
} PACKED ELF64_RELOCATION_ENTRY, *PELF64_RELOCATION_ENTRY;

/*++

Structure Description:

    This structure stores information about a relocation entry with an addend.

Members:

    Offset - Stores the location at which to apply the relocation action. For
        an executable or shared file object, the value is the virtual address
        of the storage unit affected by the relocation.

    Information - Stores both the symbol table index with respect to which
        relocation must be made, and the type of relocation to apply.

    Addend - Stores a constant addend used to compute the value to be stored in
        the relocatable field.

--*/

typedef struct _ELF64_RELOCATION_ADDEND_ENTRY {
    ELF64_ADDR Offset;
    ELF64_XWORD Information;
    ELF64_SXWORD Addend;
} PACKED ELF64_RELOCATION_ADDEND_ENTRY, *PELF64_RELOCATION_ADDEND_ENTRY;

/*++

Structure Description:

    This structure stores a symbol entry in the ELF format.

Members:

    NameOffset - Stores an offset into the string table where the name of the
        symbol is stored.

    Information - Stores the symbols type and binding attributes.

    Other - Stores 0 and has no defined meaning.

    SectionIndex - Stores the section index of the section related to this
        symbol. All symbols are defined in the context of a section.

    Value - Stores the value or address of the symbol.

    Size - Stores a size associated with the symbol. For data symbols, this
        store the size of the type. This member holds 0 if the symbol is an
        unknown size.

--*/

typedef struct _ELF64_SYMBOL {
    ELF64_WORD NameOffset;
    UCHAR Information;
    UCHAR Other;
    ELF64_HALF SectionIndex;
    ELF64_ADDR Value;
    ELF64_XWORD Size;
} PACKED ELF64_SYMBOL, *PELF64_SYMBOL;

/*++

Structure Description:

    This structure stores a single entry located in a dynamic section

Members:

    Tag - Stores the type of entry and the interpretation of the Value. See
        ELF_DYNAMIC_* definitions.

    Value - Stores the entry value, whose meaning varies with the type.

--*/

typedef struct _ELF64_DYNAMIC_ENTRY {
    ELF64_SXWORD Tag;
    ELF64_XWORD Value;
} PACKED ELF64_DYNAMIC_ENTRY, *PELF64_DYNAMIC_ENTRY;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

//
// 32-bit ELF functions.
//

KSTATUS
ImpElf32OpenLibrary (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Parent,
    PCSTR LibraryName,
    PIMAGE_FILE_INFORMATION File,
    PSTR *Path
    );

/*++

Routine Description:

    This routine attempts to open a dynamic library.

Arguments:

    ListHead - Supplies an optional pointer to the head of the list of loaded
        images.

    Parent - Supplies a pointer to the parent image requiring this image for
        load.

    LibraryName - Supplies the name of the library to open.

    File - Supplies a pointer where the information for the file including its
        open handle will be returned.

    Path - Supplies a pointer where the real path to the opened file will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    Status code.

--*/

KSTATUS
ImpElf32GetImageSize (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    PIMAGE_BUFFER Buffer,
    PSTR *InterpreterPath
    );

/*++

Routine Description:

    This routine determines the size of an ELF executable image. The image size,
    preferred lowest address, and relocatable flag will all be filled in.

Arguments:

    ListHead - Supplies a pointer to the head of the list of loaded images.

    Image - Supplies a pointer to the image to get the size of.

    Buffer - Supplies a pointer to the loaded image buffer.

    InterpreterPath - Supplies a pointer where the interpreter name will be
        returned if the program is requesting an interpreter.

Return Value:

    Returns the size of the expanded image in memory on success.

    0 on failure.

--*/

KSTATUS
ImpElf32LoadImage (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    PIMAGE_BUFFER Buffer
    );

/*++

Routine Description:

    This routine loads an ELF image into its executable form.

Arguments:

    ListHead - Supplies a pointer to the head of the list of loaded images.

    Image - Supplies a pointer to the loaded image. This must be partially
        filled out. Notable fields that must be filled out by the caller
        include the loaded virtual address and image size. This routine will
        fill out many other fields.

    Buffer - Supplies a pointer to the image buffer.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_FILE_CORRUPT if the file headers were corrupt or unexpected.

    Other errors on failure.

--*/

KSTATUS
ImpElf32AddImage (
    PIMAGE_BUFFER ImageBuffer,
    PLOADED_IMAGE Image
    );

/*++

Routine Description:

    This routine adds the accounting structures for an image that has already
    been loaded into memory.

Arguments:

    ImageBuffer - Supplies a pointer to the loaded image buffer.

    Image - Supplies a pointer to the image to initialize.

Return Value:

    Status code.

--*/

VOID
ImpElf32UnloadImage (
    PLOADED_IMAGE Image
    );

/*++

Routine Description:

    This routine unloads an ELF executable.

Arguments:

    Image - Supplies a pointer to the loaded image.

Return Value:

    None.

--*/

BOOL
ImpElf32GetHeader (
    PIMAGE_BUFFER Buffer,
    PELF32_HEADER *ElfHeader
    );

/*++

Routine Description:

    This routine returns a pointer to the ELF image header given a buffer
    containing the executable image mapped in memory.

Arguments:

    Buffer - Supplies a pointer to the loaded image buffer.

    ElfHeader - Supplies a pointer where the location of the ELF header will
        be returned.

Return Value:

    TRUE on success.

    FALSE otherwise.

--*/

BOOL
ImpElf32GetSection (
    PIMAGE_BUFFER Buffer,
    PSTR SectionName,
    PVOID *Section,
    PULONGLONG VirtualAddress,
    PULONG SectionSizeInFile,
    PULONG SectionSizeInMemory
    );

/*++

Routine Description:

    This routine gets a pointer to the given section in an ELF image given a
    memory mapped file.

Arguments:

    Buffer - Supplies a pointer to the image buffer.

    SectionName - Supplies the name of the desired section.

    Section - Supplies a pointer where the pointer to the section will be
        returned.

    VirtualAddress - Supplies a pointer where the virtual address of the section
        will be returned, if applicable.

    SectionSizeInFile - Supplies a pointer where the size of the section as it
        appears in the file will be returned.

    SectionSizeInMemory - Supplies a pointer where the size of the section as it
        appears after being loaded in memory will be returned.

Return Value:

    TRUE on success.

    FALSE otherwise.

--*/

KSTATUS
ImpElf32LoadAllImports (
    PLIST_ENTRY ListHead
    );

/*++

Routine Description:

    This routine loads all import libraries for all images.

Arguments:

    ListHead - Supplies a pointer to the head of the list of loaded images.

Return Value:

    Status code.

--*/

KSTATUS
ImpElf32RelocateImages (
    PLIST_ENTRY ListHead
    );

/*++

Routine Description:

    This routine relocates all images on the given image list that have not
    yet been relocated.

Arguments:

    ListHead - Supplies a pointer to the head of the list to relocate.

Return Value:

    Status code.

--*/

VOID
ImpElf32RelocateSelf (
    PIMAGE_BUFFER Buffer,
    PIM_RESOLVE_PLT_ENTRY PltResolver,
    PLOADED_IMAGE Image
    );

/*++

Routine Description:

    This routine relocates the currently running image.

Arguments:

    Buffer - Supplies a pointer to the image buffer.

    PltResolver - Supplies a pointer to the function used to resolve PLT
        entries.

    Image - Supplies a pointer to the zeroed but otherwise uninitialized
        image buffer.

Return Value:

    None.

--*/

KSTATUS
ImpElf32GetSymbolByName (
    PLOADED_IMAGE Image,
    PSTR SymbolName,
    PLOADED_IMAGE Skip,
    PIMAGE_SYMBOL Symbol
    );

/*++

Routine Description:

    This routine attempts to find an exported symbol with the given name in the
    given binary.

Arguments:

    Image - Supplies a pointer to the image to query.

    SymbolName - Supplies a pointer to the string containing the name of the
        symbol to search for.

    Skip - Supplies an optional pointer to an image to skip when searching.

    Symbol - Supplies a pointer to a structure that receives the symbol's
        information on success.

Return Value:

    Status code.

--*/

KSTATUS
ImpElf32GetSymbolByAddress (
    PLOADED_IMAGE Image,
    PVOID Address,
    PIMAGE_SYMBOL Symbol
    );

/*++

Routine Description:

    This routine attempts to find the given address in the given image and
    resolve it to a symbol.

Arguments:

    Image - Supplies a pointer to the image to query.

    Address - Supplies the address to search for.

    Symbol - Supplies a pointer to a structure that receives the address's
        symbol information on success.

Return Value:

    Status code.

--*/

PVOID
ImpElf32ResolvePltEntry (
    PLOADED_IMAGE Image,
    ULONG RelocationOffset
    );

/*++

Routine Description:

    This routine implements the slow path for a Procedure Linkable Table entry
    that has not yet been resolved to its target function address. This routine
    is only called once for each PLT entry, as subsequent calls jump directly
    to the destination function address. It resolves the appropriate GOT
    relocation and returns a pointer to the function to jump to.

Arguments:

    Image - Supplies a pointer to the loaded image whose PLT needs resolution.
        This is really whatever pointer is in GOT + 4.

    RelocationOffset - Supplies the byte offset from the start of the
        relocation section where the relocation for this PLT entry resides, or
        the PLT index, depending on the architecture.

Return Value:

    Returns a pointer to the function to jump to (in addition to writing that
    address in the GOT at the appropriate spot).

--*/

//
// 64-bit ELF functions. These are exactly the same as the 32-bit functions.
//

KSTATUS
ImpElf64OpenLibrary (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Parent,
    PCSTR LibraryName,
    PIMAGE_FILE_INFORMATION File,
    PSTR *Path
    );

/*++

Routine Description:

    This routine attempts to open a dynamic library.

Arguments:

    ListHead - Supplies an optional pointer to the head of the list of loaded
        images.

    Parent - Supplies a pointer to the parent image requiring this image for
        load.

    LibraryName - Supplies the name of the library to open.

    File - Supplies a pointer where the information for the file including its
        open handle will be returned.

    Path - Supplies a pointer where the real path to the opened file will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    Status code.

--*/

KSTATUS
ImpElf64GetImageSize (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    PIMAGE_BUFFER Buffer,
    PSTR *InterpreterPath
    );

/*++

Routine Description:

    This routine determines the size of an ELF executable image. The image size,
    preferred lowest address, and relocatable flag will all be filled in.

Arguments:

    ListHead - Supplies a pointer to the head of the list of loaded images.

    Image - Supplies a pointer to the image to get the size of.

    Buffer - Supplies a pointer to the loaded image buffer.

    InterpreterPath - Supplies a pointer where the interpreter name will be
        returned if the program is requesting an interpreter.

Return Value:

    Returns the size of the expanded image in memory on success.

    0 on failure.

--*/

KSTATUS
ImpElf64LoadImage (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    PIMAGE_BUFFER Buffer
    );

/*++

Routine Description:

    This routine loads an ELF image into its executable form.

Arguments:

    ListHead - Supplies a pointer to the head of the list of loaded images.

    Image - Supplies a pointer to the loaded image. This must be partially
        filled out. Notable fields that must be filled out by the caller
        include the loaded virtual address and image size. This routine will
        fill out many other fields.

    Buffer - Supplies a pointer to the image buffer.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_FILE_CORRUPT if the file headers were corrupt or unexpected.

    Other errors on failure.

--*/

KSTATUS
ImpElf64AddImage (
    PIMAGE_BUFFER ImageBuffer,
    PLOADED_IMAGE Image
    );

/*++

Routine Description:

    This routine adds the accounting structures for an image that has already
    been loaded into memory.

Arguments:

    ImageBuffer - Supplies a pointer to the loaded image buffer.

    Image - Supplies a pointer to the image to initialize.

Return Value:

    Status code.

--*/

VOID
ImpElf64UnloadImage (
    PLOADED_IMAGE Image
    );

/*++

Routine Description:

    This routine unloads an ELF executable.

Arguments:

    Image - Supplies a pointer to the loaded image.

Return Value:

    None.

--*/

BOOL
ImpElf64GetHeader (
    PIMAGE_BUFFER Buffer,
    PELF64_HEADER *ElfHeader
    );

/*++

Routine Description:

    This routine returns a pointer to the ELF image header given a buffer
    containing the executable image mapped in memory.

Arguments:

    Buffer - Supplies a pointer to the loaded image buffer.

    ElfHeader - Supplies a pointer where the location of the ELF header will
        be returned.

Return Value:

    TRUE on success.

    FALSE otherwise.

--*/

BOOL
ImpElf64GetSection (
    PIMAGE_BUFFER Buffer,
    PSTR SectionName,
    PVOID *Section,
    PULONGLONG VirtualAddress,
    PULONG SectionSizeInFile,
    PULONG SectionSizeInMemory
    );

/*++

Routine Description:

    This routine gets a pointer to the given section in an ELF image given a
    memory mapped file.

Arguments:

    Buffer - Supplies a pointer to the image buffer.

    SectionName - Supplies the name of the desired section.

    Section - Supplies a pointer where the pointer to the section will be
        returned.

    VirtualAddress - Supplies a pointer where the virtual address of the section
        will be returned, if applicable.

    SectionSizeInFile - Supplies a pointer where the size of the section as it
        appears in the file will be returned.

    SectionSizeInMemory - Supplies a pointer where the size of the section as it
        appears after being loaded in memory will be returned.

Return Value:

    TRUE on success.

    FALSE otherwise.

--*/

KSTATUS
ImpElf64LoadAllImports (
    PLIST_ENTRY ListHead
    );

/*++

Routine Description:

    This routine loads all import libraries for all images.

Arguments:

    ListHead - Supplies a pointer to the head of the list of loaded images.

Return Value:

    Status code.

--*/

KSTATUS
ImpElf64RelocateImages (
    PLIST_ENTRY ListHead
    );

/*++

Routine Description:

    This routine relocates all images on the given image list that have not
    yet been relocated.

Arguments:

    ListHead - Supplies a pointer to the head of the list to relocate.

Return Value:

    Status code.

--*/

VOID
ImpElf64RelocateSelf (
    PIMAGE_BUFFER Buffer,
    PIM_RESOLVE_PLT_ENTRY PltResolver,
    PLOADED_IMAGE Image
    );

/*++

Routine Description:

    This routine relocates the currently running image.

Arguments:

    Buffer - Supplies a pointer to the image buffer.

    PltResolver - Supplies a pointer to the function used to resolve PLT
        entries.

    Image - Supplies a pointer to the zeroed but otherwise uninitialized
        image buffer.

Return Value:

    None.

--*/

KSTATUS
ImpElf64GetSymbolByName (
    PLOADED_IMAGE Image,
    PSTR SymbolName,
    PLOADED_IMAGE Skip,
    PIMAGE_SYMBOL Symbol
    );

/*++

Routine Description:

    This routine attempts to find an exported symbol with the given name in the
    given binary.

Arguments:

    Image - Supplies a pointer to the image to query.

    SymbolName - Supplies a pointer to the string containing the name of the
        symbol to search for.

    Skip - Supplies an optional pointer to an image to skip when searching.

    Symbol - Supplies a pointer to a structure that receives the symbol's
        information on success.

Return Value:

    Status code.

--*/

KSTATUS
ImpElf64GetSymbolByAddress (
    PLOADED_IMAGE Image,
    PVOID Address,
    PIMAGE_SYMBOL Symbol
    );

/*++

Routine Description:

    This routine attempts to find the given address in the given image and
    resolve it to a symbol.

Arguments:

    Image - Supplies a pointer to the image to query.

    Address - Supplies the address to search for.

    Symbol - Supplies a pointer to a structure that receives the address's
        symbol information on success.

Return Value:

    Status code.

--*/

PVOID
ImpElf64ResolvePltEntry (
    PLOADED_IMAGE Image,
    ULONG RelocationOffset
    );

/*++

Routine Description:

    This routine implements the slow path for a Procedure Linkable Table entry
    that has not yet been resolved to its target function address. This routine
    is only called once for each PLT entry, as subsequent calls jump directly
    to the destination function address. It resolves the appropriate GOT
    relocation and returns a pointer to the function to jump to.

Arguments:

    Image - Supplies a pointer to the loaded image whose PLT needs resolution.
        This is really whatever pointer is in GOT + 4.

    RelocationOffset - Supplies the byte offset from the start of the
        relocation section where the relocation for this PLT entry resides, or
        the PLT index, depending on the architecture.

Return Value:

    Returns a pointer to the function to jump to (in addition to writing that
    address in the GOT at the appropriate spot).

--*/
