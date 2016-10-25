/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dbgsym.h

Abstract:

    This header contains definitions for the higher level debugger symbol
    support.

Author:

    Evan Green 7-May-2013

--*/

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

PDEBUGGER_MODULE
DbgpFindModuleFromAddress (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    PULONGLONG DebasedAddress
    );

/*++

Routine Description:

    This routine attempts to locate a loaded module that corresponds to a
    virtual address in the target.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies an address somewhere in one of the loaded modules.

    DebasedAddress - Supplies an optional pointer where the address minus the
        loaded base difference from where the module would have preferred to
        have been loaded will be returned. This will be the address from the
        symbols' perspective.

Return Value:

    Returns a pointer to the module that the address is contained in, or NULL if
    one cannot be found.

--*/

PDEBUGGER_MODULE
DbgpGetModule (
    PDEBUGGER_CONTEXT Context,
    PSTR ModuleName,
    ULONG MaxLength
    );

/*++

Routine Description:

    This routine gets a module given the module name.

Arguments:

    Context - Supplies a pointer to the application context.

    ModuleName - Supplies a null terminated string specifying the module name.

    MaxLength - Supplies the maximum length of the module name.

Return Value:

    Returns a pointer to the module, or NULL if one could not be found.

--*/

ULONGLONG
DbgpGetFunctionStartAddress (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address
    );

/*++

Routine Description:

    This routine looks up the address for the beginning of the function given
    an address somewhere in the function.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies a virtual address somewhere inside the function.

Return Value:

    Returns the address of the first instruction of the current function, or 0
    if the funtion could not be found.

--*/

BOOL
DbgpFindSymbol (
    PDEBUGGER_CONTEXT Context,
    PSTR SearchString,
    PSYMBOL_SEARCH_RESULT SearchResult
    );

/*++

Routine Description:

    This routine searches for symbols. Wildcards are accepted. If the search
    string is preceded by "modulename!" then only that module will be searched.

Arguments:

    Context - Supplies a pointer to the application context.

    SearchString - Supplies the string to search for.

    SearchResult - Supplies a pointer that receives symbol search result data.

Return Value:

    Returns TRUE on success, or FALSE on failure.

--*/

PDEBUGGER_MODULE
DbgpFindModuleFromEntry (
    PDEBUGGER_CONTEXT Context,
    PLOADED_MODULE_ENTRY TargetEntry
    );

/*++

Routine Description:

    This routine attempts to locate a loaded module that corresponds to the
    target's description of a loaded module.

Arguments:

    Context - Supplies a pointer to the application context.

    TargetEntry - Supplies the target's module description.

Return Value:

    Returns a pointer to the existing loaded module if one exists, or NULL if
    one cannot be found.

--*/

INT
DbgpFindLocal (
    PDEBUGGER_CONTEXT Context,
    PREGISTERS_UNION Registers,
    PSTR LocalName,
    PDEBUG_SYMBOLS *ModuleSymbols,
    PDATA_SYMBOL *Local,
    PULONGLONG DebasedPc
    );

/*++

Routine Description:

    This routine searches the local variables and parameters in the function
    containing the given address for a variable matching the given name.

Arguments:

    Context - Supplies a pointer to the application context.

    Registers - Supplies a pointer to the registers to use for the search.

    LocalName - Supplies a case insensitive string of the local name.

    ModuleSymbols - Supplies a pointer where the symbols for the module will be
        returned on success.

    Local - Supplies a pointer where the local symbol will be returned on
        success.

    DebasedPc - Supplies a pointer where the PC will be returned, adjusted by
        the amount the image load was adjusted by.

Return Value:

    0 on success.

    ENOENT if no local by that name could be found.

    Returns an error number on other failures.

--*/

PDATA_SYMBOL
DbgpGetLocal (
    PFUNCTION_SYMBOL Function,
    PSTR LocalName,
    ULONGLONG ExecutionAddress
    );

/*++

Routine Description:

    This routine gets the most up to date version of a local variable symbol.

Arguments:

    Function - Supplies a pointer to the function with the desired local
        variables.

    LocalName - Supplies a case sensitive string of the local name.

    ExecutionAddress - Supplies the current execution address. This function
        will attempt to find the local variable matching the LocalName whose
        minimum execution address is as close to ExecutionAddress as possible.
        It is assumed that this address has already been de-based (the current
        base address has been subtracted from it).

Return Value:

    Returns a pointer to the local variable symbol, or NULL if one could not
    be found.

--*/

VOID
DbgpGetFriendlyName (
    PSTR FullName,
    ULONG FullNameLength,
    PSTR *NameBegin,
    PULONG NameLength
    );

/*++

Routine Description:

    This routine determines the portion of the given binary name to use as the
    friendly name.

Arguments:

    FullName - Supplies a pointer to the full path of the binary, null
        terminated.

    FullNameLength - Supplies the length of the full name buffer in bytes
        including the null terminator.

    NameBegin - Supplies a pointer where the beginning of the friendly name
        will be returned. This will point inside the full name.

    NameLength - Supplies a pointer where the number of characters in the
        friendly name will be returned, not including the null terminator.

Return Value:

    Returns TRUE on success, or FALSE on failure.

--*/

INT
DbgpPrintNumeric (
    PTYPE_SYMBOL Type,
    PVOID Data,
    UINTN DataSize
    );

/*++

Routine Description:

    This routine prints a numeric type's contents.

Arguments:

    Type - Supplies a pointer to the type symbol to print.

    Data - Supplies a pointer to the data contents.

    DataSize - Supplies the size of the data in bytes.

Return Value:

    0 on success.

    ENOBUFS if the provided buffer is not large enough to accomodate the given
    type.

--*/

INT
DbgpGetStructureMember (
    PDEBUGGER_CONTEXT Context,
    PTYPE_SYMBOL Type,
    PSTR MemberName,
    ULONGLONG Address,
    PVOID Data,
    UINTN DataSize,
    PVOID *ShiftedData,
    PUINTN ShiftedDataSize,
    PTYPE_SYMBOL *FinalType
    );

/*++

Routine Description:

    This routine returns a shifted form of the given data for accessing
    specific members of a structure.

Arguments:

    Context - Supplies a pointer to the application context.

    Type - Supplies a pointer to the structure type.

    MemberName - Supplies a pointer to the member name string, which can
        contain dot '.' notation for accessing members, and array [] notation
        for access sub-elements and dereferencing.

    Address - Supplies the address the read data is located at.

    Data - Supplies a pointer to the read in data.

    DataSize - Supplies the size of the read data in bytes.

    ShiftedData - Supplies a pointer where the shifted data will be returned on
        success. The caller is responsible for freeing this data when finished.

    ShiftedDataSize - Supplies a pointer where the size of the shifted data
        will be returned on success.

    FinalType - Supplies an optional pointer where the final type of the
        data will be returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PVOID
DbgpShiftBufferRight (
    PVOID Buffer,
    UINTN DataSize,
    UINTN Bits,
    UINTN BitSize
    );

/*++

Routine Description:

    This routine shifts a buffer right by a given number of bits. Zero bits
    will be shifted in from the left.

Arguments:

    Buffer - Supplies a pointer to the buffer contents to shift. This buffer
        will remain untouched.

    DataSize - Supplies the size of the data in bytes.

    Bits - Supplies the number of bits to shift.

    BitSize - Supplies an optional number of bits to keep after shifting, all
        others will be masked. Supply 0 to perform no masking.

Return Value:

    Returns a pointer to a newly allocated and shifted buffer on success.

    NULL on allocation failure.

--*/

