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
// ---------------------------------------------------------------- Definitions
//

#define CK_API

#define CHALK_VERSION_MAJOR 1
#define CHALK_VERSION_MINOR 0
#define CHALK_VERSION_REVISION 0

#define CHALK_VERSION ((CHALK_VERSION_MAJOR << 24) | \
                       (CHALK_VERSION_MINOR << 16) | \
                       CHALK_VERSION_REVISION)

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

typedef struct _CK_VM CK_VM, *PCK_VM;
typedef struct _CK_OBJECT CK_OBJECT, *PCK_OBJECT;
typedef struct _CK_HANDLE CK_HANDLE, *PCK_HANDLE;

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
PCK_OBJECT
(*PCK_FOREIGN_FUNCTION) (
    PCK_VM Vm,
    PCK_OBJECT *Arguments
    );

/*++

Routine Description:

    This routine represents the prototype of a Chalk function implemented in C.
    It is the function call interface between Chalk and C.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies a pointer to the function arguments. The first
        argument is always the receiver. The remaining arguments are determined
        by the function signature.

Return Value:

    Returns a pointer to the return value, or NULL if the function does not
    return a value (in which case an implicit null object will be set as the
    return value).

--*/

typedef
VOID
(*PCK_DESTROY_OBJECT) (
    PVOID Data
    );

/*++

Routine Description:

    This routine is called to destroy a foreign object previously created.

Arguments:

    Data - Supplies a pointer to the class context to destroy.

Return Value:

    None.

--*/

typedef
PSTR
(*PCK_LOAD_MODULE) (
    PCK_VM Vm,
    PSTR ModuleName,
    PUINTN Size
    );

/*++

Routine Description:

    This routine is called to load a new Chalk module.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ModuleName - Supplies the name of the module to load.

    Size - Supplies a pointer where the size of the module string will be
        returned, not including the null terminator.

Return Value:

    Returns a pointer to a string containing the source code of the module.
    This memory should be allocated from the heap, and will be freed by Chalk.

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

    Write - Stores an optional pointer to a function used to write output to
        the console. If this is NULL, output is simply discarded.

    Error - Stores a pointer to a function used to report errors. If NULL,
        errors are not reported.

    InitialHeapSize - Stores the number of bytes to allocate before triggering
        a garbage collection.

    MinimumHeapSize - Stores the minimum size of heap, used to keep garbage
        collections from occurring too frequently.

    HeapGrowthPercent - Stores the percentage the heap has to grow to trigger
        another garbage collection.

    Flags - Stores a bitfield of flags governing the operation of the
        interpreter See CK_CONFIGURATION_* definitions.

--*/

typedef struct _CK_CONFIGURATION {
    PCK_REALLOCATE Reallocate;
    PCK_LOAD_MODULE LoadModule;
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

