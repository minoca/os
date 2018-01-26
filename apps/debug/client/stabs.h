/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    stabs.h

Abstract:

    This header contains definitions for the STABS debugging symbol information
    format.

Author:

    Evan Green 26-Jun-2012

--*/

//
// ---------------------------------------------------------------- Definitions
//

#define STAB_GLOBAL_SYMBOL          0x20 // N_GSYM
#define STAB_FUNCTION_NAME          0x22 // N_FNAME (Pascal)
#define STAB_FUNCTION               0x24 // N_FUN
#define STAB_STATIC                 0x26 // N_STSYM
#define STAB_BSS_SYMBOL             0x28 // N_LCSYM
#define STAB_MAIN                   0x2A // N_MAIN
#define STAB_READONLY_SYMBOL        0x2C // N_ROSYM
#define STAB_PC                     0x30 // N_PC (Pascal)
#define STAB_SYMBOL_COUNT           0x32 // N_NSYMS (Ultrix)
#define STAB_NO_MAP                 0x34 // N_NOMAP
#define STAB_MACRO_DEFINITION       0x36 // N_MAC_DEFINE
#define STAB_OBJ_FILE               0x38 // N_OBJ (Solaris2)
#define STAB_MACRO_UNDEFINE         0x3A // N_MAC_UNDEF
#define STAB_DEBUGGER_OPTIONS       0x3C // N_OPT (Solaris2)
#define STAB_REGISTER_VARIABLE      0x40 // N_RSYM
#define STAB_MODULA2                0x42 // N_M2C
#define STAB_SOURCE_LINE            0x44 // N_SLINE
#define STAB_DATA_SOURCE_LINE       0x46 // N_DSLINE
#define STAB_BSS_SOURCE_LINE        0x48 // N_BSLINE
#define STAB_SUN_CB_PATH            0x48 // N_BROWS (Sun browser)
#define STAB_DEFINITION_DEPENDENCY  0x4A // N_DEFD (Modula2)
#define STAB_FUNCTION_LINES         0x4C // N_FLINE (Solaris2)
#define STAB_EXCEPTION_VARIABLE     0x50 // N_EHDECL (C++)
#define STAB_FOR_IMC                0x50 // N_MOD2 (Ultrix)
#define STAB_CATCH                  0x54 // N_CATCH (C++)
#define STAB_UNION_ELEMENT          0x60 // N_SSYM
#define STAB_END_MODULE             0x62 // N_ENDM (Solaris2)
#define STAB_SOURCE_FILE            0x64 // N_SO
#define STAB_LOCAL_SYMBOL           0x80 // N_LSYM
#define STAB_INCLUDE_BEGIN          0x82 // N_BINCL
#define STAB_INCLUDE_NAME           0x84 // N_SOL
#define STAB_FUNCTION_PARAMETER     0xA0 // N_PSYM
#define STAB_INCLUDE_END            0xA2 // N_EINCL
#define STAB_ALTERNATE_ENTRY        0xA4 // N_ENTRY
#define STAB_LEFT_BRACE             0xC0 // N_LBRAC
#define STAB_INCLUDE_PLACEHOLDER    0xC2 // N_EXCL
#define STAB_SCOPE                  0xC4 // N_SCOPE (Modula2)
#define STAB_RIGHT_BRACE            0xE0 // N_RBRAC
#define STAB_COMMON_BLOCK_BEGIN     0xE2 // N_BCOMM
#define STAB_COMMON_BLOCK_END       0xE4 // N_ECOMM
#define STAB_COMMON_BLOCK_MEMBER    0xE8 // N_ECOML
#define STAB_WITH                   0xEA // N_WITH (Pascal)

//
// ------------------------------------------------------ Data Type Definitions
//

#define STAB_REGISTER_TO_GENERAL(_StabRegister) (_StabRegister)

typedef struct _INCLUDE_STACK_ELEMENT INCLUDE_STACK_ELEMENT,
                                      *PINCLUDE_STACK_ELEMENT;

/*++

Structure Description:

    This structure stores information about include file numbers. Include files
    are indexed as they are added, and an individual source file can be located
    in multiple places on the stack.

Members:

    IncludeFile - Stores a pointer to the source file being referenced.

    Index - Stores the position in the include stack.

    NextElement - Stores a pointer to the next element on the include stack. The
        final element will have a NextElement of NULL.

--*/

struct _INCLUDE_STACK_ELEMENT {
    PSOURCE_FILE_SYMBOL IncludeFile;
    ULONG Index;
    PINCLUDE_STACK_ELEMENT NextElement;
};

/*++

Structure Description:

    This structure stores information about an unresolved cross reference. Stabs
    can reference structures, unions, an enums by name that may or may not be
    defined yet. When these stabs are encountered, a cross reference entry is
    created and put onto a list. At the end of parsing a stab file, this list
    of cross references is drained and resolved.

Members:

    ListEntry - Stores pointers to the next and previous cross reference
        entries.

    ReferringTypeName - Stores a pointer to the name type that made the
        reference and currently has dangling reference information.

    ReferringTypeSource - Stores a pointer to the source file that owns the
        reference.

    ReferringTypeNumber - Stores the type number of the yet-to-be-defined time.

    ReferenceString - Stores a pointer to the string that defines the reference.

--*/

typedef struct _CROSS_REFERENCE_ENTRY {
    LIST_ENTRY ListEntry;
    PSTR ReferringTypeName;
    PSOURCE_FILE_SYMBOL ReferringTypeSource;
    LONG ReferringTypeNumber;
    PSTR ReferenceString;
} CROSS_REFERENCE_ENTRY, *PCROSS_REFERENCE_ENTRY;

/*++

Structure Description:

    This structure stores the current STABs specific symbol information. It
    is primarily used during parsing.

Members:

    RawSymbolTable - Stores a pointer to a buffer containing the symbol table
        out of the PE or ELF file.

    RawSymbolTableSize - Stores the size of the RawSymbolTable buffer, in bytes.

    RawSymbolTableStrings - Stores a pointer to a buffer containing the string
        table associated with the symbol table in the PE or ELF file.

    RawSymbolTableStringsSize - Stores the size of the RawSymbolTableStrings,
        in bytes.

    RawStabs - Stores a pointer to a buffer containing the .stab section of
        the loaded image.

    RawStabsSize - Stores the size of the RawStabs buffer.

    RawStabStrings - Stores a pointer to a buffer containing the .stabstr
        section of the loaded image.

    RawStabStringsSize - Stores the size of the RawStabStrings buffer.

    CurrentModule - Stores a pointer back to the current module being parsed.

    CurrentSourceDirectory - Stores a pointer to the string containing the last
        seen source directory.

    CurrentSourceFile - Stores a pointer to the source file currently being
        parsed.

    CurrentSourceLine - Stores a pointer to the source line currently being
        parsed.

    CurrentSourceLineFile - Stores a pointer to the source file that source
        lines belong to.

    CurrentFunction - Stores a pointer to the function currently being parsed.

    IncludeStack - Stores a pointer to the list of include files in the current
        source file and their positions in the include stack.

    CrossReferenceListHead - Stores the list head of every unresolved cross-
        reference stab that has been encountered in the current source file.

    MaxIncludeIndex - Stores the number of files that have been included by the
        current source file. Since types are defined by a type number and an
        include index, it's important to keep track of that maximum include
        index we expect to see.

    MaxBraceAddress - Stores the address of the most recent (innermost) brace.

--*/

typedef struct _STAB_CONTEXT {
    PVOID RawSymbolTable;
    ULONG RawSymbolTableSize;
    PVOID RawSymbolTableStrings;
    ULONG RawSymbolTableStringsSize;
    PVOID RawStabs;
    ULONG RawStabsSize;
    PVOID RawStabStrings;
    ULONG RawStabStringsSize;
    PDEBUG_SYMBOLS CurrentModule;
    PSTR CurrentSourceDirectory;
    PSOURCE_FILE_SYMBOL CurrentSourceFile;
    PSOURCE_LINE_SYMBOL CurrentSourceLine;
    PSOURCE_FILE_SYMBOL CurrentSourceLineFile;
    PFUNCTION_SYMBOL CurrentFunction;
    PINCLUDE_STACK_ELEMENT IncludeStack;
    LIST_ENTRY CrossReferenceListHead;
    ULONG MaxIncludeIndex;
    ULONGLONG MaxBraceAddress;
} STAB_CONTEXT, *PSTAB_CONTEXT;

/*++

Structure Description:

    This structure defines the format for the raw stabs in the .stab section.
    These structures were generated by the compiler/linker. It's important that
    members of this structure not be padded.

Members:

    StringIndex - Stores the index from the start of the .stabstr section
        where the string for this stab is located.

    Type - Stores the stab type, one of the STAB_* definitions
         (eg. STAB_LOCAL_SYMBOL).

    Other - Usually 0.

    Description - Stores a description of the stab. This field has various
        uses, for example in a STAB_SOURCE_LINE, the description field holds
        the line number.

    Value - Stores the value of the symbol. In many cases, this is a virtual
        address.

--*/

#pragma pack(push, 1)

typedef struct _RAW_STAB {
    ULONG StringIndex;
    UCHAR Type;
    UCHAR Other;
    USHORT Description;
    ULONG Value;
} PACKED RAW_STAB, *PRAW_STAB;

#pragma pack(pop)

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
DbgpStabsLoadSymbols (
    PSTR Filename,
    IMAGE_MACHINE_TYPE MachineType,
    ULONG Flags,
    PVOID HostContext,
    PDEBUG_SYMBOLS *Symbols
    );

/*++

Routine Description:

    This routine loads debugging symbol information from the specified file.

Arguments:

    Filename - Supplies the name of the binary to load symbols from.

    MachineType - Supplies the required machine type of the image. Set to
        unknown to allow the symbol library to load a file with any machine
        type.

    Flags - Supplies a bitfield of flags governing the behavior during load.
        These flags are specific to each symbol library.

    HostContext - Supplies the value to store in the host context field of the
        debug symbols.

    Symbols - Supplies an optional pointer where a pointer to the symbols will
        be returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

INT
DbgpCoffLoadSymbols (
    PSTR Filename,
    IMAGE_MACHINE_TYPE MachineType,
    ULONG Flags,
    PVOID HostContext,
    PDEBUG_SYMBOLS *Symbols
    );

/*++

Routine Description:

    This routine loads debugging symbol information from the specified file.

Arguments:

    Filename - Supplies the name of the binary to load symbols from.

    MachineType - Supplies the required machine type of the image. Set to
        unknown to allow the symbol library to load a file with any machine
        type.

    Flags - Supplies a bitfield of flags governing the behavior during load.
        These flags are specific to each symbol library.

    HostContext - Supplies the value to store in the host context field of the
        debug symbols.

    Symbols - Supplies an optional pointer where a pointer to the symbols will
        be returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

INT
DbgpElfLoadSymbols (
    PSTR Filename,
    IMAGE_MACHINE_TYPE MachineType,
    ULONG Flags,
    PVOID HostContext,
    PDEBUG_SYMBOLS *Symbols
    );

/*++

Routine Description:

    This routine loads ELF debugging symbol information from the specified file.

Arguments:

    Filename - Supplies the name of the binary to load symbols from.

    MachineType - Supplies the required machine type of the image. Set to
        unknown to allow the symbol library to load a file with any machine
        type.

    Flags - Supplies a bitfield of flags governing the behavior during load.
        These flags are specific to each symbol library.

    HostContext - Supplies the value to store in the host context field of the
        debug symbols.

    Symbols - Supplies an optional pointer where a pointer to the symbols will
        be returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

//
// Lower level COFF/ELF functions called from within STABs parsing
//

BOOL
DbgpLoadCoffSymbols (
    PDEBUG_SYMBOLS Symbols,
    PSTR Filename
    );

/*++

Routine Description:

    This routine loads COFF symbols into a pre-existing set of debug symbols.

Arguments:

    Symbols - Supplies a pointer to debug symbols that are assumed to have
        already been allocated and initialized.

    Filename - Supplies the name of the file to load COFF symbols for.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

BOOL
DbgpLoadElfSymbols (
    PDEBUG_SYMBOLS Symbols,
    PSTR Filename
    );

/*++

Routine Description:

    This routine loads ELF symbols into a pre-existing set of ELF symbols.

Arguments:

    Symbols - Supplies a pointer to debug symbols that are assumed to have
        already been allocated and initialized.

    Filename - Supplies the name of the file to load ELF symbols for.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

