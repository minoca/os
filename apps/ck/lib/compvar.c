/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    compvar.c

Abstract:

    This module implements variable related support for compiling Chalk
    source into bytecode.

Author:

    Evan Green 9-Jun-2016

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <stdio.h>

#include "chalkp.h"
#include <minoca/lib/status.h>
#include <minoca/lib/yy.h>
#include "compiler.h"
#include "lang.h"
#include "compsup.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

CK_SYMBOL_INDEX
CkpFindUpvalue (
    PCK_COMPILER Compiler,
    PCSTR Name,
    UINTN Length
    );

CK_SYMBOL_INDEX
CkpAddUpvalue (
    PCK_COMPILER Compiler,
    BOOL IsLocal,
    CK_SYMBOL_INDEX Symbol
    );

CK_SYMBOL_INDEX
CkpResolveLocal (
    PCK_COMPILER Compiler,
    PCSTR Name,
    UINTN Length
    );

CK_SYMBOL_INDEX
CkpFindFunctionDeclaration (
    PCK_COMPILER Compiler,
    PCK_FUNCTION_SIGNATURE Signature,
    BOOL Remove
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

CK_SYMBOL_INDEX
CkpDeclareMethod (
    PCK_COMPILER Compiler,
    PCK_FUNCTION_SIGNATURE Signature,
    BOOL IsStatic,
    PLEXER_TOKEN NameToken,
    PSTR Name,
    UINTN Length
    )

/*++

Routine Description:

    This routine declares a method with the given signature, giving it a slot
    in the giant global method table, and giving it a slot in the class methods.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Signature - Supplies a pointer to the function signature information.

    IsStatic - Supplies a boolean indicating if this is a static method or not.

    NameToken - Supplies a pointer to the function name token, for non-method
        functions.

    Name - Supplies a pointer to the signature string.

    Length - Supplies the length of the signature string, not including the
        null terminator.

Return Value:

    Returns the index into the table of methods for this function's signature.

--*/

{

    PCK_CLASS_COMPILER EnclosingClass;
    CK_SYMBOL_INDEX Index;
    PCK_INT_ARRAY MethodArray;
    PSTR StaticString;
    CK_SYMBOL_INDEX Symbol;

    //
    // If there's no enclosing class, then this is a local or global function
    // being declared. Create a variable with its name.
    //

    EnclosingClass = Compiler->EnclosingClass;
    if (EnclosingClass == NULL) {

        CK_ASSERT(IsStatic == FALSE);

        //
        // First try to find an existing declaration.
        //

        Symbol = CkpFindFunctionDeclaration(Compiler, Signature, TRUE);
        if (Symbol != -1) {
            return Symbol;
        }

        return CkpDeclareVariable(Compiler, NameToken);
    }

    Symbol = CkpGetSignatureSymbol(Compiler, Signature);

    //
    // Make sure this class doesn't already have this method.
    //

    if (IsStatic != FALSE) {
        StaticString = "static ";
        MethodArray = &(EnclosingClass->StaticMethods);

    } else {
        StaticString = "";
        MethodArray = &(EnclosingClass->Methods);
    }

    for (Index = 0; Index < MethodArray->Count; Index += 1) {
        if (MethodArray->Data[Index] == Symbol) {
            CkpCompileError(Compiler,
                            NULL,
                            "Class %s already defines %smethod '%s'",
                            EnclosingClass->Name->Value,
                            StaticString,
                            Name);

            break;
        }
    }

    CkpArrayAppend(Compiler->Parser->Vm, MethodArray, Symbol);
    return Symbol;
}

CK_SYMBOL_INDEX
CkpGetSignatureSymbol (
    PCK_COMPILER Compiler,
    PCK_FUNCTION_SIGNATURE Signature
    )

/*++

Routine Description:

    This routine finds or creates a symbol index in the giant array of all
    method signatures.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Signature - Supplies a pointer to the function signature information.

Return Value:

    Returns the index into the table of methods for this function's signature.

--*/

{

    UINTN Length;
    CHAR Name[CK_MAX_METHOD_SIGNATURE];

    Length = sizeof(Name);
    CkpPrintSignature(Signature, Name, &Length);
    return CkpGetMethodSymbol(Compiler, Name, Length);
}

CK_SYMBOL_INDEX
CkpGetMethodSymbol (
    PCK_COMPILER Compiler,
    PSTR Name,
    UINTN Length
    )

/*++

Routine Description:

    This routine returns the symbol for the given signature. If it did not
    previously exist, it is created.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Name - Supplies a pointer to the method signature string.

    Length - Supplies the length of the method name, not including the null
        terminator.

Return Value:

    Returns the index in the symbol table of method names.

--*/

{

    CK_SYMBOL_INDEX Symbol;

    Symbol = CkpStringTableEnsure(Compiler->Parser->Vm,
                                  &(Compiler->Function->Module->Strings),
                                  Name,
                                  Length);

    return Symbol;
}

PCK_CLASS_COMPILER
CkpGetClassCompiler (
    PCK_COMPILER Compiler
    )

/*++

Routine Description:

    This routine walks up the compiler chain looking for the most recent class
    being defined.

Arguments:

    Compiler - Supplies a pointer to the compiler.

Return Value:

    Returns the innermost class being defined.

    NULL if a class is not currently being defined.

--*/

{

    while (Compiler != NULL) {
        if (Compiler->EnclosingClass != NULL) {
            return Compiler->EnclosingClass;
        }

        Compiler = Compiler->Parent;
    }

    return NULL;
}

VOID
CkpLoadCoreVariable (
    PCK_COMPILER Compiler,
    PSTR Name
    )

/*++

Routine Description:

    This routine pushes one of the module-level variables from the core onto
    the stack.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Name - Supplies a pointer to the core name to load.

Return Value:

    None. The core variable is pushed onto the stack.

--*/

{

    CK_SYMBOL_INDEX Symbol;

    Symbol = CkpStringTableFind(&(Compiler->Parser->Module->VariableNames),
                                Name,
                                strlen(Name));

    CK_ASSERT(Symbol >= 0);

    CkpEmitShortOp(Compiler, CkOpLoadModuleVariable, Symbol);
    return;
}

VOID
CkpLoadThis (
    PCK_COMPILER Compiler,
    PLEXER_TOKEN Token
    )

/*++

Routine Description:

    This routine loads the "this" local variable.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Token - Supplies a pointer to the token, to point to in case it was an
        inappropriate scope for this.

Return Value:

    None.

--*/

{

    CK_VARIABLE This;

    This = CkpResolveNonGlobal(Compiler, "this", 4);
    if (This.Index == -1) {
        CkpCompileError(Compiler, Token, "\"this\" used outside class method");
        return;
    }

    CkpLoadVariable(Compiler, This);
    return;
}

CK_VARIABLE
CkpResolveNonGlobal (
    PCK_COMPILER Compiler,
    PCSTR Name,
    UINTN Length
    )

/*++

Routine Description:

    This routine finds the local variable or upvalue with the given name. It
    will not find module level variables.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Name - Supplies the name of the variable to find.

    Length - Supplies the length of the name, not including the null terminator.

Return Value:

    Returns the variable information on success.

--*/

{

    CK_VARIABLE Variable;

    Variable.Scope = CkScopeLocal;
    Variable.Index = CkpResolveLocal(Compiler, Name, Length);
    if (Variable.Index == -1) {
        Variable.Scope = CkScopeUpvalue;
        Variable.Index = CkpFindUpvalue(Compiler, Name, Length);
        if (Variable.Index == -1) {
            Variable.Scope = CkScopeInvalid;
        }
    }

    return Variable;
}

VOID
CkpLoadVariable (
    PCK_COMPILER Compiler,
    CK_VARIABLE Variable
    )

/*++

Routine Description:

    This routine stores a variable with a previously defined symbol index in
    the current scope from the value at the top of the stack.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Variable - Supplies the variable to load.

Return Value:

    None.

--*/

{

    switch (Variable.Scope) {
    case CkScopeLocal:
        CkpLoadLocal(Compiler, Variable.Index);
        break;

    case CkScopeUpvalue:
        CkpEmitByteOp(Compiler, CkOpLoadUpvalue, Variable.Index);
        break;

    case CkScopeModule:
        CkpEmitShortOp(Compiler, CkOpLoadModuleVariable, Variable.Index);
        break;

    default:

        CK_ASSERT(FALSE);

        break;
    }

    return;
}

VOID
CkpDefineVariable (
    PCK_COMPILER Compiler,
    CK_SYMBOL_INDEX Symbol
    )

/*++

Routine Description:

    This routine stores a variable with a previously defined symbol index in
    the current scope from the value at the top of the stack.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Symbol - Supplies the symbol being defined.

Return Value:

    None.

--*/

{

    //
    // If this is a local, the result of the initializer now on the stack is in
    // just the right place. Do nothing.
    //

    if (Compiler->ScopeDepth >= 0) {
        return;
    }

    //
    // Store the value into the module level variable.
    //

    CkpEmitShortOp(Compiler, CkOpStoreModuleVariable, Symbol);
    CkpEmitOp(Compiler, CkOpPop);
    return;
}

CK_SYMBOL_INDEX
CkpDeclareVariable (
    PCK_COMPILER Compiler,
    PLEXER_TOKEN Token
    )

/*++

Routine Description:

    This routine creates a new variable slot in the current scope.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Token - Supplies the token containing the name of the variable.

Return Value:

    Returns the index of the new variable.

--*/

{

    CK_SYMBOL_INDEX Index;
    PCK_LOCAL Local;
    PCSTR Name;
    CK_SYMBOL_INDEX Symbol;

    if (Token->Size > CK_MAX_NAME) {
        CkpCompileError(Compiler, Token, "Name too long");
        return -1;
    }

    Name = Compiler->Parser->Source + Token->Position;
    if (Compiler->ScopeDepth == -1) {
        Symbol = CkpDefineModuleVariable(Compiler->Parser->Vm,
                                         Compiler->Parser->Module,
                                         Name,
                                         Token->Size,
                                         CK_NULL_VALUE);

        if (Symbol == -1) {
            CkpCompileError(Compiler,
                            Token,
                            "Module variable is already defined");

        } else if (Symbol == -2) {
            CkpCompileError(Compiler, Token, "Too many module level variables");
        }

        return Symbol;
    }

    //
    // Search for a local that might already be in this scope.
    //

    for (Index = Compiler->LocalCount - 1; Index >= 0; Index -= 1) {
        Local = &(Compiler->Locals[Index]);

        //
        // Stop looking if an outer scope is hit.
        //

        if (Local->Scope < Compiler->ScopeDepth) {
            break;
        }

        if ((Local->Length == Token->Size) &&
            (CkCompareMemory(Local->Name, Name, Local->Length) == 0)) {

            CkpCompileError(Compiler,
                            Token,
                            "Variable already declared in this scope");

            return Index;
        }
    }

    if (Compiler->LocalCount >= CK_MAX_LOCALS) {
        CkpCompileError(Compiler, Token, "Too many locals");
        return -1;
    }

    return CkpAddLocal(Compiler, Name, Token->Size);
}

VOID
CkpPushScope (
    PCK_COMPILER Compiler
    )

/*++

Routine Description:

    This routine pushes a new local variable scope in the compiler.

Arguments:

    Compiler - Supplies a pointer to the compiler.

Return Value:

    None.

--*/

{

    Compiler->ScopeDepth += 1;
    return;
}

VOID
CkpPopScope (
    PCK_COMPILER Compiler
    )

/*++

Routine Description:

    This routine pops the most recent local variable scope, and clears any
    knowledge of local variables defined at that scope.

Arguments:

    Compiler - Supplies a pointer to the compiler.

Return Value:

    None.

--*/

{

    CK_SYMBOL_INDEX Index;
    CK_SYMBOL_INDEX Popped;

    //
    // Discard declarations.
    //

    Index = Compiler->DeclarationCount - 1;
    while ((Index >= 0) &&
           (Compiler->Declarations[Index].Scope >= Compiler->ScopeDepth)) {

        Index -= 1;
    }

    Popped = CkpDiscardLocals(Compiler, Compiler->ScopeDepth);
    Compiler->LocalCount -= Popped;
    Compiler->StackSlots -= Popped;
    Compiler->ScopeDepth -= 1;
    return;
}

CK_SYMBOL_INDEX
CkpDiscardLocals (
    PCK_COMPILER Compiler,
    LONG Depth
    )

/*++

Routine Description:

    This routine emits pop instructions to discard local variables up to a
    given depth. This doesn't actually undeclare the variables.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Depth - Supplies the depth of locals to discard.

Return Value:

    Returns the number of symbols popped.

--*/

{

    CK_SYMBOL_INDEX Index;

    CK_ASSERT(Compiler->ScopeDepth >= 0);

    Index = Compiler->LocalCount - 1;
    while ((Index >= 0) && (Compiler->Locals[Index].Scope >= Depth)) {

        //
        // If the local was closed over, make sure the upvalue gets closed as
        // this variable goes out of scope. Emit the byte directly as opposed
        // to the op because the stack effect shouldn't be tracked.
        //

        if (Compiler->Locals[Index].IsUpvalue != FALSE) {
            CkpEmitByte(Compiler, CkOpCloseUpvalue);

        } else {
            CkpEmitByte(Compiler, CkOpPop);
        }

        Index -= 1;
    }

    return Compiler->LocalCount - Index - 1;
}

VOID
CkpLoadLocal (
    PCK_COMPILER Compiler,
    CK_SYMBOL_INDEX Symbol
    )

/*++

Routine Description:

    This routine loads a local variable and pushes it onto the stack.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Symbol - Supplies the index of the local variable to load.

Return Value:

    None.

--*/

{

    if (Symbol <= 8) {
        CkpEmitOp(Compiler, CkOpLoadLocal0 + Symbol);

    } else {
        CkpEmitByteOp(Compiler, CkOpLoadLocal, Symbol);
    }

    return;
}

CK_SYMBOL_INDEX
CkpAddLocal (
    PCK_COMPILER Compiler,
    PCSTR Name,
    UINTN Length
    )

/*++

Routine Description:

    This routine unconditionally creates a new local variable with the given
    name.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Name - Supplies a pointer to the name of the variable. This pointer will be
        used directly.

    Length - Supplies the length of the local in bytes, not including the
        null terminator.

Return Value:

    Returns the index of the symbol.

    -1 on error.

--*/

{

    PCK_LOCAL Local;
    PVOID NewBuffer;
    UINTN NewCapacity;

    if (Compiler->LocalCount >= Compiler->LocalCapacity) {
        NewCapacity = Compiler->LocalCapacity * 2;
        NewBuffer = CkpReallocate(Compiler->Parser->Vm,
                                  Compiler->Locals,
                                  Compiler->LocalCapacity * sizeof(CK_LOCAL),
                                  NewCapacity * sizeof(CK_LOCAL));

        if (NewBuffer == NULL) {
            return -1;
        }

        Compiler->Locals = NewBuffer;
        Compiler->LocalCapacity = NewCapacity;
    }

    Local = &(Compiler->Locals[Compiler->LocalCount]);
    Local->Name = Name;
    Local->Length = Length;
    Local->Scope = Compiler->ScopeDepth;
    Local->IsUpvalue = FALSE;
    Compiler->LocalCount += 1;
    return Compiler->LocalCount - 1;
}

CK_SYMBOL_INDEX
CkpAddConstant (
    PCK_COMPILER Compiler,
    CK_VALUE Constant
    )

/*++

Routine Description:

    This routine adds a new constant value to the current function.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Constant - Supplies the constant to add.

Return Value:

    Returns the index of the constant.

    -1 if the compiler already has an error.

--*/

{

    PCK_OBJECT Object;

    if (Compiler->Parser->Errors != 0) {
        return -1;
    }

    if (Compiler->Function->Constants.Count < CK_MAX_CONSTANTS) {
        if (CK_IS_OBJECT(Constant)) {
            Object = CK_AS_OBJECT(Constant);

            //
            // Strings belong in their own constant table.
            //

            CK_ASSERT(Object->Type != CkObjectString);

            CkpPushRoot(Compiler->Parser->Vm, Object);
        }

        CkpArrayAppend(Compiler->Parser->Vm,
                       &(Compiler->Function->Constants),
                       Constant);

        if (CK_IS_OBJECT(Constant)) {
            CkpPopRoot(Compiler->Parser->Vm);
        }

    } else {
        CkpCompileError(Compiler, NULL, "Too many constants");
    }

    return Compiler->Function->Constants.Count - 1;
}

CK_SYMBOL_INDEX
CkpAddStringConstant (
    PCK_COMPILER Compiler,
    CK_VALUE Constant
    )

/*++

Routine Description:

    This routine adds a new string constant value to the current function.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Constant - Supplies the string constant to add.

Return Value:

    Returns the index of the constant.

    -1 if the compiler already has an error.

--*/

{

    CK_SYMBOL_INDEX Index;

    CK_ASSERT((CK_IS_OBJECT(Constant)) &&
              (CK_AS_OBJECT(Constant)->Type == CkObjectString));

    if (Compiler->Parser->Errors != 0) {
        return -1;
    }

    Index = CkpStringTableEnsureValue(Compiler->Parser->Vm,
                                      &(Compiler->Function->Module->Strings),
                                      Constant);

    if (Index >= CK_MAX_CONSTANTS) {
        CkpCompileError(Compiler, NULL, "Too many string constants");
        Index = -1;
    }

    return Index;
}

VOID
CkpComplainIfAssigning (
    PCK_COMPILER Compiler,
    PLEXER_TOKEN Token,
    PSTR ExpressionName
    )

/*++

Routine Description:

    This routine complains if the compiler is in the middle of trying to get an
    lvalue for assignment.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Token - Supplies a token that is part of the expression that is not an
        lvalue.

    ExpressionName - Supplies a pointer to the name of the expression.

Return Value:

    None.

--*/

{

    if (Compiler->Assign != FALSE) {
        CkpCompileError(Compiler, Token, "%s is not an lvalue", ExpressionName);
    }

    return;
}

VOID
CkpAddFunctionDeclaration (
    PCK_COMPILER Compiler,
    PCK_FUNCTION_SIGNATURE Signature,
    PLEXER_TOKEN NameToken
    )

/*++

Routine Description:

    This routine adds a function declaration.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Signature - Supplies the function signature.

    NameToken - Supplies the name token of the variable.

Return Value:

    None.

--*/

{

    LONG Index;
    PVOID NewBuffer;
    ULONG NewCapacity;
    CK_SYMBOL_INDEX SignatureSymbol;
    CK_SYMBOL_INDEX Symbol;

    SignatureSymbol = CkpGetSignatureSymbol(Compiler, Signature);
    if (SignatureSymbol == -1) {
        return;
    }

    //
    // Make sure there's room for one more, even if it's not needed.
    //

    if (Compiler->DeclarationCount >= Compiler->DeclarationCapacity) {
        if (Compiler->DeclarationCount == 0) {
            NewCapacity = 8;

        } else {
            NewCapacity = Compiler->DeclarationCapacity * 2;
        }

        NewBuffer = CkpReallocate(
               Compiler->Parser->Vm,
               Compiler->Declarations,
               Compiler->DeclarationCapacity * sizeof(CK_FUNCTION_DECLARATION),
               NewCapacity * sizeof(CK_FUNCTION_DECLARATION));

        if (NewBuffer == NULL) {
            return;
        }

        Compiler->Declarations = NewBuffer;
        Compiler->DeclarationCapacity = NewCapacity;
    }

    for (Index = Compiler->DeclarationCount - 1; Index >= 0; Index -= 1) {

        //
        // If there's already a declaration, return it.
        //

        if ((Compiler->Declarations[Index].Signature == SignatureSymbol) &&
            (Compiler->Declarations[Index].Scope == Compiler->ScopeDepth)) {

            return;
        }

        //
        // Stop looking if a lower scope is hit.
        //

        if (Compiler->Declarations[Index].Scope < Compiler->ScopeDepth) {
            break;
        }
    }

    Index = Compiler->DeclarationCount;

    //
    // Add the new declaration. Push a null to instantiate the variable.
    //

    CkpEmitOp(Compiler, CkOpNull);
    Symbol = CkpDeclareVariable(Compiler, NameToken);
    CkpDefineVariable(Compiler, Symbol);
    Compiler->Declarations[Index].Signature = SignatureSymbol;
    Compiler->Declarations[Index].Scope = Compiler->ScopeDepth;
    Compiler->Declarations[Index].Symbol = Symbol;
    Compiler->DeclarationCount += 1;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

CK_SYMBOL_INDEX
CkpFindUpvalue (
    PCK_COMPILER Compiler,
    PCSTR Name,
    UINTN Length
    )

/*++

Routine Description:

    This routine attempts to find an upvalue, and notes its use in the current
    compiler.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Name - Supplies a pointer to the name of the variable.

    Length - Supplies the length of the upvalue in bytes, not including the
        null terminator.

Return Value:

    Returns the index of the symbol.

    -1 on error.

--*/

{

    CK_SYMBOL_INDEX Symbol;

    //
    // If this is the top level compiler, then it's not there.
    //

    if (Compiler->Parent == NULL) {
        return -1;
    }

    //
    // Try to find it as a local in the parent function.
    //

    Symbol = CkpResolveLocal(Compiler->Parent, Name, Length);
    if (Symbol != -1) {

        //
        // Note that variable as an upvalue in the parent.
        //

        Compiler->Parent->Locals[Symbol].IsUpvalue = TRUE;
        return CkpAddUpvalue(Compiler, TRUE, Symbol);
    }

    //
    // Recurse to see if it's a variable in the enclosing function. This
    // recursion will create upvalues up the function definition stack.
    //

    Symbol = CkpFindUpvalue(Compiler->Parent, Name, Length);
    if (Symbol != -1) {
        return CkpAddUpvalue(Compiler, FALSE, Symbol);
    }

    //
    // The recursion went all the way up and didn't find anything.
    //

    return -1;
}

CK_SYMBOL_INDEX
CkpAddUpvalue (
    PCK_COMPILER Compiler,
    BOOL IsLocal,
    CK_SYMBOL_INDEX Symbol
    )

/*++

Routine Description:

    This routine adds an upvalue to the compiler's current list of upvalues, or
    at least ensures it is already known in the compiler.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    IsLocal - Supplies a boolean indicating if the new upvalue closes over a
        local or another upvalue.

    Symbol - Supplies the variable index that is an upvalue.

Return Value:

    Returns the index of the upvalue.

    -1 on error.

--*/

{

    CK_SYMBOL_INDEX Count;
    CK_SYMBOL_INDEX Index;
    PVOID NewBuffer;
    UINTN NewCapacity;
    PCK_COMPILER_UPVALUE Upvalue;

    Count = Compiler->Function->UpvalueCount;
    for (Index = 0; Index < Count; Index += 1) {
        Upvalue = &(Compiler->Upvalues[Index]);
        if ((Upvalue->Index == Symbol) && (Upvalue->IsLocal == IsLocal)) {
            return Index;
        }
    }

    if (Count >= Compiler->UpvalueCapacity) {
        NewCapacity = Compiler->UpvalueCapacity * 2;
        if (NewCapacity == 0) {
            NewCapacity = 32;
        }

        NewBuffer = CkpReallocate(
                       Compiler->Parser->Vm,
                       Compiler->Upvalues,
                       Compiler->UpvalueCapacity * sizeof(CK_COMPILER_UPVALUE),
                       NewCapacity * sizeof(CK_COMPILER_UPVALUE));

        if (NewBuffer == NULL) {
            return -1;
        }

        Compiler->Upvalues = NewBuffer;
        Compiler->UpvalueCapacity = NewCapacity;
    }

    Compiler->Upvalues[Count].IsLocal = IsLocal;
    Compiler->Upvalues[Count].Index = Symbol;
    Compiler->Function->UpvalueCount += 1;
    return Count;
}

CK_SYMBOL_INDEX
CkpResolveLocal (
    PCK_COMPILER Compiler,
    PCSTR Name,
    UINTN Length
    )

/*++

Routine Description:

    This routine attempts to find a local variable.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Name - Supplies a pointer to the name of the variable.

    Length - Supplies the length of the local in bytes, not including the
        null terminator.

Return Value:

    Returns the index of the symbol.

    -1 on error.

--*/

{

    CK_SYMBOL_INDEX Index;

    //
    // Search in reverse order so that the most recently scoped variables are
    // found first.
    //

    for (Index = Compiler->LocalCount - 1;
         Index >= 0;
         Index -= 1) {

        if ((Compiler->Locals[Index].Length == Length) &&
            (CkCompareMemory(Compiler->Locals[Index].Name, Name, Length) ==
             0)) {

            return Index;
        }
    }

    return -1;
}

CK_SYMBOL_INDEX
CkpFindFunctionDeclaration (
    PCK_COMPILER Compiler,
    PCK_FUNCTION_SIGNATURE Signature,
    BOOL Remove
    )

/*++

Routine Description:

    This routine finds a function declaration.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Signature - Supplies the function signature.

    Remove - Supplies a boolean indicating whether to remove the declaration
        or not. TRUE is supplied when the function is being defined.

Return Value:

    Returns the variable index for the function in the current scope.

--*/

{

    ULONG Index;
    CK_SYMBOL_INDEX SignatureSymbol;

    SignatureSymbol = CkpGetSignatureSymbol(Compiler, Signature);
    if (SignatureSymbol == -1) {
        return -1;
    }

    for (Index = 0; Index < Compiler->DeclarationCount; Index += 1) {
        if ((Compiler->Declarations[Index].Signature == SignatureSymbol) &&
            (Compiler->Declarations[Index].Scope == Compiler->ScopeDepth)) {

            if (Remove != FALSE) {
                Compiler->Declarations[Index].Signature = -1;
            }

            return Compiler->Declarations[Index].Symbol;
        }

        //
        // Stop looking if a lower scope is hit.
        //

        if (Compiler->Declarations[Index].Scope < Compiler->ScopeDepth) {
            break;
        }
    }

    return -1;
}

