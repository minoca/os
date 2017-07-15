/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    resdesc.c

Abstract:

    This module implements support functions for handling ACPI resource
    descriptors.

Author:

    Evan Green 4-Aug-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/intrface/acpi.h>
#include "acpip.h"
#include "namespce.h"
#include "resdesc.h"

//
// --------------------------------------------------------------------- Macros
//

#define ASSERT_SPB_UART_CONTROL_LINES_EQUIVALENT()                          \
    ASSERT((ACPI_SPB_UART_CONTROL_DTD == RESOURCE_SPB_UART_CONTROL_DTD) &&  \
           (ACPI_SPB_UART_CONTROL_RI == RESOURCE_SPB_UART_CONTROL_RI) &&    \
           (ACPI_SPB_UART_CONTROL_DSR == RESOURCE_SPB_UART_CONTROL_DSR) &&  \
           (ACPI_SPB_UART_CONTROL_DTR == RESOURCE_SPB_UART_CONTROL_DTR) &&  \
           (ACPI_SPB_UART_CONTROL_CTS == RESOURCE_SPB_UART_CONTROL_CTS) &&  \
           (ACPI_SPB_UART_CONTROL_RTS == RESOURCE_SPB_UART_CONTROL_RTS))

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
AcpipConvertFromGenericAddressDescriptor (
    PVOID GenericAddressBuffer,
    ULONG BufferLength,
    ULONG TypeSize,
    BOOL Extended,
    PRESOURCE_REQUIREMENT_LIST RequirementList
    );

KSTATUS
AcpipParseSmallDmaDescriptor (
    PVOID Buffer,
    ULONG BufferLength,
    PRESOURCE_REQUIREMENT_LIST RequirementList
    );

KSTATUS
AcpipParseSmallFixedDmaDescriptor (
    PVOID Buffer,
    ULONG BufferLength,
    PRESOURCE_REQUIREMENT_LIST RequirementList
    );

KSTATUS
AcpipParseSmallIrqDescriptor (
    PVOID Buffer,
    ULONG BufferLength,
    PRESOURCE_REQUIREMENT_LIST RequirementList
    );

KSTATUS
AcpipParseLargeIrqDescriptor (
    PVOID Buffer,
    ULONG BufferLength,
    PRESOURCE_REQUIREMENT_LIST RequirementList
    );

KSTATUS
AcpipParseGpioDescriptor (
    PACPI_OBJECT NamespaceStart,
    PVOID Buffer,
    ULONG BufferLength,
    PRESOURCE_REQUIREMENT_LIST RequirementList
    );

KSTATUS
AcpipParseSpbDescriptor (
    PACPI_OBJECT NamespaceStart,
    PVOID Buffer,
    ULONG BufferLength,
    PRESOURCE_REQUIREMENT_LIST RequirementList
    );

KSTATUS
AcpipParseSpbI2cDescriptor (
    USHORT TypeSpecificFlags,
    UCHAR TypeSpecificRevisionId,
    USHORT TypeDataLength,
    PUCHAR Buffer,
    PRESOURCE_REQUIREMENT Requirement,
    PRESOURCE_SPB_I2C Descriptor
    );

KSTATUS
AcpipParseSpbSpiDescriptor (
    USHORT TypeSpecificFlags,
    UCHAR TypeSpecificRevisionId,
    USHORT TypeDataLength,
    PUCHAR Buffer,
    PRESOURCE_REQUIREMENT Requirement,
    PRESOURCE_SPB_SPI Descriptor
    );

KSTATUS
AcpipParseSpbUartDescriptor (
    USHORT TypeSpecificFlags,
    UCHAR TypeSpecificRevisionId,
    USHORT TypeDataLength,
    PUCHAR Buffer,
    PRESOURCE_SPB_UART Descriptor
    );

KSTATUS
AcpipParseLargeVendorDescriptor (
    PVOID Buffer,
    ULONG BufferLength,
    PRESOURCE_REQUIREMENT_LIST RequirementList
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
AcpipParseGenericAddress (
    PACPI_OBJECT ResourceBuffer,
    PGENERIC_ADDRESS GenericAddress
    )

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

{

    PUCHAR Buffer;
    UCHAR Byte;
    USHORT DescriptorLength;
    ULONGLONG RemainingSize;
    KSTATUS Status;

    if ((ResourceBuffer == NULL) ||
        (ResourceBuffer->Type != AcpiObjectBuffer)) {

        Status = STATUS_INVALID_PARAMETER;
        goto ReadGenericAddressEnd;
    }

    Buffer = ResourceBuffer->U.Buffer.Buffer;
    RemainingSize = ResourceBuffer->U.Buffer.Length;
    Byte = *Buffer;
    RemainingSize -= 1;
    Buffer += 1;
    if ((Byte & RESOURCE_DESCRIPTOR_LARGE) == 0) {
        Status = STATUS_MALFORMED_DATA_STREAM;
        goto ReadGenericAddressEnd;
    }

    if (RemainingSize < 2) {
        Status = STATUS_MALFORMED_DATA_STREAM;
        goto ReadGenericAddressEnd;
    }

    DescriptorLength = *((PUSHORT)Buffer);
    Buffer += 2;
    RemainingSize -= 2;
    switch (Byte & LARGE_RESOURCE_TYPE_MASK) {
    case LARGE_RESOURCE_TYPE_GENERIC_REGISTER:
        if (DescriptorLength < sizeof(GENERIC_ADDRESS)) {
            Status = STATUS_MALFORMED_DATA_STREAM;
            goto ReadGenericAddressEnd;
        }

        RtlCopyMemory(GenericAddress, Buffer, sizeof(GENERIC_ADDRESS));
        break;

    default:
        Status = STATUS_MALFORMED_DATA_STREAM;
        goto ReadGenericAddressEnd;
    }

    Status = STATUS_SUCCESS;

ReadGenericAddressEnd:
    return Status;
}

KSTATUS
AcpipConvertFromAcpiResourceBuffer (
    PACPI_OBJECT Device,
    PACPI_OBJECT ResourceBuffer,
    PRESOURCE_CONFIGURATION_LIST *ConfigurationListResult
    )

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

{

    ULONGLONG Alignment;
    PUCHAR Buffer;
    UCHAR Byte;
    UCHAR Checksum;
    PRESOURCE_CONFIGURATION_LIST ConfigurationList;
    PRESOURCE_REQUIREMENT_LIST CurrentConfiguration;
    USHORT DescriptorLength;
    ULONGLONG Length;
    ULONGLONG Maximum;
    ULONGLONG Minimum;
    ULONGLONG RemainingSize;
    RESOURCE_REQUIREMENT Requirement;
    RESOURCE_TYPE ResourceType;
    KSTATUS Status;
    PUCHAR TemplateStart;
    BOOL Writeable;

    RtlZeroMemory(&Requirement, sizeof(RESOURCE_REQUIREMENT));
    ConfigurationList = NULL;
    CurrentConfiguration = NULL;
    if ((ResourceBuffer == NULL) ||
        (ResourceBuffer->Type != AcpiObjectBuffer)) {

        Status = STATUS_INVALID_PARAMETER;
        goto ConvertFromAcpiResourceBufferEnd;
    }

    //
    // Create an initial configuration list and configuration.
    //

    ConfigurationList = IoCreateResourceConfigurationList(NULL);
    if (ConfigurationList == NULL) {
        Status = STATUS_INVALID_PARAMETER;
        goto ConvertFromAcpiResourceBufferEnd;
    }

    CurrentConfiguration = IoCreateResourceRequirementList();
    if (CurrentConfiguration == NULL) {
        Status = STATUS_INVALID_PARAMETER;
        goto ConvertFromAcpiResourceBufferEnd;
    }

    //
    // Loop parsing the buffer.
    //

    Buffer = ResourceBuffer->U.Buffer.Buffer;
    RemainingSize = ResourceBuffer->U.Buffer.Length;
    TemplateStart = Buffer;
    while (RemainingSize != 0) {

        //
        // Investigate small resource types.
        //

        Byte = *Buffer;
        RemainingSize -= 1;
        Buffer += 1;
        if ((Byte & RESOURCE_DESCRIPTOR_LARGE) == 0) {
            DescriptorLength = Byte & RESOURCE_DESCRIPTOR_LENGTH_MASK;
            if (RemainingSize < DescriptorLength) {
                Status = STATUS_MALFORMED_DATA_STREAM;
                goto ConvertFromAcpiResourceBufferEnd;
            }

            switch (Byte & SMALL_RESOURCE_TYPE_MASK) {
            case SMALL_RESOURCE_TYPE_IRQ:
                Status = AcpipParseSmallIrqDescriptor(Buffer,
                                                      DescriptorLength,
                                                      CurrentConfiguration);

                if (!KSUCCESS(Status)) {
                    goto ConvertFromAcpiResourceBufferEnd;
                }

                break;

            case SMALL_RESOURCE_TYPE_DMA:
                Status = AcpipParseSmallDmaDescriptor(Buffer,
                                                      DescriptorLength,
                                                      CurrentConfiguration);

                if (!KSUCCESS(Status)) {
                    goto ConvertFromAcpiResourceBufferEnd;
                }

                break;

            case SMALL_RESOURCE_TYPE_START_DEPENDENT_FUNCTIONS:
                if (DescriptorLength == 1) {
                    RtlDebugPrint("Start Dependent Function: %x\n", *Buffer);

                } else {
                    RtlDebugPrint("Start Dependent Function\n");
                }

                ASSERT(FALSE);

                break;

            case SMALL_RESOURCE_TYPE_END_DEPENDENT_FUNCTIONS:
                RtlDebugPrint("End Dependent Function\n");

                ASSERT(FALSE);

                break;

            case SMALL_RESOURCE_TYPE_IO_PORT:
                if (DescriptorLength < 7) {
                    Status = STATUS_MALFORMED_DATA_STREAM;
                    goto ConvertFromAcpiResourceBufferEnd;
                }

                Requirement.Type = ResourceTypeIoPort;
                Requirement.Minimum = *((PUSHORT)(Buffer + 1));
                Requirement.Maximum = *((PUSHORT)(Buffer + 3)) + 1;
                Requirement.Alignment = *((PUCHAR)(Buffer + 5));
                Requirement.Length = *((PUCHAR)(Buffer + 6));
                if (Requirement.Maximum <
                    Requirement.Minimum + Requirement.Length) {

                    Requirement.Maximum = Requirement.Minimum +
                                          Requirement.Length;
                }

                Requirement.Characteristics = 0;
                Requirement.Flags = 0;
                Status = IoCreateAndAddResourceRequirement(&Requirement,
                                                           CurrentConfiguration,
                                                           NULL);

                if (!KSUCCESS(Status)) {
                    goto ConvertFromAcpiResourceBufferEnd;
                }

                break;

            case SMALL_RESOURCE_TYPE_FIXED_LOCATION_IO_PORT:
                if (DescriptorLength < 3) {
                    Status = STATUS_MALFORMED_DATA_STREAM;
                    goto ConvertFromAcpiResourceBufferEnd;
                }

                Requirement.Type = ResourceTypeIoPort;
                Requirement.Minimum = *((PUSHORT)Buffer);
                Requirement.Length = *(Buffer + 2);
                Requirement.Maximum = Requirement.Minimum + Requirement.Length;
                Requirement.Alignment = 1;
                Requirement.Characteristics = 0;
                Requirement.Flags = 0;
                Status = IoCreateAndAddResourceRequirement(&Requirement,
                                                           CurrentConfiguration,
                                                           NULL);

                if (!KSUCCESS(Status)) {
                    goto ConvertFromAcpiResourceBufferEnd;
                }

                break;

            case SMALL_RESOURCE_TYPE_FIXED_DMA:
                Status = AcpipParseSmallFixedDmaDescriptor(
                                                         Buffer,
                                                         DescriptorLength,
                                                         CurrentConfiguration);

                if (!KSUCCESS(Status)) {
                    goto ConvertFromAcpiResourceBufferEnd;
                }

                break;

            case SMALL_RESOURCE_TYPE_VENDOR_DEFINED:
                RtlDebugPrint("Vendor Defined, Length %d\n", DescriptorLength);

                ASSERT(FALSE);

                break;

            case SMALL_RESOURCE_TYPE_END_TAG:
                if (DescriptorLength < 1) {
                    Status = STATUS_MALFORMED_DATA_STREAM;
                    goto ConvertFromAcpiResourceBufferEnd;
                }

                //
                // Checksum the template, but only if the checksum field is
                // non-zero.
                //

                if (*Buffer != 0) {
                    Length = (UINTN)Buffer + 1 - (UINTN)TemplateStart;
                    Checksum = AcpipChecksumData(TemplateStart, (ULONG)Length);
                    if (Checksum != 0) {
                        RtlDebugPrint("ACPI: Resource template checksum "
                                      "failed. Start of template %x, Length "
                                      "%I64x, Checksum %x, Expected 0.\n",
                                      TemplateStart,
                                      Length,
                                      Checksum);

                        Status = STATUS_MALFORMED_DATA_STREAM;
                        goto ConvertFromAcpiResourceBufferEnd;
                    }
                }

                //
                // Add the current configuration to the configuration list.
                //

                Status = IoAddResourceConfiguration(CurrentConfiguration,
                                                    NULL,
                                                    ConfigurationList);

                if (!KSUCCESS(Status)) {
                    goto ConvertFromAcpiResourceBufferEnd;
                }

                CurrentConfiguration = NULL;

                //
                // If the buffer is not done, create a new configuration.
                //

                if (RemainingSize > DescriptorLength) {
                    CurrentConfiguration = IoCreateResourceRequirementList();
                    if (CurrentConfiguration == NULL) {
                        Status = STATUS_INSUFFICIENT_RESOURCES;
                        goto ConvertFromAcpiResourceBufferEnd;
                    }
                }

                break;

            default:
                RtlDebugPrint("ACPI: Error, found invalid resource descriptor "
                              "type 0x%02x.\n",
                              Byte & SMALL_RESOURCE_TYPE_MASK);

                Status = STATUS_MALFORMED_DATA_STREAM;
                goto ConvertFromAcpiResourceBufferEnd;
            }

        //
        // Parse a large descriptor.
        //

        } else {
            if (RemainingSize < 2) {
                Status = STATUS_MALFORMED_DATA_STREAM;
                goto ConvertFromAcpiResourceBufferEnd;
            }

            DescriptorLength = *((PUSHORT)Buffer);
            Buffer += 2;
            RemainingSize -= 2;
            switch (Byte & LARGE_RESOURCE_TYPE_MASK) {
            case LARGE_RESOURCE_TYPE_MEMORY24:
                if (DescriptorLength < 9) {
                    Status = STATUS_MALFORMED_DATA_STREAM;
                    goto ConvertFromAcpiResourceBufferEnd;
                }

                Writeable = FALSE;
                if ((*Buffer & ACPI_MEMORY_DESCRIPTOR_WRITEABLE) != 0) {
                    Writeable = TRUE;
                }

                Minimum = *((PUSHORT)(Buffer + 1));
                Minimum = Minimum << 8;
                Maximum = *((PUSHORT)(Buffer + 3));
                Maximum = Maximum << 8;
                Alignment = *((PUSHORT)(Buffer + 5));
                Length = *((PUSHORT)(Buffer + 7));
                Length = Length << 8;
                RtlDebugPrint("Memory24: Min 0x%I64x, Max 0x%I64x, Alignment "
                              "0x%I64x, Length 0x%I64x, Writeable: %d\n",
                              Minimum,
                              Maximum,
                              Alignment,
                              Length,
                              Writeable);

                ASSERT(FALSE);

                break;

            case LARGE_RESOURCE_TYPE_GENERIC_REGISTER:
                if (DescriptorLength < 12) {
                    Status = STATUS_MALFORMED_DATA_STREAM;
                    goto ConvertFromAcpiResourceBufferEnd;
                }

                //
                // Get the resource type.
                //

                switch (*Buffer) {
                case AddressSpaceMemory:
                    ResourceType = ResourceTypePhysicalAddressSpace;
                    break;

                case AddressSpaceIo:
                    ResourceType = ResourceTypeIoPort;
                    break;

                case AddressSpacePciConfig:
                case AddressSpaceEmbeddedController:
                case AddressSpaceSmBus:
                case AddressSpaceFixedHardware:
                default:
                    ResourceType = ResourceTypeVendorSpecific;
                    break;
                }

                //
                // Get the access size.
                //

                if (*(Buffer + 4) == 0) {
                    Alignment = 1;

                } else {
                    Alignment = 1ULL << (*(Buffer + 4) - 1);
                }

                Length = (*(Buffer + 2) + *(Buffer + 3)) / BITS_PER_BYTE;
                if (Length < Alignment) {
                    Length = Alignment;
                }

                Minimum = *((PULONGLONG)Buffer + 4);
                Maximum = Minimum + Length;
                RtlDebugPrint("Generic Register type %d, Minimum 0x%I64x, "
                              "Length 0x%I64x, Alignment 0x%I64x.\n",
                              ResourceType,
                              Minimum,
                              Length,
                              Alignment);

                ASSERT(FALSE);

                break;

            case LARGE_RESOURCE_TYPE_VENDOR_DEFINED:
                Status = AcpipParseLargeVendorDescriptor(Buffer,
                                                         DescriptorLength,
                                                         CurrentConfiguration);

                if (!KSUCCESS(Status)) {
                    goto ConvertFromAcpiResourceBufferEnd;
                }

                break;

            case LARGE_RESOURCE_TYPE_MEMORY32:
                if (DescriptorLength < 17) {
                    Status = STATUS_MALFORMED_DATA_STREAM;
                    goto ConvertFromAcpiResourceBufferEnd;
                }

                Writeable = FALSE;
                if ((*Buffer & ACPI_MEMORY_DESCRIPTOR_WRITEABLE) != 0) {
                    Writeable = TRUE;
                }

                Requirement.Type = ResourceTypePhysicalAddressSpace;
                Requirement.Minimum = *((PULONG)(Buffer + 1));
                Requirement.Maximum = *((PULONG)(Buffer + 5)) + 1;
                Requirement.Alignment = *((PULONG)(Buffer + 9));
                Requirement.Length = *((PULONG)(Buffer + 13));
                if (Requirement.Maximum <
                    Requirement.Minimum + Requirement.Length) {

                    Requirement.Maximum = Requirement.Minimum +
                                          Requirement.Length;
                }

                Requirement.Characteristics = 0;
                Requirement.Flags = 0;
                Status = IoCreateAndAddResourceRequirement(&Requirement,
                                                           CurrentConfiguration,
                                                           NULL);

                if (!KSUCCESS(Status)) {
                    goto ConvertFromAcpiResourceBufferEnd;
                }

                break;

            case LARGE_RESOURCE_TYPE_FIXED_MEMORY32:
                if (DescriptorLength < 9){
                    Status = STATUS_MALFORMED_DATA_STREAM;
                    goto ConvertFromAcpiResourceBufferEnd;
                }

                Writeable = FALSE;
                if ((*Buffer & ACPI_MEMORY_DESCRIPTOR_WRITEABLE) != 0) {
                    Writeable = TRUE;
                }

                Requirement.Type = ResourceTypePhysicalAddressSpace;
                Requirement.Minimum = *((PULONG)(Buffer + 1));
                Requirement.Length = *((PULONG)(Buffer + 5));
                Requirement.Maximum = Requirement.Minimum + Requirement.Length;
                Requirement.Alignment = 1;
                Requirement.Characteristics = 0;
                Requirement.Flags = 0;
                Status = IoCreateAndAddResourceRequirement(&Requirement,
                                                           CurrentConfiguration,
                                                           NULL);

                if (!KSUCCESS(Status)) {
                    goto ConvertFromAcpiResourceBufferEnd;
                }

                break;

            case LARGE_RESOURCE_TYPE_ADDRESS_SPACE32:
                Status = AcpipConvertFromGenericAddressDescriptor(
                                                         Buffer,
                                                         DescriptorLength,
                                                         sizeof(ULONG),
                                                         FALSE,
                                                         CurrentConfiguration);

                if (!KSUCCESS(Status)) {
                    goto ConvertFromAcpiResourceBufferEnd;
                }

                break;

            case LARGE_RESOURCE_TYPE_ADDRESS_SPACE16:
                Status = AcpipConvertFromGenericAddressDescriptor(
                                                         Buffer,
                                                         DescriptorLength,
                                                         sizeof(USHORT),
                                                         FALSE,
                                                         CurrentConfiguration);

                if (!KSUCCESS(Status)) {
                    goto ConvertFromAcpiResourceBufferEnd;
                }

                break;

            case LARGE_RESOURCE_TYPE_IRQ:
                Status = AcpipParseLargeIrqDescriptor(Buffer,
                                                      DescriptorLength,
                                                      CurrentConfiguration);

                if (!KSUCCESS(Status)) {
                    goto ConvertFromAcpiResourceBufferEnd;
                }

                break;

            case LARGE_RESOURCE_TYPE_ADDRESS_SPACE64:
                Status = AcpipConvertFromGenericAddressDescriptor(
                                                         Buffer,
                                                         DescriptorLength,
                                                         sizeof(ULONGLONG),
                                                         FALSE,
                                                         CurrentConfiguration);

                if (!KSUCCESS(Status)) {
                    goto ConvertFromAcpiResourceBufferEnd;
                }

                break;

            case LARGE_RESOURCE_TYPE_ADDRESS_SPACE_EXTENDED:
                Status = AcpipConvertFromGenericAddressDescriptor(
                                                         Buffer,
                                                         DescriptorLength,
                                                         sizeof(ULONGLONG),
                                                         TRUE,
                                                         CurrentConfiguration);

                if (!KSUCCESS(Status)) {
                    goto ConvertFromAcpiResourceBufferEnd;
                }

                break;

            case LARGE_RESOURCE_TYPE_GPIO:
                Status = AcpipParseGpioDescriptor(Device,
                                                  Buffer,
                                                  DescriptorLength,
                                                  CurrentConfiguration);

                if (!KSUCCESS(Status)) {
                    goto ConvertFromAcpiResourceBufferEnd;
                }

                break;

            case LARGE_RESOURCE_TYPE_SPB:
                Status = AcpipParseSpbDescriptor(Device,
                                                 Buffer,
                                                 DescriptorLength,
                                                 CurrentConfiguration);

                if (!KSUCCESS(Status)) {
                    goto ConvertFromAcpiResourceBufferEnd;
                }

                break;

            default:
                RtlDebugPrint("ACPI: Error, found invalid resource descriptor "
                              "type 0x%02x.\n",
                              Byte & LARGE_RESOURCE_TYPE_MASK);

                Status = STATUS_MALFORMED_DATA_STREAM;
                goto ConvertFromAcpiResourceBufferEnd;
            }
        }

        //
        // Advance the buffer beyond this descriptor.
        //

        Buffer += DescriptorLength;
        RemainingSize -= DescriptorLength;
    }

    Status = STATUS_SUCCESS;

ConvertFromAcpiResourceBufferEnd:
    if (CurrentConfiguration != NULL) {
        IoDestroyResourceRequirementList(CurrentConfiguration);
    }

    if (!KSUCCESS(Status)) {
        if (ConfigurationList != NULL) {
            IoDestroyResourceConfigurationList(ConfigurationList);
            ConfigurationList = NULL;
        }
    }

    *ConfigurationListResult = ConfigurationList;
    return Status;
}

KSTATUS
AcpipConvertFromRequirementListToAllocationList (
    PRESOURCE_CONFIGURATION_LIST ConfigurationList,
    PRESOURCE_ALLOCATION_LIST *AllocationList
    )

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

{

    RESOURCE_ALLOCATION Allocation;
    PRESOURCE_ALLOCATION_LIST NewAllocationList;
    PRESOURCE_REQUIREMENT Requirement;
    PRESOURCE_REQUIREMENT_LIST RequirementList;
    KSTATUS Status;

    //
    // Create a new allocation list.
    //

    NewAllocationList = IoCreateResourceAllocationList();
    if (NewAllocationList == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ConvertFromRequirementListToAllocationListEnd;
    }

    //
    // Get the first configuration.
    //

    RequirementList = IoGetNextResourceConfiguration(ConfigurationList, NULL);
    if (RequirementList == NULL) {
        Status = STATUS_UNSUCCESSFUL;
        goto ConvertFromRequirementListToAllocationListEnd;
    }

    //
    // Loop through every requirement in the list, and construct a resource
    // allocation based off the requirement's minimum and length.
    //

    RtlZeroMemory(&Allocation, sizeof(RESOURCE_ALLOCATION));
    Requirement = IoGetNextResourceRequirement(RequirementList, NULL);
    while (Requirement != NULL) {
        Allocation.Type = Requirement->Type;
        Allocation.Allocation = Requirement->Minimum;
        Allocation.Length = Requirement->Length;
        Allocation.Characteristics = Requirement->Characteristics;
        Allocation.Flags = Requirement->Flags;
        Allocation.Data = Requirement->Data;
        Allocation.DataSize = Requirement->DataSize;
        Allocation.Provider = Requirement->Provider;

        ASSERT(Requirement->Minimum + Requirement->Length <=
               Requirement->Maximum);

        Status = IoCreateAndAddResourceAllocation(&Allocation,
                                                  NewAllocationList);

        if (!KSUCCESS(Status)) {
            goto ConvertFromRequirementListToAllocationListEnd;
        }

        Requirement = IoGetNextResourceRequirement(RequirementList,
                                                   Requirement);
    }

    Status = STATUS_SUCCESS;

ConvertFromRequirementListToAllocationListEnd:
    if (!KSUCCESS(Status)) {
        if (NewAllocationList != NULL) {
            IoDestroyResourceAllocationList(NewAllocationList);
            NewAllocationList = NULL;
        }
    }

    *AllocationList = NewAllocationList;
    return Status;
}

KSTATUS
AcpipConvertToAcpiResourceBuffer (
    PRESOURCE_ALLOCATION_LIST AllocationList,
    PACPI_OBJECT ResourceBuffer
    )

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

{

    ULONGLONG Alignment;
    PRESOURCE_ALLOCATION Allocation;
    PUCHAR Buffer;
    UCHAR Byte;
    USHORT DescriptorLength;
    UCHAR Flags;
    ULONGLONG Length;
    ULONGLONG Maximum;
    ULONGLONG Minimum;
    ULONGLONG RemainingSize;
    RESOURCE_TYPE ResourceType;
    KSTATUS Status;
    BOOL StayOnCurrentAllocation;
    BOOL Writeable;

    if ((ResourceBuffer == NULL) ||
        (ResourceBuffer->Type != AcpiObjectBuffer)) {

        Status = STATUS_INVALID_PARAMETER;
        goto ConvertToAcpiResourceBufferEnd;
    }

    //
    // Loop parsing the buffer.
    //

    Buffer = ResourceBuffer->U.Buffer.Buffer;
    RemainingSize = ResourceBuffer->U.Buffer.Length;
    Allocation = NULL;
    StayOnCurrentAllocation = FALSE;
    while (RemainingSize != 0) {

        //
        // Get the next resource allocation.
        //

        if (StayOnCurrentAllocation == FALSE) {
            Allocation = IoGetNextResourceAllocation(AllocationList,
                                                     Allocation);
        }

        StayOnCurrentAllocation = FALSE;

        //
        // Investigate small resource types.
        //

        Byte = *Buffer;
        RemainingSize -= 1;
        Buffer += 1;
        if ((Byte & RESOURCE_DESCRIPTOR_LARGE) == 0) {
            DescriptorLength = Byte & RESOURCE_DESCRIPTOR_LENGTH_MASK;
            if (RemainingSize < DescriptorLength) {
                Status = STATUS_MALFORMED_DATA_STREAM;
                goto ConvertToAcpiResourceBufferEnd;
            }

            switch (Byte & SMALL_RESOURCE_TYPE_MASK) {
            case SMALL_RESOURCE_TYPE_IRQ:

                ASSERT(DescriptorLength >= 2);

                if (Allocation->Type != ResourceTypeInterruptLine) {
                    Status = STATUS_UNEXPECTED_TYPE;
                    goto ConvertToAcpiResourceBufferEnd;
                }

                //
                // If multiple interrupt lines are selected, implement that
                // support.
                //

                ASSERT(Allocation->Length == 1);
                ASSERT(Allocation->Allocation <= 15);

                //
                // Set the interrupt line.
                //

                *((PUSHORT)Buffer) = 1 << Allocation->Allocation;
                break;

            case SMALL_RESOURCE_TYPE_DMA:

                ASSERT(DescriptorLength >= 2);

                *((PUCHAR)Buffer) = 1 << Allocation->Allocation;
                Flags = 0;
                if ((Allocation->Characteristics & DMA_TYPE_EISA_A) != 0) {
                    Flags |= ACPI_SMALL_DMA_SPEED_EISA_A;

                } else if ((Allocation->Characteristics &
                            DMA_TYPE_EISA_B) != 0) {

                    Flags |= ACPI_SMALL_DMA_SPEED_EISA_B;

                } else if ((Allocation->Characteristics &
                            DMA_TYPE_EISA_F) != 0) {

                    Flags |= ACPI_SMALL_DMA_SPEED_EISA_F;
                }

                if ((Allocation->Characteristics & DMA_BUS_MASTER) != 0) {
                    Flags |= ACPI_SMALL_DMA_BUS_MASTER;
                }

                if ((Allocation->Characteristics & DMA_TRANSFER_SIZE_8) != 0) {
                    if ((Allocation->Characteristics &
                         DMA_TRANSFER_SIZE_16) != 0) {

                        Flags |= ACPI_SMALL_DMA_SIZE_8_AND_16_BIT;

                    } else {
                        Flags |= ACPI_SMALL_DMA_SIZE_8_BIT;
                    }

                } else if ((Allocation->Characteristics &
                            DMA_TRANSFER_SIZE_16) != 0) {

                    Flags |= ACPI_SMALL_DMA_SIZE_16_BIT;
                }

                *((PUCHAR)Buffer + 1) = Flags;
                break;

            case SMALL_RESOURCE_TYPE_START_DEPENDENT_FUNCTIONS:
                if (DescriptorLength == 1) {
                    RtlDebugPrint("Start Dependent Function: %x\n", *Buffer);

                } else {
                    RtlDebugPrint("Start Dependent Function\n");
                }

                ASSERT(FALSE);

                break;

            case SMALL_RESOURCE_TYPE_END_DEPENDENT_FUNCTIONS:
                RtlDebugPrint("End Dependent Function\n");

                ASSERT(FALSE);

                break;

            case SMALL_RESOURCE_TYPE_IO_PORT:

                ASSERT(DescriptorLength >= 7);

                if (Allocation->Type != ResourceTypeIoPort) {
                    Length = *((PUCHAR)(Buffer + 6));
                    if (Length == 0) {
                        StayOnCurrentAllocation = TRUE;
                        break;
                    }

                    Status = STATUS_UNEXPECTED_TYPE;
                    goto ConvertToAcpiResourceBufferEnd;
                }

                //
                // Set the I/O port base.
                //

                ASSERT(Allocation->Length >= *((PUCHAR)(Buffer + 6)));
                ASSERT(Allocation->Allocation <= *((PUSHORT)(Buffer + 3)) + 1);
                ASSERT(Allocation->Allocation <= 0xFFFF);

                *((PUSHORT)(Buffer + 1)) = (USHORT)Allocation->Allocation;
                break;

            case SMALL_RESOURCE_TYPE_FIXED_LOCATION_IO_PORT:

                ASSERT(DescriptorLength >= 3);

                if (Allocation->Type != ResourceTypeIoPort) {
                    Length = *(Buffer + 2);
                    if (Length == 0) {
                        StayOnCurrentAllocation = TRUE;
                        break;
                    }

                    Status = STATUS_UNEXPECTED_TYPE;
                    goto ConvertToAcpiResourceBufferEnd;
                }

                ASSERT(Allocation->Allocation == *((PUSHORT)Buffer));
                ASSERT(Allocation->Length == *(Buffer + 2));

                break;

            case SMALL_RESOURCE_TYPE_VENDOR_DEFINED:
                RtlDebugPrint("Vendor Defined, Length %d\n", DescriptorLength);

                ASSERT(FALSE);

                break;

            case SMALL_RESOURCE_TYPE_END_TAG:

                ASSERT(DescriptorLength >= 1);

                //
                // Set the checksum field to zero.
                //

                *Buffer = 0;
                break;

            case LARGE_RESOURCE_TYPE_GPIO:
                RtlDebugPrint("ACPI: GPIO not implemented.\n");
                *Buffer = 0;
                break;

            default:
                RtlDebugPrint("ACPI: Error, found invalid resource descriptor "
                              "type 0x%02x.\n",
                              Byte & SMALL_RESOURCE_TYPE_MASK);

                Status = STATUS_MALFORMED_DATA_STREAM;
                goto ConvertToAcpiResourceBufferEnd;
            }

        //
        // Parse a large descriptor.
        //

        } else {

            ASSERT(RemainingSize >= 2);

            DescriptorLength = *((PUSHORT)Buffer);
            Buffer += 2;
            RemainingSize -= 2;
            switch (Byte & LARGE_RESOURCE_TYPE_MASK) {
            case LARGE_RESOURCE_TYPE_MEMORY24:

                ASSERT(DescriptorLength >= 9);

                Writeable = FALSE;
                if ((*Buffer & ACPI_MEMORY_DESCRIPTOR_WRITEABLE) != 0) {
                    Writeable = TRUE;
                }

                Minimum = *((PUSHORT)(Buffer + 1));
                Minimum = Minimum << 8;
                Maximum = *((PUSHORT)(Buffer + 3));
                Maximum = Maximum << 8;
                Alignment = *((PUSHORT)(Buffer + 5));
                Length = *((PUSHORT)(Buffer + 7));
                Length = Length << 8;
                RtlDebugPrint("Memory24: Min 0x%I64x, Max 0x%I64x, Alignment "
                              "0x%I64x, Length 0x%I64x, Writeable: %d\n",
                              Minimum,
                              Maximum,
                              Alignment,
                              Length,
                              Writeable);

                ASSERT(FALSE);

                break;

            case LARGE_RESOURCE_TYPE_GENERIC_REGISTER:

                ASSERT(DescriptorLength >= 12);

                //
                // Get the resource type.
                //

                switch (*Buffer) {
                case AddressSpaceMemory:
                    ResourceType = ResourceTypePhysicalAddressSpace;
                    break;

                case AddressSpaceIo:
                    ResourceType = ResourceTypeIoPort;
                    break;

                case AddressSpacePciConfig:
                case AddressSpaceEmbeddedController:
                case AddressSpaceSmBus:
                case AddressSpaceFixedHardware:
                default:
                    ResourceType = ResourceTypeVendorSpecific;
                    break;
                }

                //
                // Get the access size.
                //

                if (*(Buffer + 4) == 0) {
                    Alignment = 1;

                } else {
                    Alignment = 1ULL << (*(Buffer + 4) - 1);
                }

                Length = (*(Buffer + 2) + *(Buffer + 3)) / BITS_PER_BYTE;
                if (Length < Alignment) {
                    Length = Alignment;
                }

                Minimum = *((PULONGLONG)Buffer + 4);
                Maximum = Minimum + Length;
                RtlDebugPrint("Generic Register type %d, Minimum 0x%I64x, "
                              "Length 0x%I64x, Alignment 0x%I64x.\n",
                              ResourceType,
                              Minimum,
                              Length,
                              Alignment);

                ASSERT(FALSE);

                break;

            case LARGE_RESOURCE_TYPE_VENDOR_DEFINED:
                RtlDebugPrint("Vendor Defined, Length %x\n", DescriptorLength);

                ASSERT(FALSE);

                break;

            case LARGE_RESOURCE_TYPE_MEMORY32:

                ASSERT(DescriptorLength >= 17);

                Writeable = FALSE;
                if ((*Buffer & ACPI_MEMORY_DESCRIPTOR_WRITEABLE) != 0) {
                    Writeable = TRUE;
                }

                Minimum = *((PULONG)(Buffer + 1));
                Maximum = *((PULONG)(Buffer + 5));
                Alignment = *((PULONG)(Buffer + 9));
                Length = *((PULONG)(Buffer + 13));
                RtlDebugPrint("Memory32: Min 0x%I64x, Max 0x%I64x, Alignment "
                              "0x%I64x, Length 0x%I64x, Writeable %d\n",
                              Minimum,
                              Maximum,
                              Alignment,
                              Length,
                              Writeable);

                ASSERT(FALSE);

                break;

            case LARGE_RESOURCE_TYPE_FIXED_MEMORY32:

                ASSERT(DescriptorLength >= 9);

                Writeable = FALSE;
                if ((*Buffer & ACPI_MEMORY_DESCRIPTOR_WRITEABLE) != 0) {
                    Writeable = TRUE;
                }

                Minimum = *((PULONG)(Buffer + 1));
                Alignment = 1;
                Length = *((PULONG)(Buffer + 5));
                Maximum = Minimum + Length;
                RtlDebugPrint("Memory32Fixed: Min 0x%I64x, Max 0x%I64x, "
                              "Alignment 0x%I64x, Length 0x%I64x, Writeable "
                              "%d\n",
                              Minimum,
                              Maximum,
                              Alignment,
                              Length,
                              Writeable);

                ASSERT(FALSE);

                break;

            case LARGE_RESOURCE_TYPE_ADDRESS_SPACE32:

                ASSERT(DescriptorLength >= (3 + (5 * sizeof(ULONG))));
                ASSERT(FALSE);

                break;

            case LARGE_RESOURCE_TYPE_ADDRESS_SPACE16:

                ASSERT(DescriptorLength >= (3 + (5 * sizeof(USHORT))));
                ASSERT(FALSE);

                break;

            case LARGE_RESOURCE_TYPE_IRQ:

                ASSERT(DescriptorLength <= 2 + sizeof(ULONG));

                if (Allocation->Type != ResourceTypeInterruptLine) {
                    Status = STATUS_UNEXPECTED_TYPE;
                    goto ConvertToAcpiResourceBufferEnd;
                }

                //
                // If multiple interrupt lines are selected, implement that
                // support.
                //

                ASSERT(Allocation->Length == 1);

                //
                // Set the interrupt line.
                //

                *((PULONG)(Buffer + 2)) = (ULONG)Allocation->Allocation;
                break;

            case LARGE_RESOURCE_TYPE_ADDRESS_SPACE64:

                ASSERT(DescriptorLength >= (3 + (5 * sizeof(ULONGLONG))));
                ASSERT(FALSE);

                break;

            case LARGE_RESOURCE_TYPE_ADDRESS_SPACE_EXTENDED:

                ASSERT(DescriptorLength >= (3 + (6 * sizeof(ULONGLONG))));
                ASSERT(FALSE);

                break;

            default:
                RtlDebugPrint("ACPI: Error, found invalid resource descriptor "
                              "type 0x%02x.\n",
                              Byte & LARGE_RESOURCE_TYPE_MASK);

                Status = STATUS_MALFORMED_DATA_STREAM;
                goto ConvertToAcpiResourceBufferEnd;
            }
        }

        //
        // Advance the buffer beyond this descriptor.
        //

        Buffer += DescriptorLength;
        RemainingSize -= DescriptorLength;
    }

    Status = STATUS_SUCCESS;

ConvertToAcpiResourceBufferEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
AcpipConvertFromGenericAddressDescriptor (
    PVOID GenericAddressBuffer,
    ULONG BufferLength,
    ULONG TypeSize,
    BOOL Extended,
    PRESOURCE_REQUIREMENT_LIST RequirementList
    )

/*++

Routine Description:

    This routine converts an ACPI Generic Address descriptor into an OS
    resource requirement.

Arguments:

    GenericAddressBuffer - Supplies a pointer to the generic address buffer,
        immediately after the 2 length bits.

    BufferLength - Supplies the length of the buffer, in bytes.

    TypeSize - Supplies the type of the generic address descriptor, in bytes.
        This is the size of each address-related field in the structure.

    Extended - Supplies a boolean indicating if this is an extended resource
        descriptor (which as the type specific attributes field) or not.

    RequirementList - Supplies a pointer to a resource requirement list where
        a new resource requirement will be added on success.

Return Value:

    Status code.

--*/

{

    ULONGLONG Alignment;
    ULONGLONG Attributes;
    PUCHAR Buffer;
    ULONG FieldsNeeded;
    UCHAR Flags;
    ULONGLONG Length;
    ULONGLONG Maximum;
    BOOL MaximumFixed;
    ULONGLONG Minimum;
    BOOL MinimumFixed;
    RESOURCE_REQUIREMENT Requirement;
    RESOURCE_TYPE ResourceType;
    KSTATUS Status;
    ULONGLONG TranslationOffset;

    RtlZeroMemory(&Requirement, sizeof(RESOURCE_REQUIREMENT));
    FieldsNeeded = 5;
    if (Extended != FALSE) {
        FieldsNeeded = 6;
    }

    if (BufferLength < (3 + (FieldsNeeded * TypeSize))) {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto ConvertFromGenericAddressDescriptorEnd;
    }

    Buffer = (PUCHAR)GenericAddressBuffer;

    //
    // Determine the resource type.
    //

    ResourceType = ResourceTypeVendorSpecific;
    switch (*Buffer) {
    case GENERIC_ADDRESS_TYPE_MEMORY:
        ResourceType = ResourceTypePhysicalAddressSpace;
        break;

    case GENERIC_ADDRESS_TYPE_IO:
        ResourceType = ResourceTypeIoPort;
        break;

    case GENERIC_ADDRESS_TYPE_BUS_NUMBER:
        ResourceType = ResourceTypeBusNumber;
        break;

    default:
        break;
    }

    //
    // Determine the flag values.
    //

    Flags = *(Buffer + 1);
    MinimumFixed = FALSE;
    if ((Flags & GENERIC_ADDRESS_MINIMUM_FIXED) != 0) {
        MinimumFixed = TRUE;
    }

    MaximumFixed = FALSE;
    if ((Flags & GENERIC_ADDRESS_MAXIMUM_FIXED) != 0) {
        MaximumFixed = TRUE;
    }

    //
    // Get the alignment variable. This is billed in the descriptor as a
    // "Granularity" field, where bits set to 1 are decoded by the bus. Simply
    // add 1 to get back up to a power of 2 alignment.
    //

    Alignment = 0;
    RtlCopyMemory(&Alignment, Buffer + 3, TypeSize);
    Alignment += 1;
    Buffer += 3 + TypeSize;

    //
    // Get the minimum and maximum.
    //

    Minimum = 0;
    RtlCopyMemory(&Minimum, Buffer, TypeSize);
    Buffer += TypeSize;
    Maximum = 0;
    RtlCopyMemory(&Maximum, Buffer, TypeSize);
    Buffer += TypeSize;

    //
    // Get the translation offset and length.
    //

    TranslationOffset = 0;
    RtlCopyMemory(&TranslationOffset, Buffer, TypeSize);
    Buffer += TypeSize;
    Length = 0;
    RtlCopyMemory(&Length, Buffer, TypeSize);
    Buffer += TypeSize;

    //
    // Restrict the minimum or maximum depending on the flags.
    //

    if (MinimumFixed != FALSE) {
        Maximum = Minimum + Length - 1;

    } else if (MaximumFixed != FALSE) {
        Minimum = Maximum + 1 - Length;
    }

    //
    // Get the attributes for extended descriptors.
    //

    Attributes = 0;
    if (Extended != FALSE) {
        RtlCopyMemory(&Attributes, Buffer, TypeSize);
        Buffer += TypeSize;
    }

    Requirement.Type = ResourceType;
    Requirement.Minimum = Minimum;
    Requirement.Length = Length;
    Requirement.Maximum = Maximum + 1;
    Requirement.Alignment = Alignment;
    Requirement.Characteristics = Attributes;
    Requirement.Flags = RESOURCE_FLAG_NOT_SHAREABLE;
    Status = IoCreateAndAddResourceRequirement(&Requirement,
                                               RequirementList,
                                               NULL);

    if (!KSUCCESS(Status)) {
        goto ConvertFromGenericAddressDescriptorEnd;
    }

ConvertFromGenericAddressDescriptorEnd:
    return Status;
}

KSTATUS
AcpipParseSmallDmaDescriptor (
    PVOID Buffer,
    ULONG BufferLength,
    PRESOURCE_REQUIREMENT_LIST RequirementList
    )

/*++

Routine Description:

    This routine converts an ACPI small DMA descriptor into a resource
    requirement, and puts that requirement on the given requirement list.

Arguments:

    Buffer - Supplies a pointer to the DMA descriptor buffer, pointing after the
        byte identifying the descriptor as a short DMA descriptor.

    BufferLength - Supplies the length of the descriptor buffer.

    RequirementList - Supplies a pointer to the resource requirement list to
        put the descriptor on.

Return Value:

    Status code.

--*/

{

    UCHAR Flags;
    UCHAR Mask;
    PRESOURCE_REQUIREMENT NewRequirement;
    RESOURCE_REQUIREMENT Requirement;
    KSTATUS Status;

    NewRequirement = NULL;
    if (BufferLength < 2) {
        Status = STATUS_MALFORMED_DATA_STREAM;
        goto ParseSmallDmaDescriptorEnd;
    }

    RtlZeroMemory(&Requirement, sizeof(RESOURCE_REQUIREMENT));
    Requirement.Type = ResourceTypeDmaChannel;
    Mask = *((PUCHAR)Buffer);
    Flags = *((PUCHAR)Buffer + 1);

    //
    // Skip over zero bits.
    //

    while ((Mask != 0) && ((Mask & 0x1) == 0)) {
        Requirement.Minimum += 1;
        Mask = Mask >> 1;
    }

    //
    // Collect one bits.
    //

    Requirement.Maximum = Requirement.Minimum;
    while ((Mask & 0x1) != 0) {
        Requirement.Maximum += 1;
        Mask = Mask >> 1;
    }

    if (Mask != 0) {
        Status = STATUS_MALFORMED_DATA_STREAM;
        goto ParseSmallDmaDescriptorEnd;
    }

    Requirement.Length = 1;
    Requirement.Flags = RESOURCE_FLAG_NOT_SHAREABLE;

    //
    // Parse the flags.
    //

    switch (Flags & ACPI_SMALL_DMA_SPEED_MASK) {
    case ACPI_SMALL_DMA_SPEED_ISA:
        Requirement.Characteristics |= DMA_TYPE_ISA;
        break;

    case ACPI_SMALL_DMA_SPEED_EISA_A:
        Requirement.Characteristics |= DMA_TYPE_EISA_A;
        break;

    case ACPI_SMALL_DMA_SPEED_EISA_B:
        Requirement.Characteristics |= DMA_TYPE_EISA_B;
        break;

    case ACPI_SMALL_DMA_SPEED_EISA_F:
    default:
        Requirement.Characteristics |= DMA_TYPE_EISA_F;
        break;
    }

    switch (Flags & ACPI_SMALL_DMA_SIZE_MASK) {
    case ACPI_SMALL_DMA_SIZE_8_BIT:
        Requirement.Characteristics |= DMA_TRANSFER_SIZE_8;
        break;

    case ACPI_SMALL_DMA_SIZE_8_AND_16_BIT:
        Requirement.Characteristics |= DMA_TRANSFER_SIZE_8 |
                                       DMA_TRANSFER_SIZE_16;

        break;

    case ACPI_SMALL_DMA_SIZE_16_BIT:
    default:
        Requirement.Characteristics |= DMA_TRANSFER_SIZE_16;
        break;
    }

    if ((Flags & ACPI_SMALL_DMA_BUS_MASTER) != 0) {
        Requirement.Characteristics |= DMA_BUS_MASTER;
    }

    //
    // Register the requirement.
    //

    Status = IoCreateAndAddResourceRequirement(&Requirement,
                                               RequirementList,
                                               &NewRequirement);

    if (!KSUCCESS(Status)) {
        goto ParseSmallDmaDescriptorEnd;
    }

    Status = STATUS_SUCCESS;

ParseSmallDmaDescriptorEnd:
    if (!KSUCCESS(Status)) {
        if (NewRequirement != NULL) {
            IoRemoveResourceRequirement(NewRequirement);
        }
    }

    return Status;
}

KSTATUS
AcpipParseSmallFixedDmaDescriptor (
    PVOID Buffer,
    ULONG BufferLength,
    PRESOURCE_REQUIREMENT_LIST RequirementList
    )

/*++

Routine Description:

    This routine converts an ACPI small fixed DMA descriptor into a resource
    requirement, and puts that requirement on the given requirement list.

Arguments:

    Buffer - Supplies a pointer to the DMA descriptor buffer, pointing after the
        byte identifying the descriptor as a short fixed DMA descriptor.

    BufferLength - Supplies the length of the descriptor buffer.

    RequirementList - Supplies a pointer to the resource requirement list to
        put the descriptor on.

Return Value:

    Status code.

--*/

{

    RESOURCE_DMA_DATA DmaData;
    PRESOURCE_REQUIREMENT NewRequirement;
    RESOURCE_REQUIREMENT Requirement;
    KSTATUS Status;
    UCHAR Width;

    NewRequirement = NULL;
    if (BufferLength < 5) {
        Status = STATUS_MALFORMED_DATA_STREAM;
        goto ParseSmallFixedDmaDescriptorEnd;
    }

    RtlZeroMemory(&Requirement, sizeof(RESOURCE_REQUIREMENT));
    RtlZeroMemory(&DmaData, sizeof(RESOURCE_DMA_DATA));
    DmaData.Version = RESOURCE_DMA_DATA_VERSION;
    Requirement.Type = ResourceTypeDmaChannel;
    DmaData.Request = READ_UNALIGNED16(Buffer);
    Buffer += 2;
    Requirement.Minimum = READ_UNALIGNED16(Buffer);
    Buffer += 1;
    Requirement.Maximum = Requirement.Minimum + 1;
    Requirement.Length = 1;;
    Width = *((PUCHAR)Buffer);
    Buffer += 1;
    switch (Width) {
    case ACPI_SMALL_FIXED_DMA_8BIT:
        Requirement.Characteristics |= DMA_TRANSFER_SIZE_8;
        DmaData.Width = 8;
        break;

    case ACPI_SMALL_FIXED_DMA_16BIT:
        Requirement.Characteristics |= DMA_TRANSFER_SIZE_16;
        DmaData.Width = 16;
        break;

    case ACPI_SMALL_FIXED_DMA_32BIT:
        Requirement.Characteristics |= DMA_TRANSFER_SIZE_32;
        DmaData.Width = 32;
        break;

    case ACPI_SMALL_FIXED_DMA_64BIT:
        Requirement.Characteristics |= DMA_TRANSFER_SIZE_64;
        DmaData.Width = 64;
        break;

    case ACPI_SMALL_FIXED_DMA_128BIT:
        Requirement.Characteristics |= DMA_TRANSFER_SIZE_128;
        DmaData.Width = 128;
        break;

    case ACPI_SMALL_FIXED_DMA_256BIT:
        Requirement.Characteristics |= DMA_TRANSFER_SIZE_256;
        DmaData.Width = 256;
        break;

    default:
        Status = STATUS_MALFORMED_DATA_STREAM;
        goto ParseSmallFixedDmaDescriptorEnd;
    }

    Requirement.Flags = RESOURCE_FLAG_NOT_SHAREABLE;
    Requirement.Data = &DmaData;
    Requirement.DataSize = sizeof(RESOURCE_DMA_DATA);

    //
    // Register the requirement.
    //

    Status = IoCreateAndAddResourceRequirement(&Requirement,
                                               RequirementList,
                                               &NewRequirement);

    if (!KSUCCESS(Status)) {
        goto ParseSmallFixedDmaDescriptorEnd;
    }

    Status = STATUS_SUCCESS;

ParseSmallFixedDmaDescriptorEnd:
    if (!KSUCCESS(Status)) {
        if (NewRequirement != NULL) {
            IoRemoveResourceRequirement(NewRequirement);
        }
    }

    return Status;
}

KSTATUS
AcpipParseSmallIrqDescriptor (
    PVOID Buffer,
    ULONG BufferLength,
    PRESOURCE_REQUIREMENT_LIST RequirementList
    )

/*++

Routine Description:

    This routine converts an ACPI small IRQ descriptor into a resource
    requirement, and puts that requirement on the given requirement list.

Arguments:

    Buffer - Supplies a pointer to the IRQ descriptor buffer, pointing after the
        byte identifying the descriptor as a short IRQ descriptor.

    BufferLength - Supplies the length of the descriptor buffer.

    RequirementList - Supplies a pointer to the resource requirement list to
        put the descriptor on.

Return Value:

    Status code.

--*/

{

    PUCHAR BufferPointer;
    UCHAR InterruptOptions;
    PRESOURCE_REQUIREMENT NewRequirement;
    USHORT PicInterrupts;
    RESOURCE_REQUIREMENT Requirement;
    KSTATUS Status;

    NewRequirement = NULL;
    if (BufferLength < 2) {
        Status = STATUS_MALFORMED_DATA_STREAM;
        goto ParseSmallIrqDescriptorEnd;
    }

    RtlZeroMemory(&Requirement, sizeof(RESOURCE_REQUIREMENT));
    Requirement.Type = ResourceTypeInterruptLine;
    BufferPointer = Buffer;
    PicInterrupts = *((PUSHORT)BufferPointer);
    InterruptOptions = 0;
    if (BufferLength >= 3) {
        InterruptOptions = *(BufferPointer + 2);
    }

    //
    // Set the flags and characteristics.
    //

    if ((InterruptOptions & ACPI_SMALL_IRQ_FLAG_SHAREABLE) == 0) {
        Requirement.Flags |= RESOURCE_FLAG_NOT_SHAREABLE;
    }

    if ((InterruptOptions & ACPI_SMALL_IRQ_FLAG_EDGE_TRIGGERED) != 0) {
        Requirement.Characteristics |= INTERRUPT_LINE_EDGE_TRIGGERED;
    }

    if ((InterruptOptions & ACPI_SMALL_IRQ_FLAG_ACTIVE_LOW) != 0) {
        Requirement.Characteristics |= INTERRUPT_LINE_ACTIVE_LOW;

    } else {
        Requirement.Characteristics |= INTERRUPT_LINE_ACTIVE_HIGH;
    }

    //
    // Loop getting runs of set bits.
    //

    Requirement.Length = 1;
    Requirement.Minimum = 0;
    while (PicInterrupts != 0) {

        //
        // Skip over zero bits.
        //

        while ((PicInterrupts != 0) && ((PicInterrupts & 0x1) == 0)) {
            Requirement.Minimum += 1;
            PicInterrupts = PicInterrupts >> 1;
        }

        //
        // Collect one bits.
        //

        Requirement.Maximum = Requirement.Minimum;
        while ((PicInterrupts & 0x1) != 0) {
            Requirement.Maximum += 1;
            PicInterrupts = PicInterrupts >> 1;
        }

        //
        // Bail out if there's nothing there.
        //

        if (Requirement.Minimum == Requirement.Maximum) {
            break;
        }

        //
        // Register the requirement or the alternative.
        //

        if (NewRequirement == NULL) {
            Status = IoCreateAndAddResourceRequirement(&Requirement,
                                                       RequirementList,
                                                       &NewRequirement);

        } else {
            Status = IoCreateAndAddResourceRequirementAlternative(
                                                                &Requirement,
                                                                NewRequirement);
        }

        if (!KSUCCESS(Status)) {
            goto ParseSmallIrqDescriptorEnd;
        }

        Requirement.Minimum = Requirement.Maximum;
    }

    Status = STATUS_SUCCESS;

ParseSmallIrqDescriptorEnd:
    if (!KSUCCESS(Status)) {
        if (NewRequirement != NULL) {
            IoRemoveResourceRequirement(NewRequirement);
        }
    }

    return Status;
}

KSTATUS
AcpipParseLargeIrqDescriptor (
    PVOID Buffer,
    ULONG BufferLength,
    PRESOURCE_REQUIREMENT_LIST RequirementList
    )

/*++

Routine Description:

    This routine converts an ACPI large IRQ descriptor into a resource
    requirement, and puts that requirement on the given requirement list.

Arguments:

    Buffer - Supplies a pointer to the IRQ descriptor buffer, pointing after the
        length bytes.

    BufferLength - Supplies the length of the descriptor buffer.

    RequirementList - Supplies a pointer to the resource requirement list to
        put the descriptor on.

Return Value:

    Status code.

--*/

{

    PUCHAR BufferPointer;
    PRESOURCE_REQUIREMENT CreatedRequirement;
    UCHAR InterruptCount;
    UCHAR InterruptOptions;
    PULONG LongPointer;
    RESOURCE_REQUIREMENT Requirement;
    KSTATUS Status;

    BufferPointer = (PUCHAR)Buffer;
    CreatedRequirement = NULL;
    if (BufferLength < 2) {
        Status = STATUS_MALFORMED_DATA_STREAM;
        goto ParseLargeIrqDescriptorEnd;
    }

    RtlZeroMemory(&Requirement, sizeof(RESOURCE_REQUIREMENT));
    Requirement.Type = ResourceTypeInterruptLine;
    Requirement.Length = 1;
    InterruptOptions = *BufferPointer;
    InterruptCount = *(BufferPointer + 1);
    if (BufferLength < 2 + (sizeof(ULONG) * InterruptCount)) {
        Status = STATUS_MALFORMED_DATA_STREAM;
        goto ParseLargeIrqDescriptorEnd;
    }

    //
    // Parse the options.
    //

    if ((InterruptOptions & ACPI_LARGE_IRQ_FLAG_SHAREABLE) == 0) {
        Requirement.Flags |= RESOURCE_FLAG_NOT_SHAREABLE;
    }

    if ((InterruptOptions & ACPI_LARGE_IRQ_FLAG_EDGE_TRIGGERED) != 0) {
        Requirement.Characteristics |= INTERRUPT_LINE_EDGE_TRIGGERED;
    }

    if ((InterruptOptions & ACPI_LARGE_IRQ_FLAG_ACTIVE_LOW) != 0) {
        Requirement.Characteristics |= INTERRUPT_LINE_ACTIVE_LOW;

    } else {
        Requirement.Characteristics |= INTERRUPT_LINE_ACTIVE_HIGH;
    }

    BufferPointer += 2;
    BufferLength -= 2;
    LongPointer = (PULONG)BufferPointer;
    while (InterruptCount != 0) {

        //
        // Create an interrupt line descriptor. Attempt to pack as many
        // sequential GSIs into the descriptor as there are.
        //

        Requirement.Minimum = *LongPointer;
        Requirement.Maximum = Requirement.Minimum + 1;
        LongPointer += 1;
        InterruptCount -= 1;
        while ((InterruptCount != 0) &&
               (*LongPointer == Requirement.Maximum)) {

            LongPointer += 1;
            InterruptCount -= 1;
            Requirement.Maximum += 1;
        }

        //
        // Create the main descriptor if it has not been created yet. If it has,
        // create an alternative.
        //

        if (CreatedRequirement == NULL) {
            Status = IoCreateAndAddResourceRequirement(&Requirement,
                                                       RequirementList,
                                                       &CreatedRequirement);

        } else {
            Status = IoCreateAndAddResourceRequirementAlternative(
                                                           &Requirement,
                                                           CreatedRequirement);
        }

        if (!KSUCCESS(Status)) {
            goto ParseLargeIrqDescriptorEnd;
        }
    }

    Status = STATUS_SUCCESS;

ParseLargeIrqDescriptorEnd:
    return Status;
}

KSTATUS
AcpipParseGpioDescriptor (
    PACPI_OBJECT NamespaceStart,
    PVOID Buffer,
    ULONG BufferLength,
    PRESOURCE_REQUIREMENT_LIST RequirementList
    )

/*++

Routine Description:

    This routine converts an ACPI GPIO resource descriptor into a resource
    requirement, and puts that requirement on the given requirement list.

Arguments:

    NamespaceStart - Supplies a pointer to the starting point for namespace
        traversals.

    Buffer - Supplies a pointer to the GPIO descriptor buffer, pointing after
        the length bytes.

    BufferLength - Supplies the length of the descriptor buffer.

    RequirementList - Supplies a pointer to the resource requirement list to
        put the descriptor on.

Return Value:

    Status code.

--*/

{

    PUCHAR BufferPointer;
    UCHAR ConnectionType;
    PRESOURCE_REQUIREMENT CreatedRequirement;
    USHORT DebounceTimeout;
    PRESOURCE_GPIO_DATA GpioData;
    UINTN GpioDataSize;
    INTERRUPT_CONTROLLER_INFORMATION InterruptController;
    RESOURCE_REQUIREMENT InterruptRequirement;
    USHORT IoFlags;
    USHORT OutputDrive;
    UCHAR PinConfiguration;
    USHORT PinCount;
    PUSHORT PinPointer;
    USHORT PinTableOffset;
    PACPI_OBJECT Provider;
    RESOURCE_REQUIREMENT Requirement;
    USHORT ResourceSourceNameOffset;
    PSTR SourceName;
    KSTATUS Status;
    PVOID VendorData;
    USHORT VendorDataLength;
    USHORT VendorDataOffset;

    BufferPointer = (PUCHAR)Buffer;
    CreatedRequirement = NULL;
    GpioData = NULL;
    if (BufferLength < 0x14) {
        Status = STATUS_MALFORMED_DATA_STREAM;
        goto ParseGpioDescriptorEnd;
    }

    //
    // Check the revision.
    //

    if (*BufferPointer < 1) {
        Status = STATUS_MALFORMED_DATA_STREAM;
        goto ParseGpioDescriptorEnd;
    }

    ConnectionType = *(BufferPointer + 1);
    IoFlags = READ_UNALIGNED16(BufferPointer + 4);
    PinConfiguration = *(BufferPointer + 6);
    OutputDrive = READ_UNALIGNED16(BufferPointer + 7);
    DebounceTimeout = READ_UNALIGNED16(BufferPointer + 9);
    PinTableOffset = READ_UNALIGNED16(BufferPointer + 11);
    ResourceSourceNameOffset = READ_UNALIGNED16(BufferPointer + 14);
    VendorDataOffset = READ_UNALIGNED16(BufferPointer + 15);
    VendorDataLength = READ_UNALIGNED16(BufferPointer + 16);
    VendorData = BufferPointer + VendorDataOffset - 3;
    PinPointer = (PUSHORT)(BufferPointer + PinTableOffset - 3);
    SourceName = (PSTR)(BufferPointer + ResourceSourceNameOffset - 3);
    PinCount = (ResourceSourceNameOffset - PinTableOffset) / 2;
    RtlZeroMemory(&Requirement, sizeof(RESOURCE_REQUIREMENT));
    RtlZeroMemory(&InterruptRequirement, sizeof(RESOURCE_REQUIREMENT));
    GpioDataSize = sizeof(RESOURCE_GPIO_DATA) + VendorDataLength;
    GpioData = MmAllocatePagedPool(GpioDataSize, ACPI_RESOURCE_ALLOCATION_TAG);
    if (GpioData == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ParseGpioDescriptorEnd;
    }

    RtlZeroMemory(GpioData, GpioDataSize);
    GpioData->Version = RESOURCE_GPIO_DATA_VERSION;

    //
    // Set the secondary flag on the interrupt requirement since this is a GPIO
    // line interrupt, not a primary interrupt controller interrupt. This flag
    // indicates that the interrupt vector to run-level correlation may not
    // be there.
    //

    InterruptRequirement.Type = ResourceTypeInterruptLine;
    InterruptRequirement.Characteristics = INTERRUPT_LINE_SECONDARY;
    Requirement.Type = ResourceTypeGpio;

    //
    // Parse the options.
    //

    if ((IoFlags & ACPI_GPIO_SHARED) == 0) {
        Requirement.Flags |= RESOURCE_FLAG_NOT_SHAREABLE;
    }

    switch (ConnectionType) {
    case ACPI_GPIO_CONNECTION_INTERRUPT:
        GpioData->Flags |= RESOURCE_GPIO_INTERRUPT;
        if ((IoFlags & ACPI_GPIO_WAKE) != 0) {
            GpioData->Flags |= RESOURCE_GPIO_WAKE;
            InterruptRequirement.Characteristics |= INTERRUPT_LINE_WAKE;
        }

        switch (IoFlags & ACPI_GPIO_POLARITY_MASK) {
        case ACPI_GPIO_POLARITY_ACTIVE_HIGH:
            GpioData->Flags |= RESOURCE_GPIO_ACTIVE_HIGH;
            InterruptRequirement.Characteristics |= INTERRUPT_LINE_ACTIVE_HIGH;
            break;

        case ACPI_GPIO_POLARITY_ACTIVE_LOW:
            GpioData->Flags |= RESOURCE_GPIO_ACTIVE_LOW;
            InterruptRequirement.Characteristics |= INTERRUPT_LINE_ACTIVE_LOW;
            break;

        case ACPI_GPIO_POLARITY_ACTIVE_BOTH:
            GpioData->Flags |= RESOURCE_GPIO_ACTIVE_HIGH |
                               RESOURCE_GPIO_ACTIVE_LOW;

            InterruptRequirement.Characteristics |= INTERRUPT_LINE_ACTIVE_HIGH |
                                                    INTERRUPT_LINE_ACTIVE_LOW;

            break;

        default:

            ASSERT(FALSE);

            break;
        }

        if ((IoFlags & ACPI_GPIO_EDGE_TRIGGERED) != 0) {
            GpioData->Flags |= RESOURCE_GPIO_EDGE_TRIGGERED;
            InterruptRequirement.Characteristics |=
                                                 INTERRUPT_LINE_EDGE_TRIGGERED;
        }

        break;

    case ACPI_GPIO_CONNECTION_IO:
        switch (IoFlags & ACPI_GPIO_IO_RESTRICTION_MASK) {
        case ACPI_GPIO_IO_RESTRICTION_IO:
        case ACPI_GPIO_IO_RESTRICTION_IO_PRESERVE:
            GpioData->Flags |= RESOURCE_GPIO_INPUT | RESOURCE_GPIO_OUTPUT;
            break;

        case ACPI_GPIO_IO_RESTRICTION_INPUT:
            GpioData->Flags |= RESOURCE_GPIO_INPUT;
            break;

        case ACPI_GPIO_IO_RESTRICTION_OUTPUT:
            GpioData->Flags |= RESOURCE_GPIO_OUTPUT;
            break;

        default:

            ASSERT(FALSE);

            break;
        }

        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_MALFORMED_DATA_STREAM;
        goto ParseGpioDescriptorEnd;
    }

    switch (PinConfiguration) {
    case ACPI_GPIO_PIN_PULL_DEFAULT:
        break;

    case ACPI_GPIO_PIN_PULL_UP:
        GpioData->Flags |= RESOURCE_GPIO_PULL_UP;
        break;

    case ACPI_GPIO_PIN_PULL_DOWN:
        GpioData->Flags |= RESOURCE_GPIO_PULL_DOWN;
        break;

    case ACPI_GPIO_PIN_PULL_NONE:
        GpioData->Flags |= RESOURCE_GPIO_PULL_NONE;
        break;
    }

    if (OutputDrive == ACPI_GPIO_OUTPUT_DRIVE_DEFAULT) {
        GpioData->OutputDriveStrength = RESOURCE_GPIO_DEFAULT_DRIVE_STRENGTH;

    } else {
        GpioData->OutputDriveStrength = OutputDrive * 10;
    }

    if (DebounceTimeout == ACPI_GPIO_DEBOUNCE_TIMEOUT_DEFAULT) {
        GpioData->DebounceTimeout = RESOURCE_GPIO_DEFAULT_DEBOUNCE_TIMEOUT;

    } else {
        GpioData->DebounceTimeout = DebounceTimeout * 10;
    }

    RtlCopyMemory(GpioData + 1, VendorData, VendorDataLength);
    GpioData->VendorDataOffset = sizeof(RESOURCE_GPIO_DATA);
    GpioData->VendorDataSize = VendorDataLength;
    Requirement.Data = GpioData;
    Requirement.DataSize = GpioDataSize;

    //
    // Find the device providing the GPIO resource.
    //

    Provider = AcpipGetNamespaceObject(SourceName, NamespaceStart);
    if (Provider == NULL) {
        RtlDebugPrint("ACPI: Failed to find GPIO device '%s'\n", SourceName);
        Status = STATUS_INVALID_CONFIGURATION;
        goto ParseGpioDescriptorEnd;
    }

    if (Provider->Type != AcpiObjectDevice) {

        ASSERT(FALSE);

        Status = STATUS_INVALID_CONFIGURATION;
        goto ParseGpioDescriptorEnd;
    }

    //
    // If the GPIO device is not yet started, then fail for now and try again
    // when it's ready.
    //

    if (Provider->U.Device.IsDeviceStarted == FALSE) {
        Status = AcpipCreateDeviceDependency(NamespaceStart->U.Device.OsDevice,
                                             Provider);

        if (Status != STATUS_TOO_LATE) {
            if (KSUCCESS(Status)) {
                Status = STATUS_NOT_READY;
            }

            goto ParseGpioDescriptorEnd;
        }
    }

    Requirement.Provider = Provider->U.Device.OsDevice;

    ASSERT(Requirement.Provider != NULL);

    //
    // Now add resources for each streak of pins defined in the table.
    //

    while (PinCount != 0) {

        //
        // Create a GPIO descriptor. Attempt to pack as many sequential lines
        // into the descriptor as there are.
        //

        Requirement.Minimum = READ_UNALIGNED16(PinPointer);
        Requirement.Maximum = Requirement.Minimum + 1;
        PinPointer += 1;
        PinCount -= 1;
        while ((PinCount != 0) &&
               (READ_UNALIGNED16(PinPointer) == Requirement.Maximum)) {

            PinPointer += 1;
            PinCount -= 1;
            Requirement.Maximum += 1;
        }

        Requirement.Length = Requirement.Maximum - Requirement.Minimum;
        Status = IoCreateAndAddResourceRequirement(&Requirement,
                                                   RequirementList,
                                                   &CreatedRequirement);

        if (!KSUCCESS(Status)) {
            goto ParseGpioDescriptorEnd;
        }

        //
        // Add a standard interrupt line requirement too if this is an
        // interrupt. This way devices do not have to become GPIO-aware to
        // have interrupts serviced via GPIO.
        //

        if ((GpioData->Flags & RESOURCE_GPIO_INTERRUPT) != 0) {
            InterruptRequirement.Length = Requirement.Length;
            InterruptRequirement.Flags = Requirement.Flags;
            InterruptRequirement.Data = Requirement.Data;
            InterruptRequirement.DataSize = Requirement.DataSize;

            //
            // Translate from the provider device back to an interrupt
            // controller, and then to a GSI to determine which interrupt
            // line to connect to. This line will end up being a dynamically
            // allocated GSI. Using the device pointer as the interrupt
            // controller ID is an agreed-upon convention with the GPIO library
            // driver.
            //

            Status = HlGetInterruptControllerInformation(
                                                 (UINTN)(Requirement.Provider),
                                                 &InterruptController);

            if (!KSUCCESS(Status)) {
                RtlDebugPrint("ACPI: Missing interrupt controller\n");
                Status = STATUS_NOT_READY;
                goto ParseGpioDescriptorEnd;
            }

            InterruptRequirement.Minimum = InterruptController.StartingGsi +
                                           Requirement.Minimum;

            InterruptRequirement.Maximum = InterruptController.StartingGsi +
                                           Requirement.Maximum;

            Status = IoCreateAndAddResourceRequirement(&InterruptRequirement,
                                                       RequirementList,
                                                       NULL);

            if (!KSUCCESS(Status)) {
                goto ParseGpioDescriptorEnd;
            }
        }
    }

    Status = STATUS_SUCCESS;

ParseGpioDescriptorEnd:
    if (GpioData != NULL) {
        MmFreePagedPool(GpioData);
    }

    return Status;
}

KSTATUS
AcpipParseSpbDescriptor (
    PACPI_OBJECT NamespaceStart,
    PVOID Buffer,
    ULONG BufferLength,
    PRESOURCE_REQUIREMENT_LIST RequirementList
    )

/*++

Routine Description:

    This routine converts an ACPI Simple Peripheral Bus resource descriptor
    into a resource requirement, and puts that requirement on the given
    requirement list.

Arguments:

    NamespaceStart - Supplies a pointer to the starting point for namespace
        traversals.

    Buffer - Supplies a pointer to the SPB descriptor buffer, pointing after
        the length bytes.

    BufferLength - Supplies the length of the descriptor buffer.

    RequirementList - Supplies a pointer to the resource requirement list to
        put the descriptor on.

Return Value:

    Status code.

--*/

{

    PUCHAR BufferPointer;
    UCHAR BusType;
    UCHAR GeneralFlags;
    RESOURCE_SPB_I2C I2cDescriptor;
    PACPI_OBJECT Provider;
    RESOURCE_REQUIREMENT Requirement;
    PSTR SourceName;
    PVOID SpbData;
    PRESOURCE_SPB_DATA SpbDataSource;
    UINTN SpbDataSourceSize;
    RESOURCE_SPB_SPI SpiDescriptor;
    KSTATUS Status;
    USHORT TypeDataLength;
    USHORT TypeSpecificFlags;
    UCHAR TypeSpecificRevisionId;
    RESOURCE_SPB_UART UartDescriptor;
    PVOID VendorData;
    ULONG VendorDataLength;

    BufferPointer = Buffer;
    SpbData = NULL;
    if (BufferLength < 0x0F) {
        Status = STATUS_MALFORMED_DATA_STREAM;
        goto ParseSpbDescriptorEnd;
    }

    RtlZeroMemory(&Requirement, sizeof(RESOURCE_REQUIREMENT));

    //
    // Check the revision.
    //

    if (*BufferPointer < 1) {
        Status = STATUS_MALFORMED_DATA_STREAM;
        goto ParseSpbDescriptorEnd;
    }

    BusType = *(BufferPointer + 2);
    GeneralFlags = *(BufferPointer + 3);
    TypeSpecificFlags = READ_UNALIGNED16(BufferPointer + 4);
    TypeSpecificRevisionId = *(BufferPointer + 6);
    TypeDataLength = READ_UNALIGNED16(BufferPointer + 7);
    SourceName = (PSTR)(BufferPointer + 9 + TypeDataLength);
    if (BufferLength < 9 + TypeDataLength) {
        Status = STATUS_MALFORMED_DATA_STREAM;
        goto ParseSpbDescriptorEnd;
    }

    switch (BusType) {
    case ACPI_SPB_BUS_I2C:
        VendorData = BufferPointer + 9 + ACPI_SPB_I2C_TYPE_DATA_LENGTH;
        VendorDataLength = TypeDataLength - ACPI_SPB_I2C_TYPE_DATA_LENGTH;
        Status = AcpipParseSpbI2cDescriptor(TypeSpecificFlags,
                                            TypeSpecificRevisionId,
                                            TypeDataLength,
                                            Buffer + 9,
                                            &Requirement,
                                            &I2cDescriptor);

        SpbDataSource = &(I2cDescriptor.Header);
        SpbDataSourceSize = sizeof(RESOURCE_SPB_I2C);
        break;

    case ACPI_SPB_BUS_SPI:
        VendorData = BufferPointer + 9 + ACPI_SPB_SPI_TYPE_DATA_LENGTH;
        VendorDataLength = TypeDataLength - ACPI_SPB_SPI_TYPE_DATA_LENGTH;
        Status = AcpipParseSpbSpiDescriptor(TypeSpecificFlags,
                                            TypeSpecificRevisionId,
                                            TypeDataLength,
                                            Buffer + 9,
                                            &Requirement,
                                            &SpiDescriptor);

        SpbDataSource = &(SpiDescriptor.Header);
        SpbDataSourceSize = sizeof(RESOURCE_SPB_SPI);
        break;

    case ACPI_SPB_BUS_UART:
        VendorData = BufferPointer + 9 + ACPI_SPB_UART_TYPE_DATA_LENGTH;
        VendorDataLength = TypeDataLength - ACPI_SPB_UART_TYPE_DATA_LENGTH;
        Status = AcpipParseSpbUartDescriptor(TypeSpecificFlags,
                                             TypeSpecificRevisionId,
                                             TypeDataLength,
                                             Buffer + 9,
                                             &UartDescriptor);

        SpbDataSource = &(UartDescriptor.Header);
        SpbDataSourceSize = sizeof(RESOURCE_SPB_UART);
        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_MALFORMED_DATA_STREAM;
        break;
    }

    if (!KSUCCESS(Status)) {
        goto ParseSpbDescriptorEnd;
    }

    //
    // Parse the general flags, which are not specific to any bus type.
    //

    if ((GeneralFlags & ACPI_SPB_FLAG_SLAVE) != 0) {
        SpbDataSource->Flags |= RESOURCE_SPB_DATA_SLAVE;
    }

    SpbDataSource->VendorDataOffset = SpbDataSourceSize;
    SpbDataSource->VendorDataSize = VendorDataLength;

    //
    // Find the device providing the resource.
    //

    Provider = AcpipGetNamespaceObject(SourceName, NamespaceStart);
    if (Provider == NULL) {
        RtlDebugPrint("ACPI: Failed to find SPB device '%s'\n", SourceName);
        Status = STATUS_INVALID_CONFIGURATION;
        goto ParseSpbDescriptorEnd;
    }

    if (Provider->Type != AcpiObjectDevice) {

        ASSERT(FALSE);

        Status = STATUS_INVALID_CONFIGURATION;
        goto ParseSpbDescriptorEnd;
    }

    //
    // If the SPB device is not yet started, then fail for now and try again
    // when it's ready.
    //

    if (Provider->U.Device.IsDeviceStarted == FALSE) {
        Status = AcpipCreateDeviceDependency(NamespaceStart->U.Device.OsDevice,
                                             Provider);

        if (Status != STATUS_TOO_LATE) {
            if (KSUCCESS(Status)) {
                Status = STATUS_NOT_READY;
            }

            goto ParseSpbDescriptorEnd;
        }
    }

    Requirement.Type = ResourceTypeSimpleBus;
    Requirement.Provider = Provider->U.Device.OsDevice;

    ASSERT(Requirement.Provider != NULL);

    SpbData = MmAllocatePagedPool(SpbDataSourceSize + VendorDataLength,
                                  ACPI_RESOURCE_ALLOCATION_TAG);

    if (SpbData == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ParseSpbDescriptorEnd;
    }

    RtlCopyMemory(SpbData, SpbDataSource, SpbDataSourceSize);
    if (VendorDataLength != 0) {
        RtlCopyMemory(SpbData + SpbDataSourceSize,
                      VendorData,
                      VendorDataLength);
    }

    Requirement.Data = SpbData;
    Requirement.DataSize = SpbDataSourceSize + VendorDataLength;
    Status = IoCreateAndAddResourceRequirement(&Requirement,
                                               RequirementList,
                                               NULL);

    if (!KSUCCESS(Status)) {
        goto ParseSpbDescriptorEnd;
    }

    Status = STATUS_SUCCESS;

ParseSpbDescriptorEnd:
    if (SpbData != NULL) {
        MmFreePagedPool(SpbData);
    }

    return Status;
}

KSTATUS
AcpipParseSpbI2cDescriptor (
    USHORT TypeSpecificFlags,
    UCHAR TypeSpecificRevisionId,
    USHORT TypeDataLength,
    PUCHAR Buffer,
    PRESOURCE_REQUIREMENT Requirement,
    PRESOURCE_SPB_I2C Descriptor
    )

/*++

Routine Description:

    This routine parses the bus type specific contents of an I2C resource
    descriptor.

Arguments:

    TypeSpecificFlags - Supplies the type specific flags.

    TypeSpecificRevisionId - Supplies the type specific revision identifier.

    TypeDataLength - Supplies the type data length.

    Buffer - Supplies a pointer to the descriptor buffer, pointing to the type
        specific data.

    Requirement - Supplies a pointer to the resource requirement.

    Descriptor - Supplies a pointer where the descriptor will be returned on
        success.

Return Value:

    Status code.

--*/

{

    //
    // Check the revision.
    //

    if ((TypeSpecificRevisionId < 1) ||
        (TypeDataLength < ACPI_SPB_I2C_TYPE_DATA_LENGTH)) {

        return STATUS_MALFORMED_DATA_STREAM;
    }

    RtlZeroMemory(Descriptor, sizeof(RESOURCE_SPB_I2C));
    Descriptor->Header.Version = RESOURCE_SPB_DATA_VERSION;
    Descriptor->Header.Size = sizeof(RESOURCE_SPB_I2C);
    Descriptor->Header.BusType = ResourceSpbBusI2c;
    Descriptor->Speed = READ_UNALIGNED32(Buffer);
    Descriptor->SlaveAddress = READ_UNALIGNED16(Buffer + 4);
    if ((TypeSpecificFlags & ACPI_SPB_I2C_10_BIT_ADDRESSING) != 0) {
        Descriptor->Flags |= RESOURCE_SPB_I2C_10_BIT_ADDRESSING;
    }

    Requirement->Minimum = Descriptor->SlaveAddress;
    Requirement->Maximum = Requirement->Minimum + 1;
    Requirement->Length = 1;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipParseSpbSpiDescriptor (
    USHORT TypeSpecificFlags,
    UCHAR TypeSpecificRevisionId,
    USHORT TypeDataLength,
    PUCHAR Buffer,
    PRESOURCE_REQUIREMENT Requirement,
    PRESOURCE_SPB_SPI Descriptor
    )

/*++

Routine Description:

    This routine parses the bus type specific contents of an SPI resource
    descriptor.

Arguments:

    TypeSpecificFlags - Supplies the type specific flags.

    TypeSpecificRevisionId - Supplies the type specific revision identifier.

    TypeDataLength - Supplies the type data length.

    Buffer - Supplies a pointer to the descriptor buffer, pointing to the type
        specific data.

    Requirement - Supplies a pointer to the resource requirement.

    Descriptor - Supplies a pointer where the descriptor will be returned on
        success.

Return Value:

    Status code.

--*/

{

    UCHAR Phase;
    UCHAR Polarity;

    //
    // Check the revision.
    //

    if ((TypeSpecificRevisionId < 1) ||
        (TypeDataLength < ACPI_SPB_SPI_TYPE_DATA_LENGTH)) {

        return STATUS_MALFORMED_DATA_STREAM;
    }

    RtlZeroMemory(Descriptor, sizeof(RESOURCE_SPB_SPI));
    Descriptor->Header.Version = RESOURCE_SPB_DATA_VERSION;
    Descriptor->Header.Size = sizeof(RESOURCE_SPB_SPI);
    Descriptor->Header.BusType = ResourceSpbBusSpi;
    Descriptor->Speed = READ_UNALIGNED32(Buffer);
    Descriptor->WordSize = *(Buffer + 4);
    Phase = *(Buffer + 5);
    Polarity = *(Buffer + 6);
    Descriptor->DeviceSelect = READ_UNALIGNED16(Buffer + 7);
    if (Phase == ACPI_SPB_SPI_PHASE_SECOND) {
        Descriptor->Flags |= RESOURCE_SPB_SPI_SECOND_PHASE;
    }

    if (Polarity == ACPI_SPB_SPI_POLARITY_START_HIGH) {
        Descriptor->Flags |= RESOURCE_SPB_SPI_START_HIGH;
    }

    if ((TypeSpecificFlags & ACPI_SPB_SPI_3_WIRES) != 0) {
        Descriptor->Flags |= RESOURCE_SPB_SPI_3_WIRES;
    }

    if ((TypeSpecificFlags & ACPI_SPB_SPI_DEVICE_SELECT_ACTIVE_HIGH) != 0) {
        Descriptor->Flags |= RESOURCE_SPB_SPI_DEVICE_SELECT_ACTIVE_HIGH;
    }

    if (Descriptor->DeviceSelect != 0) {
        Requirement->Minimum =
                             RtlCountTrailingZeros32(Descriptor->DeviceSelect);

        Requirement->Maximum = Requirement->Minimum + 1;
        Requirement->Length = 1;
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipParseSpbUartDescriptor (
    USHORT TypeSpecificFlags,
    UCHAR TypeSpecificRevisionId,
    USHORT TypeDataLength,
    PUCHAR Buffer,
    PRESOURCE_SPB_UART Descriptor
    )

/*++

Routine Description:

    This routine parses the bus type specific contents of a UART resource
    descriptor.

Arguments:

    TypeSpecificFlags - Supplies the type specific flags.

    TypeSpecificRevisionId - Supplies the type specific revision identifier.

    TypeDataLength - Supplies the type data length.

    Buffer - Supplies a pointer to the descriptor buffer, pointing to the type
        specific data.

    Descriptor - Supplies a pointer where the descriptor will be returned on
        success.

Return Value:

    Status code.

--*/

{

    UCHAR Parity;

    //
    // Check the revision.
    //

    if ((TypeSpecificRevisionId < 1) ||
        (TypeDataLength < ACPI_SPB_UART_TYPE_DATA_LENGTH)) {

        return STATUS_MALFORMED_DATA_STREAM;
    }

    RtlZeroMemory(Descriptor, sizeof(RESOURCE_SPB_UART));
    Descriptor->Header.Version = RESOURCE_SPB_DATA_VERSION;
    Descriptor->Header.Size = sizeof(RESOURCE_SPB_UART);
    Descriptor->Header.BusType = ResourceSpbBusUart;
    Descriptor->BaudRate = READ_UNALIGNED32(Buffer);
    Descriptor->RxFifoSize = READ_UNALIGNED16(Buffer + 4);
    Descriptor->TxFifoSize = READ_UNALIGNED16(Buffer + 6);
    Parity = *(Buffer + 8);

    //
    // The ACPI control line definitions happen to match up to the OS
    // definitions.
    //

    ASSERT_SPB_UART_CONTROL_LINES_EQUIVALENT();

    Descriptor->ControlLines = *(Buffer + 9);
    switch (Parity) {
    case ACPI_SPB_UART_PARITY_NONE:
        break;

    case ACPI_SPB_UART_PARITY_EVEN:
        Descriptor->Flags |= RESOURCE_SPB_UART_PARITY_EVEN;
        break;

    case ACPI_SPB_UART_PARITY_ODD:
        Descriptor->Flags |= RESOURCE_SPB_UART_PARITY_ODD;
        break;

    case ACPI_SPB_UART_PARITY_MARK:
        Descriptor->Flags |= RESOURCE_SPB_UART_PARITY_MARK;
        break;

    case ACPI_SPB_UART_PARITY_SPACE:
        Descriptor->Flags |= RESOURCE_SPB_UART_PARITY_SPACE;
        break;

    default:

        ASSERT(FALSE);

        break;
    }

    if ((TypeSpecificFlags & ACPI_SPB_UART_BIG_ENDIAN) != 0) {
        Descriptor->Flags |= RESOURCE_SPB_UART_BIG_ENDIAN;
    }

    switch (TypeSpecificFlags & ACPI_SPB_UART_FLOW_CONTROL_MASK) {
    case ACPI_SPB_UART_FLOW_CONTROL_NONE:
        break;

    case ACPI_SPB_UART_FLOW_CONTROL_HARDWARE:
        Descriptor->Flags |= RESOURCE_SPB_UART_FLOW_CONTROL_HARDWARE;
        break;

    case ACPI_SPB_UART_FLOW_CONTROL_SOFTWARE:
        Descriptor->Flags |= RESOURCE_SPB_UART_FLOW_CONTROL_SOFTWARE;
        break;

    default:

        ASSERT(FALSE);

        break;
    }

    switch (TypeSpecificFlags & ACPI_SPB_UART_STOP_BITS_MASK) {
    case ACPI_SPB_UART_STOP_BITS_NONE:
        Descriptor->Flags |= RESOURCE_SPB_UART_STOP_BITS_NONE;
        break;

    case ACPI_SPB_UART_STOP_BITS_1:
        Descriptor->Flags |= RESOURCE_SPB_UART_STOP_BITS_1;
        break;

    case ACPI_SPB_UART_STOP_BITS_1_5:
        Descriptor->Flags |= RESOURCE_SPB_UART_STOP_BITS_1_5;
        break;

    case ACPI_SPB_UART_STOP_BITS_2:
        Descriptor->Flags |= RESOURCE_SPB_UART_STOP_BITS_2;
        break;

    default:

        ASSERT(FALSE);

        break;
    }

    //
    // The data bits values just go 5-9 in increasing order, so just use that
    // value directly (with an offset of 5 of course).
    //

    Descriptor->DataBits = ((TypeSpecificFlags &
                             ACPI_SPB_UART_DATA_BITS_MASK) >>
                            ACPI_SPB_UART_DATA_BITS_SHIFT) + 5;

    return STATUS_SUCCESS;
}

KSTATUS
AcpipParseLargeVendorDescriptor (
    PVOID Buffer,
    ULONG BufferLength,
    PRESOURCE_REQUIREMENT_LIST RequirementList
    )

/*++

Routine Description:

    This routine converts an ACPI large vendor-defined descriptor into a
    resource requirement, and puts that requirement on the given requirement
    list.

Arguments:

    Buffer - Supplies a pointer to the IRQ descriptor buffer, pointing after the
        length bytes.

    BufferLength - Supplies the length of the descriptor buffer.

    RequirementList - Supplies a pointer to the resource requirement list to
        put the descriptor on.

Return Value:

    Status code.

--*/

{

    PRESOURCE_REQUIREMENT CreatedRequirement;
    RESOURCE_REQUIREMENT Requirement;
    KSTATUS Status;

    CreatedRequirement = NULL;
    RtlZeroMemory(&Requirement, sizeof(RESOURCE_REQUIREMENT));
    Requirement.Type = ResourceTypeVendorSpecific;
    Requirement.Data = Buffer;
    Requirement.DataSize = BufferLength;
    Status = IoCreateAndAddResourceRequirement(&Requirement,
                                               RequirementList,
                                               &CreatedRequirement);

    return Status;
}

