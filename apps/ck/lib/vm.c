/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    vm.c

Abstract:

    This module implements support for the Chalk virtual machine. This
    stack-based VM implementation is heavily inspired by Wren, a beautifully
    implemented scripting language written by Bob Nystrom.

Author:

    Evan Green 28-May-2016

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "chalkp.h"
#include "debug.h"
#include "compiler.h"
#include "vmsys.h"

//
// --------------------------------------------------------------------- Macros
//

//
// Define CKI_* macros that only apply inside the run interpreter function.
//

//
// These macros manipulate the interpreter stack.
//

#define CKI_PUSH(_Value) CK_PUSH(Fiber, _Value)
#define CKI_POP() CK_POP(Fiber)
#define CKI_DROP() Fiber->StackTop -= 1
#define CKI_STACK_TOP() *(Fiber->StackTop - 1)
#define CKI_STACK_TOP2() *(Fiber->StackTop - 2)

//
// These macros read from and advance the instruction stream.
//

#define CKI_READ_BYTE(_Value) (_Value) = *Ip; Ip += 1
#define CKI_READ_SHORT(_Value) (_Value) = CK_READ16(Ip), Ip += 2

#define CKI_READ_LOCAL(_Value) CKI_READ_BYTE(_Value)
#define CKI_READ_FIELD(_Value) CKI_READ_BYTE(_Value)
#define CKI_READ_ARITY(_Value) CKI_READ_BYTE(_Value)
#define CKI_READ_SYMBOL(_Value) CKI_READ_SHORT(_Value)
#define CKI_READ_OFFSET(_Value) CKI_READ_SHORT(_Value)

//
// These macros sync up the pieces of the VM state that are kept in local
// variables. Keeping a few things in locals allows the compiler to relax a
// little and make faster code.
//

#define CKI_STORE_FRAME() Frame->Ip = Ip;
#define CKI_LOAD_FRAME()                                \
    Frame = &(Fiber->Frames[Fiber->FrameCount - 1]);    \
    Stack = Frame->StackStart;                          \
    Ip = Frame->Ip;                                     \
    Function = Frame->Closure->U.Block.Function

//
// This macro reloads the fiber local variable in cases where a fiber context
// switch may have occured. This is basically any place where an exception
// could have been taken. Because the fiber may have changed, the frame is
// reloaded too.
//

#define CKI_LOAD_FIBER()                                \
    Fiber = Vm->Fiber;                                  \
    if ((Fiber == NULL) || (Fiber->FrameCount == 0)) {  \
        goto RunInterpreterEnd;                         \
    }                                                   \
                                                        \
    CKI_LOAD_FRAME()                                    \

//
// This macro prints the instruction and the stack. It can be used when
// debugging the interpreter.
//

#if 0

#define CKI_DEBUG_DUMP()                                                    \
    {                                                                       \
                                                                            \
        CkpDumpStack(Vm, Fiber);                                            \
        CkpDumpInstruction(Vm, Function, Ip - Function->Code.Data, NULL);   \
    }

#else

#define CKI_DEBUG_DUMP()

#endif

#if 1

//
// This macro creates the goto label for a given opcode.
//

#define CKI_OPCODE_LABEL(_Op) Handle##_Op

//
// This macro defines an entry in the computed goto table. Offsets are
// friendlier than addresses in position independent code because they generate
// fewer relocations.
//

#define CKI_GOTO_OFFSET(_Op) &&CKI_OPCODE_LABEL(_Op) - &&RunInterpreterLoop

//
// This macro creates the goto table for each opcode. Computed gotos are a
// non-standard C extension supported by most but not all compilers. The &&
// operator is used to get the address of a label.
//

#define CKI_DEFINE_GOTO_TABLE() \
    static const int DispatchTable[CkOpcodeCount] = { \
        CKI_GOTO_OFFSET(CkOpNop), \
        CKI_GOTO_OFFSET(CkOpConstant), \
        CKI_GOTO_OFFSET(CkOpStringConstant), \
        CKI_GOTO_OFFSET(CkOpNull), \
        CKI_GOTO_OFFSET(CkOpLiteral0), \
        CKI_GOTO_OFFSET(CkOpLiteral1), \
        CKI_GOTO_OFFSET(CkOpLiteral2), \
        CKI_GOTO_OFFSET(CkOpLiteral3), \
        CKI_GOTO_OFFSET(CkOpLiteral4), \
        CKI_GOTO_OFFSET(CkOpLiteral5), \
        CKI_GOTO_OFFSET(CkOpLiteral6), \
        CKI_GOTO_OFFSET(CkOpLiteral7), \
        CKI_GOTO_OFFSET(CkOpLiteral8), \
        CKI_GOTO_OFFSET(CkOpLoadLocal0), \
        CKI_GOTO_OFFSET(CkOpLoadLocal1), \
        CKI_GOTO_OFFSET(CkOpLoadLocal2), \
        CKI_GOTO_OFFSET(CkOpLoadLocal3), \
        CKI_GOTO_OFFSET(CkOpLoadLocal4), \
        CKI_GOTO_OFFSET(CkOpLoadLocal5), \
        CKI_GOTO_OFFSET(CkOpLoadLocal6), \
        CKI_GOTO_OFFSET(CkOpLoadLocal7), \
        CKI_GOTO_OFFSET(CkOpLoadLocal8), \
        CKI_GOTO_OFFSET(CkOpLoadLocal), \
        CKI_GOTO_OFFSET(CkOpStoreLocal), \
        CKI_GOTO_OFFSET(CkOpLoadUpvalue), \
        CKI_GOTO_OFFSET(CkOpStoreUpvalue), \
        CKI_GOTO_OFFSET(CkOpLoadModuleVariable), \
        CKI_GOTO_OFFSET(CkOpStoreModuleVariable), \
        CKI_GOTO_OFFSET(CkOpLoadFieldThis), \
        CKI_GOTO_OFFSET(CkOpStoreFieldThis), \
        CKI_GOTO_OFFSET(CkOpLoadField), \
        CKI_GOTO_OFFSET(CkOpStoreField), \
        CKI_GOTO_OFFSET(CkOpPop), \
        CKI_GOTO_OFFSET(CkOpCall0), \
        CKI_GOTO_OFFSET(CkOpCall1), \
        CKI_GOTO_OFFSET(CkOpCall2), \
        CKI_GOTO_OFFSET(CkOpCall3), \
        CKI_GOTO_OFFSET(CkOpCall4), \
        CKI_GOTO_OFFSET(CkOpCall5), \
        CKI_GOTO_OFFSET(CkOpCall6), \
        CKI_GOTO_OFFSET(CkOpCall7), \
        CKI_GOTO_OFFSET(CkOpCall8), \
        CKI_GOTO_OFFSET(CkOpCall), \
        CKI_GOTO_OFFSET(CkOpIndirectCall), \
        CKI_GOTO_OFFSET(CkOpSuperCall0), \
        CKI_GOTO_OFFSET(CkOpSuperCall1), \
        CKI_GOTO_OFFSET(CkOpSuperCall2), \
        CKI_GOTO_OFFSET(CkOpSuperCall3), \
        CKI_GOTO_OFFSET(CkOpSuperCall4), \
        CKI_GOTO_OFFSET(CkOpSuperCall5), \
        CKI_GOTO_OFFSET(CkOpSuperCall6), \
        CKI_GOTO_OFFSET(CkOpSuperCall7), \
        CKI_GOTO_OFFSET(CkOpSuperCall8), \
        CKI_GOTO_OFFSET(CkOpSuperCall), \
        CKI_GOTO_OFFSET(CkOpJump), \
        CKI_GOTO_OFFSET(CkOpLoop), \
        CKI_GOTO_OFFSET(CkOpJumpIf), \
        CKI_GOTO_OFFSET(CkOpAnd), \
        CKI_GOTO_OFFSET(CkOpOr), \
        CKI_GOTO_OFFSET(CkOpCloseUpvalue), \
        CKI_GOTO_OFFSET(CkOpReturn), \
        CKI_GOTO_OFFSET(CkOpClosure), \
        CKI_GOTO_OFFSET(CkOpClass), \
        CKI_GOTO_OFFSET(CkOpMethod), \
        CKI_GOTO_OFFSET(CkOpStaticMethod), \
        CKI_GOTO_OFFSET(CkOpTry), \
        CKI_GOTO_OFFSET(CkOpPopTry), \
        CKI_GOTO_OFFSET(CkOpEnd), \
    };

//
// This macro emits the main loop code for the interpreter. This macro uses
// the non-standard computed gotos, but could be changed pretty easily to a
// while/switch statement if strict C is needed. The extension is good for
// performance because 1) The compiler isn't forced to emit bounds checking
// like it is with a switch statement, and 2) the branches are spread out
// across each opcode rather than being combined into the single branch at the
// switch, allowing hardware branch predictors to do better if there are
// patterns in the instruction sequences.
//

#define CKI_INTERPRETER_LOOP RunInterpreterLoop: CKI_DISPATCH()

//
// This macro jumps to the next instruction. For computed gotos, it just jumps
// to the next case. For switches, this would be a break (or probably more
// safely a goto to allow the macro to be used within loops).
//

#define CKI_DISPATCH()                                              \
    {                                                               \
                                                                    \
        CKI_DEBUG_DUMP();                                           \
        CKI_READ_BYTE(Instruction);                                 \
        goto *(&&RunInterpreterLoop + DispatchTable[Instruction]);  \
    }

//
// This macro defines the case/label for implementing an opcode.
//

#define CKI_CASE(_Op) CKI_OPCODE_LABEL(_Op)

//
// This macro evaluates the default case. For a computed goto, it is not used.
//

#define CKI_DEFAULT_CASE()

#else

//
// Define the macros to implement a simple switch loop, since computed gotos
// are unavailable.
//

#define CKI_DEFINE_GOTO_TABLE()
#define CKI_INTERPRETER_LOOP    \
    CKI_DISPATCH();             \
    RunInterpreterLoop:         \
    switch (Instruction)        \

#define CKI_DISPATCH()                                              \
    {                                                               \
                                                                    \
        CKI_DEBUG_DUMP();                                           \
        CKI_READ_BYTE(Instruction);                                 \
        goto RunInterpreterLoop;                                    \
    }

#define CKI_CASE(_Op) case _Op
#define CKI_DEFAULT_CASE() default:

#endif

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PCK_UPVALUE
CkpCaptureUpvalue (
    PCK_VM Vm,
    PCK_FIBER Fiber,
    PCK_VALUE Local
    );

VOID
CkpCloseUpvalues (
    PCK_FIBER Fiber,
    PCK_VALUE Last
    );

BOOL
CkpValidateSuperclass (
    PCK_VM Vm,
    CK_VALUE Name,
    CK_VALUE Superclass,
    CK_SYMBOL_INDEX FieldCount
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

CK_API
VOID
CkInitializeConfiguration (
    PCK_CONFIGURATION Configuration
    )

/*++

Routine Description:

    This routine initializes a Chalk configuration with its default values.

Arguments:

    Configuration - Supplies a pointer where the initialized configuration will
        be returned.

Return Value:

    None.

--*/

{

    CkCopy(Configuration, &CkDefaultConfiguration, sizeof(CK_CONFIGURATION));
    return;
}

CK_API
PCK_VM
CkCreateVm (
    PCK_CONFIGURATION Configuration
    )

/*++

Routine Description:

    This routine creates a new Chalk virtual machine context. Each VM context
    is entirely independent.

Arguments:

    Configuration - Supplies an optional pointer to the configuration to use
        for this instance. If NULL, a default configuration will be provided.

Return Value:

    Returns a pointer to the new VM on success.

    NULL on allocation or if an invalid configuration was supplied.

--*/

{

    PCK_CONFIGURATION Default;
    PCK_REALLOCATE Reallocate;
    CK_ERROR_TYPE Status;
    PCK_VM Vm;

    Default = &CkDefaultConfiguration;
    Reallocate = Default->Reallocate;
    if (Configuration != NULL) {
        if (Configuration->Reallocate != NULL) {
            Reallocate = Configuration->Reallocate;
        }
    }

    Vm = Reallocate(NULL, sizeof(CK_VM));
    if (Vm == NULL) {
        return NULL;
    }

    CkZero(Vm, sizeof(CK_VM));
    if (Configuration == NULL) {
        CkCopy(&(Vm->Configuration), Default, sizeof(CK_CONFIGURATION));

    } else {
        CkCopy(&(Vm->Configuration), Configuration, sizeof(CK_CONFIGURATION));
        Vm->Configuration.Reallocate = Reallocate;
    }

    Vm->NextGarbageCollection = Vm->Configuration.InitialHeapSize;
    Vm->Modules = CkpDictCreate(Vm);
    if (Vm->Modules == NULL) {
        Status = CkErrorNoMemory;
        goto CreateVmEnd;
    }

    Status = CkpInitializeCore(Vm);
    if (Status != CkSuccess) {
        goto CreateVmEnd;
    }

    Status = CkSuccess;

CreateVmEnd:
    if (Status != CkSuccess) {
        if (Vm != NULL) {
            CkDestroyVm(Vm);
            Vm = NULL;
        }
    }

    return Vm;
}

CK_API
VOID
CkDestroyVm (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine destroys a Chalk virtual machine.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCK_OBJECT Next;
    PCK_OBJECT Object;
    PCK_REALLOCATE Reallocate;

    //
    // Notice double frees of the VM.
    //

    CK_ASSERT(Vm->Configuration.Reallocate != NULL);

    Object = Vm->FirstObject;
    while (Object != NULL) {
        Next = Object->Next;
        CkpDestroyObject(Vm, Object);
        Object = Next;
    }

    Vm->FirstObject = NULL;

    //
    // Null out the reallocate function to catch double frees.
    //

    Reallocate = Vm->Configuration.Reallocate;
    Vm->Configuration.Reallocate = NULL;
    Reallocate(Vm, 0);
    return;
}

CK_API
CK_ERROR_TYPE
CkInterpret (
    PCK_VM Vm,
    PCSTR Path,
    PCSTR Source,
    UINTN Length,
    LONG Line,
    BOOL Interactive
    )

/*++

Routine Description:

    This routine interprets the given Chalk source string within the context of
    the "main" module.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Path - Supplies an optional pointer to the path of the file containing the
        source being interpreted.

    Source - Supplies a pointer to the null terminated string containing the
        source to interpret.

    Length - Supplies the length of the source string, not including the null
        terminator.

    Line - Supplies the line number this code starts on. Supply 1 to start at
        the beginning.

    Interactive - Supplies a boolean indicating whether this is an interactive
        session or not. For interactive sessions, expression statements will be
        printed.

Return Value:

    Chalk status.

--*/

{

    ULONG Flags;
    CK_ERROR_TYPE Result;

    Flags = 0;
    if (Interactive != FALSE) {
        Flags |= CK_COMPILE_PRINT_EXPRESSIONS;
    }

    Result = CkpInterpret(Vm, "__main", Path, Source, Length, Line, Flags);
    return Result;
}

CK_SYMBOL_INDEX
CkpDeclareModuleVariable (
    PCK_VM Vm,
    PCK_MODULE Module,
    PSTR Name,
    UINTN Length,
    INT Line
    )

/*++

Routine Description:

    This routine creates an undefined module-level variable (essentially a
    forward declaration). These are patched up before compilation is finished.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Module - Supplies a pointer to the module that owns the variable.

    Name - Supplies a pointer to the variable name.

    Length - Supplies the length of the variable name, not including the null
        terminator.

    Line - Supplies the line number the forward reference is on.

Return Value:

    Returns the index of the new variable on success.

    -2 on allocation failure.

--*/

{

    CK_ERROR_TYPE Error;
    CK_SYMBOL_INDEX Symbol;
    CK_VALUE Value;

    if (Module->Variables.Count == CK_MAX_MODULE_VARIABLES) {
        return -2;
    }

    Symbol = CkpStringTableAdd(Vm, &(Module->VariableNames), Name, Length);
    if (Symbol == -1) {
        return -2;
    }

    CK_INT_VALUE(Value, Line);
    Error = CkpArrayAppend(Vm, &(Module->Variables), Value);
    if (Error != CkSuccess) {
        return -2;
    }

    return Symbol;
}

CK_SYMBOL_INDEX
CkpDefineModuleVariable (
    PCK_VM Vm,
    PCK_MODULE Module,
    PCSTR Name,
    UINTN Length,
    CK_VALUE Value
    )

/*++

Routine Description:

    This routine creates and defines a module-level variable to the given value.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Module - Supplies a pointer to the module that owns the variable.

    Name - Supplies a pointer to the variable name.

    Length - Supplies the length of the variable name, not including the null
        terminator.

    Value - Supplies the value to assign the variable.

Return Value:

    Returns the index of the new variable on success.

    -1 if the variable is already defined.

    -2 on allocation failure.

--*/

{

    CK_ERROR_TYPE Error;
    CK_SYMBOL_INDEX Symbol;

    if (Module->Variables.Count == CK_MAX_MODULE_VARIABLES) {
        return -2;
    }

    if (CK_IS_OBJECT(Value)) {
        CkpPushRoot(Vm, CK_AS_OBJECT(Value));
    }

    Symbol = CkpStringTableFind(&(Module->VariableNames), Name, Length);

    //
    // Add a brand new symbol.
    //

    if (Symbol == -1) {
        Symbol = CkpStringTableAdd(Vm, &(Module->VariableNames), Name, Length);
        Error = CkpArrayAppend(Vm, &(Module->Variables), Value);
        if (Error != CkSuccess) {
            return -2;
        }

    //
    // If the variable was previously declared, it will have an integer value.
    // Now it can be defined for real.
    //

    } else if (CK_IS_INTEGER(Module->Variables.Data[Symbol])) {
        Module->Variables.Data[Symbol] = Value;

    //
    // Otherwise, the variable has been previously defined.
    //

    } else {
        Symbol = -1;
    }

    if (CK_IS_OBJECT(Value)) {
        CkpPopRoot(Vm);
    }

    return Symbol;
}

PCK_VALUE
CkpFindModuleVariable (
    PCK_VM Vm,
    PCK_MODULE Module,
    PCSTR Name,
    BOOL Create
    )

/*++

Routine Description:

    This routine locates a module level variable with the given name.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Module - Supplies a pointer to the module to search in.

    Name - Supplies a pointer to the name.

    Create - Supplies a boolean indicating if the variable should be defined if
        it did not exist previously.

Return Value:

    Returns a pointer to the variable slot on success.

    NULL if the variable did not exist and the caller did not wish to define it.

    NULL on allocation failure or if the symbol table is full.

--*/

{

    UINTN NameSize;
    CK_SYMBOL_INDEX Symbol;
    CK_VALUE Value;

    NameSize = strlen(Name);
    if (Create != FALSE) {
        Symbol = CkpStringTableEnsure(Vm,
                                      &(Module->VariableNames),
                                      Name,
                                      NameSize);

        if (Symbol >= Module->Variables.Count) {

            CK_ASSERT(Symbol == Module->Variables.Count);

            Value = CkNullValue;
            CkpArrayAppend(Vm, &(Module->Variables), Value);
        }

    } else {
        Symbol = CkpStringTableFind(&(Module->VariableNames), Name, NameSize);
    }

    if (Symbol == -1) {
        return NULL;
    }

    CK_ASSERT(Symbol < Module->Variables.Count);

    return &(Module->Variables.Data[Symbol]);
}

CK_ERROR_TYPE
CkpInterpret (
    PCK_VM Vm,
    PCSTR ModuleName,
    PCSTR ModulePath,
    PCSTR Source,
    UINTN Length,
    LONG Line,
    ULONG CompilerFlags
    )

/*++

Routine Description:

    This routine interprets the given Chalk source string within the context of
    the given module.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ModuleName - Supplies a pointer to the module to interpret the source in.

    ModulePath - Supplies an optional pointer to the module path.

    Source - Supplies a pointer to the null terminated string containing the
        source to interpret.

    Length - Supplies the length of the source string, not including the null
        terminator.

    Line - Supplies the line number this code starts on. Supply 1 to start at
        the beginning.

    CompilerFlags - Supplies the bitfield of compiler flags to pass along.
        See CK_COMPILER_* definitions.

Return Value:

    Chalk status.

--*/

{

    PCK_FIBER Fiber;
    PCK_MODULE Module;
    CK_VALUE NameValue;
    CK_VALUE PathValue;

    NameValue = CkNullValue;
    if (ModuleName != NULL) {
        NameValue = CkpStringFormat(Vm, "$", ModuleName);
        if (!CK_IS_NULL(NameValue)) {
            CkpPushRoot(Vm, CK_AS_OBJECT(NameValue));
        }
    }

    PathValue = CkNullValue;
    if (ModulePath != NULL) {
        PathValue = CkpStringFormat(Vm, "$", ModulePath);
        if (!CK_IS_NULL(PathValue)) {
            CkpPushRoot(Vm, CK_AS_OBJECT(PathValue));
        }
    }

    Module = CkpModuleLoadSource(Vm,
                                 NameValue,
                                 PathValue,
                                 Source,
                                 Length,
                                 Line,
                                 CompilerFlags | CK_COMPILE_PRINT_ERRORS,
                                 NULL);

    if (!CK_IS_NULL(PathValue)) {
        CkpPopRoot(Vm);
    }

    if (!CK_IS_NULL(NameValue)) {
        CkpPopRoot(Vm);
    }

    if (Module == NULL) {
        return CkErrorCompile;
    }

    Fiber = CkpFiberCreate(Vm, Module->Closure);
    if (Fiber == NULL) {
        return CkErrorNoMemory;
    }

    return CkpRunInterpreter(Vm, Fiber);
}

CK_ERROR_TYPE
CkpRunInterpreter (
    PCK_VM Vm,
    PCK_FIBER Fiber
    )

/*++

Routine Description:

    This routine implements the heart of Chalk: the main execution loop.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Fiber - Supplies a pointer to the execution context to run.

Return Value:

    Chalk status.

--*/

{

    PCK_VALUE Arguments;
    CK_ARITY Arity;
    PCK_CLASS Class;
    PCK_CLOSURE Closure;
    UCHAR Field;
    PCK_CALL_FRAME Frame;
    PCK_FUNCTION Function;
    CK_SYMBOL_INDEX Index;
    PCK_INSTANCE Instance;
    CK_OPCODE Instruction;
    PCK_IP Ip;
    BOOL IsLocal;
    UCHAR Local;
    CK_VALUE MethodName;
    PCK_FIBER NextFiber;
    USHORT Offset;
    CK_VALUE Receiver;
    PCK_VALUE Stack;
    CK_SYMBOL_INDEX Symbol;
    PCK_UPVALUE Upvalue;
    CK_VALUE Value;

    CKI_DEFINE_GOTO_TABLE();

    CK_ASSERT((Vm->Fiber == NULL) || (Vm->Fiber->FrameCount == 0) ||
              (Vm->Fiber == Fiber));

    Vm->Fiber = Fiber;
    CKI_LOAD_FRAME();

    //
    // This is the heart of Chalk: the main interpreter loop. Comments are a
    // little more sparse than normal to give the code some room to breathe.
    // For a detailed description of what each opcode does, see the enumeration
    // description of the opcode type.
    //

    CKI_INTERPRETER_LOOP {
    CKI_CASE(CkOpNop):
        CKI_DISPATCH();

    CKI_CASE(CkOpConstant):
        CKI_READ_SYMBOL(Symbol);

        CK_ASSERT(Symbol < Function->Constants.Count);

        CKI_PUSH(Function->Constants.Data[Symbol]);
        CKI_DISPATCH();

    CKI_CASE(CkOpStringConstant):
        CKI_READ_SYMBOL(Symbol);

        CK_ASSERT(Symbol < Function->Module->Strings.List.Count);

        CKI_PUSH(Function->Module->Strings.List.Data[Symbol]);
        CKI_DISPATCH();

    CKI_CASE(CkOpNull):
        CKI_PUSH(CkNullValue);
        CKI_DISPATCH();

    CKI_CASE(CkOpLiteral0):
    CKI_CASE(CkOpLiteral1):
    CKI_CASE(CkOpLiteral2):
    CKI_CASE(CkOpLiteral3):
    CKI_CASE(CkOpLiteral4):
    CKI_CASE(CkOpLiteral5):
    CKI_CASE(CkOpLiteral6):
    CKI_CASE(CkOpLiteral7):
    CKI_CASE(CkOpLiteral8):
        CK_INT_VALUE(Value, Instruction - CkOpLiteral0);
        CKI_PUSH(Value);
        CKI_DISPATCH();

    CKI_CASE(CkOpLoadLocal0):
    CKI_CASE(CkOpLoadLocal1):
    CKI_CASE(CkOpLoadLocal2):
    CKI_CASE(CkOpLoadLocal3):
    CKI_CASE(CkOpLoadLocal4):
    CKI_CASE(CkOpLoadLocal5):
    CKI_CASE(CkOpLoadLocal6):
    CKI_CASE(CkOpLoadLocal7):
    CKI_CASE(CkOpLoadLocal8):

        CK_ASSERT(Instruction - CkOpLoadLocal0 < Function->MaxStack);

        CKI_PUSH(Stack[Instruction - CkOpLoadLocal0]);
        CKI_DISPATCH();

    CKI_CASE(CkOpLoadLocal):
        CKI_READ_LOCAL(Local);

        CK_ASSERT(Local < Function->MaxStack);

        CKI_PUSH(Stack[Local]);
        CKI_DISPATCH();

    CKI_CASE(CkOpStoreLocal):
        CKI_READ_LOCAL(Local);

        CK_ASSERT(Local < Function->MaxStack);

        Stack[Local] = CKI_STACK_TOP();
        CKI_DISPATCH();

    CKI_CASE(CkOpLoadUpvalue):
        CKI_READ_LOCAL(Local);

        CK_ASSERT(Local < Function->UpvalueCount);

        Upvalue = Frame->Closure->Upvalues[Local];
        CKI_PUSH(*(Upvalue->Value));
        CKI_DISPATCH();

    CKI_CASE(CkOpStoreUpvalue):
        CKI_READ_LOCAL(Local);

        CK_ASSERT(Local < Function->UpvalueCount);

        Upvalue = Frame->Closure->Upvalues[Local];
        *(Upvalue->Value) = CKI_STACK_TOP();
        CKI_DISPATCH();

    CKI_CASE(CkOpLoadModuleVariable):
        CKI_READ_SYMBOL(Symbol);

        CK_ASSERT(Symbol < Function->Module->Variables.Count);

        CKI_PUSH(Function->Module->Variables.Data[Symbol]);
        CKI_DISPATCH();

    CKI_CASE(CkOpStoreModuleVariable):
        CKI_READ_SYMBOL(Symbol);

        CK_ASSERT(Symbol < Function->Module->Variables.Count);

        Function->Module->Variables.Data[Symbol] = CKI_STACK_TOP();
        CKI_DISPATCH();

    CKI_CASE(CkOpLoadFieldThis):
        CKI_READ_FIELD(Field);
        Receiver = Stack[0];

        CK_ASSERT(CK_IS_INSTANCE(Receiver));

        Instance = CK_AS_INSTANCE(Receiver);
        Symbol = Field + Frame->Closure->Class->SuperFieldCount;

        CK_ASSERT(Symbol < Instance->Header.Class->FieldCount);

        CKI_PUSH(Instance->Fields[Symbol]);
        CKI_DISPATCH();

    CKI_CASE(CkOpStoreFieldThis):
        CKI_READ_FIELD(Field);
        Receiver = Stack[0];

        CK_ASSERT(CK_IS_INSTANCE(Receiver));

        Instance = CK_AS_INSTANCE(Receiver);
        Symbol = Field + Frame->Closure->Class->SuperFieldCount;

        CK_ASSERT(Symbol < Instance->Header.Class->FieldCount);

        Instance->Fields[Symbol] = CKI_STACK_TOP();
        CKI_DISPATCH();

    CKI_CASE(CkOpLoadField):
        CKI_READ_FIELD(Field);
        Receiver = CKI_POP();

        CK_ASSERT(CK_IS_INSTANCE(Receiver));

        Instance = CK_AS_INSTANCE(Receiver);
        Symbol = Field + Frame->Closure->Class->SuperFieldCount;

        CK_ASSERT(Symbol < Instance->Header.Class->FieldCount);

        CKI_PUSH(Instance->Fields[Symbol]);
        CKI_DISPATCH();

    CKI_CASE(CkOpStoreField):
        CKI_READ_FIELD(Field);
        Receiver = CKI_POP();

        CK_ASSERT(CK_IS_INSTANCE(Receiver));

        Instance = CK_AS_INSTANCE(Receiver);
        Symbol = Field + Frame->Closure->Class->SuperFieldCount;

        CK_ASSERT(Symbol < Instance->Header.Class->FieldCount);

        Instance->Fields[Symbol] = CKI_STACK_TOP();
        CKI_DISPATCH();

    CKI_CASE(CkOpPop):
        CKI_DROP();
        CKI_DISPATCH();

    CKI_CASE(CkOpCall0):
    CKI_CASE(CkOpCall1):
    CKI_CASE(CkOpCall2):
    CKI_CASE(CkOpCall3):
    CKI_CASE(CkOpCall4):
    CKI_CASE(CkOpCall5):
    CKI_CASE(CkOpCall6):
    CKI_CASE(CkOpCall7):
    CKI_CASE(CkOpCall8):
        Arity = Instruction - CkOpCall0 + 1;
        CKI_READ_SYMBOL(Symbol);
        Arguments = Fiber->StackTop - Arity;
        Class = CkpGetClass(Vm, Arguments[0]);
        MethodName = Function->Module->Strings.List.Data[Symbol];
        CKI_STORE_FRAME();
        CkpCallMethod(Vm, Class, MethodName, Arity);
        CKI_LOAD_FIBER();
        CKI_DISPATCH();

    CKI_CASE(CkOpCall):
        CKI_READ_ARITY(Arity);
        Arity += 1;
        CKI_READ_SYMBOL(Symbol);
        Arguments = Fiber->StackTop - Arity;
        Class = CkpGetClass(Vm, Arguments[0]);
        MethodName = Function->Module->Strings.List.Data[Symbol];
        CKI_STORE_FRAME();
        CkpCallMethod(Vm, Class, MethodName, Arity);
        CKI_LOAD_FIBER();
        CKI_DISPATCH();

    CKI_CASE(CkOpSuperCall0):
    CKI_CASE(CkOpSuperCall1):
    CKI_CASE(CkOpSuperCall2):
    CKI_CASE(CkOpSuperCall3):
    CKI_CASE(CkOpSuperCall4):
    CKI_CASE(CkOpSuperCall5):
    CKI_CASE(CkOpSuperCall6):
    CKI_CASE(CkOpSuperCall7):
    CKI_CASE(CkOpSuperCall8):
        Arity = Instruction - CkOpSuperCall0 + 1;
        CKI_READ_SYMBOL(Symbol);
        Arguments = Fiber->StackTop - Arity;
        Class = Frame->Closure->Class->Super;
        MethodName = Function->Module->Strings.List.Data[Symbol];
        CKI_STORE_FRAME();
        CkpCallMethod(Vm, Class, MethodName, Arity);
        CKI_LOAD_FIBER();
        CKI_DISPATCH();

    CKI_CASE(CkOpSuperCall):
        CKI_READ_ARITY(Arity);
        Arity += 1;
        CKI_READ_SYMBOL(Symbol);
        Arguments = Fiber->StackTop - Arity;
        Class = Frame->Closure->Class->Super;
        MethodName = Function->Module->Strings.List.Data[Symbol];
        CKI_STORE_FRAME();
        CkpCallMethod(Vm, Class, MethodName, Arity);
        CKI_LOAD_FIBER();
        CKI_DISPATCH();

    CKI_CASE(CkOpIndirectCall):
        CKI_READ_ARITY(Arity);
        Arity += 1;
        Arguments = Fiber->StackTop - Arity;
        CKI_STORE_FRAME();
        if (CK_IS_CLOSURE(Arguments[0])) {
            Closure = CK_AS_CLOSURE(Arguments[0]);
            CkpCallFunction(Vm, Closure, Arity);

        //
        // Calling a class is the official method of constructing a new object.
        //

        } else if (CK_IS_CLASS(Arguments[0])) {
            Class = CK_AS_CLASS(Arguments[0]);
            CkpInstantiateClass(Vm, Class, Arity);

        } else {
            CkpRuntimeError(Vm, "TypeError", "Object is not callable");
        }

        CKI_LOAD_FIBER();
        CKI_DISPATCH();

    CKI_CASE(CkOpJump):
        CKI_READ_OFFSET(Offset);

        CK_ASSERT(Ip + Offset < Function->Code.Data + Function->Code.Count);

        Ip += Offset;
        CKI_DISPATCH();

    CKI_CASE(CkOpLoop):
        CKI_READ_OFFSET(Offset);

        CK_ASSERT(Ip - Offset >= Function->Code.Data);

        Ip -= Offset;
        CKI_DISPATCH();

    CKI_CASE(CkOpJumpIf):
        CKI_READ_OFFSET(Offset);
        Value = CKI_POP();
        if (CkpGetValueBoolean(Value) == FALSE) {

            CK_ASSERT(Ip + Offset < Function->Code.Data + Function->Code.Count);

            Ip += Offset;
        }

        CKI_DISPATCH();

    //
    // For And, if the value on the stack is false, short circuit the next part.
    // Otherwise, pop the condition and keep going. Or is just the opposite:
    // skip the next part if the value is non-zero.
    //

    CKI_CASE(CkOpAnd):
    CKI_CASE(CkOpOr):
        CKI_READ_OFFSET(Offset);
        Value = CKI_STACK_TOP();
        if (CkpGetValueBoolean(Value) == (Instruction - CkOpAnd)) {

            CK_ASSERT(Ip + Offset < Function->Code.Data + Function->Code.Count);

            Ip += Offset;

        } else {
            CKI_DROP();
        }

        CKI_DISPATCH();

    CKI_CASE(CkOpCloseUpvalue):
        CkpCloseUpvalues(Fiber, Fiber->StackTop - 1);
        CKI_DISPATCH();

    CKI_CASE(CkOpReturn):
        Value = CKI_POP();

        CK_ASSERT((Fiber->FrameCount != 0) &&
                  (Frame->TryCount <= Fiber->TryCount));

        Fiber->FrameCount -= 1;
        Fiber->TryCount = Frame->TryCount;
        CkpCloseUpvalues(Fiber, Stack);

        //
        // Handle the fiber completing. Either return the value to the C caller,
        // or move on to the next fiber up the fiber stack.
        //

        if (Fiber->FrameCount == 0) {

            CK_ASSERT(Fiber->ForeignCalls == 0);
            CK_ASSERT(Fiber->TryCount == 0);

            if (Fiber->Caller == NULL) {

                CK_ASSERT(Vm->ForeignCalls == 0);

                Fiber->Stack[0] = Value;
                Fiber->StackTop = Fiber->Stack + 1;
                goto RunInterpreterEnd;
            }

            NextFiber = Fiber->Caller;
            Fiber->Caller = NULL;
            Fiber = NextFiber;
            Vm->Fiber = NextFiber;
            Vm->ForeignCalls -= NextFiber->ForeignCalls;

            CK_ASSERT(Fiber->StackTop > Fiber->Stack);

            *(NextFiber->StackTop - 1) = Value;

        //
        // Move up to the caller.
        //

        } else {

            CK_ASSERT(Stack == Frame->StackStart);

            Stack[0] = Value;
            Fiber->StackTop = Frame->StackStart + 1;
        }

        CKI_LOAD_FRAME();

        //
        // If the caller is a foreign function, return.
        //

        if (Ip == NULL) {
            return CkSuccess;
        }

        CKI_DISPATCH();

    CKI_CASE(CkOpClosure):
        CKI_READ_SYMBOL(Symbol);

        CK_ASSERT(Symbol < Function->Constants.Count);
        CK_ASSERT(CK_IS_FUNCTION(Function->Constants.Data[Symbol]));

        //
        // Bind this closure to the class of the closure that defined it, for
        // the purposes of getting its field offset and superclass. If it turns
        // out to be a method, it will get rebound to the correct class before
        // it can be invoked.
        //

        Function = CK_AS_FUNCTION(Function->Constants.Data[Symbol]);
        CKI_STORE_FRAME();
        Closure = CkpClosureCreate(Vm, Function, Frame->Closure->Class);
        if (Closure == NULL) {
            CKI_LOAD_FIBER();
            CKI_DISPATCH();
        }

        CK_OBJECT_VALUE(Value, Closure);
        CKI_PUSH(Value);

        //
        // Now that the closure is pushed (and therefore won't be garbage
        // collected), gather all the upvalues. Upvalues are either captured
        // local variables that will be closed when the local goes out of
        // scope, or point at other upvalues.
        //

        for (Index = 0; Index < Function->UpvalueCount; Index += 1) {
            CKI_READ_BYTE(IsLocal);
            CKI_READ_BYTE(Local);
            if (IsLocal != FALSE) {
                Closure->Upvalues[Index] =
                       CkpCaptureUpvalue(Vm, Fiber, Frame->StackStart + Local);

            } else {
                Closure->Upvalues[Index] = Frame->Closure->Upvalues[Local];
            }
        }

        Function = Frame->Closure->U.Block.Function;
        CKI_DISPATCH();

    CKI_CASE(CkOpClass):
        CKI_READ_FIELD(Field);
        CKI_STORE_FRAME();
        CkpClassCreate(Vm, Field, Function->Module);
        CKI_LOAD_FIBER();
        CKI_DISPATCH();

    CKI_CASE(CkOpMethod):
    CKI_CASE(CkOpStaticMethod):
        CKI_READ_SYMBOL(Symbol);

        CK_ASSERT(CK_IS_CLASS(CKI_STACK_TOP()));

        Class = CK_AS_CLASS(CKI_STACK_TOP());
        if (Instruction == CkOpStaticMethod) {
            Class = Class->Header.Class;
        }

        Value = CKI_STACK_TOP2();

        CK_ASSERT(CK_IS_CLOSURE(Value));
        CK_ASSERT(Symbol < Function->Module->Strings.List.Count);

        MethodName = Function->Module->Strings.List.Data[Symbol];
        CKI_DROP();
        CKI_DROP();
        CKI_STORE_FRAME();
        CkpBindMethod(Vm, Class, MethodName, CK_AS_CLOSURE(Value));
        CKI_LOAD_FIBER();
        CKI_DISPATCH();

    CKI_CASE(CkOpTry):
        CKI_READ_OFFSET(Offset);

        CK_ASSERT(Ip + Offset < Function->Code.Data + Function->Code.Count);

        CKI_STORE_FRAME();
        CkpPushTryBlock(Vm, Ip + Offset);
        CKI_LOAD_FIBER();
        CKI_DISPATCH();

    CKI_CASE(CkOpPopTry):

        CK_ASSERT(Fiber->TryCount != 0);

        Fiber->TryCount -= 1;
        CKI_DISPATCH();

    //
    // End opcodes should never get executed because they're always preceded
    // by a return.
    //

    CKI_CASE(CkOpEnd):
    CKI_DEFAULT_CASE()

        CK_ASSERT(FALSE);

        CKI_DISPATCH();
    }

RunInterpreterEnd:
    if ((Vm->Fiber == NULL) || (!CK_IS_NULL(Vm->Fiber->Error))) {
        return CkErrorRuntime;
    }

    return CkSuccess;
}

VOID
CkpClassCreate (
    PCK_VM Vm,
    CK_SYMBOL_INDEX FieldCount,
    PCK_MODULE Module
    )

/*++

Routine Description:

    This routine creates a new class in response to executing a class opcode.
    The superclass should be on the top of the stack, followed by the name of
    the class.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    FieldCount - Supplies the number of fields in the new class.

    Module - Supplies a pointer to the module that owns the class.

Return Value:

    None.

--*/

{

    PCK_CLASS Class;
    PCK_CLASS Metaclass;
    CK_VALUE MetaclassName;
    CK_VALUE Name;
    PCK_CLASS Super;
    CK_VALUE Superclass;

    CK_ASSERT(Vm->Fiber->StackTop >= Vm->Fiber->Stack + 2);

    Class = NULL;
    Superclass = *(Vm->Fiber->StackTop - 1);
    Name = *(Vm->Fiber->StackTop - 2);
    if (CkpValidateSuperclass(Vm, Name, Superclass, FieldCount) == FALSE) {
        goto ClassCreateEnd;
    }

    Super = CK_AS_CLASS(Superclass);

    //
    // Create the metaclass. This inherits directly from Class.
    //

    MetaclassName = CkpStringFormat(Vm, "@Meta", Name);
    if (CK_IS_NULL(MetaclassName)) {
        goto ClassCreateEnd;
    }

    CkpPushRoot(Vm, CK_AS_OBJECT(MetaclassName));
    Metaclass = CkpClassAllocate(Vm, Module, 0, CK_AS_STRING(MetaclassName));
    CkpPopRoot(Vm);
    if (Metaclass == NULL) {
        goto ClassCreateEnd;
    }

    CkpPushRoot(Vm, &(Metaclass->Header));
    CkpBindSuperclass(Vm, Metaclass, Vm->Class.Class);

    //
    // Create the actual class.
    //

    Class = CkpClassAllocate(Vm,
                             Module,
                             FieldCount + Super->FieldCount,
                             CK_AS_STRING(Name));

    if (Class == NULL) {
        CkpPopRoot(Vm);
        return;
    }

    CkpPushRoot(Vm, &(Class->Header));
    Class->Header.Class = Metaclass;
    CkpBindSuperclass(Vm, Class, Super);
    CkpPopRoot(Vm);
    CkpPopRoot(Vm);

ClassCreateEnd:

    //
    // Pop the name and superclass, but push the resulting class, for a net
    // effect of -1.
    //

    Vm->Fiber->StackTop -= 1;
    if (Class != NULL) {
        CK_OBJECT_VALUE(*(Vm->Fiber->StackTop - 1), Class);

    } else {
        *(Vm->Fiber->StackTop - 1) = CkNullValue;
    }

    return;
}

BOOL
CkpInstantiateClass (
    PCK_VM Vm,
    PCK_CLASS Class,
    CK_ARITY Arity
    )

/*++

Routine Description:

    This routine creates a new class instance on the stack and invokes its
    initialization routine.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Class - Supplies a pointer to the class that owns the method.

    Arity - Supplies the number of arguments the method was called with in
        code (plus one for the receiver).

Return Value:

    TRUE if a new frame was pushed onto the stack and needs to be run by the
    interpreter.

    FALSE if the call completed already (primitive and foreign functions fit
    this category).

--*/

{

    PCK_VALUE Arguments;
    CK_STRING FakeString;
    PCK_FIBER Fiber;
    UINTN FrameCount;
    UINTN Length;
    CHAR Name[CK_MAX_METHOD_SIGNATURE];
    CK_VALUE NameValue;
    CK_FUNCTION_SIGNATURE Signature;
    UINTN TryCount;

    Fiber = Vm->Fiber;
    FrameCount = Fiber->FrameCount;
    TryCount = Fiber->TryCount;
    Arguments = Fiber->StackTop - Arity;

    //
    // Create the uninitialized class instance.
    //

    Arguments[0] = CkpCreateInstance(Vm, Class);
    if (CK_EXCEPTION_RAISED(Vm, Fiber, TryCount, FrameCount)) {
        return FALSE;
    }

    //
    // Create the init method signature function signature and invoke the
    // init method.
    //

    Signature.Name = "__init";
    Signature.Length = 6;
    Signature.Arity = Arity - 1;
    Length = sizeof(Name);
    CkpPrintSignature(&Signature, Name, &Length);
    CkpStringFake(&FakeString, Name, Length);
    CK_OBJECT_VALUE(NameValue, &FakeString);
    return CkpCallMethod(Vm, Class, NameValue, Arity);
}

BOOL
CkpCallMethod (
    PCK_VM Vm,
    PCK_CLASS Class,
    CK_VALUE MethodName,
    CK_ARITY Arity
    )

/*++

Routine Description:

    This routine invokes a class instance method for execution.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Class - Supplies a pointer to the class that owns the method.

    MethodName - Supplies the name of the method to look up on the class.

    Arity - Supplies the number of arguments the method was called with in
        code (plus one for the receiver).

Return Value:

    TRUE if a new frame was pushed onto the stack and needs to be run by the
    interpreter.

    FALSE if the call completed already (primitive and foreign functions fit
    this category).

--*/

{

    PCK_CLOSURE Closure;
    CK_VALUE Method;
    PCK_STRING NameString;

    CK_ASSERT(CK_IS_STRING(MethodName));

    //
    // Look up the method in the receiver, and call it.
    //

    Method = CkpDictGet(Class->Methods, MethodName);
    if (CK_IS_UNDEFINED(Method)) {
        NameString = CK_AS_STRING(MethodName);
        CkpRuntimeError(Vm,
                        "LookupError",
                        "%s does not implement %s",
                        Class->Name->Value,
                        NameString->Value);

        return FALSE;
    }

    Closure = CK_AS_CLOSURE(Method);
    return CkpCallFunction(Vm, Closure, Arity);
}

BOOL
CkpCallFunction (
    PCK_VM Vm,
    PCK_CLOSURE Closure,
    CK_ARITY Arity
    )

/*++

Routine Description:

    This routine invokes the given closure for execution.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Closure - Supplies a pointer to the closure to execute.

    Arity - Supplies the number of arguments the method was called with in
        code (plus one for the receiver).

Return Value:

    TRUE if a new frame was pushed onto the stack and needs to be run by the
    interpreter.

    FALSE if the call completed already (primitive and foreign functions fit
    this category).

--*/

{

    PCK_VALUE Arguments;
    PCK_FIBER Fiber;
    UINTN FrameCount;
    BOOL FramePushed;
    CK_ARITY FunctionArity;
    PCK_STRING Name;
    UINTN RequiredStackSize;
    UINTN ReturnStackIndex;
    UINTN StackSize;
    UINTN TryCount;

    Fiber = Vm->Fiber;
    FramePushed = FALSE;

    CK_ASSERT(Closure->Header.Type == CkObjectClosure);

    //
    // Check the arity of the function against what it's supposed to be.
    //

    FunctionArity = CkpGetFunctionArity(Closure);
    if (FunctionArity != Arity - 1) {
        Name = CkpGetFunctionName(Closure);
        CkpRuntimeError(Vm,
                        "TypeError",
                        "Expected %d arguments for %s, got %d",
                        FunctionArity,
                        Name->Value,
                        Arity - 1);

        return FramePushed;
    }

    //
    // Reallocate the stack if needed.
    //

    StackSize = Fiber->StackTop - Fiber->Stack;
    if (Closure->Type == CkClosurePrimitive) {
        Arguments = Fiber->StackTop - Arity;
        if (Closure->U.Primitive.Function(Vm, Arguments) != FALSE) {
            Fiber->StackTop -= Arity - 1;
        }

    } else if (Closure->Type == CkClosureBlock) {
        CkpAppendCallFrame(Vm, Fiber, Closure, Fiber->StackTop - Arity);
        RequiredStackSize = StackSize + Closure->U.Block.Function->MaxStack;
        if (RequiredStackSize > Fiber->StackCapacity) {
            CkpEnsureStack(Vm, Fiber, RequiredStackSize);
        }

        FramePushed = TRUE;

    } else if (Closure->Type == CkClosureForeign) {
        TryCount = Fiber->TryCount;
        CkpAppendCallFrame(Vm, Fiber, Closure, Fiber->StackTop - Arity);
        FrameCount = Fiber->FrameCount;
        CkpEnsureStack(Vm,
                       Fiber,
                       (Fiber->StackTop - Fiber->Stack) + CK_MIN_FOREIGN_STACK);

        if (CK_EXCEPTION_RAISED(Vm, Fiber, TryCount, FrameCount)) {
            return FALSE;
        }

        //
        // Get the stack index to return to. Add one to account for the return
        // value. The stack get reallocated during the foreign function, which
        // is why this must be in the form of an index.
        //

        ReturnStackIndex = (Fiber->StackTop - Arity) + 1 - Fiber->Stack;

        //
        // Increment the foreign call count to prevent fiber switches from
        // happening while the VM call stack is linked with the C stack.
        //

        Fiber->ForeignCalls += 1;
        Closure->U.Foreign.Function(Vm);
        Fiber->ForeignCalls -= 1;

        //
        // If an exception was raised by the foreign function, don't restore
        // the frame count or mess with the stack.
        //

        if (CK_EXCEPTION_RAISED(Vm, Fiber, TryCount, FrameCount)) {
            return FALSE;
        }

        CK_ASSERT(Fiber->FrameCount != 0);

        Fiber->FrameCount -= 1;
        Fiber->StackTop = Fiber->Stack + ReturnStackIndex;

    } else {

        CK_ASSERT(FALSE);

    }

    return FramePushed;
}

//
// --------------------------------------------------------- Internal Functions
//

PCK_UPVALUE
CkpCaptureUpvalue (
    PCK_VM Vm,
    PCK_FIBER Fiber,
    PCK_VALUE Local
    )

/*++

Routine Description:

    This routine captures the given local variable into an upvalue. It may
    create a new upvalue in the fiber for a local that has never been closed
    over, or it may reuse an existing upvalue to ensure all closures see the
    same variable.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Fiber - Supplies a pointer to the currently running fiber.

    Local - Supplies a pointer to the local variable to capture.

Return Value:

    None.

--*/

{

    PCK_UPVALUE NewUpvalue;
    PCK_UPVALUE Previous;
    PCK_UPVALUE Upvalue;

    Previous = NULL;
    Upvalue = Fiber->OpenUpvalues;

    //
    // Find the location in the upvalue list where this one should reside.
    // The list of upvalues is kept sorted so that popping and closing them
    // is easy.
    //

    while ((Upvalue != NULL) && (Upvalue->Value > Local)) {
        Previous = Upvalue;
        Upvalue = Upvalue->Next;
    }

    //
    // If there's already an upvalue for this variable, reuse it so that when
    // it is closed every closure is still sharing the same variable.
    //

    if ((Upvalue != NULL) && (Upvalue->Value == Local)) {
        return Upvalue;
    }

    //
    // Create and initialize a new upvalue.
    //

    NewUpvalue = CkAllocate(Vm, sizeof(CK_UPVALUE));
    if (NewUpvalue == NULL) {
        return NULL;
    }

    CkpInitializeObject(Vm, &(NewUpvalue->Header), CkObjectUpvalue, NULL);
    NewUpvalue->Value = Local;
    NewUpvalue->Closed = CkNullValue;
    NewUpvalue->Next = Upvalue;
    if (Previous != NULL) {
        Previous->Next = NewUpvalue;

    } else {
        Fiber->OpenUpvalues = NewUpvalue;
    }

    return NewUpvalue;
}

VOID
CkpCloseUpvalues (
    PCK_FIBER Fiber,
    PCK_VALUE Last
    )

/*++

Routine Description:

    This routine closes any upvalues that are based on the locals greater than
    or equal the given stack value (as the stack is about to be popped to this
    value).

Arguments:

    Fiber - Supplies a pointer to the current fiber.

    Last - Supplies the soon-to-be new top of the stack.

Return Value:

    None.

--*/

{

    PCK_UPVALUE Upvalue;

    //
    // For each upvalue that's pointed at or above the new stack value, copy
    // the value into the upvalue structure, and then point the pointer at the
    // local copy.
    //

    while ((Fiber->OpenUpvalues != NULL) &&
           (Fiber->OpenUpvalues->Value >= Last)) {

        Upvalue = Fiber->OpenUpvalues;
        Upvalue->Closed = *(Upvalue->Value);
        Upvalue->Value = &(Upvalue->Closed);
        Fiber->OpenUpvalues = Upvalue->Next;
    }

    return;
}

BOOL
CkpValidateSuperclass (
    PCK_VM Vm,
    CK_VALUE Name,
    CK_VALUE Superclass,
    CK_SYMBOL_INDEX FieldCount
    )

/*++

Routine Description:

    This routine validates a given class name and superclass.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Name - Supplies the name string value.

    Superclass - Supplies the superclass object value.

    FieldCount - Supplies the number of fields in the new class.

Return Value:

    TRUE on success.

    FALSE on failure. The fiber error will be set.

--*/

{

    PCK_CLASS Super;

    if (!CK_IS_STRING(Name)) {
        CkpRuntimeError(Vm, "TypeError", "Class name must be a string");
        return FALSE;
    }

    if (!CK_IS_CLASS(Superclass)) {
        CkpRuntimeError(Vm, "TypeError", "Class must inherit from a class");
        return FALSE;
    }

    Super = CK_AS_CLASS(Superclass);

    //
    // Some classes cannot be inherited from because their structure is
    // different from a normal class instance.
    //

    if ((Super->Flags & CK_CLASS_UNINHERITABLE) != 0) {
        CkpRuntimeError(Vm,
                        "ValueError",
                        "Class cannot inherit from builtin class");

        return FALSE;
    }

    if ((Super->Flags & CK_CLASS_FOREIGN) != 0) {
        CkpRuntimeError(Vm,
                        "ValueError",
                        "Cannot inherit from a foreign class");

        return FALSE;
    }

    if ((Super->FieldCount + FieldCount) >= CK_MAX_FIELDS) {
        CkpRuntimeError(Vm, "RuntimeError", "Class has too many fields");
        return FALSE;
    }

    return TRUE;
}

