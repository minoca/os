/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    compiler.c

Abstract:

    This module implements support for compiling Chalk source code into
    bytecode.

Author:

    Evan Green 29-May-2016

Environment:

    Any

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
#include "debug.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
VOID
(*PCK_COMPILER_NODE_VISITOR) (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

/*++

Routine Description:

    This routine represents the prototype of the function called to visit a
    node in the abstract syntax tree.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

//
// ----------------------------------------------- Internal Function Prototypes
//

CK_ERROR_TYPE
CkpInitializeCompiler (
    PCK_COMPILER Compiler,
    PCK_PARSER Parser,
    PCK_COMPILER Parent,
    BOOL IsFunction
    );

PCK_FUNCTION
CkpFinalizeCompiler (
    PCK_COMPILER Compiler,
    PCSTR DebugName,
    UINTN DebugNameLength
    );

VOID
CkpVisitImportStatement (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

VOID
CkpVisitClassDefinition (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

VOID
CkpVisitModuleName (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

VOID
CkpVisitFunctionDefinition (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

VOID
CkpVisitFunctionDeclaration (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

VOID
CkpVisitExceptStatement (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

VOID
CkpVisitTryStatement (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

VOID
CkpVisitJumpStatement (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

VOID
CkpVisitIterationStatement (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

VOID
CkpVisitSelectionStatement (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

VOID
CkpVisitExpressionStatement (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

VOID
CkpVisitCompoundStatement (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

VOID
CkpVisitVariableDefinition (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

VOID
CkpVisitVariableDeclaration (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

VOID
CkpVisitVariableSpecifier (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

VOID
CkpVisitChildren (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

VOID
CkpVisitLeftRecursiveList (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

VOID
CkpReportCompileError (
    PCK_PARSER Parser,
    INT Line,
    PSTR Label,
    PSTR MessageFormat,
    va_list ArgumentList
    );

PVOID
CkpCompilerReallocate (
    PVOID Context,
    PVOID Allocation,
    UINTN Size
    );

INT
CkpParserCallback (
    PVOID Context,
    YY_VALUE Symbol,
    PVOID Elements,
    INT ElementCount,
    PVOID ReducedElement
    );

YY_STATUS
CkpParserError (
    PVOID Context,
    YY_STATUS Status
    );

//
// -------------------------------------------------------------------- Globals
//

PCK_COMPILER_NODE_VISITOR CkCompilerNodeFunctions[] = {
    CkpVisitListElementList, // CkNodeListElementList,
    CkpVisitList, // CkNodeList,
    CkpVisitChildren, // CkNodeDictElement,
    CkpVisitDictElementList, // CkNodeDictElementList,
    CkpVisitDict, // CkNodeDict,
    NULL, // CkNodeStringLiteralList,
    CkpVisitPrimaryExpression, // CkNodePrimaryExpression,
    CkpVisitPostfixExpression, // CkNodePostfixExpression,
    CkpVisitLeftRecursiveList, // CkNodeArgumentExpressionList,
    CkpVisitUnaryExpression, // CkNodeUnaryExpression,
    NULL, // CkNodeUnaryOperator,
    CkpVisitBinaryExpression, // CkNodeBinaryExpression,
    CkpVisitConditionalExpression, // CkNodeConditionalExpression,
    CkpVisitAssignmentExpression, // CkNodeAssignmentExpression,
    NULL, // CkNodeAssignmentOperator,
    CkpVisitExpression, // CkNodeExpression,
    CkpVisitVariableSpecifier, // CkNodeVariableSpecifier,
    CkpVisitVariableDeclaration, // CkNodeVariableDeclaration,
    CkpVisitVariableDefinition, // CkNodeVariableDefinition,
    CkpVisitChildren, // CkNodeStatement,
    CkpVisitCompoundStatement, // CkNodeCompoundStatement,
    CkpVisitLeftRecursiveList, // CkNodeStatementList,
    CkpVisitExpressionStatement, // CkNodeExpressionStatement,
    CkpVisitSelectionStatement, // CkNodeSelectionStatement,
    CkpVisitIterationStatement, // CkNodeIterationStatement,
    CkpVisitJumpStatement, // CkNodeJumpStatement,
    NULL, // CkNodeTryEnding,
    CkpVisitExceptStatement, // CkNodeExceptStatement,
    NULL, // CkNodeExceptStatementList,
    CkpVisitTryStatement, // CkNodeTryStatement,
    NULL, // CkNodeIdentifierList,
    CkpVisitFunctionDefinition, // CkNodeFunctionDefinition,
    CkpVisitFunctionDeclaration, // CkNodeFunctionDeclaration,
    CkpVisitChildren, // CkNodeClassMember,
    CkpVisitLeftRecursiveList, // CkNodeClassMemberList,
    CkpVisitChildren, // CkNodeClassBody,
    CkpVisitClassDefinition, // CkNodeClassDefinition,
    CkpVisitModuleName, // CkNodeModuleName,
    CkpVisitImportStatement, // CkNodeImportStatement,
    CkpVisitChildren, // CkNodeExternalDeclaration,
    CkpVisitLeftRecursiveList, // CkNodeTranslationUnit,
};

//
// ------------------------------------------------------------------ Functions
//

PCK_FUNCTION
CkpCompile (
    PCK_VM Vm,
    PCK_MODULE Module,
    PCSTR Source,
    UINTN Length,
    LONG Line,
    ULONG Flags
    )

/*++

Routine Description:

    This routine compiles Chalk source code into bytecode.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Module - Supplies a pointer to the module to compile into.

    Source - Supplies a pointer to the null-terminated source file.

    Length - Supplies the length of the source string, not including the null
        terminator.

    Line - Supplies the line number this code starts on. Supply 1 to start at
        the beginning.

    Flags - Supplies a bitfield of flags governing the behavior of the
        compiler. See CK_COMPILE_* definitions.

Return Value:

    Returns a pointer to a newly compiled function for the module on success.

    NULL on failure, and the virtual machine error will be set to contain more
    information.

--*/

{

    CK_COMPILER Compiler;
    CK_ERROR_TYPE Error;
    CK_PARSER Parser;
    PCK_SYMBOL_UNION TranslationUnit;
    YY_STATUS YyStatus;

    CkZero(&Parser, sizeof(Parser));
    Parser.Vm = Vm;
    Parser.Module = Module;
    Parser.Source = Source;
    Parser.SourceLength = Length;
    if ((Flags & CK_COMPILE_PRINT_ERRORS) != 0) {
        Parser.PrintErrors = TRUE;
    }

    CkpInitializeLexer(&(Parser.Lexer), Source, Length, Line);
    Parser.Parser.Grammar = &CkGrammar;
    Parser.Parser.Reallocate = CkpCompilerReallocate;
    Parser.Parser.Callback = CkpParserCallback;
    Parser.Parser.Error = CkpParserError;
    Parser.Parser.Context = &Compiler;
    Parser.Parser.Lexer = &(Parser.Lexer);
    Parser.Parser.GetToken = CkpLexerGetToken;
    Parser.Parser.ValueSize = sizeof(CK_SYMBOL_UNION);
    Parser.Line = Line;
    Error = CkpInitializeCompiler(&Compiler, &Parser, NULL, TRUE);
    if (Error != CkSuccess) {
        Parser.Errors += 1;
        goto CompileEnd;
    }

    Compiler.Flags = Flags;

    //
    // Parse the grammar into an abstract syntax tree.
    //

    YyStatus = YyParseGrammar(&(Parser.Parser));
    if (YyStatus != YyStatusSuccess) {
        Error = CkErrorCompile;
        Parser.Errors += 1;
        goto CompileEnd;
    }

    //
    // Compile the translation unit, which is always one beyond the node count
    // (since the callback only counts the children).
    //

    TranslationUnit = Parser.Nodes + Parser.NodeCount;

    CK_ASSERT(TranslationUnit->Symbol == CkNodeTranslationUnit);

    CkpVisitNode(&Compiler, &(TranslationUnit->Node));

    //
    // Emit a null return in case the source never had a return statement.
    //

    CkpEmitOp(&Compiler, CkOpNull);
    CkpEmitOp(&Compiler, CkOpReturn);

CompileEnd:
    return CkpFinalizeCompiler(&Compiler, "(module)", 8);
}

VOID
CkpCompileError (
    PCK_COMPILER Compiler,
    PVOID Token,
    PSTR Format,
    ...
    )

/*++

Routine Description:

    This routine reports a compile error.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Token - Supplies an optional pointer to the token where everything went
        wrong.

    Format - Supplies the printf-style format string of the error.

    ... - Supplies the remaining arguments.

Return Value:

    None.

--*/

{

    va_list ArgumentList;
    CHAR Buffer[CK_MAX_NAME + 15];
    PSTR Label;
    PLEXER_TOKEN LexerToken;
    INT Line;
    CHAR Name[CK_MAX_NAME + 1];
    ULONG NameSize;
    PCK_PARSER Parser;

    Parser = Compiler->Parser;
    if (Token == NULL) {
        Line = 0;
        Label = "Error";

    } else {
        LexerToken = Token;
        Line = LexerToken->Line;
        Label = Buffer;
        NameSize = LexerToken->Size;
        if (NameSize > CK_MAX_NAME) {
            NameSize = CK_MAX_NAME - 4;
            CkCopy(Name, Parser->Source + LexerToken->Position, NameSize);
            CkCopy(Name + NameSize, "...", 4);

        } else if (NameSize == 0) {
            Name[0] = '\0';
            Label = "Error";

        } else {
            CkCopy(Name, Parser->Source + LexerToken->Position, NameSize);
            Name[NameSize] = '\0';
        }

        snprintf(Buffer, sizeof(Buffer), "Error near '%s'", Name);
    }

    va_start(ArgumentList, Format);
    CkpReportCompileError(Parser, Line, Label, Format, ArgumentList);
    va_end(ArgumentList);
    return;
}

VOID
CkpVisitNode (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine compiles a node in the abstract syntax tree.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    PCK_COMPILER_NODE_VISITOR Visit;

    CK_ASSERT((Node->Symbol > CkNodeStart) && (Node->Symbol < CkSymbolCount));

    Compiler->Line = Node->Line;
    Visit = CkCompilerNodeFunctions[Node->Symbol - (CkNodeStart + 1)];

    CK_ASSERT(Visit != NULL);

    Visit(Compiler, Node);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

CK_ERROR_TYPE
CkpInitializeCompiler (
    PCK_COMPILER Compiler,
    PCK_PARSER Parser,
    PCK_COMPILER Parent,
    BOOL IsFunction
    )

/*++

Routine Description:

    This routine initializes a compiler structure.

Arguments:

    Compiler - Supplies a pointer to the compiler to initialize.

    Parser - Supplies a pointer to the parser this compiler will be reading
        from.

    Parent - Supplies an optional pointer to the parent compiler if this
        compiler is compiling a function.

    IsFunction - Supplies a boolean indicating whether this is compiling a
        function, which has no "this", or a method, which does have a "this".

Return Value:

    Status code.

--*/

{

    PCK_LOCAL Local;

    CkZero(Compiler, sizeof(CK_COMPILER));
    Compiler->Parser = Parser;
    Compiler->Parent = Parent;
    if (Parent != NULL) {
        Compiler->Depth = Parent->Depth + 1;
        if (Compiler->Depth > CK_MAX_NESTED_FUNCTIONS) {
            return CkErrorCompile;
        }
    }

    Parser->Vm->Compiler = Compiler;
    Compiler->Locals = CkAllocate(Parser->Vm,
                                  CK_INITIAL_LOCALS * sizeof(CK_LOCAL));

    if (Compiler->Locals == NULL) {
        return CkErrorNoMemory;
    }

    Compiler->LocalCapacity = CK_INITIAL_LOCALS;
    if (Parent != NULL) {
        Compiler->LocalCount = 1;

        //
        // Define the receiver slot, which will either be an inaccessible
        // variable for functions, or "this" for methods.
        //

        Local = &(Compiler->Locals[0]);
        if (IsFunction != FALSE) {
            Local->Name = NULL;
            Local->Length = 0;

        } else {
            Local->Name = "this";
            Local->Length = 4;
        }

        Local->Scope = -1;
        Local->IsUpvalue = FALSE;

    } else {
        Compiler->ScopeDepth = -1;
    }

    Compiler->StackSlots = Compiler->LocalCount;
    Compiler->Function = CkpFunctionCreate(Parser->Vm,
                                           Parser->Module,
                                           Compiler->LocalCount);

    if (Compiler->Function == NULL) {
        CkFree(Compiler->Parser->Vm, Compiler->Locals);
        Compiler->Locals = NULL;
        Compiler->LocalCapacity = 0;
        return CkErrorNoMemory;
    }

    return CkSuccess;
}

PCK_FUNCTION
CkpFinalizeCompiler (
    PCK_COMPILER Compiler,
    PCSTR DebugName,
    UINTN DebugNameLength
    )

/*++

Routine Description:

    This routine finalizes the given compiler, tearing it down and returning
    the function it compiled.

Arguments:

    Compiler - Supplies a pointer to the compiler to tear.

    DebugName - Supplies the name of the function to put in the debug
        information.

    DebugNameLength - Supplies the length of the debug name, not including the
        null terminator.

Return Value:

    Returns a pointer to the compiled function on success.

    NULL if there were any errors during compilation.

--*/

{

    CK_SYMBOL_INDEX Constant;
    CK_SYMBOL_INDEX Index;
    CK_VALUE Value;

    //
    // Emit an end opcode. This is never executed, and is mostly for peace of
    // mind, but does have the important side effect of updating the line
    // number information to encompass the last valid opcode.
    //

    Compiler->Line += 1;
    CkpEmitOp(Compiler, CkOpEnd);
    Value = CkpStringCreate(Compiler->Parser->Vm, DebugName, DebugNameLength);
    if (!CK_IS_STRING(Value)) {
        Compiler->Function = NULL;
        goto FinalizeCompilerEnd;
    }

    Compiler->Function->Debug.Name = CK_AS_STRING(Value);

    //
    // Don't return the function if there were any errors along the way
    // compiling it.
    //

    if (Compiler->Parser->Errors != 0) {
        Compiler->Function = NULL;
        goto FinalizeCompilerEnd;
    }

    //
    // If this is a child compiler, emit the definition for the function just
    // compiled.
    //

    if (Compiler->Parent != NULL) {
        CK_OBJECT_VALUE(Value, Compiler->Function);
        Constant = CkpAddConstant(Compiler->Parent, Value);

        //
        // Wrap the function in a closure. This is done even if the function
        // has no upvalues to simplify the implementation invoking a function
        // in the VM.
        //

        CkpEmitShortOp(Compiler->Parent, CkOpClosure, Constant);

        //
        // Emit arguments for each upvalue to know whether to capture a local
        // or an upvalue.
        //

        for (Index = 0; Index < Compiler->Function->UpvalueCount; Index += 1) {
            CkpEmitByte(Compiler->Parent, Compiler->Upvalues[Index].IsLocal);
            CkpEmitByte(Compiler->Parent, Compiler->Upvalues[Index].Index);
        }
    }

FinalizeCompilerEnd:
    Compiler->Parser->Vm->Compiler = Compiler->Parent;
    Compiler->LocalCount = 0;
    if (Compiler->Locals != NULL) {
        CkFree(Compiler->Parser->Vm, Compiler->Locals);
        Compiler->Locals = NULL;
        Compiler->LocalCapacity = 0;
    }

    if (Compiler->Declarations != NULL) {
        CkFree(Compiler->Parser->Vm, Compiler->Declarations);
        Compiler->Declarations = NULL;
        Compiler->DeclarationCapacity = 0;
    }

    if (Compiler->Upvalues != NULL) {
        CkFree(Compiler->Parser->Vm, Compiler->Upvalues);
        Compiler->Upvalues = NULL;
        Compiler->UpvalueCapacity = 0;
    }

    if (Compiler->Function != NULL) {
        if (CK_VM_FLAG_SET(Compiler->Parser->Vm,
                           CK_CONFIGURATION_DEBUG_COMPILER)) {

            CkpDumpCode(Compiler->Parser->Vm, Compiler->Function);
        }
    }

    return Compiler->Function;
}

//
// Language node visit functions
//

VOID
CkpVisitImportStatement (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine compiles an import statement.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    PCK_AST_NODE IdentifierList;
    ULONG LastChild;
    PCK_AST_NODE ModuleNameNode;
    PLEXER_TOKEN ModuleNameToken;
    CK_VARIABLE ModuleVariable;
    BOOL NamedModule;
    CK_VALUE NameString;
    CK_SYMBOL_INDEX NameVariable;
    PLEXER_TOKEN Token;

    //
    // Import statements can take a couple of forms.
    // import mydir.mymodule compiles to:
    //     var mymodule = Core.importModule("mydir.mymodule");
    //     mymodule.run();
    // from mydir.mymodule import thing1, thing2 compiles to:
    //     var _mod = Core.importModule("mydir.mymodule");
    //     _mod.run();
    //     var thing1 = _mod.__get("thing1");
    //     var thing2 = _mod.__get("thing2");
    // from mydir.mymodule import * compiles to:
    //     var _mod = Core.importModule("mydir.mymodule");
    //     _mod.run();
    //     Core.importAllSymbols(_tmp);
    //
    // Start by loading the Core module and pushing it onto the stack. _mod
    // represents an invisible local.
    //

    CkpLoadCoreVariable(Compiler, "Core");

    //
    // Create and push the module name string. Also get the first identifier,
    // which is the last component of the module name.
    //

    ModuleNameNode = CK_GET_AST_NODE(Compiler, Node->ChildIndex + 1);

    CK_ASSERT(ModuleNameNode->Symbol == CkNodeModuleName);

    if (ModuleNameNode->Children == 1) {
        ModuleNameToken = CK_GET_AST_TOKEN(Compiler,
                                           ModuleNameNode->ChildIndex);

    } else {

        CK_ASSERT(ModuleNameNode->Children == 3);

        ModuleNameToken = CK_GET_AST_TOKEN(Compiler,
                                           ModuleNameNode->ChildIndex + 2);
    }

    CK_ASSERT(ModuleNameToken->Value == CkTokenIdentifier);

    CkpVisitNode(Compiler, ModuleNameNode);

    //
    // Call the import function, which returns a pointer to the module.
    //

    CkpEmitMethodCall(Compiler, 1, "importModule@1", 14);

    //
    // Create a variable to store the resulting module. For an import statement,
    // this is a visible variable. For a from statement, this is an invisible
    // local.
    //

    Token = CK_GET_AST_TOKEN(Compiler, Node->ChildIndex);
    if (Token->Value == CkTokenImport) {
        NamedModule = TRUE;
        if (Compiler->ScopeDepth == -1) {
            ModuleVariable.Scope = CkScopeModule;

        } else {
            ModuleVariable.Scope = CkScopeLocal;
        }

        ModuleVariable.Index = CkpDeclareVariable(Compiler, ModuleNameToken);
        CkpDefineVariable(Compiler, ModuleVariable.Index);

    //
    // Create an invisible local (made invisible by the space). In non-global
    // scopes this continues to take up a stack slot since there's no way to
    // keep that around and also create new locals for each of the specific
    // imports.
    //

    } else {
        NamedModule = FALSE;
        ModuleVariable.Scope = CkScopeLocal;
        ModuleVariable.Index = CkpAddLocal(Compiler, "_mod ", 5);
    }

    //
    // Run the module contents to get everything actually loaded.
    //

    CkpLoadVariable(Compiler, ModuleVariable);
    CkpEmitMethodCall(Compiler, 0, "run@0", 5);
    CkpEmitOp(Compiler, CkOpPop);

    //
    // If it was just import mymodule ; then finish.
    //

    if (Node->Children == 3) {
        goto VisitImportStatementEnd;
    }

    CK_ASSERT(Node->Children >= 5);

    Token = CK_GET_AST_TOKEN(Compiler, Node->ChildIndex + 3);

    //
    // Handle importing everything. Call Core.importAllSymbols(mymodule), and
    // pop the null return value.
    //

    if (Token->Value == CkTokenAsterisk) {
        CkpLoadCoreVariable(Compiler, "Core");
        CkpLoadVariable(Compiler, ModuleVariable);
        CkpEmitMethodCall(Compiler, 1, "importAllSymbols@1", 18);
        CkpEmitOp(Compiler, CkOpPop);
        goto VisitImportStatementEnd;
    }

    //
    // Import each named element. Ignore the semicolon.
    //

    IdentifierList = CK_GET_AST_NODE(Compiler, Node->ChildIndex + 3);
    while (IdentifierList->Children > 0) {

        CK_ASSERT(IdentifierList->Symbol == CkNodeIdentifierList);

        LastChild = IdentifierList->ChildIndex + IdentifierList->Children - 1;
        Token = CK_GET_AST_TOKEN(Compiler, LastChild);

        CK_ASSERT(Token->Value == CkTokenIdentifier);

        NameVariable = CkpDeclareVariable(Compiler, Token);
        CkpLoadVariable(Compiler, ModuleVariable);
        NameString = CkpStringCreate(Compiler->Parser->Vm,
                                     Compiler->Parser->Source + Token->Position,
                                     Token->Size);

        CkpEmitConstant(Compiler, NameString);
        CkpEmitMethodCall(Compiler, 1, "__get@1", 7);
        CkpDefineVariable(Compiler, NameVariable);
        if (IdentifierList->Children > 1) {
            IdentifierList = CK_GET_AST_NODE(Compiler,
                                             IdentifierList->ChildIndex);

        } else {
            break;
        }
    }

VisitImportStatementEnd:

    //
    // If this is in global scope, the invisible local can be popped because
    // all the named elements were added to the global scope (rather than on
    // top of the local scope).
    //

    if ((NamedModule == FALSE) && (Compiler->ScopeDepth == -1)) {

        CK_ASSERT(Compiler->LocalCount == 1);

        Compiler->LocalCount -= 1;
        CkpEmitOp(Compiler, CkOpPop);
    }

    return;
}

VOID
CkpVisitClassDefinition (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine compiles an class definition.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    ULONG BodyIndex;
    CK_CLASS_COMPILER ClassCompiler;
    PCK_STRING ClassNameString;
    CK_VALUE ClassNameValue;
    UINTN FieldCountInstruction;
    PLEXER_TOKEN NameToken;

    CkZero(&ClassCompiler, sizeof(CK_CLASS_COMPILER));
    if (Compiler->ScopeDepth == -1) {
        ClassCompiler.ClassVariable.Scope = CkScopeModule;

    } else {
        ClassCompiler.ClassVariable.Scope = CkScopeLocal;
    }

    CK_ASSERT((Node->Children == 3) || (Node->Children == 5));

    NameToken = CK_GET_AST_TOKEN(Compiler, Node->ChildIndex + 1);
    ClassCompiler.ClassVariable.Index = CkpDeclareVariable(Compiler, NameToken);
    ClassNameValue = CkpStringCreate(
                                Compiler->Parser->Vm,
                                Compiler->Parser->Source + NameToken->Position,
                                NameToken->Size);

    if (CK_IS_NULL(ClassNameValue)) {
        return;
    }

    ClassNameString = CK_AS_STRING(ClassNameValue);
    CkpEmitConstant(Compiler, ClassNameValue);

    //
    // Get the superclass name if supplied.
    //

    if (Node->Children == 5) {
        CkpVisitNode(Compiler, CK_GET_AST_NODE(Compiler, Node->ChildIndex + 3));

    //
    // Inherit from the Object class.
    //

    } else {
        CkpLoadCoreVariable(Compiler, "Object");
    }

    //
    // The number of fields is not yet known. Set a placeholder value and
    // remember the offset to patch up at the end.
    //

    CkpEmitByteOp(Compiler, CkOpClass, 0xFF);
    FieldCountInstruction = Compiler->Function->Code.Count - 1;

    //
    // The class opcode causes the new class opcode to end up on the stack.
    // Save that in the variable slot.
    //

    CkpDefineVariable(Compiler, ClassCompiler.ClassVariable.Index);

    //
    // Create a new local variable scope. Static variables in the class will
    // be put in this scope, with methods that use them getting upvalues to
    // reference them.
    //

    CkpPushScope(Compiler);
    ClassCompiler.Name = ClassNameString;
    CkpStringTableInitialize(Compiler->Parser->Vm, &(ClassCompiler.Fields));
    CkpInitializeArray(&(ClassCompiler.Methods));
    CkpInitializeArray(&(ClassCompiler.StaticMethods));
    Compiler->EnclosingClass = &ClassCompiler;

    //
    // Compile the body, which is always the last node in the definition.
    //

    BodyIndex = Node->ChildIndex + Node->Children - 1;
    CkpVisitNode(Compiler, CK_GET_AST_NODE(Compiler, BodyIndex));
    Compiler->EnclosingClass = NULL;

    //
    // Now that the number of fields is known, patch it up.
    //

    CK_ASSERT(ClassCompiler.Fields.List.Count <= CK_MAX_FIELDS);

    Compiler->Function->Code.Data[FieldCountInstruction] =
                                               ClassCompiler.Fields.List.Count;

    CkpStringTableClear(Compiler->Parser->Vm, &(ClassCompiler.Fields));
    CkpClearArray(Compiler->Parser->Vm, &(ClassCompiler.Methods));
    CkpClearArray(Compiler->Parser->Vm, &(ClassCompiler.StaticMethods));
    CkpPopScope(Compiler);
    return;
}

VOID
CkpVisitModuleName (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine compiles a module name node.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    PSTR Current;
    PCK_AST_NODE CurrentNode;
    PLEXER_TOKEN Identifier;
    UINTN Length;
    PCK_STRING String;
    CK_VALUE Value;

    //
    // This is a left recursive node. Go down to the bottommost child to get
    // the first element, and count along the way.
    //

    Length = 0;
    CurrentNode = Node;
    while (CurrentNode->Children > 1) {

        CK_ASSERT(CurrentNode->Children == 3);

        Identifier = CK_GET_AST_TOKEN(Compiler, CurrentNode->ChildIndex + 2);

        CK_ASSERT(Identifier->Value == CkTokenIdentifier);

        //
        // Add the size of this internal string plus the dot.
        //

        Length += Identifier->Size + 1;
        CurrentNode = CK_GET_AST_NODE(Compiler, CurrentNode->ChildIndex);

        CK_ASSERT(CurrentNode->Symbol == CkNodeModuleName);
    }

    //
    // The innermost node is just an identifier.
    //

    CK_ASSERT(CurrentNode->Children == 1);

    Identifier = CK_GET_AST_TOKEN(Compiler, CurrentNode->ChildIndex);

    CK_ASSERT(Identifier->Value == CkTokenIdentifier);

    Length += Identifier->Size;
    String = CkpStringAllocate(Compiler->Parser->Vm, Length);
    if (String == NULL) {
        return;
    }

    Current = (PSTR)(String->Value);
    CkCopy(Current,
           Compiler->Parser->Source + Identifier->Position,
           Identifier->Size);

    Current += Identifier->Size;

    //
    // Walk back up the list adding the identifiers.
    //

    while (CurrentNode != Node) {
        CurrentNode = CK_GET_AST_NODE(Compiler, CurrentNode->Parent);

        CK_ASSERT((CurrentNode->Symbol == Node->Symbol) &&
                  (CurrentNode->Children == 3));

        Identifier = CK_GET_AST_TOKEN(Compiler, CurrentNode->ChildIndex + 2);
        *Current = '.';
        Current += 1;
        CkCopy(Current,
               Compiler->Parser->Source + Identifier->Position,
               Identifier->Size);

        Current += Identifier->Size;
    }

    CkpStringHash(String);

    //
    // Add the constant.
    //

    CK_OBJECT_VALUE(Value, String);
    CkpEmitConstant(Compiler, Value);
    return;
}

VOID
CkpVisitFunctionDefinition (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine compiles a function definition.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    PCK_AST_NODE Argument;
    PLEXER_TOKEN ArgumentName;
    PCK_AST_NODE ArgumentsNode;
    PCK_AST_NODE Body;
    CK_ERROR_TYPE Error;
    BOOL IsFunction;
    BOOL IsStatic;
    UINTN Length;
    CK_COMPILER MethodCompiler;
    CK_SYMBOL_INDEX MethodSymbol;
    PLEXER_TOKEN NameToken;
    CK_FUNCTION_SIGNATURE Signature;
    CHAR SignatureString[CK_MAX_METHOD_SIGNATURE];

    //
    // The grammar is [static] function myname ( identifier_list ) body.
    //

    if (Node->Children == 7) {
        IsStatic = TRUE;
        NameToken = CK_GET_AST_TOKEN(Compiler, Node->ChildIndex + 2);
        ArgumentsNode = CK_GET_AST_NODE(Compiler, Node->ChildIndex + 4);

    } else {

        CK_ASSERT(Node->Children == 6);

        IsStatic = FALSE;
        NameToken = CK_GET_AST_TOKEN(Compiler, Node->ChildIndex + 1);
        ArgumentsNode = CK_GET_AST_NODE(Compiler, Node->ChildIndex + 3);
    }

    CK_ASSERT((NameToken->Value == CkTokenIdentifier) &&
              (ArgumentsNode->Symbol == CkNodeIdentifierList));

    Signature.Name = Compiler->Parser->Source + NameToken->Position;
    Signature.Length = NameToken->Size;
    Signature.Arity = 0;
    if (Signature.Length > CK_MAX_NAME) {
        CkpCompileError(Compiler, NameToken, "Name too long");
        return;
    }

    IsFunction = TRUE;
    if (Compiler->EnclosingClass != FALSE) {
        IsFunction = FALSE;
        Compiler->EnclosingClass->InStatic = IsStatic;
    }

    if ((IsStatic != FALSE) && (IsFunction != FALSE)) {
        CkpCompileError(Compiler,
                        NameToken,
                        "Only class methods can be static");

        return;
    }

    //
    // Validate the argument count, and get to the leftmost node.
    //

    Argument = ArgumentsNode;
    while (Argument->Children > 1) {
        Signature.Arity += 1;
        Argument = CK_GET_AST_NODE(Compiler, Argument->ChildIndex);
    }

    if (Argument->Children > 0) {
        Signature.Arity += 1;
    }

    if (Signature.Arity >= CK_MAX_ARGUMENTS) {
        CkpCompileError(Compiler, NameToken, "Too many arguments");
        return;
    }

    //
    // Create an inner compiler for the function.
    //

    Error = CkpInitializeCompiler(&MethodCompiler,
                                  Compiler->Parser,
                                  Compiler,
                                  IsFunction);

    if (Error != CkSuccess) {
        CkpCompileError(Compiler, NameToken, "Failed to initialize compiler");
        return;
    }

    MethodCompiler.Function->Arity = Signature.Arity;
    MethodCompiler.StackSlots += Signature.Arity;
    if (MethodCompiler.StackSlots > MethodCompiler.Function->MaxStack) {
        MethodCompiler.Function->MaxStack = MethodCompiler.StackSlots;
    }

    //
    // Parse the parameter list. It's left recursive, so go backwards.
    //

    if (Argument->Children > 0) {
        CkpDeclareVariable(&MethodCompiler,
                           CK_GET_AST_TOKEN(Compiler, Argument->ChildIndex));

        while (Argument != ArgumentsNode) {
            Argument = CK_GET_AST_NODE(Compiler, Argument->Parent);

            CK_ASSERT(Argument->Children == 3);

            ArgumentName = CK_GET_AST_TOKEN(Compiler, Argument->ChildIndex + 2);
            CkpDeclareVariable(&MethodCompiler, ArgumentName);
        }
    }

    MethodSymbol = -1;
    Length = sizeof(SignatureString);
    CkpPrintSignature(&Signature, SignatureString, &Length);
    MethodSymbol = CkpDeclareMethod(Compiler,
                                    &Signature,
                                    IsStatic,
                                    NameToken,
                                    SignatureString,
                                    Length);

    //
    // Go compile the body.
    //

    Body = CK_GET_AST_NODE(Compiler, Node->ChildIndex + Node->Children - 1);

    CK_ASSERT(Body->Symbol == CkNodeCompoundStatement);

    CkpVisitNode(&MethodCompiler, Body);

    //
    // Emit a return statement in case the function body failed to.
    //

    CkpEmitOp(&MethodCompiler, CkOpNull);
    CkpEmitOp(&MethodCompiler, CkOpReturn);
    CkpFinalizeCompiler(&MethodCompiler, Signature.Name, Signature.Length);
    CkpDefineMethod(Compiler, IsStatic, MethodSymbol);
    return;
}

VOID
CkpVisitFunctionDeclaration (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine compiles a function declaration.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    PCK_AST_NODE Argument;
    PCK_AST_NODE ArgumentsNode;
    BOOL IsFunction;
    BOOL IsStatic;
    UINTN Length;
    PLEXER_TOKEN NameToken;
    CK_FUNCTION_SIGNATURE Signature;
    CHAR SignatureString[CK_MAX_METHOD_SIGNATURE];

    //
    // The grammar is [static] function myname ( identifier_list ) body.
    //

    if (Node->Children == 7) {
        IsStatic = TRUE;
        NameToken = CK_GET_AST_TOKEN(Compiler, Node->ChildIndex + 2);
        ArgumentsNode = CK_GET_AST_NODE(Compiler, Node->ChildIndex + 4);

    } else {

        CK_ASSERT(Node->Children == 6);

        IsStatic = FALSE;
        NameToken = CK_GET_AST_TOKEN(Compiler, Node->ChildIndex + 1);
        ArgumentsNode = CK_GET_AST_NODE(Compiler, Node->ChildIndex + 3);
    }

    CK_ASSERT((NameToken->Value == CkTokenIdentifier) &&
              (ArgumentsNode->Symbol == CkNodeIdentifierList));

    Signature.Name = Compiler->Parser->Source + NameToken->Position;
    Signature.Length = NameToken->Size;
    Signature.Arity = 0;
    if (Signature.Length > CK_MAX_NAME) {
        CkpCompileError(Compiler, NameToken, "Name too long");
        return;
    }

    IsFunction = TRUE;
    if (Compiler->EnclosingClass != FALSE) {
        IsFunction = FALSE;
        Compiler->EnclosingClass->InStatic = IsStatic;
    }

    if ((IsStatic != FALSE) && (IsFunction != FALSE)) {
        CkpCompileError(Compiler,
                        NameToken,
                        "Only class methods can be static");

        return;
    }

    //
    // Validate the argument count.
    //

    Argument = ArgumentsNode;
    while (Argument->Children > 1) {
        Signature.Arity += 1;
        Argument = CK_GET_AST_NODE(Compiler, Argument->ChildIndex);
    }

    if (Argument->Children > 0) {
        Signature.Arity += 1;
    }

    if (Signature.Arity >= CK_MAX_ARGUMENTS) {
        CkpCompileError(Compiler, NameToken, "Too many arguments");
        return;
    }

    Length = sizeof(SignatureString);
    CkpPrintSignature(&Signature, SignatureString, &Length);
    CkpAddFunctionDeclaration(Compiler, &Signature, NameToken);
    return;
}

VOID
CkpVisitExceptStatement (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine compiles a single case of an "except <expr> as e".

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    PCK_AST_NODE Body;
    CK_SYMBOL_INDEX Exception;
    CK_SYMBOL_INDEX ExceptionLocal;
    UINTN IfJump;
    UINTN Offset;
    PLEXER_TOKEN Token;

    //
    // The node takes one of the forms:
    // EXCEPT expression compound_statement
    // EXCEPT expression AS ID compound_statement
    //

    CK_ASSERT(Node->Children >= 3);
    CK_ASSERT(Compiler->LocalCount != 0);

    //
    // The exception is pushed onto the stack, and a local was created for it
    // by the visit try function.
    //

    Exception = Compiler->LocalCount - 1;
    CkpPushScope(Compiler);

    //
    // Load the exception in preparation for the call to it.
    //

    CkpLoadLocal(Compiler, Exception);

    //
    // Evaluate the expression.
    //

    CkpVisitNode(Compiler, CK_GET_AST_NODE(Compiler, Node->ChildIndex + 1));

    //
    // Call the is method to determine if the exception is of the type (or list
    // of types) specified by the expression. Jump over the except body if it
    // is not.
    //

    CkpEmitMethodCall(Compiler, 1, "__is@1", 6);
    IfJump = CkpEmitJump(Compiler, CkOpJumpIf);

    //
    // Define the visible version of the exception local if the exception block
    // wants one.
    //

    if (Node->Children == 5) {
        Token = CK_GET_AST_TOKEN(Compiler, Node->ChildIndex + 3);

        CK_ASSERT(Token->Value == CkTokenIdentifier);

        CkpLoadLocal(Compiler, Exception);
        ExceptionLocal = CkpDeclareVariable(Compiler, Token);
        if (ExceptionLocal == -1) {
            return;
        }

        CkpDefineVariable(Compiler, ExceptionLocal);
    }

    Body = CK_GET_AST_NODE(Compiler, Node->ChildIndex + Node->Children - 1);

    CK_ASSERT(Body->Symbol == CkNodeCompoundStatement);

    CkpVisitNode(Compiler, Body);
    CkpPopScope(Compiler);

    //
    // The finally block is not in the same scope as the one that contains the
    // hidden exception local. The compiler will emit a pop to remove that
    // scope, but by jumping directly to the finally case (which is also
    // executed when no exception occured), execution flow skips that pop.
    // Add an explicit pop now to remove the hidden exception local. But tweak
    // the stack slot count to avoid counting it twice.
    //

    Compiler->StackSlots += 1;
    CkpEmitOp(Compiler, CkOpPop);

    //
    // Jump backwards to the finally block.
    //

    CK_ASSERT((Compiler->FinallyOffset != 0) &&
              (Compiler->FinallyOffset < Compiler->Function->Code.Count));

    Offset = Compiler->Function->Code.Count - Compiler->FinallyOffset + 2;
    CkpEmitShortOp(Compiler, CkOpLoop, Offset);

    //
    // If the exception didn't match, end up here to try the next except
    // statement or the default case provided by the try visit function.
    //

    CkpPatchJump(Compiler, IfJump);
    return;
}

VOID
CkpVisitTryStatement (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine compiles a try-except-else-finally block.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    CK_SYMBOL_INDEX ExceptionLocal;
    PCK_AST_NODE Finally;
    UINTN FinallyJumpOffset;
    UINTN FinallyOffset;
    UINTN PreviousFinallyOffset;
    PLEXER_TOKEN Token;
    PCK_AST_NODE TryEnd;
    UINTN TryOffset;

    //
    // The statement should take the form:
    // TRY compound_statement except_statement_list try_ending.
    // The ending should be in the form:
    // [ ELSE compound_statement ] [ FINALLY compound_statement ].
    //

    CK_ASSERT(Node->Children == 4);

    TryEnd = CK_GET_AST_NODE(Compiler, Node->ChildIndex + 3);

    //
    // Emit the try, and compile the compound statement. Track the number of
    // tries in the current loop in case a break statement occurs. A break or
    // continue will need to pop the try blocks before jumping directly out of
    // the try.
    //

    TryOffset = CkpEmitJump(Compiler, CkOpTry);
    if (Compiler->Loop != NULL) {
        Compiler->Loop->TryCount += 1;
    }

    CkpVisitNode(Compiler, CK_GET_AST_NODE(Compiler, Node->ChildIndex + 1));
    if (Compiler->Loop != NULL) {

        CK_ASSERT(Compiler->Loop->TryCount != 0);

        Compiler->Loop->TryCount -= 1;
    }

    //
    // At this point the try succeeded without taking an exception. Pop the
    // try so that exceptions in the else or finally cases are not handled by
    // this try.
    //

    CkpEmitOp(Compiler, CkOpPopTry);

    //
    // If there's no else or finally, then emit a stub "finally" block that
    // just jumps over all the except cases. The "finally" is emitted before
    // the except cases to avoid having to remember N jump patch locations for
    // when each except clause finishes and needs to jump to the finally code.
    //

    FinallyOffset = Compiler->Function->Code.Count;
    if (TryEnd->Children != 0) {
        Token = CK_GET_AST_TOKEN(Compiler, TryEnd->ChildIndex);

        //
        // If there's an else, emit it now. The success no-exception case just
        // falls through to here.
        //

        Finally = NULL;
        if (Token->Value == CkTokenElse) {
            CkpVisitNode(Compiler,
                         CK_GET_AST_NODE(Compiler, TryEnd->ChildIndex + 1));

            if (TryEnd->Children == 4) {
                Finally = CK_GET_AST_NODE(Compiler, TryEnd->ChildIndex + 3);
            }

        } else {

            CK_ASSERT(Token->Value == CkTokenFinally);

            Finally = CK_GET_AST_NODE(Compiler, TryEnd->ChildIndex + 1);
        }

        FinallyOffset = Compiler->Function->Code.Count;
        if (Finally != NULL) {
            CkpVisitNode(Compiler, Finally);
        }
    }

    //
    // Now after executing the finally block, jump over all the except blocks.
    //

    FinallyJumpOffset = CkpEmitJump(Compiler, CkOpJump);

    //
    // Now handle the exception cases. This first one is where the interpreter
    // should jump to if an exception occurs.
    //

    CkpPatchJump(Compiler, TryOffset);

    //
    // Push a scope and define a local for the exception that the interpreter
    // put on the stack. Give it a hidden (illegal) name so regular code can't
    // see it.
    //

    CkpPushScope(Compiler);
    Compiler->StackSlots += 1;
    ExceptionLocal = CkpAddLocal(Compiler, " e", 2);
    PreviousFinallyOffset = Compiler->FinallyOffset;
    Compiler->FinallyOffset = FinallyOffset - 1;
    CkpVisitLeftRecursiveList(Compiler,
                              CK_GET_AST_NODE(Compiler, Node->ChildIndex + 2));

    Compiler->FinallyOffset = PreviousFinallyOffset;

    //
    // Emit the case where no except case matches the current exception.
    // In that case re-raise the exception to the next try block. The exception
    // is already on the stack, so it will need to be re-pushed as an argument.
    //

    CkpLoadCoreVariable(Compiler, "Core");
    CkpLoadLocal(Compiler, ExceptionLocal);
    CkpEmitMethodCall(Compiler, 1, "raise@1", 7);

    //
    // Pop the return value and the scope containing the hidden exception local
    // pushed on by the raise. The pops emitted here never get executed
    // (since raise doesn't return), but it keeps the compiler's stack tracking
    // in sync.
    //

    CkpEmitOp(Compiler, CkOpPop);
    CkpPopScope(Compiler);

    //
    // And finally, everything is emitted. This is where the finally block
    // jumps to continue execution. The end of the finally skips the code that
    // popped this exception scope because it's also executed in the success
    // case (where no exception was pushed). Each exception clause has an
    // extra pop on the end to cover the pop missed by not executing the pop
    // scope above.
    //

    CkpPatchJump(Compiler, FinallyJumpOffset);
    return;
}

VOID
CkpVisitJumpStatement (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine compiles a jump statement: break, continue, or return.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    UINTN Offset;
    PSTR StatementName;
    PLEXER_TOKEN Token;
    LONG TryIndex;

    CK_ASSERT(Node->Children >= 1);

    Token = CK_GET_AST_TOKEN(Compiler, Node->ChildIndex);
    switch (Token->Value) {
    case CkTokenBreak:
    case CkTokenContinue:
        if (Compiler->Loop == NULL) {
            StatementName = "break";
            if (Token->Value == CkTokenContinue) {
                StatementName = "continue";
            }

            CkpCompileError(Compiler,
                            Token,
                            "Cannot use '%s' outside of a loop",
                            StatementName);

            break;
        }

        //
        // Pop out of the try blocks currently running inside this loop.
        //

        for (TryIndex = 0; TryIndex < Compiler->Loop->TryCount; TryIndex += 1) {
            CkpEmitOp(Compiler, CkOpPopTry);
        }

        //
        // Discard the locals since breaks will jump out of a scope.
        //

        CkpDiscardLocals(Compiler, Compiler->Loop->Scope + 1);
        if (Token->Value == CkTokenBreak) {

            //
            // Emit a jump, but it's not yet known where the end of the loop is.
            // Emit an end op because it cannot occur normally inside the loop,
            // and serves as noticable placeholders for patching later.
            //

            CkpEmitJump(Compiler, CkOpEnd);

        } else {

            //
            // Emit a loop back to the conditional, the start of the loop.
            //

            Offset = Compiler->Function->Code.Count - Compiler->Loop->Start + 2;
            CkpEmitShortOp(Compiler, CkOpLoop, Offset);
        }

        break;

    case CkTokenReturn:

        //
        // If return as an expression, go compile that expression.
        //

        if (Node->Children > 2) {

            CK_ASSERT(Node->Children == 3);

            CkpVisitNode(Compiler,
                         CK_GET_AST_NODE(Compiler, Node->ChildIndex + 1));

        //
        // Otherwise, push a null to return.
        //

        } else {
            CkpEmitOp(Compiler, CkOpNull);
        }

        CkpEmitOp(Compiler, CkOpReturn);
        break;

    default:

        CK_ASSERT(FALSE);

        break;
    }

    return;
}

VOID
CkpVisitIterationStatement (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine compiles an iteration statement: a while, do-while, or for
    loop.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    PCK_AST_NODE Body;
    PCK_AST_NODE Expression;
    CK_SYMBOL_INDEX ExpressionSymbol;
    CK_SYMBOL_INDEX IteratorSymbol;
    PLEXER_TOKEN IteratorToken;
    UINTN JumpTarget;
    CK_LOOP Loop;
    PLEXER_TOKEN Token;

    CK_ASSERT(Node->Children >= 5);

    Token = CK_GET_AST_TOKEN(Compiler, Node->ChildIndex);
    switch (Token->Value) {

    //
    // While loops look like: while ( expression ) compound_statement.
    //

    case CkTokenWhile:

        CK_ASSERT(Node->Children == 5);

        CkpStartLoop(Compiler, &Loop);
        Expression = CK_GET_AST_NODE(Compiler, Node->ChildIndex + 2);

        CK_ASSERT(Expression->Symbol == CkNodeExpression);

        CkpVisitNode(Compiler, Expression);
        CkpTestLoopExit(Compiler);
        Body = CK_GET_AST_NODE(Compiler, Node->ChildIndex + 4);
        CkpCompileLoopBody(Compiler, Body);
        CkpEndLoop(Compiler);
        break;

    //
    // Do-while loops look like: do compound_statement while ( expression ) ;
    //

    case CkTokenDo:

        CK_ASSERT(Node->Children == 7);

        //
        // Jump over the condition the first time.
        //

        JumpTarget = CkpEmitJump(Compiler, CkOpJump);
        CkpStartLoop(Compiler, &Loop);
        Expression = CK_GET_AST_NODE(Compiler, Node->ChildIndex + 4);

        CK_ASSERT(Expression->Symbol == CkNodeExpression);

        CkpVisitNode(Compiler, Expression);
        CkpTestLoopExit(Compiler);
        CkpPatchJump(Compiler, JumpTarget);
        Body = CK_GET_AST_NODE(Compiler, Node->ChildIndex + 1);
        CkpCompileLoopBody(Compiler, Body);
        CkpEndLoop(Compiler);
        break;

    //
    // For takes three different forms.
    //

    case CkTokenFor:
        Token = CK_GET_AST_TOKEN(Compiler, Node->ChildIndex + 3);

        //
        // The more modern for loop looks like this (except that seq and iter
        // are hidden variables):
        // for ( identifier in expression ) compound_statement.
        // It is compiled to something like:
        // {
        //     var seq = expression;
        //     var iter;
        //     while ((iter = seq.iterate(iter)) != null) {
        //         var identifier = seq.iteratorValue(iter);
        //         compound_statement;
        //     }
        // }
        //

        if (Token->Value == CkTokenIn) {

            //
            // Create a scope for the hidden local variables in the loop.
            //

            CkpPushScope(Compiler);
            IteratorToken = CK_GET_AST_TOKEN(Compiler, Node->ChildIndex + 2);

            CK_ASSERT(IteratorToken->Value == CkTokenIdentifier);

            Expression = CK_GET_AST_NODE(Compiler, Node->ChildIndex + 4);
            CkpVisitNode(Compiler, Expression);

            //
            // The spaces in the local variables make them illegal names, so
            // they're invisible to the namespace.
            //

            ExpressionSymbol = CkpAddLocal(Compiler, "seq ", 4);
            CkpEmitOp(Compiler, CkOpNull);
            IteratorSymbol = CkpAddLocal(Compiler, "iter ", 5);
            CkpStartLoop(Compiler, &Loop);

            //
            // Emit null != (iter = seq.iterate(iter)), and check for exiting
            // the loop if the iterator becomes null.
            //

            CkpEmitOp(Compiler, CkOpNull);
            CkpLoadLocal(Compiler, ExpressionSymbol);
            CkpLoadLocal(Compiler, IteratorSymbol);
            CkpEmitMethodCall(Compiler, 1, "iterate@1", 9);
            CkpEmitByteOp(Compiler, CkOpStoreLocal, IteratorSymbol);
            CkpEmitOperatorCall(Compiler, CkTokenIsNotEqual, 1, FALSE);
            CkpTestLoopExit(Compiler);

            //
            // Emit seq.iteratorValue(iter), push a new scope, and assign the
            // iterator value to the named variable.
            //

            CkpLoadLocal(Compiler, ExpressionSymbol);
            CkpLoadLocal(Compiler, IteratorSymbol);
            CkpEmitMethodCall(Compiler, 1, "iteratorValue@1", 15);
            CkpPushScope(Compiler);
            CkpAddLocal(Compiler,
                        Compiler->Parser->Source + IteratorToken->Position,
                        IteratorToken->Size);

            //
            // Compile the body, then pop the scope and loop.
            //

            Body = CK_GET_AST_NODE(Compiler, Node->ChildIndex + 6);
            CkpCompileLoopBody(Compiler, Body);
            CkpPopScope(Compiler);
            CkpEndLoop(Compiler);

            //
            // Pop the extra scope for the hidden variables.
            //

            CkpPopScope(Compiler);

        //
        // The old traditional for loop looks like this:
        // for ( statement expression ; [expression] )
        //     compound_statement.
        //

        } else {

            //
            // Create a scope for the expression statements, and visit the
            // initial statement that only runs once.
            //

            CkpPushScope(Compiler);
            CkpVisitNode(Compiler,
                         CK_GET_AST_NODE(Compiler, Node->ChildIndex + 2));

            CkpStartLoop(Compiler, &Loop);

            //
            // Visit the termination expression, and test it for loop execution.
            //

            CkpVisitNode(Compiler,
                         CK_GET_AST_NODE(Compiler, Node->ChildIndex + 3));

            CkpTestLoopExit(Compiler);

            //
            // Compile the body.
            //

            Body = CK_GET_AST_NODE(Compiler,
                                   Node->ChildIndex + Node->Children - 1);

            CkpVisitNode(Compiler, Body);

            //
            // If there's a final expression, execute that. It's an expression,
            // so also pop it off the stack.
            //

            if (Node->Children == 8) {
                CkpVisitNode(Compiler,
                             CK_GET_AST_NODE(Compiler, Node->ChildIndex + 5));

                CkpEmitOp(Compiler, CkOpPop);
            }

            CkpEndLoop(Compiler);
            CkpPopScope(Compiler);
        }

        break;

    default:

        CK_ASSERT(FALSE);

        break;
    }

    return;
}

VOID
CkpVisitSelectionStatement (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine compiles a selection statement: an if or if-else.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    UINTN ElseJump;
    PCK_AST_NODE Expression;
    UINTN IfJump;
    PCK_AST_NODE Statement;

    CK_ASSERT(Node->Children >= 5);

    Expression = CK_GET_AST_NODE(Compiler, Node->ChildIndex + 2);

    CK_ASSERT(Expression->Symbol == CkNodeExpression);

    CkpVisitNode(Compiler, Expression);
    IfJump = CkpEmitJump(Compiler, CkOpJumpIf);
    Statement = CK_GET_AST_NODE(Compiler, Node->ChildIndex + 4);
    CkpVisitNode(Compiler, Statement);

    //
    // If there's an else, visit that too.
    //

    if (Node->Children == 7) {

        //
        // Jump over the else when the if is taken.
        //

        ElseJump = CkpEmitJump(Compiler, CkOpJump);
        CkpPatchJump(Compiler, IfJump);
        Statement = CK_GET_AST_NODE(Compiler, Node->ChildIndex + 6);
        CkpVisitNode(Compiler, Statement);
        CkpPatchJump(Compiler, ElseJump);

    //
    // No else, just patch up the if jump over target.
    //

    } else {
        CkpPatchJump(Compiler, IfJump);
    }

    return;
}

VOID
CkpVisitExpressionStatement (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine compiles an expression statement: something like 4; but
    hopefully with more side effects.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    CK_ASSERT(Node->Children >= 1);

    //
    // Expression statements can either just be a simple semicolon, or
    // expression;.
    //

    if (Node->Children < 2) {
        return;
    }

    //
    // If printing expressions, then load Core up in preparation for Core.print.
    //

    if ((Compiler->Flags & CK_COMPILE_PRINT_EXPRESSIONS) != 0) {
        CkpLoadCoreVariable(Compiler, "Core");
    }

    CkpVisitNode(Compiler, CK_GET_AST_NODE(Compiler, Node->ChildIndex));

    //
    // The expression result is now on the stack. Either call print and pop off
    // the return value of print, or just pop off the expression itself.
    //

    if ((Compiler->Flags & CK_COMPILE_PRINT_EXPRESSIONS) != 0) {
        CkpEmitMethodCall(Compiler, 1, "repr@1", 6);
    }

    CkpEmitOp(Compiler, CkOpPop);
    return;
}

VOID
CkpVisitCompoundStatement (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine compiles an compound statement.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    CK_ASSERT(Node->Children >= 2);

    //
    // A compound statement can either be { } or { statement_list }. Visit the
    // statement list if there is one, and put it in its own scope.
    //

    if (Node->Children == 3) {
        CkpPushScope(Compiler);
        CkpVisitNode(Compiler, CK_GET_AST_NODE(Compiler, Node->ChildIndex + 1));
        CkpPopScope(Compiler);
    }

    return;
}

VOID
CkpVisitVariableDefinition (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine compiles a variable definition, which declares a variable
    with or without an initializer.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    //
    // A variable definition can look like variable_declaration or
    // variable_specifier = expression ;. If it's the initialized form, get
    // the expression first, then declare the variable.
    //

    if (Node->Children == 4) {

        //
        // The grammar precludes initialized variables in a class.
        //

        CK_ASSERT(Compiler->EnclosingClass == NULL);

        CkpVisitNode(Compiler, CK_GET_AST_NODE(Compiler, Node->ChildIndex + 2));
    }

    CkpVisitNode(Compiler, CK_GET_AST_NODE(Compiler, Node->ChildIndex));
    return;
}

VOID
CkpVisitVariableDeclaration (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine compiles a variable declaration, which does not have an
    initializer.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    CK_ASSERT(Node->Children == 2);

    //
    // Emit a null to initialize the new variable, unless this is a field
    // being defined.
    //

    if (Compiler->EnclosingClass == NULL) {
        CkpEmitOp(Compiler, CkOpNull);
    }

    CkpVisitNode(Compiler, CK_GET_AST_NODE(Compiler, Node->ChildIndex));
    return;
}

VOID
CkpVisitVariableSpecifier (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine compiles a variable specifier, which is the part that actually
    declares the variable. It assumes that the initializer value has already
    been pushed on the stack.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    PLEXER_TOKEN Name;
    ULONG NameIndex;
    PCSTR NameString;
    PLEXER_TOKEN Static;
    CK_SYMBOL_INDEX Symbol;

    NameIndex = Node->ChildIndex + Node->Children - 1;
    Name = CK_GET_AST_TOKEN(Compiler, NameIndex);

    CK_ASSERT(((Node->Children == 2) || (Node->Children == 3)) &&
              (Name->Value == CkTokenIdentifier));

    Static = CK_GET_AST_TOKEN(Compiler, Node->ChildIndex);

    //
    // If there's a class compiler, then this variable is being defined
    // directly inside that class, making it a field.
    //

    if (Compiler->EnclosingClass != NULL) {

        //
        // Static fields are basically global variables in a limbo local
        // variable scope (a scope was pushed when the class compilation
        // started). So define them like a local in the scope of the class
        // itself.
        //

        if (Static->Value == CkTokenStatic) {
            Symbol = CkpDeclareVariable(Compiler, Name);
            CkpEmitOp(Compiler, CkOpNull);
            CkpDefineVariable(Compiler, Symbol);

        //
        // This is a field on the class. Make sure it does not exist, then
        // create it.
        //

        } else {
            NameString = Compiler->Parser->Source + Name->Position;
            Symbol = CkpStringTableFind(&(Compiler->EnclosingClass->Fields),
                                        NameString,
                                        Name->Size);

            if (Symbol >= 0) {
                CkpCompileError(Compiler, Name, "Field already declared");

            } else {
                Symbol = CkpStringTableAdd(Compiler->Parser->Vm,
                                           &(Compiler->EnclosingClass->Fields),
                                           NameString,
                                           Name->Size);
            }
        }

    //
    // It's a live variable, not a field declaration. The initializer
    // expression's already on the stack, so just declare and define it.
    //

    } else {
        if (Static->Value == CkTokenStatic) {
            CkpCompileError(Compiler, Name, "Only fields can be marked static");
        }

        Symbol = CkpDeclareVariable(Compiler, Name);
        CkpDefineVariable(Compiler, Symbol);
    }

    return;
}

VOID
CkpVisitChildren (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine implements a generic function for visiting a node by simply
    visiting any of its non-token children.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    PCK_SYMBOL_UNION Child;
    UINTN Index;

    Child = Compiler->Parser->Nodes + Node->ChildIndex;
    for (Index = 0; Index < Node->Children; Index += 1) {
        if (Child->Symbol >= CkNodeStart) {
            CkpVisitNode(Compiler, &(Child->Node));
        }

        Child += 1;
    }

    return;
}

VOID
CkpVisitLeftRecursiveList (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine implements a generic function for visiting a left recursive
    element backwards.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    PCK_AST_NODE CurrentNode;
    UINTN LastIndex;
    PCK_AST_NODE NextNode;
    PCK_AST_NODE VisitNode;

    //
    // The expected form is either a one element list for the non-recursive
    // form, or a two element list if the first element is recursive.
    //

    CurrentNode = Node;
    while (CurrentNode->Children > 1) {
        NextNode = CK_GET_AST_NODE(Compiler, CurrentNode->ChildIndex);

        CK_ASSERT((CurrentNode->Symbol == NextNode->Symbol) &&
                  (NextNode < CurrentNode));

        CurrentNode = NextNode;
    }

    //
    // Visit the bottom most element.
    //

    if (CurrentNode->Children != 0) {
        VisitNode = CK_GET_AST_NODE(Compiler, CurrentNode->ChildIndex);
        CkpVisitNode(Compiler, VisitNode);
    }

    //
    // Now loop going back up the tree visiting the other elements.
    //

    while (CurrentNode != Node) {
        CurrentNode = CK_GET_AST_NODE(Compiler, CurrentNode->Parent);

        CK_ASSERT(CurrentNode->Symbol == Node->Symbol);

        LastIndex = CurrentNode->ChildIndex + CurrentNode->Children - 1;
        VisitNode = CK_GET_AST_NODE(Compiler, LastIndex);
        CkpVisitNode(Compiler, VisitNode);
    }

    return;
}

VOID
CkpReportCompileError (
    PCK_PARSER Parser,
    INT Line,
    PSTR Label,
    PSTR MessageFormat,
    va_list ArgumentList
    )

/*++

Routine Description:

    This routine reports a compile error to the primary error function.

Arguments:

    Parser - Supplies a pointer to the parser.

    Line - Supplies the line number where the error occurred.

    Label - Supplies a pointer to a string containing a message label.

    MessageFormat - Supplies the message format string.

    ArgumentList - Supplies the varargs argument list for the message.

Return Value:

    None.

--*/

{

    INT Length;
    CHAR Message[CK_MAX_ERROR_MESSAGE];
    PCSTR Name;

    Parser->Errors += 1;

    //
    // If not reporting errors, or an error has already been raised, forget it.
    // Multiple exceptions cannot be raised because they would peel back
    // exception handlers before they got a chance to run.
    //

    if ((Parser->PrintErrors == FALSE) || (Parser->Errors != 1)) {
        return;
    }

    Name = Parser->Module->Name->Value;
    if (Label != NULL) {
        Length = snprintf(Message,
                          sizeof(Message),
                          "%s:%d %s: ",
                          Name,
                          Line,
                          Label);

    } else {
        Length = snprintf(Message, sizeof(Message), "%s:%d ", Name, Line);
    }

    if (Length < 0) {
        Length = 0;
    }

    vsnprintf(Message + Length,
              sizeof(Message) - Length,
              MessageFormat,
              ArgumentList);

    Message[sizeof(Message) - 1] = '\0';
    CkpRuntimeError(Parser->Vm, "CompileError", "%s", Message);
    return;
}

//
// Parser support functions
//

PVOID
CkpCompilerReallocate (
    PVOID Context,
    PVOID Allocation,
    UINTN Size
    )

/*++

Routine Description:

    This routine is called to allocate, reallocate, or free memory on behalf of
    the Chalk compiler.

Arguments:

    Context - Supplies a pointer to the context passed into the parser.

    Allocation - Supplies an optional pointer to an existing allocation to
        either reallocate or free. If NULL, then a new allocation is being
        requested.

    Size - Supplies the size of the allocation request, in bytes. If this is
        non-zero, then an allocation or reallocation is being requested. If
        this is is 0, then the given memory should be freed.

Return Value:

    Returns a pointer to the allocated memory on success.

    NULL on allocation failure or free.

--*/

{

    PCK_COMPILER Compiler;

    Compiler = Context;
    return CkpReallocate(Compiler->Parser->Vm, Allocation, 0, Size);
}

INT
CkpParserCallback (
    PVOID Context,
    YY_VALUE Symbol,
    PVOID Elements,
    INT ElementCount,
    PVOID ReducedElement
    )

/*++

Routine Description:

    This routine is called for each grammar element that is successfully parsed.

Arguments:

    Context - Supplies the context pointer supplied to the parse function.

    Symbol - Supplies the non-terminal symbol that was reduced.

    Elements - Supplies a pointer to an array of values containing the child
        elements in the rule. The function should know how many child elements
        there are based on the rule.

    ElementCount - Supplies the number of elements in the right hand side of
        this rule.

    ReducedElement - Supplies a pointer where the function can specify the
        reduction value to push back on the stack. If untouched, the default
        action is to push the first element. When called, the buffer will be
        set up for that, or zeroed if the rule is empty.

Return Value:

    0 on success.

    Returns a non-zero value if the parser should abort and return.

--*/

{

    PCK_SYMBOL_UNION Child;
    ULONG ChildIndex;
    PCK_COMPILER Compiler;
    PCK_SYMBOL_UNION Grandchild;
    ULONG GrandchildIndex;
    UINTN NeededCapacity;
    PVOID NewBuffer;
    UINTN NewCapacity;
    PCK_AST_NODE NewNode;
    PCK_PARSER Parser;

    Compiler = Context;
    Parser = Compiler->Parser;

    //
    // Make sure there's space for all the children, plus the new node.
    //

    NeededCapacity = Parser->NodeCount + ElementCount + 1;
    if (NeededCapacity >= Parser->NodeCapacity) {
        if (Parser->NodeCapacity == 0) {
            NewCapacity = CK_INITIAL_AST_NODES;

        } else {
            NewCapacity = Parser->NodeCapacity * 2;
        }

        while (NewCapacity < NeededCapacity) {
            NewCapacity *= 2;
        }

        NewBuffer = CkpReallocate(
                                Parser->Vm,
                                Parser->Nodes,
                                Parser->NodeCapacity * sizeof(CK_SYMBOL_UNION),
                                NewCapacity * sizeof(CK_SYMBOL_UNION));

        if (NewBuffer == NULL) {
            return -1;
        }

        Parser->Nodes = NewBuffer;
        Parser->NodeCapacity = NewCapacity;
    }

    //
    // Set up the new node by counting the total child descendents.
    //

    NewNode = ReducedElement;
    NewNode->ChildIndex = Parser->NodeCount;
    NewNode->Symbol = Symbol;
    NewNode->Children = ElementCount;
    NewNode->Descendants = 0;
    NewNode->Depth = 0;

    //
    // Copy the new child elements into the stream.
    //

    CkCopy(Parser->Nodes + Parser->NodeCount,
           Elements,
           ElementCount * sizeof(CK_SYMBOL_UNION));

    Child = Parser->Nodes + Parser->NodeCount;

    //
    // Get the line number for the start of this element.
    //

    if (ElementCount == 0) {
        NewNode->Line = Parser->PreviousLine;

    } else if (Child->Symbol >= CkNodeStart) {
        NewNode->Line = Child->Node.Line;

    } else {
        NewNode->Line = Child->Token.Line;
    }

    //
    // Sum the descendents.
    //

    for (ChildIndex = 0; ChildIndex < ElementCount; ChildIndex += 1) {
        if (Child->Symbol >= CkNodeStart) {
            NewNode->Descendants += Child->Node.Children +
                                    Child->Node.Descendants;

            if (Child->Node.Depth + 1 > NewNode->Depth) {
                NewNode->Depth = Child->Node.Depth + 1;
            }

            //
            // Initially set the child's parent to the end node, which will be
            // incorrect except for the very last translation unit. For all the
            // grandchildren nodes, now that the parent is settled into the
            // array, update their parent indices.
            //

            Child->Node.Parent = Parser->NodeCount + ElementCount;
            for (GrandchildIndex = 0;
                 GrandchildIndex < Child->Node.Children;
                 GrandchildIndex += 1) {

                Grandchild = Parser->Nodes + Child->Node.ChildIndex;
                if (Grandchild->Symbol >= CkNodeStart) {
                    Grandchild->Node.Parent = ChildIndex + Parser->NodeCount;
                }
            }
        }

        Child += 1;
    }

    Parser->NodeCount += ElementCount;

    //
    // Copy the current node as well in case it ends up being the last
    // translation unit. Don't update the symbols count, which means this node
    // gets overwritten if there are more elements. The check at the top of the
    // function always ensures there's space for this extra element.
    //

    if (Symbol == CkNodeTranslationUnit) {
        CkCopy(Parser->Nodes + Parser->NodeCount,
               NewNode,
               sizeof(CK_SYMBOL_UNION));
    }

    return YyStatusSuccess;
}

YY_STATUS
CkpParserError (
    PVOID Context,
    YY_STATUS Status
    )

/*++

Routine Description:

    This routine is called if the parser reaches an error state.

Arguments:

    Context - Supplies the context pointer supplied to the parse function.

    Status - Supplies the error that occurred, probably a parse error.

Return Value:

    0 if the parser should try to recover.

    Returns a non-zero value if the parser should abort immediately and return.

--*/

{

    PCK_COMPILER Compiler;
    PSTR ErrorType;
    LEXER_TOKEN Token;

    Compiler = Context;
    switch (Status) {
    case YyStatusNoMemory:
        ErrorType = "Out of memory";
        break;

    case YyStatusParseError:
        ErrorType = "Syntax error";
        break;

    case YyStatusLexError:
        ErrorType = "Lexical error";
        break;

    case YyStatusTooManyItems:
        ErrorType = "Overflow";
        break;

    default:
        ErrorType = "Unknown error";
        break;
    }

    CkZero(&Token, sizeof(Token));
    Token.Position = Compiler->Parser->PreviousPosition;
    Token.Size = Compiler->Parser->PreviousSize;
    Token.Line = Compiler->Parser->PreviousLine;
    CkpCompileError(Compiler, &Token, ErrorType);
    return YyStatusSuccess;
}

