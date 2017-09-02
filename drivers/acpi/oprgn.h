/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    oprgn.h

Abstract:

    This header contains definitions for ACPI Operation Region access.

Author:

    Evan Green 17-Nov-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

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
AcpipCreateOperationRegion (
    PAML_EXECUTION_CONTEXT Context,
    PSTR Name,
    ACPI_OPERATION_REGION_SPACE Space,
    ULONGLONG Offset,
    ULONGLONG Length
    );

/*++

Routine Description:

    This routine creates an ACPI Operation Region object.

Arguments:

    Context - Supplies a pointer to the AML execution context.

    Name - Supplies a pointer to the name of the Operation Region object.

    Space - Supplies the address space type of the region.

    Offset - Supplies the byte offset into the address space of the beginning
        of the Operation Region.

    Length - Supplies the byte length of the Operation Region.

Return Value:

    Status code.

--*/

VOID
AcpipDestroyOperationRegion (
    PACPI_OBJECT Object
    );

/*++

Routine Description:

    This routine destroys an ACPI Operation Region object. This routine should
    not be called directly, but will be called from the namespace object
    destruction routine.

Arguments:

    Object - Supplies a pointer to the operation region to destroy.

Return Value:

    None.

--*/

KSTATUS
AcpipReadFromField (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT FieldObject,
    PACPI_OBJECT *ResultObject
    );

/*++

Routine Description:

    This routine reads from an Operation Region field.

Arguments:

    Context - Supplies a pointer to the AML execution context.

    FieldObject - Supplies a pointer to the field object to read from.

    ResultObject - Supplies a pointer where a pointer to the result object will
        be returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_SUCCESS on success.

    Error codes on failure.

--*/

KSTATUS
AcpipWriteToField (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT FieldObject,
    PACPI_OBJECT ValueToWrite
    );

/*++

Routine Description:

    This routine writes to an Operation Region field.

Arguments:

    Context - Supplies a pointer to the AML execution context.

    FieldObject - Supplies a pointer to the field object to write to.

    ValueToWrite - Supplies a pointer to an Integer or Buffer object containing
        the value to write into the field.

Return Value:

    STATUS_SUCCESS on success.

    Error codes on failure.

--*/

KSTATUS
AcpipReadFromBufferField (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT BufferField,
    PACPI_OBJECT *ResultObject
    );

/*++

Routine Description:

    This routine reads from a Buffer Field.

Arguments:

    Context - Supplies a pointer to the AML execution context.

    BufferField - Supplies a pointer to the field object to read from.

    ResultObject - Supplies a pointer where a pointer to the result object will
        be returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_SUCCESS on success.

    Error codes on failure.

--*/

KSTATUS
AcpipWriteToBufferField (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT BufferField,
    PACPI_OBJECT ValueToWrite
    );

/*++

Routine Description:

    This routine writes to a Buffer Field.

Arguments:

    Context - Supplies a pointer to the AML execution context.

    BufferField - Supplies a pointer to the field object to read from.

    ValueToWrite - Supplies a pointer to the value to write to the buffer field.

Return Value:

    STATUS_SUCCESS on success.

    Error codes on failure.

--*/

VOID
AcpipPrintOperationRegion (
    PACPI_OBJECT OperationRegion
    );

/*++

Routine Description:

    This routine prints a description of the given Operation Region to the
    debugger.

Arguments:

    OperationRegion - Supplies a pointer to the Operation Region to print.

Return Value:

    None.

--*/

VOID
AcpipPrintFieldUnit (
    PACPI_OBJECT FieldUnit
    );

/*++

Routine Description:

    This routine prints a description of the given Field Unit to the
    debugger.

Arguments:

    FieldUnit - Supplies a pointer to the Field Unit to print.

Return Value:

    None.

--*/

VOID
AcpipPrintBufferField (
    PACPI_OBJECT BufferField
    );

/*++

Routine Description:

    This routine prints a description of the given Buffer Field to the
    debugger.

Arguments:

    BufferField - Supplies a pointer to the Buffer Field to print.

Return Value:

    None.

--*/

