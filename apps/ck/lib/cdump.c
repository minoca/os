/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    cdump.c

Abstract:

    This module implements support for dumping compiled Chalk module bytecode.

Author:

    Evan Green 10-Oct-2016

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "chalkp.h"

#include <stdio.h>
#include <stdlib.h>

//
// --------------------------------------------------------------------- Macros
//

#define CkpFreezeAdd(_Vm, _Array, _String, _Length) \
    CkpFillArray((_Vm), (_Array), (PUCHAR)(_String), (_Length))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the current freeze file format version.
//

#define CK_FREEZE_VERSION 1

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
CkpFreezeValue (
    PCK_VM Vm,
    PCK_BYTE_ARRAY String,
    CK_VALUE Value
    );

VOID
CkpFreezeList (
    PCK_VM Vm,
    PCK_BYTE_ARRAY String,
    PCK_VALUE_ARRAY List,
    UINTN Count,
    UINTN StartIndex
    );

VOID
CkpFreezeInteger (
    PCK_VM Vm,
    PCK_BYTE_ARRAY String,
    CK_INTEGER Value
    );

VOID
CkpFreezeRawInteger (
    PCK_VM Vm,
    PCK_BYTE_ARRAY String,
    CK_INTEGER Value
    );

VOID
CkpFreezeString (
    PCK_VM Vm,
    PCK_BYTE_ARRAY String,
    PCK_STRING Value
    );

VOID
CkpFreezeBuffer (
    PCK_VM Vm,
    PCK_BYTE_ARRAY String,
    UINTN Length,
    PVOID Buffer
    );

VOID
CkpFreezeFunction (
    PCK_VM Vm,
    PCK_BYTE_ARRAY String,
    PCK_FUNCTION Function
    );

PCSTR
CkpThawElement (
    PCSTR *Contents,
    PUINTN Size,
    PUINTN NameSize
    );

BOOL
CkpThawValue (
    PCK_VM Vm,
    PCK_MODULE Module,
    PCSTR *Contents,
    PUINTN Size,
    PCK_VALUE Value
    );

BOOL
CkpThawClosure (
    PCK_VM Vm,
    PCK_MODULE Module,
    PCSTR *Contents,
    PUINTN Size,
    PCK_CLOSURE *Closure
    );

BOOL
CkpThawFunction (
    PCK_VM Vm,
    PCK_MODULE Module,
    PCSTR *Contents,
    PUINTN Size,
    PCK_FUNCTION *NewFunction
    );

BOOL
CkpThawInteger (
    PCSTR *Contents,
    PUINTN Size,
    PCK_INTEGER Integer
    );

BOOL
CkpThawStringTable (
    PCK_VM Vm,
    PCK_MODULE Module,
    PCSTR *Contents,
    PUINTN Size,
    PCK_STRING_TABLE Table
    );

BOOL
CkpThawList (
    PCK_VM Vm,
    PCK_MODULE Module,
    PCSTR *Contents,
    PUINTN Size,
    PCK_VALUE_ARRAY List
    );

PCK_STRING
CkpThawString (
    PCK_VM Vm,
    PCSTR *Contents,
    PUINTN Size
    );

BOOL
CkpThawBuffer (
    PCK_VM Vm,
    PCSTR *Contents,
    PUINTN Size,
    PCK_BYTE_ARRAY Buffer
    );

//
// -------------------------------------------------------------------- Globals
//

const UCHAR CkModuleFreezeSignature[CK_FREEZE_SIGNATURE_SIZE] =
    {0x7F, 'C', 'k', 0x00};

//
// ------------------------------------------------------------------ Functions
//

CK_VALUE
CkpModuleFreeze (
    PCK_VM Vm,
    PCK_MODULE Module
    )

/*++

Routine Description:

    This routine freezes a module, writing out its compiled bytecode into a
    string.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Module - Supplies a pointer to the module to freeze.

Return Value:

    Returns a string describing the module on success.

    CK_NULL_VALUE on failure.

--*/

{

    PCK_CLOSURE Closure;
    PCK_MODULE CoreModule;
    UINTN CoreVariableCount;
    CK_BYTE_ARRAY String;
    CK_VALUE Value;

    CkpInitializeArray(&String);
    CkpFreezeAdd(Vm,
                 &String,
                 CkModuleFreezeSignature,
                 sizeof(CkModuleFreezeSignature));

    CkpFreezeAdd(Vm, &String, "{\nVersion: ", 11);
    CkpFreezeInteger(Vm, &String, CK_FREEZE_VERSION);
    CkpFreezeAdd(Vm, &String, "\nName: ", 6);
    CkpFreezeString(Vm, &String, Module->Name);
    if (Module->Path != NULL) {
        CkpFreezeAdd(Vm, &String, "\nPath: ", 6);
        CkpFreezeString(Vm, &String, Module->Path);
    }

    //
    // The core module adds itself to the scope of every other module. When
    // saving the variable names, don't save the core module names. It's both
    // a waste, and conflicts with the variables names that will already be
    // there on thaw. Do remember how many there were to detect changes in the
    // core module namespace and reject mismatched objects.
    //

    CoreVariableCount = 0;
    CoreModule = CkpModuleGet(Vm, CkNullValue);
    if (CoreModule != NULL) {
        CoreVariableCount = CoreModule->Variables.Count;

        CK_ASSERT(CoreVariableCount == CoreModule->CompiledVariableCount);

        CkpFreezeAdd(Vm, &String, "\nCoreVariableCount: ", 20);
        CkpFreezeInteger(Vm, &String, CoreVariableCount);
    }

    CkpFreezeAdd(Vm, &String, "\nVariableNames: ", 16);

    CK_ASSERT(CoreVariableCount <= Module->CompiledVariableCount);

    CkpFreezeList(Vm,
                  &String,
                  &(Module->VariableNames.List),
                  Module->CompiledVariableCount,
                  CoreVariableCount);

    CkpFreezeAdd(Vm, &String, "Strings: ", 9);
    CkpFreezeList(Vm,
                  &String,
                  &(Module->Strings.List),
                  Module->Strings.List.Count,
                  0);

    Closure = Module->Closure;
    if ((Closure != NULL) && (Closure->Type == CkClosureBlock)) {
        CkpFreezeAdd(Vm, &String, "Closure: ", 9);
        CkpFreezeFunction(Vm, &String, Closure->U.Block.Function);
    }

    CkpFreezeAdd(Vm, &String, "}\n", 2);
    Value = CkNullValue;
    if (String.Count != 0) {
        Value = CkpStringCreate(Vm, (PCSTR)(String.Data), String.Count);
    }

    if (!CK_IS_NULL(Value)) {
        CkpPushRoot(Vm, CK_AS_OBJECT(Value));
    }

    if (String.Data != NULL) {
        CkFree(Vm, String.Data);
    }

    if (!CK_IS_NULL(Value)) {
        CkpPopRoot(Vm);
    }

    return Value;
}

BOOL
CkpModuleThaw (
    PCK_VM Vm,
    PCK_MODULE Module,
    PCSTR Contents,
    UINTN Size
    )

/*++

Routine Description:

    This routine thaws out a previously frozen module, reloading it quicker
    than recompiling it.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Module - Supplies a pointer to the module to freeze. The caller should
        make sure this module doesn't get cleaned up by garbage collection.

    Contents - Supplies the frozen module contents.

    Size - Supplies the frozen module size in bytes.

Return Value:

    TRUE if the module was successfully thawed.

    FALSE if the module could not be thawed.

--*/

{

    PCK_MODULE CoreModule;
    CK_INTEGER CoreVariableCount;
    CK_INTEGER Integer;
    INT Match;
    PCSTR Name;
    UINTN NameSize;
    BOOL Result;
    PCK_STRING String;

    CoreVariableCount = 0;
    if (Size < sizeof(CkModuleFreezeSignature)) {
        return FALSE;
    }

    Match = CkCompareMemory(Contents,
                            CkModuleFreezeSignature,
                            sizeof(CkModuleFreezeSignature));

    if (Match != 0) {
        return FALSE;
    }

    Contents += sizeof(CkModuleFreezeSignature);
    Size -= sizeof(CkModuleFreezeSignature);
    if ((Size < 1) || (*Contents != '{')) {
        return FALSE;
    }

    Contents += 1;
    Size -= 1;

    //
    // Version needs to be first.
    //

    Name = CkpThawElement(&Contents, &Size, &NameSize);
    if ((Name == NULL) ||
        (NameSize != 7) ||
        (CkCompareMemory(Name, "Version", 7) != 0)) {

        return FALSE;
    }

    if ((!CkpThawInteger(&Contents, &Size, &Integer)) ||
        (Integer != CK_FREEZE_VERSION)) {

        return FALSE;
    }

    //
    // The module name needs to be next, and it better match up with what was
    // just loaded.
    //

    Name = CkpThawElement(&Contents, &Size, &NameSize);
    if ((Name == NULL) || (NameSize != 4) ||
        (CkCompareMemory(Name, "Name", 4) != 0)) {

        return FALSE;
    }

    String = CkpThawString(Vm, &Contents, &Size);
    if ((String == NULL) || (String->Length != Module->Name->Length) ||
        (CkCompareMemory(String->Value, Module->Name->Value, String->Length) !=
         0)) {

        return FALSE;
    }

    //
    // Get the rest of the fields.
    //

    Result = TRUE;
    while (Result != FALSE) {
        Name = CkpThawElement(&Contents, &Size, &NameSize);
        if (Name == NULL) {
            break;
        }

        if ((NameSize == 13) &&
            (CkCompareMemory(Name, "VariableNames", 13) == 0)) {

            Result = CkpThawStringTable(Vm,
                                        Module,
                                        &Contents,
                                        &Size,
                                        &(Module->VariableNames));

            //
            // Initialize all the variable values to null for each of the new
            // names.
            //

            if (Result != FALSE) {
                while (Module->Variables.Count <
                       Module->VariableNames.List.Count) {

                    if (CkpArrayAppend(Vm, &(Module->Variables), CkNullValue) !=
                        CkSuccess) {

                        Result = FALSE;
                        break;
                    }
                }
            }

        } else if ((NameSize == 7) &&
                   (CkCompareMemory(Name, "Strings", 7) == 0)) {

            Result = CkpThawStringTable(Vm,
                                        Module,
                                        &Contents,
                                        &Size,
                                        &(Module->Strings));

        } else if ((NameSize == 7) &&
                   (CkCompareMemory(Name, "Closure", 7) == 0)) {

            Result = CkpThawClosure(Vm,
                                    Module,
                                    &Contents,
                                    &Size,
                                    &(Module->Closure));

        } else if ((NameSize == 4) &&
                   (CkCompareMemory(Name, "Path", 4) == 0)) {

            Module->Path = CkpThawString(Vm, &Contents, &Size);

        } else if ((NameSize == 17) &&
                   (CkCompareMemory(Name, "CoreVariableCount", 17) == 0)) {

            //
            // Validate that the number of module level variables in the core
            // module is the same as it was when this module was frozen. If
            // they are different, then load/store module variable numbers in
            // the frozen module will be off, since values from the core module
            // are pre-loaded into every module.
            //

            Result = CkpThawInteger(&Contents, &Size, &CoreVariableCount);
            if (Result != FALSE) {
                CoreModule = CkpModuleGet(Vm, CkNullValue);
                if (CoreModule != NULL) {
                    if (CoreModule->Variables.Count != CoreVariableCount) {
                        Result = FALSE;
                    }

                } else {
                    Result = FALSE;
                }
            }

        } else {
            Result = FALSE;
        }
    }

    if ((Size < 1) || (*Contents != '}')) {
        return FALSE;
    }

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
CkpFreezeValue (
    PCK_VM Vm,
    PCK_BYTE_ARRAY String,
    CK_VALUE Value
    )

/*++

Routine Description:

    This routine prints an arbitrary value. Currently only a few types are
    supported.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    String - Supplies a pointer to the output in progress.

    Value - Supplies the value to print.

Return Value:

    None.

--*/

{

    PCK_OBJECT Object;

    switch (Value.Type) {
    case CkValueNull:
        CkpFreezeAdd(Vm, String, "null", 4);
        break;

    case CkValueInteger:
        CkpFreezeInteger(Vm, String, CK_AS_INTEGER(Value));
        break;

    case CkValueObject:
        Object = CK_AS_OBJECT(Value);
        switch (Object->Type) {
        case CkObjectString:
            CkpFreezeString(Vm, String, CK_AS_STRING(Value));
            break;

        case CkObjectFunction:
            CkpFreezeFunction(Vm, String, CK_AS_FUNCTION(Value));
            break;

        default:

            CK_ASSERT(FALSE);

            break;
        }

        break;

    default:

        CK_ASSERT(FALSE);

        break;
    }

    return;
}

VOID
CkpFreezeList (
    PCK_VM Vm,
    PCK_BYTE_ARRAY String,
    PCK_VALUE_ARRAY List,
    UINTN Count,
    UINTN StartIndex
    )

/*++

Routine Description:

    This routine prints a list of values.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    String - Supplies a pointer to the output in progress.

    List - Supplies a pointer to the list to print.

    Count - Supplies the number of elements in the list. Set this to the List
        count.

    StartIndex - Supplies the index to start from, in cases where the entire
        list is not saved.

Return Value:

    None.

--*/

{

    UINTN Index;

    CK_ASSERT((Count <= List->Count) && (StartIndex <= Count));

    CkpFreezeAdd(Vm, String, "l", 1);
    CkpFreezeRawInteger(Vm, String, Count - StartIndex);
    CkpFreezeAdd(Vm, String, "[", 1);
    for (Index = StartIndex; Index < Count; Index += 1) {
        CkpFreezeValue(Vm, String, List->Data[Index]);
        if (Index < Count - 1) {
            CkpFreezeAdd(Vm, String, ",\n", 2);
        }
    }

    CkpFreezeAdd(Vm, String, "]\n", 2);
    return;
}

VOID
CkpFreezeInteger (
    PCK_VM Vm,
    PCK_BYTE_ARRAY String,
    CK_INTEGER Value
    )

/*++

Routine Description:

    This routine prints an integer to the given module freeze string in
    progress.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    String - Supplies a pointer to the output in progress.

    Value - Supplies a pointer to the value to add.

Return Value:

    None.

--*/

{

    CkpFreezeAdd(Vm, String, "i", 1);
    CkpFreezeRawInteger(Vm, String, Value);
    CkpFreezeAdd(Vm, String, " ", 1);
    return;
}

VOID
CkpFreezeRawInteger (
    PCK_VM Vm,
    PCK_BYTE_ARRAY String,
    CK_INTEGER Value
    )

/*++

Routine Description:

    This routine prints an integer to the given module freeze string in
    progress.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    String - Supplies a pointer to the output in progress.

    Value - Supplies a pointer to the value to add.

Return Value:

    None.

--*/

{

    UINTN Length;
    CHAR ValueString[100];

    Length = snprintf(ValueString,
                      sizeof(ValueString),
                      "%lld",
                      (LONGLONG)Value);

    if (Length > 0) {

        CK_ASSERT(Length < sizeof(ValueString));

        CkpFreezeAdd(Vm, String, ValueString, Length);
    }

    return;
}

VOID
CkpFreezeString (
    PCK_VM Vm,
    PCK_BYTE_ARRAY String,
    PCK_STRING Value
    )

/*++

Routine Description:

    This routine prints a string to the given module freeze string in progress.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    String - Supplies a pointer to the output in progress.

    Value - Supplies a pointer to the value to add.

Return Value:

    None.

--*/

{

    CkpFreezeAdd(Vm, String, "s", 1);
    CkpFreezeRawInteger(Vm, String, Value->Length);
    CkpFreezeAdd(Vm, String, "\"", 1);
    CkpFreezeAdd(Vm, String, Value->Value, Value->Length);
    CkpFreezeAdd(Vm, String, "\"", 1);
    return;
}

VOID
CkpFreezeBuffer (
    PCK_VM Vm,
    PCK_BYTE_ARRAY String,
    UINTN Length,
    PVOID Buffer
    )

/*++

Routine Description:

    This routine prints a byte buffer to the given module freeze string in
    progress.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    String - Supplies a pointer to the output in progress.

    Length - Supplies the length of the buffer in bytes.

    Buffer - Supplies a pointer to the buffer.

Return Value:

    None.

--*/

{

    CkpFreezeAdd(Vm, String, "b", 1);
    CkpFreezeRawInteger(Vm, String, Length);
    CkpFreezeAdd(Vm, String, "\"", 1);
    CkpFreezeAdd(Vm, String, Buffer, Length);
    CkpFreezeAdd(Vm, String, "\"", 1);
    return;
}

VOID
CkpFreezeFunction (
    PCK_VM Vm,
    PCK_BYTE_ARRAY String,
    PCK_FUNCTION Function
    )

/*++

Routine Description:

    This routine prints a closure to the given module freeze string in progress.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    String - Supplies a pointer to the output in progress.

    Function - Supplies a pointer to the function to add.

Return Value:

    None.

--*/

{

    CkpFreezeAdd(Vm, String, "f{\nCode: ", 9);
    CkpFreezeBuffer(Vm, String, Function->Code.Count, Function->Code.Data);
    CkpFreezeAdd(Vm, String, "\nConstants: ", 12);
    CkpFreezeList(Vm,
                  String,
                  &(Function->Constants),
                  Function->Constants.Count,
                  0);

    CkpFreezeAdd(Vm, String, "MaxStack: ", 10);
    CkpFreezeInteger(Vm, String, Function->MaxStack);
    CkpFreezeAdd(Vm, String, "\nUpvalueCount: ", 15);
    CkpFreezeInteger(Vm, String, Function->UpvalueCount);
    CkpFreezeAdd(Vm, String, "\nArity: ", 8);
    CkpFreezeInteger(Vm, String, Function->Arity);
    CkpFreezeAdd(Vm, String, "\nName: ", 7);
    CkpFreezeString(Vm, String, Function->Debug.Name);
    CkpFreezeAdd(Vm, String, "\nFirstLine: ", 12);
    CkpFreezeInteger(Vm, String, Function->Debug.FirstLine);
    CkpFreezeAdd(Vm, String, "\nLineProgram: ", 14);
    CkpFreezeBuffer(Vm,
                    String,
                    Function->Debug.LineProgram.Count,
                    Function->Debug.LineProgram.Data);

    CkpFreezeAdd(Vm, String, "\n}", 2);
    return;
}

PCSTR
CkpThawElement (
    PCSTR *Contents,
    PUINTN Size,
    PUINTN NameSize
    )

/*++

Routine Description:

    This routine reads a dictionary entry.

Arguments:

    Contents - Supplies a pointer that on input points to the element to read.
        This is updated on output.

    Size - Supplies a pointer to the remaining size, not including a null
        terminator which may not exist.

    NameSize - Supplies a pointer where the size of the element name/key will
        be returned.

Return Value:

    Returns the address of the element key, if found.

    NULL on failure.

--*/

{

    PCSTR Current;
    PCSTR End;
    PCSTR Name;

    Current = *Contents;
    End = Current + *Size;

    //
    // Skip any space.
    //

    while ((Current < End) &&
           ((*Current == ' ') ||
            (*Current == '\t') ||
            (*Current == '\r') ||
            (*Current == '\n'))) {

        Current += 1;
    }

    Name = Current;

    //
    // Find a colon.
    //

    while ((Current < End) && (*Current != ':')) {
        Current += 1;
    }

    //
    // If there was no colon, just return the advance past the spaces. If the
    // name is a closing brace, exit.
    //

    if ((Current == End) || ((Name < End) && (*Name == '}'))) {
        *Size = End - Name;
        *Contents = Name;
        return NULL;
    }

    *NameSize = Current - Name;

    //
    // Get past the colon and any additional space.
    //

    Current += 1;
    while ((Current < End) &&
           ((*Current == ' ') ||
            (*Current == '\t') ||
            (*Current == '\r') ||
            (*Current == '\n'))) {

        Current += 1;
    }

    *Size = End - Current;
    *Contents = Current;
    return Name;
}

BOOL
CkpThawValue (
    PCK_VM Vm,
    PCK_MODULE Module,
    PCSTR *Contents,
    PUINTN Size,
    PCK_VALUE Value
    )

/*++

Routine Description:

    This routine thaws an arbitrary value. Only a few types are supported.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Module - Supplies a pointer to the module being thawed.

    Contents - Supplies a pointer that on input points to the element to read.
        This is updated on output.

    Size - Supplies a pointer to the remaining size, not including a null
        terminator which may not exist.

    Value - Supplies a pointer where the new value will be returned.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PCK_FUNCTION Function;
    CK_INTEGER Integer;
    BOOL Result;
    PCK_STRING String;

    if (*Size == 0) {
        return FALSE;
    }

    switch (**Contents) {
    case 'f':
        Result = CkpThawFunction(Vm, Module, Contents, Size, &Function);
        if (Result != FALSE) {
            CK_OBJECT_VALUE(*Value, Function);
        }

        break;

    case 'i':
        Result = CkpThawInteger(Contents, Size, &Integer);
        if (Result != FALSE) {
            CK_INT_VALUE(*Value, Integer);
        }

        break;

    case 'n':
        if (*Size < 4) {
            return FALSE;
        }

        *Size -= 4;
        *Contents += 4;
        *Value = CkNullValue;
        Result = TRUE;
        break;

    case 's':
        String = CkpThawString(Vm, Contents, Size);
        if (String == NULL) {
            return FALSE;
        }

        CK_OBJECT_VALUE(*Value, String);
        Result = TRUE;
        break;

    default:
        Result = FALSE;
        break;
    }

    return Result;
}

BOOL
CkpThawClosure (
    PCK_VM Vm,
    PCK_MODULE Module,
    PCSTR *Contents,
    PUINTN Size,
    PCK_CLOSURE *Closure
    )

/*++

Routine Description:

    This routine thaws and creates a closure.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Module - Supplies a pointer to the module being thawed.

    Contents - Supplies a pointer that on input points to the element to read.
        This is updated on output.

    Size - Supplies a pointer to the remaining size, not including a null
        terminator which may not exist.

    Closure - Supplies a pointer where the new closure will be returned.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PCK_FUNCTION Function;

    if (!CkpThawFunction(Vm, Module, Contents, Size, &Function)) {
        return FALSE;
    }

    CkpPushRoot(Vm, &(Function->Header));
    *Closure = CkpClosureCreate(Vm, Function, NULL);
    CkpPopRoot(Vm);
    if (*Closure == NULL) {
        return FALSE;
    }

    return TRUE;
}

BOOL
CkpThawFunction (
    PCK_VM Vm,
    PCK_MODULE Module,
    PCSTR *Contents,
    PUINTN Size,
    PCK_FUNCTION *NewFunction
    )

/*++

Routine Description:

    This routine thaws and creates a closure.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Module - Supplies a pointer to the module being thawed.

    Contents - Supplies a pointer that on input points to the element to read.
        This is updated on output.

    Size - Supplies a pointer to the remaining size, not including a null
        terminator which may not exist.

    NewFunction - Supplies a pointer where the new function will be returned.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PCK_FUNCTION Function;
    CK_INTEGER Integer;
    PCSTR Name;
    UINTN NameSize;
    BOOL Result;
    CK_VALUE Value;

    *NewFunction = NULL;
    if ((*Size < 3) || (**Contents != 'f')) {
        return FALSE;
    }

    Function = CkpFunctionCreate(Vm, Module, 0);
    if (Function == NULL) {
        return FALSE;
    }

    //
    // This is ugly, but temporarily use the module variables array as a place
    // to link this new function in to the garbage collection system. The
    // reason the regular roots can't be used is that this function is
    // recursive, as there could be more functions in the constants for this
    // function. So the number of pushes to the temporary roots could be
    // unbounded. Start by detecting pathologically recursive input.
    //

    if (Module->Variables.Count >
        Module->VariableNames.List.Count + CK_MAX_NESTED_FUNCTIONS) {

        return FALSE;
    }

    CkpPushRoot(Vm, &(Function->Header));
    CK_OBJECT_VALUE(Value, Function);
    if (CkpArrayAppend(Vm, &(Module->Variables), Value) != CkSuccess) {
        CkpPopRoot(Vm);
        return FALSE;
    }

    CkpPopRoot(Vm);
    Result = TRUE;
    *Contents += 2;
    *Size -= 2;
    while (Result != FALSE) {
        Name = CkpThawElement(Contents, Size, &NameSize);
        if (Name == NULL) {
            break;
        }

        if ((NameSize == 4) && (CkCompareMemory(Name, "Code", 4) == 0)) {
            Result = CkpThawBuffer(Vm, Contents, Size, &(Function->Code));

        } else if ((NameSize == 9) &&
                   (CkCompareMemory(Name, "Constants", 9) == 0)) {

            Result = CkpThawList(Vm,
                                 Module,
                                 Contents,
                                 Size,
                                 &(Function->Constants));

        } else if ((NameSize == 8) &&
                   (CkCompareMemory(Name, "MaxStack", 8) == 0)) {

            Result = CkpThawInteger(Contents, Size, &Integer);
            Function->MaxStack = Integer;

        } else if ((NameSize == 12) &&
                   (CkCompareMemory(Name, "UpvalueCount", 12) == 0)) {

            Result = CkpThawInteger(Contents, Size, &Integer);
            Function->UpvalueCount = Integer;

        } else if ((NameSize == 5) &&
                   (CkCompareMemory(Name, "Arity", 5) == 0)) {

            Result = CkpThawInteger(Contents, Size, &Integer);
            Function->Arity = Integer;

        } else if ((NameSize == 4) &&
                   (CkCompareMemory(Name, "Name", 4) == 0)) {

            Function->Debug.Name = CkpThawString(Vm, Contents, Size);
            if (Function->Debug.Name == NULL) {
                Result = FALSE;
            }

        } else if ((NameSize == 9) &&
                   (CkCompareMemory(Name, "FirstLine", 9) == 0)) {

            Result = CkpThawInteger(Contents, Size, &Integer);
            Function->Debug.FirstLine = Integer;

        } else if ((NameSize == 11) &&
                   (CkCompareMemory(Name, "LineProgram", 11) == 0)) {

            Result = CkpThawBuffer(Vm,
                                   Contents,
                                   Size,
                                   &(Function->Debug.LineProgram));

        } else {
            Result = FALSE;
        }
    }

    if ((Function->Code.Count == 0) ||
        (Function->Debug.Name == NULL)) {

        Result = FALSE;
        goto ThawFunctionEnd;
    }

    if ((*Size == 0) || (**Contents != '}')) {
        return FALSE;
    }

    *Size -= 1;
    *Contents += 1;
    *NewFunction = Function;

ThawFunctionEnd:

    //
    // Pop the function off the module variables list where it was temporarily
    // living to prevent garbage collection.
    //

    CK_ASSERT(Module->Variables.Count > Module->VariableNames.List.Count);

    Module->Variables.Count -= 1;
    return Result;
}

BOOL
CkpThawInteger (
    PCSTR *Contents,
    PUINTN Size,
    PCK_INTEGER Integer
    )

/*++

Routine Description:

    This routine reads an integer.

Arguments:

    Contents - Supplies a pointer that on input points to the element to read.
        This is updated on output.

    Size - Supplies a pointer to the remaining size, not including a null
        terminator which may not exist.

    Integer - Supplies a pointer to the integer to read.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSTR AfterScan;

    if ((*Size < 2) || (**Contents != 'i')) {
        return FALSE;
    }

    AfterScan = NULL;
    *Integer = strtoll(*Contents + 1, &AfterScan, 10);
    if ((AfterScan == NULL) || (AfterScan == *Contents) ||
        (AfterScan >= *Contents + *Size) || (*AfterScan != ' ')) {

        return FALSE;
    }

    AfterScan += 1;
    *Size = (*Contents + *Size) - AfterScan;
    *Contents = AfterScan;
    return TRUE;
}

BOOL
CkpThawStringTable (
    PCK_VM Vm,
    PCK_MODULE Module,
    PCSTR *Contents,
    PUINTN Size,
    PCK_STRING_TABLE Table
    )

/*++

Routine Description:

    This routine reads and restores a string table.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Module - Supplies a pointer to the module being thawed.

    Contents - Supplies a pointer that on input points to the element to read.
        This is updated on output.

    Size - Supplies a pointer to the remaining size, not including a null
        terminator which may not exist.

    Table - Supplies a pointer to the string table to thaw.

Return Value:

    Returns a pointer to the string on success.

    NULL on failure.

--*/

{

    UINTN Index;
    UINTN StartIndex;
    CK_VALUE Value;

    StartIndex = Table->List.Count;
    if (!CkpThawList(Vm, Module, Contents, Size, &(Table->List))) {
        return FALSE;
    }

    //
    // Also insert all the elements in the list into the dictionary in case
    // more compilation occurs.
    //

    for (Index = StartIndex; Index < Table->List.Count; Index += 1) {
        CK_INT_VALUE(Value, Index);
        CkpDictSet(Vm, Table->Dict, Table->List.Data[Index], Value);
    }

    if (Table->Dict->Count != Table->List.Count) {
        return FALSE;
    }

    return TRUE;
}

BOOL
CkpThawList (
    PCK_VM Vm,
    PCK_MODULE Module,
    PCSTR *Contents,
    PUINTN Size,
    PCK_VALUE_ARRAY List
    )

/*++

Routine Description:

    This routine reads a list.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Module - Supplies a pointer to the module being thawed.

    Contents - Supplies a pointer that on input points to the element to read.
        This is updated on output.

    Size - Supplies a pointer to the remaining size, not including a null
        terminator which may not exist.

    List - Supplies a pointer to a list to thaw.

Return Value:

    Returns a pointer to the string on success.

    NULL on failure.

--*/

{

    UINTN Count;
    PSTR Current;
    UINTN Index;
    CK_VALUE Value;

    if ((*Size < 4) || (**Contents != 'l')) {
        return FALSE;
    }

    Count = strtoull(*Contents + 1, &Current, 10);
    if ((Current == NULL) || (Current == *Contents) ||
        (Current >= *Contents + *Size)) {

        return FALSE;
    }

    if (*Current != '[') {
        return FALSE;
    }

    if (CkpSizeArray(Vm, List, List->Count + Count) != CkSuccess) {
        return FALSE;
    }

    *Size = *Contents + *Size - (Current + 1);
    *Contents = Current + 1;
    for (Index = 0; Index < Count; Index += 1) {
        if (!CkpThawValue(Vm, Module, Contents, Size, &Value)) {
            return FALSE;
        }

        //
        // The sizing before the loop ensures capacity, so there's no need
        // to check the return value of the append operation.
        //

        CkpArrayAppend(Vm, List, Value);
        if (Index != Count - 1) {
            if ((*Size <= 2) || (**Contents != ',')) {
                return FALSE;
            }

            *Contents += 2;
            *Size -= 2;
        }
    }

    if ((*Size == 0) || (**Contents != ']')) {
        return FALSE;
    }

    *Contents += 1;
    *Size -= 1;
    return TRUE;
}

PCK_STRING
CkpThawString (
    PCK_VM Vm,
    PCSTR *Contents,
    PUINTN Size
    )

/*++

Routine Description:

    This routine reads a string.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Contents - Supplies a pointer that on input points to the element to read.
        This is updated on output.

    Size - Supplies a pointer to the remaining size, not including a null
        terminator which may not exist.

Return Value:

    Returns a pointer to the string on success.

    NULL on failure.

--*/

{

    PSTR AfterScan;
    UINTN StringSize;
    CK_VALUE Value;

    if ((*Size < 2) || (**Contents != 's')) {
        return NULL;
    }

    StringSize = strtoull(*Contents + 1, &AfterScan, 10);
    if ((AfterScan == NULL) || (AfterScan == *Contents) ||
        (AfterScan >= *Contents + *Size) ||
        (AfterScan + 2 + StringSize > *Contents + *Size)) {

        return NULL;
    }

    AfterScan += 1;
    Value = CkpStringCreate(Vm, AfterScan, StringSize);
    if (!CK_IS_STRING(Value)) {
        return NULL;
    }

    AfterScan += StringSize;
    if (*AfterScan != '"') {
        return NULL;
    }

    AfterScan += 1;
    *Size -= AfterScan - *Contents;
    *Contents = AfterScan;
    return CK_AS_STRING(Value);
}

BOOL
CkpThawBuffer (
    PCK_VM Vm,
    PCSTR *Contents,
    PUINTN Size,
    PCK_BYTE_ARRAY Buffer
    )

/*++

Routine Description:

    This routine reads a byte buffer.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Contents - Supplies a pointer that on input points to the element to read.
        This is updated on output.

    Size - Supplies a pointer to the remaining size, not including a null
        terminator which may not exist.

    Buffer - Supplies a pointer to the buffer to fill with thawed data.

Return Value:

    Returns a pointer to the string on success.

    NULL on failure.

--*/

{

    PSTR AfterScan;
    UINTN BufferSize;
    PVOID NewBuffer;

    if ((*Size < 2) || (**Contents != 'b')) {
        return FALSE;
    }

    BufferSize = strtoull(*Contents + 1, &AfterScan, 10);
    if ((AfterScan == NULL) || (AfterScan == *Contents) ||
        (AfterScan >= *Contents + *Size) ||
        (AfterScan + 2 + BufferSize > *Contents + *Size)) {

        return FALSE;
    }

    if (Buffer->Capacity < BufferSize) {
        NewBuffer = CkpReallocate(Vm,
                                  Buffer->Data,
                                  Buffer->Capacity,
                                  BufferSize);

        if (NewBuffer == NULL) {
            return FALSE;
        }

        Buffer->Data = NewBuffer;
        Buffer->Capacity = BufferSize;
    }

    AfterScan += 1;
    CkCopy(Buffer->Data, AfterScan, BufferSize);
    Buffer->Count = BufferSize;
    AfterScan += BufferSize;
    if (*AfterScan != '"') {
        return FALSE;
    }

    AfterScan += 1;
    *Size -= AfterScan - *Contents;
    *Contents = AfterScan;
    return TRUE;
}

