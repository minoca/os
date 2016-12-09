/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    namespce.c

Abstract:

    This module implements support for the ACPI namespace.

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
#include "oprgn.h"
#include "namespce.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro determines whether or not an ACPI object is one of the interger
// constants.
//

#define IS_ACPI_CONSTANT(_AcpiObject)  \
    (((_AcpiObject) == &AcpiZero) ||   \
     ((_AcpiObject) == &AcpiOne) ||    \
     ((_AcpiObject) == &AcpiOnes32) || \
     ((_AcpiObject) == &AcpiOnes64))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the name of the system bus ACPI object.
//

#define ACPI_SYSTEM_BUS_OBJECT_NAME_STRING "_SB_"

//
// Define the name of the processor object.
//

#define ACPI_PROCESSOR_OBJECT_NAME_STRING "_PR_"

//
// Define the name of the General Purpose Event block object.
//

#define ACPI_GENERAL_PURPOSE_EVENT_OBJECT_NAME_STRING "_GPE"

//
// Define the name of the Operating System name object.
//

#define ACPI_OPERATING_SYSTEM_NAME_OBJECT_NAME_STRING "_OS_"

//
// Define the name of the Operating System interface method object.
//

#define ACPI_OSI_METHOD_OBJECT_NAME_STRING "_OSI"

//
// Define the name of the supported revision integer.
//

#define ACPI_REV_INTEGER_NAME_STRING "_REV"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
AcpipDestroyNamespaceObject (
    PACPI_OBJECT Object
    );

PACPI_OBJECT
AcpipGetPartialNamespaceObject (
    PSTR Name,
    ULONG Length,
    PACPI_OBJECT CurrentScope
    );

KSTATUS
AcpipPullOffLastName (
    PSTR Name,
    PULONG LastName,
    PULONG LastNameOffset
    );

VOID
AcpipDebugOutputObject (
    PACPI_OBJECT Object
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the root object.
//

PACPI_OBJECT AcpiNamespaceRoot = NULL;

//
// Store a pointer to the \_SB object.
//

PACPI_OBJECT AcpiSystemBusRoot = NULL;

//
// Store a pointer to the old \_PR object.
//

PACPI_OBJECT AcpiProcessorRoot = NULL;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
AcpipInitializeNamespace (
    VOID
    )

/*++

Routine Description:

    This routine initializes the ACPI global namespace.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT GeneralEventObject;
    PACPI_OBJECT OperatingSystem;
    ULONG OperatingSystemStringLength;
    PACPI_OBJECT OsInterface;
    PACPI_OBJECT Revision;
    ULONGLONG RevisionValue;
    KSTATUS Status;

    GeneralEventObject = NULL;
    OperatingSystem = NULL;
    if (AcpiNamespaceRoot != NULL) {
        return STATUS_SUCCESS;
    }

    Status = STATUS_INSUFFICIENT_RESOURCES;
    AcpiNamespaceRoot = AcpipCreateNamespaceObject(NULL,
                                                   AcpiObjectUninitialized,
                                                   NULL,
                                                   NULL,
                                                   0);

    if (AcpiNamespaceRoot == NULL) {
        goto InitializeNamespaceEnd;
    }

    //
    // Create the objects defined by the ACPI specificiation to exist. Start
    // with \_SB.
    //

    AcpiSystemBusRoot = AcpipCreateNamespaceObject(
                                            NULL,
                                            AcpiObjectDevice,
                                            ACPI_SYSTEM_BUS_OBJECT_NAME_STRING,
                                            NULL,
                                            0);

    if (AcpiSystemBusRoot == NULL) {
        goto InitializeNamespaceEnd;
    }

    AcpipObjectReleaseReference(AcpiSystemBusRoot);

    //
    // Create \_PR.
    //

    AcpiProcessorRoot = AcpipCreateNamespaceObject(
                                             NULL,
                                             AcpiObjectUninitialized,
                                             ACPI_PROCESSOR_OBJECT_NAME_STRING,
                                             NULL,
                                             0);

    if (AcpiProcessorRoot == NULL) {
        goto InitializeNamespaceEnd;
    }

    AcpipObjectReleaseReference(AcpiProcessorRoot);

    //
    // Create \_GPE.
    //

    GeneralEventObject = AcpipCreateNamespaceObject(
                                 NULL,
                                 AcpiObjectUninitialized,
                                 ACPI_GENERAL_PURPOSE_EVENT_OBJECT_NAME_STRING,
                                 NULL,
                                 0);

    if (GeneralEventObject == NULL) {
        goto InitializeNamespaceEnd;
    }

    AcpipObjectReleaseReference(GeneralEventObject);

    //
    // Create \_OS.
    //

    OperatingSystemStringLength = RtlStringLength(ACPI_OPERATING_SYSTEM_NAME);
    OperatingSystem = AcpipCreateNamespaceObject(
                                 NULL,
                                 AcpiObjectString,
                                 ACPI_OPERATING_SYSTEM_NAME_OBJECT_NAME_STRING,
                                 ACPI_OPERATING_SYSTEM_NAME,
                                 OperatingSystemStringLength + 1);

    if (OperatingSystem == NULL) {
        goto InitializeNamespaceEnd;
    }

    AcpipObjectReleaseReference(OperatingSystem);

    //
    // Create \_OSI.
    //

    OsInterface = AcpipCreateNamespaceObject(NULL,
                                             AcpiObjectMethod,
                                             ACPI_OSI_METHOD_OBJECT_NAME_STRING,
                                             NULL,
                                             0);

    if (OsInterface == NULL) {
        goto InitializeNamespaceEnd;
    }

    OsInterface->U.Method.Function = AcpipOsiMethod;
    OsInterface->U.Method.ArgumentCount = 1;
    AcpipObjectReleaseReference(OsInterface);

    //
    // Create \_REV.
    //

    RevisionValue = ACPI_IMPLEMENTED_REVISION;
    Revision = AcpipCreateNamespaceObject(NULL,
                                          AcpiObjectInteger,
                                          ACPI_REV_INTEGER_NAME_STRING,
                                          &RevisionValue,
                                          sizeof(RevisionValue));

    if (Revision == NULL) {
        goto InitializeNamespaceEnd;
    }

    AcpipObjectReleaseReference(Revision);
    Status = STATUS_SUCCESS;

InitializeNamespaceEnd:
    if (!KSUCCESS(Status)) {
        if (AcpiNamespaceRoot != NULL) {
            AcpipObjectReleaseReference(AcpiNamespaceRoot);
            AcpiNamespaceRoot = NULL;
        }

        if (AcpiSystemBusRoot != NULL) {
            AcpipObjectReleaseReference(AcpiSystemBusRoot);
            AcpiSystemBusRoot = NULL;
        }

        if (AcpiProcessorRoot != NULL) {
            AcpipObjectReleaseReference(AcpiProcessorRoot);
            AcpiProcessorRoot = NULL;
        }

        if (GeneralEventObject != NULL) {
            AcpipObjectReleaseReference(GeneralEventObject);
        }

        if (OperatingSystem != NULL) {
            AcpipObjectReleaseReference(OperatingSystem);
        }
    }

    return Status;
}

PACPI_OBJECT
AcpipGetNamespaceRoot (
    VOID
    )

/*++

Routine Description:

    This routine returns the namespace root object. This routine does not
    modify the reference count of the object.

Arguments:

    None.

Return Value:

    Returns a pointer to the ACPI object on success.

    NULL on failure.

--*/

{

    return AcpiNamespaceRoot;
}

PACPI_OBJECT
AcpipGetSystemBusRoot (
    VOID
    )

/*++

Routine Description:

    This routine returns the system bus namespace object at \_SB. This routine
    does not modify the reference count of the object.

Arguments:

    None.

Return Value:

    Returns a pointer to the ACPI object on success.

    NULL on failure.

--*/

{

    return AcpiSystemBusRoot;
}

PACPI_OBJECT
AcpipGetProcessorRoot (
    VOID
    )

/*++

Routine Description:

    This routine returns the processor namespace directory at \_PR. This
    routine does not modify the reference count of the object.

Arguments:

    None.

Return Value:

    Returns a pointer to the ACPI object on success.

    NULL on failure.

--*/

{

    return AcpiProcessorRoot;
}

PACPI_OBJECT
AcpipFindNamedObject (
    PACPI_OBJECT ParentObject,
    ULONG Name
    )

/*++

Routine Description:

    This routine attempts to find and return an object of the given name under
    a given namespace object.

Arguments:

    ParentObject - Supplies a pointer to the namespace object whose children
        should be searched.

    Name - Supplies the name of the object to search for.

Return Value:

    Returns a pointer to the ACPI object on success. Its reference count is not
    incremented.

    NULL on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PACPI_OBJECT Object;

    CurrentEntry = ParentObject->ChildListHead.Next;
    while (CurrentEntry != &(ParentObject->ChildListHead)) {
        Object = LIST_VALUE(CurrentEntry, ACPI_OBJECT, SiblingListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (Object->Name == Name) {
            return Object;
        }
    }

    return NULL;
}

PACPI_OBJECT
AcpipCreateNamespaceObject (
    PAML_EXECUTION_CONTEXT Context,
    ACPI_OBJECT_TYPE Type,
    PSTR Name,
    PVOID Buffer,
    ULONG BufferSize
    )

/*++

Routine Description:

    This routine creates an ACPI namespace object.

Arguments:

    Context - Supplies a pointer to the ACPI execution context. If the name
        parameter is supplied, this parameter is required. Otherwise, it is
        optional.

    Type - Supplies the type of namespace object to create.

    Name - Supplies the name string of the object. Supply NULL to create a
        nameless object.

    Buffer - Supplies a pointer to a buffer that is used in different ways
        depending on the type being created.

    BufferSize - Supplies a buffer size that is used in different ways depending
        on the type of object being created. If NULL is passed in but a non-zero
        buffer size is supplied, a zero-filled buffer of the given size will be
        created. For string buffers, the size includes the null terminator.

Return Value:

    Returns a pointer to the ACPI object on success.

    NULL on failure.

--*/

{

    PACPI_OBJECT CurrentScope;
    PVOID NewBuffer;
    ULONG NewName;
    PACPI_OBJECT NewObject;
    ULONG PackageCount;
    ULONG PackageIndex;
    PACPI_OBJECT PackageObject;
    PACPI_OBJECT ParentObject;
    ULONG ParentPathOffset;
    PACPI_POWER_RESOURCE_OBJECT PowerResource;
    KSTATUS Status;
    PACPI_UNRESOLVED_NAME_OBJECT UnresolvedName;
    ULONG UnresolvedNameLength;

    CurrentScope = NULL;
    NewBuffer = NULL;
    NewName = 0;
    NewObject = NULL;
    ParentObject = NULL;
    if (Name != NULL) {
        if (Context != NULL) {
            CurrentScope = Context->CurrentScope;
        }

        if (CurrentScope == NULL) {
            CurrentScope = AcpiNamespaceRoot;
        }

        //
        // Separate out the name of the object from its path, and get the
        // parent object.
        //

        Status = AcpipPullOffLastName(Name, &NewName, &ParentPathOffset);
        if (!KSUCCESS(Status)) {
            goto CreateNamespaceObjectEnd;
        }

        if (ParentPathOffset == 0) {
            ParentObject = CurrentScope;

        } else {
            ParentObject = AcpipGetPartialNamespaceObject(Name,
                                                          ParentPathOffset,
                                                          CurrentScope);

            if (ParentObject == NULL) {
                Status = STATUS_PATH_NOT_FOUND;
                goto CreateNamespaceObjectEnd;
            }
        }
    }

    //
    // Allocate the new object.
    //

    NewObject = AcpipAllocateMemory(sizeof(ACPI_OBJECT));
    if (NewObject == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateNamespaceObjectEnd;
    }

    //
    // Initialize the object depending on the type.
    //

    NewObject->ReferenceCount = 1;
    INITIALIZE_LIST_HEAD(&(NewObject->ChildListHead));
    NewObject->Parent = ParentObject;
    NewObject->Type = Type;
    NewObject->Name = NewName;
    switch (NewObject->Type) {

    //
    // Create an integer object. Copy up to a 64-bit value if the buffer was
    // supplied.
    //

    case AcpiObjectInteger:
        if (Buffer != NULL) {
            if (BufferSize < sizeof(ULONGLONG)) {
                NewObject->U.Integer.Value = 0;
                RtlCopyMemory(&(NewObject->U.Integer.Value),
                              Buffer,
                              BufferSize);

            } else {
                NewObject->U.Integer.Value = *((PULONGLONG)Buffer);
            }
        }

        break;

    //
    // Create a string object. The buffer size determines the size of the string
    // buffer, including the null-terminating character. If the buffer itself
    // is non-null, it will be copied into the new object.
    //

    case AcpiObjectString:
        if (BufferSize == 0) {
            NewObject->U.String.String = NULL;

        } else {
            NewBuffer = AcpipAllocateMemory(BufferSize);
            if (NewBuffer == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto CreateNamespaceObjectEnd;
            }

            NewObject->U.String.String = NewBuffer;
            if (Buffer != NULL) {
                RtlCopyMemory(NewObject->U.String.String, Buffer, BufferSize);

            } else {
                RtlZeroMemory(NewBuffer, BufferSize);
            }
        }

        break;

    //
    // Create a buffer object. The buffer size is used to allocate the buffer,
    // and if the buffer itself is non-null, its contents are copied in.
    //

    case AcpiObjectBuffer:
        NewObject->U.Buffer.Buffer = NULL;
        NewObject->U.Buffer.Length = 0;
        if (BufferSize != 0) {
            NewBuffer = AcpipAllocateMemory(BufferSize);
            if (NewBuffer == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto CreateNamespaceObjectEnd;
            }

            NewObject->U.Buffer.Buffer = NewBuffer;
            NewObject->U.Buffer.Length = BufferSize;
            if (Buffer != NULL) {
                RtlCopyMemory(NewObject->U.Buffer.Buffer, Buffer, BufferSize);

            } else {
                RtlZeroMemory(NewBuffer, BufferSize);
            }
        }

        break;

    //
    // Create a package object. The buffer size divided by the size of a
    // pointer determines the count of the array, and the buffer contains the
    // initial list. Each element on the list will have its reference count
    // incremented.
    //

    case AcpiObjectPackage:
        NewObject->U.Package.Array = NULL;
        NewObject->U.Package.ElementCount = 0;
        if (BufferSize != 0) {
            NewBuffer = AcpipAllocateMemory(BufferSize);
            if (NewBuffer == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto CreateNamespaceObjectEnd;
            }

            NewObject->U.Package.Array = NewBuffer;
            NewObject->U.Package.ElementCount = BufferSize /
                                                sizeof(PACPI_OBJECT);

            if (Buffer != NULL) {
                RtlCopyMemory(NewBuffer, Buffer, BufferSize);
                PackageCount = NewObject->U.Package.ElementCount;
                for (PackageIndex = 0;
                     PackageIndex < PackageCount;
                     PackageIndex += 1) {

                    PackageObject = NewObject->U.Package.Array[PackageIndex];
                    AcpipObjectAddReference(PackageObject);
                }

            } else {
                RtlZeroMemory(NewBuffer, BufferSize);
            }
        }

        break;

    case AcpiObjectFieldUnit:
        if ((Buffer != NULL) &&
            (BufferSize == sizeof(ACPI_FIELD_UNIT_OBJECT))) {

            RtlCopyMemory(&(NewObject->U.FieldUnit),
                          Buffer,
                          sizeof(ACPI_FIELD_UNIT_OBJECT));

            //
            // Increment the reference count on the bank register if supplied.
            //

            if (NewObject->U.FieldUnit.BankRegister != NULL) {
                AcpipObjectAddReference(NewObject->U.FieldUnit.BankRegister);

                ASSERT(NewObject->U.FieldUnit.BankValue != NULL);

                AcpipObjectAddReference(NewObject->U.FieldUnit.BankValue);
            }

            //
            // Increment the reference count on the bank register if supplied.
            //

            if (NewObject->U.FieldUnit.IndexRegister != NULL) {
                AcpipObjectAddReference(NewObject->U.FieldUnit.IndexRegister);

                ASSERT(NewObject->U.FieldUnit.DataRegister != NULL);

                AcpipObjectAddReference(NewObject->U.FieldUnit.DataRegister);
            }

            if (NewObject->U.FieldUnit.OperationRegion != NULL) {
                AcpipObjectAddReference(NewObject->U.FieldUnit.OperationRegion);
            }
        }

        break;

    case AcpiObjectDevice:
        NewObject->U.Device.OsDevice = NULL;
        NewObject->U.Device.DeviceContext = NULL;
        NewObject->U.Device.IsPciBus = FALSE;
        NewObject->U.Device.IsDeviceStarted = FALSE;
        break;

    case AcpiObjectEvent:
        NewObject->U.Event.OsEvent = AcpipCreateEvent();
        if (NewObject->U.Event.OsEvent == NULL) {
            Status = STATUS_UNSUCCESSFUL;
            goto CreateNamespaceObjectEnd;
        }

        break;

    case AcpiObjectMethod:
        if ((Buffer != NULL) &&
            (BufferSize == sizeof(ACPI_METHOD_OBJECT))) {

            RtlCopyMemory(&(NewObject->U.Method),
                          Buffer,
                          sizeof(ACPI_METHOD_OBJECT));

            ASSERT(NewObject->U.Method.OsMutex == NULL);

            if (NewObject->U.Method.Serialized != FALSE) {
                NewObject->U.Method.OsMutex =
                               AcpipCreateMutex(NewObject->U.Method.SyncLevel);

                if (NewObject->U.Method.OsMutex == NULL) {
                    Status = STATUS_UNSUCCESSFUL;
                    goto CreateNamespaceObjectEnd;
                }
            }

        } else {
            RtlZeroMemory(&(NewObject->U.Method), sizeof(ACPI_METHOD_OBJECT));
        }

        break;

    case AcpiObjectMutex:
        NewObject->U.Mutex.OsMutex = AcpipCreateMutex(0);
        if (NewObject->U.Mutex.OsMutex == NULL) {
            Status = STATUS_UNSUCCESSFUL;
            goto CreateNamespaceObjectEnd;
        }

        break;

    case AcpiObjectOperationRegion:
        RtlZeroMemory(&(NewObject->U.OperationRegion),
                      sizeof(ACPI_OPERATION_REGION_OBJECT));

        break;

    case AcpiObjectPowerResource:
        if ((Buffer != NULL) &&
            (BufferSize == sizeof(ACPI_POWER_RESOURCE_OBJECT))) {

            PowerResource = (PACPI_POWER_RESOURCE_OBJECT)Buffer;
            RtlCopyMemory(&(NewObject->U.PowerResource),
                          PowerResource,
                          sizeof(ACPI_POWER_RESOURCE_OBJECT));
        }

        break;

    case AcpiObjectProcessor:
        if ((Buffer != NULL) &&
            (BufferSize == sizeof(ACPI_PROCESSOR_OBJECT))) {

            RtlCopyMemory(&(NewObject->U.Processor),
                          Buffer,
                          sizeof(ACPI_PROCESSOR_OBJECT));
        }

        break;

    case AcpiObjectBufferField:
        if ((Buffer != NULL) &&
            (BufferSize == sizeof(ACPI_BUFFER_FIELD_OBJECT))) {

            RtlCopyMemory(&(NewObject->U.BufferField),
                          Buffer,
                          sizeof(ACPI_BUFFER_FIELD_OBJECT));

            if (NewObject->U.BufferField.DestinationObject != NULL) {
                AcpipObjectAddReference(
                                   NewObject->U.BufferField.DestinationObject);
            }
        }

        break;

    case AcpiObjectAlias:
        if ((Buffer != NULL) &&
            (BufferSize == sizeof(ACPI_ALIAS_OBJECT))) {

            RtlCopyMemory(&(NewObject->U.Alias),
                          Buffer,
                          sizeof(ACPI_ALIAS_OBJECT));

            if (NewObject->U.Alias.DestinationObject != NULL) {
                AcpipObjectAddReference(NewObject->U.Alias.DestinationObject);
            }

        } else {
            RtlZeroMemory(&(NewObject->U.Alias), sizeof(ACPI_ALIAS_OBJECT));
        }

        break;

    case AcpiObjectUnresolvedName:

        ASSERT((Buffer != NULL) &&
               (BufferSize == sizeof(ACPI_UNRESOLVED_NAME_OBJECT)));

        UnresolvedName = (PACPI_UNRESOLVED_NAME_OBJECT)Buffer;
        UnresolvedNameLength = RtlStringLength(UnresolvedName->Name);
        NewObject->U.UnresolvedName.Name =
                                 AcpipAllocateMemory(UnresolvedNameLength + 1);

        if (NewObject->U.UnresolvedName.Name == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CreateNamespaceObjectEnd;
        }

        RtlStringCopy(NewObject->U.UnresolvedName.Name,
                      UnresolvedName->Name,
                      UnresolvedNameLength + 1);

        NewObject->U.UnresolvedName.Scope = UnresolvedName->Scope;
        AcpipObjectAddReference(UnresolvedName->Scope);
        break;

    //
    // Other objects need no additional data.
    //

    case AcpiObjectUninitialized:
    case AcpiObjectThermalZone:
    case AcpiObjectDebug:
        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_INVALID_PARAMETER;
        goto CreateNamespaceObjectEnd;
    }

    //
    // Link the object into the parent if one was supplied. Linking it into the
    // tree adds a reference count to the object, since when the method is
    // finished or the definition block is unloaded, all objects in the
    // namespace will be released.
    //

    NewObject->DestructorListEntry.Next = NULL;
    if (ParentObject != NULL) {
        NewObject->ReferenceCount += 1;
        INSERT_BEFORE(&(NewObject->SiblingListEntry),
                      &(ParentObject->ChildListHead));

        if (Context != NULL) {

            //
            // The object is being added to the global namespace, so destroy it
            // when the definition block is unloaded.
            //

            if (Context->DestructorListHead != NULL) {
                INSERT_BEFORE(&(NewObject->DestructorListEntry),
                              Context->DestructorListHead);

            //
            // A method is executing, so add it to the list of objects created
            // under the method.
            //

            } else {
                INSERT_BEFORE(
                            &(NewObject->DestructorListEntry),
                            &(Context->CurrentMethod->CreatedObjectsListHead));
            }
        }

    } else {
        NewObject->SiblingListEntry.Next = NULL;
    }

    Status = STATUS_SUCCESS;

CreateNamespaceObjectEnd:
    if (!KSUCCESS(Status)) {
        if (NewBuffer != NULL) {
            AcpipFreeMemory(NewBuffer);
        }

        if (NewObject != NULL) {
            AcpipFreeMemory(NewObject);
            NewObject = NULL;
        }
    }

    return NewObject;
}

VOID
AcpipObjectAddReference (
    PACPI_OBJECT Object
    )

/*++

Routine Description:

    This routine adds one to the reference count of a given ACPI object.

Arguments:

    Object - Supplies a pointer to the object.

Return Value:

    None.

--*/

{

    Object->ReferenceCount += 1;
    return;
}

VOID
AcpipObjectReleaseReference (
    PACPI_OBJECT Object
    )

/*++

Routine Description:

    This routine subtracts one from the reference count of the given object. If
    this causes the reference count to hit zero, the object will be destroyed.

Arguments:

    Object - Supplies a pointer to the object.

Return Value:

    None.

--*/

{

    ASSERT(Object->Type < AcpiObjectTypeCount);
    ASSERT((Object->ReferenceCount != 0) && (Object->ReferenceCount < 0x1000));

    Object->ReferenceCount -= 1;
    if (Object->ReferenceCount == 0) {
        AcpipDestroyNamespaceObject(Object);
    }

    return;
}

PACPI_OBJECT
AcpipGetNamespaceObject (
    PSTR Name,
    PACPI_OBJECT CurrentScope
    )

/*++

Routine Description:

    This routine looks up an ACPI object in the namespace based on a location
    string.

Arguments:

    Name - Supplies a pointer to a string containing the namespace path.

    CurrentScope - Supplies a pointer to the current namespace scope. If NULL
        is supplied, the global root namespace will be used.

Return Value:

    Returns a pointer to the ACPI object on success.

    NULL if the object could not be found.

--*/

{

    return AcpipGetPartialNamespaceObject(Name, 0, CurrentScope);
}

PACPI_OBJECT *
AcpipEnumerateChildObjects (
    PACPI_OBJECT ParentObject,
    ACPI_OBJECT_TYPE ObjectType,
    PULONG ObjectCount
    )

/*++

Routine Description:

    This routine allocates and initializes an array containing pointers to the
    children of the given namespace object, optionally filtering out only
    objects of a given type.

Arguments:

    ParentObject - Supplies a pointer to the parent whose children should be
        enumerated.

    ObjectType - Supplies an object type. If a valid object type is supplied,
        then only objects of that type will be returned. Supply
        AcpiObjectTypeCount to return all objects. Note that if
        AcpiObjectDevice is requested, then AcpiObjectProcessor objects will
        also be returned.

    ObjectCount - Supplies a pointer where the number of elements in the return
        array will be returned.

Return Value:

    Returns a pointer to an array of pointers to the child object. The caller
    must call the corresponding release enumeration list to free the memory
    allocated by this routine.

    NULL if there are no objects or there was an error.

--*/

{

    ULONG ChildCount;
    PLIST_ENTRY CurrentEntry;
    PACPI_OBJECT Object;
    ULONG ObjectIndex;
    PACPI_OBJECT *Objects;
    ULONG ProcessorObjectCount;
    PACPI_OBJECT *ProcessorObjects;

    Objects = NULL;
    ChildCount = 0;
    ProcessorObjects = NULL;

    //
    // If looking for devices in the system bus root, also find processors in
    // the _PR object and merge them in here.
    //

    if ((ObjectType == AcpiObjectDevice) &&
        (ParentObject == AcpiSystemBusRoot)) {

        ProcessorObjects = AcpipEnumerateChildObjects(AcpiProcessorRoot,
                                                      AcpiObjectDevice,
                                                      &ProcessorObjectCount);

        ChildCount += ProcessorObjectCount;
    }

    //
    // Loop through once to count the number of objects.
    //

    CurrentEntry = ParentObject->ChildListHead.Next;
    while (CurrentEntry != &(ParentObject->ChildListHead)) {
        Object = LIST_VALUE(CurrentEntry, ACPI_OBJECT, SiblingListEntry);
        if ((ObjectType == AcpiObjectTypeCount) ||
            (Object->Type == ObjectType) ||
            ((ObjectType == AcpiObjectDevice) &&
             (Object->Type == AcpiObjectProcessor))) {

            ChildCount += 1;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    if (ChildCount == 0) {
        goto EnumerateChildObjectsEnd;
    }

    Objects = AcpipAllocateMemory(ChildCount * sizeof(PACPI_OBJECT));
    if (Objects == NULL) {
        ChildCount = 0;
        goto EnumerateChildObjectsEnd;
    }

    //
    // Enumerate through and for each elibile child, put it in the array and
    // increment its reference count.
    //

    ObjectIndex = 0;
    CurrentEntry = ParentObject->ChildListHead.Next;
    while (CurrentEntry != &(ParentObject->ChildListHead)) {
        Object = LIST_VALUE(CurrentEntry, ACPI_OBJECT, SiblingListEntry);
        CurrentEntry = CurrentEntry->Next;
        if ((ObjectType == AcpiObjectTypeCount) ||
            (Object->Type == ObjectType) ||
            ((ObjectType == AcpiObjectDevice) &&
             (Object->Type == AcpiObjectProcessor))) {

            Objects[ObjectIndex] = Object;
            AcpipObjectAddReference(Object);
            ObjectIndex += 1;
        }
    }

    //
    // Copy in those processor objects from the beginning if there are any.
    //

    if (ProcessorObjects != NULL) {
        RtlCopyMemory(&(Objects[ObjectIndex]),
                      ProcessorObjects,
                      ProcessorObjectCount * sizeof(PACPI_OBJECT));

        MmFreePagedPool(ProcessorObjects);
        ProcessorObjects = NULL;
    }

EnumerateChildObjectsEnd:
    if (ProcessorObjects != NULL) {
        AcpipReleaseChildEnumerationArray(ProcessorObjects,
                                          ProcessorObjectCount);
    }

    *ObjectCount = ChildCount;
    return Objects;
}

VOID
AcpipReleaseChildEnumerationArray (
    PACPI_OBJECT *Objects,
    ULONG ObjectCount
    )

/*++

Routine Description:

    This routine releases a list returned as a result of calling the enumerate
    child objects routine.

Arguments:

    Objects - Supplies a pointer to the array of objects, as returned from
        the enumerate child objects routine.

    ObjectCount - Supplies the number of elements in the array, as returned
        from the enumerate child objects routine.

Return Value:

    None.

--*/

{

    ULONG ChildIndex;

    for (ChildIndex = 0; ChildIndex < ObjectCount; ChildIndex += 1) {
        AcpipObjectReleaseReference(Objects[ChildIndex]);
    }

    AcpipFreeMemory(Objects);
    return;
}

VOID
AcpipConvertEisaIdToString (
    ULONG EisaId,
    PSTR ResultIdString
    )

/*++

Routine Description:

    This routine converts an EISA encoded ID into a device ID string.

Arguments:

    EisaId - Supplies the encoded EISA ID to get.

    ResultIdString - Supplies a pointer where the decoded result string will
        be returned. This buffer must be allocated by the caller, and must be
        at least 8 bytes long.

Return Value:

    Returns a pointer to a string, allocated using the AML interpreter
    allocation routines. The caller is responsible for freeing this memory.

--*/

{

    UCHAR Manufacturer1;
    UCHAR Manufacturer2;
    UCHAR Manufacturer3;
    UCHAR ProductId1;
    UCHAR ProductId2;

    RtlZeroMemory(ResultIdString, EISA_ID_STRING_LENGTH);

    //
    // The EISA encoding is really goofy. It jams 3 characters of manufacturer
    // ID and 4 digits of product ID into 4 bytes. The manufacturer bits are
    // uppercase letters A - Z, where 0x40 is subtracted from each character
    // so it fits into 5 bits, then jammed into 3 bytes. The last two bytes
    // contain the product
    // code (byte 3 first, then byte 4). The encoding looks like this:
    //
    // Byte 0: 7 6 5 4 3 2 1 0
    //           1 1 1 1 1 2 2 - First character plus 2 MSB of second character.
    //
    // Byte 1: 7 6 5 4 3 2 1 0
    //         2 2 2 3 3 3 3 3 - 3 LSB of second character plus third character.
    //
    // Byte 2: Product ID byte 1.
    // Byte 3: Product ID byte 2.
    //
    // To decode the manufacturer ID, unstuff the 2 byte into 4, and add 0x40
    // to each one.
    //

    Manufacturer1 = (UCHAR)((EisaId >> 2) & 0x1F);

    //
    // Get the 3 LSB bits from byte 2, plus the two MSB from byte 1.
    //

    Manufacturer2 = (UCHAR)((EisaId >> (8 + 5)) & 0x7);
    Manufacturer2 |= (UCHAR)((EisaId << 3) & 0x18);

    //
    // Get character 3 from byte 2, and add 0x40 to every character.
    //

    Manufacturer3 = (UCHAR)((EisaId >> 8) & 0x1F);
    Manufacturer1 += 0x40;
    Manufacturer2 += 0x40;
    Manufacturer3 += 0x40;

    //
    // Get the product ID bytes.
    //

    ProductId1 = (UCHAR)(EisaId >> 16);
    ProductId2 = (UCHAR)(EisaId >> 24);

    //
    // Finally, construct the string.
    //

    RtlPrintToString(ResultIdString,
                     EISA_ID_STRING_LENGTH,
                     CharacterEncodingAscii,
                     "%c%c%c%02X%02X",
                     Manufacturer1,
                     Manufacturer2,
                     Manufacturer3,
                     ProductId1,
                     ProductId2);

    return;
}

KSTATUS
AcpipPerformStoreOperation (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT Source,
    PACPI_OBJECT Destination
    )

/*++

Routine Description:

    This routine performs a store operation from one object into the value of
    another.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Source - Supplies a pointer to the source object for the store.

    Destination - Supplies a pointer to the object to store the value into.

Return Value:

    Status code.

--*/

{

    BOOL NewObjectCreated;
    PACPI_OBJECT ResolvedDestination;
    ULONG Size;
    KSTATUS Status;

    NewObjectCreated = FALSE;

    //
    // Resolve to the correct destination.
    //

    ResolvedDestination = NULL;
    Status = AcpipResolveStoreDestination(Context,
                                          Destination,
                                          &ResolvedDestination);

    if (!KSUCCESS(Status)) {
        goto PerformStoreOperationEnd;
    }

    Destination = ResolvedDestination;

    //
    // The ACPI spec states that storing to constants is fatal, but also states
    // that it is a no-op and not an error. Go with the more lenient option. A
    // lot of operators use a store to Zero to indicate a no-op.
    //

    if (IS_ACPI_CONSTANT(Destination) != FALSE) {
        Status = STATUS_SUCCESS;
        goto PerformStoreOperationEnd;
    }

    //
    // Perform a conversion if necessary. Integers, Buffers, and Strings can
    // be stored into a Field/Buffer unit. Count strings as buffers.
    //

    if ((Destination->Type == AcpiObjectFieldUnit) ||
        (Destination->Type == AcpiObjectBufferField)) {

        if ((Source->Type != AcpiObjectInteger) &&
            (Source->Type != AcpiObjectBuffer)) {

            Source = AcpipConvertObjectType(Context, Source, AcpiObjectBuffer);
            if (Source == NULL) {
                Status = STATUS_CONVERSION_FAILED;
                goto PerformStoreOperationEnd;
            }

            NewObjectCreated = TRUE;
        }

    } else if ((Source->Type != Destination->Type) &&
               (Destination->Type != AcpiObjectDebug) &&
               (Destination->Type != AcpiObjectUninitialized)) {

        Source = AcpipConvertObjectType(Context, Source, Destination->Type);
        if (Source == NULL) {
            Status = STATUS_CONVERSION_FAILED;
            goto PerformStoreOperationEnd;
        }

        NewObjectCreated = TRUE;
    }

    //
    // Perform the store, which may involve freeing an old buffer and creating
    // a new one.
    //

    switch (Destination->Type) {
    case AcpiObjectUninitialized:

        //
        // If the object is uninitialized, then do a "replace contents"
        // operation.
        //

        Status = AcpipReplaceObjectContents(Context, Destination, Source);
        if (!KSUCCESS(Status)) {
            goto PerformStoreOperationEnd;
        }

        break;

    case AcpiObjectInteger:

        ASSERT(Source->Type == AcpiObjectInteger);

        Destination->U.Integer.Value = Source->U.Integer.Value;
        break;

    case AcpiObjectString:

        ASSERT(Source->Type == AcpiObjectString);

        if (Destination->U.String.String != NULL) {
            AcpipFreeMemory(Destination->U.String.String);
        }

        //
        // If a new object was created, steal that buffer, otherwise create and
        // copy a new buffer.
        //

        if (NewObjectCreated != FALSE) {
            Destination->U.String.String = Source->U.String.String;
            Source->U.String.String = NULL;

        } else {
            Size = RtlStringLength(Source->U.String.String);
            Destination->U.String.String = AcpipAllocateMemory(Size + 1);
            if (Destination->U.String.String == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto PerformStoreOperationEnd;
            }

            RtlCopyMemory(Destination->U.String.String,
                          Source->U.String.String,
                          Size + 1);
        }

        break;

    case AcpiObjectBuffer:

        ASSERT(Source->Type == AcpiObjectBuffer);

        //
        // If the old buffer is big enough, shrink it to the right size and
        // just reuse it.
        //

        if (Destination->U.Buffer.Length >= Source->U.Buffer.Length) {
            RtlCopyMemory(Destination->U.Buffer.Buffer,
                          Source->U.Buffer.Buffer,
                          Source->U.Buffer.Length);

        } else {

            //
            // If a new object was created, steal that buffer, otherwise create
            // and copy a new buffer.
            //

            if (NewObjectCreated != FALSE) {
                Destination->U.Buffer.Buffer = Source->U.Buffer.Buffer;
                Source->U.Buffer.Buffer = 0;
                Source->U.Buffer.Length = 0;

            } else {
                Size = Source->U.Buffer.Length;
                Destination->U.Buffer.Buffer = AcpipAllocateMemory(Size);
                if (Destination->U.Buffer.Buffer == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto PerformStoreOperationEnd;
                }

                RtlCopyMemory(Destination->U.Buffer.Buffer,
                              Source->U.Buffer.Buffer,
                              Size + 1);
            }

            Destination->U.Buffer.Length = Source->U.Buffer.Length;
        }

        break;

    case AcpiObjectFieldUnit:
        Status = AcpipWriteToField(Context, Destination, Source);
        if (!KSUCCESS(Status)) {
            goto PerformStoreOperationEnd;
        }

        break;

    case AcpiObjectBufferField:
        Status = AcpipWriteToBufferField(Context, Destination, Source);
        if (!KSUCCESS(Status)) {
            goto PerformStoreOperationEnd;
        }

        break;

    case AcpiObjectPackage:
        if (Source->Type != AcpiObjectPackage) {

            ASSERT(FALSE);

            Status = STATUS_NOT_SUPPORTED;
            goto PerformStoreOperationEnd;
        }

        Status = AcpipReplaceObjectContents(Context, Destination, Source);
        break;

    //
    // Some object cannot be "stored" into.
    //

    case AcpiObjectDevice:
    case AcpiObjectEvent:
    case AcpiObjectMethod:
    case AcpiObjectMutex:
    case AcpiObjectOperationRegion:
    case AcpiObjectPowerResource:
    case AcpiObjectProcessor:
    case AcpiObjectThermalZone:

        ASSERT(FALSE);

        Status = STATUS_NOT_SUPPORTED;
        goto PerformStoreOperationEnd;

    //
    // Stores to the debug object result in printing out the source.
    //

    case AcpiObjectDebug:
        AcpipDebugOutputObject(Source);
        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_NOT_SUPPORTED;
        goto PerformStoreOperationEnd;
    }

    Status = STATUS_SUCCESS;

PerformStoreOperationEnd:
    if (NewObjectCreated != FALSE) {
        AcpipObjectReleaseReference(Source);
    }

    if (ResolvedDestination != NULL) {
        AcpipObjectReleaseReference(ResolvedDestination);
    }

    return Status;
}

PACPI_OBJECT
AcpipCopyObject (
    PACPI_OBJECT Object
    )

/*++

Routine Description:

    This routine creates an unnamed and unlinked copy of the given object.

Arguments:

    Object - Supplies a pointer to the object whose contents should be copied.

Return Value:

    Returns a pointer to the new copy on success.

    NULL on failure.

--*/

{

    PVOID Buffer;
    ULONG BufferLength;
    PACPI_OBJECT NewObject;

    //
    // Determine which part to copy.
    //

    switch (Object->Type) {
    case AcpiObjectInteger:
        Buffer = &(Object->U.Integer.Value);
        BufferLength = sizeof(ULONGLONG);
        break;

    case AcpiObjectString:
        Buffer = Object->U.String.String;
        BufferLength = 0;
        if (Buffer != NULL) {
            BufferLength = RtlStringLength(Buffer) + 1;
        }

        break;

    case AcpiObjectBuffer:
        Buffer = Object->U.Buffer.Buffer;
        BufferLength = Object->U.Buffer.Length;
        break;

    case AcpiObjectPackage:
        Buffer = Object->U.Package.Array;
        BufferLength = Object->U.Package.ElementCount * sizeof(PACPI_OBJECT);
        break;

    case AcpiObjectFieldUnit:
        Buffer = &(Object->U.FieldUnit);
        BufferLength = sizeof(ACPI_FIELD_UNIT_OBJECT);
        break;

    case AcpiObjectPowerResource:
        Buffer = &(Object->U.PowerResource);
        BufferLength = sizeof(ACPI_POWER_RESOURCE_OBJECT);
        break;

    case AcpiObjectProcessor:
        Buffer = &(Object->U.Processor);
        BufferLength = sizeof(ACPI_PROCESSOR_OBJECT);
        break;

    case AcpiObjectBufferField:
        Buffer = &(Object->U.BufferField);
        BufferLength = sizeof(ACPI_BUFFER_FIELD_OBJECT);
        break;

    case AcpiObjectUninitialized:
    case AcpiObjectThermalZone:
    case AcpiObjectDebug:
        Buffer = NULL;
        BufferLength = 0;
        break;

    case AcpiObjectAlias:
        Buffer = &(Object->U.Alias);
        BufferLength = sizeof(ACPI_ALIAS_OBJECT);
        break;

    case AcpiObjectDevice:
    case AcpiObjectEvent:
    case AcpiObjectMethod:
    case AcpiObjectMutex:
    case AcpiObjectOperationRegion:
    default:

        ASSERT(FALSE);

        return NULL;
    }

    NewObject = AcpipCreateNamespaceObject(NULL,
                                           Object->Type,
                                           NULL,
                                           Buffer,
                                           BufferLength);

    return NewObject;
}

KSTATUS
AcpipReplaceObjectContents (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT ObjectToReplace,
    PACPI_OBJECT ObjectWithContents
    )

/*++

Routine Description:

    This routine replaces the inner contents of an object with a copy of those
    from a different object.

Arguments:

    Context - Supplies a pointer to the AML execution context.

    ObjectToReplace - Supplies a pointer to an object whose contents should be
        replaced.

    ObjectWithContents - Supplies a pointer to the object that has the contents
        to use for replacement.

Return Value:

    STATUS_SUCCESS if the object to replace successfully became a copy of the
    object with contents (on everything except its name, position in the
    namespace, and actual pointer).

    STATUS_INSUFFICIENT_RESOURCES if the space needed to replace the contents
    could not be allocated. In this case, the object to be replaced will
    remain unchanged.

    Other error codes on other failures. On failure, the object to replace will
    remain unchanged.

--*/

{

    PACPI_OBJECT FieldReadResult;
    PVOID NewBuffer;
    ULONG NewBufferLength;
    ULONG PackageIndex;
    PACPI_OBJECT PackageObject;
    KSTATUS Status;

    FieldReadResult = NULL;
    NewBuffer = NULL;
    NewBufferLength = 0;

    //
    // Determine if a new buffer needs to be allocated, and its size.
    //

    switch (ObjectWithContents->Type) {
    case AcpiObjectString:
        NewBufferLength = RtlStringLength(ObjectWithContents->U.String.String) +
                          1;

        break;

    case AcpiObjectBuffer:
        NewBufferLength = ObjectWithContents->U.Buffer.Length;
        break;

    case AcpiObjectPackage:
        NewBufferLength = ObjectWithContents->U.Package.ElementCount *
                          sizeof(PACPI_OBJECT);

        break;

    default:
        break;
    }

    //
    // Attempt to allocate the new buffer if needed.
    //

    if (NewBufferLength != 0) {
        NewBuffer = AcpipAllocateMemory(NewBufferLength);
        if (NewBuffer == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    //
    // Now that all required resources are acquired, free the old stuff.
    //

    switch (ObjectToReplace->Type) {
    case AcpiObjectString:
        if (ObjectToReplace->U.String.String != NULL) {
            AcpipFreeMemory(ObjectToReplace->U.String.String);
        }

        break;

    case AcpiObjectBuffer:
        if (ObjectToReplace->U.Buffer.Buffer != NULL) {
            AcpipFreeMemory(ObjectToReplace->U.Buffer.Buffer);
        }

        break;

    case AcpiObjectPackage:
        if (ObjectToReplace->U.Package.Array != NULL) {
            for (PackageIndex = 0;
                 PackageIndex < ObjectToReplace->U.Package.ElementCount;
                 PackageIndex += 1) {

                PackageObject = ObjectToReplace->U.Package.Array[PackageIndex];
                if (PackageObject != NULL) {
                    AcpipObjectReleaseReference(PackageObject);
                }
            }

            AcpipFreeMemory(ObjectToReplace->U.Package.Array);
        }

        break;

    case AcpiObjectFieldUnit:
        if (ObjectToReplace->U.FieldUnit.BankRegister != NULL) {
            AcpipObjectReleaseReference(
                                    ObjectToReplace->U.FieldUnit.BankRegister);

            ASSERT(ObjectToReplace->U.FieldUnit.BankValue != NULL);

            AcpipObjectReleaseReference(ObjectToReplace->U.FieldUnit.BankValue);
        }

        if (ObjectToReplace->U.FieldUnit.IndexRegister != NULL) {
            AcpipObjectReleaseReference(
                                   ObjectToReplace->U.FieldUnit.IndexRegister);

            ASSERT(ObjectToReplace->U.FieldUnit.DataRegister != NULL);

            AcpipObjectReleaseReference(
                                    ObjectToReplace->U.FieldUnit.DataRegister);
        }

        break;

    case AcpiObjectEvent:
        if (ObjectToReplace->U.Event.OsEvent != NULL) {
            AcpipDestroyEvent(ObjectToReplace->U.Event.OsEvent);
            ObjectToReplace->U.Event.OsEvent = NULL;
        }

        break;

    case AcpiObjectMethod:
        if (ObjectToReplace->U.Method.OsMutex != NULL) {
            AcpipDestroyMutex(ObjectToReplace->U.Method.OsMutex);
            ObjectToReplace->U.Method.OsMutex = NULL;
        }

        break;

    case AcpiObjectMutex:
        if (ObjectToReplace->U.Mutex.OsMutex != NULL) {
            AcpipDestroyMutex(ObjectToReplace->U.Mutex.OsMutex);
            ObjectToReplace->U.Mutex.OsMutex = NULL;
        }

        break;

    case AcpiObjectBufferField:
        if (ObjectToReplace->U.BufferField.DestinationObject != NULL) {
            AcpipObjectReleaseReference(
                             ObjectToReplace->U.BufferField.DestinationObject);
        }

        break;

    case AcpiObjectAlias:
        if (ObjectToReplace->U.Alias.DestinationObject != NULL) {
            AcpipObjectReleaseReference(
                                   ObjectToReplace->U.Alias.DestinationObject);
        }

        break;

    default:
        break;
    }

    //
    // Replace with the new stuff.
    //

    Status = STATUS_SUCCESS;
    ObjectToReplace->Type = ObjectWithContents->Type;
    switch (ObjectWithContents->Type) {
    case AcpiObjectInteger:
        ObjectToReplace->U.Integer.Value = ObjectWithContents->U.Integer.Value;
        break;

    case AcpiObjectString:
        RtlCopyMemory(NewBuffer,
                      ObjectWithContents->U.String.String,
                      NewBufferLength);

        ObjectToReplace->U.String.String = NewBuffer;
        break;

    case AcpiObjectBuffer:
        RtlCopyMemory(NewBuffer,
                      ObjectWithContents->U.Buffer.Buffer,
                      NewBufferLength);

        ObjectToReplace->U.Buffer.Buffer = NewBuffer;
        ObjectToReplace->U.Buffer.Length = NewBufferLength;
        break;

    case AcpiObjectFieldUnit:
        Status = AcpipReadFromField(Context,
                                    ObjectWithContents,
                                    &FieldReadResult);

        if (!KSUCCESS(Status)) {
            return Status;
        }

        //
        // Call this routine again, replacing the object with the result of the
        // read instead of the field itself. Set the type to be uninitialized
        // so this routine doesn't try to re-free anything.
        //

        ObjectToReplace->Type = AcpiObjectUninitialized;
        Status = AcpipReplaceObjectContents(Context,
                                            ObjectToReplace,
                                            FieldReadResult);

        AcpipObjectReleaseReference(FieldReadResult);
        break;

    case AcpiObjectPackage:
        RtlCopyMemory(NewBuffer,
                      ObjectWithContents->U.Package.Array,
                      NewBufferLength);

        ObjectToReplace->U.Package.Array = NewBuffer;
        ObjectToReplace->U.Package.ElementCount = NewBufferLength /
                                                  sizeof(PACPI_OBJECT);

        //
        // Increment the reference count on every object in the package.
        //

        for (PackageIndex = 0;
             PackageIndex < ObjectToReplace->U.Package.ElementCount;
             PackageIndex += 1) {

            PackageObject = ObjectToReplace->U.Package.Array[PackageIndex];
            if (PackageObject != NULL) {
                AcpipObjectAddReference(PackageObject);
            }
        }

        break;

    case AcpiObjectPowerResource:
        RtlCopyMemory(&(ObjectToReplace->U.PowerResource),
                      &(ObjectWithContents->U.PowerResource),
                      sizeof(ACPI_POWER_RESOURCE_OBJECT));

        break;

    case AcpiObjectProcessor:
        RtlCopyMemory(&(ObjectToReplace->U.Processor),
                      &(ObjectWithContents->U.Processor),
                      sizeof(ACPI_PROCESSOR_OBJECT));

        break;

    case AcpiObjectBufferField:
        Status = AcpipReadFromBufferField(Context,
                                          ObjectWithContents,
                                          &FieldReadResult);

        if (!KSUCCESS(Status)) {
            return Status;
        }

        //
        // Call this routine again, replacing the object with the result of the
        // read instead of the field itself. Set the type to be uninitialized
        // so this routine doesn't try to re-free anything.
        //

        ObjectToReplace->Type = AcpiObjectUninitialized;
        Status = AcpipReplaceObjectContents(Context,
                                            ObjectToReplace,
                                            FieldReadResult);

        AcpipObjectReleaseReference(FieldReadResult);
        break;

    case AcpiObjectThermalZone:
    case AcpiObjectDebug:
        break;

    case AcpiObjectAlias:
        RtlCopyMemory(&(ObjectToReplace->U.Alias),
                      &(ObjectWithContents->U.Alias),
                      sizeof(ACPI_ALIAS_OBJECT));

        if (ObjectToReplace->U.Alias.DestinationObject != NULL) {
            AcpipObjectAddReference(
                               ObjectToReplace->U.Alias.DestinationObject);
        }

        break;

    case AcpiObjectDevice:
    case AcpiObjectEvent:
    case AcpiObjectMethod:
    case AcpiObjectMutex:
    case AcpiObjectOperationRegion:
    default:

        ASSERT(FALSE);

        Status = STATUS_NOT_SUPPORTED;
        break;
    }

    return Status;
}

PACPI_OBJECT
AcpipGetPackageObject (
    PACPI_OBJECT Package,
    ULONG Index,
    BOOL ConvertConstants
    )

/*++

Routine Description:

    This routine returns the object at a given index in a package.

Arguments:

    Package - Supplies a pointer to the package to read from.

    Index - Supplies the index of the element to get.

    ConvertConstants - Supplies a boolean indicating whether or not constant
        integers should be converted to non-constant integers before being
        returned.

Return Value:

    Returns a pointer to the element in the package at the given index.

    NULL on error, either if too large of an index was specified, or there is
    no value at that index.

--*/

{

    PACPI_OBJECT *Array;
    PVOID Buffer;
    PACPI_OBJECT NewObject;
    PACPI_OBJECT ResolvedName;

    ASSERT(Package->Type == AcpiObjectPackage);

    Array = (PACPI_OBJECT *)Package->U.Package.Array;
    if ((Array == NULL) || (Index >= Package->U.Package.ElementCount)) {
        return NULL;
    }

    if (Array[Index] == NULL) {
        Array[Index] = AcpipCreateNamespaceObject(NULL,
                                                  AcpiObjectUninitialized,
                                                  NULL,
                                                  NULL,
                                                  0);

    //
    // If the object is an unresolved name, attempt to resolve that name now.
    //

    } else if (Array[Index]->Type == AcpiObjectUnresolvedName) {
        ResolvedName = AcpipGetNamespaceObject(
                                         Array[Index]->U.UnresolvedName.Name,
                                         Array[Index]->U.UnresolvedName.Scope);

        //
        // The name should really resolve. If it doesn't, this is a serious
        // BIOS error.
        //

        ASSERT(ResolvedName != NULL);

        //
        // If the name resolves, replaced the unresolved reference with a
        // resolved reference.
        //

        if (ResolvedName != NULL) {
            AcpipSetPackageObject(Package, Index, ResolvedName);
        }

        return ResolvedName;

    //
    // If constant conversion is requested, convert Zero, One, and Ones into
    // private integers and set it in the package.
    //

    } else if ((ConvertConstants != FALSE) &&
               (Array[Index]->Type == AcpiObjectInteger)) {

        if (IS_ACPI_CONSTANT(Array[Index]) != FALSE) {
            Buffer = &(Array[Index]->U.Integer.Value),
            NewObject = AcpipCreateNamespaceObject(NULL,
                                                   AcpiObjectInteger,
                                                   NULL,
                                                   Buffer,
                                                   sizeof(ULONGLONG));

            if (NewObject == NULL) {
                return NULL;
            }

            AcpipSetPackageObject(Package, Index, NewObject);
            AcpipObjectReleaseReference(NewObject);
        }
    }

    return Array[Index];
}

VOID
AcpipSetPackageObject (
    PACPI_OBJECT Package,
    ULONG Index,
    PACPI_OBJECT Object
    )

/*++

Routine Description:

    This routine sets the object in a package at a given index.

Arguments:

    Package - Supplies a pointer to the package to modify.

    Index - Supplies the index of the element to set.

    Object - Supplies the object to set at Package[Index]. This can be NULL.

Return Value:

    None.

--*/

{

    PACPI_OBJECT *Array;

    ASSERT(Package->Type == AcpiObjectPackage);

    Array = (PACPI_OBJECT *)Package->U.Package.Array;
    if ((Array == NULL) || (Index >= Package->U.Package.ElementCount)) {
        return;
    }

    //
    // Decrement the reference count on the object that was there before.
    //

    if (Array[Index] != NULL) {
        AcpipObjectReleaseReference(Array[Index]);
    }

    //
    // Increment the reference count on the new object.
    //

    if (Object != NULL) {
        AcpipObjectAddReference(Object);
    }

    Array[Index] = Object;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
AcpipDestroyNamespaceObject (
    PACPI_OBJECT Object
    )

/*++

Routine Description:

    This routine destroys an ACPI namespace object (and all of its child
    object).

Arguments:

    Object - Supplies a pointer to the object to destroy. It is expected that
        this object is already properly unlinked from any namespace.

Return Value:

    None.

--*/

{

    PLIST_ENTRY ChildEntry;
    PLIST_ENTRY DestructorEntry;
    LIST_ENTRY DestructorStackHead;
    ULONG PackageIndex;
    PACPI_OBJECT PackageObject;

    //
    // If the object's sibling list entry is not null, then unlink it from the
    // parent.
    //

    if (Object->SiblingListEntry.Next != NULL) {
        LIST_REMOVE(&(Object->SiblingListEntry));
    }

    if (Object->DestructorListEntry.Next != NULL) {
        LIST_REMOVE(&(Object->DestructorListEntry));
    }

    //
    // Start by pushing the object on top of the stack.
    //

    INITIALIZE_LIST_HEAD(&DestructorStackHead);
    INSERT_AFTER(&(Object->DestructorListEntry), &DestructorStackHead);
    while (LIST_EMPTY(&DestructorStackHead) == FALSE) {

        //
        // Take a look at the value on top of the stack. If it has any children,
        // remove the child from the child list, push it onto the destructor
        // stack, and start over.
        //

        DestructorEntry = DestructorStackHead.Next;
        Object = LIST_VALUE(DestructorEntry, ACPI_OBJECT, DestructorListEntry);
        if (LIST_EMPTY(&(Object->ChildListHead)) == FALSE) {
            ChildEntry = Object->ChildListHead.Next;
            LIST_REMOVE(ChildEntry);
            Object = LIST_VALUE(ChildEntry, ACPI_OBJECT, SiblingListEntry);

            ASSERT(Object->DestructorListEntry.Next != NULL);

            LIST_REMOVE(&(Object->DestructorListEntry));
            INSERT_AFTER(&(Object->DestructorListEntry), &DestructorStackHead);
            continue;
        }

        //
        // The child list is empty, this is a leaf node. Pull it off the
        // destructor stack and destroy it.
        //

        LIST_REMOVE(DestructorEntry);
        switch (Object->Type) {
        case AcpiObjectString:
            if (Object->U.String.String != NULL) {
                AcpipFreeMemory(Object->U.String.String);
            }

            break;

        case AcpiObjectBuffer:
            if (Object->U.Buffer.Buffer != NULL) {
                AcpipFreeMemory(Object->U.Buffer.Buffer);
            }

            break;

        case AcpiObjectPackage:
            if (Object->U.Package.Array != NULL) {
                for (PackageIndex = 0;
                     PackageIndex < Object->U.Package.ElementCount;
                     PackageIndex += 1) {

                    PackageObject = Object->U.Package.Array[PackageIndex];
                    if (PackageObject != NULL) {
                        AcpipObjectReleaseReference(PackageObject);
                    }
                }
            }

            break;

        case AcpiObjectFieldUnit:
            if (Object->U.FieldUnit.BankRegister != NULL) {
                AcpipObjectReleaseReference(Object->U.FieldUnit.BankRegister);

                ASSERT(Object->U.FieldUnit.BankValue != NULL);

                AcpipObjectReleaseReference(Object->U.FieldUnit.BankValue);
            }

            if (Object->U.FieldUnit.IndexRegister != NULL) {
                AcpipObjectReleaseReference(Object->U.FieldUnit.IndexRegister);

                ASSERT(Object->U.FieldUnit.DataRegister != NULL);

                AcpipObjectReleaseReference(Object->U.FieldUnit.DataRegister);
            }

            if (Object->U.FieldUnit.OperationRegion != NULL) {
                AcpipObjectReleaseReference(
                                          Object->U.FieldUnit.OperationRegion);
            }

            break;

        case AcpiObjectEvent:
            if (Object->U.Event.OsEvent != NULL) {
                AcpipDestroyEvent(Object->U.Event.OsEvent);
            }

            break;

        case AcpiObjectMethod:
            if (Object->U.Method.OsMutex != NULL) {
                AcpipDestroyMutex(Object->U.Method.OsMutex);
                Object->U.Method.OsMutex = NULL;
            }

            break;

        case AcpiObjectMutex:
            if (Object->U.Mutex.OsMutex != NULL) {
                AcpipDestroyMutex(Object->U.Mutex.OsMutex);
            }

            break;

        case AcpiObjectOperationRegion:
            AcpipDestroyOperationRegion(Object);
            break;

        case AcpiObjectBufferField:
            if (Object->U.BufferField.DestinationObject != NULL) {
                AcpipObjectReleaseReference(
                                      Object->U.BufferField.DestinationObject);
            }

            break;

        case AcpiObjectInteger:

            ASSERT(IS_ACPI_CONSTANT(Object) == FALSE);

            break;

        case AcpiObjectUninitialized:
        case AcpiObjectDevice:
        case AcpiObjectPowerResource:
        case AcpiObjectProcessor:
        case AcpiObjectThermalZone:
        case AcpiObjectDebug:
            break;

        case AcpiObjectAlias:
            if (Object->U.Alias.DestinationObject != NULL) {
                AcpipObjectReleaseReference(Object->U.Alias.DestinationObject);
            }

            break;

        case AcpiObjectUnresolvedName:
            AcpipFreeMemory(Object->U.UnresolvedName.Name);
            AcpipObjectReleaseReference(Object->U.UnresolvedName.Scope);
            break;

        default:

            ASSERT(FALSE);

            break;
        }

        Object->Type = AcpiObjectUninitialized;
        AcpipFreeMemory(Object);
    }

    return;
}

PACPI_OBJECT
AcpipGetPartialNamespaceObject (
    PSTR Name,
    ULONG Length,
    PACPI_OBJECT CurrentScope
    )

/*++

Routine Description:

    This routine looks up an ACPI object in the namespace based on a location
    string.

Arguments:

    Name - Supplies a pointer to a string containing the namespace path.

    Length - Supplies the maximum number of bytes of the string to parse.
        Supply 0 to parse the entire string.

    CurrentScope - Supplies a pointer to the current namespace scope. If NULL
        is supplied, the global root namespace will be used.

Return Value:

    Returns a pointer to the ACPI object on success.

    NULL if the object could not be found.

--*/

{

    PACPI_OBJECT Child;
    PLIST_ENTRY CurrentEntry;
    ULONG DesiredName;
    BOOL SearchUp;

    SearchUp = TRUE;

    //
    // Zero means parse the whole string, so just set the length to a really
    // big value.
    //

    if (Length == 0) {
        Length = 0xFFFFFFFF;
    }

    if (CurrentScope == NULL) {
        CurrentScope = AcpiNamespaceRoot;
    }

    if (Name[0] == ACPI_NAMESPACE_ROOT_CHARACTER) {
        SearchUp = FALSE;
        CurrentScope = AcpiNamespaceRoot;
        Name += 1;
        Length -= 1;

    } else {
        while ((Name[0] == ACPI_NAMESPACE_PARENT_CHARACTER) && (Length != 0)) {
            SearchUp = FALSE;
            CurrentScope = CurrentScope->Parent;
            if (CurrentScope->Parent == NULL) {
                return NULL;
            }

            Name += 1;
            Length -= 1;
        }
    }

    //
    // Loop traversing into names until there are no more.
    //

    while ((Name[0] != '\0') && (Length != 0)) {
        if ((Name[1] == '\0') || (Name[2] == '\0') || (Name[3] == '\0') ||
            (Length < 4)) {

            ASSERT(FALSE);

            return NULL;
        }

        DesiredName = READ_UNALIGNED32(Name);

        //
        // Loop through all children of the current scope looking for the
        // desired child.
        //

        CurrentEntry = CurrentScope->ChildListHead.Next;
        Child = NULL;
        while (CurrentEntry != &(CurrentScope->ChildListHead)) {
            Child = LIST_VALUE(CurrentEntry, ACPI_OBJECT, SiblingListEntry);

            //
            // Stop if the name was found. Also, since a name was found, don't
            // search up the tree anymore.
            //

            if (Child->Name == DesiredName) {
                SearchUp = FALSE;
                break;
            }

            CurrentEntry = CurrentEntry->Next;
        }

        //
        // If the entry wasn't found, relative pathnames are in use, and no part
        // of the name has been found so far, go up the tree towards the root
        // as defined by the ACPI namespace search rules for relative names.
        //

        if (CurrentEntry == &(CurrentScope->ChildListHead)) {
            if ((SearchUp == FALSE) || (CurrentScope == AcpiNamespaceRoot)) {
                return NULL;
            }

            CurrentScope = CurrentScope->Parent;

            ASSERT(CurrentScope != NULL);

            continue;
        }

        CurrentScope = Child;
        Name += ACPI_MAX_NAME_LENGTH;
        Length -= ACPI_MAX_NAME_LENGTH;
    }

    return CurrentScope;
}

KSTATUS
AcpipPullOffLastName (
    PSTR Name,
    PULONG LastName,
    PULONG LastNameOffset
    )

/*++

Routine Description:

    This routine pulls the innermost name off of the given name string. It also
    validates that the last part is actually a name.

Arguments:

    Name - Supplies a pointer to a string containing the namespace path.

    LastName - Supplies a pointer where the last name in the path will be
        returned.

    LastNameOffset - Supplies a pointer where the offset, in bytes, of the last
        name in the string will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER on failure.

--*/

{

    UCHAR Character;
    ULONG Length;
    ULONG NameIndex;

    Length = RtlStringLength(Name);
    if (Length < ACPI_MAX_NAME_LENGTH) {
        return STATUS_INVALID_PARAMETER;
    }

    for (NameIndex = 0; NameIndex < ACPI_MAX_NAME_LENGTH; NameIndex += 1) {
        Character = Name[Length - 1 - NameIndex];
        if ((Character == ACPI_NAMESPACE_ROOT_CHARACTER) ||
            (Character == ACPI_NAMESPACE_PARENT_CHARACTER)) {

            return STATUS_INVALID_PARAMETER;
        }
    }

    RtlCopyMemory(LastName,
                  &(Name[Length - ACPI_MAX_NAME_LENGTH]),
                  ACPI_MAX_NAME_LENGTH);

    *LastNameOffset = Length - ACPI_MAX_NAME_LENGTH;
    return STATUS_SUCCESS;
}

VOID
AcpipDebugOutputObject (
    PACPI_OBJECT Object
    )

/*++

Routine Description:

    This routine prints an ACPI object to the debugger.

Arguments:

    Object - Supplies a pointer to the object to print.

Return Value:

    None.

--*/

{

    PUCHAR Buffer;
    ULONG BufferLength;
    ULONG ByteIndex;
    PSTR Name;
    ULONG PackageIndex;
    PACPI_OBJECT PackageObject;

    Name = (PSTR)&(Object->Name);
    RtlDebugPrint("AML: ");
    switch (Object->Type) {
    case AcpiObjectInteger:
        RtlDebugPrint("%I64x", Object->U.Integer.Value);
        break;

    case AcpiObjectString:
        RtlDebugPrint("\"%s\"", Object->U.String.String);
        break;

    case AcpiObjectBuffer:
        Buffer = Object->U.Buffer.Buffer;
        BufferLength = Object->U.Buffer.Length;
        RtlDebugPrint("{");
        if ((Buffer != NULL) && (BufferLength != 0)) {
            for (ByteIndex = 0; ByteIndex < BufferLength - 1; ByteIndex += 1) {
                RtlDebugPrint("%02x ", Buffer[ByteIndex]);
            }

            RtlDebugPrint("%02x}", Buffer[BufferLength - 1]);
        }

        break;

    case AcpiObjectPackage:
        RtlDebugPrint("Package (%d) {", Object->U.Package.ElementCount);
        if (Object->U.Package.Array != NULL) {
            for (PackageIndex = 0;
                 PackageIndex < Object->U.Package.ElementCount;
                 PackageIndex += 1) {

                PackageObject = Object->U.Package.Array[PackageIndex];
                if (PackageObject != NULL) {
                    AcpipDebugOutputObject(PackageObject);
                }
            }
        }

        RtlDebugPrint("}");
        break;

    case AcpiObjectFieldUnit:
        AcpipPrintFieldUnit(Object);
        break;

    case AcpiObjectDevice:
        RtlDebugPrint("Device (%c%c%c%c)", Name[0], Name[1], Name[2], Name[3]);
        break;

    case AcpiObjectEvent:
        RtlDebugPrint("Event (%c%c%c%c)",
                      Name[0],
                      Name[1],
                      Name[2],
                      Name[3]);

        break;

    case AcpiObjectMethod:
        RtlDebugPrint("Method (%c%c%c%c)",
                      Name[0],
                      Name[1],
                      Name[2],
                      Name[3]);

        break;

    case AcpiObjectMutex:
        RtlDebugPrint("Mutex (%c%c%c%c)",
                      Name[0],
                      Name[1],
                      Name[2],
                      Name[3]);

        break;

    case AcpiObjectOperationRegion:
        AcpipPrintOperationRegion(Object);
        break;

    case AcpiObjectPowerResource:
        RtlDebugPrint("PowerResource (%c%c%c%c, %d, %d)",
                      Name[0],
                      Name[1],
                      Name[2],
                      Name[3],
                      Object->U.PowerResource.SystemLevel,
                      Object->U.PowerResource.ResourceOrder);

        break;

    case AcpiObjectProcessor:
        RtlDebugPrint("Processor (%c%c%c%c, %d, %d, %d)",
                      Name[0],
                      Name[1],
                      Name[2],
                      Name[3],
                      Object->U.Processor.ProcessorId,
                      Object->U.Processor.ProcessorBlockAddress,
                      Object->U.Processor.ProcessorBlockLength);

        break;

    case AcpiObjectThermalZone:
        RtlDebugPrint("ThermalZone (%c%c%c%c)",
                      Name[0],
                      Name[1],
                      Name[2],
                      Name[3]);

        break;

    case AcpiObjectBufferField:
        AcpipPrintBufferField(Object);
        break;

    case AcpiObjectDebug:
        RtlDebugPrint("Debug object itself!");
        break;

    case AcpiObjectAlias:
        RtlDebugPrint("Alias (%c%c%c%c) to (",
                      Name[0],
                      Name[1],
                      Name[2],
                      Name[3]);

        AcpipDebugOutputObject(Object->U.Alias.DestinationObject);
        RtlDebugPrint(")");
        break;

    default:

        ASSERT(FALSE);

        RtlDebugPrint("Unknown object of type %d\n", Object->Type);
    }

    RtlDebugPrint("\n");
    return;
}

