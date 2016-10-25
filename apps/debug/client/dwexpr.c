/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dwexpr.c

Abstract:

    This module implements support for DWARF expressions and location lists.

Author:

    Evan Green 7-Dec-2015

Environment:

    Debug

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/lib/im.h>
#include "dwarfp.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
DwarfpEvaluateExpression (
    PDWARF_CONTEXT Context,
    PDWARF_LOCATION_CONTEXT LocationContext,
    PUCHAR Expression,
    UINTN Size
    );

PSTR
DwarfpGetOpName (
    DWARF_OP Op,
    PSTR Buffer,
    UINTN Size
    );

VOID
DwarfpExpressionPush (
    PDWARF_LOCATION_CONTEXT LocationContext,
    ULONGLONG Value
    );

ULONGLONG
DwarfpExpressionPop (
    PDWARF_LOCATION_CONTEXT LocationContext
    );

INT
DwarfpGetFrameBase (
    PDWARF_CONTEXT Context,
    PFUNCTION_SYMBOL Function,
    ULONGLONG Pc,
    PULONGLONG FrameBaseValue
    );

//
// -------------------------------------------------------------------- Globals
//

PSTR DwarfOpNames[] = {
    "DwarfOpNull",
    NULL,
    NULL,
    "DwarfOpAddress",
    NULL,
    NULL,
    "DwarfOpDereference",
    NULL,
    "DwarfOpConst1U",
    "DwarfOpConst1S",
    "DwarfOpConst2U",
    "DwarfOpConst2S",
    "DwarfOpConst4U",
    "DwarfOpConst4S",
    "DwarfOpConst8U",
    "DwarfOpConst8S",
    "DwarfOpConstU",
    "DwarfOpConstS",
    "DwarfOpDup",
    "DwarfOpDrop",
    "DwarfOpOver",
    "DwarfOpPick",
    "DwarfOpSwap",
    "DwarfOpRot",
    "DwarfOpXDeref",
    "DwarfOpAbs",
    "DwarfOpAnd",
    "DwarfOpDiv",
    "DwarfOpMinus",
    "DwarfOpMod",
    "DwarfOpMul",
    "DwarfOpNeg",
    "DwarfOpNot",
    "DwarfOpOr",
    "DwarfOpPlus",
    "DwarfOpPlusUConst",
    "DwarfOpShl",
    "DwarfOpShr",
    "DwarfOpShra",
    "DwarfOpXor",
    "DwarfOpBra",
    "DwarfOpEq",
    "DwarfOpGe",
    "DwarfOpGt",
    "DwarfOpLe",
    "DwarfOpLt",
    "DwarfOpNe",
    "DwarfOpSkip",
    "DwarfOpLit0"
};

PSTR DwarfOp90Names[] = {
    "DwarfOpRegX",
    "DwarfOpFbreg",
    "DwarfOpBregX",
    "DwarfOpPiece",
    "DwarfOpDerefSize",
    "DwarfOpXDerefSize",
    "DwarfOpNop",
    "DwarfOpPushObjectAddress",
    "DwarfOpCall2",
    "DwarfOpCall4",
    "DwarfOpCallRef",
    "DwarfOpFormTlsAddress",
    "DwarfOpCallFrameCfa",
    "DwarfOpBitPiece",
    "DwarfOpImplicitValue",
    "DwarfOpStackValue",
};

//
// ------------------------------------------------------------------ Functions
//

INT
DwarfpGetLocation (
    PDWARF_CONTEXT Context,
    PDWARF_LOCATION_CONTEXT LocationContext,
    PDWARF_ATTRIBUTE_VALUE AttributeValue
    )

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

    ENOENT if the attribute is a location list and none of the current PC is
    not in any of the locations.

    Returns an error number on failure.

--*/

{

    ULONGLONG Constant;
    PUCHAR Expression;
    UINTN ExpressionSize;
    INT Status;
    PDWARF_COMPILATION_UNIT Unit;

    Unit = LocationContext->Unit;

    assert(Unit != NULL);

    LocationContext->AddressSize = Unit->AddressSize;

    //
    // An expression location is the primary form to be dealt with.
    //

    if ((DWARF_SECTION_OFFSET_FORM(AttributeValue->Form, Unit)) ||
        (AttributeValue->Form == DwarfFormExprLoc) ||
        (DWARF_BLOCK_FORM(AttributeValue->Form))) {

        //
        // If it's a location list, find the expression that currently matches.
        //

        if (DWARF_SECTION_OFFSET_FORM(AttributeValue->Form, Unit)) {
            Status = DwarfpSearchLocationList(Context,
                                              Unit,
                                              AttributeValue->Value.Offset,
                                              LocationContext->Pc,
                                              &Expression,
                                              &ExpressionSize);

            if (Status != 0) {
                return Status;
            }

        //
        // Otherwise, use the expression built into the block.
        //

        } else {
            Expression = AttributeValue->Value.Block.Data;
            ExpressionSize = AttributeValue->Value.Block.Size;
        }

        LocationContext->Constant = TRUE;
        Status = DwarfpEvaluateExpression(Context,
                                          LocationContext,
                                          Expression,
                                          ExpressionSize);

    //
    // Try to just get a constant out of it.
    //

    } else {
        Status = 0;
        switch (AttributeValue->Form) {
        case DwarfFormData1:
        case DwarfFormData2:
        case DwarfFormData4:
        case DwarfFormData8:
        case DwarfFormSData:
        case DwarfFormUData:
            Constant = AttributeValue->Value.UnsignedConstant;
            break;

        case DwarfFormFlag:
        case DwarfFormFlagPresent:
            Constant = AttributeValue->Value.Flag;
            break;

        default:
            Status = ENOENT;
            break;
        }

        if (Status == 0) {
            LocationContext->Location.Form = DwarfLocationKnownValue;
            LocationContext->Location.Value.Value = Constant;
        }
    }

    return Status;
}

VOID
DwarfpDestroyLocationContext (
    PDWARF_CONTEXT Context,
    PDWARF_LOCATION_CONTEXT LocationContext
    )

/*++

Routine Description:

    This routine destroys a DWARF location context.

Arguments:

    Context - Supplies a pointer to the context.

    LocationContext - Supplies a pointer to the location context to clean up.

Return Value:

    None.

--*/

{

    PDWARF_LOCATION Next;
    PDWARF_LOCATION Piece;

    Piece = LocationContext->Location.NextPiece;
    LocationContext->Location.NextPiece = NULL;
    while (Piece != NULL) {
        Next = Piece->NextPiece;
        free(Piece);
        Piece = Next;
    }

    return;
}

INT
DwarfpEvaluateSimpleExpression (
    PDWARF_CONTEXT Context,
    UCHAR AddressSize,
    PDWARF_COMPILATION_UNIT Unit,
    ULONGLONG InitialPush,
    PUCHAR Expression,
    UINTN Size,
    PDWARF_LOCATION Location
    )

/*++

Routine Description:

    This routine evaluates a simple DWARF expression. A simple expression is
    one that is not possibly a location list, and will ultimately contain only
    a single piece.

Arguments:

    Context - Supplies a pointer to the DWARF context.

    AddressSize - Supplies the size of an address on the target.

    Unit - Supplies an optional pointer to the compilation unit.

    InitialPush - Supplies a value to push onto the stack initially. Supply
        -1ULL to not push anything onto the stack initially.

    Expression - Supplies a pointer to the expression bytes to evaluate.

    Size - Supplies the size of the expression in bytes.

    Location - Supplies a pointer where the location information will be
        returned on success.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    DWARF_LOCATION_CONTEXT LocationContext;
    INT Status;

    memset(&LocationContext, 0, sizeof(DWARF_LOCATION_CONTEXT));
    LocationContext.Unit = Unit;
    LocationContext.AddressSize = AddressSize;
    if (Unit != NULL) {
        LocationContext.AddressSize = Unit->AddressSize;
    }

    if (InitialPush != -1ULL) {
        LocationContext.Stack[0] = InitialPush;
        LocationContext.StackSize = 1;
    }

    Status = DwarfpEvaluateExpression(Context,
                                      &LocationContext,
                                      Expression,
                                      Size);

    if (Status == 0) {
        memcpy(Location, &(LocationContext.Location), sizeof(DWARF_LOCATION));
        if (Location->NextPiece != NULL) {
            DWARF_ERROR("DWARF: Simple expression had multiple pieces!\n");
            Location->NextPiece = NULL;
        }
    }

    DwarfpDestroyLocationContext(Context, &LocationContext);
    return Status;
}

VOID
DwarfpPrintExpression (
    PDWARF_CONTEXT Context,
    UCHAR AddressSize,
    PDWARF_COMPILATION_UNIT Unit,
    PUCHAR Expression,
    UINTN Size
    )

/*++

Routine Description:

    This routine prints out a DWARF expression.

Arguments:

    Context - Supplies a pointer to the context.

    AddressSize - Supplies the size of an address on the target.

    Unit - Supplies an optional pointer to the compilation unit.

    Expression - Supplies a pointer to the expression bytes.

    ExpressionEnd - Supplies the first byte beyond the expression bytes.

    Size - Supplies the size of the expression in bytes.

Return Value:

    None.

--*/

{

    PUCHAR Bytes;
    PUCHAR End;
    DWARF_OP Op;
    CHAR OpBuffer[30];
    ULONGLONG Operand1;
    BOOL Operand1Signed;
    ULONGLONG Operand2;
    BOOL Operand2Signed;
    ULONG OperandCount;
    PSTR OpName;

    Bytes = Expression;
    End = Expression + Size;
    while (Bytes < End) {
        Op = DwarfpRead1(&Bytes);
        OpName = DwarfpGetOpName(Op, OpBuffer, sizeof(OpBuffer));
        DWARF_PRINT("%s ", OpName);
        OperandCount = 1;
        Operand1Signed = FALSE;
        Operand2Signed = FALSE;
        if ((Op >= DwarfOpBreg0) && (Op <= DwarfOpBreg31)) {
            Operand1 = DwarfpReadSleb128(&Bytes);
            Operand1Signed = TRUE;

        } else {
            switch (Op) {
            case DwarfOpAddress:
                if (AddressSize == 4) {
                    Operand1 = DwarfpRead4(&Bytes);

                } else {

                    assert(AddressSize == 8);

                    Operand1 = DwarfpRead8(&Bytes);
                }

                break;

            case DwarfOpConst1U:
            case DwarfOpPick:
            case DwarfOpDerefSize:
            case DwarfOpXDerefSize:
                Operand1 = DwarfpRead1(&Bytes);
                break;

            case DwarfOpConst1S:
                Operand1 = (SCHAR)(DwarfpRead1(&Bytes));
                Operand1Signed = TRUE;
                break;

            case DwarfOpConst2U:
            case DwarfOpCall2:
                Operand1 = DwarfpRead2(&Bytes);
                break;

            case DwarfOpConst2S:
            case DwarfOpSkip:
            case DwarfOpBra:
                Operand1 = (SHORT)(DwarfpRead2(&Bytes));
                Operand1Signed = TRUE;
                break;

            case DwarfOpConst4U:
            case DwarfOpCall4:
                Operand1 = DwarfpRead4(&Bytes);
                break;

            case DwarfOpConst4S:
                Operand1 = (LONG)(DwarfpRead4(&Bytes));
                Operand1Signed = TRUE;
                break;

            case DwarfOpConst8U:
                Operand1 = DwarfpRead8(&Bytes);
                break;

            case DwarfOpConst8S:
                Operand1 = (LONGLONG)(DwarfpRead8(&Bytes));
                Operand1Signed = TRUE;
                break;

            case DwarfOpConstU:
            case DwarfOpPlusUConst:
            case DwarfOpRegX:
            case DwarfOpPiece:
                Operand1 = DwarfpReadLeb128(&Bytes);
                break;

            case DwarfOpConstS:
            case DwarfOpFbreg:
                Operand1 = DwarfpReadSleb128(&Bytes);
                Operand1Signed = TRUE;
                break;

            case DwarfOpBregX:
                Operand1 = DwarfpReadLeb128(&Bytes);
                Operand2 = DwarfpReadSleb128(&Bytes);
                Operand2Signed = TRUE;
                OperandCount = 2;
                break;

            case DwarfOpCallRef:
                Operand1 = 0;
                if (Unit != NULL) {
                    Operand1 = DWARF_READN(&Bytes, Unit->Is64Bit);
                }

                break;

            case DwarfOpBitPiece:
                Operand1 = DwarfpReadLeb128(&Bytes);
                Operand2 = DwarfpReadLeb128(&Bytes);
                OperandCount = 2;
                break;

            case DwarfOpImplicitValue:
                Operand1 = DwarfpReadLeb128(&Bytes);
                Bytes += Operand1;
                break;

            case DwarfOpGnuEntryValue:
                Operand1 = DwarfpReadLeb128(&Bytes);
                break;

            case DwarfOpGnuImplicitPointer:
                if (AddressSize == 8) {
                    Operand1 = DwarfpRead8(&Bytes);

                } else {
                    Operand1 = DwarfpRead4(&Bytes);
                }

                Operand2 = DwarfpReadSleb128(&Bytes);
                Operand2Signed = TRUE;
                OperandCount = 2;
                break;

            case DwarfOpGnuConstType:
                Operand1 = DwarfpReadLeb128(&Bytes);
                Operand2 = DwarfpRead1(&Bytes);
                Bytes += Operand2;
                break;

            case DwarfOpGnuConvert:
            case DwarfOpGnuReinterpret:
                Operand1 = DwarfpReadLeb128(&Bytes);
                break;

            //
            // Parameter references point to a DIE that contains an
            // optimized-away parameter.
            //

            case DwarfOpGnuParameterRef:
                Operand1 = DwarfpRead4(&Bytes);
                break;

            default:
                OperandCount = 0;
                break;
            }
        }

        if (OperandCount != 0) {
            if (Operand1Signed != FALSE) {
                DWARF_PRINT("%I64d ", Operand1);

            } else {
                DWARF_PRINT("%I64u ", Operand1);
            }

            if (OperandCount == 2) {
                if (Operand2Signed != FALSE) {
                    DWARF_PRINT("%I64d ", Operand2);

                } else {
                    DWARF_PRINT("%I64u ", Operand2);
                }
            }
        }
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
DwarfpEvaluateExpression (
    PDWARF_CONTEXT Context,
    PDWARF_LOCATION_CONTEXT LocationContext,
    PUCHAR Expression,
    UINTN Size
    )

/*++

Routine Description:

    This routine executes a DWARF expression.

Arguments:

    Context - Supplies a pointer to the context.

    LocationContext - Supplies a pointer to the location context to clean up.

    Expression - Supplies a pointer to the expression bytes.

    Size - Supplies the size of the expression in bytes.

Return Value:

    0 on success.

    Returns a status code on failure.

--*/

{

    UCHAR AddressSize;
    PUCHAR End;
    STACK_FRAME Frame;
    ULONG Index;
    PDWARF_LOCATION Location;
    DWARF_OP Op;
    PDWARF_LOCATION PreviousLocation;
    ULONG SizeOperand;
    ULONG StackSize;
    INT Status;
    ULONGLONG Value;
    ULONGLONG Value2;
    ULONGLONG Value3;

    AddressSize = LocationContext->AddressSize;

    assert((AddressSize == 8) || (AddressSize == 4));

    End = Expression + Size;
    PreviousLocation = NULL;
    Location = &(LocationContext->Location);
    Status = 0;
    while (Expression < End) {
        Op = DwarfpRead1(&Expression);
        switch (Op) {

        //
        // Push the one operand, which is the size of a target address.
        //

        case DwarfOpAddress:
            if (AddressSize == 8) {
                Value = DwarfpRead8(&Expression);

            } else {
                Value = DwarfpRead4(&Expression);
            }

            DwarfpExpressionPush(LocationContext, Value);
            break;

        //
        // Dereference pops an address, reads up to an address size's worth of
        // data from the target memory at that location, and pushes that back
        // on the stack. The X variants also pop an address space ID. The size
        // variants specify the size to read explicitly.
        //

        case DwarfOpXDerefSize:
        case DwarfOpXDeref:
        case DwarfOpDereference:
        case DwarfOpDerefSize:
        case DwarfOpGnuDerefType:
            Value = DwarfpExpressionPop(LocationContext);
            SizeOperand = AddressSize;
            if ((Op == DwarfOpXDerefSize) || (Op == DwarfOpDerefSize) ||
                (Op == DwarfOpGnuDerefType)) {

                SizeOperand = DwarfpRead1(&Expression);
                if (SizeOperand > AddressSize) {
                    SizeOperand = AddressSize;
                }
            }

            //
            // Scan past the DIE offset of a type to interpret this as.
            //

            if (Op == DwarfOpGnuDerefType) {
                DwarfpReadLeb128(&Expression);
            }

            //
            // Pop the address space ID if this is an X operation.
            //

            Value2 = 0;
            if ((Op == DwarfOpXDerefSize) || (Op == DwarfOpXDeref)) {
                Value2 = DwarfpExpressionPop(LocationContext);
            }

            Value3 = 0;
            LocationContext->Constant = FALSE;
            Status = DwarfTargetRead(Context,
                                     Value,
                                     SizeOperand,
                                     Value2,
                                     &Value3);

            if (Status != 0) {
                DWARF_ERROR("DWARF: Target read failure from address "
                            "0x%I64x (address space %d).\n",
                            Value,
                            (ULONG)Value2);

                goto EvaluateExpressionEnd;
            }

            DwarfpExpressionPush(LocationContext, Value3);
            break;

        case DwarfOpConst1U:
            Value = DwarfpRead1(&Expression);
            DwarfpExpressionPush(LocationContext, Value);
            break;

        case DwarfOpConst1S:
            Value = (LONGLONG)(SCHAR)(DwarfpRead1(&Expression));
            DwarfpExpressionPush(LocationContext, Value);
            break;

        case DwarfOpConst2U:
            Value = DwarfpRead2(&Expression);
            DwarfpExpressionPush(LocationContext, Value);
            break;

        case DwarfOpConst2S:
            Value = (LONGLONG)(SHORT)(DwarfpRead2(&Expression));
            DwarfpExpressionPush(LocationContext, Value);
            break;

        case DwarfOpConst4U:
            Value = DwarfpRead4(&Expression);
            DwarfpExpressionPush(LocationContext, Value);
            break;

        case DwarfOpConst4S:
            Value = (LONGLONG)(LONG)(DwarfpRead4(&Expression));
            DwarfpExpressionPush(LocationContext, Value);
            break;

        case DwarfOpConst8U:
            Value = DwarfpRead8(&Expression);
            DwarfpExpressionPush(LocationContext, Value);
            break;

        case DwarfOpConst8S:
            Value = (LONGLONG)(DwarfpRead8(&Expression));
            DwarfpExpressionPush(LocationContext, Value);
            break;

        case DwarfOpConstU:
            Value = DwarfpReadLeb128(&Expression);
            DwarfpExpressionPush(LocationContext, Value);
            break;

        case DwarfOpConstS:
            Value = DwarfpReadSleb128(&Expression);
            DwarfpExpressionPush(LocationContext, Value);
            break;

        //
        // Duplicate the value at the top of the stack.
        //

        case DwarfOpDup:
            Value = DwarfpExpressionPop(LocationContext);
            DwarfpExpressionPush(LocationContext, Value);
            DwarfpExpressionPush(LocationContext, Value);
            break;

        //
        // Pop and ignore the value at the top of the stack.
        //

        case DwarfOpDrop:
            DwarfpExpressionPop(LocationContext);
            break;

        //
        // Over is equivalent to pick(1).
        //

        case DwarfOpOver:
            Index = 1;

            //
            // Fall through.
            //

        //
        // Copy and push the stack entry at the specified index.
        //

        case DwarfOpPick:
            if (Op == DwarfOpPick) {
                Index = DwarfpRead1(&Expression);

            } else {
                Index = 0;
            }

            StackSize = LocationContext->StackSize;
            if (Index < StackSize) {
                Value = LocationContext->Stack[StackSize - 1 - Index];
                DwarfpExpressionPush(LocationContext, Value);

            } else {

                assert(FALSE);
            }

            break;

        //
        // Swap the top two entries of the stack.
        //

        case DwarfOpSwap:
            Value = DwarfpExpressionPop(LocationContext);
            Value2 = DwarfpExpressionPop(LocationContext);
            DwarfpExpressionPush(LocationContext, Value);
            DwarfpExpressionPush(LocationContext, Value2);
            break;

        //
        // Rotate the first three stack entries.
        //

        case DwarfOpRot:
            Value = DwarfpExpressionPop(LocationContext);
            Value2 = DwarfpExpressionPop(LocationContext);
            Value3 = DwarfpExpressionPop(LocationContext);
            DwarfpExpressionPush(LocationContext, Value);
            DwarfpExpressionPush(LocationContext, Value3);
            DwarfpExpressionPush(LocationContext, Value2);
            break;

        //
        // Handle unary arithmetic operators.
        //

        case DwarfOpAbs:
        case DwarfOpNot:
        case DwarfOpNeg:
            Value = DwarfpExpressionPop(LocationContext);
            switch (Op) {
            case DwarfOpAbs:
                if ((LONGLONG)Value < 0) {
                    Value = -Value;
                }

                break;

            case DwarfOpNot:
                Value = ~Value;
                break;

            case DwarfOpNeg:
                Value = -(LONGLONG)Value;
                break;

            default:

                assert(FALSE);

                Value = 0;
                break;
            }

            DwarfpExpressionPush(LocationContext, Value);
            break;

        //
        // Handle arithmetic operators, that pop two values, compute something,
        // and then push the value back. The second value on the stack is
        // the "thing to operate on", and the first value is the operand.
        //

        case DwarfOpAnd:
        case DwarfOpDiv:
        case DwarfOpMinus:
        case DwarfOpMod:
        case DwarfOpMul:
        case DwarfOpOr:
        case DwarfOpPlus:
        case DwarfOpShl:
        case DwarfOpShr:
        case DwarfOpShra:
        case DwarfOpXor:
        case DwarfOpEq:
        case DwarfOpGe:
        case DwarfOpGt:
        case DwarfOpLe:
        case DwarfOpLt:
        case DwarfOpNe:
            Value = DwarfpExpressionPop(LocationContext);
            Value2 = DwarfpExpressionPop(LocationContext);
            switch (Op) {
            case DwarfOpAnd:
                Value3 = Value & Value2;
                break;

            case DwarfOpDiv:
                Value3 = 0;
                if (Value != 0) {
                    Value3 = (LONGLONG)Value2 / (LONGLONG)Value;
                }

                break;

            case DwarfOpMinus:
                Value3 = Value2 - Value;
                break;

            case DwarfOpMod:
                Value3 = 0;
                if (Value3 != 0) {
                    Value3 = (LONGLONG)Value2 % (LONGLONG)Value;
                }

                break;

            case DwarfOpMul:
                Value3 = Value * Value2;
                break;

            case DwarfOpOr:
                Value3 = Value | Value2;
                break;

            case DwarfOpPlus:
                Value3 = Value + Value2;
                break;

            case DwarfOpShl:
                Value3 = Value2 << Value;
                break;

            case DwarfOpShr:
                Value3 = Value2 >> Value;
                break;

            case DwarfOpShra:
                Value3 = ((LONGLONG)Value2) >> Value;
                break;

            case DwarfOpXor:
                Value3 = Value ^ Value2;
                break;

            case DwarfOpEq:
                Value3 = Value == Value2;
                break;

            case DwarfOpGe:
                Value3 = Value2 >= Value;
                break;

            case DwarfOpGt:
                Value3 = Value2 > Value;
                break;

            case DwarfOpLe:
                Value3 = Value2 <= Value;
                break;

            case DwarfOpLt:
                Value3 = Value2 < Value;
                break;

            case DwarfOpNe:
                Value3 = Value2 != Value;
                break;

            default:

                assert(FALSE);

                Value3 = 0;
                break;
            }

            DwarfpExpressionPush(LocationContext, Value3);
            break;

        //
        // Pop the top value, add it to the LEB128 operand, and push the
        // result.
        //

        case DwarfOpPlusUConst:
            Value = DwarfpExpressionPop(LocationContext);
            Value2 = DwarfpReadLeb128(&Expression);
            Value += Value2;
            DwarfpExpressionPush(LocationContext, Value);
            break;

        //
        // Conditional branch. If the top value of the stack is non-zero,
        // branch to the 2-byte signed operand away.
        // TODO: Is the branch (and skip) from the next instruction or this one?
        //

        case DwarfOpBra:
            Value = DwarfpExpressionPop(LocationContext);
            Value2 = (SHORT)DwarfpRead2(&Expression);
            if (Value != 0) {
                Expression += Value2;
            }

            break;

        case DwarfOpSkip:
            Value2 = (SHORT)DwarfpRead2(&Expression);
            Expression += Value2;
            break;

        case DwarfOpCall2:
        case DwarfOpCall4:
        case DwarfOpCallRef:
            if ((Op == DwarfOpCall2) || (Op == DwarfOpCall4)) {
                if (Op == DwarfOpCall2) {
                    Value = DwarfpRead2(&Expression);

                } else {
                    Value = DwarfpRead4(&Expression);
                }

            } else {
                if (LocationContext->Unit == NULL) {
                    Status = EINVAL;
                    goto EvaluateExpressionEnd;
                }

                if (LocationContext->Unit->Is64Bit != FALSE) {
                    Value = DwarfpRead8(&Expression);

                } else {
                    Value = DwarfpRead4(&Expression);
                }
            }

            //
            // Calls are not currently implemented. Call2 and Call4 are not
            // so bad as they involve finding the DIE in question (a little
            // trouble, but not too bad), then getting and executing the
            // attribute. The trouble with the ref call is that it points to a
            // DIE in some other module, but with no way to find the
            // abbreviation tables or compilation unit for that DIE. No one
            // seems to implement or use it, so for now just ignore all this.
            //

            assert(FALSE);

            break;

        case DwarfOpFbreg:
            LocationContext->Constant = FALSE;
            Status = DwarfpGetFrameBase(Context,
                                        LocationContext->CurrentFunction,
                                        LocationContext->Pc,
                                        &Value);

            if (Status != 0) {
                DWARF_ERROR("DWARF: Failed to get frame base.\n");
                goto EvaluateExpressionEnd;
            }

            Value2 = DwarfpReadSleb128(&Expression);
            Value += Value2;
            DwarfpExpressionPush(LocationContext, Value);
            break;

        case DwarfOpCallFrameCfa:
            LocationContext->Constant = FALSE;
            Status = DwarfpStackUnwind(Context,
                                       LocationContext->Pc,
                                       TRUE,
                                       &Frame);

            if (Status != 0) {
                DWARF_ERROR("DWARF: Failed to get CFA.\n");
                goto EvaluateExpressionEnd;
            }

            Value = Frame.FramePointer;
            DwarfpExpressionPush(LocationContext, Value);
            break;

        //
        // Piece defines that a portion of the location resides here. Bit piece
        // takes the size and offset in bits.
        //

        case DwarfOpPiece:
        case DwarfOpBitPiece:
            if (Op == DwarfOpPiece) {
                Value = DwarfpReadLeb128(&Expression) * BITS_PER_BYTE;
                Value2 = 0;

            } else {
                Value = DwarfpReadLeb128(&Expression);
                Value2 = DwarfpReadLeb128(&Expression);
            }

            //
            // If the location is not yet formed, grab its value off the stack.
            //

            if (Location->Form == DwarfLocationInvalid) {
                StackSize = LocationContext->StackSize;
                if (StackSize != 0) {
                    Location->Form = DwarfLocationMemory;
                    Location->Value.Address =
                                         LocationContext->Stack[StackSize - 1];

                } else {
                    Location->Form = DwarfLocationUndefined;
                }
            }

            Location->BitSize = Value;
            Location->BitOffset = Value2;
            if (PreviousLocation != NULL) {
                PreviousLocation->NextPiece = Location;
            }

            PreviousLocation = Location;

            //
            // Create a new location piece if there is more stuff.
            //

            Location = NULL;
            if (Expression < End) {
                Location = malloc(sizeof(DWARF_LOCATION));
                if (Location == NULL) {
                    Status = ENOMEM;
                    goto EvaluateExpressionEnd;
                }

                memset(Location, 0, sizeof(DWARF_LOCATION));
            }

            //
            // Clear the stack. It's not obvious from the spec whether or not
            // this is the right thing to do, so change this if things aren't
            // working.
            //

            LocationContext->StackSize = 0;
            break;

        case DwarfOpNop:
            break;

        case DwarfOpPushObjectAddress:
            Value = LocationContext->ObjectAddress;
            DwarfpExpressionPush(LocationContext, Value);
            break;

        //
        // Pop the value, add it to the current thread and module's TLS base,
        // and push it back.
        //

        case DwarfOpFormTlsAddress:
        case DwarfOpGnuPushTlsAddress:
            LocationContext->Constant = FALSE;
            Value = DwarfpExpressionPop(LocationContext);
            Value += LocationContext->TlsBase;
            DwarfpExpressionPush(LocationContext, Value);
            break;

        //
        // Implicit value specifies that there is no location, but the value
        // is known.
        //

        case DwarfOpImplicitValue:
            Value = DwarfpReadLeb128(&Expression);
            Location->Form = DwarfLocationKnownData;
            Location->Value.Buffer.Data = Expression;
            Location->Value.Buffer.Size = Value;
            Expression += Value;
            break;

        //
        // Stack value specifies that there is no location, but the value
        // itself is at the top of the stack. This also terminates the
        // expression.
        //

        case DwarfOpStackValue:
            Value = DwarfpExpressionPop(LocationContext);
            Location->Form = DwarfLocationKnownValue;
            Location->Value.Value = Value;
            Status = 0;
            Expression = End;
            break;

        //
        // The variable is uninitialized.
        //

        case DwarfOpGnuUninit:
            break;

        //
        // The entry value contains a LEB128 length, followed by a block of
        // DWARF expression. The expression is either a DWARF register op,
        // or a generic expression. The expression should be evaluated as if
        // the machine was at the beginning of the current function. That is,
        // "unwind this function and then run the inner expression". For now
        // just push 0 and skip the whole thing.
        //

        case DwarfOpGnuEntryValue:
            Value = DwarfpReadLeb128(&Expression);
            Expression += Value;
            DwarfpExpressionPush(LocationContext, 0);
            break;

        //
        // The implicit pointer informs the user that while the location of
        // an object is unavailable, the actual value of that object can be
        // known. It has two operands, an address-sized offset to a DIE
        // describing the value of the variable (in it's location attribute),
        // and a SLEB128 byte offset into that value. Currently this is just
        // returned as undefined.
        //

        case DwarfOpGnuImplicitPointer:
            if (AddressSize == 8) {
                Value = DwarfpRead8(&Expression);

            } else {
                Value = DwarfpRead4(&Expression);
            }

            DwarfpReadSleb128(&Expression);
            Location->Form = DwarfLocationUndefined;
            break;

        case DwarfOpGnuAddrIndex:
        case DwarfOpGnuConstIndex:

            //
            // Consider implementing (or at least ignoring) these extensions.
            //

            assert(FALSE);

            break;

        //
        // Constant data, preceded by a type DIE offset.
        //

        case DwarfOpGnuConstType:
            DwarfpReadLeb128(&Expression);
            Size = DwarfpRead1(&Expression);
            Value = 0;
            if (Size <= sizeof(ULONGLONG)) {
                memcpy(&Value, Expression, Size);
            }

            DwarfpExpressionPush(LocationContext, Value);
            Expression += Size;
            break;

        //
        // Convert and reinterpret pop a value off the stack, cast it to
        // the given type (specified by a DIE offset to a type), and push the
        // value back. Just ignore this for now.
        //

        case DwarfOpGnuConvert:
        case DwarfOpGnuReinterpret:
            DwarfpReadLeb128(&Expression);
            break;

        //
        // Parameter references point to a DIE that contains an optimized-away
        // parameter.
        //

        case DwarfOpGnuParameterRef:
            DwarfpRead4(&Expression);
            DwarfpExpressionPush(LocationContext, 0);
            break;

        //
        // Handle unknown or ranges of values.
        //

        default:

            //
            // Handle the literal encodings.
            //

            if ((Op >= DwarfOpLit0) && (Op <= DwarfOpLit31)) {
                Value = Op - DwarfOpLit0;
                DwarfpExpressionPush(LocationContext, Value);
                break;

            //
            // Return register locations themselves.
            //

            } else if (((Op >= DwarfOpReg0) && (Op <= DwarfOpReg31)) ||
                       (Op == DwarfOpRegX)) {

                if (Op == DwarfOpRegX) {
                    Value = DwarfpReadLeb128(&Expression);

                } else {
                    Value = Op - DwarfOpReg0;
                }

                Location->Form = DwarfLocationRegister;
                Location->Value.Register = Value;
                break;

            //
            // Handle the register encodings.
            //

            } else if (((Op >= DwarfOpBreg0) && (Op <= DwarfOpBreg31)) ||
                       (Op == DwarfOpBregX) || (Op == DwarfOpGnuRegvalType)) {

                LocationContext->Constant = FALSE;
                Value2 = 0;
                if (Op == DwarfOpBregX) {
                    Value = DwarfpReadLeb128(&Expression);
                    Value2 = DwarfpReadSleb128(&Expression);

                //
                // This regval type extension reads a register and interprets
                // it as a given type (specified by a DIE offset).
                //

                } else if (Op == DwarfOpGnuRegvalType) {
                    Value = DwarfpReadLeb128(&Expression);
                    DwarfpReadLeb128(&Expression);
                    Value2 = 0;

                } else {
                    Value = Op - DwarfOpBreg0;
                    Value2 = DwarfpReadSleb128(&Expression);
                }

                Status = DwarfTargetReadRegister(Context, Value, &Value);
                if (Status != 0) {
                    DWARF_ERROR("DWARF: Failed to read register %I64d\n",
                                Value);

                    goto EvaluateExpressionEnd;
                }

                Value += Value2;
                DwarfpExpressionPush(LocationContext, Value);
                break;
            }

            DWARF_ERROR("DWARF: Unhandled expression op 0x%x", Op);

            assert(FALSE);

            Status = ENOSYS;
            goto EvaluateExpressionEnd;
        }
    }

    //
    // If this is the end and the current location has not yet been filled in,
    // assume it's a memory location at the top of the stack.
    //

    assert(Expression == End);

    if ((Location != NULL) && (Location->Form == DwarfLocationInvalid)) {
        StackSize = LocationContext->StackSize;
        if (StackSize != 0) {
            Location->Form = DwarfLocationMemory;
            Location->Value.Address = LocationContext->Stack[StackSize - 1];

        } else {
            Location->Form = DwarfLocationUndefined;
        }

        if (PreviousLocation != NULL) {
            PreviousLocation->NextPiece = Location;
        }

        Location = NULL;
    }

EvaluateExpressionEnd:

    //
    // Free a leftover location.
    //

    if ((Location != NULL) && (Location != &(LocationContext->Location))) {

        assert((PreviousLocation != NULL) &&
               (PreviousLocation->NextPiece == NULL));

        free(Location);
    }

    return Status;
}

PSTR
DwarfpGetOpName (
    DWARF_OP Op,
    PSTR Buffer,
    UINTN Size
    )

/*++

Routine Description:

    This routine returns the string describing a dwarf op.

Arguments:

    Op - Supplies the op to decode.

    Buffer - Supplies a pointer to a buffer used for dynamically generated ops.

    Size - Supplies the size of the buffer in bytes.

Return Value:

    Returns a pointer to a string containing the name of the op. This may
    either be a pointer to a static memory location, or a pointer to the
    buffer passed in.

--*/

{

    if (Op <= DwarfOpLit0) {
        return DwarfOpNames[Op];

    } else if (Op <= DwarfOpLit31) {
        snprintf(Buffer, Size, "DwarfOpLit%d", Op - DwarfOpLit0);
        return Buffer;

    } else if (Op <= DwarfOpReg31) {
        snprintf(Buffer, Size, "DwarfReg%d", Op - DwarfOpReg0);
        return Buffer;

    } else if (Op <= DwarfOpBreg31) {
        snprintf(Buffer, Size, "DwarfBreg%d", Op - DwarfOpBreg0);
        return Buffer;

    } else if (Op <= DwarfOpStackValue) {

        assert(Op >= 0x90);

        return DwarfOp90Names[Op - 0x90];
    }

    return "DwarfOpUNKNOWN";
}

VOID
DwarfpExpressionPush (
    PDWARF_LOCATION_CONTEXT LocationContext,
    ULONGLONG Value
    )

/*++

Routine Description:

    This routine pushes a value onto the DWARF expression stack.

Arguments:

    LocationContext - Supplies a pointer to the execution context.

    Value - Supplies the value to push.

Return Value:

    None.

--*/

{

    ULONG StackSize;

    StackSize = LocationContext->StackSize;
    if (StackSize < DWARF_EXPRESSION_STACK_SIZE) {
        LocationContext->Stack[StackSize] = Value;
        LocationContext->StackSize += 1;

    } else {

        assert(FALSE);

    }

    return;
}

ULONGLONG
DwarfpExpressionPop (
    PDWARF_LOCATION_CONTEXT LocationContext
    )

/*++

Routine Description:

    This routine pops a value off of the DWARF expression stack.

Arguments:

    LocationContext - Supplies a pointer to the execution context.

Return Value:

    Returns the popped value, or 0 if there were not values on the stack.

--*/

{

    ULONG StackSize;
    ULONGLONG Value;

    StackSize = LocationContext->StackSize;
    if (StackSize == 0) {

        assert(FALSE);

        return 0;
    }

    Value = LocationContext->Stack[StackSize - 1];
    LocationContext->StackSize -= 1;
    return Value;
}

INT
DwarfpGetFrameBase (
    PDWARF_CONTEXT Context,
    PFUNCTION_SYMBOL Function,
    ULONGLONG Pc,
    PULONGLONG FrameBaseValue
    )

/*++

Routine Description:

    This routine returns the current frame base register value. This usually
    resolves to something like "esp+x".

Arguments:

    Context - Supplies a pointer to the DWARF context.

    Function - Supplies an optional pointer to the current function containing
        the location to evaluate.

    Pc - Supplies the current value of the instruction pointer.

    FrameBaseValue - Supplies a pointer where the frame base register value
        will be returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PDWARF_FUNCTION_SYMBOL DwarfFunction;
    DWARF_LOCATION_CONTEXT LocationContext;
    INT Status;

    //
    // Just return a zero frame register if there's no current function or no
    // frame base attribute within that function.
    //

    if (Function == NULL) {
        *FrameBaseValue = 0;
        return 0;
    }

    DwarfFunction = Function->SymbolContext;
    if ((DwarfFunction == NULL) ||
        (DwarfFunction->FrameBase.Name != DwarfAtFrameBase)) {

        *FrameBaseValue = 0;
        return 0;
    }

    //
    // Evaluate the frame base location.
    //

    memset(&LocationContext, 0, sizeof(DWARF_LOCATION_CONTEXT));
    LocationContext.Unit = DwarfFunction->Unit;
    LocationContext.Pc = Pc;
    Status = DwarfpGetLocation(Context,
                               &LocationContext,
                               &(DwarfFunction->FrameBase));

    if (Status == 0) {
        if ((LocationContext.Location.Form != DwarfLocationMemory) ||
            (LocationContext.Location.NextPiece != NULL)) {

            assert(FALSE);

            return EINVAL;
        }

        *FrameBaseValue = LocationContext.Location.Value.Address;
    }

    DwarfpDestroyLocationContext(Context, &LocationContext);
    return Status;
}

