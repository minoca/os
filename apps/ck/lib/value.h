/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    value.h

Abstract:

    This header contains definitions for the core types within Chalk.

Author:

    Evan Green 28-May-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// These macros evaluates to non-zero if the given value is of the named type.
//

#define CK_IS_OBJECT(_Value) ((_Value).Type == CkValueObject)
#define CK_IS_NULL(_Value) ((_Value).Type == CkValueNull)
#define CK_IS_INTEGER(_Value) ((_Value).Type == CkValueInteger)
#define CK_IS_UNDEFINED(_Value) ((_Value).Type == CkValueUndefined)

#define CK_IS_OBJECT_TYPE(_Value, _Type) \
    ((CK_IS_OBJECT(_Value)) && (CK_AS_OBJECT(_Value)->Type == (_Type)))

#define CK_IS_CLASS(_Value) CK_IS_OBJECT_TYPE(_Value, CkObjectClass)
#define CK_IS_CLOSURE(_Value) CK_IS_OBJECT_TYPE(_Value, CkObjectClosure)
#define CK_IS_FIBER(_Value) CK_IS_OBJECT_TYPE(_Value, CkObjectFiber)
#define CK_IS_FOREIGN(_Value) CK_IS_OBJECT_TYPE(_Value, CkObjectForeign)
#define CK_IS_FUNCTION(_Value) CK_IS_OBJECT_TYPE(_Value, CkObjectFunction)
#define CK_IS_INSTANCE(_Value) CK_IS_OBJECT_TYPE(_Value, CkObjectInstance)
#define CK_IS_LIST(_Value) CK_IS_OBJECT_TYPE(_Value, CkObjectList)
#define CK_IS_DICT(_Value) CK_IS_OBJECT_TYPE(_Value, CkObjectDict)
#define CK_IS_MODULE(_Value) CK_IS_OBJECT_TYPE(_Value, CkObjectModule)
#define CK_IS_RANGE(_Value) CK_IS_OBJECT_TYPE(_Value, CkObjectRange)
#define CK_IS_STRING(_Value) CK_IS_OBJECT_TYPE(_Value, CkObjectString)
#define CK_IS_UPVALUE(_Value) CK_IS_OBJECT_TYPE(_Value, CkObjectUpvalue)

//
// This macro evaluates to the object pointer within a given value.
//

#define CK_AS_OBJECT(_Value) ((_Value).U.Object)
#define CK_AS_INTEGER(_Value) ((_Value).U.Integer)

#define CK_AS_CLASS(_Value) ((PCK_CLASS)CK_AS_OBJECT(_Value))
#define CK_AS_CLOSURE(_Value) ((PCK_CLOSURE)CK_AS_OBJECT(_Value))
#define CK_AS_FIBER(_Value) ((PCK_FIBER)CK_AS_OBJECT(_Value))
#define CK_AS_FOREIGN(_Value) ((PCK_FOREIGN_DATA)CK_AS_OBJECT(_Value))
#define CK_AS_FUNCTION(_Value) ((PCK_FUNCTION)CK_AS_OBJECT(_Value))
#define CK_AS_INSTANCE(_Value) ((PCK_INSTANCE)CK_AS_OBJECT(_Value))
#define CK_AS_LIST(_Value) ((PCK_LIST)CK_AS_OBJECT(_Value))
#define CK_AS_DICT(_Value) ((PCK_DICT)CK_AS_OBJECT(_Value))
#define CK_AS_MODULE(_Value) ((PCK_MODULE)CK_AS_OBJECT(_Value))
#define CK_AS_RANGE(_Value) ((PCK_RANGE)CK_AS_OBJECT(_Value))
#define CK_AS_STRING(_Value) ((PCK_STRING)CK_AS_OBJECT(_Value))
#define CK_AS_UPVALUE(_Value) ((PCK_UPVALUE)CK_AS_OBJECT(_Value))

//
// These macros initialize a value with the given object or primitive.
//

#define CK_OBJECT_VALUE(_Value, _Object)            \
    {                                               \
        (_Value).Type = CkValueObject;              \
        (_Value).U.Object = (PCK_OBJECT)(_Object);  \
    }

#define CK_INT_VALUE(_Value, _Integer)      \
    {                                       \
        (_Value).Type = CkValueInteger;     \
        (_Value).U.Integer = (_Integer);    \
    }

//
// ---------------------------------------------------------------- Definitions
//

//
// Define some core values.
//

#define CK_NULL_VALUE CkNullValue
#define CK_UNDEFINED_VALUE CkUndefinedValue
#define CK_ZERO_VALUE CkZeroValue
#define CK_ONE_VALUE CkOneValue
#define CK_FALSE_VALUE CK_ZERO_VALUE
#define CK_TRUE_VALUE CK_ONE_VALUE

//
// Define the class special behavior flags.
//

#define CK_CLASS_UNINHERITABLE 0x00000001
#define CK_CLASS_SPECIAL_CREATION 0x00000002
#define CK_CLASS_FOREIGN 0x00000004

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _CK_CLASS CK_CLASS, *PCK_CLASS;
typedef struct _CK_FIBER CK_FIBER, *PCK_FIBER;
typedef struct _CK_OBJECT CK_OBJECT, *PCK_OBJECT;
typedef struct _CK_UPVALUE CK_UPVALUE, *PCK_UPVALUE;
typedef struct _CK_MODULE CK_MODULE, *PCK_MODULE;
typedef struct _CK_VALUE CK_VALUE, *PCK_VALUE;

typedef LONG CK_ARITY, *PCK_ARITY;
typedef LONG CK_SYMBOL_INDEX, *PCK_SYMBOL_INDEX;
typedef PUCHAR PCK_IP;

typedef enum _CK_OBJECT_TYPE {
    CkObjectInvalid,
    CkObjectClass,
    CkObjectClosure,
    CkObjectDict,
    CkObjectFiber,
    CkObjectForeign,
    CkObjectFunction,
    CkObjectInstance,
    CkObjectList,
    CkObjectModule,
    CkObjectRange,
    CkObjectString,
    CkObjectUpvalue,
    CkObjectTypeCount
} CK_OBJECT_TYPE, *PCK_OBJECT_TYPE;

typedef enum _CK_VALUE_TYPE {
    CkValueUndefined,
    CkValueNull,
    CkValueInteger,
    CkValueObject
} CK_VALUE_TYPE, *PCK_VALUE_TYPE;

typedef enum _CK_CLOSURE_TYPE {
    CkClosureInvalid,
    CkClosurePrimitive,
    CkClosureBlock,
    CkClosureForeign,
} CK_CLOSURE_TYPE, *PCK_CLOSURE_TYPE;

typedef
BOOL
(*PCK_PRIMITIVE_FUNCTION) (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

/*++

Routine Description:

    This routine describes the prototype of a primitive method: a method that
    is implemented directly in C and interacts intimately with the VM itself.
    Specifically, it may modify the VM stack directly. The return value should
    be placed on the stack manually by this function.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

//
// Define some basic array types.
//

/*++

Structure Description:

    This structure defines an array of integers.

Members:

    Data - Stores a pointer to the array itself.

    Count - Stores the number of elements currently in the array.

    Capacity - Stores the maximum size of the array before it must be
        reallocated.

--*/

typedef struct _CK_INT_ARRAY {
    PLONG Data;
    UINTN Count;
    UINTN Capacity;
} CK_INT_ARRAY, *PCK_INT_ARRAY;

/*++

Structure Description:

    This structure defines an array of bytes.

Members:

    Data - Stores a pointer to the array itself.

    Count - Stores the number of elements currently in the array.

    Capacity - Stores the maximum size of the array before it must be
        reallocated.

--*/

typedef struct _CK_BYTE_ARRAY {
    PUCHAR Data;
    UINTN Count;
    UINTN Capacity;
} CK_BYTE_ARRAY, *PCK_BYTE_ARRAY;

/*++

Structure Description:

    This structure defines an array of Chalk values.

Members:

    Data - Stores a pointer to the array itself.

    Count - Stores the number of elements currently in the array.

    Capacity - Stores the maximum size of the array before it must be
        reallocated.

--*/

typedef struct _CK_VALUE_ARRAY {
    PCK_VALUE Data;
    UINTN Count;
    UINTN Capacity;
} CK_VALUE_ARRAY, *PCK_VALUE_ARRAY;

/*++

Structure Description:

    This structure contains the generic Chalk object header.

Members:

    Type - Stores the type of the object, which defines the parent type this
        structure is embedded in.

    NextKiss - Stores a pointer to the next object in the list of kissed
        objects (objects that will not get garbage collected this time).

    Next - Stores a pointer to the next object in the global list of all
        objects.

    Class - Stores a pointer to the class this object belongs to.

--*/

struct _CK_OBJECT {
    CK_OBJECT_TYPE Type;
    PCK_OBJECT NextKiss;
    PCK_OBJECT Next;
    PCK_CLASS Class;
};

/*++

Structure Description:

    This structure contains the atom of the Chalk core, a generic value.

Members:

    Type - Stores the object type. The really basic types like null and
        integers are encoded directly in the value. Most everything else is a
        pointer to an object.

    U - Stores the union of either the directly encoded value, or the pointer
        to the object.

--*/

struct _CK_VALUE {
    CK_VALUE_TYPE Type;
    union {
        CK_INTEGER Integer;
        PCK_OBJECT Object;
    } U;

};

/*++

Structure Description:

    This structure contains the official object form of a string.

Members:

    Header - Stores the required object header.

    Length - Stores the length of the string, not including the null terminator.

    Hash - Stores the hash of the string.

    Value - Stores a pointer to the actual string value. This currently always
        points immediately after the structure, as strings are immutable and
        so the string plus structure are created in a single allocation.

--*/

typedef struct _CK_STRING {
    CK_OBJECT Header;
    UINTN Length;
    ULONG Hash;
    PCSTR Value;
} CK_STRING, *PCK_STRING;

/*++

Structure Description:

    This structure defines an upvalue object.

Members:

    Header - Stores the required object header.

    Value - Stores a pointer to the variable this upvalue is closing. This may
        either point to the real variable if it is still in scope or the closed
        member of this structure if the variable has gone out of scope.

    Closed - Stores the copy of the value for when the real variable goes out
        of scope.

    Next - Stores a pointer to the next open upvalue in the list of all open
        upvalues in the current fiber.

--*/

struct _CK_UPVALUE {
    CK_OBJECT Header;
    PCK_VALUE Value;
    CK_VALUE Closed;
    PCK_UPVALUE Next;
};

/*++

Structure Description:

    This structure contains a single dictionary element.

Members:

    Key - Stores the key associated with the value.

    Value - Stores the value in this slot.

--*/

typedef struct _CK_DICT_ENTRY {
    CK_VALUE Key;
    CK_VALUE Value;
} CK_DICT_ENTRY, *PCK_DICT_ENTRY;

/*++

Structure Description:

    This structure encapsulates a hash table dictionary.

Members:

    Header - Stores the required object header.

    Count - Stores the number of values in the dictionary.

    Capacity - Stores the size of the entries array.

    Entries - Stores a pointer to the entries.

--*/

typedef struct _CK_DICT {
    CK_OBJECT Header;
    UINTN Count;
    UINTN Capacity;
    PCK_DICT_ENTRY Entries;
} CK_DICT, *PCK_DICT;

/*++

Structure Description:

    This structure contains a string table, which can be linearly indexed and
    searched quickly.

Members:

    List - Stores the list of entries, which allows for linear indexing.

    Dict - Stores the dictionary of entries, which allows for fast searching.

--*/

typedef struct _CK_STRING_TABLE {
    CK_VALUE_ARRAY List;
    PCK_DICT Dict;
} CK_STRING_TABLE, *PCK_STRING_TABLE;

/*++

Structure Description:

    This structure defines debug information about a function object.

Members:

    Name - Stores a pointer to the name of the function.

    FirstLine - Stores the first line of the function.

    LineProgram - Stores the line program that converts between line numbers
        and offsets. It's similar to the DWARF line number program, except
        much simpler and more hardcoded.

--*/

typedef struct _CK_FUNCTION_DEBUG {
    PCK_STRING Name;
    LONG FirstLine;
    CK_BYTE_ARRAY LineProgram;
} CK_FUNCTION_DEBUG, *PCK_FUNCTION_DEBUG;

/*++

Structure Description:

    This structure defines a function object.

Members:

    Header - Stores the required object header.

    Code - Stores the bytecode for the function.

    Constants - Stores the constants used by the function.

    Module - Stores a pointer to the module the function resides in.

    MaxStack - Stores the number of stack slots used by the function.

    UpvalueCount - Stores the number of upvalues closed over by the function.

    Arity - Stores the number of arguments the function takes.

    Debug - Stores a pointer to the debug information, which translates
        bytecode back to line numbers.

--*/

typedef struct _CK_FUNCTION {
    CK_OBJECT Header;
    CK_BYTE_ARRAY Code;
    CK_VALUE_ARRAY Constants;
    PCK_MODULE Module;
    CK_SYMBOL_INDEX MaxStack;
    CK_SYMBOL_INDEX UpvalueCount;
    CK_ARITY Arity;
    CK_FUNCTION_DEBUG Debug;
} CK_FUNCTION, *PCK_FUNCTION;

/*++

Structure Description:

    This structure defines a closure formed by compiled code.

Members:

    Function - Stores a pointer to the compiled code.

--*/

typedef struct _CK_BLOCK_CLOSURE {
    PCK_FUNCTION Function;
} CK_BLOCK_CLOSURE, *PCK_BLOCK_CLOSURE;

/*++

Structure Description:

    This structure defines a closure formed by a builtin primitive function.

Members:

    Function - Stores a pointer to the primitive function to call.

    Arity - Stores the number of arguments the primitive takes.

    Name - Stores a pointer to the name of the function.

--*/

typedef struct _CK_PRIMITIVE_CLOSURE {
    PCK_PRIMITIVE_FUNCTION Function;
    CK_ARITY Arity;
    PCK_STRING Name;
} CK_PRIMITIVE_CLOSURE, *PCK_PRIMITIVE_CLOSURE;

/*++

Structure Description:

    This structure defines a closure formed by a C function.

Members:

    Function - Stores a pointer to the primitive function to call.

    Arity - Stores the number of arguments the primitive takes.

    Name - Stores a pointer to the name of the function.

    Module - Stores a pointer to the module the function is defined in.

--*/

typedef struct _CK_FOREIGN_CLOSURE {
    PCK_FOREIGN_FUNCTION Function;
    CK_ARITY Arity;
    PCK_STRING Name;
    PCK_MODULE Module;
} CK_FOREIGN_CLOSURE, *PCK_FOREIGN_CLOSURE;

/*++

Structure Description:

    This union defines the different closure types.

Members:

    Function - Stores a pointer to the primitive function to call.

    Arity - Stores the number of arguments the primitive takes.

    Name - Stores a pointer to the name of the function.

--*/

typedef union _CK_CLOSURE_UNION {
    CK_BLOCK_CLOSURE Block;
    CK_PRIMITIVE_CLOSURE Primitive;
    CK_FOREIGN_CLOSURE Foreign;
} CK_CLOSURE_UNION, *PCK_CLOSURE_UNION;

/*++

Structure Description:

    This structure defines a closure object.

Members:

    Header - Stores the required object header.

    Type - Stores the closure type, defining which union member is valid.

    U - Stores the closure function information, which depends on the type.

    Class - Stores a pointer to the class the closure is bound to.

    Upvalues - Stores a pointer to the array of upvalue objects. Currently this
        points directly after the array since the number of upvalues is static.

--*/

typedef struct _CK_CLOSURE {
    CK_OBJECT Header;
    CK_CLOSURE_TYPE Type;
    CK_CLOSURE_UNION U;
    PCK_CLASS Class;
    PCK_UPVALUE *Upvalues;
} CK_CLOSURE, *PCK_CLOSURE;

/*++

Structure Description:

    This structure defines a module object.

Members:

    Header - Stores the required object header.

    Variables - Stores the array of module level variables.

    VariableNames - Stores the array of module level symbol names.

    Strings - Stores the table of string constants in the module.

    Name - Stores a pointer to the string containing the name of the module.

    Path - Stores an optional pointer to the string containing the full path
        to the module.

    Handle - Stores a pointer to the dynamic library handle if this is a
        foreign module.

    Closure - Stores a pointer to the main closure of the module.

    Run - Stores a boolean indicating whether or not the module closure has
        been run.

    CompiledVariableCount - Stores the number of module level variables at the
        time the module was compiled.

--*/

struct _CK_MODULE {
    CK_OBJECT Header;
    CK_VALUE_ARRAY Variables;
    CK_STRING_TABLE VariableNames;
    CK_STRING_TABLE Strings;
    PCK_STRING Name;
    PCK_STRING Path;
    PVOID Handle;
    PCK_CLOSURE Closure;
    BOOL Run;
    UINTN CompiledVariableCount;
};

/*++

Structure Description:

    This structure describes a Chalk class.

Members:

    Header - Stores the required object header.

    Super - Stores a pointer to the superclass.

    SuperFieldCount - Stores the cumulative number of fields in the hierarchy
        of super classes up to the root. This is set to -1 if the class cannot
        be inherited from (as one of the core builtin classes).

    FieldCount - Stores the number of fields required for an instance of this
        class, including all of its superclass fields. Stores -1 if this is a
        builtin class.

    Methods - Stores a dictionary of methods defined by or inherited by this
        class. The keys are the signature strings, and the values are method
        objects.

    Name - Stores a pointer to the string object containing the name of the
        class.

    Module - Stores a pointer to the module the class is defined in.

    Flags - Stores flags describing special behaviors of this class. See
        CK_CLASS_* definitions.

--*/

struct _CK_CLASS {
    CK_OBJECT Header;
    PCK_CLASS Super;
    CK_SYMBOL_INDEX SuperFieldCount;
    CK_SYMBOL_INDEX FieldCount;
    PCK_DICT Methods;
    PCK_STRING Name;
    PCK_MODULE Module;
    ULONG Flags;
};

/*++

Structure Description:

    This structure stores a class instance.

Members:

    Header - Stores the required object header.

    Fields - Stores a pointer to the array of fields for this instance. This
        currently points immediately after this structure.

--*/

typedef struct _CK_INSTANCE {
    CK_OBJECT Header;
    PCK_VALUE Fields;
} CK_INSTANCE, *PCK_INSTANCE;

/*++

Structure Description:

    This structure stores a list object.

Members:

    Header - Stores the required object header.

    Elements - Stores the array of elements.

--*/

typedef struct _CK_LIST {
    CK_OBJECT Header;
    CK_VALUE_ARRAY Elements;
} CK_LIST, *PCK_LIST;

/*++

Structure Description:

    This structure stores a range.

Members:

    Header - Stores the required object header.

    Inclusive - Stores a boolean indicating if the range is inclusive of the to
        value or not.

    From - Stores the start of the range, inclusive.

    To - Stores the end of the range, which may be inclusive or exclusive.

--*/

typedef struct _CK_RANGE {
    CK_OBJECT Header;
    BOOL Inclusive;
    CK_INTEGER From;
    CK_INTEGER To;
} CK_RANGE, *PCK_RANGE;

/*++

Structure Description:

    This structure stores the state for a function call frame in the Chalk
    interpreter.

Members:

    Ip - Stores a pointer to the next instruction to be executed in the
        bytecode.

    Closure - Stores a pointer to the closure/function being executed.

    StackStart - Stores a pointer to the base of the stack for this call frame.
        The first element is the receiver, followed by the function parameters,
        followed by the local variables.

    TryCount - Stores the try stack size upon invocation of this frame. This is
        used during function return to quickly pop all open try blocks off.

--*/

typedef struct _CK_CALL_FRAME {
    PCK_IP Ip;
    PCK_CLOSURE Closure;
    PCK_VALUE StackStart;
    UINTN TryCount;
} CK_CALL_FRAME, *PCK_CALL_FRAME;

/*++

Structure Description:

    This structure stores the state for an instantiated try-except scope.

Members:

    Ip - Stores a pointer to the first instruction of the first except clause.

    Stack - Stores a pointer to the stack value to set if jumping to this
        except clause.

    FrameCount - Stores the call frame count at the time this try block was
        entered.

--*/

typedef struct _CK_TRY_BLOCK {
    PCK_IP Ip;
    PCK_VALUE Stack;
    UINTN FrameCount;
} CK_TRY_BLOCK, *PCK_TRY_BLOCK;

/*++

Structure Description:

    This structure stores the context for a line of execution within the
    Chalk virtual machine. Cooperatively, there can be multiple lines of
    execution transferring back and forth to each other.

Members:

    Header - Stores the mandatory object header.

    Stack - Stores a pointer to the base of the stack.

    StackTop - Stores a pointer to the next stack slot to be used on a push.
        That is, one beyond the last valid stack slot.

    StackCapacity - Stores the number of elements that can be stored on the
        stack before it must be reallocated.

    Frames - Stores the stack of call frames.

    FrameCount - Stores the number of elements currently on the call frame
        stack.

    FrameCapacity - Stores the maximum number of elements that can be stored on
        the call frame stack before it will need to be reallocated.

    TryStack - Stores the start of the stack of executing try blocks.

    TryCount - Stores the number of open try blocks in this fiber.

    TryCapacity - Stores the number of blocks in the try block stack before the
        array will need to be reallocated.

    OpenUpvalues - Stores the head of the singly linked list of upvalues that
        might need to be closed if something goes out of scope. The first
        element is the most recent upvalue.

    Caller - Stores an optional pointer to the calling fiber that caused this
        one to end up on the stack.

    Error - Stores an error value if a runtime error occurred.

    ForeignCalls - Stores the number of foreign calls happening in the current
        fiber. The current fiber cannot transfer or yield if there are foreign
        calls happening.

--*/

struct _CK_FIBER {
    CK_OBJECT Header;
    PCK_VALUE Stack;
    PCK_VALUE StackTop;
    UINTN StackCapacity;
    PCK_CALL_FRAME Frames;
    UINTN FrameCount;
    UINTN FrameCapacity;
    PCK_TRY_BLOCK TryStack;
    UINTN TryCount;
    UINTN TryCapacity;
    PCK_UPVALUE OpenUpvalues;
    PCK_FIBER Caller;
    CK_VALUE Error;
    LONG ForeignCalls;
};

/*++

Structure Description:

    This structure stores a foreign data object. This object type encapsulates
    an opaque pointer whose destriction is wired up to the garbage collector.

Members:

    Header - Stores the required object header.

    Data - Stores the opaque data pointer.

    Destroy - Stores a pointer to a function called when the object is garbage
        collected.

--*/

typedef struct _CK_FOREIGN_DATA {
    CK_OBJECT Header;
    PVOID Data;
    PCK_DESTROY_DATA Destroy;
} CK_FOREIGN_DATA, *PCK_FOREIGN_DATA;

//
// -------------------------------------------------------------------- Globals
//

extern const CK_VALUE CkNullValue;
extern const CK_VALUE CkUndefinedValue;
extern const CK_VALUE CkZeroValue;
extern const CK_VALUE CkOneValue;

//
// -------------------------------------------------------- Function Prototypes
//

//
// Miscellaneous value functions
//

PCK_CLOSURE
CkpClosureCreate (
    PCK_VM Vm,
    PCK_FUNCTION Function,
    PCK_CLASS Class
    );

/*++

Routine Description:

    This routine creates a new closure object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Function - Supplies a pointer to the function the closure encloses.

    Class - Supplies a pointer to the class the closure was defined in.

Return Value:

    Returns a pointer to the new closure on success.

    NULL on allocation failure.

--*/

PCK_CLOSURE
CkpClosureCreatePrimitive (
    PCK_VM Vm,
    PCK_PRIMITIVE_FUNCTION Function,
    PCK_CLASS Class,
    PCK_STRING Name,
    CK_ARITY Arity
    );

/*++

Routine Description:

    This routine creates a new closure object for a primitive function.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Function - Supplies a pointer to the primitive C function.

    Class - Supplies a pointer to the class the closure was defined in.

    Name - Supplies a pointer to the function name string.

    Arity - Supplies the function arity.

Return Value:

    Returns a pointer to the new closure on success.

    NULL on allocation failure.

--*/

PCK_CLOSURE
CkpClosureCreateForeign (
    PCK_VM Vm,
    PCK_FOREIGN_FUNCTION Function,
    PCK_MODULE Module,
    PCK_STRING Name,
    CK_ARITY Arity
    );

/*++

Routine Description:

    This routine creates a new closure object for a foreign function.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Function - Supplies a pointer to the foreign C function pointer.

    Module - Supplies a pointer to the module the function was defined in.

    Name - Supplies a pointer to the function name string.

    Arity - Supplies the function arity.

Return Value:

    Returns a pointer to the new closure on success.

    NULL on allocation failure.

--*/

PCK_FUNCTION
CkpFunctionCreate (
    PCK_VM Vm,
    PCK_MODULE Module,
    CK_SYMBOL_INDEX StackSize
    );

/*++

Routine Description:

    This routine creates a new function object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Module - Supplies a pointer to the module the function is in.

    StackSize - Supplies the number of stack slots used by the function.

Return Value:

    Returns a pointer to the new function on success.

    NULL on allocation failure.

--*/

VOID
CkpDestroyObject (
    PCK_VM Vm,
    PCK_OBJECT Object
    );

/*++

Routine Description:

    This routine destroys a Chalk object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Object - Supplies a pointer to the object to destroy.

Return Value:

    None.

--*/

VOID
CkpInitializeObject (
    PCK_VM Vm,
    PCK_OBJECT Object,
    CK_OBJECT_TYPE Type,
    PCK_CLASS Class
    );

/*++

Routine Description:

    This routine initializes a new Chalk object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Object - Supplies a pointer to the object to initialize.

    Type - Supplies the type of object being created.

    Class - Supplies a pointer to the object class.

Return Value:

    None.

--*/

BOOL
CkpAreValuesEqual (
    CK_VALUE Left,
    CK_VALUE Right
    );

/*++

Routine Description:

    This routine determines if two objects are equal.

Arguments:

    Left - Supplies the left value.

    Right - Supplies the right value.

Return Value:

    TRUE if the values are equal.

    FALSE if the values are not equal.

--*/

BOOL
CkpAreValuesIdentical (
    CK_VALUE Left,
    CK_VALUE Right
    );

/*++

Routine Description:

    This routine determines if two objects are strictly the same.

Arguments:

    Left - Supplies the left value.

    Right - Supplies the right value.

Return Value:

    TRUE if the values are equal.

    FALSE if the values are not equal.

--*/

BOOL
CkpGetValueBoolean (
    CK_VALUE Value
    );

/*++

Routine Description:

    This routine determines if the given value "is" or "isn't".

Arguments:

    Value - Supplies the value to evaluate.

Return Value:

    FALSE if the value is undefined, Null, or zero.

    TRUE otherwise.

--*/

PCK_CLASS
CkpGetClass (
    PCK_VM Vm,
    CK_VALUE Value
    );

/*++

Routine Description:

    This routine returns the class of the given value.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Value - Supplies a pointer to the class object.

Return Value:

    NULL for the undefined value.

    Returns a pointer to the class for all other values.

--*/

PCK_CLASS
CkpClassAllocate (
    PCK_VM Vm,
    PCK_MODULE Module,
    CK_SYMBOL_INDEX FieldCount,
    PCK_STRING Name
    );

/*++

Routine Description:

    This routine allocates a new class object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Module - Supplies a pointer to the module to define the class in.

    FieldCount - Supplies the number of fields the class has.

    Name - Supplies a pointer to the name of the class.

Return Value:

    Returns a pointer to the newly allocated class object on success.

    NULL on allocation failure.

--*/

VOID
CkpBindMethod (
    PCK_VM Vm,
    PCK_CLASS Class,
    CK_VALUE Signature,
    PCK_CLOSURE Closure
    );

/*++

Routine Description:

    This routine binds a method to a class.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Class - Supplies a pointer to the class to bind the method to.

    Signature - Supplies a name string value of the function signature to bind
        to the class.

    Closure - Supplies a pointer to the closure to bind.

Return Value:

    None.

--*/

VOID
CkpBindSuperclass (
    PCK_VM Vm,
    PCK_CLASS Class,
    PCK_CLASS Super
    );

/*++

Routine Description:

    This routine binds a class to its superclass.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Class - Supplies a pointer to the subclass.

    Super - Supplies a pointer to the superclass.

Return Value:

    None.

--*/

CK_VALUE
CkpCreateInstance (
    PCK_VM Vm,
    PCK_CLASS Class
    );

/*++

Routine Description:

    This routine creates a new class instance.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Class - Supplies a pointer to the class.

Return Value:

    Returns an instance of the given class on success.

--*/

//
// Dictionary functions
//

PCK_DICT
CkpDictCreate (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine creates a new dictionary.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Returns a pointer to the new dictionary on success.

    NULL on allocation failure.

--*/

CK_VALUE
CkpDictGet (
    PCK_DICT Dict,
    CK_VALUE Key
    );

/*++

Routine Description:

    This routine finds an entry in the given dictionary.

Arguments:

    Dict - Supplies a pointer to the dictionary object.

    Key - Supplies the key to look up on.

Return Value:

    Returns the value at the given key on success.

    CK_UNDEFINED_VALUE if no entry exists in the dictionary for the given key.

--*/

VOID
CkpDictSet (
    PCK_VM Vm,
    PCK_DICT Dict,
    CK_VALUE Key,
    CK_VALUE Value
    );

/*++

Routine Description:

    This routine sets the value for the given key in a dictionary.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Dict - Supplies a pointer to the dictionary object.

    Key - Supplies the key to set the value for.

    Value - Supplies the value to set.

Return Value:

    None. On allocation failure the entry is simply not set.

--*/

CK_VALUE
CkpDictRemove (
    PCK_VM Vm,
    PCK_DICT Dict,
    CK_VALUE Key
    );

/*++

Routine Description:

    This routine unsets the value for the given key in a dictionary.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Dict - Supplies a pointer to the dictionary object.

    Key - Supplies the key to unset.

Return Value:

    Returns the old value at the key, or CK_NULL_VALUE if no value existed at
    that key.

--*/

VOID
CkpDictClear (
    PCK_VM Vm,
    PCK_DICT Dict
    );

/*++

Routine Description:

    This routine removes all entries from the given dictionary.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Dict - Supplies a pointer to the dictionary object.

Return Value:

    None.

--*/

VOID
CkpDictCombine (
    PCK_VM Vm,
    PCK_DICT Destination,
    PCK_DICT Source
    );

/*++

Routine Description:

    This routine adds all entries from the source dictionary into the
    destination dictionary, clobbering any existing entries of the same key.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Destination - Supplies a pointer to the dictionary where the entries will
        be copied to.

    Source - Supplies the dictionary to make a copy of.

Return Value:

    None.

--*/

//
// List functions
//

PCK_LIST
CkpListCreate (
    PCK_VM Vm,
    UINTN ElementCount
    );

/*++

Routine Description:

    This routine creates a new list object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ElementCount - Supplies the initial capacity and count to allocate. The
        caller is expected to fill these in with values, as the count is set
        to this number so the elements are live.

Return Value:

    Returns a pointer to the created list on success.

    NULL on allocation failure.

--*/

VOID
CkpListDestroy (
    PCK_VM Vm,
    PCK_LIST List
    );

/*++

Routine Description:

    This routine destroys a list object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    List - Supplies a pointer to the list to destroy.

Return Value:

    None.

--*/

VOID
CkpListInsert (
    PCK_VM Vm,
    PCK_LIST List,
    CK_VALUE Element,
    UINTN Index
    );

/*++

Routine Description:

    This routine inserts an element into the list.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    List - Supplies a pointer to the list to insert into.

    Element - Supplies the element to insert.

    Index - Supplies the index into the list to insert. Valid values are 0 to
        the current size of the list, inclusive.

Return Value:

    None.

--*/

CK_VALUE
CkpListRemoveIndex (
    PCK_VM Vm,
    PCK_LIST List,
    UINTN Index
    );

/*++

Routine Description:

    This routine removes the element at the given index.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    List - Supplies a pointer to the list to operate on.

    Index - Supplies the index into the list to remove. The elements after this
        one will be shifted down. Valid values are between 0 and the number of
        elements in the list, exclusive.

Return Value:

    Returns the element at the index that was removed.

--*/

PCK_LIST
CkpListConcatenate (
    PCK_VM Vm,
    PCK_LIST Destination,
    PCK_LIST Source
    );

/*++

Routine Description:

    This routine concatenates two lists together. It can alternatively be used
    to copy a list.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Destination - Supplies an optional pointer to the list to operate on. If
        this is NULL, then a new list is created.

    Source - Supplies the list to append to the end of the destination list.

Return Value:

    Returns the destination list on success. If no destination list was
    provided, returns a pointer to a new list.

    NULL on allocation failure.

--*/

VOID
CkpListClear (
    PCK_VM Vm,
    PCK_LIST List
    );

/*++

Routine Description:

    This routine resets a list to be empty.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    List - Supplies a pointer to the list to clear.

Return Value:

    None.

--*/

//
// String functions
//

CK_VALUE
CkpStringCreate (
    PCK_VM Vm,
    PCSTR Text,
    UINTN Length
    );

/*++

Routine Description:

    This routine creates a new string object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Text - Supplies a pointer to the value of the string. A copy of this memory
        will be made.

    Length - Supplies the length of the string, not including the null
        terminator.

Return Value:

    Returns the new string value on success.

    CK_NULL_VALUE on allocation failure.

--*/

CK_VALUE
CkpStringCreateFromInteger (
    PCK_VM Vm,
    CK_INTEGER Integer
    );

/*++

Routine Description:

    This routine creates a new string object based on an integer.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Integer - Supplies the integer to convert.

Return Value:

    Returns the new string value on success.

    CK_NULL_VALUE on allocation failure.

--*/

CK_VALUE
CkpStringCreateFromIndex (
    PCK_VM Vm,
    PCK_STRING Source,
    UINTN Index
    );

/*++

Routine Description:

    This routine creates a new string from a single UTF-8 codepoint at the
    given byte index into the source string.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Source - Supplies the source string to index into.

    Index - Supplies the byte index to read from.

Return Value:

    Returns the new string value on success.

    CK_NULL_VALUE on allocation failure.

--*/

CK_VALUE
CkpStringCreateFromCharacter (
    PCK_VM Vm,
    INT Character
    );

/*++

Routine Description:

    This routine creates a new string object based on a UTF-8 codepoint.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Character - Supplies the UTF-8 codepoint to convert.

Return Value:

    Returns the new string value on success.

    CK_NULL_VALUE on allocation failure.

--*/

CK_VALUE
CkpStringFormat (
    PCK_VM Vm,
    PSTR Format,
    ...
    );

/*++

Routine Description:

    This routine creates a new string object based on a formatted string. This
    formatting is much simpler than printf-style formatting. The only format
    specifiers are '$', which specifies a C string, or '@', which specifies a
    string object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Format - Supplies a pointer to the format string.

    ... - Supplies the remainder of the arguments, which depend on the format
        string.

Return Value:

    Returns the new string value on success.

    CK_NULL_VALUE on allocation failure.

--*/

UINTN
CkpStringFindLast (
    PCK_STRING Haystack,
    PCK_STRING Needle
    );

/*++

Routine Description:

    This routine searches for the last instance of a given substring within a
    string.

Arguments:

    Haystack - Supplies a pointer to the string to search.

    Needle - Supplies a pointer to the string to search for.

Return Value:

    Returns the index of the last instance of the needle within the haystack on
    success.

    (UINTN)-1 if the needle could not be found in the haystack.

--*/

UINTN
CkpStringFind (
    PCK_STRING Haystack,
    UINTN Offset,
    PCK_STRING Needle
    );

/*++

Routine Description:

    This routine searches for a given substring within a string.

Arguments:

    Haystack - Supplies a pointer to the string to search.

    Offset - Supplies the offset from within the haystack to begin searching.

    Needle - Supplies a pointer to the string to search for.

Return Value:

    Returns the index of the needle within the haystack on success.

    (UINTN)-1 if the needle could not be found in the haystack.

--*/

INT
CkpUtf8EncodeSize (
    INT Character
    );

/*++

Routine Description:

    This routine returns the number of bytes required to enocde the given
    codepoint.

Arguments:

    Character - Supplies the UTF8 codepoint to get the length for.

Return Value:

    Returns the number of bytes needed to encode that codepoint, or 0 if the
    codepoint is invalid.

--*/

INT
CkpUtf8Encode (
    INT Character,
    PUCHAR Bytes
    );

/*++

Routine Description:

    This routine encodes the given UTF8 character into the given byte stream.

Arguments:

    Character - Supplies the UTF8 codepoint to encode.

    Bytes - Supplies a pointer where the bytes for the given codepoint will be
        returned.

Return Value:

    Returns the number of bytes used to encode that codepoint, or 0 if the
    codepoint is invalid.

--*/

INT
CkpUtf8DecodeSize (
    UCHAR Byte
    );

/*++

Routine Description:

    This routine determines the number of bytes in the given UTF-8 sequence
    given the first byte.

Arguments:

    Byte - Supplies the first byte of the sequence.

Return Value:

    Returns the number of bytes needed to decode that codepoint, or 0 if the
    first byte is not the beginning of a valid UTF-8 sequence.

--*/

INT
CkpUtf8Decode (
    PUCHAR Bytes,
    UINTN Length
    );

/*++

Routine Description:

    This routine decodes the given UTF8 byte sequence into a codepoint.

Arguments:

    Bytes - Supplies a pointer to the byte stream.

    Length - Supplies the remaining length of the bytestream.

Return Value:

    Returns the decoded codepoint on success.

    -1 if the bytestream is invalid.

--*/

PCK_STRING
CkpStringAllocate (
    PCK_VM Vm,
    UINTN Length
    );

/*++

Routine Description:

    This routine allocates a new string object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Length - Supplies the length of the string in bytes, not including the null
        terminator.

Return Value:

    Returns a pointer to the newly allocated string object on success.

    NULL on allocation failure.

--*/

VOID
CkpStringHash (
    PCK_STRING String
    );

/*++

Routine Description:

    This routine hashes a string. The hash value is saved in the string object.

Arguments:

    String - Supplies a pointer to the string.

Return Value:

    None.

--*/

CK_VALUE
CkpStringFake (
    PCK_STRING FakeStringObject,
    PCSTR String,
    UINTN Length
    );

/*++

Routine Description:

    This routine initializes a temporary string object, usually used as a local
    variable in a C function. It's important that this string not get saved
    anywhere that might stick around after this fake string goes out of scope.

Arguments:

    FakeStringObject - Supplies a pointer to the string object storage to
        initialize.

    String - Supplies a pointer to the string to use.

    Length - Supplies the length of the string in bytes, not including the null
        terminator.

Return Value:

    Returns a string value for the fake string.

--*/

//
// Fiber functions
//

PCK_FIBER
CkpFiberCreate (
    PCK_VM Vm,
    PCK_CLOSURE Closure
    );

/*++

Routine Description:

    This routine creates a new fiber object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Closure - Supplies a pointer to the closure to execute.

Return Value:

    Returns a pointer to the new fiber on success.

    NULL on allocation failure.

--*/

VOID
CkpFiberDestroy (
    PCK_VM Vm,
    PCK_FIBER Fiber
    );

/*++

Routine Description:

    This routine destroys a fiber object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Fiber - Supplies a pointer to the fiber to destroy.

Return Value:

    None.

--*/

VOID
CkpAppendCallFrame (
    PCK_VM Vm,
    PCK_FIBER Fiber,
    PCK_CLOSURE Closure,
    PCK_VALUE Stack
    );

/*++

Routine Description:

    This routine adds a new call frame onto the given fiber.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Fiber - Supplies a pointer to the fiber to run on.

    Closure - Supplies a pointer to the closure to execute.

    Stack - Supplies a pointer to the base of the stack. The receiver and
        arguments should already be set up after this pointer.

Return Value:

    None. On allocation failure, the runtime error will be set.

--*/

VOID
CkpPushTryBlock (
    PCK_VM Vm,
    PCK_IP ExceptionHandler
    );

/*++

Routine Description:

    This routine pushes a try block onto the current fiber's try stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ExceptionHandler - Supplies a pointer to the exception handler code.

Return Value:

    None. On allocation failure, the runtime error will be set.

--*/

VOID
CkpEnsureStack (
    PCK_VM Vm,
    PCK_FIBER Fiber,
    UINTN Size
    );

/*++

Routine Description:

    This routine ensures that the stack is at least the given size.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Fiber - Supplies a pointer to the currently executing fiber.

    Size - Supplies the required size of the stack.

Return Value:

    None. The fiber error will be set on failure.

--*/

//
// Integer and range functions
//

CK_VALUE
CkpRangeCreate (
    PCK_VM Vm,
    CK_INTEGER From,
    CK_INTEGER To,
    BOOL Inclusive
    );

/*++

Routine Description:

    This routine creates a range object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    From - Supplies the starting value.

    To - Supplies the ending value.

    Inclusive - Supplies a boolean indicating whether the range is inclusive
        or exclusive.

Return Value:

    Returns the range value on success.

    CK_NULL_VALUE on allocation failure.

--*/

