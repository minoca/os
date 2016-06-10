/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

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

//
// This macro evaluates to the object pointer within a given value.
//

#define CK_AS_OBJECT(_Value) ((_Value).U.Object)
#define CK_AS_INTEGER(_Value) ((_Value).U.Integer)

#define CK_AS_CLASS(_Value) ((PCK_CLASS)CK_AS_OBJECT(_Value))
#define CK_AS_CLOSURE(_Value) ((PCK_CLOSURE)CK_AS_OBJECT(_Value))
#define CK_AS_FIBER(_Value) ((PCK_FIBER)CK_AS_OBJECT(_Value))
#define CK_AS_FUNCTION(_Value) ((PCK_FUNCTION)CK_AS_OBJECT(_Value))
#define CK_AS_INSTANCE(_Value) ((PCK_INSTANCE)CK_AS_OBJECT(_Value))
#define CK_AS_LIST(_Value) ((PCK_LIST)CK_AS_OBJECT(_Value))
#define CK_AS_DICT(_Value) ((PCK_DICT)CK_AS_OBJECT(_Value))
#define CK_AS_MODULE(_Value) ((PCK_MODULE)CK_AS_OBJECT(_Value))
#define CK_AS_RANGE(_Value) ((PCK_RANGE)CK_AS_OBJECT(_Value))
#define CK_AS_STRING(_Value) ((PCK_STRING_OBJECT)CK_AS_OBJECT(_Value))

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
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _CK_CLASS CK_CLASS, *PCK_CLASS;
typedef struct _CK_UPVALUE CK_UPVALUE, *PCK_UPVALUE;
typedef struct _CK_FIBER CK_FIBER, *PCK_FIBER;
typedef struct _CK_VALUE CK_VALUE, *PCK_VALUE;
typedef struct _CK_METHOD CK_METHOD, *PCK_METHOD;
typedef LONG CK_SYMBOL_INDEX, *PCK_SYMBOL_INDEX;
typedef LONG CK_ARITY, *PCK_ARITY;
typedef LONGLONG CK_INTEGER;
typedef PUCHAR PCK_IP;

typedef enum _CK_OBJECT_TYPE {
    CkObjectInvalid,
    CkObjectClass,
    CkObjectClosure,
    CkObjectFiber,
    CkObjectFunction,
    CkObjectForeign,
    CkObjectInstance,
    CkObjectList,
    CkObjectDict,
    CkObjectModule,
    CkObjectRange,
    CkObjectString,
    CkObjectUpvalue
} CK_OBJECT_TYPE, *PCK_OBJECT_TYPE;

typedef enum _CK_VALUE_TYPE {
    CkValueUndefined,
    CkValueNull,
    CkValueInteger,
    CkValueObject
} CK_VALUE_TYPE, *PCK_VALUE_TYPE;

typedef enum _CK_METHOD_TYPE {
    CkMethodInvalid,
    CkMethodPrimitive,
    CkMethodBound,
    CkMethodUnbound,
    CkMethodForeign,
} CK_METHOD_TYPE, *PCK_METHOD_TYPE;

//
// Define some basic array types.
//

/*++

Structure Description:

    This structure encapsulates a raw string (as opposed to a string object
    instance).

Members:

    Data - Stores a pointer to the string data itself.

    Size - Stores the size of the string, not including the null terminator.

--*/

typedef struct _CK_RAW_STRING {
    PSTR Data;
    UINTN Size;
} CK_RAW_STRING, *PCK_RAW_STRING;

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

    This structure defines an array of Chalk strings.

Members:

    Data - Stores a pointer to the array itself.

    Count - Stores the number of elements currently in the array.

    Capacity - Stores the maximum size of the array before it must be
        reallocated.

--*/

typedef struct _CK_STRING_ARRAY {
    PCK_RAW_STRING Data;
    UINTN Count;
    UINTN Capacity;
} CK_STRING_ARRAY, *PCK_STRING_ARRAY;

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

    This structure defines an array of Chalk methods.

Members:

    Data - Stores a pointer to the array itself.

    Count - Stores the number of elements currently in the array.

    Capacity - Stores the maximum size of the array before it must be
        reallocated.

--*/

typedef struct _CK_METHOD_ARRAY {
    PCK_METHOD Data;
    UINTN Count;
    UINTN Capacity;
} CK_METHOD_ARRAY, *PCK_METHOD_ARRAY;

typedef CK_STRING_ARRAY CK_SYMBOL_TABLE, *PCK_SYMBOL_TABLE;

/*++

Structure Description:

    This structure contains the generic Chalk object header.

Members:

    Type - Stores the type of the object, which defines the parent type this
        structure is embedded in.

    Mark - Stores the garbage collection mark. Objects that are visible and
        can't be garbage collected get a mark.

    Next - Stores a pointer to the next object in the global list of all
        objects.

    Class - Stores a pointer to the class this object belongs to.

--*/

struct _CK_OBJECT {
    CK_OBJECT_TYPE Type;
    CHAR Mark;
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

    This structure contains a handle, which is an object that has been handed
    out somewhere outside of the Chalk interpreter.

Members:

    Value - Stores the value that is being referenced somewhere.

    Previous - Stores a pointer to the previous handle in the global list of
        handles.

    Next - Stores a pointer to the next handle in the big list of handles.

--*/

struct _CK_HANDLE {
    CK_VALUE Value;
    PCK_HANDLE Previous;
    PCK_HANDLE Next;
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

typedef struct _CK_STRING_OBJECT {
    CK_OBJECT Header;
    UINTN Length;
    ULONG Hash;
    PSTR Value;
} CK_STRING_OBJECT, *PCK_STRING_OBJECT;

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

    This structure defines a module object.

Members:

    Header - Stores the required object header.

    Variables - Stores the array of module level variables.

    VariableNames - Stores the array of module level variable names.

    Name - Stores a pointer to the string containing the name of the module.

--*/

typedef struct _CK_MODULE {
    CK_OBJECT Header;
    CK_VALUE_ARRAY Variables;
    CK_SYMBOL_TABLE VariableNames;
    PCK_STRING_OBJECT Name;
} CK_MODULE, *PCK_MODULE;

/*++

Structure Description:

    This structure defines debug information about a function object.

Members:

    Name - Stores a pointer to the heap allocated name of the function.

    FirstLine - Stores the first line of the function.

    LineProgram - Stores the line program that converts between line numbers
        and offsets. It's similar to the DWARF line number program, except
        much simpler and more hardcoded.

--*/

typedef struct _CK_FUNCTION_DEBUG {
    PSTR Name;
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

    This structure defines a closure object.

Members:

    Header - Stores the required object header.

    Function - Stores a pointer to the function object.

    Upvalues - Stores a pointer to the array of upvalue objects. Currently this
        points directly after the array since the number of upvalues is static.

--*/

typedef struct _CK_CLOSURE {
    CK_OBJECT Header;
    PCK_FUNCTION Function;
    PCK_UPVALUE *Upvalues;
} CK_CLOSURE, *PCK_CLOSURE;

typedef
BOOL
(*PCK_PRIMITIVE_METHOD) (
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

/*++

Structure Description:

    This structure defines a method.

Members:

    Type - Stores the method type, which indicates which member of the union
        below is valid.

    U - Stores the union of method pointers.

        Primitive - Stores the function pointer for the primitive method.

        Foreign - Stores the function pointer for the foreign method.

        Closure - Stores a pointer to the closure for the regular function.

--*/

struct _CK_METHOD {
    CK_METHOD_TYPE Type;
    union {
        PCK_PRIMITIVE_METHOD Primitive;
        PCK_FOREIGN_FUNCTION Foreign;
        PCK_CLOSURE Closure;
    } U;

};

/*++

Structure Description:

    This structure describes a Chalk class.

Members:

    Header - Stores the required object header.

    Super - Stores a pointer to the superclass.

    FieldCount - Stores the number of fields required for an instance of this
        class, including all of its superclass fields.

    Methods - Stores an array of methods defined by or inherited by this class.
        Methods are called by symbol index, so this array is index by symbol.
        It means some elements of this array may be empty for superclass
        methods this class doesn't implement.

    Name - Stores a pointer to the string object containing the name of the
        class.

--*/

struct _CK_CLASS {
    CK_OBJECT Header;
    PCK_CLASS Super;
    CK_SYMBOL_INDEX FieldCount;
    CK_METHOD_ARRAY Methods;
    PCK_STRING_OBJECT Name;
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

    This structure stores a range.

Members:

    Header - Stores the required object header.

    Type - Stores a value indicating the value types of the from and to unions.
        Currently the only valid value is integer.

    Inclusive - Stores a boolean indicating if the range is inclusive of the to
        value or not.

--*/

typedef struct _CK_RANGE {
    CK_OBJECT Header;
    CK_VALUE_TYPE Type;
    BOOL Inclusive;
    union {
        CK_INTEGER Int;

    } From;

    union {
        CK_INTEGER Int;
    } To;

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

--*/

typedef struct _CK_CALL_FRAME {
    PCK_IP Ip;
    PCK_CLOSURE Closure;
    PCK_VALUE StackStart;
} CK_CALL_FRAME, *PCK_CALL_FRAME;

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

    OpenUpvalues - Stores the head of the singly linked list of upvalues that
        might need to be closed if something goes out of scope. The first
        element is the most recent upvalue.

    Caller - Stores an optional pointer to the calling fiber that caused this
        one to end up on the stack.

    Error - Stores an error value if a runtime error occurred.

--*/

struct _CK_FIBER {
    CK_OBJECT Header;
    PCK_VALUE Stack;
    PCK_VALUE StackTop;
    UINTN StackCapacity;
    PCK_CALL_FRAME Frames;
    UINTN FrameCount;
    UINTN FrameCapacity;
    PCK_UPVALUE OpenUpvalues;
    PCK_FIBER Caller;
    CK_VALUE Error;
};

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
    PCK_FUNCTION Function
    );

/*++

Routine Description:

    This routine creates a new closure object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Function - Supplies a pointer to the function the closure encloses.

Return Value:

    Returns a pointer to the new closure on success.

    NULL on allocation failure.

--*/

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
CkpFunctionSetDebugName (
    PCK_VM Vm,
    PCK_FUNCTION Function,
    PSTR Name,
    UINTN Length
    );

/*++

Routine Description:

    This routine sets the name of the function used when printing stack traces.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Function - Supplies a pointer to the function to set the name of.

    Name - Supplies a pointer to the name of the function. A copy of this
        memory will be made.

    Length - Supplies the length of the function name, not including the null
        terminator.

Return Value:

    None.

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

//
// String functions
//

CK_VALUE
CkpStringCreate (
    PCK_VM Vm,
    PSTR Text,
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
CkpStringCreateFromRange (
    PCK_VM Vm,
    PCK_STRING_OBJECT Source,
    UINTN Start,
    UINTN Count,
    LONG Step
    );

/*++

Routine Description:

    This routine creates a new string object based on a portion of another
    string.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Source - Supplies a pointer to the source string.

    Start - Supplies the starting index to slice from.

    Count - Supplies the number of characters to slice.

    Step - Supplies the whether to increment (1) or decrement (-1).

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
    PCK_STRING_OBJECT Source,
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
CkpStringFind (
    PCK_STRING_OBJECT Haystack,
    PCK_STRING_OBJECT Needle
    );

/*++

Routine Description:

    This routine searches for a given substring within a string.

Arguments:

    Haystack - Supplies a pointer to the string to search.

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

PCK_STRING_OBJECT
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
    PCK_STRING_OBJECT String
    );

/*++

Routine Description:

    This routine hashes a string. The hash value is saved in the string object.

Arguments:

    String - Supplies a pointer to the string.

Return Value:

    None.

--*/
