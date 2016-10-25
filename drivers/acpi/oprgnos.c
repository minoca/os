/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    oprgnos.c

Abstract:

    This module implements operating system specific support for ACPI Operation
    Regions.

Author:

    Evan Green 17-Nov-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/intrface/pci.h>
#include "acpip.h"
#include "namespce.h"
#include "amlos.h"
#include "earlypci.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes an Memory Operation Region.

Members:

    PhysicalAddress - Stores the physical address of the Operation Region.

    Length - Stores the length of the Operation Region, in bytes.

    VirtualAddress - Stores the virtual address of the Operation Region.

    Offset - Stores the offset (in bytes) to add before accessing the Operation
        Region.

--*/

typedef struct _MEMORY_OPERATION_REGION {
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONGLONG Length;
    PVOID VirtualAddress;
    ULONG Offset;
} MEMORY_OPERATION_REGION, *PMEMORY_OPERATION_REGION;

/*++

Structure Description:

    This structure describes an I/O Port Operation Region.

Members:

    Offset - Stores the first I/O port address in this operation region.

    Length - Stores the length, in bytes, of the Operation Region.

--*/

typedef struct _IO_PORT_OPERATION_REGION {
    USHORT Offset;
    USHORT Length;
} IO_PORT_OPERATION_REGION, *PIO_PORT_OPERATION_REGION;

/*++

Structure Description:

    This structure describes a PCI Configuration space Operation Region.

Members:

    U - Stores a union of the two different interface access methods. Only
        one of these methods is used at a time, depending on whether the
        device that owns the operation region is started when the operation
        region is first accessed.

        Access - Stores the PCI configuration access interface.

        SpecificAccess - Stores the specific PCI configuration access interface.

    UsingSpecificAccess - Stores a boolean indicating whether specific access
        is in use or not.

    BusNumber - Stores the bus number of the device that owns the Operation
        Region. This is only used with specific access.

    DeviceNumber - Stores the device number of the device that owns the
        Operation Region. This is only used with specific access.

    FunctionNumber - Stores the function number of the device that owns the
        Operation Region. This is only used with specific access.

    Offset - Stores the offset from the beginning of PCI Config space for this
        device to the beginning of the Operation Region.

    Length - Stores the length, in bytes, of the Operation Region.

    AcpiObject - Stores a pointer to the ACPI object that represents the
        Operation Region.

    Configured - Stores a boolean indicating whether or not the region is
        configured and ready for access.

--*/

typedef struct _PCI_CONFIG_OPERATION_REGION {
    union {
        INTERFACE_PCI_CONFIG_ACCESS Access;
        INTERFACE_SPECIFIC_PCI_CONFIG_ACCESS SpecificAccess;
    } U;

    BOOL UsingSpecificAccess;
    ULONG BusNumber;
    ULONG DeviceNumber;
    ULONG FunctionNumber;
    ULONG Offset;
    ULONG Length;
    PACPI_OBJECT AcpiObject;
    BOOL Configured;
} PCI_CONFIG_OPERATION_REGION, *PPCI_CONFIG_OPERATION_REGION;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
AcpipCreateUnsupportedOperationRegion (
    PVOID AcpiObject,
    ULONGLONG Offset,
    ULONGLONG Length,
    PVOID *OsContext
    );

VOID
AcpipDestroyUnsupportedOperationRegion (
    PVOID OsContext
    );

KSTATUS
AcpipReadUnsupportedOperationRegion (
    PVOID OsContext,
    ULONGLONG Offset,
    ULONG Size,
    PVOID Value
    );

KSTATUS
AcpipWriteUnsupportedOperationRegion (
    PVOID OsContext,
    ULONGLONG Offset,
    ULONG Size,
    PVOID Value
    );

KSTATUS
AcpipCreateMemoryOperationRegion (
    PVOID AcpiObject,
    ULONGLONG Offset,
    ULONGLONG Length,
    PVOID *OsContext
    );

VOID
AcpipDestroyMemoryOperationRegion (
    PVOID OsContext
    );

KSTATUS
AcpipReadMemoryOperationRegion (
    PVOID OsContext,
    ULONGLONG Offset,
    ULONG Size,
    PVOID Value
    );

KSTATUS
AcpipWriteMemoryOperationRegion (
    PVOID OsContext,
    ULONGLONG Offset,
    ULONG Size,
    PVOID Value
    );

KSTATUS
AcpipCreateIoPortOperationRegion (
    PVOID AcpiObject,
    ULONGLONG Offset,
    ULONGLONG Length,
    PVOID *OsContext
    );

VOID
AcpipDestroyIoPortOperationRegion (
    PVOID OsContext
    );

KSTATUS
AcpipReadIoPortOperationRegion (
    PVOID OsContext,
    ULONGLONG Offset,
    ULONG Size,
    PVOID Value
    );

KSTATUS
AcpipWriteIoPortOperationRegion (
    PVOID OsContext,
    ULONGLONG Offset,
    ULONG Size,
    PVOID Value
    );

KSTATUS
AcpipCreatePciConfigOperationRegion (
    PVOID AcpiObject,
    ULONGLONG Offset,
    ULONGLONG Length,
    PVOID *OsContext
    );

VOID
AcpipDestroyPciConfigOperationRegion (
    PVOID OsContext
    );

KSTATUS
AcpipReadPciConfigOperationRegion (
    PVOID OsContext,
    ULONGLONG Offset,
    ULONG Size,
    PVOID Value
    );

KSTATUS
AcpipWritePciConfigOperationRegion (
    PVOID OsContext,
    ULONGLONG Offset,
    ULONG Size,
    PVOID Value
    );

KSTATUS
AcpipConfigurePciConfigOperationRegion (
    PPCI_CONFIG_OPERATION_REGION OperationRegion
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define an operation region function table for system memory.
//

ACPI_OPERATION_REGION_FUNCTION_TABLE AcpiMemoryOperationRegionTable = {
    AcpipCreateMemoryOperationRegion,
    AcpipDestroyMemoryOperationRegion,
    AcpipReadMemoryOperationRegion,
    AcpipWriteMemoryOperationRegion
};

//
// Define an operation region function table for system IO.
//

ACPI_OPERATION_REGION_FUNCTION_TABLE AcpiIoOperationRegionTable = {
    AcpipCreateIoPortOperationRegion,
    AcpipDestroyIoPortOperationRegion,
    AcpipReadIoPortOperationRegion,
    AcpipWriteIoPortOperationRegion
};

//
// Define an operation region function table for PCI Configuration Space.
//

ACPI_OPERATION_REGION_FUNCTION_TABLE AcpiPciConfigOperationRegionTable = {
    AcpipCreatePciConfigOperationRegion,
    AcpipDestroyPciConfigOperationRegion,
    AcpipReadPciConfigOperationRegion,
    AcpipWritePciConfigOperationRegion
};

//
// Define an operation region function table for the ACPI Embedded Controller.
//

ACPI_OPERATION_REGION_FUNCTION_TABLE AcpiEmbeddedControlOperationRegionTable = {
    AcpipCreateUnsupportedOperationRegion,
    AcpipDestroyUnsupportedOperationRegion,
    AcpipReadUnsupportedOperationRegion,
    AcpipWriteUnsupportedOperationRegion
};

//
// Define an operation region function table for SMBus.
//

ACPI_OPERATION_REGION_FUNCTION_TABLE AcpiSmBusOperationRegionTable = {
    AcpipCreateUnsupportedOperationRegion,
    AcpipDestroyUnsupportedOperationRegion,
    AcpipReadUnsupportedOperationRegion,
    AcpipWriteUnsupportedOperationRegion
};

//
// Define an operation region function table for CMOS.
//

ACPI_OPERATION_REGION_FUNCTION_TABLE AcpiCmosOperationRegionTable = {
    AcpipCreateUnsupportedOperationRegion,
    AcpipDestroyUnsupportedOperationRegion,
    AcpipReadUnsupportedOperationRegion,
    AcpipWriteUnsupportedOperationRegion
};

//
// Define an operation region function table for PCI Base Address Register
// targets.
//

ACPI_OPERATION_REGION_FUNCTION_TABLE AcpiPciBarTargetOperationRegionTable = {
    AcpipCreateUnsupportedOperationRegion,
    AcpipDestroyUnsupportedOperationRegion,
    AcpipReadUnsupportedOperationRegion,
    AcpipWriteUnsupportedOperationRegion
};

//
// Define an operation region function table for IPMI.
//

ACPI_OPERATION_REGION_FUNCTION_TABLE AcpiIpmiOperationRegionTable = {
    AcpipCreateUnsupportedOperationRegion,
    AcpipDestroyUnsupportedOperationRegion,
    AcpipReadUnsupportedOperationRegion,
    AcpipWriteUnsupportedOperationRegion
};

//
// Define the global operation region access array.
//

PACPI_OPERATION_REGION_FUNCTION_TABLE
                     AcpiOperationRegionFunctionTable[OperationRegionCount] = {

    &AcpiMemoryOperationRegionTable,
    &AcpiIoOperationRegionTable,
    &AcpiPciConfigOperationRegionTable,
    &AcpiEmbeddedControlOperationRegionTable,
    &AcpiSmBusOperationRegionTable,
    &AcpiCmosOperationRegionTable,
    &AcpiPciBarTargetOperationRegionTable,
    &AcpiIpmiOperationRegionTable
};

//
// Store the interface UUID of PCI config space accesses.
//

UUID AcpiPciConfigUuid = UUID_PCI_CONFIG_ACCESS;

//
// Store the interface UUID of specific PCI config space accesses.
//

UUID AcpiSpecificPciConfigUuid = UUID_PCI_CONFIG_ACCESS_SPECIFIC;

//
// ------------------------------------------------------------------ Functions
//

//
// --------------------------------------------------------- Internal Functions
//

//
// Dummy functions for unsupported Operation Region types.
//

KSTATUS
AcpipCreateUnsupportedOperationRegion (
    PVOID AcpiObject,
    ULONGLONG Offset,
    ULONGLONG Length,
    PVOID *OsContext
    )

/*++

Routine Description:

    This routine implements a dummy routine for creating an Operation Region
    of an unsupported type.

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

{

    //
    // Allow creation so the loading of definition blocks doesn't barf, but
    // freak out if these regions are ever accessed.
    //

    *OsContext = NULL;
    return STATUS_SUCCESS;
}

VOID
AcpipDestroyUnsupportedOperationRegion (
    PVOID OsContext
    )

/*++

Routine Description:

    This routine tears down OS support for an ACPI Operation Region of an
    unsupported type.

Arguments:

    OsContext - Supplies the context pointer supplied by the OS when the
        Operation Region was created.

Return Value:

    None. No further accesses to the given operation region will be made.

--*/

{

    ASSERT(OsContext == NULL);

    return;
}

KSTATUS
AcpipReadUnsupportedOperationRegion (
    PVOID OsContext,
    ULONGLONG Offset,
    ULONG Size,
    PVOID Value
    )

/*++

Routine Description:

    This routine performs a read from an unsupported Operation Region. This
    code should never execute.

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

{

    ASSERT(FALSE);

    return STATUS_NOT_SUPPORTED;
}

KSTATUS
AcpipWriteUnsupportedOperationRegion (
    PVOID OsContext,
    ULONGLONG Offset,
    ULONG Size,
    PVOID Value
    )

/*++

Routine Description:

    This routine performs a write to an unsupported Operation Region. This code
    should never execute.

Arguments:

    OsContext - Supplies the context pointer supplied by the OS when the
        Operation Region was created.

    Offset - Supplies the byte offset within the operation region to write to.

    Size - Supplies the size of the write to perform. Valid values are 8, 16,
        32, and 64. Other values are considered invalid.

    Value - Supplies the value to write.

Return Value:

    STATUS_SUCCESS on success.

    Other error codes on failure.

--*/

{

    ASSERT(FALSE);

    return STATUS_NOT_SUPPORTED;
}

//
// Memory space Operation Region handlers.
//

KSTATUS
AcpipCreateMemoryOperationRegion (
    PVOID AcpiObject,
    ULONGLONG Offset,
    ULONGLONG Length,
    PVOID *OsContext
    )

/*++

Routine Description:

    This routine creates an ACPI Operation Region to physical address space.

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

{

    PMEMORY_OPERATION_REGION OperationRegion;
    ULONG PageSize;
    KSTATUS Status;

    PageSize = MmPageSize();

    //
    // Allocate space for the operation region.
    //

    OperationRegion = MmAllocatePagedPool(sizeof(MEMORY_OPERATION_REGION),
                                          ACPI_OS_ALLOCATION_TAG);

    if (OperationRegion == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateMemoryOperationRegionEnd;
    }

    RtlZeroMemory(OperationRegion, sizeof(MEMORY_OPERATION_REGION));
    OperationRegion->Offset = Offset - ALIGN_RANGE_DOWN(Offset, PageSize);
    OperationRegion->PhysicalAddress = Offset;
    OperationRegion->Length = Length;

    //
    // Map the address as uncached memory.
    //

    Offset -= OperationRegion->Offset;
    Length += OperationRegion->Offset;
    OperationRegion->VirtualAddress = MmMapPhysicalAddress(Offset,
                                                           Length,
                                                           TRUE,
                                                           FALSE,
                                                           TRUE);

    if (OperationRegion->VirtualAddress == NULL) {
        RtlDebugPrint("ACPI: Failed to create Memory OpRegion at %I64x, "
                      "Size %I64x.\n",
                      Offset,
                      Length);

        ASSERT(FALSE);

        Status = STATUS_UNSUCCESSFUL;
        goto CreateMemoryOperationRegionEnd;
    }

    Status = STATUS_SUCCESS;

CreateMemoryOperationRegionEnd:
    if (!KSUCCESS(Status)) {
        if (OperationRegion != NULL) {
            MmFreePagedPool(OperationRegion);
            OperationRegion = NULL;
        }
    }

    *OsContext = OperationRegion;
    return STATUS_SUCCESS;
}

VOID
AcpipDestroyMemoryOperationRegion (
    PVOID OsContext
    )

/*++

Routine Description:

    This routine tears down OS support for an Memory Operation Region.

Arguments:

    OsContext - Supplies the context pointer supplied by the OS when the
        Operation Region was created.

Return Value:

    None. No further accesses to the given operation region will be made.

--*/

{

    PMEMORY_OPERATION_REGION OperationRegion;

    ASSERT(OsContext != NULL);

    OperationRegion = (PMEMORY_OPERATION_REGION)OsContext;

    ASSERT(OperationRegion->VirtualAddress != NULL);

    MmUnmapAddress(OperationRegion->VirtualAddress,
                   OperationRegion->Length + OperationRegion->Offset);

    OperationRegion->VirtualAddress = NULL;
    MmFreePagedPool(OperationRegion);
    return;
}

KSTATUS
AcpipReadMemoryOperationRegion (
    PVOID OsContext,
    ULONGLONG Offset,
    ULONG Size,
    PVOID Value
    )

/*++

Routine Description:

    This routine performs a read from a Memory Operation Region.

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

{

    PVOID DataPointer;
    PMEMORY_OPERATION_REGION OperationRegion;
    KSTATUS Status;

    ASSERT(OsContext != NULL);

    OperationRegion = (PMEMORY_OPERATION_REGION)OsContext;

    //
    // Check the range.
    //

    if ((Offset >= OperationRegion->Length) ||
        (Offset + (Size / BITS_PER_BYTE) > OperationRegion->Length) ||
        (Offset >= Offset + (Size / BITS_PER_BYTE))) {

        Status = STATUS_OUT_OF_BOUNDS;
        goto ReadMemoryOperationRegionEnd;
    }

    //
    // Perform the read.
    //

    DataPointer = OperationRegion->VirtualAddress + OperationRegion->Offset +
                  Offset;

    switch (Size) {
    case 8:
        *((PUCHAR)Value) = *((PUCHAR)DataPointer);
        break;

    case 16:
        *((PUSHORT)Value) = *((PUSHORT)DataPointer);
        break;

    case 32:
        *((PULONG)Value) = *((PULONG)DataPointer);
        break;

    case 64:
        *((PULONGLONG)Value) = *((PULONGLONG)DataPointer);
        break;

    //
    // Allow arbitrary reads on a memory op-region to accomodate the Load
    // instruction.
    //

    default:

        ASSERT(IS_ALIGNED(Size, BITS_PER_BYTE));

        RtlCopyMemory(Value, DataPointer, Size / BITS_PER_BYTE);
        break;
    }

    Status = STATUS_SUCCESS;

ReadMemoryOperationRegionEnd:
    return Status;
}

KSTATUS
AcpipWriteMemoryOperationRegion (
    PVOID OsContext,
    ULONGLONG Offset,
    ULONG Size,
    PVOID Value
    )

/*++

Routine Description:

    This routine performs a write to a Memory Operation Region.

Arguments:

    OsContext - Supplies the context pointer supplied by the OS when the
        Operation Region was created.

    Offset - Supplies the byte offset within the operation region to write to.

    Size - Supplies the size of the write to perform. Valid values are 8, 16,
        32, and 64. Other values are considered invalid.

    Value - Supplies the value to write.

Return Value:

    STATUS_SUCCESS on success.

    Other error codes on failure.

--*/

{

    PVOID DataPointer;
    PMEMORY_OPERATION_REGION OperationRegion;
    KSTATUS Status;

    ASSERT(OsContext != NULL);

    OperationRegion = (PMEMORY_OPERATION_REGION)OsContext;

    //
    // Check the range.
    //

    if ((Offset >= OperationRegion->Length) ||
        (Offset + (Size / BITS_PER_BYTE) > OperationRegion->Length) ||
        (Offset >= Offset + (Size / BITS_PER_BYTE))) {

        Status = STATUS_OUT_OF_BOUNDS;
        goto WriteMemoryOperationRegionEnd;
    }

    //
    // Perform the write.
    //

    DataPointer = OperationRegion->VirtualAddress + OperationRegion->Offset +
                  Offset;

    switch (Size) {
    case 8:
        *((PUCHAR)DataPointer) = *((PUCHAR)Value);
        break;

    case 16:
        *((PUSHORT)DataPointer) = *((PUSHORT)Value);
        break;

    case 32:
        *((PULONG)DataPointer) = *((PULONG)Value);
        break;

    case 64:
        *((PULONGLONG)DataPointer) = *((PULONGLONG)Value);
        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        goto WriteMemoryOperationRegionEnd;
    }

    Status = STATUS_SUCCESS;

WriteMemoryOperationRegionEnd:
    return Status;
}

//
// I/O space Operation Region handlers.
//

KSTATUS
AcpipCreateIoPortOperationRegion (
    PVOID AcpiObject,
    ULONGLONG Offset,
    ULONGLONG Length,
    PVOID *OsContext
    )

/*++

Routine Description:

    This routine creates an ACPI Operation Region to system I/O ports.

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

{

    PIO_PORT_OPERATION_REGION OperationRegion;
    KSTATUS Status;

    //
    // Allocate space for the operation region.
    //

    OperationRegion = MmAllocatePagedPool(sizeof(IO_PORT_OPERATION_REGION),
                                          ACPI_OS_ALLOCATION_TAG);

    if (OperationRegion == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateIoPortOperationRegionEnd;
    }

    RtlZeroMemory(OperationRegion, sizeof(IO_PORT_OPERATION_REGION));
    OperationRegion->Offset = (USHORT)Offset;
    OperationRegion->Length = (USHORT)Length;
    Status = STATUS_SUCCESS;

CreateIoPortOperationRegionEnd:
    if (!KSUCCESS(Status)) {
        if (OperationRegion != NULL) {
            MmFreePagedPool(OperationRegion);
            OperationRegion = NULL;
        }
    }

    *OsContext = OperationRegion;
    return STATUS_SUCCESS;
}

VOID
AcpipDestroyIoPortOperationRegion (
    PVOID OsContext
    )

/*++

Routine Description:

    This routine tears down OS support for an I/O Port Operation Region.

Arguments:

    OsContext - Supplies the context pointer supplied by the OS when the
        Operation Region was created.

Return Value:

    None. No further accesses to the given operation region will be made.

--*/

{

    PIO_PORT_OPERATION_REGION OperationRegion;

    ASSERT(OsContext != NULL);

    OperationRegion = (PIO_PORT_OPERATION_REGION)OsContext;
    MmFreePagedPool(OperationRegion);
    return;
}

KSTATUS
AcpipReadIoPortOperationRegion (
    PVOID OsContext,
    ULONGLONG Offset,
    ULONG Size,
    PVOID Value
    )

/*++

Routine Description:

    This routine performs a read from an I/O port Operation Region.

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

{

    USHORT ActualOffset;
    PIO_PORT_OPERATION_REGION OperationRegion;
    KSTATUS Status;

    ASSERT(OsContext != NULL);

    OperationRegion = (PIO_PORT_OPERATION_REGION)OsContext;

    //
    // Check the range.
    //

    if ((Offset >= OperationRegion->Length) ||
        (Offset + (Size / BITS_PER_BYTE) > OperationRegion->Length) ||
        (Offset >= Offset + (Size / BITS_PER_BYTE))) {

        Status = STATUS_OUT_OF_BOUNDS;
        goto ReadIoPortOperationRegionEnd;
    }

    //
    // Perform the read.
    //

    ActualOffset = OperationRegion->Offset + (USHORT)Offset;
    switch (Size) {
    case 8:
        *((PUCHAR)Value) = HlIoPortInByte(ActualOffset);
        break;

    case 16:
        *((PUSHORT)Value) = HlIoPortInShort(ActualOffset);
        break;

    case 32:
        *((PULONG)Value) = HlIoPortInLong(ActualOffset);
        break;

    case 64:
        *((PULONG)Value) = HlIoPortInLong(ActualOffset);
        *((PULONG)Value + 1) = HlIoPortInLong(ActualOffset + sizeof(ULONG));
        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        goto ReadIoPortOperationRegionEnd;
    }

    Status = STATUS_SUCCESS;

ReadIoPortOperationRegionEnd:
    return Status;
}

KSTATUS
AcpipWriteIoPortOperationRegion (
    PVOID OsContext,
    ULONGLONG Offset,
    ULONG Size,
    PVOID Value
    )

/*++

Routine Description:

    This routine performs a write to an I/O Port Operation Region.

Arguments:

    OsContext - Supplies the context pointer supplied by the OS when the
        Operation Region was created.

    Offset - Supplies the byte offset within the operation region to write to.

    Size - Supplies the size of the write to perform. Valid values are 8, 16,
        32, and 64. Other values are considered invalid.

    Value - Supplies the value to write.

Return Value:

    STATUS_SUCCESS on success.

    Other error codes on failure.

--*/

{

    USHORT ActualOffset;
    PIO_PORT_OPERATION_REGION OperationRegion;
    KSTATUS Status;

    ASSERT(OsContext != NULL);

    OperationRegion = (PIO_PORT_OPERATION_REGION)OsContext;

    //
    // Check the range.
    //

    if ((Offset >= OperationRegion->Length) ||
        (Offset + (Size / BITS_PER_BYTE) > OperationRegion->Length) ||
        (Offset >= Offset + (Size / BITS_PER_BYTE))) {

        Status = STATUS_OUT_OF_BOUNDS;
        goto WriteIoPortOperationRegionEnd;
    }

    //
    // Perform the write.
    //

    ActualOffset = OperationRegion->Offset + (USHORT)Offset;
    switch (Size) {
    case 8:
        HlIoPortOutByte(ActualOffset, *((PUCHAR)Value));
        break;

    case 16:
        HlIoPortOutShort(ActualOffset, *((PUSHORT)Value));
        break;

    case 32:
        HlIoPortOutLong(ActualOffset, *((PULONG)Value));
        break;

    case 64:
        HlIoPortOutLong(ActualOffset, *((PULONG)Value));
        HlIoPortOutLong(ActualOffset, *((PULONG)Value + 1));
        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        goto WriteIoPortOperationRegionEnd;
    }

    Status = STATUS_SUCCESS;

WriteIoPortOperationRegionEnd:
    return Status;
}

//
// PCI Configuration space Operation Region handlers.
//

KSTATUS
AcpipCreatePciConfigOperationRegion (
    PVOID AcpiObject,
    ULONGLONG Offset,
    ULONGLONG Length,
    PVOID *OsContext
    )

/*++

Routine Description:

    This routine creates an ACPI Operation Region to PCI Configuration space.

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

{

    PPCI_CONFIG_OPERATION_REGION OperationRegion;
    KSTATUS Status;

    //
    // Allocate space for the operation region.
    //

    OperationRegion = MmAllocatePagedPool(sizeof(PCI_CONFIG_OPERATION_REGION),
                                          ACPI_OS_ALLOCATION_TAG);

    if (OperationRegion == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreatePciConfigOperationRegionEnd;
    }

    RtlZeroMemory(OperationRegion, sizeof(PCI_CONFIG_OPERATION_REGION));
    OperationRegion->Offset = Offset;
    OperationRegion->Length = Length;
    OperationRegion->AcpiObject = (PACPI_OBJECT)AcpiObject;
    Status = STATUS_SUCCESS;

CreatePciConfigOperationRegionEnd:
    if (!KSUCCESS(Status)) {
        if (OperationRegion != NULL) {
            MmFreePagedPool(OperationRegion);
            OperationRegion = NULL;
        }
    }

    *OsContext = OperationRegion;
    return STATUS_SUCCESS;
}

VOID
AcpipDestroyPciConfigOperationRegion (
    PVOID OsContext
    )

/*++

Routine Description:

    This routine tears down OS support for a PCI Configuration space Operation
    Region.

Arguments:

    OsContext - Supplies the context pointer supplied by the OS when the
        Operation Region was created.

Return Value:

    None. No further accesses to the given operation region will be made.

--*/

{

    PPCI_CONFIG_OPERATION_REGION OperationRegion;

    ASSERT(OsContext != NULL);

    OperationRegion = (PPCI_CONFIG_OPERATION_REGION)OsContext;
    MmFreePagedPool(OperationRegion);
    return;
}

KSTATUS
AcpipReadPciConfigOperationRegion (
    PVOID OsContext,
    ULONGLONG Offset,
    ULONG Size,
    PVOID Value
    )

/*++

Routine Description:

    This routine performs a read from a PCI Configuration space Operation
    Region.

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

{

    ULONG ActualOffset;
    PVOID DeviceToken;
    PPCI_CONFIG_OPERATION_REGION OperationRegion;
    BOOL PciLockHeld;
    ULONGLONG ReadValue;
    KSTATUS Status;

    ASSERT(OsContext != NULL);

    PciLockHeld = FALSE;
    OperationRegion = (PPCI_CONFIG_OPERATION_REGION)OsContext;
    if (OperationRegion->Configured == FALSE) {

        //
        // Acquire the lock to prevent the PCI driver from coming online during
        // this early access.
        //

        AcpipAcquirePciLock();
        Status = AcpipConfigurePciConfigOperationRegion(OperationRegion);

        //
        // Let the lock go now if the region was configured, otherwise, keep it
        // through the duration of the access.
        //

        if (OperationRegion->Configured != FALSE) {
            AcpipReleasePciLock();

        } else {
            PciLockHeld = TRUE;
        }

        if (!KSUCCESS(Status)) {
            goto ReadPciConfigOperationRegionEnd;
        }
    }

    //
    // Check the range.
    //

    if ((Offset >= OperationRegion->Length) ||
        (Offset + (Size / BITS_PER_BYTE) > OperationRegion->Length) ||
        (Offset >= Offset + (Size / BITS_PER_BYTE))) {

        Status = STATUS_OUT_OF_BOUNDS;
        goto ReadPciConfigOperationRegionEnd;
    }

    ActualOffset = OperationRegion->Offset + (ULONG)Offset;

    //
    // Use the built in early access methods if the region is still not
    // configured due to no PCI devices being alive yet.
    //

    if (OperationRegion->Configured == FALSE) {
        ReadValue = AcpipEarlyReadPciConfigurationSpace(
                                        (UCHAR)OperationRegion->BusNumber,
                                        (UCHAR)OperationRegion->DeviceNumber,
                                        (UCHAR)OperationRegion->FunctionNumber,
                                        ActualOffset,
                                        Size / BITS_PER_BYTE);

    } else {

        //
        // Perform the read using normal access or specific access.
        //

        if (OperationRegion->UsingSpecificAccess == FALSE) {
            DeviceToken = OperationRegion->U.Access.DeviceToken;
            Status = OperationRegion->U.Access.ReadPciConfig(
                                                        DeviceToken,
                                                        ActualOffset,
                                                        Size / BITS_PER_BYTE,
                                                        &ReadValue);

            if (!KSUCCESS(Status)) {
                goto ReadPciConfigOperationRegionEnd;
            }

        } else {
            DeviceToken = OperationRegion->U.SpecificAccess.DeviceToken;
            Status = OperationRegion->U.SpecificAccess.ReadPciConfig(
                                               DeviceToken,
                                               OperationRegion->BusNumber,
                                               OperationRegion->DeviceNumber,
                                               OperationRegion->FunctionNumber,
                                               ActualOffset,
                                               Size / BITS_PER_BYTE,
                                               &ReadValue);

            if (!KSUCCESS(Status)) {
                goto ReadPciConfigOperationRegionEnd;
            }
        }
    }

    RtlCopyMemory(Value, &ReadValue, Size / BITS_PER_BYTE);
    Status = STATUS_SUCCESS;

ReadPciConfigOperationRegionEnd:
    if (PciLockHeld != FALSE) {
        AcpipReleasePciLock();
    }

    return Status;
}

KSTATUS
AcpipWritePciConfigOperationRegion (
    PVOID OsContext,
    ULONGLONG Offset,
    ULONG Size,
    PVOID Value
    )

/*++

Routine Description:

    This routine performs a write to a PCI Configuration space Operation Region.

Arguments:

    OsContext - Supplies the context pointer supplied by the OS when the
        Operation Region was created.

    Offset - Supplies the byte offset within the operation region to write to.

    Size - Supplies the size of the write to perform. Valid values are 8, 16,
        32, and 64. Other values are considered invalid.

    Value - Supplies the value to write.

Return Value:

    STATUS_SUCCESS on success.

    Other error codes on failure.

--*/

{

    ULONG ActualOffset;
    PVOID DeviceToken;
    PPCI_CONFIG_OPERATION_REGION OperationRegion;
    BOOL PciLockHeld;
    KSTATUS Status;
    ULONGLONG WriteValue;

    ASSERT(OsContext != NULL);

    PciLockHeld = FALSE;
    OperationRegion = (PPCI_CONFIG_OPERATION_REGION)OsContext;
    if (OperationRegion->Configured == FALSE) {

        //
        // Acquire the lock to prevent the PCI driver from coming online during
        // this early access.
        //

        AcpipAcquirePciLock();
        Status = AcpipConfigurePciConfigOperationRegion(OperationRegion);

        //
        // Let the lock go now if the region was configured, otherwise, keep it
        // through the duration of the access.
        //

        if (OperationRegion->Configured != FALSE) {
            AcpipReleasePciLock();

        } else {
            PciLockHeld = TRUE;
        }

        if (!KSUCCESS(Status)) {
            goto WritePciConfigOperationRegionEnd;
        }
    }

    //
    // Check the range.
    //

    if ((Offset >= OperationRegion->Length) ||
        (Offset + (Size / BITS_PER_BYTE) > OperationRegion->Length) ||
        (Offset >= Offset + (Size / BITS_PER_BYTE))) {

        Status = STATUS_OUT_OF_BOUNDS;
        goto WritePciConfigOperationRegionEnd;
    }

    //
    // Perform the configuration space write.
    //

    ActualOffset = OperationRegion->Offset + (ULONG)Offset;
    WriteValue = 0;
    RtlCopyMemory(&WriteValue, Value, Size / BITS_PER_BYTE);

    //
    // Use the built in early access methods if the region is still not
    // configured due to no PCI devices being alive yet.
    //

    if (OperationRegion->Configured == FALSE) {
        AcpipEarlyWritePciConfigurationSpace(
                                        (UCHAR)OperationRegion->BusNumber,
                                        (UCHAR)OperationRegion->DeviceNumber,
                                        (UCHAR)OperationRegion->FunctionNumber,
                                        ActualOffset,
                                        Size / BITS_PER_BYTE,
                                        WriteValue);

    } else {

        //
        // Perform the read using normal access or specific access.
        //

        if (OperationRegion->UsingSpecificAccess == FALSE) {
            DeviceToken = OperationRegion->U.Access.DeviceToken;
            Status = OperationRegion->U.Access.WritePciConfig(
                                                          DeviceToken,
                                                          ActualOffset,
                                                          Size / BITS_PER_BYTE,
                                                          WriteValue);

            if (!KSUCCESS(Status)) {
                goto WritePciConfigOperationRegionEnd;
            }

        } else {
            DeviceToken = OperationRegion->U.SpecificAccess.DeviceToken;
            Status = OperationRegion->U.SpecificAccess.WritePciConfig(
                                               DeviceToken,
                                               OperationRegion->BusNumber,
                                               OperationRegion->DeviceNumber,
                                               OperationRegion->FunctionNumber,
                                               ActualOffset,
                                               Size / BITS_PER_BYTE,
                                               WriteValue);

            if (!KSUCCESS(Status)) {
                goto WritePciConfigOperationRegionEnd;
            }
        }
    }

    Status = STATUS_SUCCESS;

WritePciConfigOperationRegionEnd:
    if (PciLockHeld != FALSE) {
        AcpipReleasePciLock();
    }

    return Status;
}

KSTATUS
AcpipConfigurePciConfigOperationRegion (
    PPCI_CONFIG_OPERATION_REGION OperationRegion
    )

/*++

Routine Description:

    This routine attempts to set up a PCI operation region for immediate use.

Arguments:

    OperationRegion - Supplies a pointer to the operation region to configure.

Return Value:

    Status code.

--*/

{

    ULONGLONG BusAddress;
    PACPI_OBJECT Device;
    BOOL FirstTime;
    PDEVICE OperationRegionDevice;
    PIRP QueryInterfaceIrp;
    PACPI_OBJECT RootDevice;
    KSTATUS Status;
    BOOL UseSpecificAccess;

    UseSpecificAccess = FALSE;

    ASSERT(OperationRegion->Configured == FALSE);

    //
    // Attempt to find the device associated with this PCI config operation
    // region.
    //

    Device = OperationRegion->AcpiObject;
    FirstTime = TRUE;
    while (TRUE) {
        if (FirstTime == FALSE) {
            Device = Device->Parent;
        }

        FirstTime = FALSE;
        while (Device != NULL) {
            if (Device->Type == AcpiObjectDevice) {
                break;
            }

            Device = Device->Parent;
        }

        //
        // If there is no parent device, this is not a valid operation region.
        //

        if (Device == NULL) {
            Status = STATUS_NO_ELIGIBLE_DEVICES;
            goto ConfigurePciConfigOperationRegionEnd;
        }

        //
        // If there is no OS device for the operation region's destination,
        // then this device is not currently ready to be queried for PCI
        // configuration space access.
        //

        OperationRegionDevice = Device->U.Device.OsDevice;
        if (OperationRegionDevice == NULL) {

            //
            // Get the bus address of the namespace object. If this device has
            // no bus address, look up for the next one.
            //

            Status = AcpipGetDeviceBusAddress(Device, &BusAddress);
            if (!KSUCCESS(Status)) {
                continue;
            }

            OperationRegion->BusNumber = 0;
            OperationRegion->DeviceNumber = (ULONG)(BusAddress >> 16);
            OperationRegion->FunctionNumber = (ULONG)(BusAddress & 0xFFFF);

            //
            // Go up the chain looking for a PCI bus or bridge that's set up.
            //

            RootDevice = AcpipGetSystemBusRoot();
            Device = Device->Parent;
            while (Device != RootDevice) {
                if ((Device->Type == AcpiObjectDevice) &&
                    (Device->U.Device.IsPciBus != FALSE) &&
                    (Device->U.Device.IsDeviceStarted != FALSE)) {

                    UseSpecificAccess = TRUE;
                    break;
                }

                Device = Device->Parent;
            }

            //
            // If nothing is ready or configured, then use the "early" PCI
            // config access routines.
            //

            if (Device == RootDevice) {
                Status = STATUS_SUCCESS;
                goto ConfigurePciConfigOperationRegionEnd;
            }

            OperationRegionDevice = Device->U.Device.OsDevice;
        }

        //
        // Allocate and send an IRP to the bus driver requesting access
        // to the device's PCI config space.
        //

        QueryInterfaceIrp = IoCreateIrp(OperationRegionDevice,
                                        IrpMajorStateChange,
                                        0);

        if (QueryInterfaceIrp == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ConfigurePciConfigOperationRegionEnd;
        }

        QueryInterfaceIrp->MinorCode = IrpMinorQueryInterface;

        //
        // Request configuration space access from the device directly if
        // possible, or request specific access to the device from the bus if
        // the device is not yet started.
        //

        if (UseSpecificAccess != FALSE) {
            QueryInterfaceIrp->U.QueryInterface.Interface =
                                                    &AcpiSpecificPciConfigUuid;

            QueryInterfaceIrp->U.QueryInterface.InterfaceBuffer =
                                          &(OperationRegion->U.SpecificAccess);

            QueryInterfaceIrp->U.QueryInterface.InterfaceBufferSize =
                                  sizeof(INTERFACE_SPECIFIC_PCI_CONFIG_ACCESS);

        } else {
            QueryInterfaceIrp->U.QueryInterface.Interface = &AcpiPciConfigUuid;
            QueryInterfaceIrp->U.QueryInterface.InterfaceBuffer =
                                                  &(OperationRegion->U.Access);

            QueryInterfaceIrp->U.QueryInterface.InterfaceBufferSize =
                                       sizeof(INTERFACE_PCI_CONFIG_ACCESS);
        }

        Status = IoSendSynchronousIrp(QueryInterfaceIrp);
        if (!KSUCCESS(Status)) {
            IoDestroyIrp(QueryInterfaceIrp);
            continue;
        }

        if (!KSUCCESS(IoGetIrpStatus(QueryInterfaceIrp))) {
            IoDestroyIrp(QueryInterfaceIrp);
            continue;
        }

        OperationRegion->UsingSpecificAccess = UseSpecificAccess;
        OperationRegion->Configured = TRUE;
        IoDestroyIrp(QueryInterfaceIrp);
        break;
    }

ConfigurePciConfigOperationRegionEnd:
    return Status;
}

