/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    compio.c

Abstract:

    This module implements support for reading in Chalk source and emitting
    bytecode.

Author:

    Evan Green 9-Jun-2016

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

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

UINTN
CkpGetInstructionSize (
    PUCHAR ByteCode,
    PCK_VALUE Constants,
    UINTN Ip
    );

VOID
CkpEmitLineNumberInformation (
    PCK_COMPILER Compiler,
    ULONG Line,
    ULONG Offset
    );

VOID
CkpReadUnicodeEscape (
    PCK_COMPILER Compiler,
    PCK_BYTE_ARRAY ByteArray,
    PLEXER_TOKEN Token,
    UINTN Offset,
    ULONG Length
    );

INT
CkpReadHexEscape (
    PCK_COMPILER Compiler,
    PLEXER_TOKEN Token,
    UINTN Offset,
    ULONG Length,
    PSTR Description
    );

INT
CkpReadHexDigit (
    CHAR Character
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the table of how each opcode affects the stack (always taking the
// worst case towards larger stacks.
//

CHAR CkOpcodeStackEffects[CkOpcodeCount] = {
    0,  // CkOpNop
    1,  // CkOpConstant
    1,  // CkOpStringConstant
    1,  // CkOpNull
    1,  // CkOpLiteral0
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,  // CkOpLiteral8
    1,  // CkOpLoadLocal0
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,  // CkOpLoadLocal8
    1,  // CkOpLoadLocal
    0,  // CkOpStoreLocal
    1,  // CkOpLoadUpvalue
    0,  // CkOpStoreUpvalue
    1,  // CkOpLoadModuleVariable
    0,  // CkOpStoreModuleVariable
    1,  // CkOpLoadFieldThis
    0,  // CkOpStoreFieldThis
    1,  // CkOpLoadField
    -1, // CkOpStoreField
    -1, // CkOpPop
    0,  // CkOpCall0
    -1,
    -2,
    -3,
    -4,
    -5,
    -6,
    -7,
    -8, // CkOpCall8
    0,  // CkOpCall (not actually zero)
    0,  // CkOpIndirectCall (not actually zero).
    0,  // CkOpSuperCall0
    -1,
    -2,
    -3,
    -4,
    -5,
    -6,
    -7,
    -8, // CkOpSuperCall8
    0,  // CkOpSuperCall (not actually zero).
    0,  // CkOpJump
    0,  // CkOpLoop
    -1, // CkOpJumpIf
    -1, // CkOpAnd
    -1, // CkOpOr
    -1, // CkOpCloseUpvalue
    -1,  // CkOpReturn
    1,  // CkOpClosure
    -1, // CkOpClass
    -2, // CkOpMethod
    -2, // CkOpStaticMethod
    0,  // CkOpTry
    0,  // CkOpPopTry
    0,  // CkOpEnd
};

//
// Define the number of bytes in operands for each instruction opcode.
//

UCHAR CkCompilerOperandSizes[CkOpcodeCount] = {
    0, // CkOpNop
    2, // CkOpConstant
    2, // CkOpStringConstant
    0, // CkOpNull
    0, // CkOpLiteral0
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, // CkOpLiteral8
    0, // CkOpLoadLocal0
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, // CkOpLoadLocal8
    1, // CkOpLoadLocal
    1, // CkOpStoreLocal
    1, // CkOpLoadUpvalue
    1, // CkOpStoreUpvalue
    2, // CkOpLoadModuleVariable
    2, // CkOpStoreModuleVariable
    1, // CkOpLoadFieldThis
    1, // CkOpStoreFieldThis
    1, // CkOpLoadField
    1, // CkOpStoreField
    0, // CkOpPop
    2, // CkOpCall0
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2, // CkOpCall8
    3, // CkOpCall
    1, // CkOpIndirectCall
    4, // CkOpSuperCall0
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4, // CkOpSuperCall8
    5, // CkOpSuperCall
    2, // CkOpJump
    2, // CkOpLoop
    2, // CkOpJumpIf
    2, // CkOpAnd
    2, // CkOpOr
    0, // CkOpCloseUpvalue
    0, // CkOpReturn
    0, // CkOpClosure (not actually zero)
    1, // CkOpClass
    2, // CkOpMethod
    2, // CkOpStaticMethod
    2, // CkOpTry
    0, // CkOpPopTry
    0, // CkOpEnd
};

//
// ------------------------------------------------------------------ Functions
//

VOID
CkpStartLoop (
    PCK_COMPILER Compiler,
    PCK_LOOP Loop
    )

/*++

Routine Description:

    This routine begins compilation of a looping structure.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Loop - Supplies a pointer to the loop to initialize.

Return Value:

    None.

--*/

{

    Loop->Enclosing = Compiler->Loop;
    Loop->Start = Compiler->Function->Code.Count - 1;
    Loop->Scope = Compiler->ScopeDepth;
    Loop->TryCount = 0;
    Compiler->Loop = Loop;
    return;
}

VOID
CkpTestLoopExit (
    PCK_COMPILER Compiler
    )

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

{

    Compiler->Loop->ExitJump = CkpEmitJump(Compiler, CkOpJumpIf);
    return;
}

VOID
CkpCompileLoopBody (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine compiles the body of a loop.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the compound statement node.

Return Value:

    None.

--*/

{

    CK_ASSERT(Node->Symbol == CkNodeCompoundStatement);

    Compiler->Loop->Body = Compiler->Function->Code.Count;
    CkpVisitNode(Compiler, Node);
    return;
}

VOID
CkpEndLoop (
    PCK_COMPILER Compiler
    )

/*++

Routine Description:

    This routine cleans up the current loop.

Arguments:

    Compiler - Supplies a pointer to the compiler.

Return Value:

    None.

--*/

{

    PUCHAR Code;
    UINTN CurrentIp;
    UINTN Index;
    UINTN LoopOffset;

    CurrentIp = Compiler->Function->Code.Count;
    LoopOffset = CurrentIp - Compiler->Loop->Start + 2;
    CkpEmitShortOp(Compiler, CkOpLoop, LoopOffset);
    Code = Compiler->Function->Code.Data;

    //
    // Patch up the jump of the original conditional now that the size of the
    // loop body code is known.
    //

    CkpPatchJump(Compiler, Compiler->Loop->ExitJump);

    //
    // Go through the loop body looking for break instructions, and patch them
    // with the end of loop location. The break instructions will be known
    // because they were emitted with the end opcode.
    //

    Index = Compiler->Loop->Body;
    while (Index < CurrentIp) {
        if (Code[Index] == CkOpEnd) {
            Code[Index] = CkOpJump;
            CkpPatchJump(Compiler, Index + 1);
            Index += 3;

        } else {
            Index += CkpGetInstructionSize(Code,
                                           Compiler->Function->Constants.Data,
                                           Index);
        }
    }

    Compiler->Loop = Compiler->Loop->Enclosing;
    return;
}

VOID
CkpEmitOperatorCall (
    PCK_COMPILER Compiler,
    CK_SYMBOL Operator,
    CK_ARITY Arguments,
    BOOL Assign
    )

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

{

    PSTR Method;

    Method = NULL;
    if (Arguments == 1) {

        CK_ASSERT((Assign == FALSE) ||
                  ((Operator == CkTokenOpenBracket) ||
                   (Operator == CkTokenDot)));

        switch (Operator) {
        case CkTokenIs:
            Method = "__is@1";
            break;

        case CkTokenRightShift:
        case CkTokenRightAssign:
            Method = "__rightShift@1";
            break;

        case CkTokenLeftShift:
        case CkTokenLeftAssign:
            Method = "__leftShift@1";
            break;

        case CkTokenLessOrEqual:
            Method = "__le@1";
            break;

        case CkTokenGreaterOrEqual:
            Method = "__ge@1";
            break;

        case CkTokenIsEqual:
            Method = "__eq@1";
            break;

        case CkTokenIsNotEqual:
            Method = "__ne@1";
            break;

        case CkTokenOpenBracket:
            if (Assign != FALSE) {
                Method = "__sliceAssign@2";

            } else {
                Method = "__slice@1";
            }

            break;

        case CkTokenBitAnd:
        case CkTokenAndAssign:
            Method = "__and@1";
            break;

        case CkTokenMinus:
        case CkTokenSubtractAssign:
            Method = "__sub@1";
            break;

        case CkTokenPlus:
        case CkTokenAddAssign:
            Method = "__add@1";
            break;

        case CkTokenAsterisk:
        case CkTokenMultiplyAssign:
            Method = "__mul@1";
            break;

        case CkTokenDivide:
        case CkTokenDivideAssign:
            Method = "__div@1";
            break;

        case CkTokenModulo:
        case CkTokenModuloAssign:
            Method = "__mod@1";
            break;

        case CkTokenLessThan:
            Method = "__lt@1";
            break;

        case CkTokenGreaterThan:
            Method = "__gt@1";
            break;

        case CkTokenXor:
        case CkTokenXorAssign:
            Method = "__xor@1";
            break;

        case CkTokenBitOr:
        case CkTokenOrAssign:
            Method = "__or@1";
            break;

        case CkTokenDot:
            if (Assign != FALSE) {
                Method = "__set@2";

            } else {
                Method = "__get@1";
            }

            break;

        case CkTokenDotDot:
            Method = "__rangeExclusive@1";
            break;

        case CkTokenDotDotDot:
            Method = "__rangeInclusive@1";
            break;

        default:
            break;
        }

    } else {

        CK_ASSERT((Arguments == 0) && (Assign == FALSE));

        switch (Operator) {
        case CkTokenIncrement:
            Method = "__inc@0";
            break;

        case CkTokenDecrement:
            Method = "__dec@0";
            break;

        case CkTokenLogicalNot:
            Method = "__lnot@0";
            break;

        case CkTokenBitNot:
            Method = "__compl@0";
            break;

        case CkTokenMinus:
            Method = "__neg@0";
            break;

        default:
            break;
        }
    }

    if (Method == NULL) {

        CK_ASSERT(FALSE);

        CkpCompileError(Compiler, NULL, "Unknown operator %d", Operator);
        return;
    }

    //
    // Assign can currently only be TRUE with open brackets.
    //

    if (Assign != FALSE) {
        Arguments += 1;
    }

    CkpEmitMethodCall(Compiler, Arguments, Method, strlen(Method));
    return;
}

VOID
CkpCallSignature (
    PCK_COMPILER Compiler,
    CK_OPCODE Op,
    PCK_FUNCTION_SIGNATURE Signature
    )

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

{

    CK_SYMBOL_INDEX Symbol;

    CK_ASSERT((Op == CkOpCall0) || (Op == CkOpSuperCall0));

    Symbol = CkpGetSignatureSymbol(Compiler, Signature);
    if (Signature->Arity <= 8) {
        CkpEmitShortOp(Compiler, Op + Signature->Arity, Symbol);

    } else {
        if (Op == CkOpCall0) {
            Op = CkOpCall;

        } else {
            Op = CkOpSuperCall;
        }

        //
        // Manually track stack usage since the op doesn't inherently
        // know it's stack effects.
        //

        Compiler->StackSlots -= Signature->Arity;
        CkpEmitByteOp(Compiler, Op, Signature->Arity);
        CkpEmitShort(Compiler, Symbol);
    }

    return;
}

VOID
CkpEmitMethodCall (
    PCK_COMPILER Compiler,
    CK_ARITY ArgumentCount,
    PSTR Name,
    UINTN Length
    )

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

{

    CK_SYMBOL_INDEX Symbol;

    //
    // Get the method number in the giant table of all method signatures.
    //

    Symbol = CkpGetMethodSymbol(Compiler, Name, Length);
    if (ArgumentCount <= 8) {
        CkpEmitShortOp(Compiler, CkOpCall0 + ArgumentCount, Symbol);

    } else {
        if (ArgumentCount >= MAX_UCHAR) {
            CkpCompileError(Compiler, NULL, "Too many arguments");
            return;
        }

        CkpEmitByteOp(Compiler, CkOpCall, ArgumentCount);
        CkpEmitShort(Compiler, Symbol);

        //
        // Manually track the stack usage since the instruction itself doesn't
        // have that information encoded.
        //

        Compiler->StackSlots -= ArgumentCount;
    }

    return;
}

VOID
CkpDefineMethod (
    PCK_COMPILER Compiler,
    BOOL IsStatic,
    CK_SYMBOL_INDEX Symbol
    )

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

{

    CK_OPCODE Op;

    //
    // If this compiler directly is not compiling a class, then define the
    // local or global that was created.
    //

    if (Compiler->EnclosingClass == NULL) {

        //
        // If this is the definition for a previous declaration, then put it
        // in the right place further down the stack.
        //

        if ((Compiler->ScopeDepth >= 0) &&
            (Symbol != Compiler->LocalCount - 1)) {

            CK_ASSERT(Symbol < CK_MAX_LOCALS);

            CkpEmitByteOp(Compiler, CkOpStoreLocal, Symbol);
            CkpEmitOp(Compiler, CkOpPop);

        //
        // This is a definition with no previous declaration.
        //

        } else {
            CkpDefineVariable(Compiler, Symbol);
        }

        return;
    }

    //
    // Load the class onto the stack.
    //

    CkpLoadVariable(Compiler, Compiler->EnclosingClass->ClassVariable);
    Op = CkOpMethod;
    if (IsStatic != FALSE) {
        Op = CkOpStaticMethod;
    }

    CkpEmitShortOp(Compiler, Op, Symbol);
    return;
}

VOID
CkpPatchJump (
    PCK_COMPILER Compiler,
    UINTN Offset
    )

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

{

    PUCHAR Bytes;
    UINTN JumpTarget;

    //
    // The extra two adjusts for the argument part of the jump instruction.
    //

    JumpTarget = Compiler->Function->Code.Count - Offset - 2;
    if (JumpTarget > CK_MAX_JUMP) {
        CkpCompileError(Compiler, NULL, "Jump too large");
    }

    Bytes = Compiler->Function->Code.Data + Offset;
    *Bytes = (UCHAR)(JumpTarget >> 8);
    Bytes += 1;
    *Bytes = (UCHAR)JumpTarget;
    return;
}

UINTN
CkpEmitJump (
    PCK_COMPILER Compiler,
    CK_OPCODE Op
    )

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

{

    UINTN Offset;

    CkpEmitOp(Compiler, Op);
    Offset = Compiler->Function->Code.Count;
    CkpEmitShort(Compiler, -1);
    return Offset;
}

VOID
CkpEmitConstant (
    PCK_COMPILER Compiler,
    CK_VALUE Constant
    )

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

{

    CK_SYMBOL_INDEX Index;

    //
    // If the constant is a string, emit a string constant op. Strings are
    // stored in their own table so they can be reused within a module.
    //

    if ((CK_IS_OBJECT(Constant)) &&
        (CK_AS_OBJECT(Constant)->Type == CkObjectString)) {

        Index = CkpAddStringConstant(Compiler, Constant);
        CkpEmitShortOp(Compiler, CkOpStringConstant, Index);

    } else {
        Index = CkpAddConstant(Compiler, Constant);
        CkpEmitShortOp(Compiler, CkOpConstant, Index);
    }

    return;
}

VOID
CkpEmitShortOp (
    PCK_COMPILER Compiler,
    CK_OPCODE Opcode,
    USHORT Argument
    )

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

{

    CkpEmitOp(Compiler, Opcode);
    CkpEmitShort(Compiler, Argument);
    return;
}

VOID
CkpEmitByteOp (
    PCK_COMPILER Compiler,
    CK_OPCODE Opcode,
    UCHAR Argument
    )

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

{

    CkpEmitOp(Compiler, Opcode);
    CkpEmitByte(Compiler, Argument);
    return;
}

VOID
CkpEmitOp (
    PCK_COMPILER Compiler,
    CK_OPCODE Opcode
    )

/*++

Routine Description:

    This routine emits an opcode byte to the current instruction stream.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Opcode - Supplies the opcode to emit.

    Line - Supplies the line number corresponding to this opcode.

Return Value:

    None.

--*/

{

    ULONG Offset;

    assert(Opcode <= CkOpEnd);

    Offset = Compiler->Function->Code.Count;
    CkpEmitByte(Compiler, Opcode);
    Compiler->StackSlots += CkOpcodeStackEffects[Opcode];
    if (Compiler->StackSlots > Compiler->Function->MaxStack) {
        Compiler->Function->MaxStack = Compiler->StackSlots;
    }

    CkpEmitLineNumberInformation(Compiler, Compiler->Line, Offset);
    return;
}

VOID
CkpEmitShort (
    PCK_COMPILER Compiler,
    USHORT Value
    )

/*++

Routine Description:

    This routine emits a two-byte value in big endian.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Value - Supplies the value to emit.

Return Value:

    None.

--*/

{

    CkpEmitByte(Compiler, (UCHAR)(Value >> 8));
    CkpEmitByte(Compiler, (UCHAR)Value);
    return;
}

VOID
CkpEmitByte (
    PCK_COMPILER Compiler,
    UCHAR Byte
    )

/*++

Routine Description:

    This routine emits a byte to the current instruction stream.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Byte - Supplies the byte to emit.

Return Value:

    None.

--*/

{

    CkpArrayAppend(Compiler->Parser->Vm, &(Compiler->Function->Code), Byte);
    return;
}

CK_VALUE
CkpReadSourceInteger (
    PCK_COMPILER Compiler,
    PLEXER_TOKEN Token,
    INT Base
    )

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

{

    CK_VALUE CkValue;
    PCSTR Current;
    INT Digit;
    PCSTR End;
    ULONGLONG PreviousValue;
    ULONGLONG Value;

    Current = Compiler->Parser->Source + Token->Position;
    End = Current + Token->Size;
    if (Base == 16) {

        CK_ASSERT((Token->Size > 2) &&
                  ((Current[1] == 'x') || (Current[1] == 'X')));

        Current += 2;

    } else if (Base == 2) {

        CK_ASSERT((Token->Size > 2) &&
                  ((Current[1] == 'b') || (Current[1] == 'B')));

        Current += 2;
    }

    Value = 0;
    while (Current < End) {
        if ((*Current >= '0') && (*Current <= '9')) {
            Digit = *Current - '0';

        } else if (Base >= 10) {
            if ((*Current >= 'a') && (*Current <= ('a' + Base - 10))) {
                Digit = *Current + 10 - 'a';

            } else if ((*Current >= 'A') && (*Current <= ('A' + Base - 10))) {
                Digit = *Current + 10 - 'A';

            } else {
                CkpCompileError(Compiler, Token, "Invalid number");
                break;
            }

        } else {
            CkpCompileError(Compiler, Token, "Invalid number");
            break;
        }

        if (Digit >= Base) {
            CkpCompileError(Compiler,
                            Token,
                            "Invalid digit %d for base %d integer",
                            Digit,
                            Base);
        }

        PreviousValue = Value;
        Value = (Value * Base) + Digit;
        if ((Value - Digit) / Base != PreviousValue) {
            CkpCompileError(Compiler, Token, "Integer too large");
        }

        Current += 1;
    }

    if (Value > CK_INT_MAX) {
        CkpCompileError(Compiler, Token, "Integer too large");
    }

    CK_INT_VALUE(CkValue, Value);
    return CkValue;
}

CK_VALUE
CkpReadSourceString (
    PCK_COMPILER Compiler,
    PLEXER_TOKEN Token
    )

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

{

    PCSTR BasicStart;
    CK_BYTE_ARRAY ByteArray;
    INT Character;
    PCSTR Current;
    PCSTR End;
    PCSTR Start;
    CK_VALUE StringValue;

    CkpInitializeArray(&ByteArray);
    Start = Compiler->Parser->Source + Token->Position;
    Current = Start;
    End = Current + Token->Size;

    CK_ASSERT((*Current == '"') && (Current + 2 <= End) && (*(End - 1) == '"'));

    Current += 1;
    End -= 1;
    while (Current < End) {

        //
        // Most of the string is probably not backslashes, so batch as much of
        // that together for copy as possible.
        //

        BasicStart = Current;
        while ((Current < End) && (*Current != '\\')) {
            Current += 1;
        }

        if (Current != BasicStart) {
            CkpFillArray(Compiler->Parser->Vm,
                         &ByteArray,
                         BasicStart,
                         Current - BasicStart);
        }

        if ((Current < End) && (*Current == '\\')) {
            Current += 1;

            CK_ASSERT(Current != End);

            switch (*Current) {
            case '"':
            case '\\':
                Character = *Current;
                break;

            case '0':
                Character = '\0';
                break;

            case 'a':
                Character = '\a';
                break;

            case 'b':
                Character = '\b';
                break;

            case 'f':
                Character = '\f';
                break;

            case 'n':
                Character = '\n';
                break;

            case 'r':
                Character = '\r';
                break;

            case 't':
                Character = '\t';
                break;

            case 'u':
                Character = -1;
                Current += 1;
                CkpReadUnicodeEscape(Compiler,
                                     &ByteArray,
                                     Token,
                                     Current - Start,
                                     4);

                Current += 4;
                break;

            case 'U':
                Character = -1;
                Current += 1;
                CkpReadUnicodeEscape(Compiler,
                                     &ByteArray,
                                     Token,
                                     Current - Start,
                                     8);

                Current += 8;
                break;

            case 'v':
                Character = '\v';
                break;

            case 'x':
                Current += 1;
                Character = CkpReadHexEscape(Compiler,
                                             Token,
                                             Current - Start,
                                             2,
                                             "byte");

                Current += 1;
                break;

            default:
                Character = -1;
                break;
            }

            if (Character != -1) {
                CkpArrayAppend(Compiler->Parser->Vm, &ByteArray, Character);
                Current += 1;
            }
        }
    }

    StringValue = CkpStringCreate(Compiler->Parser->Vm,
                                  (PSTR)(ByteArray.Data),
                                  ByteArray.Count);

    CkpClearArray(Compiler->Parser->Vm, &ByteArray);
    return StringValue;
}

//
// --------------------------------------------------------- Internal Functions
//

UINTN
CkpGetInstructionSize (
    PUCHAR ByteCode,
    PCK_VALUE Constants,
    UINTN Ip
    )

/*++

Routine Description:

    This routine determines the size of the instruction in the bytecode,
    including any operands.

Arguments:

    ByteCode - Supplies a pointer to the function bytecode stream.

    Constants - Supplies a pointer to the constants table for the function.

    Ip - Supplies the current instruction pointer.

Return Value:

    Returns the number of bytes in the instruction, including the opcode and
    any arguments.

--*/

{

    CK_SYMBOL_INDEX Constant;
    PCK_FUNCTION Function;
    CK_OPCODE Op;
    UINTN Size;

    Op = ByteCode[Ip];
    if (Op == CkOpClosure) {
        Constant = ((USHORT)ByteCode[Ip + 1] << 8) | ByteCode[Ip + 2];
        Function = CK_AS_FUNCTION(Constants[Constant]);

        //
        // There are two bytes for the constant, then two bytes for each
        // upvalue.
        //

        Size = 2 + (Function->UpvalueCount);

    } else if (Op < CkOpcodeCount) {
        Size = CkCompilerOperandSizes[Op];

    } else {

        CK_ASSERT(FALSE);

        Size = 0;
    }

    //
    // Count the op byte as well.
    //

    return Size + 1;
}

VOID
CkpEmitLineNumberInformation (
    PCK_COMPILER Compiler,
    ULONG Line,
    ULONG Offset
    )

/*++

Routine Description:

    This routine updates the line number program to include the latest bytecode
    that was emitted.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Line - Supplies the line number the latest bytecode belongs to.

    Offset - Supplies the offset within the bytecode that was just emitted.

Return Value:

    None.

--*/

{

    ULONG LastOffset;
    UCHAR LastOp;
    LONG LineAdvance;
    PCK_BYTE_ARRAY LineProgram;
    LONG OffsetAdvance;
    UCHAR Op;
    LONG PreviousAdvance;
    ULONG Size;
    ULONG Value;

    //
    // If this is the first thing ever emitted, initialize the first line.
    //

    if (Compiler->Function->Debug.FirstLine == 0) {

        CK_ASSERT(Line != 0);

        Compiler->Function->Debug.FirstLine = Line;
        Compiler->PreviousLine = Line;
    }

    //
    // Emit the next statement in the line number program used to store the
    // relationship between bytecode bytes and line numbers. The line program
    // is similar to the DWARF line program, except there are fewer opcodes and
    // the special opcode parameters are hardcoded. Start by seeing if the
    // info for this bytecode can be encoded by simply patching up the last
    // instruction.
    //

    LineProgram = &(Compiler->Function->Debug.LineProgram);
    if ((Compiler->LastLineOp != NULL) &&
        (Compiler->PreviousLine == Line)) {

        //
        // The last operation should always end in updating the address, since
        // updating the line number is an intermediate step. Since the current
        // line number is still the same, see if the address can simply be
        // moved forward a bit.
        //

        LastOp = *(Compiler->LastLineOp);

        //
        // If the offset was set explicitly, just replace it with the slightly
        // advanced offset.
        //

        if (LastOp == CkLineOpSetOffset) {
            CkCopy(Compiler->LastLineOp + 1, &Offset, sizeof(ULONG));
            Line = 0;

        //
        // If the offset was being advanced, unwind it to figure out the old
        // offset, and see if the new offset can be encoded instead.
        //

        } else if (LastOp == CkLineOpAdvanceOffset) {
            PreviousAdvance = CkpUtf8Decode(Compiler->LastLineOp + 1, 4);
            LastOffset = Compiler->LineOffset - PreviousAdvance;
            OffsetAdvance = Offset - LastOffset;

            CK_ASSERT(OffsetAdvance > 0);

            if ((OffsetAdvance <= CK_MAX_UTF8) &&
                (CkpUtf8EncodeSize(PreviousAdvance) ==
                 CkpUtf8EncodeSize(OffsetAdvance))) {

                CkpUtf8Encode(OffsetAdvance, Compiler->LastLineOp + 1);
                Line = 0;
            }

        //
        // Decode the special op to undo the instruction. Then see if this
        // offset will fit in a special opcode byte.
        //

        } else {

            CK_ASSERT(LastOp >= CkLineOpSpecial);

            LineAdvance = CK_LINE_ADVANCE(LastOp);
            LastOffset = Compiler->LineOffset - CK_OFFSET_ADVANCE(LastOp);
            OffsetAdvance = Offset - LastOffset;

            CK_ASSERT(OffsetAdvance > 0);

            if (CK_LINE_IS_SPECIAL_ENCODABLE(LineAdvance, OffsetAdvance)) {
                *(Compiler->LastLineOp) =
                            CK_LINE_ENCODE_SPECIAL(LineAdvance, OffsetAdvance);

                Line = 0;
            }
        }
    }

    //
    // If the previous instruction couldn't be patched to accommodate this new
    // bytecode, encode a new instruction.
    //

    if (Line != 0) {
        LineAdvance = Line - Compiler->PreviousLine;
        OffsetAdvance = Offset - Compiler->LineOffset;
        if (CK_LINE_IS_SPECIAL_ENCODABLE(LineAdvance, OffsetAdvance)) {
            Op = CK_LINE_ENCODE_SPECIAL(LineAdvance, OffsetAdvance);
            CkpArrayAppend(Compiler->Parser->Vm, LineProgram, Op);
            Compiler->LastLineOp = LineProgram->Data +
                                   LineProgram->Count - 1;

        //
        // The line or offset advance is too wild to encode with a special byte.
        // Use the bigger opcodes. Start with the line advance, as that is the
        // intermediate step.
        //

        } else {
            if (Line != Compiler->PreviousLine) {
                LineAdvance = Line - Compiler->PreviousLine;
                if ((LineAdvance > 0) && (LineAdvance < CK_MAX_UTF8)) {
                    Op = CkLineOpAdvanceLine;
                    Value = 0;
                    Size = CkpUtf8EncodeSize(LineAdvance);
                    CkpUtf8Encode(LineAdvance, (PUCHAR)&Value);

                } else {
                    Op = CkLineOpSetLine;
                    Value = Line;
                    Size = sizeof(ULONG);
                }

                CkpArrayAppend(Compiler->Parser->Vm, LineProgram, Op);
                CkpFillArray(Compiler->Parser->Vm,
                             LineProgram,
                             &Value,
                             Size);
            }

            OffsetAdvance = Offset - Compiler->LineOffset;

            CK_ASSERT(OffsetAdvance > 0);

            if (OffsetAdvance < CK_MAX_UTF8) {
                Op = CkLineOpAdvanceOffset;
                Value = 0;
                Size = CkpUtf8EncodeSize(OffsetAdvance);
                CkpUtf8Encode(OffsetAdvance, (PUCHAR)&Value);

            } else {
                Op = CkLineOpSetOffset;
                Value = Offset;
                Size = sizeof(ULONG);
            }

            CkpArrayAppend(Compiler->Parser->Vm,
                           &(Compiler->Function->Debug.LineProgram),
                           Op);

            CkpFillArray(Compiler->Parser->Vm,
                         &(Compiler->Function->Debug.LineProgram),
                         &Value,
                         Size);

            Compiler->LastLineOp = LineProgram->Data +
                                   LineProgram->Count - (Size + 1);
        }

        Compiler->PreviousLine = Line;
    }

    Compiler->LineOffset = Offset;
    return;
}

VOID
CkpReadUnicodeEscape (
    PCK_COMPILER Compiler,
    PCK_BYTE_ARRAY ByteArray,
    PLEXER_TOKEN Token,
    UINTN Offset,
    ULONG Length
    )

/*++

Routine Description:

    This routine reads a unicode escape sequence coded into a string.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    ByteArray - Supplies a pointer to the byte array being built.

    Token - Supplies the token being read from.

    Offset - Supplies the offset within the token to read from.

    Length - Supplies the number of characters in the escape sequence.

Return Value:

    None.

--*/

{

    INT Count;
    INT Value;

    Value = CkpReadHexEscape(Compiler, Token, Offset, Length, "Unicode");
    Count = CkpUtf8EncodeSize(Value);
    if (Count != 0) {
        CkpFillArray(Compiler->Parser->Vm, ByteArray, NULL, Count);
        CkpUtf8Encode(Value, ByteArray->Data + ByteArray->Count - Count);
    }

    return;
}

INT
CkpReadHexEscape (
    PCK_COMPILER Compiler,
    PLEXER_TOKEN Token,
    UINTN Offset,
    ULONG Length,
    PSTR Description
    )

/*++

Routine Description:

    This routine reads a sequence of hex characters as an escape sequence.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Token - Supplies the token being read from.

    Offset - Supplies the offset within the token to read from.

    Length - Supplies the number of characters in the escape sequence.

    Description - Supplies a pointer describing the purpose of this escape
        sequence.

Return Value:

    Returns the value the escape sequence was encoding.

--*/

{

    INT Digit;
    ULONG Index;
    PCSTR String;
    INT Value;

    String = Compiler->Parser->Source + Token->Position + Offset;
    if (Offset + Length > Token->Size) {
        CkpCompileError(Compiler,
                        Token,
                        "Incomplete %s escape sequence",
                        Description);

        return -1;
    }

    Value = 0;
    for (Index = 0; Index < Length; Index += 1) {
        Digit = CkpReadHexDigit(*String);
        if (Digit == -1) {
            CkpCompileError(Compiler,
                            Token,
                            "Invalid %s escape sequence",
                            Description);

            return -1;
        }

        Value = (Value << 4) | Digit;
        String += 1;
    }

    return Value;
}

INT
CkpReadHexDigit (
    CHAR Character
    )

/*++

Routine Description:

    This routine converts a hex digit character into a value.

Arguments:

    Character - Supplies the character to convert.

Return Value:

    Returns the value on success.

    -1 if this is not a hex digit.

--*/

{

    if ((Character >= '0') && (Character <= '9')) {
        return Character - '0';

    } else if ((Character >= 'a') && (Character <= 'f')) {
        return Character + 0xA - 'a';

    } else if ((Character >= 'A') && (Character <= 'F')) {
        return Character + 0xA - 'A';
    }

    return -1;
}

