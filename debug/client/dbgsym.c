/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    dbgsym.c

Abstract:

    This module implements high level symbol support for the debugger.

Author:

    Evan Green 7-May-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "dbgrtl.h"
#include <minoca/spproto.h>
#include <minoca/im.h>
#include "symbols.h"
#include "dbgext.h"
#include "dbgapi.h"
#include "dbgrprof.h"
#include "dbgrcomm.h"
#include "dbgsym.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define a constant representing the maximum string length of an address
// symbol offset. This is equal to the length of "+0x0000000000000000".
//

#define OFFSET_MAX_LENGTH 19
#define LINE_NUMBER_STRING_LENGTH 9
#define MAX_LINE_NUMBER 99999999

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
DbgPrintAddressSymbol (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address
    )

/*++

Routine Description:

    This routine prints a descriptive version of the given address, including
    the module and function name if possible.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies the virtual address of the target to print information
        about.

Return Value:

    0 if information was successfully printed.

    Returns an error code on failure.

--*/

{

    PSTR AddressSymbol;

    AddressSymbol = DbgGetAddressSymbol(Context, Address);
    if (AddressSymbol == NULL) {
        return ENOENT;
    }

    DbgOut("%s", AddressSymbol);
    free(AddressSymbol);
    return 0;
}

PSTR
DbgGetAddressSymbol (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address
    )

/*++

Routine Description:

    This routine gets a descriptive string version of the given address,
    including the module and function name if possible. It is the caller's
    responsibility to free the returned string.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies the virtual address of the target to get information
        about.

Return Value:

    Returns a null-terminated string if successfull, or NULL on failure.

--*/

{

    PSOURCE_LINE_SYMBOL Line;
    LONG LineNumber;
    PDEBUGGER_MODULE Module;
    ULONGLONG Offset;
    PSYMBOL_SEARCH_RESULT ResultValid;
    SYMBOL_SEARCH_RESULT SearchResult;
    PSTR Symbol;
    ULONG SymbolSize;

    Line = NULL;

    //
    // Attempt to get the module this address is in. If one cannot be found,
    // then there is no useful information to print, so exit.
    //

    Module = DbgpFindModuleFromAddress(Context, Address, &Address);
    if (Module == NULL) {
        SymbolSize = OFFSET_MAX_LENGTH + sizeof(CHAR);
        Symbol = malloc(SymbolSize);
        if (Symbol == NULL) {
            return NULL;
        }

        sprintf(Symbol, "0x%08I64x", Address);
        return Symbol;
    }

    //
    // Attempt to find the current function symbol in the module.
    //

    SearchResult.Variety = SymbolResultInvalid;
    ResultValid = NULL;
    if (Module->Symbols != NULL) {
        ResultValid = DbgLookupSymbol(Module->Symbols, Address, &SearchResult);
    }

    //
    // If a symbol was found, allocate a buffer of the appropriate size and
    // print the string into that buffer.
    //

    if (ResultValid != NULL) {
        if (SearchResult.Variety == SymbolResultFunction) {
            LineNumber = 0;
            if ((Context->Flags & DEBUGGER_FLAG_PRINT_LINE_NUMBERS) != 0) {
                Line = DbgLookupSourceLine(Module->Symbols, Address);
                if (Line != NULL) {
                    LineNumber = Line->LineNumber;
                    if (LineNumber > MAX_LINE_NUMBER) {
                        LineNumber = MAX_LINE_NUMBER;
                    }
                }
            }

            //
            // Determine the size of the function string, accounting for the
            // module name, function name, the separating exclamation point
            // and the NULL terminating character.
            //

            SymbolSize = RtlStringLength(Module->ModuleName) +
                         RtlStringLength(SearchResult.U.FunctionResult->Name) +
                         (sizeof(CHAR) * 2);

            //
            // If there's a line number, also include space for a space,
            // open bracket, source file string, colon, line number, and
            // close bracket.
            //

            if (Line != NULL) {
                SymbolSize += RtlStringLength(Line->ParentSource->SourceFile) +
                              LINE_NUMBER_STRING_LENGTH +
                              (4 * sizeof(CHAR));
            }

            //
            // Add additional length if an offset needs to be appended.
            //

            Offset = Address - SearchResult.U.FunctionResult->StartAddress;
            if (Offset != 0) {
                SymbolSize += OFFSET_MAX_LENGTH;
            }

            Symbol = malloc(SymbolSize);
            if (Symbol == NULL) {
                return NULL;
            }

            if (Offset != 0) {
                if (Line != NULL) {
                    snprintf(Symbol,
                             SymbolSize,
                             "%s!%s+0x%I64x [%s:%d]",
                             Module->ModuleName,
                             SearchResult.U.FunctionResult->Name,
                             Offset,
                             Line->ParentSource->SourceFile,
                             LineNumber);

                } else {
                    snprintf(Symbol,
                             SymbolSize,
                             "%s!%s+0x%I64x",
                             Module->ModuleName,
                             SearchResult.U.FunctionResult->Name,
                             Offset);
                }

            } else {
                if (Line != NULL) {
                    snprintf(Symbol,
                             SymbolSize,
                             "%s!%s [%s:%d]",
                             Module->ModuleName,
                             SearchResult.U.FunctionResult->Name,
                             Line->ParentSource->SourceFile,
                             LineNumber);

                } else {
                    snprintf(Symbol,
                             SymbolSize,
                             "%s!%s",
                             Module->ModuleName,
                             SearchResult.U.FunctionResult->Name);

                }
            }

            return Symbol;

        } else if (SearchResult.Variety == SymbolResultData) {

            //
            // Determine the size of the data string, accounting for the
            // module name, data name, the separating exclamation pointer and
            // the NULL terminating character.
            //

            SymbolSize = RtlStringLength(Module->ModuleName) +
                         RtlStringLength(SearchResult.U.DataResult->Name) +
                         (sizeof(CHAR) * 2);

            Symbol = malloc(SymbolSize);
            if (Symbol == NULL) {
                return NULL;
            }

            sprintf(Symbol,
                    "%s!%s",
                    Module->ModuleName,
                    SearchResult.U.DataResult->Name);

            return Symbol;

        } else {
            return NULL;
        }
    }

    //
    // If a symbol was not found, then create a string based on the module name
    // and the module offset. Allocate a string accounting for the module name,
    // offset length, and NULL terminating character.
    //

    SymbolSize = RtlStringLength(Module->ModuleName) +
                 OFFSET_MAX_LENGTH +
                 sizeof(CHAR);

    Symbol = malloc(SymbolSize);
    if (Symbol == NULL) {
        return NULL;
    }

    Address += Module->BaseDifference;
    if (Address >= Module->BaseAddress) {
        Offset = Address - Module->BaseAddress;
        sprintf(Symbol, "%s+0x%I64x", Module->ModuleName, Offset);

    } else {
        Offset = Module->BaseAddress - Address;
        sprintf(Symbol, "%s-0x%I64x", Module->ModuleName, Offset);
    }

    return Symbol;
}

BOOL
DbgGetDataSymbolTypeInformation (
    PDATA_SYMBOL DataSymbol,
    PTYPE_SYMBOL *TypeSymbol,
    PULONG TypeSize
    )

/*++

Routine Description:

    This routine computes the type and type size of the given data symbol.

Arguments:

    DataSymbol - Supplies a pointer to the data symbol whose type and type size
        are to be calculated.

    TypeSymbol - Supplies a pointer that receives a pointer to the type symbol
        that corresponds to the given data symbol.

    TypeSize - Supplies a pointer that receives the type size of the given
        data symbol.

Return Value:

    Returns TRUE on success, or FALSE on failure.

--*/

{

    PTYPE_SYMBOL Type;

    assert(DataSymbol != NULL);
    assert(TypeSize != NULL);
    assert(TypeSymbol != NULL);

    //
    // Get the type information and size.
    //

    Type = DbgGetType(DataSymbol->TypeOwner, DataSymbol->TypeNumber);
    if (Type == NULL) {
        DbgOut("Error: Could not lookup type number for data symbol!\n"
               "Type was in file %s, symbol number %d\n",
               DataSymbol->TypeOwner->SourceFile,
               DataSymbol->TypeNumber);

        return FALSE;
    }

    *TypeSize = DbgGetTypeSize(Type, 0);
    *TypeSymbol = Type;
    return TRUE;
}

INT
DbgGetDataSymbolData (
    PDEBUGGER_CONTEXT Context,
    PDATA_SYMBOL DataSymbol,
    PVOID DataStream,
    ULONG DataStreamSize
    )

/*++

Routine Description:

    This routine returns the data contained by the given data symbol.

Arguments:

    Context - Supplies a pointer to the application context.

    DataSymbol - Supplies a pointer to the data symbol whose data is to be
        retrieved.

    DataStream - Supplies a pointer that receives the data from the data symbol.

    DataStreamSize - Supplies the size of the data stream buffer.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    ULONG BytesRead;
    ULONG Register;
    PULONG RegisterBase;
    INT Result;
    ULONGLONG TargetAddress;
    PX86_GENERAL_REGISTERS X86Registers;

    //
    // Collect the data contents for the symbol based on where it is located.
    //

    X86Registers = &(Context->FrameRegisters.X86);
    switch (DataSymbol->LocationType) {
    case DataLocationRegister:
        Register = DataSymbol->Location.Register;

        //
        // Get a pointer to the data.
        //

        switch (Context->MachineType) {
        case MACHINE_TYPE_X86:
            if ((DataStreamSize > 4) &&
                (Register != X86RegisterEax) && (Register != X86RegisterEbx)) {

                DbgOut("Error: Data symbol location was a register, but type "
                       "size was %d!\n",
                       DataStreamSize);

                DbgOut("Error: the register was %d.\n", Register);
            }

            switch (Register) {
                case X86RegisterEax:
                    *(PULONG)DataStream = (ULONG)X86Registers->Eax;
                    if (DataStreamSize > 4) {
                        *((PULONG)DataStream + 1) = (ULONG)X86Registers->Edx;
                    }

                    break;

                case X86RegisterEbx:
                    *(PULONG)DataStream = (ULONG)X86Registers->Ebx;
                    if (DataStreamSize > 4) {
                        *((PULONG)DataStream + 1) = (ULONG)X86Registers->Ecx;
                    }

                    break;

                case X86RegisterEcx:
                    *(PULONG)DataStream = (ULONG)X86Registers->Ecx;
                    break;

                case X86RegisterEdx:
                    *(PULONG)DataStream = (ULONG)X86Registers->Edx;
                    break;

                case X86RegisterEsi:
                    *(PULONG)DataStream = (ULONG)X86Registers->Esi;
                    break;

                case X86RegisterEdi:
                    *(PULONG)DataStream = (ULONG)X86Registers->Edi;
                    break;

                case X86RegisterEbp:
                    *(PULONG)DataStream = (ULONG)X86Registers->Ebp;
                    break;

                case X86RegisterEsp:
                    *(PULONG)DataStream = (ULONG)X86Registers->Esp;
                    break;

                default:
                    DbgOut("Error: Unknown register %d.\n", Register);
                    Result = EINVAL;
                    goto GetDataSymbolDataEnd;
            }

            break;

        //
        // ARM registers. Since the registers are all in order and are named
        // r0-r15, the register number is an offset from the register base, r0.
        //

        case MACHINE_TYPE_ARMV7:
        case MACHINE_TYPE_ARMV6:
            if (Register > 16) {
                DbgOut("Error: Unknown register %d.\n", Register);
                Result = EINVAL;
                goto GetDataSymbolDataEnd;
            }

            RegisterBase = &(Context->FrameRegisters.Arm.R0);
            *(PULONG)DataStream = *(PULONG)(RegisterBase + Register);
            if (DataStreamSize > 4) {
                *((PULONG)DataStream + 1) =
                                      *(PULONG)(RegisterBase + (Register + 1));
            }

            break;

        //
        // Unknown machine type.
        //

        default:
            DbgOut("Error: Unknown machine type %d.\n", Context->MachineType);
            Result = EINVAL;
            break;
        }

        break;

    case DataLocationIndirect:

        //
        // Get the target virtual address and attempt to read from the debuggee.
        //

        TargetAddress = DbgGetRegister(Context,
                                       &(Context->FrameRegisters),
                                       DataSymbol->Location.Indirect.Register);

        TargetAddress += DataSymbol->Location.Indirect.Offset;
        Result = DbgReadMemory(Context,
                               TRUE,
                               TargetAddress,
                               DataStreamSize,
                               DataStream,
                               &BytesRead);

        if ((Result != 0) || (BytesRead != DataStreamSize)) {
            if (Result == 0) {
                Result = EINVAL;
            }

            DbgOut("Error: Type is %d bytes large, but only %d bytes could be "
                   "read from the target!\n",
                   DataStreamSize,
                   BytesRead);

            goto GetDataSymbolDataEnd;
        }

        break;

    case DataLocationAbsoluteAddress:
        TargetAddress = DataSymbol->Location.Address;
        Result = DbgReadMemory(Context,
                               TRUE,
                               TargetAddress,
                               DataStreamSize,
                               DataStream,
                               &BytesRead);

        if ((Result != 0) || (BytesRead != DataStreamSize)) {
            if (Result == 0) {
                Result = EINVAL;
            }

            DbgOut("Error: Type is %d bytes large, but only %d bytes could be "
                   "read from the target!\n",
                   DataStreamSize,
                   BytesRead);

            goto GetDataSymbolDataEnd;
        }

        break;

    default:
        DbgOut("Error: Unknown data symbol location %d.\n",
               DataSymbol->LocationType);

        Result = EINVAL;
        goto GetDataSymbolDataEnd;
    }

    Result = 0;

GetDataSymbolDataEnd:
    return Result;
}

INT
DbgPrintDataSymbol (
    PDEBUGGER_CONTEXT Context,
    PDATA_SYMBOL DataSymbol,
    ULONG SpaceLevel,
    ULONG RecursionDepth
    )

/*++

Routine Description:

    This routine prints the location and value of a data symbol.

Arguments:

    Context - Supplies a pointer to the application context.

    DataSymbol - Supplies a pointer to the data symbol to print.

    SpaceLevel - Supplies the number of spaces to print after every newline.
        Used for nesting types.

    RecursionDepth - Supplies how many times this should recurse on structure
        members. If 0, only the name of the type is printed.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PVOID DataStream;
    INT Result;
    PSTR StackRegister;
    PTYPE_SYMBOL Type;
    ULONG TypeSize;

    DataStream = NULL;
    StackRegister = NULL;

    assert(Context->CurrentEvent.Type == DebuggerEventBreak);

    Result = DbgGetDataSymbolTypeInformation(DataSymbol, &Type, &TypeSize);
    if (Result == FALSE) {
        Result = 0;
        goto PrintDataSymbolEnd;
    }

    //
    // Allocate and get the data stream.
    //

    DataStream = malloc(TypeSize);
    if (DataStream == NULL) {
        Result = ENOMEM;
        goto PrintDataSymbolEnd;
    }

    Result = DbgGetDataSymbolData(Context, DataSymbol, DataStream, TypeSize);
    if (Result != 0) {
        DbgOut("Error: unable to get data for data symbol %s\n",
               DataSymbol->Name);

        goto PrintDataSymbolEnd;
    }

    //
    // Depending on where the symbol is, print out its location, name and
    // contents.
    //

    switch (DataSymbol->LocationType) {
    case DataLocationRegister:

        //
        // Print the register name and get a pointer to the data.
        //

        switch (Context->MachineType) {
        case MACHINE_TYPE_X86:
            switch (DataSymbol->Location.Register) {
                case X86RegisterEax:
                    DbgOut("@eax");
                    break;

                case X86RegisterEbx:
                    DbgOut("@ebx");
                    break;

                case X86RegisterEcx:
                    DbgOut("@ecx");
                    break;

                case X86RegisterEdx:
                    DbgOut("@edx");
                    break;

                case X86RegisterEsi:
                    DbgOut("@esi");
                    break;

                case X86RegisterEdi:
                    DbgOut("@edi");
                    break;

                case X86RegisterEbp:
                    DbgOut("@ebp");
                    break;

                case X86RegisterEsp:
                    DbgOut("@esp");
                    break;

                default:
                    goto PrintDataSymbolEnd;
            }

            break;

        //
        // ARM registers. Since the registers are all in order and are named
        // r0-r15, do a direct translation from register number to register.
        //

        case MACHINE_TYPE_ARMV7:
        case MACHINE_TYPE_ARMV6:
            DbgOut("@r%d", DataSymbol->Location.Register);
            break;

        //
        // Unknown machine type.
        //

        default:
            break;
        }

        DbgOut("     ");
        break;

    case DataLocationIndirect:

        //
        // Determine what the stack register should be.
        //

        if (Context->MachineType == MACHINE_TYPE_X86) {
            StackRegister = "@ebp";

        } else if ((Context->MachineType == MACHINE_TYPE_ARMV7) ||
                   (Context->MachineType == MACHINE_TYPE_ARMV6)) {

            StackRegister = "@fp";
        }

        //
        // Print the stack offset. The -4 in the format specifier specifies that
        // the field width should be 4 characters, left justified. Ideally the
        // format specifier would be "%+-4x", so that a sign would be
        // unconditionally printed, but that doesn't seem to work.
        // TODO: This should honor the Indirect.Register.
        //

        if (DataSymbol->Location.Indirect.Offset >= 0) {
            DbgOut("%s+%-4I64x",
                   StackRegister,
                   DataSymbol->Location.Indirect.Offset);

        } else {
            DbgOut("%s-%-4I64x",
                   StackRegister,
                   -DataSymbol->Location.Indirect.Offset);
        }

        break;

    case DataLocationAbsoluteAddress:
        break;

    default:
        goto PrintDataSymbolEnd;
    }

    //
    // Print the symbol name.
    //

    DbgOut("%-20s: ", DataSymbol->Name);

    //
    // Print the type contents.
    //

    DbgPrintTypeContents(DataStream, Type, SpaceLevel, RecursionDepth);
    Result = 0;

PrintDataSymbolEnd:
    if (DataStream != NULL) {
        free(DataStream);
    }

    return Result;
}

ULONGLONG
DbgGetRegister (
    PDEBUGGER_CONTEXT Context,
    PREGISTERS_UNION Registers,
    ULONG RegisterNumber
    )

/*++

Routine Description:

    This routine returns the contents of a register given a debug symbol
    register index.

Arguments:

    Context - Supplies a pointer to the application context.

    Registers - Supplies a pointer to the current machine context.

    RegisterNumber - Supplies the register index to get.

Return Value:

    Returns the register at the given index.

    -1 if the register does not exist.

--*/

{

    PULONG Registers32;
    ULONGLONG Value;

    Value = -1ULL;
    switch (Context->MachineType) {
    case MACHINE_TYPE_X86:
        switch (RegisterNumber) {
        case X86RegisterEax:
            Value = Registers->X86.Eax;
            break;

        case X86RegisterEcx:
            Value = Registers->X86.Ecx;
            break;

        case X86RegisterEdx:
            Value = Registers->X86.Edx;
            break;

        case X86RegisterEbx:
            Value = Registers->X86.Ebx;
            break;

        case X86RegisterEsp:
            Value = Registers->X86.Esp;
            break;

        case X86RegisterEbp:
            Value = Registers->X86.Ebp;
            break;

        case X86RegisterEsi:
            Value = Registers->X86.Esi;
            break;

        case X86RegisterEdi:
            Value = Registers->X86.Edi;
            break;

        case X86RegisterEip:
            Value = Registers->X86.Eip;
            break;

        case X86RegisterEflags:
            Value = Registers->X86.Eflags;
            break;

        case X86RegisterCs:
            Value = Registers->X86.Cs;
            break;

        case X86RegisterSs:
            Value = Registers->X86.Ss;
            break;

        case X86RegisterDs:
            Value = Registers->X86.Ds;
            break;

        case X86RegisterEs:
            Value = Registers->X86.Es;
            break;

        case X86RegisterFs:
            Value = Registers->X86.Fs;
            break;

        case X86RegisterGs:
            Value = Registers->X86.Gs;
            break;

        default:

            //
            // TODO: Fetch the floating point registers if not yet grabbed.
            //

            if ((RegisterNumber >= X86RegisterSt0) &&
                (RegisterNumber <= X86RegisterFpDo)) {

                DbgOut("TODO: FPU Register %d.\n", RegisterNumber);
                Value = 0;
                break;
            }

            assert(FALSE);

            break;
        }

        break;

    case MACHINE_TYPE_ARMV7:
    case MACHINE_TYPE_ARMV6:
        if ((RegisterNumber >= ArmRegisterR0) &&
            (RegisterNumber <= ArmRegisterR15)) {

            Registers32 = &(Registers->Arm.R0);
            Value = Registers32[RegisterNumber];

        } else if ((RegisterNumber >= ArmRegisterD0) &&
                   (RegisterNumber <= ArmRegisterD31)) {

            //
            // TODO: Fetch the floating point registers if not yet grabbed.
            //

            DbgOut("TODO: FPU Register D%d\n", RegisterNumber - ArmRegisterD0);
            Value = 0;

        } else {

            assert(FALSE);
        }

        break;

    default:

        assert(FALSE);

        break;
    }

    return Value;
}

PDEBUGGER_MODULE
DbgpFindModuleFromAddress (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    PULONGLONG DebasedAddress
    )

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

{

    PDEBUGGER_MODULE CurrentModule;
    PLIST_ENTRY CurrentModuleEntry;
    PDEBUGGER_MODULE FoundModule;

    FoundModule = NULL;
    CurrentModuleEntry = Context->ModuleList.ModulesHead.Next;
    while (CurrentModuleEntry != &(Context->ModuleList.ModulesHead)) {
        CurrentModule = LIST_VALUE(CurrentModuleEntry,
                                   DEBUGGER_MODULE,
                                   ListEntry);

        CurrentModuleEntry = CurrentModuleEntry->Next;
        if (!IS_MODULE_IN_CURRENT_PROCESS(Context, CurrentModule)) {
            continue;
        }

        if ((Address >= CurrentModule->LowestAddress) &&
            (Address < CurrentModule->LowestAddress + CurrentModule->Size)) {

            FoundModule = CurrentModule;
            break;
        }
    }

    if ((FoundModule != NULL) && (DebasedAddress != NULL)) {
        *DebasedAddress = Address - FoundModule->BaseDifference;
    }

    return FoundModule;
}

PDEBUGGER_MODULE
DbgpGetModule (
    PDEBUGGER_CONTEXT Context,
    PSTR ModuleName,
    ULONG MaxLength
    )

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

{

    PDEBUGGER_MODULE CurrentModule;
    PLIST_ENTRY CurrentModuleEntry;

    CurrentModuleEntry = Context->ModuleList.ModulesHead.Next;
    while (CurrentModuleEntry != &(Context->ModuleList.ModulesHead)) {
        CurrentModule = LIST_VALUE(CurrentModuleEntry,
                                   DEBUGGER_MODULE,
                                   ListEntry);

        CurrentModuleEntry = CurrentModuleEntry->Next;
        if (!IS_MODULE_IN_CURRENT_PROCESS(Context, CurrentModule)) {
            continue;
        }

        if (strncasecmp(ModuleName, CurrentModule->ModuleName, MaxLength) ==
                                                                           0) {

            return CurrentModule;
        }
    }

    return NULL;
}

ULONGLONG
DbgpGetFunctionStartAddress (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address
    )

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

{

    ULONGLONG DebasedAddress;
    ULONGLONG FunctionStart;
    PDEBUGGER_MODULE Module;
    PSYMBOL_SEARCH_RESULT ResultValid;
    SYMBOL_SEARCH_RESULT SearchResult;

    FunctionStart = 0;

    //
    // Attempt to get the module this address is in. If one cannot be found,
    // then there is no useful information to print, so exit.
    //

    Module = DbgpFindModuleFromAddress(Context, Address, &DebasedAddress);
    if (Module == NULL) {
        goto GetFunctionStartAddressEnd;
    }

    //
    // Attempt to find the current function symbol in the module.
    //

    SearchResult.Variety = SymbolResultInvalid;
    ResultValid = NULL;
    if (Module->Symbols != NULL) {
        ResultValid = DbgLookupSymbol(Module->Symbols,
                                      DebasedAddress,
                                      &SearchResult);
    }

    if ((ResultValid != NULL) &&
        (SearchResult.Variety == SymbolResultFunction)) {

        FunctionStart = SearchResult.U.FunctionResult->StartAddress +
                        Module->BaseDifference;
    }

GetFunctionStartAddressEnd:
    return FunctionStart;
}

BOOL
DbgpFindSymbol (
    PDEBUGGER_CONTEXT Context,
    PSTR SearchString,
    PSYMBOL_SEARCH_RESULT SearchResult
    )

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

{

    PDEBUGGER_MODULE CurrentModule;
    PLIST_ENTRY CurrentModuleEntry;
    BOOL HaveSilverMedalResult;
    PSTR ModuleEnd;
    ULONG ModuleLength;
    PTYPE_SYMBOL ResolvedType;
    BOOL Result;
    PSYMBOL_SEARCH_RESULT ResultValid;
    SYMBOL_SEARCH_RESULT SilverMedalResult;
    PDATA_TYPE_STRUCTURE Structure;
    PDEBUGGER_MODULE UserModule;

    Result = FALSE;
    ResultValid = NULL;
    HaveSilverMedalResult = FALSE;
    UserModule = NULL;

    //
    // Parameter checking.
    //

    if (SearchResult == NULL) {
        goto FindSymbolEnd;
    }

    //
    // If an exclamation point exists, then the module was specified. Find that
    // module.
    //

    ModuleEnd = strchr(SearchString, '!');
    if (ModuleEnd != NULL) {
        ModuleLength = (UINTN)ModuleEnd - (UINTN)SearchString;
        UserModule = DbgpGetModule(Context, SearchString, ModuleLength);
        if (UserModule == NULL) {
            DbgOut("Module %s not found.\n", SearchString);
            goto FindSymbolEnd;
        }

        //
        // Move the search string and initialize the list entry.
        //

        SearchString = ModuleEnd + 1;
        CurrentModuleEntry = &(UserModule->ListEntry);

    //
    // If a module was not specified, simply start with the first one.
    //

    } else {
        CurrentModuleEntry = Context->ModuleList.ModulesHead.Next;
    }

    //
    // Loop over all modules.
    //

    while (CurrentModuleEntry != &(Context->ModuleList.ModulesHead)) {
        CurrentModule = LIST_VALUE(CurrentModuleEntry,
                                   DEBUGGER_MODULE,
                                   ListEntry);

        CurrentModuleEntry = CurrentModuleEntry->Next;
        if (!IS_MODULE_IN_CURRENT_PROCESS(Context, CurrentModule)) {
            if (UserModule != NULL) {
                break;
            }

            continue;
        }

        //
        // Search for the symbol in the current module. Exit if it is found.
        //

        SearchResult->Variety = SymbolResultInvalid;
        SearchResult->U.TypeResult = NULL;
        while (TRUE) {
            ResultValid = DbgpFindSymbolInModule(CurrentModule->Symbols,
                                                 SearchString,
                                                 SearchResult);

            //
            // If not found, stop looking in this module, and go to the next
            // module.
            //

            if (ResultValid == NULL) {
                break;
            }

            Result = TRUE;

            //
            // If it's a structure with a zero size, keep looking to see if
            // another there is a different definition with a non-zero size.
            //

            if (SearchResult->Variety == SymbolResultType) {
                ResolvedType =
                        DbgResolveRelationType(SearchResult->U.TypeResult, 0);

                if ((ResolvedType != NULL) &&
                    (ResolvedType->Type == DataTypeStructure)) {

                    Structure = &(ResolvedType->U.Structure);

                    //
                    // If it's got a body, return it.
                    //

                    if (Structure->SizeInBytes != 0) {
                        goto FindSymbolEnd;
                    }

                    RtlCopyMemory(&SilverMedalResult,
                                  SearchResult,
                                  sizeof(SYMBOL_SEARCH_RESULT));

                    //
                    // Remember that there is this search result with a zero
                    // size in case that's all there is, but keep looking for
                    // something better.
                    //

                    HaveSilverMedalResult = TRUE;

                //
                // It doesn't resolve or it's not a structure, so return it.
                //

                } else {
                    goto FindSymbolEnd;
                }

            //
            // It's not a type result, so return it.
            //

            } else {
                goto FindSymbolEnd;
            }
        }

        //
        // If a specific user module was specified, do not loop over more
        // modules.
        //

        if (UserModule != NULL) {
            break;
        }
    }

    //
    // If there's not a valid result but there's a valid "second best"
    // result, then use that and declare success.
    //

    if (HaveSilverMedalResult != FALSE) {
        Result = TRUE;
        RtlCopyMemory(SearchResult,
                      &SilverMedalResult,
                      sizeof(SYMBOL_SEARCH_RESULT));
    }

FindSymbolEnd:
    return Result;
}

PDEBUGGER_MODULE
DbgpFindModuleFromEntry (
    PDEBUGGER_CONTEXT Context,
    PLOADED_MODULE_ENTRY TargetEntry
    )

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

{

    PDEBUGGER_MODULE Backup;
    ULONG BinaryNameLength;
    ULONG CharacterIndex;
    PLIST_ENTRY CurrentListEntry;
    PDEBUGGER_MODULE CurrentModule;
    PSTR FriendlyName;
    ULONG FriendlyNameLength;

    Backup = NULL;
    if (TargetEntry == NULL) {
        return NULL;
    }

    CurrentListEntry = Context->ModuleList.ModulesHead.Next;
    while (CurrentListEntry != &(Context->ModuleList.ModulesHead)) {
        CurrentModule = LIST_VALUE(CurrentListEntry,
                                   DEBUGGER_MODULE,
                                   ListEntry);

        //
        // Set up now for the next entry so that conditions can fail and use
        // continue.
        //

        CurrentListEntry = CurrentListEntry->Next;
        if (CurrentModule->Process != TargetEntry->Process) {
            continue;
        }

        if (CurrentModule->LowestAddress != TargetEntry->LowestAddress) {
            continue;
        }

        BinaryNameLength = TargetEntry->StructureSize -
                           sizeof(LOADED_MODULE_ENTRY) +
                           (ANYSIZE_ARRAY * sizeof(CHAR));

        DbgpGetFriendlyName(TargetEntry->BinaryName,
                            BinaryNameLength,
                            &FriendlyName,
                            &FriendlyNameLength);

        CharacterIndex = 0;
        while (CharacterIndex < FriendlyNameLength) {
            if (CurrentModule->ModuleName[CharacterIndex] !=
                FriendlyName[CharacterIndex]) {

                break;
            }

            CharacterIndex += 1;
        }

        if (CharacterIndex < FriendlyNameLength) {
            continue;
        }

        if (CurrentModule->ModuleName[CharacterIndex] != '\0') {
            continue;
        }

        //
        // If the timestamps don't match, save this as a backup but look for
        // something even better.
        //

        if ((TargetEntry->Timestamp != 0) &&
            (TargetEntry->Timestamp != CurrentModule->Timestamp)) {

            if (Backup == NULL) {
                Backup = CurrentModule;
            }

            continue;
        }

        //
        // All conditions were met, so this must be a match.
        //

        return CurrentModule;
    }

    return Backup;
}

PDATA_SYMBOL
DbgpFindLocal (
    PDEBUGGER_CONTEXT Context,
    PSTR LocalName,
    ULONGLONG CurrentFrameInstructionPointer
    )

/*++

Routine Description:

    This routine searches the local variables and parameters in the function
    containing the given address for a variable matching the given name.

Arguments:

    Context - Supplies a pointer to the application context.

    LocalName - Supplies a case sensitive string of the local name.

    CurrentFrameInstructionPointer - Supplies the current frame instruction
        pointer.

Return Value:

    Returns a pointer to the local variable or function parameter symbol.

    NULL if no local variable matching the given name could be found.

--*/

{

    PLIST_ENTRY CurrentEntry;
    ULONGLONG ExecutionAddress;
    PFUNCTION_SYMBOL Function;
    PDATA_SYMBOL LocalSymbol;
    PDATA_SYMBOL Parameter;
    BOOL Result;

    Result = DbgpGetCurrentFunctionInformation(Context,
                                               CurrentFrameInstructionPointer,
                                               &Function,
                                               &ExecutionAddress);

    if (Result == FALSE) {
        return NULL;
    }

    //
    // First check the locals.
    //

    LocalSymbol = DbgpGetLocal(Function, LocalName, ExecutionAddress);
    if (LocalSymbol != NULL) {
        return LocalSymbol;
    }

    //
    // Then check any function parameters.
    //

    CurrentEntry = Function->ParametersHead.Next;
    while (CurrentEntry != &(Function->ParametersHead)) {
        Parameter = LIST_VALUE(CurrentEntry, DATA_SYMBOL, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (strcasecmp(LocalName, Parameter->Name) == 0) {
            return Parameter;
        }
    }

    return NULL;
}

PDATA_SYMBOL
DbgpGetLocal (
    PFUNCTION_SYMBOL Function,
    PSTR LocalName,
    ULONGLONG ExecutionAddress
    )

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

{

    PDATA_SYMBOL CurrentLocal;
    PLIST_ENTRY CurrentLocalEntry;
    PDATA_SYMBOL Winner;

    Winner = NULL;
    CurrentLocalEntry = Function->LocalsHead.Next;
    while (CurrentLocalEntry != &(Function->LocalsHead)) {
        CurrentLocal = LIST_VALUE(CurrentLocalEntry,
                                  DATA_SYMBOL,
                                  ListEntry);

        CurrentLocalEntry = CurrentLocalEntry->Next;

        //
        // Skip this symbol if the minimum execution address is not even valid.
        // This is done first because it is a cheaper test than string compare.
        //

        if (ExecutionAddress < CurrentLocal->MinimumValidExecutionAddress) {
            continue;
        }

        //
        // Check if the name matches.
        //

        if (strcasecmp(LocalName, CurrentLocal->Name) != 0) {
            continue;
        }

        //
        // If no winner has been found yet, this one becomes the current winner
        // by default.
        //

        if (Winner == NULL) {
            Winner = CurrentLocal;

        //
        // There is already a current winner, see if this one has a lower
        // minimum execution address (closer to the current one, but still
        // greater).
        //

        } else {
            if (CurrentLocal->MinimumValidExecutionAddress <
                Winner->MinimumValidExecutionAddress) {

                Winner = CurrentLocal;
            }
        }
    }

    return Winner;
}

BOOL
DbgpGetCurrentFunctionInformation (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG CurrentFrameInstructionPointer,
    PFUNCTION_SYMBOL *Function,
    PULONGLONG ExecutionAddress
    )

/*++

Routine Description:

    This routine gets the function for the current instruction pointer and
    the module-adjusted execution address.

Arguments:

    Context - Supplies a pointer to the application context.

    CurrentFrameInstructionPointer - Supplies the current frame instruction
        pointer.

    Function - Supplies a pointer that receives symbol information for the
        current function.

    ExecutionAddress - Supplies a pointer that receives the current
        module-adjusted execution address.

Return Value:

    Returns TRUE on success, or FALSE on failure.

--*/

{

    ULONGLONG InstructionPointer;
    PDEBUGGER_MODULE Module;
    PSYMBOL_SEARCH_RESULT ResultValid;
    SYMBOL_SEARCH_RESULT SearchResult;

    assert(Function != NULL);
    assert(ExecutionAddress != NULL);

    //
    // Attempt to get the module this address is in. If one cannot be found,
    // then there is no useful information to print, so exit.
    //

    InstructionPointer = CurrentFrameInstructionPointer;
    Module = DbgpFindModuleFromAddress(Context,
                                       InstructionPointer,
                                       &InstructionPointer);

    if (Module == NULL) {
        return FALSE;
    }

    //
    // Attempt to find the current function symbol in the module.
    //

    SearchResult.Variety = SymbolResultInvalid;
    ResultValid = NULL;
    if (Module->Symbols != NULL) {
        ResultValid = DbgFindFunctionSymbol(Module->Symbols,
                                            NULL,
                                            InstructionPointer,
                                            &SearchResult);

    } else {
        return FALSE;
    }

    //
    // If a function could not be found, bail.
    //

    if ((ResultValid == NULL) ||
        (SearchResult.Variety != SymbolResultFunction)) {

        return FALSE;
    }

    *Function = SearchResult.U.FunctionResult;
    *ExecutionAddress = InstructionPointer;
    return TRUE;
}

VOID
DbgpGetFriendlyName (
    PSTR FullName,
    ULONG FullNameLength,
    PSTR *NameBegin,
    PULONG NameLength
    )

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

{

    ULONG FriendlyNameLength;
    PSTR LastSeparator;
    PSTR LastSlash;
    PSTR Period;

    LastSeparator = RtlStringFindCharacterRight(FullName, '\\', FullNameLength);
    if (LastSeparator != NULL) {
        LastSeparator += 1;
    }

    LastSlash = RtlStringFindCharacterRight(FullName, '/', FullNameLength);
    if (LastSlash != NULL) {
        LastSlash += 1;
    }

    if ((LastSeparator == NULL) ||
        ((LastSlash != NULL) && (LastSlash > LastSeparator))) {

        LastSeparator = LastSlash;
    }

    if (LastSeparator == NULL) {
        LastSeparator = FullName;
    }

    FullNameLength -= (UINTN)LastSeparator - (UINTN)FullName;
    Period = RtlStringFindCharacterRight(LastSeparator, '.', FullNameLength);
    if (Period == NULL) {
        FriendlyNameLength = FullNameLength;

    } else {
        FriendlyNameLength = (UINTN)Period - (UINTN)LastSeparator;
        if (FriendlyNameLength == 0) {

            assert(FullNameLength > 1);

            FriendlyNameLength = FullNameLength - 1;
            LastSeparator += 1;
        }
    }

    assert(FriendlyNameLength != 0);

    *NameBegin = LastSeparator;
    *NameLength = FriendlyNameLength;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

