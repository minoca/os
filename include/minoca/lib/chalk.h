/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

#define CkIsData(_Vm, _StackIndex) \
    (CkGetType((_Vm), (_StackIndex)) == CkTypeData)

//
// ---------------------------------------------------------------- Definitions
//

#ifndef CK_API

#define CK_API __DLLIMPORT

#endif

#define CHALK_VERSION_MAJOR 1
#define CHALK_VERSION_MINOR 0
#define CHALK_VERSION_REVISION 0

#define CHALK_VERSION ((CHALK_VERSION_MAJOR << 24) | \
                       (CHALK_VERSION_MINOR << 16) | \
                       CHALK_VERSION_REVISION)

#define CK_SOURCE_EXTENSION "ck"
#define CK_OBJECT_EXTENSION "cko"
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
// Define the maximum UTF-8 value that can be encoded.
//

#define CK_MAX_UTF8 0x10FFFF

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _CK_ERROR_TYPE {
    CkSuccess,
    CkErrorNoMemory,
    CkErrorCompile,
    CkErrorRuntime
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
    CkTypeData,
    CkTypeCount
} CK_API_TYPE, *PCK_API_TYPE;

typedef struct _CK_VM CK_VM, *PCK_VM;
typedef LONGLONG CK_INTEGER, *PCK_INTEGER;

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
        will be separated with dots. If this contains a slash, then it is an
        absolute path that should be loaded directly.

    ModuleData - Supplies a pointer where the loaded module information will
        be returned on success.

Return Value:

    Returns a load module error code.

--*/

typedef
INT
(*PCK_SAVE_MODULE) (
    PCK_VM Vm,
    PCSTR ModulePath,
    PCSTR FrozenData,
    UINTN FrozenDataSize
    );

/*++

Routine Description:

    This routine is called after a module is compiled, so that the caller can
    save the compilation object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ModulePath - Supplies a pointer to the source file path that was just
        loaded.

    FrozenData - Supplies an opaque binary representation of the compiled
        module. The format of this data is unspecified and may change between
        revisions of the language.

    FrozenDataSize - Supplies the number of bytes in the frozen module data.

Return Value:

    0 on success.

    Returns a non-zero value on failure. Unless being explicitly asked to
    save a compilation, failures are not normally fatal, and so zero should
    almost always be returned.

--*/

typedef
VOID
(*PCK_WRITE) (
    PCK_VM Vm,
    PCSTR String
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
    PSTR Message
    );

/*++

Routine Description:

    This routine when the Chalk interpreter experiences an error.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ErrorType - Supplies the type of error occurring.

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

    SaveModule - Stores an optional pointer to a function used to save the
        compiled representation of a newly loaded module.

    UnloadForeignModule - Stores an optional pointer to a function called when
        a foreign module is being destroyed.

    Write - Stores an optional pointer to a function used to write output to
        the console. If this is NULL, output is simply discarded.

    Error - Stores a pointer to a function used to report errors. If NULL,
        errors are not reported.

    UnhandledException - Stores a pointer to a foreign function to call if an
        unhandled exception occurs. If null, a default function will be
        provided that prints the error.

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
    PCK_SAVE_MODULE SaveModule;
    PCK_DESTROY_DATA UnloadForeignModule;
    PCK_WRITE Write;
    PCK_ERROR Error;
    PCK_FOREIGN_FUNCTION UnhandledException;
    UINTN InitialHeapSize;
    UINTN MinimumHeapSize;
    ULONG HeapGrowthPercent;
    ULONG Flags;
} CK_CONFIGURATION, *PCK_CONFIGURATION;

/*++

Structure Description:

    This structure describes a variable or other data object in Chalk.

Members:

    Type - Stores the type of object to register.

    Name - Stores the name used to access the object in Chalk.

    Value - Stores the value of the object

    Integer - Stores the integer value of the object. For many types, this
        member is ignored.

--*/

typedef struct _CK_VARIABLE_DESCRIPTION {
    CK_API_TYPE Type;
    PSTR Name;
    PVOID Value;
    CK_INTEGER Integer;
} CK_VARIABLE_DESCRIPTION, *PCK_VARIABLE_DESCRIPTION;

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
    PCSTR Path,
    PCSTR Source,
    UINTN Length,
    LONG Line,
    BOOL Interactive
    );

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
PVOID
CkGetContext (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine returns the context pointer stored inside the Chalk VM. This
    pointer is not used at all by Chalk, and can be used by the surrounding
    environment integrating Chalk.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Returns the user context pointer.

--*/

CK_API
PVOID
CkSetContext (
    PCK_VM Vm,
    PVOID NewValue
    );

/*++

Routine Description:

    This routine sets the context pointer stored inside the Chalk VM. This
    pointer is not used at all by Chalk, and can be used by the surrounding
    environment integrating Chalk.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    NewValue - Supplies the new context pointer value to set.

Return Value:

    Returns the previous value.

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
BOOL
CkLoadModule (
    PCK_VM Vm,
    PCSTR ModuleName,
    PCSTR Path
    );

/*++

Routine Description:

    This routine loads (but does not run) the given module, and pushes it on
    the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ModuleName - Supplies a pointer to the full "dotted.module.name". A copy of
        this memory will be made.

    Path - Supplies an optional pointer to the full path of the module. A copy
        of this memory will be made. If this is supplied, then this is the only
        path that is attempted when opening the module. If this is not supplied,
        then the standard load paths will be used. If a module by the given
        name is already loaded, this is ignored.

Return Value:

    TRUE on success.

    FALSE on failure. In this case, an exception will have been thrown. The
    caller should not modify the stack anymore, and should return as soon as
    possible.

--*/

CK_API
UINTN
CkGetStackSize (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine returns the number of elements currently on the stack for the
    current frame.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Returns the number of stack slots occupied by the current frame.

--*/

CK_API
UINTN
CkGetStackRemaining (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine returns the number of free slots remaining on the stack.

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
PVOID
CkPushStringBuffer (
    PCK_VM Vm,
    UINTN MaxLength
    );

/*++

Routine Description:

    This routine creates an uninitialized string and pushes it on the top of
    the stack. The string must be finalized before use in the Chalk environment.
    Once finalized, the string buffer must not be modified.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    MaxLength - Supplies the maximum length of the string buffer, not including
        a null terminator.

Return Value:

    Returns a pointer to the string buffer on success.

    NULL on allocation failure.

--*/

CK_API
VOID
CkFinalizeString (
    PCK_VM Vm,
    INTN StackIndex,
    UINTN Length
    );

/*++

Routine Description:

    This routine finalizes a string that was previously created as a buffer.
    The string must not be modified after finalization.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the string to slice. Negative
        values reference stack indices from the end of the stack.

    Length - Supplies the final length of the string, not including the null
        terminator. This must not be greater than the initial maximum length
        provided when the string buffer was pushed.

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
BOOL
CkDictGet (
    PCK_VM Vm,
    INTN StackIndex
    );

/*++

Routine Description:

    This routine pops a key off the stack, and uses it to get the corresponding
    value for the dictionary stored at the given stack index. The resulting
    value is pushed onto the stack. If no value exists for the given key, then
    nothing is pushed.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the dictionary (before the key is
        popped off). Negative values reference stack indices from the end of
        the stack.

Return Value:

    TRUE if there was a value for that key.

    FALSE if the dictionary has no contents for that value.

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
VOID
CkDictRemove (
    PCK_VM Vm,
    INTN StackIndex
    );

/*++

Routine Description:

    This routine pops a key off the stack, and removes that key and
    corresponding value from the dictionary. No error is raised if the key
    did not previously exist in the dictionary.

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
    more elements in the dictionary. Callers should push a null value onto
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
BOOL
CkPushData (
    PCK_VM Vm,
    PVOID Data,
    PCK_DESTROY_DATA DestroyRoutine
    );

/*++

Routine Description:

    This routine pushes an opaque pointer onto the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Data - Supplies the pointer to encapsulate.

    DestroyRoutine - Supplies an optional pointer to a function to call if this
        value is garbage collected.

Return Value:

    TRUE on success.

    FALSE on allocation failure.

--*/

CK_API
PVOID
CkGetData (
    PCK_VM Vm,
    INTN StackIndex
    );

/*++

Routine Description:

    This routine returns a data pointer that is stored the given stack index.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the object to get. Negative
        values reference stack indices from the end of the stack.

Return Value:

    Returns the opaque pointer passed in when the object was created.

    NULL if the value at the stack was not a foreign data object.

--*/

CK_API
VOID
CkPushClass (
    PCK_VM Vm,
    INTN ModuleIndex,
    ULONG FieldCount
    );

/*++

Routine Description:

    This routine pops a class and a string off the stack, creates a new class,
    and pushes it onto the stack. The popped class is the superclass of the
    new class, and the popped string is the name of the class.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ModuleIndex - Supplies the stack index of the module to create the class in,
        before any items are popped from the stack.

    FieldCount - Supplies the number of fields to allocate for each instance of
        the class. When a new class is created, these fields start out as null.

Return Value:

    None.

--*/

CK_API
VOID
CkPushFunction (
    PCK_VM Vm,
    PCK_FOREIGN_FUNCTION Function,
    PSTR Name,
    ULONG ArgumentCount,
    INTN ModuleIndex
    );

/*++

Routine Description:

    This routine pushes a C function onto the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Function - Supplies a pointer to the C function.

    Name - Supplies a pointer to a null terminated string containing the name
        of the function, used for debugging purposes. This name is not actually
        assigned in the Chalk namespace.

    ArgumentCount - Supplies the number of arguments the function takes, not
        including the receiver slot.

    ModuleIndex - Supplies the index of the module this function should be
        defined within. Functions must be tied to modules to ensure that the
        module containing the C function is not garbage collected and unloaded.

Return Value:

    None.

--*/

CK_API
VOID
CkBindMethod (
    PCK_VM Vm,
    INTN ClassIndex
    );

/*++

Routine Description:

    This routine pops a string and then a function off the stack. It binds the
    function as a class method. The class is indicated by the given stack index
    (before either of the pops). The function may be either a C or Chalk
    function.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ClassIndex - Supplies the stack index of the class to bind the function to.
        Negative values reference stack indices from the end of the stack.

Return Value:

    None.

--*/

CK_API
VOID
CkGetField (
    PCK_VM Vm,
    UINTN FieldIndex
    );

/*++

Routine Description:

    This routine gets the value from the instance field with the given index,
    and pushes it on the stack. This only applies to bound methods, and
    operates on the receiver ("this"). If the current method is not a bound
    method, or the field is out of bounds, null is pushed.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    FieldIndex - Supplies the field index of the instance to get.

Return Value:

    None.

--*/

CK_API
VOID
CkSetField (
    PCK_VM Vm,
    UINTN FieldIndex
    );

/*++

Routine Description:

    This routine pops the top value off the stack, and saves it to a specific
    field index in the function receiver. This function only applies to bound
    methods. If the current function is unbound or the field index is out of
    bounds, the value is popped and discarded.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    FieldIndex - Supplies the field index of the intance to get.

Return Value:

    None.

--*/

CK_API
VOID
CkGetVariable (
    PCK_VM Vm,
    INTN StackIndex,
    PCSTR Name
    );

/*++

Routine Description:

    This routine gets a global variable and pushes it on the stack. If the
    variable does not exist in the given module, or the given stack index is
    not a module, then null is pushed.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the module to look in. Negative
        values reference stack indices from the end of the stack.

    Name - Supplies a pointer to the null terminated string containing the
        name of the variable to get.

Return Value:

    None.

--*/

CK_API
VOID
CkSetVariable (
    PCK_VM Vm,
    INTN StackIndex,
    PCSTR Name
    );

/*++

Routine Description:

    This routine pops the top value off the stack, and saves it to a global
    variable with the given name in the given module. If the variable did not
    exist previously, it is created.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the module to look in. Negative
        values reference stack indices from the end of the stack.

    Name - Supplies a pointer to the null terminated string containing the
        name of the variable to set.

Return Value:

    None.

--*/

CK_API
BOOL
CkCall (
    PCK_VM Vm,
    UINTN ArgumentCount
    );

/*++

Routine Description:

    This routine pops the given number of arguments off the stack, then pops
    a callable object or class, and executes that call. The return value is
    pushed onto the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ArgumentCount - Supplies the number of arguments to the call. The callable
        object (either a function or a class) will also be popped after these
        arguments.

Return Value:

    TRUE on success.

    FALSE if an error occurred.

--*/

CK_API
BOOL
CkCallMethod (
    PCK_VM Vm,
    PSTR MethodName,
    UINTN ArgumentCount
    );

/*++

Routine Description:

    This routine pops the given number of arguments off the stack, then pops
    an object, and executes the method with the given name on that object. The
    return value is pushed onto the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    MethodName - Supplies a pointer to the null terminated string containing
        the name of the method to call.

    ArgumentCount - Supplies the number of arguments to the call. The class
        instance will also be popped after these arguments.

Return Value:

    TRUE on success.

    FALSE if an error occurred.

--*/

CK_API
VOID
CkRaiseException (
    PCK_VM Vm,
    INTN StackIndex
    );

/*++

Routine Description:

    This routine raises an exception. The caller must not make any more
    modifications to the stack, and should return as soon as possible.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the exception to raise. Negative
        values reference stack indices from the end of the stack.

Return Value:

    None. The foreign function call frame is no longer on the execution stack.

--*/

CK_API
VOID
CkRaiseBasicException (
    PCK_VM Vm,
    PCSTR Type,
    PCSTR MessageFormat,
    ...
    );

/*++

Routine Description:

    This routine reports a runtime error in the current fiber. The caller must
    not make any more modifications to the stack, and should return as soon as
    possible.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Type - Supplies the name of a builtin exception type. This type must
        already be in scope.

    MessageFormat - Supplies the printf message format string. The total size
        of the resulting string is limited, so please be succinct.

    ... - Supplies the remaining arguments.

Return Value:

    None.

--*/

CK_API
VOID
CkPushModule (
    PCK_VM Vm,
    PSTR ModuleName
    );

/*++

Routine Description:

    This routine pushes the module with the given full.dotted.name onto the
    stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ModuleName - Supplies the name of the module to push. If no module by the
        given name can be found, null is pushed.

Return Value:

    None.

--*/

CK_API
VOID
CkPushCurrentModule (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine pushes the module that the running function was defined in
    onto the stack. If no function is currently running, then null is pushed.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

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

CK_API
VOID
CkDeclareVariables (
    PCK_VM Vm,
    INTN ModuleIndex,
    PCK_VARIABLE_DESCRIPTION Variables
    );

/*++

Routine Description:

    This routine registers an array of Chalk objects in the given module.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ModuleIndex - Supplies the stack index of the module to add the variables
        to.

    Variables - Supplies a pointer to an array of variables. The array should
        be NULL terminated.

Return Value:

    None.

--*/

CK_API
VOID
CkReturnNull (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine sets null as the return value.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

CK_API
VOID
CkReturnInteger (
    PCK_VM Vm,
    CK_INTEGER Integer
    );

/*++

Routine Description:

    This routine sets an integer as the return value.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Integer - Supplies the integer to set as the foreign function return.

Return Value:

    None.

--*/

CK_API
VOID
CkReturnString (
    PCK_VM Vm,
    PCSTR String,
    UINTN Length
    );

/*++

Routine Description:

    This routine creates a new string and sets it as the return value.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    String - Supplies a pointer to the buffer containing the string. A copy of
        this buffer will be made.

    Length - Supplies the length of the buffer, in bytes, not including the
        null terminator.

Return Value:

    None.

--*/

CK_API
BOOL
CkGetLength (
    PCK_VM Vm,
    INTN StackIndex,
    PCK_INTEGER Length
    );

/*++

Routine Description:

    This routine returns the number of elements in the given list or dict by
    calling its length method.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the list to get the length of.

    Length - Supplies a pointer where the result of the length method will be
        returned.

Return Value:

    None.

--*/

