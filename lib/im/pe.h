/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pe.h

Abstract:

    This header contains definitions for PE images.

Author:

    Evan Green 13-Oct-2012

--*/

//
// ---------------------------------------------------------------- Definitions
//

//
// This macro returns the image alignment, in bytes, given the image section
// characteristics.
//

#define PE_SECTION_ALIGNMENT(_SectionCharacteristics) \
    (1 << ((((_SectionCharacteristics) & PE_SECTION_ALIGNMENT_MASK) >> \
           PE_SECTION_ALIGNMENT_SHIFT) - 1))

//
// PE Image definitions.
//

#define PE_MAX_LIBRARY_NAME 100
#define PE_MAX_FUNCTION_NAME 256
#define IMAGE_SIZEOF_SHORT_NAME 8
#define IMAGE_DOS_SIGNATURE 0x5A4D
#define IMAGE_NT_OPTIONAL_HDR32_MAGIC 0x10b
#define IMAGE_FILE_EXECUTABLE_IMAGE 2
#define IMAGE_NUMBEROF_DIRECTORY_ENTRIES 16
#define IMAGE_SCN_MEM_DISCARDABLE 0x2000000
#define IMAGE_SCN_CNT_CODE 0x00000020
#define IMAGE_SCN_CNT_INITIALIZED_DATA 0x00000040
#define IMAGE_SCN_CNT_UNINITIALIZED_DATA 0x00000080
#define PE_IMPORT_BY_ORDINAL 0x80000000
#define COFF_SYMBOL_NAME_LENGTH 8

//
// Machine type definitions.
//

#define IMAGE_FILE_MACHINE_I386 0x14C
#define IMAGE_FILE_MACHINE_AMD64 0x8664
#define IMAGE_FILE_MACHINE_ARM 0x1C0
#define IMAGE_FILE_MACHINE_ARMT 0x1C2
#define IMAGE_FILE_MACHINE_ARM64    0xAA64

//
// Data directory definitions.
//

#define PE_EXPORT_DIRECTORY 0
#define PE_IMPORT_DIRECTORY 1
#define PE_RESOURCE_DIRECTORY 2
#define PE_EXCEPTION_DIRECTORY 3
#define PE_SECURITY_DIRECTORY 4
#define PE_RELOCATION_DIRECTORY 5
#define PE_DEBUG_DIRECTORY 6
#define PE_DESCRIPTION_DIRECTORY 7
#define PE_SPECIAL_DIRECTORY 8
#define PE_THREAD_LOCAL_STORAGE_DIRECTORY 9
#define PE_LOAD_CONFIGURATION_DIRECTORY 10
#define PE_BOUND_IMPORT_DIRECTORY 11
#define PE_IMPORT_ADDRESS_TABLE_DIRECTORY 12
#define PE_DELAY_IMPORT_TABLE 13
#define PE_CLR_RUNTIME_DIRECTORY 14
#define PE_RESERVED_DIRECTORY 15

//
// Relocation definitions.
//

#define PE_RELOCATION_OFFSET_MASK 0x0FFF
#define PE_RELOCATION_TYPE_SHIFT 12

//
// Section Definitions
//

#define PE_SECTION_ALIGNMENT_MASK 0x00F00000
#define PE_SECTION_ALIGNMENT_SHIFT 20

//
// ------------------------------------------------------ Data Type Definitions
//

//
// PE Image header definitions.
//

typedef struct _IMAGE_SECTION_HEADER {
    BYTE Name[IMAGE_SIZEOF_SHORT_NAME];
    union {
        DWORD PhysicalAddress;
        DWORD VirtualSize;
    } Misc;
    DWORD VirtualAddress;
    DWORD SizeOfRawData;
    DWORD PointerToRawData;
    DWORD PointerToRelocations;
    DWORD PointerToLinenumbers;
    WORD NumberOfRelocations;
    WORD NumberOfLinenumbers;
    DWORD Characteristics;
} PACKED IMAGE_SECTION_HEADER, *PIMAGE_SECTION_HEADER;

typedef struct _IMAGE_FILE_HEADER {
    WORD Machine;
    WORD NumberOfSections;
    DWORD TimeDateStamp;
    DWORD PointerToSymbolTable;
    DWORD NumberOfSymbols;
    WORD SizeOfOptionalHeader;
    WORD Characteristics;
} PACKED IMAGE_FILE_HEADER, *PIMAGE_FILE_HEADER;

typedef struct _IMAGE_DATA_DIRECTORY {
    DWORD VirtualAddress;
    DWORD Size;
} PACKED IMAGE_DATA_DIRECTORY, *PIMAGE_DATA_DIRECTORY;

typedef struct _IMAGE_OPTIONAL_HEADER {
    WORD Magic;
    BYTE MajorLinkerVersion;
    BYTE MinorLinkerVersion;
    DWORD SizeOfCode;
    DWORD SizeOfInitializedData;
    DWORD SizeOfUninitializedData;
    DWORD AddressOfEntryPoint;
    DWORD BaseOfCode;
    DWORD BaseOfData;
    DWORD ImageBase;
    DWORD SectionAlignment;
    DWORD FileAlignment;
    WORD MajorOperatingSystemVersion;
    WORD MinorOperatingSystemVersion;
    WORD MajorImageVersion;
    WORD MinorImageVersion;
    WORD MajorSubsystemVersion;
    WORD MinorSubsystemVersion;
    DWORD Win32VersionValue;
    DWORD SizeOfImage;
    DWORD SizeOfHeaders;
    DWORD CheckSum;
    WORD Subsystem;
    WORD DllCharacteristics;
    DWORD SizeOfStackReserve;
    DWORD SizeOfStackCommit;
    DWORD SizeOfHeapReserve;
    DWORD SizeOfHeapCommit;
    DWORD LoaderFlags;
    DWORD NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
} PACKED IMAGE_OPTIONAL_HEADER32, *PIMAGE_OPTIONAL_HEADER32;

typedef IMAGE_OPTIONAL_HEADER32 IMAGE_OPTIONAL_HEADER;
typedef PIMAGE_OPTIONAL_HEADER32 PIMAGE_OPTIONAL_HEADER;

typedef struct _IMAGE_NT_HEADERS32 {
    DWORD Signature;
    IMAGE_FILE_HEADER FileHeader;
    IMAGE_OPTIONAL_HEADER OptionalHeader;
} PACKED IMAGE_NT_HEADERS32, *PIMAGE_NT_HEADERS32;

typedef IMAGE_NT_HEADERS32 IMAGE_NT_HEADERS;
typedef PIMAGE_NT_HEADERS32 PIMAGE_NT_HEADERS;

typedef struct _IMAGE_DOS_HEADER {
    WORD e_magic;
    WORD e_cblp;
    WORD e_cp;
    WORD e_crlc;
    WORD e_cparhdr;
    WORD e_minalloc;
    WORD e_maxalloc;
    WORD e_ss;
    WORD e_sp;
    WORD e_csum;
    WORD e_ip;
    WORD e_cs;
    WORD e_lfarlc;
    WORD e_ovno;
    WORD e_res[4];
    WORD e_oemid;
    WORD e_oeminfo;
    WORD e_res2[10];
    LONG e_lfanew;
} PACKED IMAGE_DOS_HEADER, *PIMAGE_DOS_HEADER;

typedef struct _PE_RELOCATION_BLOCK {
    PVOID PageRva;
    ULONG BlockSizeInBytes;
} PACKED PE_RELOCATION_BLOCK, *PPE_RELOCATION_BLOCK;

typedef USHORT PE_RELOCATION, *PPE_RELOCATION;

typedef enum _PE_RELOCATION_TYPE {
    PeRelocationAbsolute = 0,
    PeRelocationHigh = 1,
    PeRelocationLow = 2,
    PeRelocationHighLow = 3,
    PeRelocationHighAdjust = 4,
    PeRelocationMipsJumpAddress = 5,
    PeRelocationMipsJumpAddress16 = 9,
    PeRelocation64 = 10
} PE_RELOCATION_TYPE, *PPE_RELOCATION_TYPE;

typedef struct _PE_EXPORT_DIRECTORY_TABLE {
    ULONG ExportFlags;
    ULONG Timestamp;
    USHORT MajorVersion;
    USHORT MinorVersion;
    ULONG NameRva;
    ULONG OrdinalBase;
    ULONG AddressTableEntryCount;
    ULONG NamePointerCount;
    ULONG ExportAddressTableRva;
    ULONG NamePointerRva;
    ULONG OrdinalTableRva;
} PACKED PE_EXPORT_DIRECTORY_TABLE, *PPE_EXPORT_DIRECTORY_TABLE;

typedef struct _PE_IMPORT_DIRECTORY_TABLE {
    ULONG ImportLookupTableRva;
    ULONG Timestamp;
    ULONG ForwarderChain;
    ULONG NameRva;
    ULONG ImportAddressTableRva;
} PACKED PE_IMPORT_DIRECTORY_TABLE, *PPE_IMPORT_DIRECTORY_TABLE;

typedef ULONG PE_IMPORT_LOOKUP_TABLE, *PPE_IMPORT_LOOKUP_TABLE;

typedef struct _PE_IMPORT_NAME_ENTRY {
    USHORT Hint;
    CHAR Name[ANYSIZE_ARRAY];
} PACKED PE_IMPORT_NAME_ENTRY, *PPE_IMPORT_NAME_ENTRY;

typedef struct _COFF_SYMBOL {
    union {
        CHAR Name[COFF_SYMBOL_NAME_LENGTH];
        struct {
            ULONG Zeroes;
            ULONG Offset;
        };
    };

    ULONG Value;
    USHORT Section;
    USHORT Type;
    UCHAR Class;
    UCHAR AuxCount;
} PACKED COFF_SYMBOL, *PCOFF_SYMBOL;

//
// -------------------------------------------------------- Function Prototypes
//

BOOL
ImpPeGetHeaders (
    PIMAGE_BUFFER Buffer,
    PIMAGE_NT_HEADERS *PeHeaders
    );

/*++

Routine Description:

    This routine returns a pointer to the PE image headers given a buffer
    containing the executable image mapped in memory.

Arguments:

    Buffer - Supplies a pointer to the buffer to get the headers from.

    PeHeaders - Supplies a pointer where the location of the PE headers will
        be returned.

Return Value:

    TRUE on success.

    FALSE otherwise.

--*/

BOOL
ImpPeGetSection (
    PIMAGE_BUFFER Buffer,
    PSTR SectionName,
    PVOID *Section,
    PULONGLONG VirtualAddress,
    PULONG SectionSizeInFile,
    PULONG SectionSizeInMemory
    );

/*++

Routine Description:

    This routine gets a pointer to the given section in a PE image given a
    memory mapped file.

Arguments:

    Buffer - Supplies a pointer to the file buffer.

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

