/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

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
// This macro extracts the Bind information out of an ELF Symbol's Information
// field.
//

#define ELF32_EXTRACT_SYMBOL_BIND(_Information) ((_Information) >> 4)

//
// This macro extracts the Type information from an ELF Symbol's Information
// field.
//

#define ELF32_EXTRACT_SYMBOL_TYPE(_Information) ((_Information) & 0xF)

//
// ---------------------------------------------------------------- Definitions
//

#define ELF32_IDENTIFICATION_LENGTH 16
#define ELF_MAGIC0 0x7F
#define ELF_MAGIC1 'E'
#define ELF_MAGIC2 'L'
#define ELF_MAGIC3 'F'
#define ELF_MAGIC 0x464C457F

#define ELF_CLASS_OFFSET 4
#define ELF_ENDIANNESS_OFFSET 5
#define ELF_VERSION_OFFSET 6

#define ELF_IMAGE_RELOCATABLE   1
#define ELF_IMAGE_EXECUTABLE    2
#define ELF_IMAGE_SHARED_OBJECT 3

#define ELF_MACHINE_I386 3
#define ELF_MACHINE_ARM  40
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
#define ELF_SECTION_FLAG_PROCESSOR 0xF0000000

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

//
// TODO: The shift is 6 for ELF64.
//

#define ELF_WORD_SIZE_SHIFT 5
#define ELF_WORD_SIZE_MASK ((1 << ELF_WORD_SIZE_SHIFT) - 1)

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

typedef enum _ELF32_SYMBOL_BIND {
    ElfBindLocal = 0,
    ElfBindGlobal = 1,
    ElfBindWeak = 2,
} ELF32_SYMBOL_BIND, *PELF32_SYMBOL_BIND;

typedef enum _ELF32_SYMBOL_TYPE {
    ElfSymbolNone = 0,
    ElfSymbolObject = 1,
    ElfSymbolFunction = 2,
    ElfSymbolSection = 3,
    ElfSymbolFile = 4,
    ElfSymbolCommon = 5,
    ElfSymbolTls = 6,
} ELF32_SYMBOL_TYPE, *PELF32_SYMBOL_TYPE;

typedef enum _ELF32_RELOCATION_TYPE {
    ElfRelocationNone           = 0,
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
} ELF32_RELOCATION_TYPE, *PELF32_RELOCATION_TYPE;

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
    SHORT ImageType;
    SHORT Machine;
    ULONG Version;
    ULONG EntryPoint;
    ULONG ProgramHeaderOffset;
    ULONG SectionHeaderOffset;
    ULONG Flags;
    USHORT ElfHeaderSize;
    USHORT ProgramHeaderSize;
    USHORT ProgramHeaderCount;
    USHORT SectionHeaderSize;
    USHORT SectionHeaderCount;
    USHORT StringSectionIndex;
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
    ULONG NameOffset;
    ULONG SectionType;
    ULONG Flags;
    ULONG VirtualAddress;
    ULONG Offset;
    ULONG Size;
    ULONG Link;
    ULONG Information;
    ULONG Alignment;
    ULONG EntrySize;
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
    ULONG Type;
    ULONG Offset;
    ULONG VirtualAddress;
    ULONG PhysicalAddress;
    ULONG FileSize;
    ULONG MemorySize;
    ULONG Flags;
    ULONG Alignment;
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
    ULONG Offset;
    ULONG Information;
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
    ULONG Offset;
    ULONG Information;
    LONG Addend;
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
    ULONG NameOffset;
    ULONG Value;
    ULONG Size;
    UCHAR Information;
    UCHAR Other;
    USHORT SectionIndex;
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
    ULONG Tag;
    ULONG Value;
} PACKED ELF32_DYNAMIC_ENTRY, *PELF32_DYNAMIC_ENTRY;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
ImpElfGetImageSize (
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
ImpElfLoadImage (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    PIMAGE_BUFFER Buffer,
    ULONG ImportDepth
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

    ImportDepth - Supplies the import depth to assign to the image.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_FILE_CORRUPT if the file headers were corrupt or unexpected.

    Other errors on failure.

--*/

KSTATUS
ImpElfAddImage (
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
ImpElfUnloadImage (
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
ImpElfGetHeader (
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
ImpElfGetSection (
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
ImpElfLoadAllImports (
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
ImpElfRelocateImages (
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

KSTATUS
ImpElfGetSymbolAddress (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    PSTR SymbolName,
    PVOID *Address
    );

/*++

Routine Description:

    This routine attempts to find an exported symbol with the given name in the
    given binary. This routine looks through the image and its imports.

Arguments:

    ListHead - Supplies the head of the list of loaded images.

    Image - Supplies a pointer to the image to query.

    SymbolName - Supplies a pointer to the string containing the name of the
        symbol to search for.

    Address - Supplies a pointer where the address of the symbol will be
        returned on success, or NULL will be returned on failure.

Return Value:

    Status code.

--*/

