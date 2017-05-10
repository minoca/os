/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    aml.c

Abstract:

    This module implements the ACPI AML interpreter.

Author:

    Evan Green 13-Nov-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "acpip.h"
#include "amlos.h"
#include "amlops.h"
#include "namespce.h"
#include "oprgn.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define AML execution options.
//

//
// Set this bit to actually execute the given definition block.
//

#define AML_EXECUTION_OPTION_RUN 0x00000001

//
// Set this bit to print out the definition block to the debugger.
//

#define AML_EXECUTION_OPTION_PRINT 0x00000002

//
// Define return values from _OSI indicating whether the given request is
// supported or unsupported by the OS.
//

#define OSI_BEHAVIOR_SUPPORTED 0xFFFFFFFFFFFFFFFF
#define OSI_BEHAVIOR_UNSUPPORTED 0

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores information about a loaded SSDT table.

Members:

    ListEntry - Stores pointers to the next and previous secondary definition
        tables.

    HandleObject - Stores the optional handle associated with this definition
        block.

    ObjectList - Stores the head of the list of namespace objects to destroy if
        this definition block is unloaded. The objects on this list will be
        ACPI objects, and the list entry is the destructor list entry.

    Code - Stores the AML code for this definition block.

--*/

typedef struct _ACPI_LOADED_DEFINITION_BLOCK {
    LIST_ENTRY ListEntry;
    PACPI_OBJECT HandleObject;
    LIST_ENTRY ObjectList;
    PDESCRIPTION_HEADER Code;
} ACPI_LOADED_DEFINITION_BLOCK, *PACPI_LOADED_DEFINITION_BLOCK;

//
// ----------------------------------------------- Internal Function Prototypes
//

PAML_EXECUTION_CONTEXT
AcpipCreateAmlExecutionContext (
    ULONG Options
    );

VOID
AcpipDestroyAmlExecutionContext (
    PAML_EXECUTION_CONTEXT Context
    );

KSTATUS
AcpipExecuteAml (
    PAML_EXECUTION_CONTEXT Context
    );

KSTATUS
AcpipCreateNextStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT *NextStatement
    );

VOID
AcpipDestroyStatement (
    PAML_STATEMENT Statement
    );

KSTATUS
AcpipEvaluateStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

PAML_STATEMENT
AcpipCreateStatement (
    );

KSTATUS
AcpipCreateExecutingMethodStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT *NextStatement
    );

KSTATUS
AcpipRunDeviceInitialization (
    PACPI_OBJECT Device,
    PBOOL TraverseDown
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define a value that can be set from the debugger to change the behavior
// of the AML interpreter. For example, it can be set to print out the
// statements it's executing. See AML_EXECUTION_OPTION_* definitions.
//

ULONG AcpiDebugExecutionOptions = 0x0;

//
// Set this to TRUE and every _OSI request will get printed.
//

BOOL AcpiPrintOsiRequests = FALSE;

//
// Store the list of SSDT definition blocks.
//

LIST_ENTRY AcpiLoadedDefinitionBlockList;

//
// Store globals for the read-only ACPI objects Zero, One and Ones.
//

ACPI_OBJECT AcpiZero = {
   AcpiObjectInteger,
   0,
   1,
   NULL,
   {NULL, NULL},
   {NULL, NULL},
   {NULL, NULL},
   {{0ULL}}
};

ACPI_OBJECT AcpiOne = {
   AcpiObjectInteger,
   0,
   1,
   NULL,
   {NULL, NULL},
   {NULL, NULL},
   {NULL, NULL},
   {{1ULL}}
};

ACPI_OBJECT AcpiOnes32 = {
   AcpiObjectInteger,
   0,
   1,
   NULL,
   {NULL, NULL},
   {NULL, NULL},
   {NULL, NULL},
   {{0xFFFFFFFFULL}}
};

ACPI_OBJECT AcpiOnes64 = {
   AcpiObjectInteger,
   0,
   1,
   NULL,
   {NULL, NULL},
   {NULL, NULL},
   {NULL, NULL},
   {{0xFFFFFFFFFFFFFFFFULL}}
};

//
// Store the OSI strings for which TRUE will be returned by default.
//

PCSTR AcpiDefaultOsiStrings[] = {
    "Windows 2000",
    "Windows 2001",
    "Windows 2001 SP1",
    "Windows 2001.1",
    "Windows 2001 SP2",
    "Windows 2001.1 SP1",
    "Windows 2006",
    "Windows 2006.1",
    "Windows 2006 SP1",
    "Windows 2006 SP2",
    "Windows 2009",
    "Windows 2012",
    "Windows 2013",
    "Windows 2015",
    NULL
};

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
AcpiLoadDefinitionBlock (
    PDESCRIPTION_HEADER Table,
    PACPI_OBJECT Handle
    )

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

{

    ULONG AmlSize;
    PACPI_LOADED_DEFINITION_BLOCK CurrentBlock;
    PLIST_ENTRY CurrentEntry;
    PAML_EXECUTION_CONTEXT ExecutionContext;
    ULONG ExecutionOptions;
    BOOL IntegerWidthIs32;
    PACPI_LOADED_DEFINITION_BLOCK LoadedBlock;
    BOOL Match;
    PSTR Name;
    KSTATUS Status;

    //
    // First look to see if this table has already been loaded. Don't double
    // load tables.
    //

    CurrentEntry = AcpiLoadedDefinitionBlockList.Next;
    while (CurrentEntry != &AcpiLoadedDefinitionBlockList) {
        CurrentBlock = LIST_VALUE(CurrentEntry,
                                  ACPI_LOADED_DEFINITION_BLOCK,
                                  ListEntry);

        CurrentEntry = CurrentEntry->Next;
        if ((CurrentBlock->Code->OemTableId == Table->OemTableId) &&
            (CurrentBlock->Code->Length == Table->Length)) {

            Match = RtlCompareMemory(Table, CurrentBlock->Code, Table->Length);
            if (Match != FALSE) {
                return STATUS_SUCCESS;
            }
        }
    }

    //
    // Create an execution context. Before ACPI 2.0, integers were 32-bits wide.
    //

    IntegerWidthIs32 = FALSE;
    ExecutionContext = NULL;
    LoadedBlock = NULL;
    ExecutionOptions = AML_EXECUTION_OPTION_RUN;
    if (Table->Revision < 2) {
        IntegerWidthIs32 = TRUE;
    }

    AmlSize = Table->Length - sizeof(DESCRIPTION_HEADER);
    ExecutionContext = AcpipCreateAmlExecutionContext(ExecutionOptions);
    if (ExecutionContext == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto LoadDefinitionBlockEnd;
    }

    LoadedBlock = AcpipAllocateMemory(sizeof(ACPI_LOADED_DEFINITION_BLOCK));
    if (LoadedBlock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto LoadDefinitionBlockEnd;
    }

    RtlZeroMemory(LoadedBlock, sizeof(ACPI_LOADED_DEFINITION_BLOCK));
    INITIALIZE_LIST_HEAD(&(LoadedBlock->ObjectList));
    LoadedBlock->Code = Table;
    if (Handle != NULL) {
        AcpipObjectAddReference(Handle);
        LoadedBlock->HandleObject = Handle;
    }

    INSERT_BEFORE(&(LoadedBlock->ListEntry), &AcpiLoadedDefinitionBlockList);

    //
    // Push a default method context onto the execution context that spans the
    // entire block being loaded.
    //

    Status = AcpipPushMethodOnExecutionContext(ExecutionContext,
                                               NULL,
                                               NULL,
                                               IntegerWidthIs32,
                                               Table + 1,
                                               AmlSize,
                                               0,
                                               NULL);

    if (!KSUCCESS(Status)) {
        goto LoadDefinitionBlockEnd;
    }

    ExecutionContext->DestructorListHead = &(LoadedBlock->ObjectList);
    if (ExecutionContext->PrintStatements != FALSE) {
        Name = (PSTR)&(Table->Signature);
        RtlDebugPrint("Loading %c%c%c%c\n", Name[0], Name[1], Name[2], Name[3]);
    }

    Status = AcpipExecuteAml(ExecutionContext);
    if (!KSUCCESS(Status)) {
        goto LoadDefinitionBlockEnd;
    }

LoadDefinitionBlockEnd:
    if (ExecutionContext != NULL) {
        AcpipDestroyAmlExecutionContext(ExecutionContext);
    }

    if (!KSUCCESS(Status)) {
        if (LoadedBlock != NULL) {
            AcpiUnloadDefinitionBlock(Handle);
        }
    }

    return Status;
}

VOID
AcpiUnloadDefinitionBlock (
    PACPI_OBJECT Handle
    )

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

{

    PLIST_ENTRY CurrentEntry;
    PACPI_LOADED_DEFINITION_BLOCK LoadedBlock;
    PACPI_OBJECT Object;

    CurrentEntry = AcpiLoadedDefinitionBlockList.Next;
    while (CurrentEntry != &AcpiLoadedDefinitionBlockList) {
        LoadedBlock = LIST_VALUE(CurrentEntry,
                                 ACPI_LOADED_DEFINITION_BLOCK,
                                 ListEntry);

        CurrentEntry = CurrentEntry->Next;
        if ((Handle == NULL) || (LoadedBlock->HandleObject == Handle)) {
            LIST_REMOVE(&(LoadedBlock->ListEntry));
            if (LoadedBlock->HandleObject != NULL) {
                AcpipObjectReleaseReference(LoadedBlock->HandleObject);
            }

            //
            // Destroy all the namespace objects created by this definition
            // block.
            //

            while (!LIST_EMPTY(&(LoadedBlock->ObjectList))) {
                Object = LIST_VALUE(LoadedBlock->ObjectList.Next,
                                    ACPI_OBJECT,
                                    DestructorListEntry);

                LIST_REMOVE(&(Object->DestructorListEntry));
                Object->DestructorListEntry.Next = NULL;
                AcpipObjectReleaseReference(Object);
            }

            //
            // Free the table as well if this came with a handle. The main
            // DSDT and SSDTs do not have handles, but every AML Load
            // instruction does.
            //

            if (LoadedBlock->HandleObject != NULL) {
                AcpipFreeMemory(LoadedBlock->Code);
            }

            AcpipFreeMemory(LoadedBlock);
        }
    }

    return;
}

KSTATUS
AcpiExecuteMethod (
    PACPI_OBJECT MethodObject,
    PACPI_OBJECT *Arguments,
    ULONG ArgumentCount,
    ACPI_OBJECT_TYPE ReturnType,
    PACPI_OBJECT *ReturnValue
    )

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

{

    PACPI_OBJECT ConvertedReturnObject;
    PAML_EXECUTION_CONTEXT ExecutionContext;
    ULONG ExecutionOptions;
    PSTR Name;
    PACPI_OBJECT ReturnObject;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    ExecutionContext = NULL;
    ReturnObject = NULL;
    if (MethodObject->Type != AcpiObjectMethod) {
        ReturnObject = MethodObject;
        AcpipObjectAddReference(MethodObject);
        Status = STATUS_SUCCESS;
        goto ExecuteMethodEnd;
    }

    //
    // Fire up an execution context.
    //

    ExecutionOptions = AML_EXECUTION_OPTION_RUN;
    ExecutionContext = AcpipCreateAmlExecutionContext(ExecutionOptions);
    if (ExecutionContext == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ExecuteMethodEnd;
    }

    //
    // Push a default method context onto the execution context that spans the
    // entire block being loaded.
    //

    Status = AcpipPushMethodOnExecutionContext(
                                       ExecutionContext,
                                       MethodObject,
                                       MethodObject->U.Method.OsMutex,
                                       MethodObject->U.Method.IntegerWidthIs32,
                                       MethodObject->U.Method.AmlCode,
                                       MethodObject->U.Method.AmlCodeSize,
                                       ArgumentCount,
                                       Arguments);

    if (!KSUCCESS(Status)) {
        goto ExecuteMethodEnd;
    }

    if (ExecutionContext->PrintStatements != FALSE) {
        Name = (PSTR)&(MethodObject->Name);
        RtlDebugPrint("Executing %c%c%c%c\n",
                      Name[0],
                      Name[1],
                      Name[2],
                      Name[3]);
    }

    Status = AcpipExecuteAml(ExecutionContext);
    if (!KSUCCESS(Status)) {
        goto ExecuteMethodEnd;
    }

    //
    // If a return value is requested, pluck it out of the context list and
    // convert it to the desired object type.
    //

    if (ReturnValue != NULL) {
        ReturnObject = ExecutionContext->ReturnValue;
        if (ReturnObject != NULL) {
            while (ReturnObject->Type == AcpiObjectAlias) {

                ASSERT(ReturnObject->U.Alias.DestinationObject != NULL);

                ReturnObject = ReturnObject->U.Alias.DestinationObject;
            }

            if ((ReturnType != AcpiObjectUninitialized) &&
                (ReturnObject->Type != ReturnType)) {

                ConvertedReturnObject = AcpipConvertObjectType(ExecutionContext,
                                                               ReturnObject,
                                                               ReturnType);

                if (ConvertedReturnObject == NULL) {
                    RtlDebugPrint("ACPI: Failed to convert object 0x%x "
                                  "(type %d) to return type %d.\n",
                                  ReturnObject,
                                  ReturnObject->Type,
                                  ReturnType);
                }

                ReturnObject = ConvertedReturnObject;

            } else {

                //
                // Dereference field units, since no one ever wants to get one
                // of those back.
                //

                if (ReturnObject->Type == AcpiObjectFieldUnit) {
                    Status = AcpipReadFromField(ExecutionContext,
                                                ReturnObject,
                                                &ReturnObject);

                    if (!KSUCCESS(Status)) {
                        RtlDebugPrint("ACPI: Failed to read from field for "
                                      "return value conversion: %x.\n",
                                      Status);

                        goto ExecuteMethodEnd;
                    }

                } else {
                    AcpipObjectAddReference(ReturnObject);
                }
            }
        }
    }

ExecuteMethodEnd:
    if (ExecutionContext != NULL) {
        AcpipDestroyAmlExecutionContext(ExecutionContext);
    }

    if (ReturnValue != NULL) {
        *ReturnValue = ReturnObject;
    }

    return Status;
}

KSTATUS
AcpipInitializeAmlInterpreter (
    VOID
    )

/*++

Routine Description:

    This routine initializes the ACPI AML interpreter and global namespace.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT Argument;
    ULONGLONG ArgumentValue;
    PDESCRIPTION_HEADER DsdtTable;
    INTERRUPT_MODEL InterruptModel;
    PACPI_OBJECT PicMethod;
    PDESCRIPTION_HEADER SsdtTable;
    KSTATUS Status;

    Argument = NULL;
    INITIALIZE_LIST_HEAD(&AcpiLoadedDefinitionBlockList);

    //
    // Initialize operating system specific support.
    //

    Status = AcpipInitializeOperatingSystemAmlSupport();
    if (!KSUCCESS(Status)) {
        goto InitializeAmlInterpreterEnd;
    }

    //
    // Initialize the global namespace.
    //

    Status = AcpipInitializeNamespace();
    if (!KSUCCESS(Status)) {
        goto InitializeAmlInterpreterEnd;
    }

    //
    // Load the DSDT.
    //

    DsdtTable = AcpiFindTable(DSDT_SIGNATURE, NULL);
    if (DsdtTable != NULL) {
        Status = AcpiLoadDefinitionBlock(DsdtTable, NULL);
        if (!KSUCCESS(Status)) {
            goto InitializeAmlInterpreterEnd;
        }
    }

    //
    // Load all SSDT tables.
    //

    SsdtTable = NULL;
    while (TRUE) {
        SsdtTable = AcpiFindTable(SSDT_SIGNATURE, SsdtTable);
        if (SsdtTable == NULL) {
            break;
        }

        Status = AcpiLoadDefinitionBlock(SsdtTable, NULL);
        if (!KSUCCESS(Status)) {
            goto InitializeAmlInterpreterEnd;
        }
    }

    //
    // Run any _INI methods. The DSDT may depend on the SSDT, so the _INI
    // methods cannot be run until after all tables have loaded.
    //

    Status = AcpipRunInitializationMethods(NULL);
    if (!KSUCCESS(Status)) {
        goto InitializeAmlInterpreterEnd;
    }

    //
    // Get the current interrupt model.
    //

    InterruptModel = HlGetInterruptModel();
    if (InterruptModel == InterruptModelPic) {
        ArgumentValue = ACPI_INTERRUPT_PIC_MODEL;

    } else if (InterruptModel == InterruptModelApic) {
        ArgumentValue = ACPI_INTERRUPT_APIC_MODEL;

    } else {

        ASSERT(FALSE);

        Status = STATUS_INVALID_CONFIGURATION;
        goto InitializeAmlInterpreterEnd;
    }

    //
    // Attempt to call the \_PIC routine to tell the firmware which interrupt
    // model is in use.
    //

    PicMethod = AcpipFindNamedObject(AcpipGetNamespaceRoot(),
                                     ACPI_METHOD__PIC);

    if (PicMethod != NULL) {
        Argument = AcpipCreateNamespaceObject(NULL,
                                              AcpiObjectInteger,
                                              NULL,
                                              &ArgumentValue,
                                              sizeof(ULONGLONG));

        if (Argument == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializeAmlInterpreterEnd;
        }

        Status = AcpiExecuteMethod(PicMethod,
                                   &Argument,
                                   1,
                                   0,
                                   NULL);

        if (!KSUCCESS(Status)) {
            goto InitializeAmlInterpreterEnd;
        }
    }

InitializeAmlInterpreterEnd:
    if (Argument != NULL) {
        AcpipObjectReleaseReference(Argument);
    }

    if (!KSUCCESS(Status)) {

        //
        // Unload everything.
        //

        AcpiUnloadDefinitionBlock(NULL);
    }

    return Status;
}

UCHAR
AcpipChecksumData (
    PVOID Address,
    ULONG Length
    )

/*++

Routine Description:

    This routine sums all of the bytes in a given buffer. In a correctly
    checksummed region, the result should be zero.

Arguments:

    Address - Supplies the address of the region to checksum.

    Length - Supplies the length of the region, in bytes.

Return Value:

    Returns the sum of all the bytes in the region.

--*/

{

    PBYTE CurrentByte;
    BYTE Sum;

    CurrentByte = Address;
    Sum = 0;
    while (Length != 0) {
        Sum += *CurrentByte;
        CurrentByte += 1;
        Length -= 1;
    }

    return Sum;
}

VOID
AcpipPopExecutingStatements (
    PAML_EXECUTION_CONTEXT Context,
    BOOL PopToWhile,
    BOOL ContinueWhile
    )

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

{

    PLIST_ENTRY CurrentEntry;
    PAML_STATEMENT Statement;

    //
    // Don't touch the current statement, but pop statements off behind it
    // until the while is reached or bust.
    //

    while (TRUE) {
        CurrentEntry = Context->StatementStackHead.Next->Next;
        if (CurrentEntry == &(Context->StatementStackHead)) {

            ASSERT(PopToWhile == FALSE);

            break;
        }

        Statement = LIST_VALUE(CurrentEntry, AML_STATEMENT, ListEntry);

        //
        // Assert that the statement wasn't evaluating arguments, but is just
        // here to define scope. Something like Increment (Break) isn't allowed.
        //

        ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

        //
        // If this is a while and that's what's being sought, stop (or pop it
        // off too as the last one.)
        //

        if (PopToWhile != FALSE) {
            if (Statement->Type == AmlStatementWhile) {
                if (ContinueWhile == FALSE) {
                    LIST_REMOVE(CurrentEntry);
                    AcpipDestroyStatement(Statement);
                }

                break;
            }

        } else {
            if (Statement->Type == AmlStatementExecutingMethod) {
                break;
            }
        }

        //
        // Destroy the statement.
        //

        LIST_REMOVE(CurrentEntry);
        AcpipDestroyStatement(Statement);

        ASSERT(Context->IndentationLevel != 0);

        Context->IndentationLevel -= 1;
    }

    return;
}

VOID
AcpipPrintIndentedNewLine (
    PAML_EXECUTION_CONTEXT Context
    )

/*++

Routine Description:

    This routine prints a newline and then a number of space characters
    corresponding to the current indentation level.

Arguments:

    Context - Supplies a pointer to the AML execution context.

Return Value:

    None.

--*/

{

    ULONG SpaceIndex;

    if (Context->PrintStatements == FALSE) {
        return;
    }

    ASSERT(Context->IndentationLevel < 1000);

    RtlDebugPrint("\n");
    for (SpaceIndex = 0;
         SpaceIndex < Context->IndentationLevel;
         SpaceIndex += 1) {

        RtlDebugPrint("  ");
    }

    return;
}

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
    )

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

{

    ULONG ArgumentIndex;
    PAML_METHOD_EXECUTION_CONTEXT NewMethod;
    BOOL Result;
    KSTATUS Status;

    //
    // If a method is being executed that is actually covered by a C function,
    // then run the C function now and return. The C function is responsible
    // for setting the return value.
    //

    if ((AmlCode == NULL) && (AmlCodeSize == 0) && (Scope != NULL) &&
        (Scope->Type == AcpiObjectMethod) &&
        (Scope->U.Method.Function != NULL)) {

        Status = Scope->U.Method.Function(Context,
                                          Scope,
                                          Arguments,
                                          ArgumentCount);

        goto PushMethodOnExecutionContextEnd;
    }

    //
    // If it's an empty function, just set the return value to zero.
    //

    if (AmlCodeSize == 0) {
        if (Context->ReturnValue != NULL) {
            AcpipObjectReleaseReference(Context->ReturnValue);
        }

        Context->ReturnValue = &AcpiZero;
        AcpipObjectAddReference(Context->ReturnValue);
        Status = STATUS_SUCCESS;
        goto PushMethodOnExecutionContextEnd;
    }

    //
    // Allocate space for the new method.
    //

    NewMethod = AcpipAllocateMemory(sizeof(AML_METHOD_EXECUTION_CONTEXT));
    if (NewMethod == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto PushMethodOnExecutionContextEnd;
    }

    RtlZeroMemory(NewMethod, sizeof(AML_METHOD_EXECUTION_CONTEXT));

    //
    // Initialize the method context.
    //

    NewMethod->CallingMethodContext = Context->CurrentMethod;
    NewMethod->MethodMutex = MethodMutex;
    NewMethod->IntegerWidthIs32 = IntegerWidthIs32;
    INITIALIZE_LIST_HEAD(&(NewMethod->CreatedObjectsListHead));
    NewMethod->SavedAmlCode = Context->AmlCode;
    NewMethod->SavedAmlCodeSize = Context->AmlCodeSize;
    NewMethod->SavedCurrentOffset = Context->CurrentOffset;
    NewMethod->SavedIndentationLevel = Context->IndentationLevel;
    NewMethod->SavedCurrentScope = Context->CurrentScope;
    NewMethod->LastLocalIndex = AML_INVALID_LOCAL_INDEX;
    if (ArgumentCount != 0) {
        for (ArgumentIndex = 0;
             ArgumentIndex < ArgumentCount;
             ArgumentIndex += 1) {

            NewMethod->Argument[ArgumentIndex] = Arguments[ArgumentIndex];
            AcpipObjectAddReference(NewMethod->Argument[ArgumentIndex]);
        }
    }

    //
    // Acquire the method mutex if there is one.
    //

    if (MethodMutex != NULL) {
        Result = AcpipAcquireMutex(Context,
                                   MethodMutex,
                                   ACPI_MUTEX_WAIT_INDEFINITELY);

        ASSERT(Result != FALSE);
    }

    //
    // Set this context as the current one.
    //

    Context->CurrentMethod = NewMethod;
    Context->AmlCode = AmlCode;
    Context->AmlCodeSize = AmlCodeSize;
    Context->CurrentOffset = 0;
    if (Scope == NULL) {
        Scope = AcpipGetNamespaceRoot();
    }

    Context->CurrentScope = Scope;
    Status = STATUS_SUCCESS;

PushMethodOnExecutionContextEnd:
    return Status;
}

VOID
AcpipPopCurrentMethodContext (
    PAML_EXECUTION_CONTEXT Context
    )

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

{

    PLIST_ENTRY CurrentEntry;
    ULONG Index;
    PAML_METHOD_EXECUTION_CONTEXT Method;
    PACPI_OBJECT Object;

    Method = Context->CurrentMethod;
    if (Method == NULL) {
        return;
    }

    //
    // Delete the previous statement manually here if there was one.
    //

    if (Context->PreviousStatement != NULL) {
        AcpipDestroyStatement(Context->PreviousStatement);
        Context->PreviousStatement = NULL;
    }

    //
    // Destroy all locals.
    //

    for (Index = 0; Index < MAX_AML_LOCAL_COUNT; Index += 1) {
        if (Method->LocalVariable[Index] != NULL) {
            AcpipObjectReleaseReference(Method->LocalVariable[Index]);
        }
    }

    //
    // Destroy all arguments.
    //

    for (Index = 0; Index < MAX_AML_METHOD_ARGUMENT_COUNT; Index += 1) {
        if (Method->Argument[Index] != NULL) {
            AcpipObjectReleaseReference(Method->Argument[Index]);
        }
    }

    //
    // Destroy all objects created during this context.
    //

    CurrentEntry = Method->CreatedObjectsListHead.Next;
    while (CurrentEntry != &(Method->CreatedObjectsListHead)) {
        Object = LIST_VALUE(CurrentEntry,
                            ACPI_OBJECT,
                            DestructorListEntry);

        CurrentEntry = CurrentEntry->Next;

        //
        // Pull the object off of the destructor list in case decrementing its
        // reference count does NOT kill it. That would be bad because when it
        // did finally get destroyed, it would follow a probably freed
        // destructor list entry pointer.
        //

        LIST_REMOVE(&(Object->DestructorListEntry));
        Object->DestructorListEntry.Next = NULL;
        AcpipObjectReleaseReference(Object);
    }

    //
    // Release the implicit method mutex if it was acquired.
    //

    if (Method->MethodMutex != NULL) {
        AcpipReleaseMutex(Context, Method->MethodMutex);
    }

    //
    // Pop the saved values back into the current context.
    //

    Context->CurrentMethod = Method->CallingMethodContext;
    Context->AmlCode = Method->SavedAmlCode;
    Context->AmlCodeSize = Method->SavedAmlCodeSize;
    Context->CurrentOffset = Method->SavedCurrentOffset;
    Context->IndentationLevel = Method->SavedIndentationLevel;
    Context->CurrentScope = Method->SavedCurrentScope;

    //
    // Free this object and return.
    //

    AcpipFreeMemory(Method);
    return;
}

KSTATUS
AcpipRunInitializationMethods (
    PACPI_OBJECT RootObject
    )

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

{

    PACPI_OBJECT CurrentObject;
    PACPI_OBJECT PreviousObject;
    PACPI_OBJECT PreviousSibling;
    KSTATUS Status;
    BOOL TraverseDown;

    if (RootObject == NULL) {
        RootObject = AcpipGetSystemBusRoot();
    }

    CurrentObject = RootObject;
    PreviousObject = CurrentObject->Parent;
    while (CurrentObject != NULL) {

        //
        // If this is the first time the node is being visited (via parent or
        // sibling, but not child), then process it.
        //

        PreviousSibling = LIST_VALUE(CurrentObject->SiblingListEntry.Previous,
                                     ACPI_OBJECT,
                                     SiblingListEntry);

        if ((PreviousObject == CurrentObject->Parent) ||
            ((CurrentObject->SiblingListEntry.Previous != NULL) &&
             (PreviousObject == PreviousSibling))) {

            TraverseDown = TRUE;
            if (CurrentObject->Type == AcpiObjectDevice) {
                Status = AcpipRunDeviceInitialization(CurrentObject,
                                                      &TraverseDown);

                if (!KSUCCESS(Status)) {
                    goto RunInitializationMethodsEnd;
                }
            }

            //
            // Move to the first child if eligible.
            //

            PreviousObject = CurrentObject;
            if ((TraverseDown != FALSE) &&
                (LIST_EMPTY(&(CurrentObject->ChildListHead)) == FALSE)) {

                CurrentObject = LIST_VALUE(CurrentObject->ChildListHead.Next,
                                           ACPI_OBJECT,
                                           SiblingListEntry);

            //
            // Move to the next sibling if possible.
            //

            } else if ((CurrentObject != RootObject) &&
                       (CurrentObject->SiblingListEntry.Next !=
                        &(CurrentObject->Parent->ChildListHead))) {

                CurrentObject = LIST_VALUE(CurrentObject->SiblingListEntry.Next,
                                           ACPI_OBJECT,
                                           SiblingListEntry);

            //
            // There are no children and this is the last sibling, move up to
            // the parent.
            //

            } else {

                //
                // This case only gets hit if the root is the only node in the
                // tree.
                //

                if (CurrentObject == RootObject) {
                    CurrentObject = NULL;

                } else {
                    CurrentObject = CurrentObject->Parent;
                }
            }

        //
        // If the node is popping up from the previous, attempt to move to
        // the next sibling, or up the tree.
        //

        } else {
            PreviousObject = CurrentObject;
            if (CurrentObject == RootObject) {
                CurrentObject = NULL;

            } else if (CurrentObject->SiblingListEntry.Next !=
                       &(CurrentObject->Parent->ChildListHead)) {

                CurrentObject = LIST_VALUE(CurrentObject->SiblingListEntry.Next,
                                           ACPI_OBJECT,
                                           SiblingListEntry);

            } else {
                CurrentObject = CurrentObject->Parent;
            }
        }
    }

    Status = STATUS_SUCCESS;

RunInitializationMethodsEnd:
    return Status;
}

KSTATUS
AcpipOsiMethod (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT Method,
    PACPI_OBJECT *Arguments,
    ULONG ArgumentCount
    )

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

{

    PACPI_OBJECT Argument;
    PACPI_OBJECT ConvertedArgument;
    PCSTR *Default;
    ULONGLONG Result;
    PCSTR ResultString;
    KSTATUS Status;

    ConvertedArgument = NULL;
    Result = OSI_BEHAVIOR_UNSUPPORTED;
    Status = STATUS_SUCCESS;
    if (ArgumentCount != 1) {
        RtlDebugPrint("ACPI: Warning: _OSI called with %u arguments.\n",
                      ArgumentCount);

        goto OsiMethodEnd;
    }

    Argument = Arguments[0];
    if (Argument->Type != AcpiObjectString) {
        ConvertedArgument = AcpipConvertObjectType(Context,
                                                   Argument,
                                                   AcpiObjectString);

        if (ConvertedArgument == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto OsiMethodEnd;
        }

        Argument = ConvertedArgument;
    }

    ASSERT(Argument->Type == AcpiObjectString);

    Default = AcpiDefaultOsiStrings;
    while (*Default != NULL) {
        if (RtlAreStringsEqual(*Default, Argument->U.String.String, -1) !=
            FALSE) {

            Result = OSI_BEHAVIOR_SUPPORTED;
            break;
        }

        Default += 1;
    }

    if (Result == OSI_BEHAVIOR_UNSUPPORTED) {
        if (AcpipCheckOsiSupport(Argument->U.String.String) != FALSE) {
            Result = OSI_BEHAVIOR_SUPPORTED;
        }
    }

    if (AcpiPrintOsiRequests != FALSE) {
        ResultString = "Unsupported";
        if (Result == OSI_BEHAVIOR_SUPPORTED) {
            ResultString = "Supported";
        }

        RtlDebugPrint("_OSI Request \"%s\": %s\n",
                      Argument->U.String.String,
                      ResultString);
    }

OsiMethodEnd:

    //
    // Set the return value integer.
    //

    if (Context->ReturnValue != NULL) {
        AcpipObjectReleaseReference(Context->ReturnValue);
    }

    Context->ReturnValue = AcpipCreateNamespaceObject(Context,
                                                      AcpiObjectInteger,
                                                      NULL,
                                                      &Result,
                                                      sizeof(Result));

    if (Context->ReturnValue == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

PAML_EXECUTION_CONTEXT
AcpipCreateAmlExecutionContext (
    ULONG Options
    )

/*++

Routine Description:

    This routine creates an ACPI execution context.

Arguments:

    Options - Supplies a bitfield of options that govern the behavior of the
        execution context. See AML_EXECUTION_OPTION_* bitfield definitions.

Return Value:

    Status code.

--*/

{

    PAML_EXECUTION_CONTEXT NewContext;

    //
    // Allocate space for the context.
    //

    NewContext = AcpipAllocateMemory(sizeof(AML_EXECUTION_CONTEXT));
    if (NewContext == NULL) {
        goto CreateAmlExecutionContextEnd;
    }

    RtlZeroMemory(NewContext, sizeof(AML_EXECUTION_CONTEXT));

    //
    // Use the debug options if specified.
    //

    if (AcpiDebugExecutionOptions != 0) {
        RtlDebugPrint("ACPI: Overriding AML execution options from 0x%08x to "
                      "0x%08x.\n",
                      Options,
                      AcpiDebugExecutionOptions);

        Options = AcpiDebugExecutionOptions;
    }

    //
    // Set the options.
    //

    if ((Options & AML_EXECUTION_OPTION_RUN) != 0) {
        NewContext->ExecuteStatements = TRUE;
    }

    if ((Options & AML_EXECUTION_OPTION_PRINT) != 0) {
        NewContext->PrintStatements = TRUE;
    }

    INITIALIZE_LIST_HEAD(&(NewContext->StatementStackHead));

CreateAmlExecutionContextEnd:
    return NewContext;
}

VOID
AcpipDestroyAmlExecutionContext (
    PAML_EXECUTION_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys an ACPI execution context.

Arguments:

    Context - Supplies a pointer to the execution context.

Return Value:

    None.

--*/

{

    //
    // Destroy the current method context.
    //

    if (Context->CurrentMethod != NULL) {
        AcpipPopCurrentMethodContext(Context);

        ASSERT(Context->CurrentMethod == NULL);
    }

    //
    // Destroy the return value. The caller had better upped the reference count
    // if it was desired.
    //

    if (Context->ReturnValue != NULL) {
        AcpipObjectReleaseReference(Context->ReturnValue);
    }

    AcpipFreeMemory(Context);
    return;
}

KSTATUS
AcpipExecuteAml (
    PAML_EXECUTION_CONTEXT Context
    )

/*++

Routine Description:

    This routine executes a block of ACPI AML.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context.

Return Value:

    Status code indicating whether the given block of AML was processed
    successfully.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PAML_STATEMENT Statement;
    KSTATUS Status;

    //
    // Loop while not all of the AML has been processed.
    //

    while (TRUE) {

        //
        // Attempt to process the currently executing statement at the top
        // of the stack.
        //

        if (LIST_EMPTY(&(Context->StatementStackHead)) == FALSE) {
            CurrentEntry = Context->StatementStackHead.Next;
            Statement = LIST_VALUE(CurrentEntry, AML_STATEMENT, ListEntry);

            //
            // Attempt to evaluate the statement at the top of the stack.
            //

            Status = AcpipEvaluateStatement(Context, Statement);

            //
            // If there was a previous statement, free it.
            //

            if (Context->PreviousStatement != NULL) {
                AcpipDestroyStatement(Context->PreviousStatement);
                Context->PreviousStatement = NULL;
            }

            //
            // If the statement executed successfully, save it as the previous
            // statement, and pop up the stack to hand it to the parent
            // instruction.
            //

            if (KSUCCESS(Status)) {
                LIST_REMOVE(&(Statement->ListEntry));
                Context->PreviousStatement = Statement;

                //
                // Check to see if the previous statement resolved to a method.
                // If it did, push an executing method statement on to gather
                // arguments and then execute the method.
                //

                if ((Statement->Reduction != NULL) &&
                    (Statement->Reduction->Type == AcpiObjectMethod)) {

                    Status = AcpipCreateExecutingMethodStatement(Context,
                                                                 &Statement);

                    if (!KSUCCESS(Status)) {
                        goto ExecuteAmlEnd;
                    }

                    INSERT_AFTER(&(Statement->ListEntry),
                                 &(Context->StatementStackHead));
                }

                continue;
            }

            //
            // Bail if the error was anything other than "not done yet".
            //

            if (Status != STATUS_MORE_PROCESSING_REQUIRED) {

                ASSERT(FALSE);

                goto ExecuteAmlEnd;
            }
        }

        //
        // If there was a previous statement, free it.
        //

        if (Context->PreviousStatement != NULL) {
            AcpipDestroyStatement(Context->PreviousStatement);
            Context->PreviousStatement = NULL;
        }

        //
        // If this is the end of the AML code, finish.
        //

        if (Context->CurrentOffset == Context->AmlCodeSize) {

            //
            // All statements had better be done.
            //

            ASSERT(LIST_EMPTY(&(Context->StatementStackHead)) != FALSE);

            AcpipPrintIndentedNewLine(Context);
            break;
        }

        //
        // If the list was empty, this is definitely the beginning of a new
        // statement, so print a newline.
        //

        if (LIST_EMPTY(&(Context->StatementStackHead)) != FALSE) {
            AcpipPrintIndentedNewLine(Context);
        }

        //
        // Create the next AML statement and put it on the stack.
        //

        Status = AcpipCreateNextStatement(Context, &Statement);
        if (!KSUCCESS(Status)) {
            goto ExecuteAmlEnd;
        }

        INSERT_AFTER(&(Statement->ListEntry), &(Context->StatementStackHead));
    }

    Status = STATUS_SUCCESS;

ExecuteAmlEnd:
    return Status;
}

KSTATUS
AcpipCreateNextStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT *NextStatement
    )

/*++

Routine Description:

    This routine creates the next AML statement based on the current AML
    execution context.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where a pointer to the next statement
        will be returned. The caller is responsible for freeing this memory.

Return Value:

    Status code indicating whether a statement was successfully created.

--*/

{

    PAML_CREATE_NEXT_STATEMENT_ROUTINE CreateNextStatementRoutine;
    UCHAR FirstByte;
    PAML_STATEMENT Statement;
    KSTATUS Status;

    ASSERT(Context->CurrentOffset < Context->AmlCodeSize);

    //
    // Allocate and initialize the next statement structure.
    //

    Statement = AcpipCreateStatement();
    if (Statement == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateNextStatementEnd;
    }

    //
    // Get the first byte of the opcode, and use that as an index into the table
    // of functions that create the correct statement based on the opcode byte.
    //

    FirstByte = *((PUCHAR)(Context->AmlCode) + Context->CurrentOffset);
    CreateNextStatementRoutine = AcpiCreateStatement[FirstByte];
    Status = CreateNextStatementRoutine(Context, Statement);
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("\nACPI: Failed to create statement. "
                      "Status %d, Routine 0x%x, Context 0x%x\n",
                      Status,
                      CreateNextStatementRoutine,
                      Context);

        ASSERT(FALSE);

        goto CreateNextStatementEnd;
    }

CreateNextStatementEnd:
    if (!KSUCCESS(Status)) {
        if (Statement != NULL) {
            AcpipFreeMemory(Statement);
            Statement = NULL;
        }
    }

    *NextStatement = Statement;
    return Status;
}

VOID
AcpipDestroyStatement (
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine destroys an AML statement object.

Arguments:

    Statement - Supplies a pointer to the statement to destroy. This routine
        will also free any objects in the arguments list that are not owned
        by a namespace.

Return Value:

    None.

--*/

{

    ULONG ArgumentIndex;

    for (ArgumentIndex = 0;
         ArgumentIndex < Statement->ArgumentsAcquired;
         ArgumentIndex += 1) {

        if (Statement->Argument[ArgumentIndex] != NULL) {
            AcpipObjectReleaseReference(Statement->Argument[ArgumentIndex]);
        }
    }

    if (Statement->Reduction != NULL) {
        AcpipObjectReleaseReference(Statement->Reduction);
    }

    AcpipFreeMemory(Statement);
    return;
}

KSTATUS
AcpipEvaluateStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine attempts to evaluate an AML statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PAML_EVALUATE_STATEMENT_ROUTINE EvaluateRoutine;
    KSTATUS Status;

    EvaluateRoutine = AcpiEvaluateStatement[Statement->Type];

    ASSERT(EvaluateRoutine != NULL);

    Status = EvaluateRoutine(Context, Statement);
    if ((!KSUCCESS(Status)) && (Status != STATUS_MORE_PROCESSING_REQUIRED)) {
        RtlDebugPrint("\nACPI: Failed to evaluate AML statement. Status: %d, "
                      "Context 0x%x, Statement 0x%x\n",
                      Status,
                      Context,
                      Statement);

        ASSERT(FALSE);

        goto EvaluateStatementEnd;
    }

    //
    // If the statement is not a local type, then the local index needs to be
    // cleared. It should not persist to the next statement.
    //

    if ((Statement->Type != AmlStatementLocal) &&
        (Context->CurrentMethod != NULL)) {

        Context->CurrentMethod->LastLocalIndex = AML_INVALID_LOCAL_INDEX;
    }

EvaluateStatementEnd:
    return Status;
}

PAML_STATEMENT
AcpipCreateStatement (
    )

/*++

Routine Description:

    This routine allocates and initializes a blank AML statement.

Arguments:

    None.

Return Value:

    Returns a pointer to the allocated statement on success.

    NULL on allocation failure.

--*/

{

    PAML_STATEMENT Statement;

    //
    // Allocate the next statement structure.
    //

    Statement = AcpipAllocateMemory(sizeof(AML_STATEMENT));
    if (Statement == NULL) {
        goto CreateStatementEnd;
    }

    //
    // Initialize just the essential fields, as this is a very hot path.
    //

    Statement->Reduction = NULL;
    Statement->ArgumentsAcquired = 0;

CreateStatementEnd:
    return Statement;
}

KSTATUS
AcpipCreateExecutingMethodStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT *NextStatement
    )

/*++

Routine Description:

    This routine creates an executing method statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context.

    NextStatement - Supplies a pointer where a pointer to the next statement
        will be returned. The caller is responsible for freeing this memory.

Return Value:

    Status code indicating whether a statement was successfully created.

--*/

{

    PAML_STATEMENT Statement;
    KSTATUS Status;

    Statement = AcpipCreateStatement();
    if (Statement == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateExecutingMethodStatementEnd;
    }

    Statement->Type = AmlStatementExecutingMethod;
    Statement->ArgumentsNeeded = 0;
    Statement->ArgumentsAcquired = 0;

    //
    // Set additional data to NULL to indicate the first time the executing
    // method statement is evaluated.
    //

    Statement->AdditionalData = (UINTN)NULL;
    Status = STATUS_SUCCESS;

    //
    // Initialize Additional Data 2 to zero for now. It will eventually hold
    // the original method context.
    //

    Statement->AdditionalData2 = 0;

CreateExecutingMethodStatementEnd:
    *NextStatement = Statement;
    return Status;
}

KSTATUS
AcpipRunDeviceInitialization (
    PACPI_OBJECT Device,
    PBOOL TraverseDown
    )

/*++

Routine Description:

    This routine runs the _INI initialization method on a device, if it exists.

Arguments:

    Device - Supplies a pointer to the device to initialize.

    TraverseDown - Supplies a pointer where a boolean will be returned
        indicating whether or not any of the device's children should be
        initialized.

Return Value:

    Status code. Failure means something serious went wrong, not just that the
    device returned a non-functioning status.

--*/

{

    ULONG DeviceStatus;
    BOOL EvaluateChildren;
    PACPI_OBJECT InitializationMethod;
    KSTATUS Status;

    ASSERT(Device->Type == AcpiObjectDevice);

    DeviceStatus = ACPI_DEFAULT_DEVICE_STATUS;
    EvaluateChildren = TRUE;
    Status = AcpipGetDeviceStatus(Device, &DeviceStatus);
    if (!KSUCCESS(Status)) {
        goto RunDeviceInitializationEnd;
    }

    //
    // Do not evaluate children if the device is neither present nor functional.
    //

    if (((DeviceStatus & ACPI_DEVICE_STATUS_FUNCTIONING_PROPERLY) == 0) &&
        ((DeviceStatus & ACPI_DEVICE_STATUS_PRESENT) == 0)) {

        EvaluateChildren = FALSE;
    }

    //
    // If the device is not present, do not run the _INI method.
    //

    if ((DeviceStatus & ACPI_DEVICE_STATUS_PRESENT) == 0) {
        goto RunDeviceInitializationEnd;
    }

    InitializationMethod = AcpipFindNamedObject(Device, ACPI_METHOD__INI);
    if (InitializationMethod == NULL) {
        goto RunDeviceInitializationEnd;
    }

    Status = AcpiExecuteMethod(InitializationMethod, NULL, 0, 0, NULL);
    if (!KSUCCESS(Status)) {
        goto RunDeviceInitializationEnd;
    }

RunDeviceInitializationEnd:
    *TraverseDown = EvaluateChildren;
    return Status;
}

