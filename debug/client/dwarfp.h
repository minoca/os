/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    dwarfp.h

Abstract:

    This header contains internal definitions for the DWARF symbol parser.
    This should not be included by users of the DWARF parser, it is only for
    use internally to the parser.

Author:

    Evan Green 4-Dec-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "symbols.h"
#include "dwarf.h"

//
// --------------------------------------------------------------------- Macros
//

#define DWARF_ERROR(...) DbgOut(__VA_ARGS__)
#define DWARF_PRINT(...) DbgOut(__VA_ARGS__)

//
// This macro creates an identifier for a DIE that is unique to the module.
//

#define DWARF_DIE_ID(_Context, _Die) \
    ((PVOID)((_Die)->Start) - (_Context)->Sections.Info.Data)

//
// This macro reads 4 for 32-bit sections and 8 for 64-bit sections.
//

#define DWARF_READN(_Bytes, _Is64) \
    (((_Is64) != FALSE) ? DwarfpRead8(_Bytes) : DwarfpRead4(_Bytes))

//
// This macro evaluates to non-zero if the given DWARF_FORM is a block.
//

#define DWARF_BLOCK_FORM(_Form) \
    (((_Form) == DwarfFormBlock1) || ((_Form) == DwarfFormBlock2) || \
     ((_Form) == DwarfFormBlock4) || ((_Form) == DwarfFormBlock))

//
// This macro evaluates to non-zero if the given DWARF_FORM is a section offset.
// This macro allows data4 and data8, which were used as section offsets in
// DWARF2 but not in DWARF4.
//

#define DWARF_SECTION_OFFSET_FORM(_Form, _Unit) \
    (((_Form) == DwarfFormSecOffset) || \
     (((_Unit)->Version < 4) && \
      (((_Form) == DwarfFormData4) || ((_Form) == DwarfFormData8))))

//
// ---------------------------------------------------------------- Definitions
//

//
// This flag is set if the DIE has children.
//

#define DWARF_DIE_HAS_CHILDREN 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the types underlying the LEB128. Currently this cannot represent all
// 128-bits of the value.
//

typedef ULONGLONG DWARF_LEB128;
typedef LONGLONG DWARF_SLEB128;

typedef struct _DWARF_DIE DWARF_DIE, *PDWARF_DIE;

/*++

Structure Description:

    This structure contains the parsed out header for a DWARF compilation unit.

Members:

    ListEntry - Stores pointers to the next and previous compilation units in
        the .debug_info sections.

    DieList - Stores the head of the list of child DWARF_DIE structures.

    Is64Bit - Stores a boolean indicating whether or not this compilation unit
        is 64-bit or not.

    Version - Stores the version number of the compilation unit.

    UnitLength - Stores the length of the compilation unit in bytes, not
        including the unit length itself.

    AbbreviationOffset - Stores the offset into the .debug_abbrev section,
        associating this compilation unit with a set of debugging information
        entry abbreviations.

    AddressSize - Stores the size of an address on the target architecture.
        If the system uses segmented addressing, this contains the offset port

    Start - Stores a pointer to the start of the compilation unit header.

    Dies - Stores a pointer to the Debug Information Entries.

    DiesEnd - Stores a pointer to the first byte not in the DIEs.

    LowPc - Stores the low PC value from the compile unit DIE.

    HighPc - Stores the high PC value from the compile unit DIE.

--*/

struct _DWARF_COMPILATION_UNIT {
    LIST_ENTRY ListEntry;
    LIST_ENTRY DieList;
    BOOL Is64Bit;
    USHORT Version;
    ULONGLONG UnitLength;
    ULONGLONG AbbreviationOffset;
    UCHAR AddressSize;
    PUCHAR Start;
    PUCHAR Dies;
    PUCHAR DiesEnd;
    ULONGLONG LowPc;
    ULONGLONG HighPc;
};

/*++

Structure Description:

    This structure stores the state of the DWARF parser while symbols are being
    loaded.

Members:

    CurrentUnit - Stores a pointer to the current compilation unit being
        processed.

    CurrentFile - Stores a pointer to the current source file unit.

    CurrentFunction - Stores a pointer to the current function being processed.

    CurrentType - Stores a pointer to the current type being processed.

--*/

typedef struct _DWARF_LOADING_CONTEXT {
    PDWARF_COMPILATION_UNIT CurrentUnit;
    PSOURCE_FILE_SYMBOL CurrentFile;
    PFUNCTION_SYMBOL CurrentFunction;
    PTYPE_SYMBOL CurrentType;
} DWARF_LOADING_CONTEXT, *PDWARF_LOADING_CONTEXT;

/*++

Structure Description:

    This union stores a block of data in DWARF.

Members:

    Data - Stores a pointer to the data.

    Size - Stores the size of the data in bytes.

--*/

typedef struct _DWARF_BLOCK {
    PVOID Data;
    ULONGLONG Size;
} DWARF_BLOCK, *PDWARF_BLOCK;

/*++

Structure Description:

    This union stores the value of a single DWARF attribute.

Members:

    Address - Stores a target address.

    Block - Stores a pointer to a generic region of bytes.

    UnsignedConstant - Stores an unsigned constant value.

    SignedConstant - Stores a signed constant value.

    Flag - Stores a single bit flag.

    Offset - Stores the offset from another section.

    TypeSignature - Stores the type signature.

    String - Stores a pointer to the string.

--*/

typedef union _DWARF_FORM_VALUE {
    ULONGLONG Address;
    DWARF_BLOCK Block;
    ULONGLONG UnsignedConstant;
    LONGLONG SignedConstant;
    BOOL Flag;
    ULONGLONG Offset;
    ULONGLONG TypeSignature;
    PSTR String;
} DWARF_FORM_VALUE, *PDWARF_FORM_VALUE;

/*++

Structure Description:

    This structure stores the value of a single DWARF attribute.

Members:

    Name - Stores the name of the attribute.

    Form - Stores the data format of the attribute, which tells the reader
        which union member below to look through.

    Value - Stores the value union.

--*/

typedef struct _DWARF_ATTRIBUTE_VALUE {
    DWARF_ATTRIBUTE Name;
    DWARF_FORM Form;
    DWARF_FORM_VALUE Value;
} DWARF_ATTRIBUTE_VALUE, *PDWARF_ATTRIBUTE_VALUE;

/*++

Structure Description:

    This structure contains the DWARF Debug Information Entry structure, which
    contains the union of all possible attributes.

Members:

    ListEntry - Stores pointers to the siblings of this DIE.

    ChildList - Stores the head of the list of children for this DIE.

    Parent - Stores a pointer to the parent for this DIE.

    Start - Stores a pointer to the beginning of the DIE.

    AbbreviationNumber - Stores the abbreviation number the DIE conforms to.

    Tag - Stores the top level information type for this DIE.

    Flags - Stores a bitfield of boolean attributes. See DWARF_DIE_*
        definitions.

    Depth - Stores the depth of this node in the tree.

    Attributes - Stores a pointer to the array of attributes for this DIE.

    Count - Stores the number of attributes in the array.

    Capacity - Stores the number of possible attributes in the array before
        the array needs to be resized.

--*/

struct _DWARF_DIE {
    LIST_ENTRY ListEntry;
    LIST_ENTRY ChildList;
    PDWARF_DIE Parent;
    PUCHAR Start;
    DWARF_LEB128 AbbreviationNumber;
    DWARF_TAG Tag;
    ULONG Flags;
    ULONG Depth;
    PDWARF_ATTRIBUTE_VALUE Attributes;
    UINTN Count;
    UINTN Capacity;
};

/*++

Structure Description:

    This structure stores the context saved into a data symbol such that the
    DWARF library can compute a location later.

Members:

    Unit - Stores a pointer to the compilation unit.

    LocationAttribute - Stores the location attribute for the variable.

--*/

typedef struct _DWARF_COMPLEX_DATA_SYMBOL {
    PDWARF_COMPILATION_UNIT Unit;
    DWARF_ATTRIBUTE_VALUE LocationAttribute;
} DWARF_COMPLEX_DATA_SYMBOL, *PDWARF_COMPLEX_DATA_SYMBOL;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

PSOURCE_FILE_SYMBOL
DwarfpFindSource (
    PDWARF_CONTEXT Context,
    PSTR Directory,
    PSTR FileName,
    BOOL Create
    );

/*++

Routine Description:

    This routine searches for a source file symbol matching the given directory
    and file name.

Arguments:

    Context - Supplies a pointer to the application context.

    Directory - Supplies a pointer to the source directory.

    FileName - Supplies a pointer to the source file name.

    Create - Supplies a boolean indicating if a source file should be
        created if none is found.

Return Value:

    Returns a pointer to a source file symbol on success.

    NULL if no such file exists.

--*/

//
// Read functions
//

VOID
DwarfpReadCompilationUnit (
    PUCHAR *Data,
    PULONGLONG Size,
    PDWARF_COMPILATION_UNIT Unit
    );

/*++

Routine Description:

    This routine reads a DWARF compilation unit header, and pieces it out into
    a structure.

Arguments:

    Data - Supplies a pointer that on input contains a pointer to the header.
        On output this pointer will be advanced past the header and the DIEs,
        meaning it will point at the next compilation unit.

    Size - Supplies a pointer that on input contains the size of the section.
        On output this will be decreased by the amount that the data was
        advanced.

    Unit - Supplies a pointer where the header information will be filled in.

Return Value:

    None.

--*/

INT
DwarfpLoadCompilationUnit (
    PDWARF_CONTEXT Context,
    PDWARF_COMPILATION_UNIT Unit
    );

/*++

Routine Description:

    This routine processes all the DIEs within a DWARF compilation unit.

Arguments:

    Context - Supplies a pointer to the application context.

    Unit - Supplies a pointer to the compilation unit.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

VOID
DwarfpDestroyCompilationUnit (
    PDWARF_CONTEXT Context,
    PDWARF_COMPILATION_UNIT Unit
    );

/*++

Routine Description:

    This routine destroys a compilation unit. It's assumed it's already off the
    list.

Arguments:

    Context - Supplies a pointer to the application context.

    Unit - Supplies a pointer to the compilation unit to destroy.

Return Value:

    None.

--*/

VOID
DwarfpDestroyDie (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    );

/*++

Routine Description:

    This routine destroys a Debug Information Entry.

Arguments:

    Context - Supplies a pointer to the application context.

    Die - Supplies a pointer to the DIE to destroy.

Return Value:

    None.

--*/

PSTR
DwarfpGetStringAttribute (
    PDWARF_DIE Die,
    DWARF_ATTRIBUTE Attribute
    );

/*++

Routine Description:

    This routine returns the given attribute with type string.

Arguments:

    Die - Supplies a pointer to the DIE to get the attribute from.

    Attribute - Supplies the attribute to retrieve.

Return Value:

    Returns a pointer to the string on success.

    NULL if no such attribute exists, or its type is not a string.

--*/

BOOL
DwarfpGetAddressAttribute (
    PDWARF_DIE Die,
    DWARF_ATTRIBUTE Attribute,
    PULONGLONG Address
    );

/*++

Routine Description:

    This routine returns the given attribute, ensuring it is of type address.

Arguments:

    Die - Supplies a pointer to the DIE to get the attribute from.

    Attribute - Supplies the attribute to retrieve.

    Address - Supplies a pointer where the address is returned on success.

Return Value:

    TRUE if an address was retrieved.

    FALSE if no address was retrieved or it was not of type address.

--*/

BOOL
DwarfpGetIntegerAttribute (
    PDWARF_DIE Die,
    DWARF_ATTRIBUTE Attribute,
    PULONGLONG Integer
    );

/*++

Routine Description:

    This routine returns the given attribute, ensuring it is of type integer
    (data or flag).

Arguments:

    Die - Supplies a pointer to the DIE to get the attribute from.

    Attribute - Supplies the attribute to retrieve.

    Integer - Supplies a pointer where the integer is returned on success.

Return Value:

    TRUE if an address was retrieved.

    FALSE if no address was retrieved or it was not of type address.

--*/

BOOL
DwarfpGetTypeReferenceAttribute (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die,
    DWARF_ATTRIBUTE Attribute,
    PSOURCE_FILE_SYMBOL *File,
    PLONG Identifier
    );

/*++

Routine Description:

    This routine reads a given attribute and converts that reference into a
    symbol type reference tuple.

Arguments:

    Context - Supplies a pointer to the parsing context.

    Die - Supplies a pointer to the DIE to get the attribute from.

    Attribute - Supplies the attribute to retrieve.

    File - Supplies a pointer where a pointer to the file will be returned on
        success.

    Identifier - Supplies a pointer where the type identifier will be returned
        on success.

Return Value:

    TRUE if a value was retrieved.

    FALSE if no value was retrieved or it was not of type reference.

--*/

BOOL
DwarfpGetLocalReferenceAttribute (
    PDWARF_DIE Die,
    DWARF_ATTRIBUTE Attribute,
    PULONGLONG Offset
    );

/*++

Routine Description:

    This routine returns the given attribute, ensuring it is of type reference.

Arguments:

    Die - Supplies a pointer to the DIE to get the attribute from.

    Attribute - Supplies the attribute to retrieve.

    Offset - Supplies a pointer where the DIE offset is returned on success.

Return Value:

    TRUE if a value was retrieved.

    FALSE if no value was retrieved or it was not of type reference.

--*/

BOOL
DwarfpGetGlobalReferenceAttribute (
    PDWARF_DIE Die,
    DWARF_ATTRIBUTE Attribute,
    PULONGLONG Offset
    );

/*++

Routine Description:

    This routine returns the given attribute, ensuring it is of type reference
    address.

Arguments:

    Die - Supplies a pointer to the DIE to get the attribute from.

    Attribute - Supplies the attribute to retrieve.

    Offset - Supplies a pointer where the DIE offset is returned on success.

Return Value:

    TRUE if a value was retrieved.

    FALSE if no value was retrieved or it was not of type reference.

--*/

PDWARF_ATTRIBUTE_VALUE
DwarfpGetAttribute (
    PDWARF_DIE Die,
    DWARF_ATTRIBUTE Attribute
    );

/*++

Routine Description:

    This routine returns the requested attribute from a DIE.

Arguments:

    Die - Supplies a pointer to the DIE to get the attribute from.

    Attribute - Supplies the attribute to retrieve.

Return Value:

    Returns a pointer to the attribute value on success.

    NULL if no such attribute exists.

--*/

INT
DwarfpSearchLocationList (
    PDWARF_CONTEXT Context,
    PDWARF_COMPILATION_UNIT Unit,
    UINTN Offset,
    ULONGLONG Pc,
    PUCHAR *LocationExpression,
    PUINTN LocationExpressionSize
    );

/*++

Routine Description:

    This routine searches a location list and returns the expression that
    matches the given PC value.

Arguments:

    Context - Supplies a pointer to the DWARF symbol context.

    Unit - Supplies a pointer to the current compilation unit.

    Offset - Supplies the byte offset into the location list section of the
        list to search.

    Pc - Supplies the current PC value to match against.

    LocationExpression - Supplies a pointer where a pointer to the location
        expression that matched will be returned.

    LocationExpressionSize - Supplies a pointer where the size of the location
        expression will be returned on success.

Return Value:

    0 if an expression matched.

    EAGAIN if the locations section is missing.

    ENOENT if none of the entries matched the current PC value.

--*/

DWARF_LEB128
DwarfpReadLeb128 (
    PUCHAR *Data
    );

/*++

Routine Description:

    This routine reads a DWARF unsigned LEB128 variable length encoded value.

Arguments:

    Data - Supplies a pointer that on input contains a pointer to the data.
        On output this pointer will be advanced past the value read.

Return Value:

    Returns the read value.

--*/

DWARF_SLEB128
DwarfpReadSleb128 (
    PUCHAR *Data
    );

/*++

Routine Description:

    This routine reads a DWARF signed LEB128 variable length encoded value.

Arguments:

    Data - Supplies a pointer that on input contains a pointer to the data.
        On output this pointer will be advanced past the value read.

Return Value:

    Returns the read value.

--*/

VOID
DwarfpReadInitialLength (
    PUCHAR *Data,
    PBOOL Is64Bit,
    PULONGLONG Value
    );

/*++

Routine Description:

    This routine reads an initial length member from a DWARF header. The
    initial length is either 32-bits for 32-bit sections, or 96-bits for
    64-bit sections.

Arguments:

    Data - Supplies a pointer that on input contains a pointer to the header.
        On output this pointer will be advanced past the initial length.

    Is64Bit - Supplies a pointer where a boolean will be returned indicating
        whether or not this is a 64-bit section. Most sections, even in 64-bit
        code, are not.

    Value - Supplies a pointer where the actual initial length value will be
        returned.

Return Value:

    None.

--*/

UCHAR
DwarfpRead1 (
    PUCHAR *Data
    );

/*++

Routine Description:

    This routine reads a byte from the DWARF data stream and advances the
    stream.

Arguments:

    Data - Supplies a pointer that on input contains a pointer to the data.
        On output this pointer will be advanced past the value read.

Return Value:

    Returns the read value.

--*/

USHORT
DwarfpRead2 (
    PUCHAR *Data
    );

/*++

Routine Description:

    This routine reads two bytes from the DWARF data stream and advances the
    stream.

Arguments:

    Data - Supplies a pointer that on input contains a pointer to the data.
        On output this pointer will be advanced past the value read.

Return Value:

    Returns the read value.

--*/

ULONG
DwarfpRead4 (
    PUCHAR *Data
    );

/*++

Routine Description:

    This routine reads four bytes from the DWARF data stream and advances the
    stream.

Arguments:

    Data - Supplies a pointer that on input contains a pointer to the data.
        On output this pointer will be advanced past the value read.

Return Value:

    Returns the read value.

--*/

ULONGLONG
DwarfpRead8 (
    PUCHAR *Data
    );

/*++

Routine Description:

    This routine reads eight bytes from the DWARF data stream and advances the
    stream.

Arguments:

    Data - Supplies a pointer that on input contains a pointer to the data.
        On output this pointer will be advanced past the value read.

Return Value:

    Returns the read value.

--*/

//
// Expression routines
//

INT
DwarfpGetLocation (
    PDWARF_CONTEXT Context,
    PDWARF_LOCATION_CONTEXT LocationContext,
    PDWARF_ATTRIBUTE_VALUE AttributeValue
    );

/*++

Routine Description:

    This routine evaluates a DWARF location or location list. The caller is
    responsible for calling the destroy location context routine after this
    routine runs.

Arguments:

    Context - Supplies a pointer to the context.

    LocationContext - Supplies a pointer to the location context, which is
        assumed to have been zeroed and properly filled in.

    AttributeValue - Supplies a pointer to the attribute value that contains
        the location expression.

Return Value:

    0 on success, and the final location will be returned in the location
    context.

    Returns an error number on failure.

--*/

VOID
DwarfpDestroyLocationContext (
    PDWARF_CONTEXT Context,
    PDWARF_LOCATION_CONTEXT LocationContext
    );

/*++

Routine Description:

    This routine destroys a DWARF location context.

Arguments:

    Context - Supplies a pointer to the context.

    LocationContext - Supplies a pointer to the location context to clean up.

Return Value:

    None.

--*/

VOID
DwarfpPrintExpression (
    PDWARF_CONTEXT Context,
    PDWARF_COMPILATION_UNIT Unit,
    PDWARF_ATTRIBUTE_VALUE AttributeValue
    );

/*++

Routine Description:

    This routine prints out a DWARF expression.

Arguments:

    Context - Supplies a pointer to the context.

    Unit - Supplies a pointer to the compilation unit.

    AttributeValue - Supplies a pointer to the value, which should be of type
        expression location.

Return Value:

    None.

--*/

//
// Line number functions
//

INT
DwarfpProcessStatementList (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    );

/*++

Routine Description:

    This routine is called on a compile unit DIE to process the line numbers.

Arguments:

    Context - Supplies a pointer to the DWARF context.

    Die - Supplies a pointer to the compile unit DIE.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

