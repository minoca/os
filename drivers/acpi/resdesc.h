/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    resdesc.h

Abstract:

    This header contains definitions for handling and converting resource
    descriptors in ACPI.

Author:

    Evan Green 4-Aug-2015

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
AcpipParseGenericAddress (
    PACPI_OBJECT ResourceBuffer,
    PGENERIC_ADDRESS GenericAddress
    );

/*++

Routine Description:

    This routine reads a single generic address from the given resource buffer.

Arguments:

    ResourceBuffer - Supplies a pointer to the ACPI resource buffer to parse.

    GenericAddress - Supplies a pointer where the extracted generic address
        will be returned.

Return Value:

     Status code.

--*/

KSTATUS
AcpipConvertFromAcpiResourceBuffer (
    PACPI_OBJECT Device,
    PACPI_OBJECT ResourceBuffer,
    PRESOURCE_CONFIGURATION_LIST *ConfigurationListResult
    );

/*++

Routine Description:

    This routine converts an ACPI resource buffer into an OS configuration list.

Arguments:

    Device - Supplies a pointer to the namespace object of the device this
        buffer is coming from. This is used for relative namespace traversal
        for certain types of resource descriptors (like GPIO).

    ResourceBuffer - Supplies a pointer to the ACPI resource list buffer to
        parse.

    ConfigurationListResult - Supplies a pointer where a newly allocated
        resource configuration list will be returned. It is the callers
        responsibility to manage this memory once it is returned.

Return Value:

     Status code.

--*/

KSTATUS
AcpipConvertFromRequirementListToAllocationList (
    PRESOURCE_CONFIGURATION_LIST ConfigurationList,
    PRESOURCE_ALLOCATION_LIST *AllocationList
    );

/*++

Routine Description:

    This routine converts a resource requirement list into a resource allocation
    list. For every requirement, it will create an allocation from the
    requirement's minimum and length.

Arguments:

    ConfigurationList - Supplies a pointer to the resource configuration list to
        convert. This routine assumes there is only one configuration on the
        list.

    AllocationList - Supplies a pointer where a pointer to a new resource
        allocation list will be returned on success. The caller is responsible
        for freeing this memory once it is returned.

Return Value:

    Status code.

--*/

KSTATUS
AcpipConvertToAcpiResourceBuffer (
    PRESOURCE_ALLOCATION_LIST AllocationList,
    PACPI_OBJECT ResourceBuffer
    );

/*++

Routine Description:

    This routine converts an ACPI resource buffer into an OS configuration list.

Arguments:

    AllocationList - Supplies a pointer to a resource allocation list to convert
        to a resource buffer.

    ResourceBuffer - Supplies a pointer to a resource buffer to tweak to fit
        the allocation list. The resource buffer comes from executing the _CRS
        method.

Return Value:

    Status code.

--*/

