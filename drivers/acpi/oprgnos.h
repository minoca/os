/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    oprgnos.h

Abstract:

    This header contains definitions for operating system support of ACPI
    Operation Regions.

Author:

    Evan Green 17-Nov-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define ACPI_OS_ALLOCATION_TAG 0x4F6C6D41 // 'OlmA'

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _ACPI_OPERATION_REGION_SPACE {
    OperationRegionSystemMemory       = 0,
    OperationRegionSystemIo           = 1,
    OperationRegionPciConfig          = 2,
    OperationRegionEmbeddedController = 3,
    OperationRegionSmBus              = 4,
    OperationRegionCmos               = 5,
    OperationRegionPciBarTarget       = 6,
    OperationRegionIpmi               = 7,
    OperationRegionCount
} ACPI_OPERATION_REGION_SPACE, *PACPI_OPERATION_REGION_SPACE;

typedef
KSTATUS
(*PACPI_OPERATION_REGION_CREATE) (
    PVOID AcpiObject,
    ULONGLONG Offset,
    ULONGLONG Length,
    PVOID *OsContext
    );

/*++

Routine Description:

    This routine creates an ACPI Operation Region of a known type. This region
    will be used by AML code to access system hardware.

Arguments:

    AcpiObject - Supplies a pointer to the ACPI object that represents the new
        operation region.

    Offset - Supplies an offset in the given address space to start the
        Operation Region.

    Length - Supplies the length, in bytes, of the Operation Region.

    OsContext - Supplies a pointer where an opaque pointer will be returned by
        the OS support routines on success. This pointer will be passed to the
        OS Operation Region access and destruction routines, and can store
        any OS-specific context related to the Operation Region.

Return Value:

    STATUS_SUCCESS on success.

    Other error codes on failure.

--*/

typedef
VOID
(*PACPI_OPERATION_REGION_DESTROY) (
    PVOID OsContext
    );

/*++

Routine Description:

    This routine tears down OS support for an ACPI Operation Region.

Arguments:

    OsContext - Supplies the context pointer supplied by the OS when the
        Operation Region was created.

Return Value:

    None. No further accesses to the given operation region will be made.

--*/

typedef
KSTATUS
(*PACPI_OPERATION_REGION_READ) (
    PVOID OsContext,
    ULONGLONG Offset,
    ULONG Size,
    PVOID Value
    );

/*++

Routine Description:

    This routine performs a read from an Operation Region.

Arguments:

    OsContext - Supplies the context pointer supplied by the OS when the
        Operation Region was created.

    Offset - Supplies the byte offset within the operation region to read from.

    Size - Supplies the size of the read to perform. Valid values are 8, 16,
        32, and 64. Other values are considered invalid.

    Value - Supplies a pointer where the value from the read will be returned
        on success.

Return Value:

    STATUS_SUCCESS on success.

    Other error codes on failure.

--*/

typedef
KSTATUS
(*PACPI_OPERATION_REGION_WRITE) (
    PVOID OsContext,
    ULONGLONG Offset,
    ULONG Size,
    PVOID Value
    );

/*++

Routine Description:

    This routine performs a write to an Operation Region.

Arguments:

    OsContext - Supplies the context pointer supplied by the OS when the
        Operation Region was created.

    Offset - Supplies the byte offset within the operation region to write to.

    Size - Supplies the size of the write to perform. Valid values are 8, 16,
        32, and 64. Other values are considered invalid.

    Value - Supplies a pointer to a buffer containing the value to write.

Return Value:

    STATUS_SUCCESS on success.

    Other error codes on failure.

--*/

/*++

Structure Description:

    This structure describes the function table for ACPI Operation Region
    support of one address space type.

Members:

    Create - Supplies a pointer to a function that creates new Operation
        Regions.

    Destroy - Supplies a pointer to a function that destroys an Operation
        Region.

    Read - Supplies a pointer to a function that performs 8, 16, 32, and 64 bit
        reads from an Operation Region.

    Write - Supplies a pointer to a function that performs 8, 16, 32, and 64 bit
        writes to an Operation Region.

--*/

typedef struct _ACPI_OPERATION_REGION_FUNCTION_TABLE {
    PACPI_OPERATION_REGION_CREATE Create;
    PACPI_OPERATION_REGION_DESTROY Destroy;
    PACPI_OPERATION_REGION_READ Read;
    PACPI_OPERATION_REGION_WRITE Write;
} ACPI_OPERATION_REGION_FUNCTION_TABLE, *PACPI_OPERATION_REGION_FUNCTION_TABLE;

//
// -------------------------------------------------------------------- Globals
//

//
// Define the global operation region access array. This table must be defined
// by the OS support portion.
//

extern PACPI_OPERATION_REGION_FUNCTION_TABLE
                         AcpiOperationRegionFunctionTable[OperationRegionCount];

//
// -------------------------------------------------------- Function Prototypes
//
