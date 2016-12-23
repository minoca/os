/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    acpiobj.h

Abstract:

    This header contains definitions for ACPI objects used in the ACPI namespace
    and AML intepreter.

Author:

    Evan Green 13-Nov-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "oprgnos.h"
#include <minoca/kernel/acpi.h>

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the revision of the AML interpreter. This define is visible to
// executing AML code via the Revision opcode.
//

#define AML_REVISION 4

//
// Define the maximum number of arguments any ACPI statement can have.
//

#define MAX_AML_STATEMENT_ARGUMENT_COUNT 6

#define MAX_AML_METHOD_ARGUMENT_COUNT 7
#define MAX_AML_LOCAL_COUNT 8

//
// Define an invalid AML local index constant.
//

#define AML_INVALID_LOCAL_INDEX (ULONG)-1

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _ACPI_OBJECT_TYPE {
    AcpiObjectUninitialized   = 0,
    AcpiObjectInteger         = 1,
    AcpiObjectString          = 2,
    AcpiObjectBuffer          = 3,
    AcpiObjectPackage         = 4,
    AcpiObjectFieldUnit       = 5,
    AcpiObjectDevice          = 6,
    AcpiObjectEvent           = 7,
    AcpiObjectMethod          = 8,
    AcpiObjectMutex           = 9,
    AcpiObjectOperationRegion = 10,
    AcpiObjectPowerResource   = 11,
    AcpiObjectProcessor       = 12,
    AcpiObjectThermalZone     = 13,
    AcpiObjectBufferField     = 14,
    AcpiObjectDdbHandle       = 15,
    AcpiObjectDebug           = 16,
    AcpiObjectAlias           = 0x100,
    AcpiObjectUnresolvedName  = 0x101,
    AcpiObjectTypeCount
} ACPI_OBJECT_TYPE, *PACPI_OBJECT_TYPE;

typedef enum _ACPI_FIELD_ACCESS {
    AcpiFieldAccessAny        = 0,
    AcpiFieldAccessByte       = 1,
    AcpiFieldAccessWord       = 2,
    AcpiFieldAccessDoubleWord = 3,
    AcpiFieldAccessQuadWord   = 4,
    AcpiFieldAccessBuffer     = 5
} ACPI_FIELD_ACCESS, *PACPI_FIELD_ACCESS;

typedef enum _ACPI_FIELD_UPDATE_RULE {
    AcpiFieldUpdatePreserve     = 0,
    AcpiFieldUpdateWriteAsOnes  = 1,
    AcpiFieldUpdateWriteAsZeros = 2,
    AcpiFieldUpdateRuleCount
} ACPI_FIELD_UPDATE_RULE, *PACPI_FIELD_UPDATE_RULE;

typedef enum _AML_STATEMENT_TYPE {
    AmlStatementInvalid,
    AmlStatementAcquire,
    AmlStatementAdd,
    AmlStatementAlias,
    AmlStatementAnd,
    AmlStatementArgument,
    AmlStatementBankField,
    AmlStatementBreak,
    AmlStatementBreakPoint,
    AmlStatementBuffer,
    AmlStatementConcatenate,
    AmlStatementConcatenateResourceTemplates,
    AmlStatementConditionalReferenceOf,
    AmlStatementContinue,
    AmlStatementCopyObject,
    AmlStatementCreateBufferField,
    AmlStatementCreateBufferFieldFixed,
    AmlStatementData,
    AmlStatementDataTableRegion,
    AmlStatementDebug,
    AmlStatementDecrement,
    AmlStatementDereferenceOf,
    AmlStatementDevice,
    AmlStatementDivide,
    AmlStatementElse,
    AmlStatementEvent,
    AmlStatementExecutingMethod,
    AmlStatementFatal,
    AmlStatementField,
    AmlStatementFindSetLeftBit,
    AmlStatementFindSetRightBit,
    AmlStatementFromBcd,
    AmlStatementIf,
    AmlStatementIncrement,
    AmlStatementIndex,
    AmlStatementIndexField,
    AmlStatementLoad,
    AmlStatementLoadTable,
    AmlStatementLocal,
    AmlStatementLogicalAnd,
    AmlStatementLogicalEqual,
    AmlStatementLogicalGreater,
    AmlStatementLogicalLess,
    AmlStatementLogicalNot,
    AmlStatementLogicalOr,
    AmlStatementMatch,
    AmlStatementMethod,
    AmlStatementMid,
    AmlStatementMod,
    AmlStatementMultiply,
    AmlStatementMutex,
    AmlStatementName,
    AmlStatementNameString,
    AmlStatementNand,
    AmlStatementNoOp,
    AmlStatementNor,
    AmlStatementNot,
    AmlStatementNotify,
    AmlStatementObjectType,
    AmlStatementOne,
    AmlStatementOnes,
    AmlStatementOperationRegion,
    AmlStatementOr,
    AmlStatementPackage,
    AmlStatementPowerResource,
    AmlStatementProcessor,
    AmlStatementReferenceOf,
    AmlStatementRelease,
    AmlStatementReset,
    AmlStatementReturn,
    AmlStatementRevision,
    AmlStatementScope,
    AmlStatementShiftLeft,
    AmlStatementShiftRight,
    AmlStatementSignal,
    AmlStatementSizeOf,
    AmlStatementSleep,
    AmlStatementStall,
    AmlStatementStore,
    AmlStatementSubtract,
    AmlStatementThermalZone,
    AmlStatementTimer,
    AmlStatementToBcd,
    AmlStatementToBuffer,
    AmlStatementToDecimalString,
    AmlStatementToHexString,
    AmlStatementToInteger,
    AmlStatementToString,
    AmlStatementUnload,
    AmlStatementVariablePackage,
    AmlStatementWait,
    AmlStatementWhile,
    AmlStatementXor,
    AmlStatementZero,
    AmlStatementCount
} AML_STATEMENT_TYPE, *PAML_STATEMENT_TYPE;

typedef struct _AML_EXECUTION_CONTEXT
    AML_EXECUTION_CONTEXT, *PAML_EXECUTION_CONTEXT;

typedef struct _ACPI_OBJECT ACPI_OBJECT, *PACPI_OBJECT;

typedef
KSTATUS
(*PACPI_C_METHOD) (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT Method,
    PACPI_OBJECT *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine implements an ACPI method in C.

Arguments:

    Context - Supplies a pointer to the execution context.

    Method - Supplies a pointer to the method getting executed.

    Arguments - Supplies a pointer to the function arguments.

    ArgumentCount - Supplies the number of arguments provided.

Return Value:

    STATUS_SUCCESS if execution completed.

    Returns a failing status code if a catastrophic error occurred that
    prevented the proper execution of the method.

--*/

/*++

Structure Description:

    This structure stores information about an ACPI namespace integer.

Members:

    Value - Stores the integer value.

--*/

typedef struct _ACPI_INTEGER_OBJECT {
    ULONGLONG Value;
} ACPI_INTEGER_OBJECT, *PACPI_INTEGER_OBJECT;

/*++

Structure Description:

    This structure stores information about an ACPI namespace string.

Members:

    String - Stores a pointer to the null-terminated string buffer.

--*/

typedef struct _ACPI_STRING_OBJECT {
    PSTR String;
} ACPI_STRING_OBJECT, *PACPI_STRING_OBJECT;

/*++

Structure Description:

    This structure stores information about an ACPI namespace buffer.

Members:

    Buffer - Stores a pointer to the buffer.

    Length - Stores the length of the buffer, in bytes.

--*/

typedef struct _ACPI_BUFFER_OBJECT {
    PVOID Buffer;
    ULONG Length;
} ACPI_BUFFER_OBJECT, *PACPI_BUFFER_OBJECT;

/*++

Structure Description:

    This structure stores information about an ACPI namespace package.

Members:

    Array - Stores a pointer to an array of ACPI Object pointers.

    ElementCount - Stores the number of elements that can be stored in the
        array.

--*/

typedef struct _ACPI_PACKAGE_OBJECT {
    PVOID *Array;
    ULONG ElementCount;
} ACPI_PACKAGE_OBJECT, *PACPI_PACKAGE_OBJECT;

/*++

Structure Description:

    This structure stores information about an ACPI Field Unit object.

Members:

    OperationRegion - Stores a pointer to the operation region this field
        references.

    Access - Stores the access width to use when reading from or writing to
        the Operation Region.

    AcquireGlobalLock - Stores a boolean indicating whether the global ACPI
        lock should be acquired when accessing this field.

    UpdateRule - Stores the rule for unreferenced bits when the field is
        smaller than the access width.

    BitOffset - Stores the offset from the beginning of the Operation Region,
        in bits.

    BitLength - Stores the length of the field, in bits.

    BankRegister - Stores an optional pointer to the bank register to write
        to before accessing this field.

    BankValue - Stores a pointer to the value to write into the bank register.

    IndexRegister - Stores an optional pointer to the Index register to write
        to if Index/Data style access is used.

    DataRegister - Stores a pointer to the Data register to use for accessing
        the field if Index/Data style access is used.

--*/

typedef struct _ACPI_FIELD_UNIT_OBJECT {
    PVOID OperationRegion;
    ACPI_FIELD_ACCESS Access;
    BOOL AcquireGlobalLock;
    ACPI_FIELD_UPDATE_RULE UpdateRule;
    ULONGLONG BitOffset;
    ULONGLONG BitLength;
    PVOID BankRegister;
    PVOID BankValue;
    PVOID IndexRegister;
    PVOID DataRegister;
} ACPI_FIELD_UNIT_OBJECT, *PACPI_FIELD_UNIT_OBJECT;

/*++

Structure Description:

    This structure stores information about an ACPI namespace Device object.

Members:

    OsDevice - Stores a pointer to the operating system event object.

    DeviceContext - Stores a pointer to the ACPI device context associated
        with this device.

    IsPciBus - Stores a pointer indicating if this device is a PCI bus or not.
        PCI busses are special in that they have an interaction with certain
        ACPI Operation Regions, namely PCI config and BAR target operation
        regions.

    IsDeviceStarted - Stores a boolean indicating whether or not the device is
        started or not.

--*/

typedef struct _ACPI_DEVICE_OBJECT {
    PVOID OsDevice;
    PVOID DeviceContext;
    BOOL IsPciBus;
    BOOL IsDeviceStarted;
} ACPI_DEVICE_OBJECT, *PACPI_DEVICE_OBJECT;

/*++

Structure Description:

    This structure stores information about an ACPI namespace Event object.

Members:

    OsEvent - Stores a pointer to the operating system event object.

--*/

typedef struct _ACPI_EVENT_OBJECT {
    PVOID OsEvent;
} ACPI_EVENT_OBJECT, *PACPI_EVENT_OBJECT;

/*++

Structure Description:

    This structure stores information about an ACPI namespace Method object.

Members:

    ArgumentCount - Stores the number of arguments this method takes.

    Serialized - Stores a boolean indicating whether concurrent execution of
        this routine is allowed or not.

    SyncLevel - Stores the sync level of this routine.

    OsMutex - Stores a pointer to the OS mutex guarding serial access of the
        routine.

    AmlCode - Stores a pointer to the code that implements this method.

    AmlCodeSize - Stores the length of the AML code, in bytes.

    IntegerWidthIs32 - Stores a boolean indicating whether the definition block
        defining this method only supports 32 bit integers (a table revision of
        1).

    Function - Stores a pointer to the C function to run when executing this
        method. Most of the time this is NULL, as functions are implemented in
        AML code. The _OSI function is a notable exception.

--*/

typedef struct _ACPI_METHOD_OBJECT {
    ULONG ArgumentCount;
    BOOL Serialized;
    UCHAR SyncLevel;
    PVOID OsMutex;
    PVOID AmlCode;
    ULONG AmlCodeSize;
    BOOL IntegerWidthIs32;
    PACPI_C_METHOD Function;
} ACPI_METHOD_OBJECT, *PACPI_METHOD_OBJECT;

/*++

Structure Description:

    This structure stores information about an ACPI namespace Mutex object.

Members:

    OsMutex - Stores a pointer to the operating system mutex object.

--*/

typedef struct _ACPI_MUTEX_OBJECT {
    PVOID OsMutex;
} ACPI_MUTEX_OBJECT, *PACPI_MUTEX_OBJECT;

/*++

Structure Description:

    This structure stores information about an ACPI Operation Region Object.

Members:

    Space - Stores a pointer to the Operation Region address space type.

    OsContext - Stores a pointer to the operating system context pointer
        for the given Operation Region.

    Offset - Stores the byte offset into the address space where this
        operation region begins.

    Length - Stores the length of the operation region, in bytes.

    FunctionTable - Stores a pointer to a table of function pointers used to
        access and destroy the operation region.

    OsMutex - Stores a pointer to the mutex guarding this Operation Region.

--*/

typedef struct _ACPI_OPERATION_REGION_OBJECT {
    ACPI_OPERATION_REGION_SPACE Space;
    PVOID OsContext;
    ULONGLONG Offset;
    ULONGLONG Length;
    PACPI_OPERATION_REGION_FUNCTION_TABLE FunctionTable;
    PVOID OsMutex;
} ACPI_OPERATION_REGION_OBJECT, *PACPI_OPERATION_REGION_OBJECT;

/*++

Structure Description:

    This structure stores information about an ACPI Power Resource object.

Members:

    SystemLevel - Stores the lower power system sleep level the OSPM must
        maintain to keep this power resource on (0 is S0, 1 is S1, etc).

    ResourceOrder - Stores a unique value per power resource specifying the
        order in which power resources must be enabled or disabled (enabling
        goes low to high, disabling goes high to low).

--*/

typedef struct _ACPI_POWER_RESOURCE_OBJECT {
    UCHAR SystemLevel;
    USHORT ResourceOrder;
} ACPI_POWER_RESOURCE_OBJECT, *PACPI_POWER_RESOURCE_OBJECT;

/*++

Structure Description:

    This structure stores information about an ACPI Processor object.

Members:

    Device - Stores the device information, since processors are treated like
        devices.

    ProcessorBlockAddress - Stores the address of the processor block registers
        for this processor.

    ProcessorId - Stores the ACPI processor identifier for this processor.

    ProcessorBlockLength - Stores the length of the processor block register
        space. 0 implies that the there are no processor block registers.

--*/

typedef struct _ACPI_PROCESSOR_OBJECT {
    ACPI_DEVICE_OBJECT Device;
    ULONG ProcessorBlockAddress;
    UCHAR ProcessorId;
    UCHAR ProcessorBlockLength;
} ACPI_PROCESSOR_OBJECT, *PACPI_PROCESSOR_OBJECT;

/*++

Structure Description:

    This structure stores information about an ACPI namespace alias.

Members:

    DestinationObject - Stores a pointer to the ACPI object that owns the
        buffer.

    BitOffset - Stores the offset, in bits, from the beginning of the buffer
        to this field.

    BitLength - Stores the length of the field, in bits.

--*/

typedef struct _ACPI_BUFFER_FIELD_OBJECT {
    PVOID DestinationObject;
    ULONGLONG BitOffset;
    ULONGLONG BitLength;
} ACPI_BUFFER_FIELD_OBJECT, *PACPI_BUFFER_FIELD_OBJECT;

/*++

Structure Description:

    This structure stores information about an ACPI namespace alias.

Members:

    DestinationObject - Stores a pointer to the ACPI object that this alias
        points to.

--*/

typedef struct _ACPI_ALIAS_OBJECT {
    PVOID DestinationObject;
} ACPI_ALIAS_OBJECT, *PACPI_ALIAS_OBJECT;

/*++

Structure Description:

    This structure stores information about an "unresolved name" placeholder
    object. These objects can live in various places where a name can be
    referenced before it is defined. When someone actually goes to use the
    object, the name is then resolved to the real object.

Members:

    Name - Stores a pointer to a string containing the name of the object.

    Scope - Stores a pointer to an ACPI namespace object representing the
        current scope at the time the object was referenced.

--*/

typedef struct _ACPI_UNRESOLVED_NAME_OBJECT {
    PSTR Name;
    PVOID Scope;
} ACPI_UNRESOLVED_NAME_OBJECT, *PACPI_UNRESOLVED_NAME_OBJECT;

/*++

Structure Description:

    This structure stores information about an ACPI namespace object.

Members:

    Type - Stores the type of ACPI object this structure represents.

    Name - Stores the name of the ACPI object.

    ReferenceCount - Stores the number of parties that have references to this
        object. When the number reaches zero, the object is destroyed.

    Parent - Stores a pointer to the parent namespace object. This may be NULL
        if the object is not linked into the namespace.

    SiblingListEntry - Stores pointers to the next and previous objects in the
        parent's child list.

    ChildListHead - Stores pointers to the first and last children of this
        object.

    DestructorListEntry - Stores a list structure used to avoid recursion during
        namespace object destruction. Also used during object lifetime to
        store a list of all objects created during a given method execution.

    U - Stores type-specific information about the given object.

--*/

struct _ACPI_OBJECT {
    ACPI_OBJECT_TYPE Type;
    ULONG Name;
    ULONG ReferenceCount;
    PACPI_OBJECT Parent;
    LIST_ENTRY SiblingListEntry;
    LIST_ENTRY ChildListHead;
    LIST_ENTRY DestructorListEntry;
    union {
        ACPI_INTEGER_OBJECT Integer;
        ACPI_STRING_OBJECT String;
        ACPI_BUFFER_OBJECT Buffer;
        ACPI_PACKAGE_OBJECT Package;
        ACPI_FIELD_UNIT_OBJECT FieldUnit;
        ACPI_DEVICE_OBJECT Device;
        ACPI_EVENT_OBJECT Event;
        ACPI_METHOD_OBJECT Method;
        ACPI_MUTEX_OBJECT Mutex;
        ACPI_OPERATION_REGION_OBJECT OperationRegion;
        ACPI_POWER_RESOURCE_OBJECT PowerResource;
        ACPI_PROCESSOR_OBJECT Processor;
        ACPI_BUFFER_FIELD_OBJECT BufferField;
        ACPI_ALIAS_OBJECT Alias;
        ACPI_UNRESOLVED_NAME_OBJECT UnresolvedName;
    } U;
};

/*++

Structure Description:

    This structure stores information about an ACPI AML statement.

Members:

    ListEntry - Stores pointers to the next and previous AML statements on the
        currently executing statement stack.

    Type - Stores the general flavor of this AML statement.

    AdditionalData - Stores statement-specific additional data related to the
        operation.

    AdditionalData2 - Stores more statement-specific information.

    ArgumentsNeeded - Stores the number of arguments this statement needs.

    ArgumentsAcquired - Stores the number of arguments that this statement
        has in-hand. Once all arguments are acquired, the statement can be
        completely evaluated.

    Argument - Stores an array of pointers to the various ACPI objects needed
        to evaluate this statement.

    Reduction - Stores a pointer to the ACPI object that this statement reduced
        to.

    SavedScope - Stores a pointer to the original scope before this statement
        started executing for statements that change the scope.

--*/

typedef struct _AML_STATEMENT {
    LIST_ENTRY ListEntry;
    AML_STATEMENT_TYPE Type;
    ULONGLONG AdditionalData;
    ULONGLONG AdditionalData2;
    ULONG ArgumentsNeeded;
    ULONG ArgumentsAcquired;
    PACPI_OBJECT Argument[MAX_AML_STATEMENT_ARGUMENT_COUNT];
    PACPI_OBJECT Reduction;
    PACPI_OBJECT SavedScope;
} AML_STATEMENT, *PAML_STATEMENT;

/*++

Structure Description:

    This structure stores information about an executing ACPI method.

Members:

    CallingMethodContext - Stores a pointer to the caller's method context
        (points to an object of this type).

    MethodMutex - Stores an optional pointer to the mutex associated with this
        synchronized method.

    IntegerWidthIs32 - Stores a boolean indicating whether the bit width of
        an AML integer is 32 (TRUE) or 64 (FALSE).

    CreatedObjectsListHead - Stores the head of the list of objects created
        while this method is executing. When the method returns, the list of
        objects will be destroyed.

    SavedAmlCode - Stores a pointer to the AML code pointer immediately
        before this method was called.

    SavedAmlCodeSize - Stores the old AML code size immediately before this
        method was called.

    SavedCurrentOffset - Stores the offset into the old AML code immediately
        after this function call. Think of it like the return instruction
        pointer.

    SavedIndentationLevel - Stores the indentation level immediately before
        this method was executed.

    LastLocalIndex - Stores an index into the local variable array of the last
        local statement evaluated.

    SavedCurrentScope - Stores the current scope immeidately before this
        function was called.

    LocalVariable - Supplies an array of objects that represent local variables
        to the functions.

    Argument - Stores an array of pointers to the method arguments, if supplied.

--*/

typedef struct _AML_METHOD_EXECUTION_CONTEXT {
    PVOID CallingMethodContext;
    PVOID MethodMutex;
    BOOL IntegerWidthIs32;
    LIST_ENTRY CreatedObjectsListHead;
    PVOID SavedAmlCode;
    ULONG SavedAmlCodeSize;
    ULONG SavedCurrentOffset;
    ULONG SavedIndentationLevel;
    ULONG LastLocalIndex;
    PACPI_OBJECT SavedCurrentScope;
    PACPI_OBJECT LocalVariable[MAX_AML_LOCAL_COUNT];
    PACPI_OBJECT Argument[MAX_AML_METHOD_ARGUMENT_COUNT];
} AML_METHOD_EXECUTION_CONTEXT, *PAML_METHOD_EXECUTION_CONTEXT;

/*++

Structure Description:

    This structure stores information about the state of the AML interpreter
    during execution.

Members:

    ExecuteStatements - Stores a boolean indicating whether or not to actually
        execute the statements processed. If this is set to FALSE, then the
        AML statements will be interpreted (and printed if that option is set),
        but no changes will be made to the namespace or anything else.

    PrintStatements - Stores a boolean determining whether statements should be
        printed out. If execution is also enabled, then only executed statements
        will be printed. If execution is not enabled, then all statements will
        be printed (ie both outcomes of a branch are entered).

    EscapingDynamicScope - Stores a boolean indicating whether objects created
        at this time will fall under the dynamic scope and therefore be deleted
        when the method completes or the block unloads (FALSE), or whether
        a scope operator has escaped the context from the original dynamic
        scope.

    AmlCode - Stores a pointer to the AML code buffer to be executed.

    AmlCodeSize - Stores the size of the AML code buffer, in bytes.

    CurrentOffset - Stores the current execution offset, in bytes.

    IndentationLevel - Stores the current indentation level, used when printing
        out instructions.

    SyncLevel - Stores the sync level of the current execution context, the
        highest numbered mutex it has acquired.

    CurrentScope - Stores a pointer to the current scope.

    StatementStackHead - Stores the list head of the in-flight statement stack.
        Values are pushed on the stack by inserting them immediately after the
        head, and are popped off the stack by looking at Head->Next.

    PreviousStatement - Stores a pointer to the previous statement that was
        evaluated, as the items on the statement stack are waiting for future
        statements to evaluate.

    CurrentMethod - Stores a pointer to the executing method's execution
        context.

    ReturnValue - Stores a pointer to the return value object if a method is
        executing.

    LastIfStatementResult - Stores the result of the last If statement to
        finish executing. This parameter is used for evaluating an Else
        statment.

    DestructorListHead - Stores a pointer to the head of a list where created
        namespace objects will have their destructor list entries placed. This
        is used for unloading definition blocks.

--*/

struct _AML_EXECUTION_CONTEXT {
    BOOL ExecuteStatements;
    BOOL PrintStatements;
    BOOL EscapingDynamicScope;
    PVOID AmlCode;
    ULONG AmlCodeSize;
    ULONG CurrentOffset;
    ULONG IndentationLevel;
    ULONG SyncLevel;
    PACPI_OBJECT CurrentScope;
    LIST_ENTRY StatementStackHead;
    PAML_STATEMENT PreviousStatement;
    PAML_METHOD_EXECUTION_CONTEXT CurrentMethod;
    PACPI_OBJECT ReturnValue;
    BOOL LastIfStatementResult;
    PLIST_ENTRY DestructorListHead;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
AcpiLoadDefinitionBlock (
    PDESCRIPTION_HEADER Table,
    PACPI_OBJECT Handle
    );

/*++

Routine Description:

    This routine loads an ACPI definition block, which contains a standard
    table description header followed by a block of AML. The AML will be loaded
    into the namespace.

Arguments:

    Table - Supplies a pointer to the table containing the definition block.
        This table should probably only be the DSDT or an SSDT.

    Handle - Supplies an optional pointer to the handle associated with this
        definition block.

Return Value:

    Status code.

--*/

VOID
AcpiUnloadDefinitionBlock (
    PACPI_OBJECT Handle
    );

/*++

Routine Description:

    This routine unloads all ACPI definition blocks loaded under the given
    handle.

Arguments:

    Handle - Supplies the handle to unload blocks based on. If NULL is
        supplied, then all blocks will be unloaded.

Return Value:

    None.

--*/

KSTATUS
AcpiExecuteMethod (
    PACPI_OBJECT MethodObject,
    PACPI_OBJECT *Arguments,
    ULONG ArgumentCount,
    ACPI_OBJECT_TYPE ReturnType,
    PACPI_OBJECT *ReturnValue
    );

/*++

Routine Description:

    This routine executes an ACPI method.

Arguments:

    MethodObject - Supplies a pointer to the method object. If this object
        is not of type method, then the return value will be set
        directly to this object (and the reference count incremented).

    Arguments - Supplies a pointer to an array of arguments to pass to the
        method. This parameter is optional if the method takes no parameters.

    ArgumentCount - Supplies the number of arguments in the argument array.

    ReturnType - Supplies the desired type to convert the return type argument
        to. Set this to AcpiObjectUninitialized to specify that no conversion
        of the return type should occur (I'm feeling lucky mode).

    ReturnValue - Supplies an optional pointer where a pointer to the return
        value object will be returned. The caller must release the reference
        on this memory when finished with it.

Return Value:

    Status code.

--*/

VOID
AcpipPopExecutingStatements (
    PAML_EXECUTION_CONTEXT Context,
    BOOL PopToWhile,
    BOOL ContinueWhile
    );

/*++

Routine Description:

    This routine causes the AML execution context to pop back up, either because
    the method returned or because while within a while statement a break or
    continue was encountered. This routine only pops statements off the stack,
    it does not modify the current offset pointer.

Arguments:

    Context - Supplies a pointer to the AML execution context.

    PopToWhile - Supplies a boolean that indicates whether the entire function
        should return (FALSE) or whether to pop to the nearest while statement
        (TRUE). For while statements, the caller is still responsible for
        modifying the AML offset.

    ContinueWhile - Supplies a boolean that indicates whether to re-execute
        the while statement (TRUE) or whether to pop it too (FALSE, for a break
        statement). If the previous argument is FALSE, this parameter is
        ignored.

Return Value:

    None.

--*/

VOID
AcpipPrintIndentedNewLine (
    PAML_EXECUTION_CONTEXT Context
    );

/*++

Routine Description:

    This routine prints a newline and then a number of space characters
    corresponding to the current indentation level.

Arguments:

    Context - Supplies a pointer to the AML execution context.

Return Value:

    None.

--*/

KSTATUS
AcpipPushMethodOnExecutionContext (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT Scope,
    PVOID MethodMutex,
    BOOL IntegerWidthIs32,
    PVOID AmlCode,
    ULONG AmlCodeSize,
    ULONG ArgumentCount,
    PACPI_OBJECT *Arguments
    );

/*++

Routine Description:

    This routine pushes a control method onto the given AML execution context,
    causing it to be the next thing to run when the execution context is
    evaluated.

Arguments:

    Context - Supplies a pointer to the AML execution context to push the
        method onto.

    Scope - Supplies a pointer to the ACPI object to put as the starting scope.
        If NULL is supplied, the namespace root will be used as the default
        scope.

    MethodMutex - Supplies an optional pointer to the mutex to acquire in
        conjunction with executing this serialized method.

    IntegerWidthIs32 - Supplies a boolean indicating if integers should be
        treated as 32-bit values or 64-bit values.

    AmlCode - Supplies a pointer to the first byte of the method.

    AmlCodeSize - Supplies the size of the method, in bytes.

    ArgumentCount - Supplies the number of arguments to pass to the routine.
        Valid values are 0 to 7.

    Arguments - Supplies an array of pointers to ACPI objects representing the
        method arguments. The number of elements in this array is defined by
        the argument count parameter. If that parameter is non-zero, this
        parameter is required.

Return Value:

    Status code indicating whether the method was successfully pushed onto the
    execution context.

--*/

VOID
AcpipPopCurrentMethodContext (
    PAML_EXECUTION_CONTEXT Context
    );

/*++

Routine Description:

    This routine pops the current method execution context off of the AML
    execution context, releasing all its associated objects and freeing the
    method context itself.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context.

Return Value:

    None.

--*/

KSTATUS
AcpipRunInitializationMethods (
    PACPI_OBJECT RootObject
    );

/*++

Routine Description:

    This routine runs immediately after a definition block has been loaded. As
    defined by the ACPI spec, it runs all applicable _INI methods on devices.

Arguments:

    RootObject - Supplies a pointer to the object to start from. If NULL is
        supplied, the root system bus object \_SB will be used.

Return Value:

    Status code. Failure means something serious went wrong, not just that some
    device returned a non-functioning status.

--*/

KSTATUS
AcpipOsiMethod (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT Method,
    PACPI_OBJECT *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine implements the _OSI method, which allows the AML code to
    determine support for OS-specific features.

Arguments:

    Context - Supplies a pointer to the execution context.

    Method - Supplies a pointer to the method getting executed.

    Arguments - Supplies a pointer to the function arguments.

    ArgumentCount - Supplies the number of arguments provided.

Return Value:

    STATUS_SUCCESS if execution completed.

    Returns a failing status code if a catastrophic error occurred that
    prevented the proper execution of the method.

--*/

