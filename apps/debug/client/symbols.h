/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    symbols.h

Abstract:

    This header contains definitions for the generic debugger symbol
    information.

Author:

    Evan Green 1-Jul-2012

--*/

//
// ---------------------------------------------------------------- Definitions
//

#define MAX_RANGE_STRING 32

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _STRUCTURE_MEMBER STRUCTURE_MEMBER, *PSTRUCTURE_MEMBER;
typedef struct _ENUMERATION_MEMBER ENUMERATION_MEMBER, *PENUMERATION_MEMBER;
typedef struct _DEBUG_SYMBOLS DEBUG_SYMBOLS, *PDEBUG_SYMBOLS;
typedef struct _DATA_SYMBOL DATA_SYMBOL, *PDATA_SYMBOL;
typedef struct _SOURCE_FILE_SYMBOL SOURCE_FILE_SYMBOL, *PSOURCE_FILE_SYMBOL;
typedef struct _FUNCTION_SYMBOL FUNCTION_SYMBOL, *PFUNCTION_SYMBOL;

typedef enum _DATA_TYPE_TYPE {
    DataTypeInvalid,
    DataTypeRelation,
    DataTypeNumeric,
    DataTypeStructure,
    DataTypeEnumeration,
    DataTypeFunctionPointer,
    DataTypeNumberOfTypes
} DATA_TYPE_TYPE, *PDATA_TYPE_TYPE;

typedef enum _X86_REGISTER {
    X86RegisterEax,
    X86RegisterEcx,
    X86RegisterEdx,
    X86RegisterEbx,
    X86RegisterEsp,
    X86RegisterEbp,
    X86RegisterEsi,
    X86RegisterEdi,
    X86RegisterEip,
    X86RegisterEflags,
    X86RegisterCs,
    X86RegisterSs,
    X86RegisterDs,
    X86RegisterEs,
    X86RegisterFs,
    X86RegisterGs,
    X86RegisterSt0,
    X86RegisterSt1,
    X86RegisterSt2,
    X86RegisterSt3,
    X86RegisterSt4,
    X86RegisterSt5,
    X86RegisterSt6,
    X86RegisterSt7,
    X86RegisterCtrl,
    X86RegisterStat,
    X86RegisterTag,
    X86RegisterFpcs,
    X86RegisterFpIp,
    X86RegisterFpDs,
    X86RegisterFpDo
} X86_REGISTER, *PX86_REGISTER;

typedef enum _ARM_REGISTER {
    ArmRegisterR0,
    ArmRegisterR1,
    ArmRegisterR2,
    ArmRegisterR3,
    ArmRegisterR4,
    ArmRegisterR5,
    ArmRegisterR6,
    ArmRegisterR7,
    ArmRegisterR8,
    ArmRegisterR9,
    ArmRegisterR10,
    ArmRegisterR11,
    ArmRegisterR12,
    ArmRegisterR13,
    ArmRegisterR14,
    ArmRegisterR15,
    ArmRegisterSpsr = 128,
    ArmRegisterSpsrFiq,
    ArmRegisterSpsrIrq,
    ArmRegisterSpsrAbort,
    ArmRegisterSpsrUndefined,
    ArmRegisterSpsrSvc,
    ArmRegisterR8User = 144,
    ArmRegisterR9User,
    ArmRegisterR10User,
    ArmRegisterR11User,
    ArmRegisterR12User,
    ArmRegisterR13User,
    ArmRegisterR14User,
    ArmRegisterR8Fiq,
    ArmRegisterR9Fiq,
    ArmRegisterR10Fiq,
    ArmRegisterR11Fiq,
    ArmRegisterR12Fiq,
    ArmRegisterR13Fiq,
    ArmRegisterR14Fiq,
    ArmRegisterR8Irq,
    ArmRegisterR9Irq,
    ArmRegisterR10Irq,
    ArmRegisterR11Irq,
    ArmRegisterR12Irq,
    ArmRegisterR13Irq,
    ArmRegisterR14Irq,
    ArmRegisterR8Abort,
    ArmRegisterR9Abort,
    ArmRegisterR10Abort,
    ArmRegisterR11Abort,
    ArmRegisterR12Abort,
    ArmRegisterR13Abort,
    ArmRegisterR14Abort,
    ArmRegisterR8Undefined,
    ArmRegisterR9Undefined,
    ArmRegisterR10Undefined,
    ArmRegisterR11Undefined,
    ArmRegisterR12Undefined,
    ArmRegisterR13Undefined,
    ArmRegisterR14Undefined,
    ArmRegisterR8Svc,
    ArmRegisterR9Svc,
    ArmRegisterR10Svc,
    ArmRegisterR11Svc,
    ArmRegisterR12Svc,
    ArmRegisterR13Svc,
    ArmRegisterR14Svc,
    ArmRegisterD0 = 256,
    ArmRegisterD1,
    ArmRegisterD2,
    ArmRegisterD3,
    ArmRegisterD4,
    ArmRegisterD5,
    ArmRegisterD6,
    ArmRegisterD7,
    ArmRegisterD8,
    ArmRegisterD9,
    ArmRegisterD10,
    ArmRegisterD11,
    ArmRegisterD12,
    ArmRegisterD13,
    ArmRegisterD14,
    ArmRegisterD15,
    ArmRegisterD16,
    ArmRegisterD17,
    ArmRegisterD18,
    ArmRegisterD19,
    ArmRegisterD20,
    ArmRegisterD21,
    ArmRegisterD22,
    ArmRegisterD23,
    ArmRegisterD24,
    ArmRegisterD25,
    ArmRegisterD26,
    ArmRegisterD27,
    ArmRegisterD28,
    ArmRegisterD29,
    ArmRegisterD30,
    ArmRegisterD31,
} ARM_REGISTER, *PARM_REGISTER;

typedef enum _X64_REGISTER {
    X64RegisterRax,
    X64RegisterRdx,
    X64RegisterRcx,
    X64RegisterRbx,
    X64RegisterRsi,
    X64RegisterRdi,
    X64RegisterRbp,
    X64RegisterRsp,
    X64RegisterR8,
    X64RegisterR9,
    X64RegisterR10,
    X64RegisterR11,
    X64RegisterR12,
    X64RegisterR13,
    X64RegisterR14,
    X64RegisterR15,
    X64RegisterReturnAddress,
    X64RegisterXmm0 = 17,
    X64RegisterXmm1,
    X64RegisterXmm2,
    X64RegisterXmm3,
    X64RegisterXmm4,
    X64RegisterXmm5,
    X64RegisterXmm6,
    X64RegisterXmm7,
    X64RegisterXmm8,
    X64RegisterXmm9,
    X64RegisterXmm10,
    X64RegisterXmm11,
    X64RegisterXmm12,
    X64RegisterXmm13,
    X64RegisterXmm14,
    X64RegisterXmm15,
    X64RegisterSt0 = 33,
    X64RegisterSt1,
    X64RegisterSt2,
    X64RegisterSt3,
    X64RegisterSt4,
    X64RegisterSt5,
    X64RegisterSt6,
    X64RegisterSt7,
    X64RegisterMm0 = 41,
    X64RegisterMm1,
    X64RegisterMm2,
    X64RegisterMm3,
    X64RegisterMm4,
    X64RegisterMm5,
    X64RegisterMm6,
    X64RegisterMm7,
    X64RegisterRflags = 49,
    X64RegisterEs = 50,
    X64RegisterCs,
    X64RegisterSs,
    X64RegisterDs,
    X64RegisterFs,
    X64RegisterGs,
    X64RegisterFsBase = 58,
    X64RegisterGsBase = 59,
    X64RegisterTr = 62,
    X64RegisterLdtr = 63,
    X64RegisterMxcsr = 64,
    X64RegisterFcw = 65,
    X64RegisterFsw = 66,
    X64RegisterXmm16 = 67,
    X64RegisterXmm17,
    X64RegisterXmm18,
    X64RegisterXmm19,
    X64RegisterXmm20,
    X64RegisterXmm21,
    X64RegisterXmm22,
    X64RegisterXmm23,
    X64RegisterXmm24,
    X64RegisterXmm25,
    X64RegisterXmm26,
    X64RegisterXmm27,
    X64RegisterXmm28,
    X64RegisterXmm29,
    X64RegisterXmm30,
    X64RegisterXmm31,
    X64RegisterK0 = 118,
    X64RegisterK1,
    X64RegisterK2,
    X64RegisterK3,
    X64RegisterK4,
    X64RegisterK5,
    X64RegisterK6,
    X64RegisterK7,
    X64RegisterBnd0 = 126,
    X64RegisterBnd1,
    X64RegisterBnd2,
    X64RegisterBnd3,
} X64_REGISTER, *PX64_REGISTER;

typedef enum _DATA_SYMBOL_LOCATION_TYPE {
    DataLocationInvalid,
    DataLocationRegister,
    DataLocationIndirect,
    DataLocationAbsoluteAddress,
    DataLocationComplex
} DATA_SYMBOL_LOCATION_TYPE, *PDATA_SYMBOL_LOCATION_TYPE;

typedef enum _SYMBOL_RESULT_TYPE {
    SymbolResultInvalid,
    SymbolResultFunction,
    SymbolResultType,
    SymbolResultData
} SYMBOL_RESULT_TYPE, *PSYMBOL_RESULT_TYPE;

//
// Symbol interface function types.
//

typedef
INT
(*PSYMBOLS_LOAD) (
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

typedef
VOID
(*PSYMBOLS_UNLOAD) (
    PDEBUG_SYMBOLS Symbols
    );

/*++

Routine Description:

    This routine frees all memory associated with an instance of debugging
    symbols, including the symbols structure itsefl.

Arguments:

    Symbols - Supplies a pointer to the debugging symbols.

Return Value:

    None.

--*/

typedef
INT
(*PSYMBOLS_STACK_UNWIND) (
    PDEBUG_SYMBOLS Symbols,
    ULONGLONG DebasedPc,
    PSTACK_FRAME Frame
    );

/*++

Routine Description:

    This routine attempts to unwind the stack by one frame.

Arguments:

    Symbols - Supplies a pointer to the debug symbols.

    DebasedPc - Supplies the program counter value, assuming the image were
        loaded at its preferred base address (that is, actual PC minus loaded
        base difference of the module).

    Frame - Supplies a pointer where the basic frame information for this
        frame will be returned.

Return Value:

    0 on success.

    EOF if there are no more stack frames.

    Returns an error code on failure.

--*/

typedef
INT
(*PSYMBOLS_READ_DATA_SYMBOL) (
    PDEBUG_SYMBOLS Symbols,
    PDATA_SYMBOL Symbol,
    ULONGLONG DebasedPc,
    PVOID Data,
    ULONG DataSize,
    PSTR Location,
    ULONG LocationSize
    );

/*++

Routine Description:

    This routine reads the contents of a data symbol.

Arguments:

    Symbols - Supplies a pointer to the debug symbols.

    Symbol - Supplies a pointer to the data symbol to read.

    DebasedPc - Supplies the program counter value, assuming the image were
        loaded at its preferred base address (that is, actual PC minus the
        loaded base difference of the module).

    Data - Supplies a pointer to the buffer where the symbol data will be
        returned on success.

    DataSize - Supplies the size of the data buffer in bytes.

    Location - Supplies a pointer where the symbol location will be described
        in text on success.

    LocationSize - Supplies the size of the location buffer in bytes.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

typedef
INT
(*PSYMBOLS_GET_ADDRESS_OF_DATA_SYMBOL) (
    PDEBUG_SYMBOLS Symbols,
    PDATA_SYMBOL Symbol,
    ULONGLONG DebasedPc,
    PULONGLONG Address
    );

/*++

Routine Description:

    This routine gets the memory address of a data symbol.

Arguments:

    Symbols - Supplies a pointer to the debug symbols.

    Symbol - Supplies a pointer to the data symbol to read.

    DebasedPc - Supplies the program counter value, assuming the image were
        loaded at its preferred base address (that is, actual PC minus the
        loaded base difference of the module).

    Address - Supplies a pointer where the address of the data symbol will be
        returned on success.

Return Value:

    0 on success.

    ENOENT if the data symbol is not currently valid.

    ERANGE if the data symbol is not stored in memory.

    Other error codes on other failures.

--*/

typedef
BOOL
(*PSYMBOLS_CHECK_RANGE) (
    PDEBUG_SYMBOLS Symbols,
    PSOURCE_FILE_SYMBOL Source,
    ULONGLONG Address,
    PVOID Ranges
    );

/*++

Routine Description:

    This routine determines whether the given address is actually in range of
    the given ranges. This is used for things like inline functions that have
    several discontiguous address ranges.

Arguments:

    Symbols - Supplies a pointer to the debug symbols.

    Source - Supplies a pointer to the compilation unit the given object is in.

    Address - Supplies the address to query.

    Ranges - Supplies the opaque pointer to the range list information.

Return Value:

    TRUE if the address is within the range list for the object.

    FALSE if the address is not within the range list for the object.

--*/

/*++

Structure Description:

    This structure defines the interface to a symbol parsing library.

Members:

    Load - Stores a pointer to a function that loads symbols.

    Unload - Stores a pointer to a function that unloads loaded symbols.

    Unwind - Stores an optional pointer to a function that can unwind the
        target stack. If not supplied, then traditional frame chaining will be
        used.

    ReadDataSymbol - Stores an optional pointer to a function that can read
        a data symbol value.

    GetAddressOfDataSymbol - Stores an optional pointer to a function that
        can return the memory address of a data symbol.

    CheckRange - Stores an optional pointer to a function used to determine if
        an address is within a given discontiguous range for a function or
        module.

--*/

typedef struct _DEBUG_SYMBOL_INTERFACE {
    PSYMBOLS_LOAD Load;
    PSYMBOLS_UNLOAD Unload;
    PSYMBOLS_STACK_UNWIND Unwind;
    PSYMBOLS_READ_DATA_SYMBOL ReadDataSymbol;
    PSYMBOLS_GET_ADDRESS_OF_DATA_SYMBOL GetAddressOfDataSymbol;
    PSYMBOLS_CHECK_RANGE CheckRange;
} DEBUG_SYMBOL_INTERFACE, *PDEBUG_SYMBOL_INTERFACE;

/*++

Structure Description:

    This structure holds internal information pertaining to a loaded module's
    symbols. It stores all symbol information for a given module.

Members:

    Filename - Stores the file name of the current module. This will most likely
        point to the RawStabStrings buffer, and will not need to be freed
        explicitly.

    ImageBase - Stores the default base of the image.

    Machine - Stores the machine architecture of the file.

    ImageFormat - Stores the image format of the file.

    RawSymbolTable - Stores a pointer to a buffer containing the symbol table
        out of the PE or ELF file.

    RawSymbolTableSize - Stores the size of the RawSymbolTable buffer, in bytes.

    RawSymbolTableStrings - Stores a pointer to a buffer containing the string
        table associated with the symbol table in the PE or ELF file.

    RawSymbolTableStringsSize - Stores the size of the RawSymbolTableStrings,
        in bytes.

    SourcesHead - Stores the list head for a linked list of SOURCE_FILE_SYMBOL
        structures. This list contains the symbols for all the source files in
        the image.

    SymbolContext - Stores an opaque pointer that the symbol parsing library
        can use to store global state for this image.

    Interface - Stores a pointer to a table of functions used to interact with
        the symbol library.

    HostContext - Stores a pointer's worth of context for the user of the
        debug symbols library. This currently holds a pointer back to the
        debugger context.

    RegistersContext - Stores an optional pointer's worth of context regarding
        which set of registers to access when the symbol library needs to do
        accesses.

--*/

struct _DEBUG_SYMBOLS {
    PSTR Filename;
    ULONGLONG ImageBase;
    ULONG Machine;
    IMAGE_FORMAT ImageFormat;
    LIST_ENTRY SourcesHead;
    PVOID SymbolContext;
    PDEBUG_SYMBOL_INTERFACE Interface;
    PVOID HostContext;
    PVOID RegistersContext;
};

/*++

Structure Description:

    This structure holds a subrange. This is used in type definitions where a
    type will be defined as a subrange of another type. It's also used in array
    definitions, specifying the minimum and maximum index in the array.

Members:

    Minimum - Stores the minimum value of the range, inclusive.

    Maximum - Stores the maximum value of the range.

    MaxUlonglong - Stores a boolean indicating if the actual maximum of the
        range is maximum value of a 64-bit unsigned integer. If this flag is
        set, the Maximum field is undefined.

--*/

typedef struct _DATA_RANGE {
    LONGLONG Minimum;
    LONGLONG Maximum;
    BOOL MaxUlonglong;
} DATA_RANGE, *PDATA_RANGE;

/*++

Structure Description:

    This structure stores all of the debug symbols for one source file.

Members:

    SourceDirectory - Stores a string of the sources complete directory path.
        This will not need to be freed explicitly if it points directly to a
        stab string.

    SourceFile - Stores a string of the source file name. This will also not
        need to be freed explicitly.

    ListEntry - Stores links to the next and previous source files in the image.

    TypesHead - Stores the list head for all the types defined by this file. The
        values for these list entries will be TYPE_SYMBOL structures.

    SourceLinesHead - Stores the list head for all the source line symbols
        defined in this file. These entries will be of type SOURCE_LINE_SYMBOL.

    FunctionsHead - Stores the list head for all the functions defined in this
        file. These entries will be of type FUNCTION_SYMBOL.

    DataSymbolsHead - Stores the list head for all the data symbols defined in
        this source file. These entries will be of type DATA_SYMBOL.

    StartAddress - Stores the virtual address of the start of the text section
        for this source file. This makes it easy narrow down which file a
        symbol is in.

    EndAddress - Stores the virtual address of the end of the text section for
        this source file.

    Identifier - Stores an identifier for the source file, used to match up
        future references to the file. For Stabs, this is the value of the
        stab, and is used to match N_EXCL references to N_BINCLs.

    SymbolContext - Stores a pointer's worth of context reserved for the symbol
        parsing library.

--*/

struct _SOURCE_FILE_SYMBOL {
    PSTR SourceDirectory;
    PSTR SourceFile;
    LIST_ENTRY ListEntry;
    LIST_ENTRY TypesHead;
    LIST_ENTRY SourceLinesHead;
    LIST_ENTRY FunctionsHead;
    LIST_ENTRY DataSymbolsHead;
    ULONGLONG StartAddress;
    ULONGLONG EndAddress;
    ULONG Identifier;
    PVOID SymbolContext;
};

/*++

Structure Description:

    This structure stores symbols information pertaining to a function.

Members:

    ParentSource - Stores a pointer to the source file this function is defined
        in.

    Name - Stores a pointer to the name of the function. This buffer will need
        to be freed explicitly on destruction.

    FunctionNumber - Stores the function number, as referred to by the stab
        Description field. This information is stored but currently unused.

    ListEntry - Stores links to the next and previous functions in the owning
        source file.

    ParametersHead - Stores the head of the list of the function's parameters,
        in order. The list values will be of type DATA_SYMBOL.

    LocalsHead - Stores the head of the list of the function's local variables.
        The list will be of type DATA_SYMBOL.

    FunctionsHead - Stores the head of the list of the function's subfunctions
        (often inlined functions).

    StartAddress - Stores the starting virtual address of the function.

    EndAddress - Stores the ending virtual address of the function, exclusive.

    Ranges - Stores an opaque pointer that is passed in to the check range
        function to determine if the given address is in range.

    ReturnTypeNumber - Stores the type number of the function's return type.

    ReturnTypeOwner - Stores a pointer to the source file where the function's
        return type resides.

    SymbolContext - Stores a pointer's worth of additional context for the
        symbol library.

    ParentFunction - Stores a pointer to the parent function if this is an
        inner or inlined function.

--*/

struct _FUNCTION_SYMBOL {
    PSOURCE_FILE_SYMBOL ParentSource;
    PSTR Name;
    USHORT FunctionNumber;
    LIST_ENTRY ListEntry;
    LIST_ENTRY ParametersHead;
    LIST_ENTRY LocalsHead;
    LIST_ENTRY FunctionsHead;
    ULONGLONG StartAddress;
    ULONGLONG EndAddress;
    PVOID Ranges;
    LONG ReturnTypeNumber;
    PSOURCE_FILE_SYMBOL ReturnTypeOwner;
    PVOID SymbolContext;
    PFUNCTION_SYMBOL ParentFunction;
};

/*++

Structure Description:

    This structure stores a single source line symbol.

Members:

    ParentSource - Stores a pointer to the source file that this line refers to.
        This could point to an include file.

    ListEntry - Stores links to the previous and next source lines in this
        source file.

    LineNumber - Stores the line number of this source line symbol.

    Start - Stores the starting address of this line, inclusive.

    End - Stores the ending address of this line, exclusive.

--*/

typedef struct _SOURCE_LINE_SYMBOL {
    PSOURCE_FILE_SYMBOL ParentSource;
    LIST_ENTRY ListEntry;
    LONG LineNumber;
    ULONGLONG Start;
    ULONGLONG End;
} SOURCE_LINE_SYMBOL, *PSOURCE_LINE_SYMBOL;

/*++

Structure Description:

    This structure defines a relation type between the type being defined and
    another type.

Members:

    Pointer - Stores a combination of a flag and a value. If zero, it indicates
        this relation is not a pointer. If non-zero it indicates both that this
        relation is a pointer type, and the size of a pointer on the machine.

    OwningFile - Stores a pointer to the source file that contains the
        reference type.

    TypeNumber - Stores the number of the reference type.

    Array - Stores the allowable array indices of this type. If any of the
        values inside this parameter are nonzero, this indicates that this type
        is an array of the reference type.

    Function - Stores a flag which is set when this type is a function. The type
        information then refers to the return type of the function.

--*/

typedef struct _DATA_TYPE_RELATION {
    UCHAR Pointer;
    PSOURCE_FILE_SYMBOL OwningFile;
    LONG TypeNumber;
    DATA_RANGE Array;
    BOOL Function;
} DATA_TYPE_RELATION, *PDATA_TYPE_RELATION;

/*++

Structure Description:

    This structure defines a numeric type.

Members:

    Signed - Stores a flag indicating whether this type is signed or unsigned.

    Float - Stores a flag indicating whether this type should be interpreted as
        a floating point number. If this flag is TRUE, the Signed member is
        meaningless.

    BitSize - Stores the size of the numeric type, in bits.

--*/

typedef struct _DATA_TYPE_NUMERIC {
    BOOL Signed;
    BOOL Float;
    ULONG BitSize;
} DATA_TYPE_NUMERIC, *PDATA_TYPE_NUMERIC;

/*++

Structure Description:

    This structure defines a structure type (ie. the source file defined a
        structure of some sort).

Members:

    SizeInBytes - Stores the total size of the structure, in bytes.

    MemberCount - Stores the number of members in this structure.

    FirstMember - Stores a pointer to the first structure member.

--*/

typedef struct _DATA_TYPE_STRUCTURE {
    ULONG SizeInBytes;
    ULONG MemberCount;
    PSTRUCTURE_MEMBER FirstMember;
} DATA_TYPE_STRUCTURE, *PDATA_TYPE_STRUCTURE;

/*++

Structure Description:

    This structure defines an enumeration type.

Members:

    SizeInBytes - Stores the number of bytes required to hold an instantiation
        of this enumeration. This might be zero if the symbol format does not
        describe this information.

    MemberCount - Stores the number of values defined in this enum.

    FirstMember - Stores a pointer to the first enumeration definition.

--*/

typedef struct _DATA_TYPE_ENUMERATION {
    ULONG SizeInBytes;
    ULONG MemberCount;
    PENUMERATION_MEMBER FirstMember;
} DATA_TYPE_ENUMERATION, *PDATA_TYPE_ENUMERATION;

/*++

Structure Description:

    This structure defines a function pointer type.

Members:

    SizeInBytes - Stores the size of the type (the size of an address in the
        target).

--*/

typedef struct _DATA_TYPE_FUNCTION_POINTER {
    ULONG SizeInBytes;
} DATA_TYPE_FUNCTION_POINTER, *PDATA_TYPE_FUNCTION_POINTER;

/*++

Structure Description:

    This structure defines a new type (such as a bool, int, structure, or enum).

Members:

    ListEntry - Stores links to the next and previous types in the owning source
        file.

    ParentSource - Stores a link to the source file this type was defined in.
        This is necessary because types are defined with a type index and
        potentially an include file index. This could be an include file.

    TypeNumber - Stores the type number, which can be referred to by other
        types.

    Name - Stores the name of the type. This buffer will need to be freed
        explicitly upon destruction.

    ParentFunction - Stores a link to the function where this type was defined.

    Type - Stores the type of this type, such as whether it is a basic type,
        structure, enum, etc.

    U - Stores the union of type information. Which structure to reach through
        can be determined by the type member above.

--*/

struct _TYPE_SYMBOL {
    LIST_ENTRY ListEntry;
    PSOURCE_FILE_SYMBOL ParentSource;
    LONG TypeNumber;
    PSTR Name;
    PFUNCTION_SYMBOL ParentFunction;
    DATA_TYPE_TYPE Type;
    union {
        DATA_TYPE_RELATION Relation;
        DATA_TYPE_NUMERIC Numeric;
        DATA_TYPE_STRUCTURE Structure;
        DATA_TYPE_ENUMERATION Enumeration;
        DATA_TYPE_FUNCTION_POINTER FunctionPointer;
    } U;

};

/*++

Structure Description:

    This structure defines a data address that is a register plus an offset.

Members:

    Register - Stores the register number.

    Offset - Stores the offset in bytes to add to the value at the register.

--*/

typedef struct _DATA_LOCATION_REGISTER_OFFSET {
    ULONG Register;
    LONGLONG Offset;
} DATA_LOCATION_REGISTER_OFFSET, *PDATA_LOCATION_REGISTER_OFFSET;

/*++

Structure Description:

    This union defines the various forms a data symbol location can take.

Members:

    Address - Stores the memory address of the symbol.

    Register - Stores the register number of the symbol.

    Indirect - Stores the register plus offset address of the symbol.

    Complex - Stores a context pointer that the symbol library can interpret
        to evaluate a more complicated location.

--*/

typedef union _DATA_LOCATION_UNION {
    ULONGLONG Address;
    ULONG Register;
    DATA_LOCATION_REGISTER_OFFSET Indirect;
    PVOID Complex;
} DATA_LOCATION_UNION, *PDATA_LOCATION_UNION;

/*++

Structure Description:

    This structure defines a general data symbol for a global or local variable
    located in a register, stack, or at an absolute address.

Members:

    ParentSource - Stores a link to the source file where this symbol was
        defined.

    ParentFunction - Stores a link to the function where this (potentially
        local) variable was defined. Can be NULL.

    ListEntry - Stores a link to the next and previous variable in the function
        or source file.

    Name - Stores a pointer to the name of this variable. This buffer will need
        to be explicitly freed upon destruction.

    LocationType - Stores a value that indicates the form the location union
        should be accessed through.

    Location - Stores the location of the symbol.

    MinimumValidExecutionAddress - Stores the point in the execution flow when
        this variable becomes active. For globals, this will probably be 0. For
        stack variables and register variables, this will be somewhere around
        where the variable gets initialized.

    TypeOwner - Stores a link to the source file where the type of the variable
        can be found.

    TypeNumber - Stores the type number of this variable.

--*/

struct _DATA_SYMBOL {
    PSOURCE_FILE_SYMBOL ParentSource;
    PFUNCTION_SYMBOL ParentFunction;
    LIST_ENTRY ListEntry;
    PSTR Name;
    DATA_SYMBOL_LOCATION_TYPE LocationType;
    DATA_LOCATION_UNION Location;
    ULONGLONG MinimumValidExecutionAddress;
    PSOURCE_FILE_SYMBOL TypeOwner;
    LONG TypeNumber;
};

/*++

Structure Description:

    This structure defines a member in a structure type definition.

Members:

    Name - Stores a pointer to the name of this member. This buffer *will* need
        to be freed explicitly on destruction.

    TypeFile - Stores a pointer to the file where the type of this member is
        defined.

    TypeNumber - Stores the type number for this structure member.

    BitOffset - Stores the offset from the beginning of the structure where this
        member begins, in bits. For unions, many members will have the same
        value here.

    BitSize - Stores the size of this member in bits.

    NextMember - Stores a pointer to the next structure member, or NULL if this
        is the last structure member.

--*/

struct _STRUCTURE_MEMBER {
    PSTR Name;
    PSOURCE_FILE_SYMBOL TypeFile;
    LONG TypeNumber;
    ULONG BitOffset;
    ULONG BitSize;
    PSTRUCTURE_MEMBER NextMember;
};

/*++

Structure Description:

    This structure defines a member in an enumeration type definition.

Members:

    Name - Stores a pointer to the name of this enumeration. This buffer *will*
        need to be explicitly freed on destruction.

    Value - Stores the value that Name enumerates to. On a normal enumeration,
        this value will start at 0 and work its way up in subsequent members.

    NextMember - Stores a pointer to the next enumeration member, or NULL if
        this is the last enumeration.

--*/

struct _ENUMERATION_MEMBER {
    PSTR Name;
    LONGLONG Value;
    PENUMERATION_MEMBER NextMember;
};

/*++

Structure Description:

    This structure defines an individual result of searching for a symbol.

Members:

    Variety - Stores which member of the union of the union is valid,
        depending on the type of symbol returned from the query.

    FunctionResult - Stores a pointer to the function symbol,
        provided that the variety specifies a function.

    TypeResult - Stores a pointer to the data type symbol, provided that
        the variety specifies a type.

    DataResult - Stores a pointer to the data symbol, provided that the variety
        specifies a data result.

--*/

typedef struct _SYMBOL_SEARCH_RESULT {
    SYMBOL_RESULT_TYPE Variety;
    union {
        PFUNCTION_SYMBOL FunctionResult;
        PTYPE_SYMBOL TypeResult;
        PDATA_SYMBOL DataResult;
    } U;

} SYMBOL_SEARCH_RESULT, *PSYMBOL_SEARCH_RESULT;

/*++

Structure Description:

    This structure stores a loaded module in the debugger.

Members:

    ListEntry - Stores pointers to the next and previous loaded modules in the
        list of all loaded modules.

    Filename - Stores the name of the file these symbols were loaded from.

    ModuleName - Stores the friendly name of the module.

    Timestamp - Stores the modification date of this module in seconds since
        2001.

    BaseDifference - Supplies the difference between the preferred load
        address of the module and the actual load address of the module.

    LowestAddress - Stores the lowest address of the image actually in use,
        since this can be lower than the base address.

    Size - Stores the lize of the loaded image in memory.

    Process - Stores the ID of the process the image is specific to.

    Symbols - Stores a pointer to the debug symbols associated with this
        module.

    Loaded - Stores a boolean indicating if this module is still loaded.

--*/

typedef struct _DEBUGGER_MODULE {
    LIST_ENTRY ListEntry;
    PSTR Filename;
    PSTR ModuleName;
    ULONGLONG Timestamp;
    ULONGLONG BaseDifference;
    ULONGLONG LowestAddress;
    ULONGLONG Size;
    ULONG Process;
    PDEBUG_SYMBOLS Symbols;
    BOOL Loaded;
} DEBUGGER_MODULE, *PDEBUGGER_MODULE;

/*++

Structure Description:

    This structure stores a list of loaded modules.

Members:

    ModuleCount - Stores the number of modules in the list.

    Signature - Stores the total of all timestamps and loaded addresses in the
        module list.

    ModulesHead - Stores the head of the list of DEBUGGER_MODULE structures.

--*/

typedef struct _DEBUGGER_MODULE_LIST {
    ULONG ModuleCount;
    ULONGLONG Signature;
    LIST_ENTRY ModulesHead;
} DEBUGGER_MODULE_LIST, *PDEBUGGER_MODULE_LIST;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

INT
DbgLoadSymbols (
    PSTR Filename,
    IMAGE_MACHINE_TYPE MachineType,
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

    HostContext - Supplies the value to store in the host context field of the
        debug symbols.

    Symbols - Supplies an optional pointer where a pointer to the symbols will
        be returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

VOID
DbgUnloadSymbols (
    PDEBUG_SYMBOLS Symbols
    );

/*++

Routine Description:

    This routine frees all memory associated with an instance of debugging
    symbols. Once called, the pointer passed in should not be dereferenced
    again by the caller.

Arguments:

    Symbols - Supplies a pointer to the debugging symbols.

Return Value:

    None.

--*/

VOID
DbgPrintFunctionPrototype (
    PFUNCTION_SYMBOL Function,
    PSTR ModuleName,
    ULONGLONG Address
    );

/*++

Routine Description:

    This routine prints a C function prototype directly to the screen.

Arguments:

    Function - Supplies a pointer to the function symbol to print.

    ModuleName - Supplies an optional string containing the module name.

    Address - Supplies the final address of the function.

Return Value:

    None (information is printed directly to the standard output).

--*/

VOID
DbgPrintTypeName (
    PTYPE_SYMBOL Type
    );

/*++

Routine Description:

    This routine prints a type name, formatted with any array an pointer
    decorations.

Arguments:

    Type - Supplies a pointer to the type to print information about.

Return Value:

    None (information is printed directly to the standard output).

--*/

ULONG
DbgGetTypeSize (
    PTYPE_SYMBOL Type,
    ULONG RecursionDepth
    );

/*++

Routine Description:

    This routine determines the size in bytes of a given type.

Arguments:

    Type - Supplies a pointer to the type to get the size of.

    RecursionDepth - Supplies the function recursion depth. Supply zero here.

Return Value:

    Returns the size of the type in bytes. On error or on querying a void type,
    0 is returned.

--*/

VOID
DbgPrintTypeDescription (
    PTYPE_SYMBOL Type,
    ULONG SpaceLevel,
    ULONG RecursionDepth
    );

/*++

Routine Description:

    This routine prints a description of the structure of a given type.

Arguments:

    Type - Supplies a pointer to the type to print information about.

    SpaceLevel - Supplies the number of spaces to print after every newline.
        Used for nesting types.

    RecursionDepth - Supplies how many times this should recurse on structure
        members. If 0, only the name of the type is printed.

Return Value:

    None (information is printed directly to the standard output).

--*/

PTYPE_SYMBOL
DbgSkipTypedefs (
    PTYPE_SYMBOL Type
    );

/*++

Routine Description:

    This routine skips all relation types that aren't pointers or arrays.

Arguments:

    Type - Supplies a pointer to the type to get to the bottom of.

Return Value:

    NULL if the type ended up being void or not found.

    Returns a pointer to the root type on success.

--*/

PTYPE_SYMBOL
DbgGetType (
    PSOURCE_FILE_SYMBOL SourceFile,
    LONG TypeNumber
    );

/*++

Routine Description:

    This routine looks up a type symbol based on the type number and the source
    file the type is in.

Arguments:

    SourceFile - Supplies a pointer to the source file containing the type.

    TypeNumber - Supplies the type number to look up.

Return Value:

    Returns a pointer to the type on success, or NULL on error.

--*/

PSOURCE_LINE_SYMBOL
DbgLookupSourceLine (
    PDEBUG_SYMBOLS Module,
    ULONGLONG Address
    );

/*++

Routine Description:

    This routine looks up a source line in a given module based on the address.

Arguments:

    Module - Supplies a pointer to the module which contains the symbols to
        search through.

    Address - Supplies the query address to search the source line symbols for.

Return Value:

    If a successful match is found, returns a pointer to the source line symbol.
    If a source line matching the address could not be found or an error
    occured, returns NULL.

--*/

PSYMBOL_SEARCH_RESULT
DbgLookupSymbol (
    PDEBUG_SYMBOLS Module,
    ULONGLONG Address,
    PSYMBOL_SEARCH_RESULT Input
    );

/*++

Routine Description:

    This routine looks up a symbol in a module based on the given address. It
    first searches through data symbols, then functions.

Arguments:

    Module - Supplies a pointer to the module which contains the symbols to
        search through.

    Address - Supplies the address of the symbol to look up.

    Input - Supplies a pointer to the search result structure. On input, the
        parameter contains the search result to start the search from. On
        output, contains the new found search result. To signify that the search
        should start from the beginning, set the Type member to ResultInvalid.

Return Value:

    If a successful match is found, returns Input with the search results filled
    into the structure. If no result was found or an error occurred, NULL is
    returned.

--*/

PSYMBOL_SEARCH_RESULT
DbgpFindSymbolInModule (
    PDEBUG_SYMBOLS Module,
    PSTR Query,
    PSYMBOL_SEARCH_RESULT Input
    );

/*++

Routine Description:

    This routine searches for a symbol in a module. It first searches through
    types, then data symbols, then functions.

Arguments:

    Module - Supplies a pointer to the module which contains the symbols to
        search through.

    Query - Supplies the search string.

    Input - Supplies a pointer to the search result structure. On input, the
        parameter contains the search result to start the search from. On
        output, contains the new found search result. To signify that the search
        should start from the beginning, set the Type member to ResultInvalid.

Return Value:

    If a successful match is found, returns Input with the search results filled
    into the structure. If no result was found or an error occurred, NULL is
    returned.

--*/

PSYMBOL_SEARCH_RESULT
DbgFindTypeSymbol (
    PDEBUG_SYMBOLS Module,
    PSTR Query,
    PSYMBOL_SEARCH_RESULT Input
    );

/*++

Routine Description:

    This routine searches for a type symbol in a module.

Arguments:

    Module - Supplies a pointer to the module which contains the symbols to
        search through.

    Query - Supplies the search string.

    Input - Supplies a pointer to the search result structure. On input, the
        parameter contains the search result to start the search from. On
        output, contains the new found search result. To signify that the search
        should start from the beginning, set the Type member to ResultInvalid.

Return Value:

    If a successful match is found, returns Input with the search results filled
    into the structure. If no result was found or an error occurred, NULL is
    returned.

--*/

PSYMBOL_SEARCH_RESULT
DbgFindDataSymbol (
    PDEBUG_SYMBOLS Module,
    PSTR Query,
    ULONGLONG Address,
    PSYMBOL_SEARCH_RESULT Input
    );

/*++

Routine Description:

    This routine searches for a data symbol in a module based on a query string
    or address.

Arguments:

    Module - Supplies a pointer to the module which contains the symbols to
        search through.

    Query - Supplies the search string. This parameter can be NULL if searching
        by address.

    Address - Supplies the address of the symbol. Can be NULL if search by
        query string is desired.

    Input - Supplies a pointer to the search result structure. On input, the
        parameter contains the search result to start the search from. On
        output, contains the new found search result. To signify that the search
        should start from the beginning, set the Type member to ResultInvalid.

Return Value:

    If a successful match is found, returns Input with the search results filled
    into the structure. If no result was found or an error occurred, NULL is
    returned.

--*/

PSYMBOL_SEARCH_RESULT
DbgFindFunctionSymbol (
    PDEBUG_SYMBOLS Module,
    PSTR Query,
    ULONGLONG Address,
    PSYMBOL_SEARCH_RESULT Input
    );

/*++

Routine Description:

    This routine searches for a function symbol in a module based on a search
    string or an address.

Arguments:

    Module - Supplies a pointer to the module which contains the symbols to
        search through.

    Query - Supplies the search string. This parameter can be NULL if searching
        by address.

    Address - Supplies the search address. This parameter can be NULL if
        searching by query string.

    Input - Supplies a pointer to the search result structure. On input, the
        parameter contains the search result to start the search from. On
        output, contains the new found search result. To signify that the search
        should start from the beginning, set the Type member to ResultInvalid.

Return Value:

    If a successful match is found, returns Input with the search results filled
    into the structure. If no result was found or an error occurred, NULL is
    returned.

--*/

PSTR
DbgGetRegisterName (
    IMAGE_MACHINE_TYPE MachineType,
    ULONG Register
    );

/*++

Routine Description:

    This routine returns a string containing the name of the given register.

Arguments:

    MachineType - Supplies the machine type.

    Register - Supplies the register number.

Return Value:

    Returns a pointer to a constant string containing the name of the register.

--*/

