/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    peimage.h

Abstract:

    This header contains definitions for PE32+ UEFI images.

Author:

    Evan Green 6-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// PE32+ Subsystem types for EFI images
//

#define EFI_IMAGE_SUBSYSTEM_EFI_APPLICATION         10
#define EFI_IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER 11
#define EFI_IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER      12
#define EFI_IMAGE_SUBSYSTEM_SAL_RUNTIME_DRIVER      13

//
// PE32+ Machine type for EFI images
//
#define IMAGE_FILE_MACHINE_I386     0x014C
#define IMAGE_FILE_MACHINE_IA64     0x0200
#define IMAGE_FILE_MACHINE_EBC      0x0EBC
#define IMAGE_FILE_MACHINE_X64      0x8664

//
// Thumb only image.
//

#define IMAGE_FILE_MACHINE_ARM      0x01C0

//
// Mixed ARM and Thumb/Thumb2 (little endian).
//

#define IMAGE_FILE_MACHINE_ARMT     0x01C2

//
// 64-bit ARM, little endian.
//

#define IMAGE_FILE_MACHINE_ARM64    0xAA64

#define EFI_IMAGE_DOS_SIGNATURE     0x5A4D
#define EFI_IMAGE_OS2_SIGNATURE     0x454E
#define EFI_IMAGE_OS2_SIGNATURE_LE  0x454C
#define EFI_IMAGE_NT_SIGNATURE      0x00004550
#define EFI_IMAGE_EDOS_SIGNATURE    0x44454550

#define EFI_IMAGE_SIZEOF_FILE_HEADER        20

//
// Set this if relocation information is stripped from the file.
//

#define EFI_IMAGE_FILE_RELOCS_STRIPPED      0x0001

//
// Set this if the file is executable.
//

#define EFI_IMAGE_FILE_EXECUTABLE_IMAGE     0x0002

//
// Set this if line numbers have been stripped from the file.
//

#define EFI_IMAGE_FILE_LINE_NUMS_STRIPPED   0x0004

//
// Set this if local symbols have been stripped from the file.
//

#define EFI_IMAGE_FILE_LOCAL_SYMS_STRIPPED  0x0008

//
// Set this if the image supports address > 2GB.
//

#define EFI_IMAGE_FILE_LARGE_ADDRESS_AWARE  0x0020

//
// Set this if the bytes of the machine word are reversed.
//

#define EFI_IMAGE_FILE_BYTES_REVERSED_LO    0x0080

//
// Set this if the machine is 32-bit.
//

#define EFI_IMAGE_FILE_32BIT_MACHINE        0x0100

//
// Set this if debuging information is stripped from the file.
//

#define EFI_IMAGE_FILE_DEBUG_STRIPPED       0x0200

//
// Set this if this is a system file.
//

#define EFI_IMAGE_FILE_SYSTEM               0x1000

//
// Set this if the file is a DLL.
//

#define EFI_IMAGE_FILE_DLL                  0x2000

//
// Set this if the high bytes of the machine word are reversed.
//

#define EFI_IMAGE_FILE_BYTES_REVERSED_HI    0x8000

//
// Define machine types.
//

#define EFI_IMAGE_FILE_MACHINE_UNKNOWN      0

//
// Intel 386
//

#define EFI_IMAGE_FILE_MACHINE_I386         0x14C

//
// MIPS little endian, 0540 big endian
//

#define EFI_IMAGE_FILE_MACHINE_R3000        0x162

//
// MIPS little endian
//

#define EFI_IMAGE_FILE_MACHINE_R4000        0x166

//
// Alpha AXP
//

#define EFI_IMAGE_FILE_MACHINE_ALPHA        0x184

//
// IBM PowerPC little endian
//

#define EFI_IMAGE_FILE_MACHINE_POWERPC      0x1F0

//
// Intel EM machine
//

#define EFI_IMAGE_FILE_MACHINE_TAHOE        0x7CC

#define EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES 16

#define EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC 0x10B

#define EFI_IMAGE_ROM_OPTIONAL_HDR_MAGIC      0x107
#define EFI_IMAGE_SIZEOF_ROM_OPTIONAL_HEADER  \
    sizeof(EFI_IMAGE_ROM_OPTIONAL_HEADER)

#define EFI_IMAGE_NT_OPTIONAL_HDR64_MAGIC 0x20b
#define EFI_IMAGE_SIZEOF_NT_OPTIONAL32_HEADER sizeof(EFI_IMAGE_NT_HEADERS32)
#define EFI_IMAGE_SIZEOF_NT_OPTIONAL64_HEADER sizeof(EFI_IMAGE_NT_HEADERS64)

//
// Subsystem Values
//

#define EFI_IMAGE_SUBSYSTEM_UNKNOWN     0
#define EFI_IMAGE_SUBSYSTEM_NATIVE      1
#define EFI_IMAGE_SUBSYSTEM_WINDOWS_GUI 2
#define EFI_IMAGE_SUBSYSTEM_WINDOWS_CUI 3
#define EFI_IMAGE_SUBSYSTEM_OS2_CUI     5
#define EFI_IMAGE_SUBSYSTEM_POSIX_CUI   7

//
// Directory Entries
//

#define EFI_IMAGE_DIRECTORY_ENTRY_EXPORT      0
#define EFI_IMAGE_DIRECTORY_ENTRY_IMPORT      1
#define EFI_IMAGE_DIRECTORY_ENTRY_RESOURCE    2
#define EFI_IMAGE_DIRECTORY_ENTRY_EXCEPTION   3
#define EFI_IMAGE_DIRECTORY_ENTRY_SECURITY    4
#define EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC   5
#define EFI_IMAGE_DIRECTORY_ENTRY_DEBUG       6
#define EFI_IMAGE_DIRECTORY_ENTRY_COPYRIGHT   7
#define EFI_IMAGE_DIRECTORY_ENTRY_GLOBALPTR   8
#define EFI_IMAGE_DIRECTORY_ENTRY_TLS         9
#define EFI_IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG 10

//
// Section header format.
//

#define EFI_IMAGE_SIZEOF_SHORT_NAME 8

#define EFI_IMAGE_SIZEOF_SECTION_HEADER       40

#define EFI_IMAGE_SCN_TYPE_NO_PAD             0x00000008
#define EFI_IMAGE_SCN_CNT_CODE                0x00000020
#define EFI_IMAGE_SCN_CNT_INITIALIZED_DATA    0x00000040
#define EFI_IMAGE_SCN_CNT_UNINITIALIZED_DATA  0x00000080

#define EFI_IMAGE_SCN_LNK_OTHER               0x00000100

//
// Set if the section contains comments or some other type of information.
//

#define EFI_IMAGE_SCN_LNK_INFO                0x00000200

//
// Set if the section contents will not become part of the image.
//

#define EFI_IMAGE_SCN_LNK_REMOVE              0x00000800
#define EFI_IMAGE_SCN_LNK_COMDAT              0x00001000

#define EFI_IMAGE_SCN_ALIGN_1BYTES            0x00100000
#define EFI_IMAGE_SCN_ALIGN_2BYTES            0x00200000
#define EFI_IMAGE_SCN_ALIGN_4BYTES            0x00300000
#define EFI_IMAGE_SCN_ALIGN_8BYTES            0x00400000
#define EFI_IMAGE_SCN_ALIGN_16BYTES           0x00500000
#define EFI_IMAGE_SCN_ALIGN_32BYTES           0x00600000
#define EFI_IMAGE_SCN_ALIGN_64BYTES           0x00700000

#define EFI_IMAGE_SCN_MEM_DISCARDABLE         0x02000000
#define EFI_IMAGE_SCN_MEM_NOT_CACHED          0x04000000
#define EFI_IMAGE_SCN_MEM_NOT_PAGED           0x08000000
#define EFI_IMAGE_SCN_MEM_SHARED              0x10000000
#define EFI_IMAGE_SCN_MEM_EXECUTE             0x20000000
#define EFI_IMAGE_SCN_MEM_READ                0x40000000
#define EFI_IMAGE_SCN_MEM_WRITE               0x80000000

//
// Symbol format.
//

#define EFI_IMAGE_SIZEOF_SYMBOL 18

//
// Section values.
//
// Symbols have a section number of the section in which they are
// defined. Otherwise, section numbers have the following meanings:
//

//
// The symbol is undefined or is common.
//

#define EFI_IMAGE_SYM_UNDEFINED (UINT16)0

//
// The symbol is an absolute value.
//

#define EFI_IMAGE_SYM_ABSOLUTE  (UINT16)-1

//
// The symbol is a special debug item.
//

#define EFI_IMAGE_SYM_DEBUG     (UINT16)-2

//
// Type (fundamental) values.
//

#define EFI_IMAGE_SYM_TYPE_NULL   0
#define EFI_IMAGE_SYM_TYPE_VOID   1
#define EFI_IMAGE_SYM_TYPE_CHAR   2
#define EFI_IMAGE_SYM_TYPE_SHORT  3
#define EFI_IMAGE_SYM_TYPE_INT    4
#define EFI_IMAGE_SYM_TYPE_LONG   5
#define EFI_IMAGE_SYM_TYPE_FLOAT  6
#define EFI_IMAGE_SYM_TYPE_DOUBLE 7
#define EFI_IMAGE_SYM_TYPE_STRUCT 8
#define EFI_IMAGE_SYM_TYPE_UNION  9
#define EFI_IMAGE_SYM_TYPE_ENUM   10
#define EFI_IMAGE_SYM_TYPE_MOE    11
#define EFI_IMAGE_SYM_TYPE_BYTE   12
#define EFI_IMAGE_SYM_TYPE_WORD   13
#define EFI_IMAGE_SYM_TYPE_UINT   14
#define EFI_IMAGE_SYM_TYPE_DWORD  15

//
// Type (derived) values.
//

#define EFI_IMAGE_SYM_DTYPE_NULL      0
#define EFI_IMAGE_SYM_DTYPE_POINTER   1
#define EFI_IMAGE_SYM_DTYPE_FUNCTION  2
#define EFI_IMAGE_SYM_DTYPE_ARRAY     3

//
// Storage classes.
//

#define EFI_IMAGE_SYM_CLASS_END_OF_FUNCTION   (UINT8)-1
#define EFI_IMAGE_SYM_CLASS_NULL              0
#define EFI_IMAGE_SYM_CLASS_AUTOMATIC         1
#define EFI_IMAGE_SYM_CLASS_EXTERNAL          2
#define EFI_IMAGE_SYM_CLASS_STATIC            3
#define EFI_IMAGE_SYM_CLASS_REGISTER          4
#define EFI_IMAGE_SYM_CLASS_EXTERNAL_DEF      5
#define EFI_IMAGE_SYM_CLASS_LABEL             6
#define EFI_IMAGE_SYM_CLASS_UNDEFINED_LABEL   7
#define EFI_IMAGE_SYM_CLASS_MEMBER_OF_STRUCT  8
#define EFI_IMAGE_SYM_CLASS_ARGUMENT          9
#define EFI_IMAGE_SYM_CLASS_STRUCT_TAG        10
#define EFI_IMAGE_SYM_CLASS_MEMBER_OF_UNION   11
#define EFI_IMAGE_SYM_CLASS_UNION_TAG         12
#define EFI_IMAGE_SYM_CLASS_TYPE_DEFINITION   13
#define EFI_IMAGE_SYM_CLASS_UNDEFINED_STATIC  14
#define EFI_IMAGE_SYM_CLASS_ENUM_TAG          15
#define EFI_IMAGE_SYM_CLASS_MEMBER_OF_ENUM    16
#define EFI_IMAGE_SYM_CLASS_REGISTER_PARAM    17
#define EFI_IMAGE_SYM_CLASS_BIT_FIELD         18
#define EFI_IMAGE_SYM_CLASS_BLOCK             100
#define EFI_IMAGE_SYM_CLASS_FUNCTION          101
#define EFI_IMAGE_SYM_CLASS_END_OF_STRUCT     102
#define EFI_IMAGE_SYM_CLASS_FILE              103
#define EFI_IMAGE_SYM_CLASS_SECTION           104
#define EFI_IMAGE_SYM_CLASS_WEAK_EXTERNAL     105

//
// type packing constants
//

#define EFI_IMAGE_N_BTMASK  017
#define EFI_IMAGE_N_TMASK   060
#define EFI_IMAGE_N_TMASK1  0300
#define EFI_IMAGE_N_TMASK2  0360
#define EFI_IMAGE_N_BTSHFT  4
#define EFI_IMAGE_N_TSHIFT  2

//
// Communal selection types.
//

#define EFI_IMAGE_COMDAT_SELECT_NODUPLICATES    1
#define EFI_IMAGE_COMDAT_SELECT_ANY             2
#define EFI_IMAGE_COMDAT_SELECT_SAME_SIZE       3
#define EFI_IMAGE_COMDAT_SELECT_EXACT_MATCH     4
#define EFI_IMAGE_COMDAT_SELECT_ASSOCIATIVE     5

#define EFI_IMAGE_WEAK_EXTERN_SEARCH_NOLIBRARY  1
#define EFI_IMAGE_WEAK_EXTERN_SEARCH_LIBRARY    2
#define EFI_IMAGE_WEAK_EXTERN_SEARCH_ALIAS      3

//
// Relocation format definitions
//

#define EFI_IMAGE_SIZEOF_RELOCATION 10

//
// I386 relocation types.
//

//
// The reference is absolute, no relocation is necessary.
//

#define EFI_IMAGE_REL_I386_ABSOLUTE 0

//
// This is a direct 16-bit reference to the symbol's virtual address.
//

#define EFI_IMAGE_REL_I386_DIR16    01

//
// This is a PC-relative 16-bit reference to the symbol's virtual address.
//

#define EFI_IMAGE_REL_I386_REL16    02

//
// This is a direct 32-bit reference to the symbol's virtual address.
//

#define EFI_IMAGE_REL_I386_DIR32    06

//
// This is a direct 32-bit reference to the symbol's virtual address, not
// including the base.
//

#define EFI_IMAGE_REL_I386_DIR32NB  07

//
// This is a direct 16-bit reference to the segment selector bits of a 32-bit
// virtual address.
//

#define EFI_IMAGE_REL_I386_SEG12    09
#define EFI_IMAGE_REL_I386_SECTION  010
#define EFI_IMAGE_REL_I386_SECREL   011

//
// This is a PC-relative 32-bit reference to the symbol's virtual address.
//

#define EFI_IMAGE_REL_I386_REL32    020

//
// x64 processor relocation types.
//

#define IMAGE_REL_AMD64_ABSOLUTE    0x0000
#define IMAGE_REL_AMD64_ADDR64      0x0001
#define IMAGE_REL_AMD64_ADDR32      0x0002
#define IMAGE_REL_AMD64_ADDR32NB    0x0003
#define IMAGE_REL_AMD64_REL32       0x0004
#define IMAGE_REL_AMD64_REL32_1     0x0005
#define IMAGE_REL_AMD64_REL32_2     0x0006
#define IMAGE_REL_AMD64_REL32_3     0x0007
#define IMAGE_REL_AMD64_REL32_4     0x0008
#define IMAGE_REL_AMD64_REL32_5     0x0009
#define IMAGE_REL_AMD64_SECTION     0x000A
#define IMAGE_REL_AMD64_SECREL      0x000B
#define IMAGE_REL_AMD64_SECREL7     0x000C
#define IMAGE_REL_AMD64_TOKEN       0x000D
#define IMAGE_REL_AMD64_SREL32      0x000E
#define IMAGE_REL_AMD64_PAIR        0x000F
#define IMAGE_REL_AMD64_SSPAN32     0x0010

//
// Based relocation types
//

#define EFI_IMAGE_SIZEOF_BASE_RELOCATION  8

//
// Based relocation types.
//

#define EFI_IMAGE_REL_BASED_ABSOLUTE      0
#define EFI_IMAGE_REL_BASED_HIGH          1
#define EFI_IMAGE_REL_BASED_LOW           2
#define EFI_IMAGE_REL_BASED_HIGHLOW       3
#define EFI_IMAGE_REL_BASED_HIGHADJ       4
#define EFI_IMAGE_REL_BASED_MIPS_JMPADDR  5
#define EFI_IMAGE_REL_BASED_ARM_MOV32A    5
#define EFI_IMAGE_REL_BASED_ARM_MOV32T    7
#define EFI_IMAGE_REL_BASED_IA64_IMM64    9
#define EFI_IMAGE_REL_BASED_DIR64         10

#define EFI_IMAGE_SIZEOF_LINENUMBER 6

//
// Archive format definitions
//

#define EFI_IMAGE_ARCHIVE_START_SIZE        8
#define EFI_IMAGE_ARCHIVE_START             "!<arch>\n"
#define EFI_IMAGE_ARCHIVE_END               "`\n"
#define EFI_IMAGE_ARCHIVE_PAD               "\n"
#define EFI_IMAGE_ARCHIVE_LINKER_MEMBER     "/               "
#define EFI_IMAGE_ARCHIVE_LONGNAMES_MEMBER  "//              "

#define EFI_IMAGE_SIZEOF_ARCHIVE_MEMBER_HDR 60

#define EFI_IMAGE_ORDINAL_FLAG              0x80000000
#define EFI_IMAGE_SNAP_BY_ORDINAL(Ordinal)  \
    ((Ordinal & EFI_IMAGE_ORDINAL_FLAG) != 0)

#define EFI_IMAGE_ORDINAL(Ordinal)          (Ordinal & 0xFFFF)

#define EFI_IMAGE_DEBUG_TYPE_CODEVIEW 2
#define CODEVIEW_SIGNATURE_NB10 0x3031424E // 'NB10'
#define CODEVIEW_SIGNATURE_RSDS 0x53445352 // 'RSDS'
#define CODEVIEW_SIGNATURE_MTOC 0x434F544D // 'MTOC'

#define EFI_TE_IMAGE_HEADER_SIGNATURE 0x5A56      // "VZ"

//
// Data directory indexes in our TE image header
//

#define EFI_TE_IMAGE_DIRECTORY_ENTRY_BASERELOC  0
#define EFI_TE_IMAGE_DIRECTORY_ENTRY_DEBUG      1

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores information about a DOS header. PE images can start
    with an optional DOS header so that if an image is run under DOS it can
    print an error message.

Members:

    e_magic - Stores the magic number.

    e_cblp - Stores the byte count on the last page of the file.

    e_cp - Stores the number of pages in the file.

    e_crlc - Stores the location of the relocations.

    e_cparhdr - Stores the size of the header in paragraphs.

    e_minalloc - Stores the minimum number of extra paragraphs needed.

    e_maxalloc - Stores the maximum number of extra paragraphs needed.

    e_ss - Stores the initial (relative) SS value.

    e_sp - Stores the initial SP value.

    e_csum - Stores the checksum.

    e_ip - Stores the initial IP value.

    e_cs - Stores the initial (relative) CS value.

    e_lfarlc - Stores the file address of the relocation table.

    e_ovno - Stores the overlay number.

    e_res - Stores reserved words.

    e_oemid - Stores the OEM identifier.

    e_oeminfo - Stores the OEM-specific information.

    e_res - Stores additional reserved words.

    e_lfanew - Stores the file address of the new EXE header.

--*/

typedef struct {
    UINT16 e_magic;
    UINT16 e_cblp;
    UINT16 e_cp;
    UINT16 e_crlc;
    UINT16 e_cparhdr;
    UINT16 e_minalloc;
    UINT16 e_maxalloc;
    UINT16 e_ss;
    UINT16 e_sp;
    UINT16 e_csum;
    UINT16 e_ip;
    UINT16 e_cs;
    UINT16 e_lfarlc;
    UINT16 e_ovno;
    UINT16 e_res[4];
    UINT16 e_oemid;
    UINT16 e_oeminfo;
    UINT16 e_res2[10];
    UINT32 e_lfanew;
} EFI_IMAGE_DOS_HEADER;

typedef struct {
    UINT16 Machine;
    UINT16 NumberOfSections;
    UINT32 TimeDateStamp;
    UINT32 PointerToSymbolTable;
    UINT32 NumberOfSymbols;
    UINT16 SizeOfOptionalHeader;
    UINT16 Characteristics;
} EFI_IMAGE_FILE_HEADER;

typedef struct {
    UINT32 VirtualAddress;
    UINT32 Size;
} EFI_IMAGE_DATA_DIRECTORY;

typedef struct {
    UINT16 Magic;
    UINT8 MajorLinkerVersion;
    UINT8 MinorLinkerVersion;
    UINT32 SizeOfCode;
    UINT32 SizeOfInitializedData;
    UINT32 SizeOfUninitializedData;
    UINT32 AddressOfEntryPoint;
    UINT32 BaseOfCode;
    UINT32 BaseOfData;
    UINT32 BaseOfBss;
    UINT32 GprMask;
    UINT32 CprMask[4];
    UINT32 GpValue;
} EFI_IMAGE_ROM_OPTIONAL_HEADER;

typedef struct {
  EFI_IMAGE_FILE_HEADER FileHeader;
  EFI_IMAGE_ROM_OPTIONAL_HEADER OptionalHeader;
} EFI_IMAGE_ROM_HEADERS;

//
// These structure are for use ONLY by tools. All proper EFI code MUST use the
// EFI_IMAGE_OPTIONAL_HEADER only.
//

typedef struct {
    UINT16 Magic;
    UINT8 MajorLinkerVersion;
    UINT8 MinorLinkerVersion;
    UINT32 SizeOfCode;
    UINT32 SizeOfInitializedData;
    UINT32 SizeOfUninitializedData;
    UINT32 AddressOfEntryPoint;
    UINT32 BaseOfCode;
    UINT32 BaseOfData;
    UINT32 ImageBase;
    UINT32 SectionAlignment;
    UINT32 FileAlignment;
    UINT16 MajorOperatingSystemVersion;
    UINT16 MinorOperatingSystemVersion;
    UINT16 MajorImageVersion;
    UINT16 MinorImageVersion;
    UINT16 MajorSubsystemVersion;
    UINT16 MinorSubsystemVersion;
    UINT32 Win32VersionValue;
    UINT32 SizeOfImage;
    UINT32 SizeOfHeaders;
    UINT32 CheckSum;
    UINT16 Subsystem;
    UINT16 DllCharacteristics;
    UINT32 SizeOfStackReserve;
    UINT32 SizeOfStackCommit;
    UINT32 SizeOfHeapReserve;
    UINT32 SizeOfHeapCommit;
    UINT32 LoaderFlags;
    UINT32 NumberOfRvaAndSizes;
    EFI_IMAGE_DATA_DIRECTORY
                          DataDirectory[EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES];

} EFI_IMAGE_OPTIONAL_HEADER32;

typedef struct {
    UINT16 Magic;
    UINT8 MajorLinkerVersion;
    UINT8 MinorLinkerVersion;
    UINT32 SizeOfCode;
    UINT32 SizeOfInitializedData;
    UINT32 SizeOfUninitializedData;
    UINT32 AddressOfEntryPoint;
    UINT32 BaseOfCode;
    UINT64 ImageBase;
    UINT32 SectionAlignment;
    UINT32 FileAlignment;
    UINT16 MajorOperatingSystemVersion;
    UINT16 MinorOperatingSystemVersion;
    UINT16 MajorImageVersion;
    UINT16 MinorImageVersion;
    UINT16 MajorSubsystemVersion;
    UINT16 MinorSubsystemVersion;
    UINT32 Win32VersionValue;
    UINT32 SizeOfImage;
    UINT32 SizeOfHeaders;
    UINT32 CheckSum;
    UINT16 Subsystem;
    UINT16 DllCharacteristics;
    UINT64 SizeOfStackReserve;
    UINT64 SizeOfStackCommit;
    UINT64 SizeOfHeapReserve;
    UINT64 SizeOfHeapCommit;
    UINT32 LoaderFlags;
    UINT32 NumberOfRvaAndSizes;
    EFI_IMAGE_DATA_DIRECTORY
                          DataDirectory[EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES];

} EFI_IMAGE_OPTIONAL_HEADER64;

typedef struct {
    UINT32 Signature;
    EFI_IMAGE_FILE_HEADER FileHeader;
    EFI_IMAGE_OPTIONAL_HEADER32 OptionalHeader;
} EFI_IMAGE_NT_HEADERS32;

typedef struct {
    UINT32 Signature;
    EFI_IMAGE_FILE_HEADER FileHeader;
    EFI_IMAGE_OPTIONAL_HEADER64 OptionalHeader;
} EFI_IMAGE_NT_HEADERS64;

typedef struct {
    UINT8 Name[EFI_IMAGE_SIZEOF_SHORT_NAME];
    union {
        UINT32 PhysicalAddress;
        UINT32 VirtualSize;
    } Misc;

    UINT32 VirtualAddress;
    UINT32 SizeOfRawData;
    UINT32 PointerToRawData;
    UINT32 PointerToRelocations;
    UINT32 PointerToLinenumbers;
    UINT16 NumberOfRelocations;
    UINT16 NumberOfLinenumbers;
    UINT32 Characteristics;
} EFI_IMAGE_SECTION_HEADER;

typedef struct {
    UINT32 VirtualAddress;
    UINT32 SymbolTableIndex;
    UINT16 Type;
} EFI_IMAGE_RELOCATION;

typedef struct {
    UINT32 VirtualAddress;
    UINT32 SizeOfBlock;
} EFI_IMAGE_BASE_RELOCATION;

typedef struct {
    union {
        UINT32 SymbolTableIndex;
        UINT32 VirtualAddress;
    } Type;

    UINT16 Linenumber;
} EFI_IMAGE_LINENUMBER;

/*++

Structure Description:

    This structure defines a PE archive header member.

Members:

    Name - Stores the file member name, / terminated.

    Date - Stores the file member date in decimal format.

    UserID - Stores the file member user ID in decimal format.

    GroupID - Stores the file member group ID in decimal format.

    Mode - Stores the file member mode in octal format.

    Size - Stores the file member size in decimal format.

    EndHeader - Stores the string to end the header.

--*/

typedef struct {
    UINT8 Name[16];
    UINT8 Date[12];
    UINT8 UserID[6];
    UINT8 GroupID[6];
    UINT8 Mode[8];
    UINT8 Size[10];
    UINT8 EndHeader[2];
} EFI_IMAGE_ARCHIVE_MEMBER_HEADER;

//
// DLL Support
//

typedef struct {
    UINT32 Characteristics;
    UINT32 TimeDateStamp;
    UINT16 MajorVersion;
    UINT16 MinorVersion;
    UINT32 Name;
    UINT32 Base;
    UINT32 NumberOfFunctions;
    UINT32 NumberOfNames;
    UINT32 AddressOfFunctions;
    UINT32 AddressOfNames;
    UINT32 AddressOfNameOrdinals;
} EFI_IMAGE_EXPORT_DIRECTORY;

typedef struct {
    UINT16 Hint;
    UINT8 Name[1];
} EFI_IMAGE_IMPORT_BY_NAME;

typedef struct {
    union {
        UINT32 Function;
        UINT32 Ordinal;
        EFI_IMAGE_IMPORT_BY_NAME *AddressOfData;
    } u1;

} EFI_IMAGE_THUNK_DATA;

typedef struct {
    UINT32 Characteristics;
    UINT32 TimeDateStamp;
    UINT32 ForwarderChain;
    UINT32 Name;
    EFI_IMAGE_THUNK_DATA *FirstThunk;
} EFI_IMAGE_IMPORT_DESCRIPTOR;

typedef struct {
    UINT32 Characteristics;
    UINT32 TimeDateStamp;
    UINT16 MajorVersion;
    UINT16 MinorVersion;
    UINT32 Type;
    UINT32 SizeOfData;
    UINT32 RVA;
    UINT32 FileOffset;
} EFI_IMAGE_DEBUG_DIRECTORY_ENTRY;

typedef struct {
    UINT32 Signature;
    UINT32 Unknown;
    UINT32 Unknown2;
    UINT32 Unknown3;
} EFI_IMAGE_DEBUG_CODEVIEW_NB10_ENTRY;

typedef struct {
    UINT32 Signature;
    UINT32 Unknown;
    UINT32 Unknown2;
    UINT32 Unknown3;
    UINT32 Unknown4;
    UINT32 Unknown5;
} EFI_IMAGE_DEBUG_CODEVIEW_RSDS_ENTRY;

typedef struct {
    UINT32 Signature;
    EFI_GUID MachOUuid;
} EFI_IMAGE_DEBUG_CODEVIEW_MTOC_ENTRY;

//
// .pdata entries for X64
//

typedef struct {
    UINT32 FunctionStartAddress;
    UINT32 FunctionEndAddress;
    UINT32 UnwindInfoAddress;
} RUNTIME_FUNCTION;

typedef struct {
    UINT8 Version:3;
    UINT8 Flags:5;
    UINT8 SizeOfProlog;
    UINT8 CountOfUnwindCodes;
    UINT8 FrameRegister:4;
    UINT8 FrameRegisterOffset:4;
} UNWIND_INFO;

//
// Resource format.
//

typedef struct {
    UINT32 Characteristics;
    UINT32 TimeDateStamp;
    UINT16 MajorVersion;
    UINT16 MinorVersion;
    UINT16 NumberOfNamedEntries;
    UINT16 NumberOfIdEntries;
} EFI_IMAGE_RESOURCE_DIRECTORY;

//
// Resource directory entry format.
//

typedef struct {
    union {
        struct {
            UINT32 NameOffset:31;
            UINT32 NameIsString:1;
        } s;

        UINT32 Id;
    } u1;

    union {
        UINT32 OffsetToData;
        struct {
            UINT32 OffsetToDirectory:31;
            UINT32 DataIsDirectory:1;
        } s;

    } u2;

} EFI_IMAGE_RESOURCE_DIRECTORY_ENTRY;

//
// Resource directory entry for string.
//
typedef struct {
    UINT16 Length;
    CHAR16 String[1];
} EFI_IMAGE_RESOURCE_DIRECTORY_STRING;

//
// Resource directory entry for data array.
//
typedef struct {
    UINT32 OffsetToData;
    UINT32 Size;
    UINT32 CodePage;
    UINT32 Reserved;
} EFI_IMAGE_RESOURCE_DATA_ENTRY;

//
// Header format for TE images
//

/*++

Structure Description:

    This structure defines the header format for TE images.

Members:

    Signature - Stores the magic value for the TE format, "VZ".

    Machine - Stores the machine type.

    NumberOfSections - Stores the total number of file sections.

    Subsystem - Stores the PE subsystem.

    StrippedSize - Stores the count of bytes removed from the header.

    AddressOfEntryPoint - Stores the offset to the entry point.

    BaseOfCode - Stores the image base of the text section, required for ITP
        debugging.

    ImageBase - Stores the image base.

    DataDirectory - Stores the base relocation and debug directories.

--*/

typedef struct {
    UINT16 Signature;
    UINT16 Machine;
    UINT8 NumberOfSections;
    UINT8 Subsystem;
    UINT16 StrippedSize;
    UINT32 AddressOfEntryPoint;
    UINT32 BaseOfCode;
    UINT64 ImageBase;
    EFI_IMAGE_DATA_DIRECTORY DataDirectory[2];
} EFI_TE_IMAGE_HEADER;

//
// Union of PE32, PE32+, and TE headers
//

typedef union {
    EFI_IMAGE_NT_HEADERS32 Pe32;
    EFI_IMAGE_NT_HEADERS64 Pe32Plus;
    EFI_TE_IMAGE_HEADER  Te;
} EFI_IMAGE_OPTIONAL_HEADER_UNION;

typedef union {
    EFI_IMAGE_NT_HEADERS32 *Pe32;
    EFI_IMAGE_NT_HEADERS64 *Pe32Plus;
    EFI_TE_IMAGE_HEADER *Te;
    EFI_IMAGE_OPTIONAL_HEADER_UNION *Union;
} EFI_IMAGE_OPTIONAL_HEADER_PTR_UNION;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
