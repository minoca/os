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
#include "dbgext.h"
#include "symbols.h"
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

typedef union _NUMERIC_UNION {
    UCHAR Uint8;
    CHAR Int8;
    USHORT Uint16;
    SHORT Int16;
    ULONG Uint32;
    LONG Int32;
    ULONGLONG Uint64;
    LONGLONG Int64;
    float Float32;
    double Float64;
} NUMERIC_UNION, *PNUMERIC_UNION;

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

        Result = DbgGetRegister(Context,
                                &(Context->FrameRegisters),
                                DataSymbol->Location.Indirect.Register,
                                &TargetAddress);

        if (Result != 0) {
            DbgOut("Error: Failed to get register %d.\n",
                   DataSymbol->Location.Indirect.Register);

            goto GetDataSymbolDataEnd;
        }

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

    Result = DbgPrintType(Context,
                          Type,
                          DataStream,
                          TypeSize,
                          SpaceLevel,
                          RecursionDepth);

    Result = 0;

PrintDataSymbolEnd:
    if (DataStream != NULL) {
        free(DataStream);
    }

    return Result;
}

INT
DbgGetRegister (
    PDEBUGGER_CONTEXT Context,
    PREGISTERS_UNION Registers,
    ULONG RegisterNumber,
    PULONGLONG RegisterValue
    )

/*++

Routine Description:

    This routine returns the contents of a register given a debug symbol
    register index.

Arguments:

    Context - Supplies a pointer to the application context.

    Registers - Supplies a pointer to the current machine context.

    RegisterNumber - Supplies the register index to get.

    RegisterValue - Supplies a pointer where the register value will be
        returned on success.

Return Value:

    0 on success.

    EINVAL if the register number is invalid.

    Other error codes on other failures.

--*/

{

    PULONG Registers32;
    INT Status;
    ULONGLONG Value;

    Status = 0;
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

            Status = EINVAL;
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

            Status = EINVAL;
        }

        break;

    default:

        assert(FALSE);

        Status = EINVAL;
        break;
    }

    *RegisterValue = Value;
    return Status;
}

INT
DbgSetRegister (
    PDEBUGGER_CONTEXT Context,
    PREGISTERS_UNION Registers,
    ULONG RegisterNumber,
    ULONGLONG Value
    )

/*++

Routine Description:

    This routine sets the contents of a register given its register number.

Arguments:

    Context - Supplies a pointer to the application context.

    Registers - Supplies a pointer to the current machine context. The register
        value will be set in this context.

    RegisterNumber - Supplies the register index to set.

    Value - Supplies the new value to set.

Return Value:

    0 on success.

    EINVAL if the register number is invalid.

    Other error codes on other failures.

--*/

{

    PULONG Registers32;
    INT Status;

    Status = 0;
    switch (Context->MachineType) {
    case MACHINE_TYPE_X86:
        switch (RegisterNumber) {
        case X86RegisterEax:
            Registers->X86.Eax = Value;
            break;

        case X86RegisterEcx:
            Registers->X86.Ecx = Value;
            break;

        case X86RegisterEdx:
            Registers->X86.Edx = Value;
            break;

        case X86RegisterEbx:
            Registers->X86.Ebx = Value;
            break;

        case X86RegisterEsp:
            Registers->X86.Esp = Value;
            break;

        case X86RegisterEbp:
            Registers->X86.Ebp = Value;
            break;

        case X86RegisterEsi:
            Registers->X86.Esi = Value;
            break;

        case X86RegisterEdi:
            Registers->X86.Edi = Value;
            break;

        case X86RegisterEip:
            Registers->X86.Eip = Value;
            break;

        case X86RegisterEflags:
            Registers->X86.Eflags = Value;
            break;

        case X86RegisterCs:
            Registers->X86.Cs = Value;
            break;

        case X86RegisterSs:
            Registers->X86.Ss = Value;
            break;

        case X86RegisterDs:
            Registers->X86.Ds = Value;
            break;

        case X86RegisterEs:
            Registers->X86.Es = Value;
            break;

        case X86RegisterFs:
            Registers->X86.Fs = Value;
            break;

        case X86RegisterGs:
            Registers->X86.Gs = Value;
            break;

        default:

            //
            // TODO: Fetch the floating point registers if not yet grabbed.
            //

            if ((RegisterNumber >= X86RegisterSt0) &&
                (RegisterNumber <= X86RegisterFpDo)) {

                DbgOut("TODO: FPU Register %d.\n", RegisterNumber);
                break;
            }

            assert(FALSE);

            Status = EINVAL;
            break;
        }

        break;

    case MACHINE_TYPE_ARMV7:
    case MACHINE_TYPE_ARMV6:
        if ((RegisterNumber >= ArmRegisterR0) &&
            (RegisterNumber <= ArmRegisterR15)) {

            Registers32 = &(Registers->Arm.R0);
            Registers32[RegisterNumber] = Value;

        } else if ((RegisterNumber >= ArmRegisterD0) &&
                   (RegisterNumber <= ArmRegisterD31)) {

            //
            // TODO: Fetch the floating point registers if not yet grabbed.
            //

            DbgOut("TODO: FPU Register D%d\n", RegisterNumber - ArmRegisterD0);

        } else {

            assert(FALSE);

            Status = EINVAL;
        }

        break;

    default:

        assert(FALSE);

        Status = EINVAL;
        break;
    }

    return Status;
}

INT
DbgGetTypeByName (
    PDEBUGGER_CONTEXT Context,
    PSTR TypeName,
    PTYPE_SYMBOL *Type
    )

/*++

Routine Description:

    This routine finds a type symbol object by its type name.

Arguments:

    Context - Supplies a pointer to the application context.

    TypeName - Supplies a pointer to the string containing the name of the
        type to find. This can be prefixed with an module name if needed.

    Type - Supplies a pointer where a pointer to the type will be returned.

Return Value:

    0 on success.

    ENOENT if no type with the given name was found.

    Returns an error number on failure.

--*/

{

    PTYPE_SYMBOL FoundType;
    BOOL Result;
    SYMBOL_SEARCH_RESULT SearchResult;
    INT Status;

    FoundType = NULL;
    SearchResult.Variety = SymbolResultType;
    Result = DbgpFindSymbol(Context, TypeName, &SearchResult);
    if ((Result == FALSE) || (SearchResult.Variety != SymbolResultType)) {
        Status = ENOENT;
        goto GetTypeByNameEnd;
    }

    //
    // Read the base type.
    //

    FoundType = SearchResult.U.TypeResult;
    FoundType = DbgSkipTypedefs(FoundType);
    Status = 0;

GetTypeByNameEnd:
    if (Status != 0) {
        FoundType = NULL;
    }

    *Type = FoundType;
    return Status;
}

INT
DbgReadIntegerMember (
    PDEBUGGER_CONTEXT Context,
    PTYPE_SYMBOL Type,
    PSTR MemberName,
    ULONGLONG Address,
    PVOID Data,
    ULONG DataSize,
    PULONGLONG Value
    )

/*++

Routine Description:

    This routine reads an integer sized member out of an already read-in
    structure.

Arguments:

    Context - Supplies a pointer to the application context.

    Type - Supplies a pointer to the type of the data.

    MemberName - Supplies a pointer to the member name.

    Address - Supplies the address where the data was obtained.

    Data - Supplies a pointer to the data contents.

    DataSize - Supplies the size of the data buffer in bytes.

    Value - Supplies a pointer where the value will be returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PVOID ShiftedData;
    UINTN ShiftedDataSize;
    INT Status;

    Status = DbgpGetStructureMember(Context,
                                    Type,
                                    MemberName,
                                    Address,
                                    Data,
                                    DataSize,
                                    &ShiftedData,
                                    &ShiftedDataSize,
                                    &Type);

    if (Status != 0) {
        return Status;
    }

    if (ShiftedDataSize > sizeof(ULONGLONG)) {
        DbgOut("Error: Member %s.%s was larger than integer size.\n",
               Type->Name,
               MemberName);

        free(ShiftedData);
        return EINVAL;
    }

    *Value = 0;
    memcpy(Value, ShiftedData, ShiftedDataSize);
    return Status;
}

INT
DbgReadTypeByName (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    PSTR TypeName,
    PTYPE_SYMBOL *FinalType,
    PVOID *Data,
    PULONG DataSize
    )

/*++

Routine Description:

    This routine reads in data from the target for a specified type, which is
    given as a string.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies a target address pointer where the data resides.

    TypeName - Supplies a pointer to a string containing the type name to get.
        This should start with a type name, and can use dot '.' notation to
        specify field members, and array[] notation to specify dereferences.

    FinalType - Supplies a pointer where the final type symbol will be returned
        on success.

    Data - Supplies a pointer where the data will be returned on success. The
        caller is responsible for freeing this data when finished.

    DataSize - Supplies a pointer where the size of the data in bytes will be
        returned.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PSTR Current;
    PVOID CurrentData;
    ULONG CurrentDataSize;
    PSTR End;
    PVOID NewData;
    UINTN NewDataSize;
    INT Status;
    PTYPE_SYMBOL Type;

    CurrentData = NULL;
    CurrentDataSize = 0;
    TypeName = strdup(TypeName);
    if (TypeName == NULL) {
        Status = ENOMEM;
        goto ReadTypeByNameEnd;
    }

    End = TypeName + strlen(TypeName);

    //
    // Get the base type name.
    //

    Current = TypeName;
    while ((*Current != '\0') && (*Current != '.') && (*Current != '[')) {
        Current += 1;
    }

    *Current = '\0';
    Current += 1;
    Status = DbgGetTypeByName(Context, TypeName, &Type);
    if (Status != 0) {
        goto ReadTypeByNameEnd;
    }

    if (Type == NULL) {
        DbgOut("Error: Cannot read void.\n");
        Status = EINVAL;
        goto ReadTypeByNameEnd;
    }

    //
    // Read the base type.
    //

    Status = DbgReadType(Context,
                         Address,
                         Type,
                         &CurrentData,
                         &CurrentDataSize);

    if (Status != 0) {
        goto ReadTypeByNameEnd;
    }

    //
    // Dereference through the structure members.
    //

    if (Current < End) {
        Status = DbgpGetStructureMember(Context,
                                        Type,
                                        Current,
                                        Address,
                                        CurrentData,
                                        CurrentDataSize,
                                        &NewData,
                                        &NewDataSize,
                                        &Type);

        if (Status != 0) {
            goto ReadTypeByNameEnd;
        }

        free(CurrentData);
        CurrentData = NewData;
        CurrentDataSize = NewDataSize;
    }

ReadTypeByNameEnd:
    if (TypeName != NULL) {
        free(TypeName);
    }

    if (Status != 0) {
        if (CurrentData != NULL) {
            free(CurrentData);
            CurrentData = NULL;
        }

        CurrentDataSize = 0;
        Type = NULL;
    }

    if (FinalType != NULL) {
        *FinalType = Type;
    }

    *Data = CurrentData;
    *DataSize = CurrentDataSize;
    return Status;
}

INT
DbgReadType (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    PTYPE_SYMBOL Type,
    PVOID *Data,
    PULONG DataSize
    )

/*++

Routine Description:

    This routine reads in data from the target for a specified type.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies a target address pointer where the data resides.

    Type - Supplies a pointer to the type symbol to get.

    Data - Supplies a pointer where the data will be returned on success. The
        caller is responsible for freeing this data when finished.

    DataSize - Supplies a pointer where the size of the data in bytes will be
        returned.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PVOID Buffer;
    ULONG BytesRead;
    ULONGLONG Size;
    INT Status;

    *Data = NULL;
    *DataSize = 0;
    Size = DbgGetTypeSize(Type, 0);
    Buffer = malloc(Size);
    if (Buffer == NULL) {
        return ENOMEM;
    }

    memset(Buffer, 0, Size);
    Status = DbgReadMemory(Context, TRUE, Address, Size, Buffer, &BytesRead);
    if (Status != 0) {
        free(Buffer);
        return Status;
    }

    *Data = Buffer;
    *DataSize = Size;
    return 0;
}

INT
DbgPrintTypeByName (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    PSTR TypeName,
    ULONG SpaceLevel,
    ULONG RecursionCount
    )

/*++

Routine Description:

    This routine prints a structure or value at a specified address, whose type
    is specified by a string.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies a target address pointer where the data resides.

    TypeName - Supplies a pointer to a string containing the type name to get.
        This should start with a type name, and can use dot '.' notation to
        specify field members, and array[] notation to specify dereferences.

    SpaceLevel - Supplies the number of spaces worth of indentation to print
        for subsequent lines.

    RecursionCount - Supplies the number of substructures to recurse into.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PVOID Data;
    UINTN DataSize;
    INT Status;
    PTYPE_SYMBOL Type;

    Data = NULL;
    Status = DbgReadTypeByName(Context,
                               Address,
                               TypeName,
                               &Type,
                               &Data,
                               &DataSize);

    if (Status != 0) {
        return Status;
    }

    Status = DbgPrintType(Context,
                          Type,
                          Data,
                          DataSize,
                          SpaceLevel,
                          RecursionCount);

    free(Data);
    return Status;
}

INT
DbgPrintTypeMember (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    PVOID Data,
    ULONG DataSize,
    PTYPE_SYMBOL Type,
    PSTR MemberName,
    ULONG SpaceLevel,
    ULONG RecursionCount
    )

/*++

Routine Description:

    This routine prints a member of a structure or union whose contents have
    already been read in.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies the address where this data came from.

    Data - Supplies a pointer to the data contents.

    DataSize - Supplies the size of the data contents buffer in bytes.

    Type - Supplies a pointer to the structure type.

    MemberName - Supplies the name of the member to print.

    SpaceLevel - Supplies the number of spaces worth of indentation to print
        for subsequent lines.

    RecursionCount - Supplies the number of substructures to recurse into.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PVOID ShiftedData;
    UINTN ShiftedDataSize;
    INT Status;

    Status = DbgpGetStructureMember(Context,
                                    Type,
                                    MemberName,
                                    Address,
                                    Data,
                                    DataSize,
                                    &ShiftedData,
                                    &ShiftedDataSize,
                                    &Type);

    if (Status != 0) {
        return Status;
    }

    Status = DbgPrintType(Context,
                          Type,
                          ShiftedData,
                          ShiftedDataSize,
                          SpaceLevel,
                          RecursionCount);

    return Status;
}

INT
DbgPrintType (
    PDEBUGGER_CONTEXT Context,
    PTYPE_SYMBOL Type,
    PVOID Data,
    UINTN DataSize,
    ULONG SpaceLevel,
    ULONG RecursionCount
    )

/*++

Routine Description:

    This routine prints the given type to the debugger console.

Arguments:

    Context - Supplies a pointer to the application context.

    Type - Supplies a pointer to the data type to print.

    Data - Supplies a pointer to the data contents.

    DataSize - Supplies the size of the data buffer in bytes.

    SpaceLevel - Supplies the number of spaces worth of indentation to print
        for subsequent lines.

    RecursionCount - Supplies the number of substructures to recurse into.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    ULONGLONG ArrayIndex;
    ULONG BitRemainder;
    ULONG Bytes;
    PDATA_TYPE_ENUMERATION Enumeration;
    PENUMERATION_MEMBER EnumerationMember;
    CHAR Field[256];
    PVOID MemberData;
    PTYPE_SYMBOL MemberType;
    NUMERIC_UNION NumericValue;
    PDATA_TYPE_RELATION Relation;
    PTYPE_SYMBOL RelativeType;
    PVOID ShiftedData;
    INT Status;
    PDATA_TYPE_STRUCTURE Structure;
    PSTRUCTURE_MEMBER StructureMember;
    UINTN TypeSize;

    Status = 0;
    switch (Type->Type) {
    case DataTypeNumeric:
        Status = DbgpPrintNumeric(Type, Data, DataSize);
        break;

    case DataTypeRelation:
        Type = DbgSkipTypedefs(Type);
        if (Type == NULL) {
            DbgOut("void");
            Status = 0;
            break;
        }

        //
        // If it just ended up being a typedef to something else, print that
        // something else.
        //

        if (Type->Type != DataTypeRelation) {
            Status = DbgPrintType(Context,
                                  Type,
                                  Data,
                                  DataSize,
                                  SpaceLevel,
                                  RecursionCount);

            return Status;
        }

        //
        // This is either a pointer or an array.
        //

        Relation = &(Type->U.Relation);
        RelativeType = DbgGetType(Relation->OwningFile, Relation->TypeNumber);

        assert((Relation->Array.Minimum != Relation->Array.Maximum) ||
               (Relation->Pointer != 0));

        //
        // If it's a pointer, then the type is just a pointer.
        //

        if (Relation->Pointer != 0) {
            TypeSize = Relation->Pointer;
            if (DataSize < TypeSize) {
                return ERANGE;
            }

            NumericValue.Uint64 = 0;
            memcpy(&NumericValue, Data, TypeSize);
            DbgOut("0x%08I64x", NumericValue.Uint64);
            break;
        }

        //
        // This is an array.
        //

        TypeSize = 0;
        DbgPrintTypeName(Type);
        if (RecursionCount == 0) {
            break;
        }

        SpaceLevel += 2;
        TypeSize = DbgGetTypeSize(RelativeType, 0);

        //
        // If it's a a string, print it out as such.
        //

        if ((RelativeType->Type == DataTypeNumeric) &&
            (RelativeType->U.Numeric.Signed != FALSE) &&
            (RelativeType->U.Numeric.BitSize == BITS_PER_BYTE) &&
            (RelativeType->U.Numeric.Float == FALSE)) {

            TypeSize = Relation->Array.Maximum - Relation->Array.Minimum + 1;
            if (DataSize < TypeSize) {
                return ERANGE;
            }

            DbgPrintStringData(Data, TypeSize, SpaceLevel);

        } else {
            for (ArrayIndex = Relation->Array.Minimum;
                 ArrayIndex <= Relation->Array.Maximum;
                 ArrayIndex += 1) {

                if (DataSize < TypeSize) {
                    Status = ERANGE;
                    break;
                }

                DbgOut("\n%*s", SpaceLevel, "");
                DbgOut("[%I64d] --------------------------------------"
                       "-------", ArrayIndex);

                DbgOut("\n%*s", SpaceLevel + 2, "");
                Status = DbgPrintType(Context,
                                      RelativeType,
                                      Data,
                                      DataSize,
                                      SpaceLevel + 2,
                                      RecursionCount - 1);

                if (Status != 0) {
                    break;
                }

                Data += TypeSize;
                DataSize -= TypeSize;
            }
        }

        SpaceLevel -= 2;
        break;

    case DataTypeEnumeration:
        Enumeration = &(Type->U.Enumeration);
        TypeSize = Enumeration->SizeInBytes;
        if (TypeSize > sizeof(NumericValue)) {
            TypeSize = sizeof(NumericValue);
        }

        NumericValue.Uint64 = 0;
        memcpy(&NumericValue, Data, TypeSize);
        switch (TypeSize) {
        case 1:
            NumericValue.Int64 = NumericValue.Int8;
            break;

        case 2:
            NumericValue.Int64 = NumericValue.Int16;
            break;

        case 4:
            NumericValue.Int64 = NumericValue.Int32;
            break;

        case 8:
            break;

        default:

            assert(FALSE);

            return EINVAL;
        }

        DbgOut("%I64d", NumericValue.Int64);
        EnumerationMember = Enumeration->FirstMember;
        while (EnumerationMember != NULL) {
            if (EnumerationMember->Value == NumericValue.Int64) {
                DbgOut(" %s", EnumerationMember->Name);
                break;
            }

            EnumerationMember = EnumerationMember->NextMember;
        }

        break;

    case DataTypeStructure:
        Structure = &(Type->U.Structure);
        TypeSize = Structure->SizeInBytes;
        if (DataSize < TypeSize) {
            return ERANGE;
        }

        //
        // If the recursion depth is zero, don't print this structure contents
        // out, only print the name.
        //

        DbgPrintTypeName(Type);
        if (RecursionCount == 0) {
            break;
        }

        SpaceLevel += 2;
        StructureMember = Structure->FirstMember;
        while (StructureMember != NULL) {
            Bytes = StructureMember->BitOffset / BITS_PER_BYTE;
            if (Bytes >= DataSize) {
                return ERANGE;
            }

            BitRemainder = StructureMember->BitOffset % BITS_PER_BYTE;
            MemberData = Data + Bytes;
            DbgOut("\n%*s", SpaceLevel, "");
            snprintf(Field, sizeof(Field), "+0x%x", Bytes);
            DbgOut("%-6s  ", Field);
            ShiftedData = NULL;
            if (BitRemainder != 0) {
                snprintf(Field,
                         sizeof(Field),
                         "%s:%d",
                         StructureMember->Name,
                         BitRemainder);

            } else {
                snprintf(Field, sizeof(Field), StructureMember->Name);
            }

            Field[sizeof(Field) - 1] = '\0';
            DbgOut("%-17s : ", Field);

            //
            // Manipulate the data for the structure member if it's got a
            // bitwise offset or size.
            //

            if ((BitRemainder != 0) || (StructureMember->BitSize != 0)) {
                ShiftedData = DbgpShiftBufferRight(MemberData,
                                                   DataSize - Bytes,
                                                   BitRemainder,
                                                   StructureMember->BitSize);

                if (ShiftedData == NULL) {
                    return ENOMEM;
                }

                MemberData = ShiftedData;
            }

            MemberType = DbgGetType(StructureMember->TypeFile,
                                    StructureMember->TypeNumber);

            if (MemberType == NULL) {
                DbgOut("DANGLING REFERENCE %s, %d\n",
                       StructureMember->TypeFile->SourceFile,
                       StructureMember->TypeNumber);

                assert(MemberType != NULL);

            } else {
                Status = DbgPrintType(Context,
                                      MemberType,
                                      MemberData,
                                      DataSize - Bytes,
                                      SpaceLevel,
                                      RecursionCount - 1);

                if (Status != 0) {
                    break;
                }
            }

            if (ShiftedData != NULL) {
                free(ShiftedData);
                ShiftedData = NULL;
            }

            StructureMember = StructureMember->NextMember;
        }

        SpaceLevel -= 2;
        break;

    case DataTypeFunctionPointer:
        TypeSize = Type->U.FunctionPointer.SizeInBytes;
        if (TypeSize > sizeof(NumericValue)) {
            TypeSize = sizeof(NumericValue);
        }

        NumericValue.Uint64 = 0;
        memcpy(&NumericValue, Data, TypeSize);
        DbgOut("(*0x%08I64x)()", NumericValue.Uint64);
        break;

    default:

        assert(FALSE);

        break;
    }

    return Status;
}

VOID
DbgPrintStringData (
    PSTR String,
    UINTN Size,
    ULONG SpaceDepth
    )

/*++

Routine Description:

    This routine prints string data to the debugger console.

Arguments:

    String - Supplies a pointer to the string data.

    Size - Supplies the number of bytes to print out.

    SpaceDepth - Supplies the indentation to use when breaking up a string into
        multiple lines.

Return Value:

    None.

--*/

{

    UCHAR Character;
    ULONG Column;

    Column = SpaceDepth;
    DbgOut("\"");
    Column += 1;
    while (Size != 0) {
        Character = *String;
        if ((Character >= ' ') && (Character < 0x80)) {
            DbgOut("%c", Character);
            Column += 1;

        } else if (Character == '\0') {
            DbgOut("\\0");
            Column += 2;

        } else if (Character == '\r') {
            DbgOut("\\r");
            Column += 2;

        } else if (Character == '\n') {
            DbgOut("\\n");
            Column += 2;

        } else if (Character == '\f') {
            DbgOut("\\f");
            Column += 2;

        } else if (Character == '\v') {
            DbgOut("\\v");
            Column += 2;

        } else if (Character == '\t') {
            DbgOut("\\t");
            Column += 2;

        } else if (Character == '\a') {
            DbgOut("\\a");
            Column += 2;

        } else if (Character == '\b') {
            DbgOut("\\b");
            Column += 2;

        } else {
            DbgOut("\\x%02x", Character);
            Column += 4;
        }

        String += 1;
        Size -= 1;
        if (Column >= 80) {
            Column = SpaceDepth;
            DbgOut("\n%*s", SpaceDepth, "");
        }
    }

    DbgOut("\"");
    return;
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
                ResolvedType = DbgSkipTypedefs(SearchResult->U.TypeResult);
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

INT
DbgpPrintNumeric (
    PTYPE_SYMBOL Type,
    PVOID Data,
    UINTN DataSize
    )

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

{

    ULONG BitSize;
    PDATA_TYPE_NUMERIC Numeric;
    NUMERIC_UNION NumericValue;
    UINTN TypeSize;

    assert(Type->Type == DataTypeNumeric);

    Numeric = &(Type->U.Numeric);
    BitSize = Numeric->BitSize;
    TypeSize = ALIGN_RANGE_UP(BitSize, BITS_PER_BYTE) / BITS_PER_BYTE;
    if (DataSize < TypeSize) {
        return ERANGE;
    }

    NumericValue.Uint64 = 0;
    memcpy(&NumericValue, Data, TypeSize);
    if ((BitSize & (BITS_PER_BYTE - 1)) != 0) {
        NumericValue.Uint64 &= (1ULL << BitSize) - 1;
    }

    if (Numeric->Float != FALSE) {
        switch (TypeSize) {
        case 4:
            DbgOut("%f", (double)(NumericValue.Float32));
            break;

        case 8:
            DbgOut("%f", NumericValue.Float64);
            break;

        default:
            DbgOut("%I64x", NumericValue.Uint64);
            break;
        }

    } else if (Numeric->Signed != FALSE) {
        switch (TypeSize) {
        case 1:
            DbgOut("%d", NumericValue.Int8);
            break;

        case 2:
            DbgOut("%d", NumericValue.Int16);
            break;

        case 4:
            DbgOut("%d", NumericValue.Int32);
            break;

        case 8:
        default:
            DbgOut("%I64d", NumericValue.Int64);
            break;
        }

    } else {
        DbgOut("0x%I64x", NumericValue.Uint64);
    }

    return 0;
}

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
    )

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

{

    ULONGLONG ArrayIndex;
    PSTR Current;
    PVOID CurrentData;
    UINTN CurrentDataSize;
    BOOL Dereference;
    PSTR End;
    PSTR FieldName;
    CHAR FieldType;
    PSTRUCTURE_MEMBER Member;
    PVOID NewData;
    ULONG NewDataSize;
    PTYPE_SYMBOL RelativeType;
    UINT ShiftAmount;
    INT Status;
    UINTN TypeSize;

    CurrentData = Data;
    CurrentDataSize = DataSize;
    MemberName = strdup(MemberName);
    if (MemberName == NULL) {
        Status = ENOMEM;
        goto GetStructureMemberEnd;
    }

    Type = DbgSkipTypedefs(Type);
    End = MemberName + strlen(MemberName);
    Current = MemberName;
    Status = 0;

    //
    // Now loop reading members and array indices.
    //

    while (Current < End) {

        //
        // Assume a member if a dot is missing from the beginning.
        //

        if ((*Current != '.') && (*Current != '[')) {
            FieldType = '.';

        } else {
            FieldType = *Current;
            Current += 1;
        }

        if (*Current == '\0') {
            break;
        }

        FieldName = Current;

        //
        // Handle an array access.
        //

        if (FieldType == '[') {

            //
            // Find the closing square bracket.
            //

            while ((*Current != '\0') && (*Current != ']')) {
                Current += 1;
            }

            *Current = '\0';
            Current += 1;
            Status = DbgEvaluate(Context, FieldName, &ArrayIndex);
            if (Status != 0) {
                DbgOut("Error: Failed to evaluate array index '%s'.\n",
                       FieldName);

                goto GetStructureMemberEnd;
            }

            //
            // If this current type is not a relation, then a dereference will
            // have to occur to make something like mytype[3] work, where
            // mytype is a structure.
            //

            Dereference = FALSE;
            if (Type->Type != DataTypeRelation) {
                Dereference = TRUE;

            //
            // If it is a relation, use the relative type as the type size. If
            // this is a pointer type, then it will also need a dereference
            // through it.
            //

            } else if ((Type->U.Relation.Pointer != 0) ||
                       (Type->U.Relation.Array.Minimum !=
                        Type->U.Relation.Array.Maximum)) {

                if (Type->U.Relation.Pointer != 0) {
                    Dereference = TRUE;
                    Address = 0;
                    memcpy(&Address, CurrentData, Type->U.Relation.Pointer);
                }

                RelativeType = DbgGetType(Type->U.Relation.OwningFile,
                                          Type->U.Relation.TypeNumber);

                if ((RelativeType == NULL) || (RelativeType == Type)) {
                    DbgOut("Error: Cannot get void type.\n");
                    Status = EINVAL;
                    goto GetStructureMemberEnd;
                }

                Type = RelativeType;
            }

            TypeSize = DbgGetTypeSize(Type, 0);
            if (TypeSize == 0) {
                DbgOut("Error: Got a type size of zero.\n");
                Status = EINVAL;
                goto GetStructureMemberEnd;
            }

            //
            // If this was a pointer, dereference through the pointer to get
            // the new data.
            //

            if (Dereference != FALSE) {
                Address += TypeSize * ArrayIndex;
                Status = DbgReadType(Context,
                                     Address,
                                     Type,
                                     &NewData,
                                     &NewDataSize);

                if (Status != 0) {
                    goto GetStructureMemberEnd;
                }

            //
            // If this was an array, just shift the buffer over to index into
            // it.
            //

            } else {
                ShiftAmount = TypeSize * ArrayIndex * BITS_PER_BYTE;
                NewData = DbgpShiftBufferRight(CurrentData,
                                               CurrentDataSize,
                                               ShiftAmount,
                                               0);

                if (NewData == NULL) {
                    Status = ENOMEM;
                    goto GetStructureMemberEnd;
                }

                NewDataSize = TypeSize;
            }

        //
        // Access a structure member.
        //

        } else if (FieldType == '.') {

            //
            // Find the end of the member name.
            //

            while ((*Current != '\0') && (*Current != '.') &&
                   (*Current != '[')) {

                Current += 1;
            }

            *Current = '\0';
            Current += 1;
            if (Type->Type != DataTypeStructure) {
                DbgOut("Error: %s is not a structure.\n", Type->Name);
            }

            //
            // Find the member. First try matching case, then try case
            // insensitive.
            //

            Member = Type->U.Structure.FirstMember;
            while (Member != NULL) {
                if (strcmp(Member->Name, FieldName) == 0) {
                    break;
                }

                Member = Member->NextMember;
            }

            if (Member == NULL) {
                Member = Type->U.Structure.FirstMember;
                while (Member != NULL) {
                    if (strcasecmp(Member->Name, FieldName) == 0) {
                        break;
                    }

                    Member = Member->NextMember;
                }
            }

            if (Member == NULL) {
                DbgOut("Error: Structure %s has no member %s.\n",
                       Type->Name,
                       FieldName);

                Status = ENOENT;
                goto GetStructureMemberEnd;
            }

            //
            // Get the next type of this member.
            //

            Type = DbgGetType(Member->TypeFile, Member->TypeNumber);
            if (Type != NULL) {
                Type = DbgSkipTypedefs(Type);
            }

            if (Type == NULL) {
                DbgOut("Error: Got incomplete member %s.\n", FieldName);
                Status = EINVAL;
                goto GetStructureMemberEnd;
            }

            //
            // Manipulate the buffer to put the member at the beginning, which
            // creates a new buffer.
            //

            NewData = DbgpShiftBufferRight(CurrentData,
                                           CurrentDataSize,
                                           Member->BitOffset,
                                           Member->BitSize);

            if (NewData == NULL) {
                Status = ENOMEM;
                goto GetStructureMemberEnd;
            }

            NewDataSize = DbgGetTypeSize(Type, 0);

        } else {

            assert(FALSE);

            Status = EINVAL;
            break;
        }

        assert(NewData != NULL);

        if (CurrentData != Data) {
            free(CurrentData);
        }

        CurrentData = NewData;
        CurrentDataSize = NewDataSize;
    }

GetStructureMemberEnd:
    if (MemberName != NULL) {
        free(MemberName);
    }

    if (Status != 0) {
        if ((CurrentData != NULL) && (CurrentData != Data)) {
            free(CurrentData);
            CurrentData = NULL;
        }

        CurrentDataSize = 0;
        Type = NULL;
    }

    *ShiftedData = CurrentData;
    *ShiftedDataSize = CurrentDataSize;
    *FinalType = Type;
    return Status;
}

PVOID
DbgpShiftBufferRight (
    PVOID Buffer,
    UINTN DataSize,
    UINTN Bits,
    UINTN BitSize
    )

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

{

    UINTN ByteCount;
    PUCHAR Bytes;
    UINTN Index;

    Bytes = malloc(DataSize);
    if (Bytes == NULL) {
        return NULL;
    }

    ByteCount = Bits / BITS_PER_BYTE;
    Bits %= BITS_PER_BYTE;
    if (DataSize > ByteCount) {
        memcpy(Bytes, Buffer + ByteCount, DataSize - ByteCount);
        memset(Bytes + DataSize - ByteCount, 0, ByteCount);

    } else {
        memset(Bytes, 0, DataSize);
        return Bytes;
    }

    if (Bits != 0) {

        //
        // Now the tricky part, shifting by between 1 and 7 bits.
        //

        for (Index = 0; Index < DataSize - 1; Index += 1) {
            Bytes[Index] = (Bytes[Index] >> Bits) |
                           (Bytes[Index + 1] << (BITS_PER_BYTE - Bits));
        }

        Bytes[Index] = Bytes[Index] >> Bits;
    }

    //
    // Do some masking as well.
    //

    if (BitSize != 0) {
        Index = BitSize / BITS_PER_BYTE;
        BitSize %= BITS_PER_BYTE;
        if (BitSize != 0) {
            Bytes[Index] &= (1 << BitSize) - 1;
            Index += 1;
        }

        if (Index != DataSize) {
            memset(Bytes + Index, 0, DataSize - Index);
        }
    }

    return Bytes;
}

//
// --------------------------------------------------------- Internal Functions
//

