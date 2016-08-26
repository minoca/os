/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    chalk.h

Abstract:

    This header contains definitions for the Chalk scripting language.

Author:

    Evan Green 26-May-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

#define CK_ASSERT(_Condition) assert(_Condition)

//
// This macro pops the top value off the stack and discards it.
//

#define CkStackPop(_Vm) CkStackRemove((_Vm), -1)

//
// These macros evaluate to non-zero if the value at the given stack index is
// of the named type.
//

#define CkIsNull(_Vm, _StackIndex) \
    (CkGetType((_Vm), (_StackIndex)) == CkTypeNull)

#define CkIsInteger(_Vm, _StackIndex) \
    (CkGetType((_Vm), (_StackIndex)) == CkTypeInteger)

#define CkIsString(_Vm, _StackIndex) \
    (CkGetType((_Vm), (_StackIndex)) == CkTypeString)

#define CkIsDict(_Vm, _StackIndex) \
    (CkGetType((_Vm), (_StackIndex)) == CkTypeDict)

#define CkIsList(_Vm, _StackIndex) \
    (CkGetType((_Vm), (_StackIndex)) == CkTypeList)

#define CkIsFunction(_Vm, _StackIndex) \
    (CkGetType((_Vm), (_StackIndex)) == CkTypeFunction)

#define CkIsObject(_Vm, _StackIndex) \
    (CkGetType((_Vm), (_StackIndex)) == CkTypeObject)

//
// ---------------------------------------------------------------- Definitions
//

#define CK_API

#define CHALK_VERSION_MAJOR 1
#define CHALK_VERSION_MINOR 0
#define CHALK_VERSION_REVISION 0

#define CHALK_VERSION ((CHALK_VERSION_MAJOR << 24) | \
                       (CHALK_VERSION_MINOR << 16) | \
                       CHALK_VERSION_REVISION)

#define CK_SOURCE_EXTENSION ".ck"
#define CK_MODULE_ENTRY_NAME "CkModuleInit"

//
// Define Chalk configuration flags.
//

//
// Define this flag to perform a garbage collection after every allocation.
//

#define CK_CONFIGURATION_GC_STRESS 0x00000001

//
// Define this flag to print the bytecode for all compiled functions.
//

#define CK_CONFIGURATION_DEBUG_COMPILER 0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _CK_ERROR_TYPE {
    CkSuccess,
    CkErrorNoMemory,
    CkErrorCompile,
    CkErrorRuntime,
    CkErrorStackTrace
} CK_ERROR_TYPE, *PCK_ERROR_TYPE;

typedef enum _CK_LOAD_MODULE_RESULT {
    CkLoadModuleSource,
    CkLoadModuleForeign,
    CkLoadModuleNotFound,
    CkLoadModuleNoMemory,
    CkLoadModuleNotSupported,
    CkLoadModuleStaticError,
    CkLoadModuleFreeError
} CK_LOAD_MODULE_RESULT, *PCK_LOAD_MODULE_RESULT;

//
// Define the data types to the C API.
//

typedef enum _CK_API_TYPE {
    CkTypeInvalid,
    CkTypeNull,
    CkTypeInteger,
    CkTypeString,
    CkTypeDict,
    CkTypeList,
    CkTypeFunction,
    CkTypeObject,
    CkTypeCount
} CK_API_TYPE, *PCK_API_TYPE;

typedef struct _CK_VM CK_VM, *PCK_VM;
typedef LONGLONG CK_INTEGER;

typedef
PVOID
(*PCK_REALLOCATE) (
    PVOID Allocation,
    UINTN NewSize
    );

/*++

Routine Description:

    This routine represents the prototype of the function called when Chalk
    needs to allocate, reallocate, or free memory.

Arguments:

    Allocation - Supplies an optional pointer to the allocation to resize or
        free. If NULL, then this routine will allocate new memory.

    NewSize - Supplies the size of the desired allocation. If this is 0 and the
        allocation parameter is non-null, the given allocation will be freed.
        Otherwise it will be resized to requested size.

Return Value:

    Returns a pointer to the allocation on success.

    NULL on allocation failure, or in the case the memory is being freed.

--*/

typedef
VOID
(*PCK_FOREIGN_FUNCTION) (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine represents the prototype of a Chalk function implemented in C.
    It is the function call interface between Chalk and C.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None. The return value of the function should be in the first stack slot.

--*/

typedef
VOID
(*PCK_DESTROY_DATA) (
    PVOID Data
    );

/*++

Routine Description:

    This routine is called to destroy a foreign data object previously created.

Arguments:

    Data - Supplies a pointer to the class context to destroy.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure stores a loaded module in the form of source code.

Members:

    Path - Stores a pointer to the full path of the file containing the source.
        The VM will free this memory when finished with this memory.

    PathLength - Stores the length of the full path, not including the null
        terminator.

    Text - Stores a pointer to the heap allocated source for the module. The
        VM will call its free function when it's through with this memory.

    Length - Stores the size of the source in bytes, not including a null
        terminator that is expected to be at the end.

--*/

typedef struct _CK_MODULE_SOURCE {
    PSTR Path;
    UINTN PathLength;
    PSTR Text;
    UINTN Length;
} CK_MODULE_SOURCE, *PCK_MODULE_SOURCE;

/*++

Structure Description:

    This structure stores a loaded foreign module.

Members:

    Path - Stores a pointer to the full path of the file containing the library.
        The VM will free this memory when finished with this memory.

    PathLength - Stores the length of the full path, not including the null
        terminator.

    Handle - Stores a context pointer often used to store the dynamic library
        handle.

    Entry - Stores a pointer to a function used to load the module. More
        precisely, it is the foreign function called when the module's fiber is
        run. It will be called with a single argument, the module object.

--*/

typedef struct _CK_FOREIGN_MODULE {
    PSTR Path;
    UINTN PathLength;
    PVOID Handle;
    PCK_FOREIGN_FUNCTION Entry;
} CK_FOREIGN_MODULE, *PCK_FOREIGN_MODULE;

/*++

Union Description:

    This union stores the data resulting from an attempt to load a module.

Members:

    Source - Stores the loaded module in source form.

    Foreign - Stores the loaded foreign module.

    Error - Stores a pointer to an error string describing why the module could
        not be loaded. If the error type is static, this string will not be
        freed. Otherwise it will be.

--*/

typedef union _CK_MODULE_HANDLE {
    CK_MODULE_SOURCE Source;
    CK_FOREIGN_MODULE Foreign;
    PSTR Error;
} CK_MODULE_HANDLE, *PCK_MODULE_HANDLE;

typedef
CK_LOAD_MODULE_RESULT
(*PCK_LOAD_MODULE) (
    PCK_VM Vm,
    PCSTR ModulePath,
    PCK_MODULE_HANDLE ModuleData
    );

/*++

Routine Description:

    This routine is called to load a new Chalk module.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ModulePath - Supplies a pointer to the module path to load. Directories
        will be separated with dots.

    ModuleData - Supplies a pointer where the loaded module information will
        be returned on success.

Return Value:

    Returns a load module error code.

--*/

typedef
VOID
(*PCK_WRITE) (
    PCK_VM Vm,
    PSTR String
    );

/*++

Routine Description:

    This routine is called to print text in Chalk.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    String - Supplies a pointer to the string to print. This routine should not
        modify or free this string.

Return Value:

    None.

--*/

typedef
VOID
(*PCK_ERROR) (
    PCK_VM Vm,
    CK_ERROR_TYPE ErrorType,
    PSTR Module,
    INT Line,
    PSTR Message
    );

/*++

Routine Description:

    This routine when the Chalk interpreter experiences an error.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ErrorType - Supplies the type of error occurring.

    Module - Supplies a pointer to the module the error occurred in.

    Line - Supplies the line number the error occurred on.

    Message - Supplies a pointer to a string describing the error.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure encapsulates the configuration of the Chalk virtual machine.

Members:

    Reallocate - Stores a pointer to a function used to allocate, reallocate,
        and free memory.

    LoadModule - Stores an optional pointer to a function used to load a Chalk
        module.

    UnloadForeignModule - Stores an optional pointer to a function called when
        a foreign module is being destroyed.

    Write - Stores an optional pointer to a function used to write output to
        the console. If this is NULL, output is simply discarded.

    Error - Stores a pointer to a function used to report errors. If NULL,
        errors are not reported.

    InitialHeapSize - Stores the number of bytes to allocate before triggering
        a garbage collection.

    MinimumHeapSize - Stores the minimum size of heap, used to keep garbage
        collections from occurring too frequently.

    HeapGrowthPercent - Stores the percentage the heap has to grow to trigger
        another garbage collection. Rather than expressing this as a number
        over 100, it's expressed as a number over 1024 to avoid the divide.
        So 50% would be 512 for instance.

    Flags - Stores a bitfield of flags governing the operation of the
        interpreter See CK_CONFIGURATION_* definitions.

--*/

typedef struct _CK_CONFIGURATION {
    PCK_REALLOCATE Reallocate;
    PCK_LOAD_MODULE LoadModule;
    PCK_DESTROY_DATA UnloadForeignModule;
    PCK_WRITE Write;
    PCK_ERROR Error;
    UINTN InitialHeapSize;
    UINTN MinimumHeapSize;
    ULONG HeapGrowthPercent;
    ULONG Flags;
} CK_CONFIGURATION, *PCK_CONFIGURATION;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

CK_API
VOID
CkInitializeConfiguration (
    PCK_CONFIGURATION Configuration
    );

/*++

Routine Description:

    This routine initializes a Chalk configuration with its default values.

Arguments:

    Configuration - Supplies a pointer where the initialized configuration will
        be returned.

Return Value:

    None.

--*/

CK_API
PCK_VM
CkCreateVm (
    PCK_CONFIGURATION Configuration
    );

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

CK_API
VOID
CkDestroyVm (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine destroys a Chalk virtual machine.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

CK_API
CK_ERROR_TYPE
CkInterpret (
    PCK_VM Vm,
    PSTR Source,
    UINTN Length
    );

/*++

Routine Description:

    This routine interprets the given Chalk source string within the context of
    the "main" module.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Source - Supplies a pointer to the null terminated string containing the
        source to interpret.

    Length - Supplies the length of the source string, not including the null
        terminator.

Return Value:

    Chalk status.

--*/

CK_API
VOID
CkCollectGarbage (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine performs garbage collection on the given Chalk instance,
    freeing up unused dynamic memory as appropriate.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Returns a pointer to the newly allocated or reallocated memory on success.

    NULL on allocation failure or for free operations.

--*/

CK_API
BOOL
CkPreloadForeignModule (
    PCK_VM Vm,
    PSTR ModuleName,
    PSTR Path,
    PVOID Handle,
    PCK_FOREIGN_FUNCTION LoadFunction
    );

/*++

Routine Description:

    This routine registers the availability of a foreign module that might not
    otherwise be reachable via the standard module load methods. This is often
    used for adding specialized modules in an embedded interpreter. The load
    function isn't called until someone actually imports the module from the
    interpreter. The loaded module is pushed onto the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ModuleName - Supplies a pointer to the full "dotted.module.name". A copy of
        this memory will be made.

    Path - Supplies an optional pointer to the full path of the module. A copy
        of this memory will be made.

    Handle - Supplies an optional pointer to a handle (usually a dynamic
        library handle) that is used if the module is unloaded.

    LoadFunction - Supplies a pointer to a C function to call to load the
        module symbols. The function will be called on a new fiber, with the
        module itself in slot zero.

Return Value:

    TRUE on success.

    FALSE on failure (usually allocation failure).

--*/

CK_API
UINTN
CkGetStackSize (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine gets the current stack size.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Returns the number of stack slots available to the C API.

--*/

CK_API
BOOL
CkEnsureStack (
    PCK_VM Vm,
    UINTN Size
    );

/*++

Routine Description:

    This routine ensures that there are at least the given number of
    stack slots currently available for the C API.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Size - Supplies the number of additional stack slots needed by the C API.

Return Value:

    TRUE on success.

    FALSE on allocation failure.

--*/

CK_API
VOID
CkPushValue (
    PCK_VM Vm,
    INTN StackIndex
    );

/*++

Routine Description:

    This routine pushes a value already on the stack to the top of the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the existing value to push.
        Negative values reference stack indices from the end of the stack.

Return Value:

    None.

--*/

CK_API
VOID
CkStackRemove (
    PCK_VM Vm,
    INTN StackIndex
    );

/*++

Routine Description:

    This routine removes a value from the stack, and shifts all the other
    values down.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the value to remove. Negative
        values reference stack indices from the end of the stack.

Return Value:

    None.

--*/

CK_API
VOID
CkStackInsert (
    PCK_VM Vm,
    INTN StackIndex
    );

/*++

Routine Description:

    This routine adds the element at the top of the stack into the given
    stack position, and shifts all remaining elements over.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index location to insert at. Negative
        values reference stack indices from the end of the stack.

Return Value:

    None.

--*/

CK_API
VOID
CkStackReplace (
    PCK_VM Vm,
    INTN StackIndex
    );

/*++

Routine Description:

    This routine pops the value from the top of the stack and replaces the
    value at the given stack index with it.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index to replace with the top of the stack.
        Negative values reference stack indices from the end of the stack. This
        is the stack index before the value is popped.

Return Value:

    None.

--*/

CK_API
CK_API_TYPE
CkGetType (
    PCK_VM Vm,
    INTN StackIndex
    );

/*++

Routine Description:

    This routine returns the type of the value at the given stack index.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the object to query. Negative
        values reference stack indices from the end of the stack.

Return Value:

    Returns the stack type.

--*/

CK_API
VOID
CkPushNull (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine pushes a null value on the top of the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

CK_API
VOID
CkPushInteger (
    PCK_VM Vm,
    CK_INTEGER Integer
    );

/*++

Routine Description:

    This routine pushes an integer value on the top of the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Integer - Supplies the integer to push.

Return Value:

    None.

--*/

CK_API
CK_INTEGER
CkGetInteger (
    PCK_VM Vm,
    INTN StackIndex
    );

/*++

Routine Description:

    This routine returns an integer at the given stack index.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the object to get. Negative
        values reference stack indices from the end of the stack.

Return Value:

    Returns the integer value.

    0 if the value at the stack is not an integer.

--*/

CK_API
VOID
CkPushString (
    PCK_VM Vm,
    PCSTR String,
    UINTN Length
    );

/*++

Routine Description:

    This routine pushes a string value on the top of the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    String - Supplies a pointer to the string data to push. A copy of this
        string will be made.

    Length - Supplies the length of the string in bytes, not including the
        null terminator.

Return Value:

    None.

--*/

CK_API
PCSTR
CkGetString (
    PCK_VM Vm,
    UINTN StackIndex,
    PUINTN Length
    );

/*++

Routine Description:

    This routine returns a string at the given stack index.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the object to get. Negative
        values reference stack indices from the end of the stack.

    Length - Supplies an optional pointer where the length of the string will
        be returned, not including a null terminator. If the value at the stack
        index is not a string, 0 is returned here.

Return Value:

    Returns a pointer to the string. The caller must not modify or free this
    value.

    NULL if the value at the specified stack index is not a string.

--*/

CK_API
VOID
CkPushSubstring (
    PCK_VM Vm,
    INTN StackIndex,
    INTN Start,
    INTN End
    );

/*++

Routine Description:

    This routine creates a new string consisting of a portion of the string
    at the given stack index, and pushes it on the stack. If the value at the
    given stack index is not a string, then an empty string is pushed as the
    result. If either the start or end indices are out of range, they are
    adjusted to be in range.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the string to slice. Negative
        values reference stack indices from the end of the stack.

    Start - Supplies the starting index of the substring, inclusive. Negative
        values reference from the end of the string, with -1 being after the
        last character of the string.

    End - Supplies the ending index of the substring, exclusive. Negative
        values reference from the end of the string, with -1 being after the
        last character of the string.

Return Value:

    None.

--*/

CK_API
VOID
CkStringConcatenate (
    PCK_VM Vm,
    UINTN Count
    );

/*++

Routine Description:

    This routine pops a given number of strings off the stack and concatenates
    them. The resulting string is then pushed on the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Count - Supplies the number of strings to pop off the stack.

Return Value:

    None.

--*/

CK_API
VOID
CkPushDict (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine creates a new empty dictionary and pushes it onto the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

CK_API
VOID
CkDictGet (
    PCK_VM Vm,
    INTN StackIndex
    );

/*++

Routine Description:

    This routine pops a key value off the stack, and uses it to get the
    corresponding value for the dictionary stored at the given stack index.
    The resulting value is pushed onto the stack. If no value exists for the
    given key, then null is pushed.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the dictionary (before the key is
        popped off). Negative values reference stack indices from the end of
        the stack.

Return Value:

    Returns the integer value.

    0 if the value at the stack is not an integer.

--*/

CK_API
VOID
CkDictSet (
    PCK_VM Vm,
    INTN StackIndex
    );

/*++

Routine Description:

    This routine pops a key and then a value off the stack, then sets that
    key-value pair in the dictionary at the given stack index.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the dictionary (before anything is
        popped off). Negative values reference stack indices from the end of
        the stack.

Return Value:

    None.

--*/

CK_API
UINTN
CkDictSize (
    PCK_VM Vm,
    INTN StackIndex
    );

/*++

Routine Description:

    This routine returns the size of the dictionary at the given stack index.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the dictionary. Negative values
        reference stack indices from the end of the stack.

Return Value:

    Returns the number of elements in the dictionary.

    0 if the list is empty or the referenced item is not a dictionary.

--*/

CK_API
BOOL
CkDictIterate (
    PCK_VM Vm,
    INTN StackIndex
    );

/*++

Routine Description:

    This routine advances a dictionary iterator at the top of the stack. It
    pushes the next key and then the next value onto the stack, if there are
    more elements in the dictionary. Callers should pull a null values onto
    the stack as the initial iterator before calling this routine for the first
    time. Callers are responsible for popping the value, key, and potentially
    finished iterator off the stack. Callers should not modify a dictionary
    during iteration, as the results are undefined.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the dictionary. Negative values
        reference stack indices from the end of the stack.

Return Value:

    TRUE if the next key and value were pushed on.

    FALSE if there are no more elements, the iterator value is invalid, or the
    item at the given stack index is not a dictionary.

--*/

CK_API
VOID
CkPushList (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine creates a new empty list and pushes it onto the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

CK_API
VOID
CkListGet (
    PCK_VM Vm,
    INTN StackIndex,
    INTN ListIndex
    );

/*++

Routine Description:

    This routine gets the value at the given list index, and pushes it on the
    stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the list. Negative values
        reference stack indices from the end of the stack.

    ListIndex - Supplies the list index to get. If this index is out of bounds,
        the null will be pushed.

Return Value:

    None.

--*/

CK_API
VOID
CkListSet (
    PCK_VM Vm,
    INTN StackIndex,
    INTN ListIndex
    );

/*++

Routine Description:

    This routine pops the top value off the stack, and saves it to a specific
    index in a list.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the list. Negative values
        reference stack indices from the end of the stack.

    ListIndex - Supplies the list index to set. If this index is one beyond the
        end, then the value will be appended. If this index is otherwise out of
        bounds, the item at the top of the stack will simply be discarded.

Return Value:

    None.

--*/

CK_API
UINTN
CkListSize (
    PCK_VM Vm,
    INTN StackIndex
    );

/*++

Routine Description:

    This routine returns the size of the list at the given stack index.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the list. Negative values
        reference stack indices from the end of the stack.

Return Value:

    Returns the number of elements in the list.

    0 if the list is empty or the referenced item is not a list.

--*/

CK_API
VOID
CkPushModulePath (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine pushes the module path onto the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

//
// Higher level support functions
//

CK_API
BOOL
CkCheckArguments (
    PCK_VM Vm,
    UINTN Count,
    ...
    );

/*++

Routine Description:

    This routine validates that the given arguments are of the correct type. If
    any of them are not, it throws a nicely formatted error.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Count - Supplies the number of arguments coming next.

    ... - Supplies the remaining type arguments.

Return Value:

    TRUE if the given arguments match the required type.

    FALSE if an argument is not of the right type. In that case, an error
    will be created.

--*/

CK_API
BOOL
CkCheckArgument (
    PCK_VM Vm,
    INTN StackIndex,
    CK_API_TYPE Type
    );

/*++

Routine Description:

    This routine validates that the given argument is of the correct type. If
    it is not, it throws a nicely formatted error.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index to check. Remember that 1 is the
        first argument index.

    Type - Supplies the type to check.

Return Value:

    TRUE if the given argument matches the required type.

    FALSE if the argument is not of the right type. In that case, an error
    will be created.

--*/

