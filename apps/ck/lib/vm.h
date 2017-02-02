/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    vm.h

Abstract:

    This header contains definitions for the Chalk virtual machine.

Author:

    Evan Green 28-May-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

#define CK_READ8(_Bytes) (*(PUCHAR)(_Bytes))
#define CK_READ16(_Bytes) \
    (((*(PUCHAR)(_Bytes)) << 8) | *(((PUCHAR)(_Bytes)) + 1))

//
// This macro determines whether or not the fiber has errored out. Users of
// this macro must save the fiber try count before attempting the operation
// that might have generated an exception.
//

#define CK_EXCEPTION_RAISED(_Vm, _Fiber, _TryCount, _FrameCount) \
    (((_Vm)->Fiber != (_Fiber)) || ((_Fiber)->TryCount < (_TryCount)) || \
     ((_Fiber)->FrameCount < (_FrameCount)))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum number of objects that can be pushed onto the working
// object stack.
//

#define CK_MAX_WORKING_OBJECTS 6

//
// Define a reasonable size for error messages.
//

#define CK_MAX_ERROR_MESSAGE (CK_MAX_NAME + 128)

//
// Define the maximum stack size. Above this, the power of 2 growth starts to
// run the risk of overflowing.
//

#define CK_MAX_STACK 0x80000000

//
// Define the minimum number of stack slots available to foreign functions.
//

#define CK_MIN_FOREIGN_STACK 20

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _CK_COMPILER CK_COMPILER, *PCK_COMPILER;

/*++

Enumeration Description:

    This enumeration describes the opcodes of the Chalk intepreter.

Values:

    CkOpNop - Performs no operation.

    CkOpConstant - Pushes the constant with the index specified by the next
        two bytes in the instruction stream onto the stack.

    CkOpStringConstant - Pushes the string constant with the index specified
        by the next two bytes in the instruction stream onto the stack. The
        string constant table is located in the module.

    CkOpNull - Pushes null onto the stack.

    CkOpLiteral0 - Pushes the value 0 onto the stack. Subsequent opcodes up to
        CkOpLiteral8 push 1-7 onto the stack, respectively.

    CkOpLiteral8 - Pushes the literal 8 onto the stack.

    CkOpLoadLocal0 - Pushes the value at the zeroth local variable slot (given
        as a following byte op) onto the stack. Subsequent opcodes up to
        CkOpLoadLocal8 push locals 1-7 onto the stack, respectively.

    CkOpLoadLocal8 - Pushes the value at the eighth local variable slot onto
        the stack.

    CkOpLoadLocal - Pushes the value at the local variable slot indicated by
        the next opcode byte onto the stack.

    CkOpStoreLocal - Stores the value at the top of the stack into the local
        variable slot named by the next instruction byte. Does not pop the
        value from the stack.

    CkOpLoadUpvalue - Loads the upvalue specified by the next instruction byte,
        and pushes it onto the stack.

    CkOpStoreUpvalue - Stores the top of the stack into the upvalue specified
        by the next instruction byte. Does not pop the stack value.

    CkOpLoadModuleVariable - Pushes the top-level module variable specified by
        the next instruction 2-bytes onto the stack.

    CkOpStoreModuleVariable - Saves the top of the stack into the top-level
        module variable specified by the next instruction 2-bytes. Does not pop
        the stack value.

    CkOpLoadFieldThis - Pushes the value of the instance field specified in the
        next instruction byte onto the stack. The instance is the receiver of
        the current function.

    CkOpStoreFieldThis - Stores the top of the stack into the instance field
        specified by the next instruction byte. The instance is the receiver
        of the current function. Does not pop the stack.

    CkOpLoadField - Pops a class instance value and pushes its field specified
        by the next instruction byte.

    CkOpStoreField - Pops a class instance value and stores the subsequent
        top of the stack in the field specified by the next instruction byte.
        Pops the instance but not the value.

    CkOpPop - Pops and discards the top of the stack.

    CkOpCall0 - Invokes the method with the symbol specified by the next
        instruction word. The opcode number describes the number of arguments
        that have already been pushed (not including the receiver). Subsequent
        opcodes code for 1-7 arguments, respectively.

    CkOpCall8 - Invokes the method with the symbol specified by the next
        instruction word, with 8 arguments.

    CkOpCall - Invokes the method with the number of arguments specified by the
        next instruction byte. The symbol is specified by the subsequent
        instruction word.

    CkOpIndirectCall - Invokes the method with the number of arguments
        specified by the next instruction byte. The method to call is pushed
        in the receiver slot on the stack.

    CkOpSuperCall0 - Invokes a method on the superclass with the symbol
        given by the next instruction word. The opcode specifies the number of
        arguments (the next 8 opcodes code for 1-8 arguments).

    CkOpSuperCall8 - Invokes a method on the superclass with the symbol given
        by the next instruction word, specifying 8 arguments.

    CkOpSuperCall - Invokes a method on the superclass with the number of
        arguments in the next instruction byte. The subsequent instruction word
        specifies the symbol to invoke.

    CkOpJump - Moves the instruction pointer forward by the number of bytes
        specified in the following instruction word.

    CkOpLoop - Moves the instruction pointer backward by the number of bytes
        specified in the following instruction word.

    CkOpJumpIf - Pops a value off the stack. If it is not true-ish then jump
        the instruction pointer forward by the amount specified by the
        following instruction word.

    CkOpAnd - Check the boolean value of the value at the top of the stack. If
        it is false, move the instruction pointer forward by the amount
        specified in the following instruction word. If it is true, pop the
        value and continue.

    CkOpOr - Check the boolean value of the value at the top of the stack. If
        the value is not false, jump the instruction pointer forward by the
        amount specified in the following instruction word. If it is false, pop
        the value and continue.

    CkOpCloseUpvalue - Close the upvalue for the local on the top of the
        stack. Pops the value.

    CkOpReturn - Return from the current function. The value at the top of
        the stack is the return value. This is not popped, as it is given
        to the caller.

    CkOpClosure - Create a closure for the function stored at the symbol
        index specified by the following instruction word. Following in the
        instruction stream is a number of argument pairs. The first element in
        each pair is true if the variable bein captured is a local (as
        opposed to an upvalue). The second element is the symbol index of the
        local or upvalue being captured. The created closure is then pushed.

    CkOpClass - Creates a class type. The superclass type is popped from the
        stack. The name of the new class is then popped from the stack. The
        number of fields in the class is encoded in the next instruction byte.
        The new class type object is then pushed onto the stack.

    CkOpMethod - Defines a new method for the symbol given in the subsequent
        instruction word. The class that will receive the method is popped off
        the stack. Then the function or closure defining the body is popped.

    CkOpStaticMethod - Defines a new static for the symbol given in the
        subsequent instruction word. The class whose metaclass will receive the
        method is popped off the stack. Then the function or closure defining
        the body is popped.

    CkOpTry - Enters a new try block scope. The offset of the first except
        block is encoded in the next two bytes in the instruction stream.

    CkOpPopTry - Leaves a previously pushed try block scope.

    CkOpEnd - This opcode terminates a compilation. It should always be
        preceded by a return and therefore should never be executed.

--*/

typedef enum _CK_OPCODE {
    CkOpNop,
    CkOpConstant,
    CkOpStringConstant,
    CkOpNull,
    CkOpLiteral0,
    CkOpLiteral1,
    CkOpLiteral2,
    CkOpLiteral3,
    CkOpLiteral4,
    CkOpLiteral5,
    CkOpLiteral6,
    CkOpLiteral7,
    CkOpLiteral8,
    CkOpLoadLocal0,
    CkOpLoadLocal1,
    CkOpLoadLocal2,
    CkOpLoadLocal3,
    CkOpLoadLocal4,
    CkOpLoadLocal5,
    CkOpLoadLocal6,
    CkOpLoadLocal7,
    CkOpLoadLocal8,
    CkOpLoadLocal,
    CkOpStoreLocal,
    CkOpLoadUpvalue,
    CkOpStoreUpvalue,
    CkOpLoadModuleVariable,
    CkOpStoreModuleVariable,
    CkOpLoadFieldThis,
    CkOpStoreFieldThis,
    CkOpLoadField,
    CkOpStoreField,
    CkOpPop,
    CkOpCall0,
    CkOpCall1,
    CkOpCall2,
    CkOpCall3,
    CkOpCall4,
    CkOpCall5,
    CkOpCall6,
    CkOpCall7,
    CkOpCall8,
    CkOpCall,
    CkOpIndirectCall,
    CkOpSuperCall0,
    CkOpSuperCall1,
    CkOpSuperCall2,
    CkOpSuperCall3,
    CkOpSuperCall4,
    CkOpSuperCall5,
    CkOpSuperCall6,
    CkOpSuperCall7,
    CkOpSuperCall8,
    CkOpSuperCall,
    CkOpJump,
    CkOpLoop,
    CkOpJumpIf,
    CkOpAnd,
    CkOpOr,
    CkOpCloseUpvalue,
    CkOpReturn,
    CkOpClosure,
    CkOpClass,
    CkOpMethod,
    CkOpStaticMethod,
    CkOpTry,
    CkOpPopTry,
    CkOpEnd,
    CkOpcodeCount
} CK_OPCODE, *PCK_OPCODE;

/*++

Structure Description:

    This structure contains pointers to the builtin classes.

Members:

    Class - Stores a pointer to the class type, the base type behind all types.

    Fiber - Stores a pointer to the fiber type, the encapsulation of a unit of
        execution.

    Function - Stores a pointer to the function type.

    List - Stores a pointer to the basic array type.

    Dict - Stores a pointer to the dictionary type.

    Null - Stores a pointer to the null class.

    Int - Stores a pointer to the integer class.

    Object - Stores a pointer to the object, the grandmother of all
        instantiations.

    Range - Stores a pointer to the range class.

    String - Stores a pointer to the string class.

    Module - Stores a pointer to the module class.

    Core - Stores a pointer to the core interpreter class.

    Exception - Stores a pointer to the exception class type.

--*/

typedef struct _CK_BUILTIN_CLASSES {
    PCK_CLASS Class;
    PCK_CLASS Fiber;
    PCK_CLASS Function;
    PCK_CLASS List;
    PCK_CLASS Dict;
    PCK_CLASS Null;
    PCK_CLASS Int;
    PCK_CLASS Object;
    PCK_CLASS Range;
    PCK_CLASS String;
    PCK_CLASS Module;
    PCK_CLASS Core;
    PCK_CLASS Exception;
} CK_BUILTIN_CLASSES, *PCK_BUILTIN_CLASSES;

/*++

Structure Description:

    This structure contains the state for the Chalk interpreter.

Members:

    Class - Stores the builtin classes.

    Configuration - Stores the VM configuration and wiring to the rest of the
        system.

    Modules - Stores the dictionary of loaded modules.

    BytesAllocated - Stores the number of bytes allocated under Chalk memory
        management. Chalk makes a few allocations that are not tracked here,
        but they're fairly minimal and mostly fixed. This number includes all
        memory that was live after the last garbage collection, as well as any
        memory that's been allocated since then. This number does not include
        memory that has been freed since the last garbage collection.

    NextGarbageCollection - Stores the size that the allocated bytes have to
        get to in order to trigger the next garbage collection.

    GarbageRuns - Stores the number of times the garbage collector has run.

    GarbageFreed - Stores the number of objects freed during the most recent
        garbage collection run.

    FirstObject - Stores a pointer to the first object in the massive singly
        linked list of all living objects. This is the list that the garbage
        collector traverses.

    KissList - Stores the tail of the list of objects that have been kissed.
        The list is circular to ensure that the last object has a non-null
        next pointer.

    WorkingObjects - Stores a fixed stack of objects that should not be
        garbage collected but who are not necessarily linked anywhere else.

    WorkingObjectCount - Stores the number of valid objects in the working
        object stack.

    Compiler - Stores an optional pointer to the current compiler. This link is
        needed so the garbage collector can discover its objects.

    Fiber - Stores a pointer to the currently running fiber.

    ModulePath - Stores the list of paths to search when loading a new module.

    ForeignCalls - Stores the number of active foreign calls running
        in any fibers that are not the currently running one.

    MemoryException - Stores a value indicating whether a memory exception is
        currently being dispatched. This prevents infinite recursion in the
        case that allocating part of an exception fails.

    UnhandledException - Stores a pointer to a function used to catch any
        unhandled exceptions.

    Context - Stores an opaque user context pointer that can be used by whoever
        is integrating the Chalk library.

--*/

struct _CK_VM {
    CK_CONFIGURATION Configuration;
    CK_BUILTIN_CLASSES Class;
    PCK_DICT Modules;
    UINTN BytesAllocated;
    UINTN NextGarbageCollection;
    ULONG GarbageRuns;
    ULONG GarbageFreed;
    PCK_OBJECT FirstObject;
    PCK_OBJECT KissList;
    PCK_OBJECT WorkingObjects[CK_MAX_WORKING_OBJECTS];
    ULONG WorkingObjectCount;
    PCK_COMPILER Compiler;
    PCK_FIBER Fiber;
    PCK_LIST ModulePath;
    LONG ForeignCalls;
    INT MemoryException;
    PCK_CLOSURE UnhandledException;
    PVOID Context;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

CK_SYMBOL_INDEX
CkpDeclareModuleVariable (
    PCK_VM Vm,
    PCK_MODULE Module,
    PSTR Name,
    UINTN Length,
    INT Line
    );

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

CK_SYMBOL_INDEX
CkpDefineModuleVariable (
    PCK_VM Vm,
    PCK_MODULE Module,
    PCSTR Name,
    UINTN Length,
    CK_VALUE Value
    );

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

PCK_VALUE
CkpFindModuleVariable (
    PCK_VM Vm,
    PCK_MODULE Module,
    PCSTR Name,
    BOOL Create
    );

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

CK_ERROR_TYPE
CkpInterpret (
    PCK_VM Vm,
    PCSTR ModuleName,
    PCSTR ModulePath,
    PCSTR Source,
    UINTN Length,
    LONG Line,
    ULONG CompilerFlags
    );

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

CK_ERROR_TYPE
CkpRunInterpreter (
    PCK_VM Vm,
    PCK_FIBER Fiber
    );

/*++

Routine Description:

    This routine implements the heart of Chalk: the main execution loop.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Fiber - Supplies a pointer to the execution context to run.

Return Value:

    Chalk status.

--*/

VOID
CkpClassCreate (
    PCK_VM Vm,
    CK_SYMBOL_INDEX FieldCount,
    PCK_MODULE Module
    );

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

BOOL
CkpInstantiateClass (
    PCK_VM Vm,
    PCK_CLASS Class,
    CK_ARITY Arity
    );

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

BOOL
CkpCallMethod (
    PCK_VM Vm,
    PCK_CLASS Class,
    CK_VALUE MethodName,
    CK_ARITY Arity
    );

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

BOOL
CkpCallFunction (
    PCK_VM Vm,
    PCK_CLOSURE Closure,
    CK_ARITY Arity
    );

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

//
// Module support functions
//

CK_VALUE
CkpModuleLoad (
    PCK_VM Vm,
    CK_VALUE ModuleName,
    PCSTR ForcedPath
    );

/*++

Routine Description:

    This routine loads the given module.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ModuleName - Supplies the module name value.

    ForcedPath - Supplies an optional pointer to the path to load.

Return Value:

    Returns the newly loaded module value on success.

    CK_NULL_VALUE on failure.

--*/

PCK_MODULE
CkpModuleLoadSource (
    PCK_VM Vm,
    CK_VALUE ModuleName,
    CK_VALUE Path,
    PCSTR Source,
    UINTN Length,
    LONG Line,
    ULONG CompilerFlags,
    PBOOL WasPrecompiled
    );

/*++

Routine Description:

    This routine loads the given source under the given module name.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ModuleName - Supplies the module name value.

    Path - Supplies an optional value that is the full path to the module.

    Source - Supplies a pointer to the null terminated string containing the
        source to interpret.

    Length - Supplies the length of the source string, not including the null
        terminator.

    Line - Supplies the line number this code starts on. Supply 1 to start at
        the beginning.

    CompilerFlags - Supplies the bitfield of compiler flags to pass along.
        See CK_COMPILER_* definitions.

    WasPrecompiled - Supplies a pointer where a boolean will be returned
        indicating if this was precompiled code or not.

Return Value:

    Returns a pointer to the newly loaded module on success.

    NULL on failure.

--*/

PCK_MODULE
CkpModuleLoadForeign (
    PCK_VM Vm,
    CK_VALUE ModuleName,
    CK_VALUE Path,
    PVOID Handle,
    PCK_FOREIGN_FUNCTION EntryPoint
    );

/*++

Routine Description:

    This routine loads a new foreign module.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ModuleName - Supplies the module name value.

    Path - Supplies an optional value that is the full path to the module.

    Handle - Supplies a handle to the opened dynamic library.

    EntryPoint - Supplies a pointer to the C function to call to load the
        module.

Return Value:

    Returns a pointer to the newly loaded module on success.

    NULL on failure.

--*/

PCK_MODULE
CkpModuleCreate (
    PCK_VM Vm,
    PCK_STRING Name,
    PCK_STRING Path
    );

/*++

Routine Description:

    This routine allocates and initializes new module object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Name - Supplies a pointer to the module name.

    Path - Supplies an optional pointer to the complete module path.

Return Value:

    Returns a pointer to the newly created module on success.

    NULL on allocation failure.

--*/

PCK_MODULE
CkpModuleAllocate (
    PCK_VM Vm,
    PCK_STRING Name,
    PCK_STRING Path
    );

/*++

Routine Description:

    This routine allocates a new module object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Name - Supplies a pointer to the module name.

    Path - Supplies an optional pointer to the complete module path.

Return Value:

    Returns a pointer to the newly allocated module on success.

    NULL on allocation failure.

--*/

PCK_MODULE
CkpModuleGet (
    PCK_VM Vm,
    CK_VALUE Name
    );

/*++

Routine Description:

    This routine attempts to find a previously loaded module.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Name - Supplies the module name value.

Return Value:

    Returns a pointer to the module with the given name on success.

    NULL if no such module exists.

--*/

VOID
CkpModuleDestroy (
    PCK_VM Vm,
    PCK_MODULE Module
    );

/*++

Routine Description:

    This routine is called when a module object is being destroyed.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Module - Supplies a pointer to the dying module.

Return Value:

    None.

--*/

CK_VALUE
CkpModuleFreeze (
    PCK_VM Vm,
    PCK_MODULE Module
    );

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

BOOL
CkpModuleThaw (
    PCK_VM Vm,
    PCK_MODULE Module,
    PCSTR Contents,
    UINTN Size
    );

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

