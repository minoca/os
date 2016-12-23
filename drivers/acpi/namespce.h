/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    namespce.h

Abstract:

    This header contains definitions for the ACPI namespace.

Author:

    Evan Green 13-Nov-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define special character to the namespace.
//

#define ACPI_NAMESPACE_ROOT_CHARACTER '\\'
#define ACPI_NAMESPACE_PARENT_CHARACTER '^'

//
// Define the maximum length of an ACPI name.
//

#define ACPI_MAX_NAME_LENGTH 4

//
// Define the length of an EISA ID string after being decoded, including the
// NULL terminator.
//

#define EISA_ID_STRING_LENGTH 8

//
// Define the device ID that processor objects are converted to.
//

#define ACPI_PROCESSOR_DEVICE_ID "ACPI0007"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
AcpipInitializeNamespace (
    VOID
    );

/*++

Routine Description:

    This routine initializes the ACPI global namespace.

Arguments:

    None.

Return Value:

    Status code.

--*/

PACPI_OBJECT
AcpipGetNamespaceRoot (
    VOID
    );

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

PACPI_OBJECT
AcpipGetSystemBusRoot (
    VOID
    );

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

PACPI_OBJECT
AcpipGetProcessorRoot (
    VOID
    );

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

PACPI_OBJECT
AcpipFindNamedObject (
    PACPI_OBJECT ParentObject,
    ULONG Name
    );

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

PACPI_OBJECT
AcpipCreateNamespaceObject (
    PAML_EXECUTION_CONTEXT Context,
    ACPI_OBJECT_TYPE Type,
    PSTR Name,
    PVOID Buffer,
    ULONG BufferSize
    );

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

VOID
AcpipObjectAddReference (
    PACPI_OBJECT Object
    );

/*++

Routine Description:

    This routine adds one to the reference count of a given ACPI object.

Arguments:

    Object - Supplies a pointer to the object.

Return Value:

    None.

--*/

VOID
AcpipObjectReleaseReference (
    PACPI_OBJECT Object
    );

/*++

Routine Description:

    This routine subtracts one from the reference count of the given object. If
    this causes the reference count to hit zero, the object will be destroyed.

Arguments:

    Object - Supplies a pointer to the object.

Return Value:

    None.

--*/

PACPI_OBJECT
AcpipGetNamespaceObject (
    PSTR Name,
    PACPI_OBJECT CurrentScope
    );

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

PACPI_OBJECT *
AcpipEnumerateChildObjects (
    PACPI_OBJECT ParentObject,
    ACPI_OBJECT_TYPE ObjectType,
    PULONG ObjectCount
    );

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
        AcpiObjectTypeCount to return all objects.

    ObjectCount - Supplies a pointer where the number of elements in the return
        array will be returned.

Return Value:

    Returns a pointer to an array of pointers to the child object. The caller
    must call the corresponding release enumeration list to free the memory
    allocated by this routine.

    NULL if there are no objects or there was an error.

--*/

VOID
AcpipReleaseChildEnumerationArray (
    PACPI_OBJECT *Objects,
    ULONG ObjectCount
    );

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

VOID
AcpipConvertEisaIdToString (
    ULONG EisaId,
    PSTR ResultIdString
    );

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

KSTATUS
AcpipPerformStoreOperation (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT Source,
    PACPI_OBJECT Destination
    );

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

PACPI_OBJECT
AcpipCopyObject (
    PACPI_OBJECT Object
    );

/*++

Routine Description:

    This routine creates an unnamed and unlinked copy of the given object.

Arguments:

    Object - Supplies a pointer to the object whose contents should be copied.

Return Value:

    Returns a pointer to the new copy on success.

    NULL on failure.

--*/

KSTATUS
AcpipReplaceObjectContents (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT ObjectToReplace,
    PACPI_OBJECT ObjectWithContents
    );

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

PACPI_OBJECT
AcpipGetPackageObject (
    PACPI_OBJECT Package,
    ULONG Index,
    BOOL ConvertConstants
    );

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

VOID
AcpipSetPackageObject (
    PACPI_OBJECT Package,
    ULONG Index,
    PACPI_OBJECT Object
    );

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

