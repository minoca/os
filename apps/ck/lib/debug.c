/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    debug.c

Abstract:

    This module implements debug support in Chalk.

Author:

    Evan Green 22-Jun-2016

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
#include "debug.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
CkpDumpValue (
    PCK_VM Vm,
    CK_VALUE Value
    );

VOID
CkpDumpObject (
    PCK_VM Vm,
    PCK_OBJECT Object
    );

//
// -------------------------------------------------------------------- Globals
//

PSTR CkOpcodeNames[CkOpcodeCount] = {
    "Nop",
    "Constant",
    "String",
    "Null",
    "Literal0",
    "Literal1",
    "Literal2",
    "Literal3",
    "Literal4",
    "Literal5",
    "Literal6",
    "Literal7",
    "Literal8",
    "LoadLocal0",
    "LoadLocal1",
    "LoadLocal2",
    "LoadLocal3",
    "LoadLocal4",
    "LoadLocal5",
    "LoadLocal6",
    "LoadLocal7",
    "LoadLocal8",
    "LoadLocal",
    "StoreLocal",
    "LoadUpvalue",
    "StoreUpvalue",
    "LoadModuleVariable",
    "StoreModuleVariable",
    "LoadFieldThis",
    "StoreFieldThis",
    "LoadField",
    "StoreField",
    "Pop",
    "Call0",
    "Call1",
    "Call2",
    "Call3",
    "Call4",
    "Call5",
    "Call6",
    "Call7",
    "Call8",
    "Call",
    "IndirectCall",
    "SuperCall0",
    "SuperCall1",
    "SuperCall2",
    "SuperCall3",
    "SuperCall4",
    "SuperCall5",
    "SuperCall6",
    "SuperCall7",
    "SuperCall8",
    "SuperCall",
    "Jump",
    "Loop",
    "JumpIf",
    "And",
    "Or",
    "CloseUpvalue",
    "Return",
    "Closure",
    "Class",
    "Method",
    "StaticMethod",
    "Try",
    "PopTry",
    "End"
};

PSTR CkObjectTypeNames[CkObjectTypeCount] = {
    "Invalid",
    "Class",
    "Closure",
    "Dict",
    "Fiber",
    "Foreign",
    "Function",
    "Instance",
    "List",
    "Module",
    "Range",
    "String",
    "Upvalue"
};

//
// ------------------------------------------------------------------ Functions
//

CK_VALUE
CkpCreateStackTrace (
    PCK_VM Vm,
    UINTN Skim
    )

/*++

Routine Description:

    This routine creates a stack trace object from the current fiber.

Arguments:

    Vm - Supplies a pointer to the VM.

    Skim - Supplies the number of most recently called functions not to include
        in the stack trace. This is usually 0 for exceptions created in C and
        1 for exceptions created in Chalk.

Return Value:

    Returns a list of lists containing the stack trace. The first element is
    the least recently called. Each elements contains a list of 3 elements:
    the module name, the function name, and the line number.

    CK_NULL_VALUE on allocation failure.

--*/

{

    PCK_CLOSURE Closure;
    PCK_FIBER Fiber;
    PCK_CALL_FRAME Frame;
    PCK_LIST FrameElement;
    INTN FrameIndex;
    PCK_FUNCTION Function;
    LONG Line;
    PCK_MODULE Module;
    PCK_STRING Name;
    PCK_LIST Stack;
    CK_VALUE Value;

    Fiber = Vm->Fiber;
    Stack = CkpListCreate(Vm, 0);
    if (Stack == NULL) {
        return CkNullValue;
    }

    if (Fiber == NULL) {
        CK_OBJECT_VALUE(Value, Stack);
        return Value;
    }

    CK_ASSERT(Fiber->FrameCount >= Skim);

    CkpPushRoot(Vm, &(Stack->Header));
    for (FrameIndex = Fiber->FrameCount - 1 - Skim;
         FrameIndex >= 0;
         FrameIndex -= 1) {

        Frame = &(Fiber->Frames[FrameIndex]);
        Closure = Frame->Closure;
        Line = 0;
        switch (Closure->Type) {
        case CkClosureBlock:
            Function = Closure->U.Block.Function;
            Module = Function->Module;
            Line = CkpGetLineForOffset(Closure->U.Block.Function,
                                       Frame->Ip - Function->Code.Data - 1);

            break;

        case CkClosurePrimitive:
            Module = CkpModuleGet(Vm, CK_NULL_VALUE);
            break;

        case CkClosureForeign:
            Module = Closure->U.Foreign.Module;
            break;

        default:

            CK_ASSERT(FALSE);

            Module = NULL;
            break;
        }

        FrameElement = CkpListCreate(Vm, 4);
        if (FrameElement == NULL) {
            CkpPopRoot(Vm);
            return CkNullValue;
        }

        CK_OBJECT_VALUE(FrameElement->Elements.Data[0], Module->Name);
        if (Module->Path != NULL) {
            CK_OBJECT_VALUE(FrameElement->Elements.Data[1], Module->Path);

        } else {
            FrameElement->Elements.Data[1] = CkNullValue;
        }

        Name = CkpGetFunctionName(Closure);

        CK_ASSERT(Name != NULL);

        CK_OBJECT_VALUE(FrameElement->Elements.Data[2], Name);
        CK_INT_VALUE(FrameElement->Elements.Data[3], Line);
        CK_OBJECT_VALUE(Value, FrameElement);
        CkpListInsert(Vm, Stack, Value, Stack->Elements.Count);
    }

    CkpPopRoot(Vm);
    CK_OBJECT_VALUE(Value, Stack);
    return Value;
}

VOID
CkpDumpCode (
    PCK_VM Vm,
    PCK_FUNCTION Function
    )

/*++

Routine Description:

    This routine prints the bytecode assembly for the given function.

Arguments:

    Vm - Supplies a pointer to the VM.

    Function - Supplies a pointer to the function containing the bytecode.

Return Value:

    None.

--*/

{

    LONG LastLine;
    PCSTR Name;
    UINTN Offset;
    INTN Size;

    Name = Function->Module->Name->Value;
    CkpDebugPrint(Vm, "%s: %s\n", Name, Function->Debug.Name->Value);
    Offset = 0;
    LastLine = -1;
    while (TRUE) {
        Size = CkpDumpInstruction(Vm, Function, Offset, &LastLine);
        if (Size == -1) {
            break;
        }

        Offset += Size;
    }

    CkpDebugPrint(Vm, "\n");
    return;
}

VOID
CkpDumpStack (
    PCK_VM Vm,
    PCK_FIBER Fiber
    )

/*++

Routine Description:

    This routine prints the current contents of the stack for the most recent
    call frame.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Fiber - Supplies a pointer to the fiber.

Return Value:

    None.

--*/

{

    PCK_VALUE Base;
    PCK_VALUE Current;
    PCK_CALL_FRAME Frame;

    if (Fiber->FrameCount == 0) {
        CkpDebugPrint(Vm, "Not running\n");
        return;
    }

    Frame = &(Fiber->Frames[Fiber->FrameCount - 1]);
    Base = Frame->StackStart;
    Current = Fiber->StackTop - 1;
    while (Current >= Base) {
        CkpDebugPrint(Vm, "%2d ", Current - Base);
        CkpDumpValue(Vm, *Current);
        CkpDebugPrint(Vm, "\n");
        Current -= 1;
    }

    CkpDebugPrint(Vm, "========\n");
    return;
}

INTN
CkpDumpInstruction (
    PCK_VM Vm,
    PCK_FUNCTION Function,
    UINTN Offset,
    PLONG LastLine
    )

/*++

Routine Description:

    This routine prints the bytecode for a single instruction.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Function - Supplies a pointer to the function.

    Offset - Supplies the offset into the function code to print from.

    LastLine - Supplies an optional pointer where the last line number printed
        is given on input. On output, returns the line number of this
        instruction.

Return Value:

    Returns the length of this instruction.

    -1 if there are no more instructions.

--*/

{

    PUCHAR ByteCode;
    CK_SYMBOL_INDEX Capture;
    CK_SYMBOL_INDEX Constant;
    BOOL IsLocal;
    INTN Jump;
    LONG Line;
    PCK_FUNCTION LoadedFunction;
    PSTR LocalType;
    CK_OPCODE Op;
    UINTN Start;
    PCK_STRING StringObject;
    CK_SYMBOL_INDEX Symbol;

    Start = Offset;
    ByteCode = Function->Code.Data;
    Op = ByteCode[Offset];
    Line = CkpGetLineForOffset(Function, Offset);
    CkpDebugPrint(Vm, "%4x ", Offset);
    if ((LastLine == NULL) || (*LastLine != Line)) {
        CkpDebugPrint(Vm, "%4d: ", Line);
        if (LastLine != NULL) {
            *LastLine = Line;
        }

    } else {
        CkpDebugPrint(Vm, "      ");
    }

    if (Op >= CkOpcodeCount) {
        CkpDebugPrint(Vm, "Unknown %d", Op);

    } else {
        CkpDebugPrint(Vm, "%s ", CkOpcodeNames[Op]);
    }

    Offset += 1;
    switch (Op) {
    case CkOpConstant:
        Constant = CK_READ16(ByteCode + Offset);
        Offset += 2;

        CK_ASSERT(Constant < Function->Constants.Count);

        CkpDumpValue(Vm, Function->Constants.Data[Constant]);
        break;

    case CkOpStringConstant:
        Constant = CK_READ16(ByteCode + Offset);
        Offset += 2;

        CK_ASSERT(Constant < Function->Module->Strings.List.Count);

        CkpDumpValue(Vm, Function->Module->Strings.List.Data[Constant]);
        break;

    case CkOpLoadModuleVariable:
    case CkOpStoreModuleVariable:
        Symbol = CK_READ16(ByteCode + Offset);
        Offset += 2;

        CK_ASSERT(Symbol < Function->Module->VariableNames.List.Count);

        StringObject =
               CK_AS_STRING(Function->Module->VariableNames.List.Data[Symbol]);

        CkpDebugPrint(Vm, "%s", StringObject->Value);
        break;

    case CkOpCall:
    case CkOpSuperCall:
        Symbol = CK_READ8(ByteCode + Offset);
        Offset += 1;
        CkpDebugPrint(Vm, "%d ", Symbol);

    //
    // Fall through
    //

    case CkOpCall0:
    case CkOpCall1:
    case CkOpCall2:
    case CkOpCall3:
    case CkOpCall4:
    case CkOpCall5:
    case CkOpCall6:
    case CkOpCall7:
    case CkOpCall8:
    case CkOpSuperCall0:
    case CkOpSuperCall1:
    case CkOpSuperCall2:
    case CkOpSuperCall3:
    case CkOpSuperCall4:
    case CkOpSuperCall5:
    case CkOpSuperCall6:
    case CkOpSuperCall7:
    case CkOpSuperCall8:
    case CkOpMethod:
    case CkOpStaticMethod:
        Symbol = CK_READ16(ByteCode + Offset);
        Offset += 2;

        CK_ASSERT(Symbol < Function->Module->Strings.List.Count);

        StringObject =
                     CK_AS_STRING(Function->Module->Strings.List.Data[Symbol]);

        CkpDebugPrint(Vm, "%s", StringObject->Value);
        break;

    case CkOpIndirectCall:
    case CkOpLoadLocal:
    case CkOpStoreLocal:
    case CkOpLoadUpvalue:
    case CkOpStoreUpvalue:
    case CkOpLoadFieldThis:
    case CkOpStoreFieldThis:
    case CkOpLoadField:
    case CkOpStoreField:
        Constant = CK_READ8(ByteCode + Offset);
        Offset += 1;
        CkpDebugPrint(Vm, "%d", Constant);
        break;

    case CkOpJump:
    case CkOpJumpIf:
    case CkOpAnd:
    case CkOpOr:
    case CkOpTry:
        Jump = CK_READ16(ByteCode + Offset);
        Offset += 2;
        CkpDebugPrint(Vm, "%x", Offset + Jump);
        break;

    case CkOpLoop:
        Jump = CK_READ16(ByteCode + Offset);
        Offset += 2;
        CkpDebugPrint(Vm, "%x", Offset - Jump);
        break;

    case CkOpClosure:
        Constant = CK_READ16(ByteCode + Offset);
        Offset += 2;

        CK_ASSERT(Constant < Function->Constants.Count);

        LoadedFunction = CK_AS_FUNCTION(Function->Constants.Data[Constant]);
        CkpDumpValue(Vm, Function->Constants.Data[Constant]);
        CkpDebugPrint(Vm, " ");
        for (Capture = 0;
             Capture < LoadedFunction->UpvalueCount;
             Capture += 1) {

            IsLocal = CK_READ8(ByteCode + Offset);
            Offset += 1;
            Symbol = CK_READ8(ByteCode + Offset);
            Offset += 1;
            if (Capture > 0) {
                CkpDebugPrint(Vm, ", ");
            }

            LocalType = "upvalue";
            if (IsLocal != FALSE) {
                LocalType = "local";
            }

            CkpDebugPrint(Vm, "%s %d", LocalType, Symbol);
        }

        break;

    case CkOpClass:
        Constant = CK_READ8(ByteCode + Offset);
        Offset += 1;
        CkpDebugPrint(Vm, "%d fields", Constant);
        break;

    default:
        break;
    }

    CkpDebugPrint(Vm, "\n");
    if (Op == CkOpEnd) {
        return -1;
    }

    return Offset - Start;
}

INT
CkpGetLineForOffset (
    PCK_FUNCTION Function,
    UINTN CodeOffset
    )

/*++

Routine Description:

    This routine determines what line the given bytecode offset is on.

Arguments:

    Function - Supplies a pointer to the function containing the bytecode.

    CodeOffset - Supplies the offset whose line number is desired.

Return Value:

    Returns the line number the offset in question.

    -1 if no line number information could be found.

--*/

{

    PUCHAR End;
    LONG Line;
    PUCHAR LineProgram;
    UINTN Offset;
    CK_LINE_OP Op;

    LineProgram = Function->Debug.LineProgram.Data;
    End = LineProgram + Function->Debug.LineProgram.Count;
    Offset = 0;
    Line = Function->Debug.FirstLine;
    while (LineProgram < End) {
        Op = *LineProgram;
        LineProgram += 1;
        switch (Op) {
        case CkLineOpNop:
            break;

        case CkLineOpSetLine:
            CkCopy(&Line, LineProgram, sizeof(ULONG));
            LineProgram += sizeof(ULONG);
            break;

        case CkLineOpSetOffset:
            CkCopy(&Offset, LineProgram, sizeof(ULONG));
            LineProgram += sizeof(ULONG);
            break;

        case CkLineOpAdvanceLine:
            Line += CkpUtf8Decode(LineProgram, End - LineProgram);
            LineProgram += CkpUtf8DecodeSize(*LineProgram);
            break;

        case CkLineOpAdvanceOffset:
            Offset += CkpUtf8Decode(LineProgram, End - LineProgram);
            LineProgram += CkpUtf8DecodeSize(*LineProgram);
            break;

        case CkLineOpSpecial:
        default:
            Line += CK_LINE_ADVANCE(Op);
            Offset += CK_OFFSET_ADVANCE(Op);
            break;
        }

        if (Offset >= CodeOffset) {
            return Line;
        }
    }

    CK_ASSERT(FALSE);

    return -1;
}

VOID
CkpDebugPrint (
    PCK_VM Vm,
    PSTR Message,
    ...
    )

/*++

Routine Description:

    This routine prints something to the output for the debug code.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Message - Supplies the printf-style format message to print.

    ... - Supplies the remainder of the arguments.

Return Value:

    None.

--*/

{

    va_list ArgumentList;
    CHAR Buffer[CK_MAX_ERROR_MESSAGE];

    va_start(ArgumentList, Message);
    vsnprintf(Buffer, sizeof(Buffer), Message, ArgumentList);
    va_end(ArgumentList);
    Buffer[sizeof(Buffer) - 1] = '\0';
    if (Vm->Configuration.Write != NULL) {
        Vm->Configuration.Write(Vm, Buffer);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
CkpDumpValue (
    PCK_VM Vm,
    CK_VALUE Value
    )

/*++

Routine Description:

    This routine prints the given value.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Value - Supplies the value to print.

Return Value:

    None.

--*/

{

    switch (Value.Type) {
    case CkValueNull:
        CkpDebugPrint(Vm, "null");
        break;

    case CkValueInteger:
        CkpDebugPrint(Vm, "%lld", CK_AS_INTEGER(Value));
        break;

    case CkValueObject:
        CkpDumpObject(Vm, CK_AS_OBJECT(Value));
        break;

    default:

        CK_ASSERT(FALSE);

        CkpDebugPrint(Vm, "<invalid object>");
        break;
    }

    return;
}

VOID
CkpDumpObject (
    PCK_VM Vm,
    PCK_OBJECT Object
    )

/*++

Routine Description:

    This routine prints the given object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Object - Supplies a pointer to the object to print.

Return Value:

    None.

--*/

{

    PCK_CLASS Class;
    PCK_MODULE Module;
    PCK_RANGE Range;
    PCSTR Separator;

    switch (Object->Type) {
    case CkObjectRange:
        Range = (PCK_RANGE)Object;
        Separator = "..";
        if (Range->Inclusive != FALSE) {
            Separator = "...";
        }

        CkpDebugPrint(Vm,
                      "%lld%s%lld",
                      (LONGLONG)(Range->From),
                      Separator,
                      (LONGLONG)(Range->To));

        break;

    case CkObjectString:
        CkpDebugPrint(Vm, "\"%s\"", ((PCK_STRING)Object)->Value);
        break;

    case CkObjectClass:
        Class = (PCK_CLASS)Object;
        CkpDebugPrint(Vm, "Class(");
        CkpDumpObject(Vm, &(Class->Name->Header));
        CkpDebugPrint(Vm, ")");
        break;

    case CkObjectModule:
        Module = (PCK_MODULE)Object;
        CkpDebugPrint(Vm, "<module ");
        CkpDumpObject(Vm, &(Module->Name->Header));
        if (Module->Path != NULL) {
            CkpDebugPrint(Vm, " at ");
            CkpDumpObject(Vm, &(Module->Path->Header));
        }

        CkpDebugPrint(Vm, ">");
        break;

    default:
        if (Object->Type == CkObjectInstance) {
            CkpDebugPrint(Vm,
                          "<%s %p>",
                          Object->Class->Name->Value,
                          Object);

        } else if (Object->Type < CkObjectTypeCount) {
            CkpDebugPrint(Vm,
                          "<%s %p>",
                          CkObjectTypeNames[Object->Type],
                          Object);

        } else {
            CkpDebugPrint(Vm, "<unknown %d %p>", Object->Type, Object);
        }

        break;
    }

    return;
}
