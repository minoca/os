/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

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
// ---------------------------------------------------------------- Definitions
//

//
// Define the initial capacity of the stack of unprocessed objects stored while
// garbage collecting.
//

#define CK_INITIAL_GRAY_CAPACITY 32

//
// Define the maximum number of objects that can be pushed onto the working
// object stack.
//

#define CK_MAX_WORKING_OBJECTS 5

//
// Define a reasonable size for error messages.
//

#define CK_MAX_ERROR_MESSAGE (CK_MAX_NAME + 128)

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

    CkOpConstruct - Creates a new instance of a class. The class object is
        popped from the top of the stack, and the uninitialized new instance
        is pushed in its place.

    CkOpForeignConstruct - Creates a new instance of a foreign class. The
        class object is popped from the top of the stack, and the new
        uninitialized instance is pushed in its place.

    CkOpClass - Creates a class type. The superclass type is popped from the
        stack. The name of the new class is then popped from the stack. The
        number of fields in the class is encoded in the next instruction byte.
        The new class type object is then pushed onto the stack.

    CkOpForeignClass - Creates a new foreign class type. The superclass is
        popped from the stack. Then the name is popped from the stack. The new
        class object is pushed as a result.

    CkOpMethod - Defines a new method for the symbol given in the subsequent
        instruction word. The class that will receive the method is popped off
        the stack. Then the function or closure defining the body is popped.

    CkOpStaticMethod - Defines a new static for the symbol given in the
        subsequent instruction word. The class whose metaclass will receive the
        method is popped off the stack. Then the function or closure defining
        the body is popped.

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
    CkOpConstruct,
    CkOpForeignConstruct,
    CkOpClass,
    CkOpForeignClass,
    CkOpMethod,
    CkOpStaticMethod,
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

    FirstObject - Stores a pointer to the first object in the massive singly
        linked list of all living objects. This is the list that the garbage
        collector traverses.

    Gray - Stores a pointer to an array of objects that have been marked as
        not garbage collectable, but whose children and dependencies have not
        been marked.

    GrayCount - Stores the number of elements in the gray array.

    GrayCapacity - Stores the maximum number of elements that can be put in the
        gray array before it must be reallocated.

    WorkingObjects - Stores a fixed stack of objects that should not be
        garbage collected but who are not necessarily linked anywhere else.

    WorkingObjectCount - Stores the number of valid objects in the working
        object stack.

    Handles - Stores the head of the list of handles to objects that are being
        referenced outside of the Chalk VM.

    Compiler - Stores an optional pointer to the current compiler. This link is
        needed so the garbage collector can discover its objects.

    Fiber - Stores a pointer to the currently running fiber.

--*/

struct _CK_VM {
    CK_CONFIGURATION Configuration;
    CK_BUILTIN_CLASSES Class;
    PCK_DICT Modules;
    UINTN BytesAllocated;
    UINTN NextGarbageCollection;
    PCK_OBJECT FirstObject;
    PCK_OBJECT *Gray;
    UINTN GrayCount;
    UINTN GrayCapacity;
    PCK_OBJECT WorkingObjects[CK_MAX_WORKING_OBJECTS];
    ULONG WorkingObjectCount;
    PCK_HANDLE Handles;
    PCK_COMPILER Compiler;
    PCK_FIBER Fiber;
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
    PSTR Name,
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

CK_VALUE
CkpFindModuleVariable (
    PCK_VM Vm,
    PCK_MODULE Module,
    PSTR Name
    );

/*++

Routine Description:

    This routine locates a module level variable with the given name.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Module - Supplies a pointer to the module to search in.

    Name - Supplies a pointer to the name.

Return Value:

    Returns the module variable value on success.

    CK_UNDEFINED_VALUE if the variable does not exist.

--*/

VOID
CkpPushRoot (
    PCK_VM Vm,
    PCK_OBJECT Object
    );

/*++

Routine Description:

    This routine pushes the given object onto a temporary stack to ensure that
    it will not be garbage collected.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Object - Supplies a pointer to the object to push.

Return Value:

    None.

--*/

VOID
CkpPopRoot (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine pops the top working object off of the temporary stack used to
    ensure that certain objects are not garbage collected.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

PVOID
CkpReallocate (
    PCK_VM Vm,
    PVOID Memory,
    UINTN OldSize,
    UINTN NewSize
    );

/*++

Routine Description:

    This routine performs a Chalk dynamic memory operation.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Memory - Supplies an optional pointer to the memory to resize or free.

    OldSize - Supplies the optional previous size of the allocation.

    NewSize - Supplies the new size of the allocation. Set this to 0 to free
        the memory.

Return Value:

    Returns a pointer to the newly allocated or reallocated memory on success.

    NULL on allocation failure or for free operations.

--*/

CK_ERROR_TYPE
CkpInterpret (
    PCK_VM Vm,
    PSTR ModuleName,
    PSTR Source,
    UINTN Length
    );

/*++

Routine Description:

    This routine interprets the given Chalk source string within the context of
    the given module.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ModuleName - Supplies a pointer to the module to interpret the source in.

    Source - Supplies a pointer to the null terminated string containing the
        source to interpret.

    Length - Supplies the length of the source string, not including the null
        terminator.

Return Value:

    Chalk status.

--*/

//
// Module support functions
//

CK_VALUE
CkpModuleLoad (
    PCK_VM Vm,
    CK_VALUE ModuleName
    );

/*++

Routine Description:

    This routine loads the given module.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ModuleName - Supplies the module name value.

Return Value:

    Returns the newly loaded module value on success.

    CK_NULL_VALUE on failure.

--*/

PCK_MODULE
CkpModuleLoadSource (
    PCK_VM Vm,
    CK_VALUE ModuleName,
    PSTR Source,
    UINTN Length
    );

/*++

Routine Description:

    This routine loads the given source under the given module name.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ModuleName - Supplies the module name value.

    Source - Supplies a pointer to the null terminated string containing the
        source to interpret.

    Length - Supplies the length of the source string, not including the null
        terminator.

Return Value:

    Returns a pointer to the newly loaded module on success.

    NULL on failure.

--*/

PCK_MODULE
CkpModuleCreate (
    PCK_VM Vm,
    PCK_STRING_OBJECT Name
    );

/*++

Routine Description:

    This routine creates a new module object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Name - Supplies a pointer to the module name.

Return Value:

    Returns a pointer to the newly allocated module on success.

    NULL on allocation failure.

--*/
