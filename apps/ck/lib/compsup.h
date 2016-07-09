/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    compsup.h

Abstract:

    This header contains internal definitions for the Chalk compiler.

Author:

    Evan Green 9-Jun-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// This macro gets an AST node at the given index.
//

#define CK_GET_AST_NODE(_Compiler, _Index) \
    (PCK_AST_NODE)((_Compiler)->Parser->Nodes + (_Index))

#define CK_GET_AST_TOKEN(_Compiler, _Index) \
    (PLEXER_TOKEN)((_Compiler)->Parser->Nodes + (_Index))

//
// This macro decodes the line advance out of the given special line opcode.
//

#define CK_LINE_ADVANCE(_SpecialOp) \
    ((((_SpecialOp) - CkLineOpSpecial) % CK_LINE_RANGE) + CK_LINE_START)

//
// This macro decodes the offset advance out of the given special line opcode.
//

#define CK_OFFSET_ADVANCE(_SpecialOp) \
    (((_SpecialOp) - CkLineOpSpecial) / CK_LINE_RANGE)

//
// This macro evaluates to non-zero if the given line and offset advance can
// be encoded as a special opcode.
//

#define CK_LINE_IS_SPECIAL_ENCODABLE(_LineAdvance, _OffsetAdvance) \
    (((_LineAdvance) >= CK_LINE_START) && \
     ((_LineAdvance) < (CK_LINE_START + CK_LINE_RANGE)) && \
     (CK_LINE_ENCODE_SPECIAL(_LineAdvance, _OffsetAdvance) <= \
      CkLineOpSpecialMax))

//
// This macro encodes the given line advance and offset advance into a special
// line opcode. Think of it like a table, where each row is an incrementing
// offset advance, and each column encodes an incrementing line advance.
//

#define CK_LINE_ENCODE_SPECIAL(_LineAdvance, _OffsetAdvance) \
    (CkLineOpSpecial + ((_OffsetAdvance) * CK_LINE_RANGE) + \
     ((_LineAdvance) - CK_LINE_START))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the initial number of abstract syntax tree nodes in the array.
//

#define CK_INITIAL_AST_NODES 64

//
// Define the initial number of locals to allocate.
//

#define CK_INITIAL_LOCALS 32

//
// Define the hardcoded parameters for the special opcodes in the line number
// program. The special opcodes encode a table of line advances (columns) and
// offset advances (rows). These parameters define the width of the table, as
// well as allow it to encode negative line advances.
//

#define CK_LINE_START (-4)
#define CK_LINE_RANGE 16

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _CK_LOOP CK_LOOP, *PCK_LOOP;

typedef enum _CK_SCOPE_TYPE {
    CkScopeInvalid,
    CkScopeLocal,
    CkScopeUpvalue,
    CkScopeModule
} CK_SCOPE_TYPE, *PCK_SCOPE_TYPE;

typedef enum _CK_LINE_OP {
    CkLineOpNop,
    CkLineOpSetLine,
    CkLineOpSetOffset,
    CkLineOpAdvanceLine,
    CkLineOpAdvanceOffset,
    CkLineOpSpecial,
    CkLineOpSpecialMax = 0xFF
} CK_LINE_OP, *PCK_LINE_OP;

/*++

Structure Description:

    This structure encapsulates the information for a variable during
    compilation.

Members:

    Index - Stores the index of the variable.

    Scope - Stores the scope to look for the variable in.

--*/

typedef struct _CK_VARIABLE {
    CK_SYMBOL_INDEX Index;
    CK_SCOPE_TYPE Scope;
} CK_VARIABLE, *PCK_VARIABLE;

/*++

Structure Description:

    This structure contains the context for a local variable.

Members:

    Name - Stores a pointer to the name of the local variable.

    Length - Stores the length of the local variable's name.

    Scope - Stores the scope index this local exists at. Zero is the outermost
        scope: parameters for a method, or the first local block in module
        level code.

    IsUpvalue - Stores a boolean indicating whether or not this local is being
        used as an upvalue.

--*/

typedef struct _CK_LOCAL {
    PSTR Name;
    ULONG Length;
    LONG Scope;
    BOOL IsUpvalue;
} CK_LOCAL, *PCK_LOCAL;

/*++

Structure Description:

    This structure describes an upvalue in the compiler.

Members:

    IsLocal - Stores whether or not this is capturing a local variable (TRUE)
        or an upvalue (FALSE).

    Index - Stores the index of the local or other upvalue being captured.

--*/

typedef struct _CK_COMPILER_UPVALUE {
    BOOL IsLocal;
    CK_SYMBOL_INDEX Index;
} CK_COMPILER_UPVALUE, *PCK_COMPILER_UPVALUE;

/*++

Structure Description:

    This structure contains the context for a loop that's being compiled.

Members:

    Start - Stores the index of the instruction that the loop should jump back
        to.

    ExitBody - Stores the index of the argument for the IF instruction used to
        exit the loop. This is stored so that it can be patched once the loop
        length is determined.

    Body - Stores the index of the first instruction of the loop body.

    Scope - Stores the scope index for the loop.

    Enclosing - Stores a pointer to the loop enclosing this one, or NULL if
        this is the outermost loop currently being processed.

--*/

struct _CK_LOOP {
    UINTN Start;
    UINTN ExitJump;
    UINTN Body;
    LONG Scope;
    PCK_LOOP Enclosing;
};

/*++

Structure Description:

    This structure describes a function signature.

Members:

    Name - Stores a pointer to the name of the function.

    Length - Stores the length of the function name, not including the null
        terminator.

    Arity - Stores the number of arguments the function takes.

--*/

typedef struct _CK_FUNCTION_SIGNATURE {
    PSTR Name;
    UINTN Length;
    CK_ARITY Arity;
} CK_FUNCTION_SIGNATURE, *PCK_FUNCTION_SIGNATURE;

/*++

Structure Description:

    This structure contains the context needed while compiling a class.

Members:

    Name - Stores a pointer to the class name.

    Fields - Stores the fields of the class.

    Methods - Stores the symbols for the methods in the class. This is used to
        reject duplicate methods.

    StaticMethods - Stores the names of the static methods in the class.

    InStatic - Stores a boolean indicating whether the current method being
        compiled is static.

    ClassVariable - Stores the class variable.

--*/

typedef struct _CK_CLASS_COMPILER {
    PCK_STRING_OBJECT Name;
    CK_SYMBOL_TABLE Fields;
    CK_INT_ARRAY Methods;
    CK_INT_ARRAY StaticMethods;
    BOOL InStatic;
    CK_VARIABLE ClassVariable;
} CK_CLASS_COMPILER, *PCK_CLASS_COMPILER;

/*++

Structure Description:

    This structure contains the context for a Chalk bytecode compiler.

Members:

    Locals - Stores a pointer to the array of local variables.

    LocalCount - Stores the number of valid local variables.

    LocalCapacity - Stores the maximum number of locals before the array will
        have to be reallocated.

    Upvalues - Stores a pointer to the array of upvalues.

    UpvalueCount - Stores the number of valid upvalues in the array.

    UpvalueCapacity - Stores the maximum number of upvalues before the array
        will have to be reallocated.

    ScopeDepth - Stores the current scope number being compiled.

    StackSlots - Stores the current number of stack slots being used for
        locals and temporaries. This is used to track the number of stack slots
        a function may need while executing. This does not include parameters,
        which are pushed by the caller.

    Loop - Stores a pointer to innermost loop currently being compiled.

    EnclosingClass - Stores a pointer to the innermost class currently being
        compiled.

    Function - Stores a pointer to the current function being compiled.

    Parser - Stores a pointer to the parser.

    Parent - Stores a pointer to the parent compiler if this is an inner
        function compiler.

    PreviousLine - Stores the last line number generated in the line number
        program. An empty line number program starts with this at zero.

    LineOffset - Stores the last offset generated in the line number program.
        An empty line number proram starts with this at zero.

    LastLineOp - Stores a pointer to the previous last line number program
        operation. Initially this is NULL. This is used to determine if the
        previous opcode can be updated to accomodate the next bytecode.

    Signature - Stores the current signature of the method or function being
        compiled.

    Assign - Stores a boolean indicating whether the next primary expression
        needs to be an lvalue or not.

    LastPrimaryToken - Stores the most recent primary expression token.

--*/

struct _CK_COMPILER {
    PCK_LOCAL Locals;
    ULONG LocalCount;
    ULONG LocalCapacity;
    PCK_COMPILER_UPVALUE Upvalues;
    ULONG UpvalueCount;
    ULONG UpvalueCapacity;
    LONG ScopeDepth;
    LONG StackSlots;
    PCK_LOOP Loop;
    PCK_CLASS_COMPILER EnclosingClass;
    PCK_FUNCTION Function;
    PCK_PARSER Parser;
    PCK_COMPILER Parent;
    INT Line;
    UINTN LineOffset;
    PUCHAR LastLineOp;
    PCK_FUNCTION_SIGNATURE Signature;
    BOOL Assign;
    CK_SYMBOL LastPrimaryToken;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

//
// General compiler support functions
//

VOID
CkpVisitNode (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

/*++

Routine Description:

    This routine compiles a node in the abstract syntax tree.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

//
// Expression compilation functions
//

VOID
CkpVisitExpression (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

/*++

Routine Description:

    This routine compiles an expression.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

VOID
CkpVisitAssignmentExpression (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

/*++

Routine Description:

    This routine compiles an assignment expression.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

VOID
CkpVisitConditionalExpression (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

/*++

Routine Description:

    This routine compiles a conditional expression.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

VOID
CkpVisitLogicalExpression (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

/*++

Routine Description:

    This routine compiles a logical and or logical or expression.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

VOID
CkpVisitBinaryExpression (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

/*++

Routine Description:

    This routine compiles a binary operator expression. Precedence isn't
    handled here since it's built into the grammar definition. The hierarchy
    of the nodes already reflects the correct precendence.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

VOID
CkpVisitUnaryExpression (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

/*++

Routine Description:

    This routine compiles a unary expression: [+-~! ++ --] postfix_expression.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

VOID
CkpVisitPostfixExpression (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

/*++

Routine Description:

    This routine compiles a postfix expression: e.id(...), e[...], e(...), e++,
    e--, and just simply e.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

VOID
CkpVisitPrimaryExpression (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

/*++

Routine Description:

    This routine compiles a primary expression: name, number, string, null,
    this, super, true, false, dict, list, and ( expression ).

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

VOID
CkpVisitDict (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

/*++

Routine Description:

    This routine compiles a dictionary constant.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

VOID
CkpVisitDictElementList (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

/*++

Routine Description:

    This routine visits the dict element list node, which contains the inner
    elements of a defined dictionary.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

VOID
CkpVisitList (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

/*++

Routine Description:

    This routine compiles a list constant.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

VOID
CkpVisitListElementList (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

/*++

Routine Description:

    This routine visits the list element list node, which contains the inner
    elements of a defined list.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

//
// Variable support functions
//

CK_SYMBOL_INDEX
CkpDeclareMethod (
    PCK_COMPILER Compiler,
    PCK_FUNCTION_SIGNATURE Signature,
    BOOL IsStatic,
    PLEXER_TOKEN NameToken,
    PSTR Name,
    UINTN Length
    );

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

    Returns the index into the giant table of methods for this function's
    signature.

--*/

CK_SYMBOL_INDEX
CkpGetSignatureSymbol (
    PCK_COMPILER Compiler,
    PCK_FUNCTION_SIGNATURE Signature
    );

/*++

Routine Description:

    This routine finds or creates a symbol index in the giant array of all
    method signatures.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Signature - Supplies a pointer to the function signature information.

Return Value:

    Returns the index into the giant table of methods for this function's
    signature.

--*/

CK_SYMBOL_INDEX
CkpGetMethodSymbol (
    PCK_COMPILER Compiler,
    PSTR Name,
    UINTN Length
    );

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

    Returns the index in the giant symbol table of method names.

--*/

VOID
CkpPrintSignature (
    PCK_FUNCTION_SIGNATURE Signature,
    PSTR Name,
    PUINTN Length
    );

/*++

Routine Description:

    This routine converts function signature information into a single string
    that contains the equivalent signature information.

Arguments:

    Signature - Supplies a pointer to the signature information.

    Name - Supplies a pointer to the function name.

    Length - Supplies a pointer that on input contains the size of the buffer.
        On output, this will contain the size of the signature string, not
        including the null terminator.

Return Value:

    None.

--*/

PCK_CLASS_COMPILER
CkpGetClassCompiler (
    PCK_COMPILER Compiler
    );

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

VOID
CkpLoadCoreVariable (
    PCK_COMPILER Compiler,
    PSTR Name
    );

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

VOID
CkpLoadThis (
    PCK_COMPILER Compiler,
    PLEXER_TOKEN Token
    );

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

CK_VARIABLE
CkpResolveNonGlobal (
    PCK_COMPILER Compiler,
    PSTR Name,
    UINTN Length
    );

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

VOID
CkpLoadVariable (
    PCK_COMPILER Compiler,
    CK_VARIABLE Variable
    );

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

VOID
CkpDefineVariable (
    PCK_COMPILER Compiler,
    CK_SYMBOL_INDEX Symbol
    );

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

CK_SYMBOL_INDEX
CkpDeclareVariable (
    PCK_COMPILER Compiler,
    PLEXER_TOKEN Token
    );

/*++

Routine Description:

    This routine creates a new variable slot in the current scope.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Token - Supplies the token containing the name of the variable.

Return Value:

    Returns the index of the new variable.

--*/

VOID
CkpPushScope (
    PCK_COMPILER Compiler
    );

/*++

Routine Description:

    This routine pushes a new local variable scope in the compiler.

Arguments:

    Compiler - Supplies a pointer to the compiler.

Return Value:

    None.

--*/

VOID
CkpPopScope (
    PCK_COMPILER Compiler
    );

/*++

Routine Description:

    This routine pops the most recent local variable scope, and clears any
    knowledge of local variables defined at that scope.

Arguments:

    Compiler - Supplies a pointer to the compiler.

Return Value:

    None.

--*/

CK_SYMBOL_INDEX
CkpDiscardLocals (
    PCK_COMPILER Compiler,
    LONG Depth
    );

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

VOID
CkpLoadLocal (
    PCK_COMPILER Compiler,
    CK_SYMBOL_INDEX Symbol
    );

/*++

Routine Description:

    This routine loads a local variable and pushes it onto the stack.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Symbol - Supplies the index of the local variable to load.

Return Value:

    None.

--*/

CK_SYMBOL_INDEX
CkpAddLocal (
    PCK_COMPILER Compiler,
    PSTR Name,
    UINTN Length
    );

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

CK_SYMBOL_INDEX
CkpAddConstant (
    PCK_COMPILER Compiler,
    CK_VALUE Constant
    );

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

VOID
CkpComplainIfAssigning (
    PCK_COMPILER Compiler,
    PLEXER_TOKEN Token,
    PSTR ExpressionName
    );

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

//
// Compiler I/O support functions
//

VOID
CkpStartLoop (
    PCK_COMPILER Compiler,
    PCK_LOOP Loop
    );

/*++

Routine Description:

    This routine begins compilation of a looping structure.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Loop - Supplies a pointer to the loop to initialize.

Return Value:

    None.

--*/

VOID
CkpTestLoopExit (
    PCK_COMPILER Compiler
    );

/*++

Routine Description:

    This routine emits the jump-if opcode used to test the loop condition and
    potentially exit the loop. It also keeps track of the place where this
    branch is emitted so it can be patched up once the end of the loop is
    compiled. The conditional expression should already be pushed on the
    stack.

Arguments:

    Compiler - Supplies a pointer to the compiler.

Return Value:

    None.

--*/

VOID
CkpCompileLoopBody (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

/*++

Routine Description:

    This routine compiles the body of a loop.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the compound statement node.

Return Value:

    None.

--*/

VOID
CkpEndLoop (
    PCK_COMPILER Compiler
    );

/*++

Routine Description:

    This routine cleans up the current loop.

Arguments:

    Compiler - Supplies a pointer to the compiler.

Return Value:

    None.

--*/

VOID
CkpEmitOperatorCall (
    PCK_COMPILER Compiler,
    CK_SYMBOL Operator,
    CK_ARITY Arguments,
    BOOL Assign
    );

/*++

Routine Description:

    This routine emits a call to service an operator.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Operator - Supplies the operator to call. The assignment operators are
        converted to their non assigning forms.

    Arguments - Supplies the number of arguments. Valid values are 0 and 1.
        This is really only needed to differentiate the unary operators + and -
        from the binary ones. This does not include a setter value argument
        for operators that have a setter form.

    Assign - Supplies a boolean indicating if this is the setter form.

Return Value:

    None.

--*/

VOID
CkpCallSignature (
    PCK_COMPILER Compiler,
    CK_OPCODE Op,
    PCK_FUNCTION_SIGNATURE Signature
    );

/*++

Routine Description:

    This routine emits a method call to a particular signature.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Op - Supplies the opcode, either CkOpCall0 or CkOpSuperCall0.

    Signature - Supplies a pointer to the signature to call.

Return Value:

    None.

--*/

VOID
CkpCallMethod (
    PCK_COMPILER Compiler,
    CK_ARITY ArgumentCount,
    PSTR Name,
    UINTN Length
    );

/*++

Routine Description:

    This routine emits a method call.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    ArgumentCount - Supplies the number of arguments to the function.

    Name - Supplies a pointer to the method signature string.

    Length - Supplies the length of the method name, not including the null
        terminator.

Return Value:

    None.

--*/

VOID
CkpDefineMethod (
    PCK_COMPILER Compiler,
    BOOL IsStatic,
    CK_SYMBOL_INDEX Symbol
    );

/*++

Routine Description:

    This routine emits the code for binding a method on a class.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    IsStatic - Supplies a boolean indicating if this is a static method or not.

    Symbol - Supplies the method symbol to bind.

Return Value:

    None. The core variable is pushed onto the stack.

--*/

VOID
CkpPatchJump (
    PCK_COMPILER Compiler,
    UINTN Offset
    );

/*++

Routine Description:

    This routine patches a previous jump location to point to the current end
    of the bytecode.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Offset - Supplies the offset where the jump target to be patched exists.
        This is the value that was returned from the emit jump instruction.

Return Value:

    None.

--*/

UINTN
CkpEmitJump (
    PCK_COMPILER Compiler,
    CK_OPCODE Op
    );

/*++

Routine Description:

    This routine emits some form of jump instruction. The jump location will
    be set to a placeholder value that will need to be patched up later.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Op - Supplies the jump opcode to emit.

Return Value:

    Returns the index into the code where the patched location will need to be
    set.

--*/

VOID
CkpEmitConstant (
    PCK_COMPILER Compiler,
    CK_VALUE Constant
    );

/*++

Routine Description:

    This routine adds a new constant value to the current function and pushes
    it on the stack.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Constant - Supplies the constant to push.

Return Value:

    None.

--*/

VOID
CkpEmitShortOp (
    PCK_COMPILER Compiler,
    CK_OPCODE Opcode,
    USHORT Argument
    );

/*++

Routine Description:

    This routine emits an opcode and a two-byte argument.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Opcode - Supplies the opcode to emit.

    Argument - Supplies the argument to emit.

Return Value:

    None.

--*/

VOID
CkpEmitByteOp (
    PCK_COMPILER Compiler,
    CK_OPCODE Opcode,
    UCHAR Argument
    );

/*++

Routine Description:

    This routine emits an opcode and a single byte argument.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Opcode - Supplies the opcode to emit.

    Argument - Supplies the argument to emit.

Return Value:

    None.

--*/

VOID
CkpEmitOp (
    PCK_COMPILER Compiler,
    CK_OPCODE Opcode
    );

/*++

Routine Description:

    This routine emits an opcode byte to the current instruction stream.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Opcode - Supplies the opcode to emit.

Return Value:

    None.

--*/

VOID
CkpEmitShort (
    PCK_COMPILER Compiler,
    USHORT Value
    );

/*++

Routine Description:

    This routine emits a two-byte value in big endian.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Value - Supplies the value to emit.

Return Value:

    None.

--*/

VOID
CkpEmitByte (
    PCK_COMPILER Compiler,
    UCHAR Byte
    );

/*++

Routine Description:

    This routine emits a byte to the current instruction stream.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Byte - Supplies the byte to emit.

Return Value:

    None.

--*/

CK_VALUE
CkpReadSourceInteger (
    PCK_COMPILER Compiler,
    PLEXER_TOKEN Token,
    INT Base
    );

/*++

Routine Description:

    This routine reads an integer literal.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Token - Supplies a pointer to the integer literal token.

    Base - Supplies the base of the integer.

Return Value:

    Returns the new integer value.

--*/

CK_VALUE
CkpReadSourceString (
    PCK_COMPILER Compiler,
    PLEXER_TOKEN Token
    );

/*++

Routine Description:

    This routine converts a string literal token into a string constant, and
    adds it as a constant.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Token - Supplies a pointer to the string literal token.

Return Value:

    Returns the new string value.

--*/

//
// Lexer functionality.
//

VOID
CkpInitializeLexer (
    PLEXER Lexer,
    PSTR Source,
    UINTN Length
    );

/*++

Routine Description:

    This routine initializes the Chalk lexer.

Arguments:

    Lexer - Stores a pointer to the lexer to initialize.

    Source - Stores a pointer to the null terminated source string to lex.

    Length - Supplies the length of the source string, not including the null
        terminator.

Return Value:

    None.

--*/

INT
CkpLexerGetToken (
    PVOID Lexer,
    PYY_VALUE Value
    );

/*++

Routine Description:

    This routine is called to get a new token from the input.

Arguments:

    Lexer - Supplies a pointer to the lexer context.

    Value - Supplies a pointer where the new token will be returned. The token
        data may be larger than simply a value, but it returns at least a
        value.

Return Value:

    0 on success, including EOF.

    Returns a non-zero value if there was an error reading the token.

--*/

