/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    msi.c

Abstract:

    This module implements support for PCI message signaled interrupts.

Author:

    Chris Stevens 9-Jul-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "pci.h"
#include <minoca/kernel/acpi.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define offset values for PCI MSI configuration space.
//

#define PCI_MSI_CONTROL_OFFSET 0x0
#define PCI_MSI_CONTROL_MASK   0xFFFF0000
#define PCI_MSI_CONTROL_SHIFT  16
#define PCI_MSI_LOWER_ADDRESS_OFFSET 0x04
#define PCI_MSI_MASK_OFFSET 0x0C
#define PCI_MSI_64_BIT_MASK_OFFSET 0x10
#define PCI_MSI_PENDING_OFFSET 0x10
#define PCI_MSI_64_BIT_PENDING_OFFSET 0x14

//
// Define the PCI MSI message control register bits.
//

#define PCI_MSI_CONTROL_ENABLE                     0x0001
#define PCI_MSI_CONTROL_MULTI_VECTOR_CAPABLE_MASK  0x000E
#define PCI_MSI_CONTROL_MULTI_VECTOR_CAPABLE_SHIFT 1
#define PCI_MSI_CONTROL_MULTI_VECTOR_ENABLE_MASK   0x0070
#define PCI_MSI_CONTROL_MULTI_VECTOR_ENABLE_SHIFT  4
#define PCI_MSI_CONTROL_64_BIT_CAPABLE             0x0080
#define PCI_MSI_CONTROL_VECTOR_MASKING             0x0100

#define PCI_MSI_MAXIMUM_VECTOR_ENCODING 5

//
// Define offset values for PCI MSI-X configuration space.
//

#define PCI_MSI_X_CONTROL_OFFSET 0x0
#define PCI_MSI_X_CONTROL_MASK 0xFFFF0000
#define PCI_MSI_X_CONTROL_SHIFT 16
#define PCI_MSI_X_TABLE_DATA_OFFSET 0x4
#define PCI_MSI_X_PENDING_ARRAY_DATA_OFFSET 0x8

//
// Define the PCI MSI-X message control register bits.
//

#define PCI_MSI_X_CONTROL_TABLE_SIZE_MASK  0x7FF
#define PCI_MSI_X_CONTROL_TABLE_SIZE_SHIFT 0
#define PCI_MSI_X_CONTROL_GLOBAL_MASK      0x4000
#define PCI_MSI_X_CONTROL_ENABLE           0x8000

//
// Define the PCI MSI-X table data register bits.
//

#define PCI_MSI_X_TABLE_BAR_INDEX_MASK 0x00000007
#define PCI_MSI_X_TABLE_OFFSET_MASK    0xFFFFFFF8

//
// Define the PCI MSI-X pending array data register bits.
//

#define PCI_MSI_X_PENDING_ARRAY_BAR_INDEX_MASK 0x00000007
#define PCI_MSI_X_PENDING_ARRAY_OFFSET_MASK    0xFFFFFFF8

//
// Define the PCI MSI-X vector control bits.
//

#define PCI_MSI_X_VECTOR_CONTROL_MASKED 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

#pragma pack(push, 1)

typedef struct _PCI_MSI_X_TABLE_ENTRY {
    ULONGLONG Address;
    ULONG Data;
    ULONG Control;
} PACKED PCI_MSI_X_TABLE_ENTRY, *PPCI_MSI_X_TABLE_ENTRY;

#pragma pack(pop)

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
PcipMsiGetSetInformation (
    PVOID DeviceToken,
    PPCI_MSI_INFORMATION Information,
    BOOL Set
    );

KSTATUS
PcipMsiSetVectors (
    PVOID DeviceToken,
    PCI_MSI_TYPE MsiType,
    ULONGLONG Vector,
    ULONGLONG VectorIndex,
    ULONGLONG VectorCount,
    PPROCESSOR_SET Processors
    );

KSTATUS
PcipMsiMaskVectors (
    PVOID DeviceToken,
    PCI_MSI_TYPE MsiType,
    ULONGLONG VectorIndex,
    ULONGLONG VectorCount,
    BOOL MaskVectors
    );

KSTATUS
PcipMsiIsVectorMasked (
    PVOID DeviceToken,
    PCI_MSI_TYPE MsiType,
    ULONGLONG VectorIndex,
    PBOOL Masked
    );

KSTATUS
PcipMsiIsVectorPending (
    PVOID DeviceToken,
    PCI_MSI_TYPE MsiType,
    ULONGLONG VectorIndex,
    PBOOL Pending
    );

KSTATUS
PcipMapMsiXTable (
    PPCI_MSI_CONTEXT MsiContext
    );

KSTATUS
PcipMapMsiXPendingArray (
    PPCI_MSI_CONTEXT MsiContext
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
PcipMsiCreateContextAndInterface (
    PDEVICE Device,
    PPCI_DEVICE PciDevice
    )

/*++

Routine Description:

    This routine initializes the MSI/MSI-X context and interface for the given
    PCI device.

Arguments:

    Device - Supplies a pointer to the device in need of an MSI/MSI-X context
        and interface.

    PciDevice - Supplies a pointer to the PCI device context.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    UCHAR CapabilitiesListOffset;
    ULONG CapabilitiesPointerOffset;
    UCHAR Capability;
    ULONG Control;
    PFADT Fadt;
    ULONG HeaderType;
    USHORT ListEntry;
    PPCI_MSI_CONTEXT MsiContext;
    ULONG MsiFlags;
    PINTERFACE_PCI_MSI MsiInterface;
    ULONGLONG MsiMaxVectorCount;
    UCHAR MsiOffset;
    ULONGLONG MsiXMaxVectorCount;
    UCHAR MsiXOffset;
    UCHAR NextOffset;
    ULONG Offset;
    ULONG PciStatus;
    KSTATUS Status;

    MsiContext = NULL;

    //
    // If there is no capabilities list then there is definitely no MSI or
    // MSI-X interface.
    //

    PciStatus = (ULONG)PciDevice->ReadConfig(PciDevice->BusNumber,
                                             PciDevice->DeviceNumber,
                                             PciDevice->FunctionNumber,
                                             PCI_STATUS_OFFSET,
                                             sizeof(ULONG));

    PciStatus = (PciStatus & PCI_STATUS_MASK) >> PCI_STATUS_SHIFT;
    if ((PciStatus & PCI_STATUS_CAPABILITIES_LIST) == 0) {
        Status = STATUS_SUCCESS;
        goto MsiCreateInterfaceEnd;
    }

    //
    // Get the header type to determine the offset of the capabilities pointer.
    //

    HeaderType = (ULONG)PciDevice->ReadConfig(PciDevice->BusNumber,
                                              PciDevice->DeviceNumber,
                                              PciDevice->FunctionNumber,
                                              PCI_HEADER_TYPE_OFFSET,
                                              sizeof(ULONG));

    HeaderType = (HeaderType & PCI_HEADER_TYPE_MASK) >> PCI_HEADER_TYPE_SHIFT;
    HeaderType &= PCI_HEADER_TYPE_VALUE_MASK;
    if (HeaderType == PCI_HEADER_TYPE_CARDBUS_BRIDGE) {
        CapabilitiesPointerOffset = PCI_ALTERNATE_CAPABILITIES_POINTER_OFFSET;

    } else {
        CapabilitiesPointerOffset = PCI_DEFAULT_CAPABILITIES_POINTER_OFFSET;
    }

    //
    // Read the capabilities pointer offset to get the start of the
    // capabilities list.
    //

    CapabilitiesListOffset = (UCHAR)PciDevice->ReadConfig(
                                                     PciDevice->BusNumber,
                                                     PciDevice->DeviceNumber,
                                                     PciDevice->FunctionNumber,
                                                     CapabilitiesPointerOffset,
                                                     sizeof(UCHAR));

    ASSERT((CapabilitiesListOffset == 0) ||
           (CapabilitiesListOffset > PCI_INTERRUPT_LINE_OFFSET));

    //
    // Loop through the capabilities list searching for the MSI and MSI-X
    // capabilities. They should only ever appear once in the list.
    //

    MsiOffset = 0;
    MsiXOffset = 0;
    NextOffset = CapabilitiesListOffset & PCI_CAPABILITY_POINTER_MASK;
    while (NextOffset != 0) {
        ListEntry = (USHORT)PciDevice->ReadConfig(PciDevice->BusNumber,
                                                  PciDevice->DeviceNumber,
                                                  PciDevice->FunctionNumber,
                                                  NextOffset,
                                                  sizeof(USHORT));

        //
        // If this list entry is for either of the desired MSI/MSI-X
        // capabilities then record the offset.
        //

        Capability = (ListEntry & PCI_CAPABILITY_LIST_ID_MASK) >>
                     PCI_CAPABILITY_LIST_ID_SHIFT;

        if (Capability == PCI_CAPABILITY_MSI) {

            ASSERT(MsiOffset == 0);

            MsiOffset = NextOffset;

            //
            // Stop if both have now been found.
            //

            if (MsiXOffset != 0) {
                break;
            }

        } else if (Capability == PCI_CAPABILITY_MSI_X) {

            ASSERT(MsiXOffset == 0);

            MsiXOffset = NextOffset;

            //
            // Stop if both have now been found.
            //

            if (MsiOffset != 0) {
                break;
            }
        }

        //
        // Get the offset of the next capability.
        //

        NextOffset = (ListEntry & PCI_CAPABILITY_LIST_NEXT_POINTER_MASK) >>
                     PCI_CAPABILITY_LIST_NEXT_POINTER_SHIFT;

        NextOffset &= PCI_CAPABILITY_POINTER_MASK;
    }

    //
    // If neither of the capabilities exist, then bail out.
    //

    if ((MsiOffset == 0) && (MsiXOffset == 0)) {
        Status = STATUS_SUCCESS;
        goto MsiCreateInterfaceEnd;
    }

    //
    // TODO: Test to see if the device tree has MSI/MSI-X support.
    //

    Fadt = AcpiFindTable(FADT_SIGNATURE, NULL);
    if ((Fadt == NULL) ||
        ((Fadt->IaBootFlags & FADT_IA_FLAG_MSI_NOT_SUPPORTED) != 0)) {

        Status = STATUS_SUCCESS;
        goto MsiCreateInterfaceEnd;
    }

    //
    // Save the read-only information from the MSI configuration space.
    //

    MsiFlags = 0;
    MsiMaxVectorCount = 0;
    if (MsiOffset != 0) {
        Offset = MsiOffset + PCI_MSI_CONTROL_OFFSET;
        Control = (ULONG)PciDevice->ReadConfig(PciDevice->BusNumber,
                                               PciDevice->DeviceNumber,
                                               PciDevice->FunctionNumber,
                                               Offset,
                                               sizeof(ULONG));

        Control = (Control & PCI_MSI_CONTROL_MASK) >> PCI_MSI_CONTROL_SHIFT;
        if ((Control & PCI_MSI_CONTROL_64_BIT_CAPABLE) != 0) {
            MsiFlags |= PCI_MSI_FLAG_64_BIT_CAPABLE;
        }

        if ((Control & PCI_MSI_CONTROL_VECTOR_MASKING) != 0) {
            MsiFlags |= PCI_MSI_FLAG_MASKABLE;
        }

        MsiMaxVectorCount = (Control &
                             PCI_MSI_CONTROL_MULTI_VECTOR_CAPABLE_MASK) >>
                            PCI_MSI_CONTROL_MULTI_VECTOR_CAPABLE_SHIFT;

        MsiMaxVectorCount = 1ULL << MsiMaxVectorCount;
    }

    //
    // Save the read-only information from the MSI-X configuration space.
    //

    MsiXMaxVectorCount = 0;
    if (MsiXOffset != 0) {
        Offset = MsiXOffset + PCI_MSI_X_CONTROL_OFFSET;
        Control = (ULONG)PciDevice->ReadConfig(PciDevice->BusNumber,
                                               PciDevice->DeviceNumber,
                                               PciDevice->FunctionNumber,
                                               Offset,
                                               sizeof(ULONG));

        Control = (Control & PCI_MSI_X_CONTROL_MASK) >> PCI_MSI_X_CONTROL_SHIFT;
        MsiXMaxVectorCount = (Control & PCI_MSI_X_CONTROL_TABLE_SIZE_MASK) >>
                             PCI_MSI_X_CONTROL_TABLE_SIZE_SHIFT;

        MsiXMaxVectorCount = MsiXMaxVectorCount + 1;
    }

    //
    // One or both of the MSI and/or MSI-X capabilities exists. Create the
    // context and interface, recording the config space offsets.
    //

    AllocationSize = sizeof(PCI_MSI_CONTEXT) + sizeof(INTERFACE_PCI_MSI);
    MsiContext = MmAllocateNonPagedPool(AllocationSize, PCI_ALLOCATION_TAG);
    if (MsiContext == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto MsiCreateInterfaceEnd;
    }

    RtlZeroMemory(MsiContext, AllocationSize);
    MsiInterface = (PINTERFACE_PCI_MSI)(MsiContext + 1);
    MsiInterface->GetSetInformation = PcipMsiGetSetInformation;
    MsiInterface->SetVectors = PcipMsiSetVectors;
    MsiInterface->MaskVectors = PcipMsiMaskVectors;
    MsiInterface->IsVectorMasked = PcipMsiIsVectorMasked;
    MsiInterface->IsVectorPending = PcipMsiIsVectorPending;
    MsiInterface->DeviceToken = PciDevice;
    MsiContext->MsiOffset = MsiOffset;
    MsiContext->MsiXOffset = MsiXOffset;
    MsiContext->MsiFlags = MsiFlags;
    MsiContext->MsiMaxVectorCount = MsiMaxVectorCount;
    MsiContext->MsiXMaxVectorCount = MsiXMaxVectorCount;
    MsiContext->Interface = MsiInterface;
    PciDevice->MsiContext = MsiContext;
    Status = IoCreateInterface(&PciMessageSignaledInterruptsUuid,
                               Device,
                               MsiInterface,
                               sizeof(INTERFACE_PCI_MSI));

    if (!KSUCCESS(Status)) {
        goto MsiCreateInterfaceEnd;
    }

MsiCreateInterfaceEnd:
    if (!KSUCCESS(Status)) {
        if (MsiContext != NULL) {
            MmFreeNonPagedPool(MsiContext);
        }

        PciDevice->MsiContext = NULL;
    }

    return Status;
}

VOID
PcipMsiDestroyContextAndInterface (
    PDEVICE Device,
    PPCI_DEVICE PciDevice
    )

/*++

Routine Description:

    This routine destroys the given PCI device's MSI context and interface if
    they exist.

Arguments:

    Device - Supplies a pointer to the device whose MSI/MSI-x context and
        interface is to be destroyed.

    PciDevice - Supplies a pointer to the PCI device context.

Return Value:

    None.

--*/

{

    if (PciDevice->MsiContext == NULL) {
        return;
    }

    IoDestroyInterface(&PciMessageSignaledInterruptsUuid,
                       Device,
                       PciDevice->MsiContext->Interface);

    MmFreeNonPagedPool(PciDevice->MsiContext);
    PciDevice->MsiContext = NULL;
    return;
}

VOID
PcipGetMsiXBarInformation (
    PPCI_DEVICE PciDevice,
    PULONG TableBarIndex,
    PULONG TableOffset,
    PULONG PendingArrayBarIndex,
    PULONG PendingArrayOffset
    )

/*++

Routine Description:

    This routine gets the BAR information for the MSI-X table and pending bit
    array out of PCI configuration space.

Arguments:

    PciDevice - Supplies a pointer to the PCI device context.

    TableBarIndex - Supplies a pointer that receives the index of the BAR
        within which the MSI-X table resides.

    TableOffset - Supplies a pointer that receives the offset within the BAR
        of the MSI-X table.

    PendingArrayBarIndex - Supplies a pointer that receives the index of the
        BAR within which the MSI-X pending bit array resides.

    PendingArrayOffset - Supplies a pointer that receives the offset within the
        BAR of the MSI-X pending bit array.

Return Value:

    None.

--*/

{

    PPCI_MSI_CONTEXT MsiContext;
    ULONG Offset;
    ULONG PendingArrayData;
    ULONG TableData;

    ASSERT(PciDevice->MsiContext != NULL);
    ASSERT(PciDevice->MsiContext->MsiXOffset != 0);

    //
    // Get the BAR index and offset for the MSI-X vector table.
    //

    MsiContext = PciDevice->MsiContext;
    Offset = MsiContext->MsiXOffset + PCI_MSI_X_TABLE_DATA_OFFSET;
    TableData = (ULONG)PciDevice->ReadConfig(PciDevice->BusNumber,
                                             PciDevice->DeviceNumber,
                                             PciDevice->FunctionNumber,
                                             Offset,
                                             sizeof(ULONG));

    *TableBarIndex = TableData & PCI_MSI_X_TABLE_BAR_INDEX_MASK;
    *TableOffset = TableData & PCI_MSI_X_TABLE_OFFSET_MASK;

    //
    // Get the BAR index and offset for the MSI-X pending bit array.
    //

    Offset = MsiContext->MsiXOffset + PCI_MSI_X_PENDING_ARRAY_DATA_OFFSET;
    PendingArrayData = (ULONG)PciDevice->ReadConfig(PciDevice->BusNumber,
                                                    PciDevice->DeviceNumber,
                                                    PciDevice->FunctionNumber,
                                                    Offset,
                                                    sizeof(ULONG));

    *PendingArrayBarIndex = PendingArrayData &
                            PCI_MSI_X_PENDING_ARRAY_BAR_INDEX_MASK;

    *PendingArrayOffset = PendingArrayData &
                          PCI_MSI_X_PENDING_ARRAY_OFFSET_MASK;

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
PcipMsiGetSetInformation (
    PVOID DeviceToken,
    PPCI_MSI_INFORMATION Information,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets MSI/MSI-X information for the given PCI device.
    Returned information includes whether or not MSI/MSI-X is enabled, uses
    64-bit addresses, is maskable, etc. Information to set includes enabling
    and disabling MSI/MSI-X, the MSI vector count, and the MSI-X global mask.

Arguments:

    DeviceToken - Supplies the device token supplied when the interface was
        acquired.

    Information - Supplies a pointer to a structure that either receives the
        requested information or contains the information to set. In both
        cases, the caller should specify the structure version and the MSI
        type - basic (MSI) or extended (MSI-X) - before passing it to the
        routine.

    Set - Supplies a boolean indicating whether to get or set the MSI/MSI-X
        information.

Return Value:

    Status code.

--*/

{

    ULONG Control;
    ULONG Flags;
    ULONGLONG MaxVectorCount;
    PPCI_MSI_CONTEXT MsiContext;
    ULONG Offset;
    PPCI_DEVICE PciDevice;
    KSTATUS Status;
    ULONGLONG VectorCount;

    if (Information->Version != PCI_MSI_INTERFACE_INFORMATION_VERSION) {
        return STATUS_INVALID_PARAMETER;
    }

    PciDevice = (PPCI_DEVICE)DeviceToken;
    if (PciDevice->MsiContext == NULL) {
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Get the information for the requested MSI/MSI-X type.
    //

    Status = STATUS_SUCCESS;
    MsiContext = PciDevice->MsiContext;
    if (Set == FALSE) {
        switch (Information->MsiType) {
        case PciMsiTypeBasic:
            if (MsiContext->MsiOffset == 0) {
                Status = STATUS_NOT_SUPPORTED;
                goto MsiGetSetInformationEnd;
            }

            Offset = MsiContext->MsiOffset + PCI_MSI_CONTROL_OFFSET;
            Control = (ULONG)PciDevice->ReadConfig(PciDevice->BusNumber,
                                                   PciDevice->DeviceNumber,
                                                   PciDevice->FunctionNumber,
                                                   Offset,
                                                   sizeof(ULONG));

            ASSERT((UCHAR)Control == PCI_CAPABILITY_MSI);

            Control = (Control & PCI_MSI_CONTROL_MASK) >>
                      PCI_MSI_CONTROL_SHIFT;

            Information->Flags = 0;
            if ((Control & PCI_MSI_CONTROL_ENABLE) != 0) {
                Information->Flags |= PCI_MSI_INTERFACE_FLAG_ENABLED;
            }

            if ((Control & PCI_MSI_CONTROL_64_BIT_CAPABLE) != 0) {
                Information->Flags |= PCI_MSI_INTERFACE_FLAG_64_BIT_CAPABLE;
            }

            if ((Control & PCI_MSI_CONTROL_VECTOR_MASKING) != 0) {
                Information->Flags |= PCI_MSI_INTERFACE_FLAG_MASKABLE;
            }

            MaxVectorCount = (Control &
                              PCI_MSI_CONTROL_MULTI_VECTOR_CAPABLE_MASK) >>
                             PCI_MSI_CONTROL_MULTI_VECTOR_CAPABLE_SHIFT;

            Information->MaxVectorCount = 1ULL << MaxVectorCount;

            ASSERT(MsiContext->MsiMaxVectorCount ==
                   Information->MaxVectorCount);

            VectorCount = (Control &
                           PCI_MSI_CONTROL_MULTI_VECTOR_ENABLE_MASK) >>
                          PCI_MSI_CONTROL_MULTI_VECTOR_ENABLE_SHIFT;

            Information->VectorCount = 1ULL << VectorCount;
            break;

        case PciMsiTypeExtended:
            if (MsiContext->MsiXOffset == 0) {
                Status = STATUS_NOT_SUPPORTED;
                goto MsiGetSetInformationEnd;
            }

            Offset = MsiContext->MsiXOffset + PCI_MSI_X_CONTROL_OFFSET;
            Control = (ULONG)PciDevice->ReadConfig(PciDevice->BusNumber,
                                                   PciDevice->DeviceNumber,
                                                   PciDevice->FunctionNumber,
                                                   Offset,
                                                   sizeof(ULONG));

            ASSERT((UCHAR)Control == PCI_CAPABILITY_MSI_X);

            Control = (Control & PCI_MSI_X_CONTROL_MASK) >>
                      PCI_MSI_X_CONTROL_SHIFT;

            Information->Flags = PCI_MSI_INTERFACE_FLAG_64_BIT_CAPABLE |
                                 PCI_MSI_INTERFACE_FLAG_MASKABLE;

            if ((Control & PCI_MSI_X_CONTROL_ENABLE) != 0) {
                Information->Flags |= PCI_MSI_INTERFACE_FLAG_ENABLED;
            }

            if ((Control & PCI_MSI_X_CONTROL_GLOBAL_MASK) != 0) {
                Information->Flags |= PCI_MSI_INTERFACE_FLAG_GLOBAL_MASK;
            }

            MaxVectorCount = (Control &
                              PCI_MSI_X_CONTROL_TABLE_SIZE_MASK) >>
                             PCI_MSI_X_CONTROL_TABLE_SIZE_SHIFT;

            Information->MaxVectorCount = MaxVectorCount + 1;

            ASSERT(MsiContext->MsiXMaxVectorCount ==
                   Information->MaxVectorCount);

            Information->VectorCount = MsiContext->MsiXVectorCount;
            break;

        default:
            Status = STATUS_INVALID_PARAMETER;
            goto MsiGetSetInformationEnd;
        }

    //
    // Set the MSI/MSI-X state based on the supplied information.
    //

    } else {
        switch (Information->MsiType) {
        case PciMsiTypeBasic:
            if (MsiContext->MsiOffset == 0) {
                Status = STATUS_NOT_SUPPORTED;
                goto MsiGetSetInformationEnd;
            }

            //
            // Validate that the supplied vector count is less than the maximum
            // allowed vectors and that it is a power of two.
            //

            if ((Information->VectorCount == 0) ||
                (Information->VectorCount > MsiContext->MsiMaxVectorCount) ||
                (POWER_OF_2(Information->VectorCount) == FALSE)) {

                Status = STATUS_INVALID_PARAMETER;
                goto MsiGetSetInformationEnd;
            }

            //
            // The configuration space encodes the value x where the vector
            // count is 2^x. Make sure that the exponent isn't too large.
            //

            VectorCount = RtlCountTrailingZeros64(Information->VectorCount);
            if (VectorCount > PCI_MSI_MAXIMUM_VECTOR_ENCODING) {
                Status = STATUS_INVALID_PARAMETER;
                goto MsiGetSetInformationEnd;
            }

            //
            // Read the current control information.
            //

            Offset = MsiContext->MsiOffset + PCI_MSI_CONTROL_OFFSET;
            Control = (ULONG)PciDevice->ReadConfig(PciDevice->BusNumber,
                                                   PciDevice->DeviceNumber,
                                                   PciDevice->FunctionNumber,
                                                   Offset,
                                                   sizeof(ULONG));

            ASSERT((UCHAR)Control == PCI_CAPABILITY_MSI);

            Control = (Control & PCI_MSI_CONTROL_MASK) >> PCI_MSI_CONTROL_SHIFT;

            //
            // Update the control status based on the supplied information.
            //

            if ((Information->Flags & PCI_MSI_INTERFACE_FLAG_ENABLED) != 0) {
                Control |= PCI_MSI_CONTROL_ENABLE;

            } else {
                Control &= ~PCI_MSI_CONTROL_ENABLE;
            }

            MaxVectorCount = (Control &
                              PCI_MSI_CONTROL_MULTI_VECTOR_CAPABLE_MASK) >>
                             PCI_MSI_CONTROL_MULTI_VECTOR_CAPABLE_SHIFT;

            MaxVectorCount = 1ULL << MaxVectorCount;

            ASSERT(MsiContext->MsiMaxVectorCount == MaxVectorCount);

            Control |= (VectorCount <<
                        PCI_MSI_CONTROL_MULTI_VECTOR_ENABLE_SHIFT) &
                       PCI_MSI_CONTROL_MULTI_VECTOR_ENABLE_MASK;

            //
            // Write out the updated control information. There shouldn't be
            // a need to preserve the read-only capability ID and next pointer
            // offset, so just shift the control data and write it out.
            //

            Control <<= PCI_MSI_CONTROL_SHIFT;
            PciDevice->WriteConfig(PciDevice->BusNumber,
                                   PciDevice->DeviceNumber,
                                   PciDevice->FunctionNumber,
                                   Offset,
                                   sizeof(ULONG),
                                   Control);

            break;

        case PciMsiTypeExtended:
            if (MsiContext->MsiXOffset == 0) {
                Status = STATUS_NOT_SUPPORTED;
                goto MsiGetSetInformationEnd;
            }

            //
            // Read the current control information.
            //

            Offset = MsiContext->MsiXOffset + PCI_MSI_X_CONTROL_OFFSET;
            Control = (ULONG)PciDevice->ReadConfig(PciDevice->BusNumber,
                                                   PciDevice->DeviceNumber,
                                                   PciDevice->FunctionNumber,
                                                   Offset,
                                                   sizeof(ULONG));

            ASSERT((UCHAR)Control == PCI_CAPABILITY_MSI_X);

            Control = (Control & PCI_MSI_X_CONTROL_MASK) >>
                      PCI_MSI_X_CONTROL_SHIFT;

            //
            // Update the control status based on the supplied information.
            //

            Flags = Information->Flags;
            if ((Flags & PCI_MSI_INTERFACE_FLAG_ENABLED) != 0) {
                Control |= PCI_MSI_X_CONTROL_ENABLE;

            } else {
                Control &= ~PCI_MSI_X_CONTROL_ENABLE;
            }

            if ((Flags & PCI_MSI_INTERFACE_FLAG_GLOBAL_MASK) != 0) {
                Control |= PCI_MSI_X_CONTROL_GLOBAL_MASK;

            } else {
                Control &= ~PCI_MSI_X_CONTROL_GLOBAL_MASK;
            }

            //
            // Write out the updated control information. There shouldn't be
            // a need to preserve the read-only capability ID and next pointer
            // offset, so just shift the control data and write it out.
            //

            Control <<= PCI_MSI_X_CONTROL_SHIFT;
            PciDevice->WriteConfig(PciDevice->BusNumber,
                                   PciDevice->DeviceNumber,
                                   PciDevice->FunctionNumber,
                                   Offset,
                                   sizeof(ULONG),
                                   Control);

            break;

        default:
            Status = STATUS_INVALID_PARAMETER;
            goto MsiGetSetInformationEnd;
        }

        //
        // If the above set enabled MSI/MSI-X, then disable legacy interrupts.
        //

        if ((Information->Flags & PCI_MSI_INTERFACE_FLAG_ENABLED) != 0) {
            Control = (USHORT)PciDevice->ReadConfig(PciDevice->BusNumber,
                                                   PciDevice->DeviceNumber,
                                                   PciDevice->FunctionNumber,
                                                   PCI_CONTROL_OFFSET,
                                                   sizeof(USHORT));

            Control |= PCI_CONTROL_INTERRUPT_DISABLE;
            PciDevice->WriteConfig(PciDevice->BusNumber,
                                   PciDevice->DeviceNumber,
                                   PciDevice->FunctionNumber,
                                   PCI_CONTROL_OFFSET,
                                   sizeof(USHORT),
                                   Control);
        }
    }

MsiGetSetInformationEnd:
    return Status;
}

KSTATUS
PcipMsiSetVectors (
    PVOID DeviceToken,
    PCI_MSI_TYPE MsiType,
    ULONGLONG Vector,
    ULONGLONG VectorIndex,
    ULONGLONG VectorCount,
    PPROCESSOR_SET Processors
    )

/*++

Routine Description:

    This routine sets the address and data for the given contiguous MSI/MSI-X
    vectors.

Arguments:

    DeviceToken - Supplies the device token supplied when the interface was
        acquired.

    MsiType - Supplies the type of the MSI vector.

    Vector - Supplies the starting vector to be set at the given vector index.

    VectorIndex - Supplies the index into the vector table where this vector
        information should be written. This is only valid for MSI-X.

    VectorCount - Supplies the number of contiguous vectors to set starting at
        the given vector and index.

    Processors - Supplies the set of processors that the MSIs should utilize.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    ULONGLONG Index;
    PMSI_INFORMATION Information;
    MSI_INFORMATION InformationBuffer;
    ULONGLONG MessageData;
    PPCI_MSI_CONTEXT MsiContext;
    ULONG Offset;
    PPCI_DEVICE PciDevice;
    PHYSICAL_ADDRESS PhysicalAddress;
    KSTATUS Status;
    PPCI_MSI_X_TABLE_ENTRY TableEntry;

    Information = NULL;
    PciDevice = (PPCI_DEVICE)DeviceToken;
    MsiContext = PciDevice->MsiContext;

    //
    // Bail out immediately if an unsupported MSI type was requested.
    //

    if ((MsiContext == NULL) ||
        ((MsiType == PciMsiTypeBasic) && (MsiContext->MsiOffset == 0)) ||
        ((MsiType == PciMsiTypeExtended) && (MsiContext->MsiXOffset == 0)) ||
        ((MsiType != PciMsiTypeBasic) && (MsiType != PciMsiTypeExtended))) {

        return STATUS_NOT_SUPPORTED;
    }

    //
    // If no vector count was supplied, then it was likely by mistake.
    //

    if (VectorCount == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Validate the index and count based on the cached maximum vector count.
    //

    switch (MsiType) {
    case PciMsiTypeBasic:
        if ((VectorIndex + VectorCount) > MsiContext->MsiMaxVectorCount) {
            Status = STATUS_OUT_OF_BOUNDS;
            goto MsiSetVectorsEnd;
        }

        //
        // Truncate the vector count to 1 as MSI only has one physical address
        // and message register pair. Multiple vectors must be contiguous.
        //

        VectorCount = 1;
        break;

    case PciMsiTypeExtended:
        if ((VectorIndex + VectorCount) > MsiContext->MsiXMaxVectorCount) {
            Status = STATUS_OUT_OF_BOUNDS;
            goto MsiSetVectorsEnd;
        }

        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        goto MsiSetVectorsEnd;
    }

    //
    // Get an appropriate array for physical addresses and data pairs.
    //

    if (VectorCount == 1) {
        Information = &InformationBuffer;

    } else {
        AllocationSize = VectorCount * sizeof(MSI_INFORMATION);
        Information = MmAllocatePagedPool(AllocationSize,
                                          PCI_ALLOCATION_TAG);

        if (Information == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto MsiSetVectorsEnd;
        }
    }

    Status = HlGetMsiInformation(Vector,
                                 VectorCount,
                                 Processors,
                                 Information);

    if (!KSUCCESS(Status)) {
        goto MsiSetVectorsEnd;
    }

    //
    // Set the addresses and data in the MSI vector.
    //

    Status = STATUS_SUCCESS;
    switch (MsiType) {
    case PciMsiTypeBasic:

        ASSERT(MsiContext->MsiOffset != 0);
        ASSERT(VectorIndex == 0);

        //
        // Even if more than one vector is being programmed, there is only one
        // address and data field. Take the information from the first vector.
        //

        PhysicalAddress = Information[0].Address;
        MessageData = Information[0].Data;
        Offset = MsiContext->MsiOffset + PCI_MSI_LOWER_ADDRESS_OFFSET;
        PciDevice->WriteConfig(PciDevice->BusNumber,
                               PciDevice->DeviceNumber,
                               PciDevice->FunctionNumber,
                               Offset,
                               sizeof(ULONG),
                               (ULONG)PhysicalAddress);

        if ((MsiContext->MsiFlags & PCI_MSI_FLAG_64_BIT_CAPABLE) != 0) {
            Offset += sizeof(ULONG);
            PciDevice->WriteConfig(PciDevice->BusNumber,
                                   PciDevice->DeviceNumber,
                                   PciDevice->FunctionNumber,
                                   Offset,
                                   sizeof(ULONG),
                                   (ULONG)(PhysicalAddress >> 32));
        }

        Offset += sizeof(ULONG);
        PciDevice->WriteConfig(PciDevice->BusNumber,
                               PciDevice->DeviceNumber,
                               PciDevice->FunctionNumber,
                               Offset,
                               sizeof(USHORT),
                               (USHORT)MessageData);

        break;

    case PciMsiTypeExtended:

        ASSERT(MsiContext->MsiXOffset != 0);

        if (MsiContext->MsiXTable == NULL) {
            Status = PcipMapMsiXTable(MsiContext);
            if (!KSUCCESS(Status)) {
                goto MsiSetVectorsEnd;
            }
        }

        ASSERT(MsiContext->MsiXTable != NULL);

        TableEntry = (PPCI_MSI_X_TABLE_ENTRY)(MsiContext->MsiXTable +
                                              (VectorIndex *
                                               sizeof(PCI_MSI_X_TABLE_ENTRY)));

        for (Index = 0; Index < VectorCount; Index += 1) {
            if ((TableEntry->Control & PCI_MSI_X_VECTOR_CONTROL_MASKED) == 0) {
                TableEntry->Control |= PCI_MSI_X_VECTOR_CONTROL_MASKED;
                RtlMemoryBarrier();

            } else {
                MsiContext->MsiXVectorCount += 1;
            }

            TableEntry->Address = Information[Index].Address;
            TableEntry->Data = (ULONG)Information[Index].Data;
            RtlMemoryBarrier();
            TableEntry->Control &= ~PCI_MSI_X_VECTOR_CONTROL_MASKED;
            TableEntry += 1;
        }

        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        break;
    }

MsiSetVectorsEnd:
    if ((VectorCount != 1) && (Information != NULL)) {
        MmFreePagedPool(Information);
    }

    return Status;
}

KSTATUS
PcipMsiMaskVectors (
    PVOID DeviceToken,
    PCI_MSI_TYPE MsiType,
    ULONGLONG VectorIndex,
    ULONGLONG VectorCount,
    BOOL MaskVectors
    )

/*++

Routine Description:

    This routine masks or unmasks a set of contiguous MSI/MSI-X vectors for the
    given PCI device.

Arguments:

    DeviceToken - Supplies the device token supplied when the interface was
        acquired.

    MsiType - Supplies the type of the MSI vector.

    VectorIndex - Supplies the starting index of the vectors that are to be
        masked or unmasked.

    VectorCount - Supplies the number fo contiguous vectors to mask or unmask
        starting at the given index.

    MaskVectors - Supplies a boolean indicating whether the vector should be
        masked (TRUE) or unmasked (FALSE).

Return Value:

    Status code.

--*/

{

    ULONGLONG Index;
    ULONG Mask;
    PPCI_MSI_CONTEXT MsiContext;
    ULONG Offset;
    PPCI_DEVICE PciDevice;
    KSTATUS Status;
    PPCI_MSI_X_TABLE_ENTRY TableEntry;
    ULONG VectorMask;

    PciDevice = (PPCI_DEVICE)DeviceToken;
    MsiContext = PciDevice->MsiContext;

    //
    // Bail out immediately if an unsupported MSI type was requested.
    //

    if ((MsiContext == NULL) ||
        ((MsiType == PciMsiTypeBasic) && (MsiContext->MsiOffset == 0)) ||
        ((MsiType == PciMsiTypeExtended) && (MsiContext->MsiXOffset == 0)) ||
        ((MsiType != PciMsiTypeBasic) && (MsiType != PciMsiTypeExtended))) {

        return STATUS_NOT_SUPPORTED;
    }

    //
    // Consider it a success if no vectors were asked to be masked or unmaksed.
    //

    if (VectorCount == 0) {
        return STATUS_SUCCESS;
    }

    //
    // Validate the index and count based on the cached maximum vector count.
    //

    switch (MsiType) {
    case PciMsiTypeBasic:
        if ((VectorIndex + VectorCount) > MsiContext->MsiMaxVectorCount) {
            Status = STATUS_OUT_OF_BOUNDS;
            goto MsiMaskVectorsEnd;
        }

        //
        // Bail now if masking is not supported.
        //

        if ((MsiContext->MsiFlags & PCI_MSI_FLAG_MASKABLE) == 0) {
            Status = STATUS_NOT_SUPPORTED;
            goto MsiMaskVectorsEnd;
        }

        break;

    case PciMsiTypeExtended:
        if ((VectorIndex + VectorCount) > MsiContext->MsiXMaxVectorCount) {
            Status = STATUS_OUT_OF_BOUNDS;
            goto MsiMaskVectorsEnd;
        }

        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        goto MsiMaskVectorsEnd;
    }

    //
    // Mask or unmask the vectors based on the type.
    //

    Status = STATUS_SUCCESS;
    switch (MsiType) {
    case PciMsiTypeBasic:

        ASSERT(MsiContext->MsiOffset != 0);

        if ((MsiContext->MsiFlags & PCI_MSI_FLAG_64_BIT_CAPABLE) != 0) {
            Offset = MsiContext->MsiOffset + PCI_MSI_64_BIT_MASK_OFFSET;

        } else {
            Offset = MsiContext->MsiOffset + PCI_MSI_MASK_OFFSET;
        }

        //
        // Build out the mask of the vectors that are to be modified.
        //

        Mask = 0;
        for (Index = VectorIndex;
             Index < (VectorIndex + VectorCount);
             Index += 1) {

            Mask |= (1 << Index);
        }

        //
        // Read, modify, and write the configuration space to update the masks.
        //

        VectorMask = (ULONG)PciDevice->ReadConfig(PciDevice->BusNumber,
                                                  PciDevice->DeviceNumber,
                                                  PciDevice->FunctionNumber,
                                                  Offset,
                                                  sizeof(ULONG));

        if (MaskVectors != FALSE) {
            VectorMask |= Mask;

        } else {
            VectorMask &= ~Mask;
        }

        PciDevice->WriteConfig(PciDevice->BusNumber,
                               PciDevice->DeviceNumber,
                               PciDevice->FunctionNumber,
                               Offset,
                               sizeof(ULONG),
                               VectorMask);

        break;

    case PciMsiTypeExtended:

        ASSERT(MsiContext->MsiXOffset != 0);

        if (MsiContext->MsiXTable == NULL) {
            Status = PcipMapMsiXTable(MsiContext);
            if (!KSUCCESS(Status)) {
                goto MsiMaskVectorsEnd;
            }
        }

        ASSERT(MsiContext->MsiXTable != NULL);

        TableEntry = (PPCI_MSI_X_TABLE_ENTRY)(MsiContext->MsiXTable +
                                              (VectorIndex *
                                               sizeof(PCI_MSI_X_TABLE_ENTRY)));

        if (MaskVectors != FALSE) {
            for (Index = 0; Index < VectorCount; Index += 1) {
                if ((TableEntry->Control & PCI_MSI_X_VECTOR_CONTROL_MASKED) ==
                    0) {

                    TableEntry->Control |= PCI_MSI_X_VECTOR_CONTROL_MASKED;
                    MsiContext->MsiXVectorCount -= 1;
                }

                TableEntry += 1;
            }

        } else {
            for (Index = 0; Index < VectorCount; Index += 1) {
                if ((TableEntry->Control & PCI_MSI_X_VECTOR_CONTROL_MASKED) !=
                    0) {

                    TableEntry->Control &= ~PCI_MSI_X_VECTOR_CONTROL_MASKED;
                    MsiContext->MsiXVectorCount += 1;
                }

                TableEntry += 1;
            }
        }

        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        goto MsiMaskVectorsEnd;
    }

MsiMaskVectorsEnd:
    return Status;
}

KSTATUS
PcipMsiIsVectorMasked (
    PVOID DeviceToken,
    PCI_MSI_TYPE MsiType,
    ULONGLONG VectorIndex,
    PBOOL Masked
    )

/*++

Routine Description:

    This routine determines whether or not an MSI/MSI-X vector for the given
    PCI device is masked.

Arguments:

    DeviceToken - Supplies the device token supplied when the interface was
        acquired.

    MsiType - Supplies the type of the MSI vector.

    VectorIndex - Supplies the index of the vector whose masked state is to be
        returned.

    Masked - Supplies a pointer to a boolean that receives whether or not the
        vector is masked.

Return Value:

    Status code.

--*/

{

    ULONG Mask;
    PPCI_MSI_CONTEXT MsiContext;
    ULONG Offset;
    PPCI_DEVICE PciDevice;
    KSTATUS Status;
    PPCI_MSI_X_TABLE_ENTRY TableEntry;
    ULONG VectorMask;

    PciDevice = (PPCI_DEVICE)DeviceToken;
    MsiContext = PciDevice->MsiContext;

    //
    // Bail out immediately if an unsupported MSI type was requested.
    //

    if ((MsiContext == NULL) ||
        ((MsiType == PciMsiTypeBasic) && (MsiContext->MsiOffset == 0)) ||
        ((MsiType == PciMsiTypeExtended) && (MsiContext->MsiXOffset == 0)) ||
        ((MsiType != PciMsiTypeBasic) && (MsiType != PciMsiTypeExtended))) {

        return STATUS_NOT_SUPPORTED;
    }

    //
    // Validate the index based on the cached maximum vector count.
    //

    switch (MsiType) {
    case PciMsiTypeBasic:
        if (VectorIndex >= MsiContext->MsiMaxVectorCount) {
            Status = STATUS_OUT_OF_BOUNDS;
            goto MsiIsVectorMaskedEnd;
        }

        //
        // Bail now if masking is not supported.
        //

        if ((MsiContext->MsiFlags & PCI_MSI_FLAG_MASKABLE) == 0) {
            Status = STATUS_NOT_SUPPORTED;
            goto MsiIsVectorMaskedEnd;
        }

        break;

    case PciMsiTypeExtended:
        if (VectorIndex >= MsiContext->MsiXMaxVectorCount) {
            Status = STATUS_OUT_OF_BOUNDS;
            goto MsiIsVectorMaskedEnd;
        }

        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        goto MsiIsVectorMaskedEnd;
    }

    //
    // Determine whether or not the vector is masked or unmasked.
    //

    Status = STATUS_SUCCESS;
    switch (MsiType) {
    case PciMsiTypeBasic:

        ASSERT(MsiContext->MsiOffset != 0);

        if ((MsiContext->MsiFlags & PCI_MSI_FLAG_64_BIT_CAPABLE) != 0) {
            Offset = MsiContext->MsiOffset + PCI_MSI_64_BIT_MASK_OFFSET;

        } else {
            Offset = MsiContext->MsiOffset + PCI_MSI_MASK_OFFSET;
        }

        //
        // Read the vector mask data and check to see if the given vector's bit
        // is set.
        //

        VectorMask = (ULONG)PciDevice->ReadConfig(PciDevice->BusNumber,
                                                  PciDevice->DeviceNumber,
                                                  PciDevice->FunctionNumber,
                                                  Offset,
                                                  sizeof(ULONG));

        Mask = 1 << VectorIndex;
        if ((VectorMask & Mask) != 0) {
            *Masked = TRUE;

        } else {
            *Masked = FALSE;
        }

        break;

    case PciMsiTypeExtended:

        ASSERT(MsiContext->MsiXOffset != 0);

        if (MsiContext->MsiXTable == NULL) {
            Status = PcipMapMsiXTable(MsiContext);
            if (!KSUCCESS(Status)) {
                goto MsiIsVectorMaskedEnd;
            }
        }

        ASSERT(MsiContext->MsiXTable != NULL);

        TableEntry = (PPCI_MSI_X_TABLE_ENTRY)(MsiContext->MsiXTable +
                                              (VectorIndex *
                                               sizeof(PCI_MSI_X_TABLE_ENTRY)));

        if ((TableEntry->Control & PCI_MSI_X_VECTOR_CONTROL_MASKED) != 0) {
            *Masked = TRUE;

        } else {
            *Masked = FALSE;
        }

        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        goto MsiIsVectorMaskedEnd;
    }

MsiIsVectorMaskedEnd:
    return Status;
}

KSTATUS
PcipMsiIsVectorPending (
    PVOID DeviceToken,
    PCI_MSI_TYPE MsiType,
    ULONGLONG VectorIndex,
    PBOOL Pending
    )

/*++

Routine Description:

    This routine determines whether or not an MSI/MSI-X vector for the given
    PCI device is pending.

Arguments:

    DeviceToken - Supplies the device token supplied when the interface was
        acquired.

    MsiType - Supplies the type of the MSI vector.

    VectorIndex - Supplies the index of the vector whose pending state is to be
        returned.

    Pending - Supplies a pointer to a boolean that receives whether or not the
        vector has a pending interrupt.

Return Value:

    Status code.

--*/

{

    PULONG Address;
    ULONG Mask;
    PPCI_MSI_CONTEXT MsiContext;
    ULONG Offset;
    PPCI_DEVICE PciDevice;
    ULONG PendingMask;
    KSTATUS Status;

    PciDevice = (PPCI_DEVICE)DeviceToken;
    MsiContext = PciDevice->MsiContext;

    //
    // Bail out immediately if an unsupported MSI type was requested.
    //

    if ((MsiContext == NULL) ||
        ((MsiType == PciMsiTypeBasic) && (MsiContext->MsiOffset == 0)) ||
        ((MsiType == PciMsiTypeExtended) && (MsiContext->MsiXOffset == 0)) ||
        ((MsiType != PciMsiTypeBasic) && (MsiType != PciMsiTypeExtended))) {

        return STATUS_NOT_SUPPORTED;
    }

    //
    // Validate the index based on the cached maximum vector count.
    //

    switch (MsiType) {
    case PciMsiTypeBasic:
        if (VectorIndex >= MsiContext->MsiMaxVectorCount) {
            Status = STATUS_OUT_OF_BOUNDS;
            goto MsiIsVectorPendingEnd;
        }

        //
        // Bail now if masking/pending is not supported.
        //

        if ((MsiContext->MsiFlags & PCI_MSI_FLAG_MASKABLE) == 0) {
            Status = STATUS_NOT_SUPPORTED;
            goto MsiIsVectorPendingEnd;
        }

        break;

    case PciMsiTypeExtended:
        if (VectorIndex >= MsiContext->MsiXMaxVectorCount) {
            Status = STATUS_OUT_OF_BOUNDS;
            goto MsiIsVectorPendingEnd;
        }

        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        goto MsiIsVectorPendingEnd;
    }

    //
    // Determine whether or not the vector is pending or not.
    //

    Status = STATUS_SUCCESS;
    switch (MsiType) {
    case PciMsiTypeBasic:

        ASSERT(MsiContext->MsiOffset != 0);

        if ((MsiContext->MsiFlags & PCI_MSI_FLAG_64_BIT_CAPABLE) != 0) {
            Offset = MsiContext->MsiOffset + PCI_MSI_64_BIT_PENDING_OFFSET;

        } else {
            Offset = MsiContext->MsiOffset + PCI_MSI_PENDING_OFFSET;
        }

        //
        // Read the vector pending data and check to see if the given vector's
        // bit is set.
        //

        PendingMask = (ULONG)PciDevice->ReadConfig(PciDevice->BusNumber,
                                                   PciDevice->DeviceNumber,
                                                   PciDevice->FunctionNumber,
                                                   Offset,
                                                   sizeof(ULONG));

        Mask = 1 << VectorIndex;
        if ((PendingMask & Mask) != 0) {
            *Pending = TRUE;

        } else {
            *Pending = FALSE;
        }

        break;

    case PciMsiTypeExtended:

        ASSERT(MsiContext->MsiXOffset != 0);

        if (MsiContext->MsiXPendingArray == NULL) {
            Status = PcipMapMsiXPendingArray(MsiContext);
            if (!KSUCCESS(Status)) {
                goto MsiIsVectorPendingEnd;
            }
        }

        ASSERT(MsiContext->MsiXPendingArray != NULL);

        Address = MsiContext->MsiXPendingArray +
                  ((VectorIndex / 32) * sizeof(ULONG));

        Mask = 1 << (VectorIndex % 32);
        if ((*Address & Mask) != 0) {
            *Pending = TRUE;

        } else {
            *Pending = FALSE;
        }

        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        goto MsiIsVectorPendingEnd;
    }

MsiIsVectorPendingEnd:
    return Status;
}

KSTATUS
PcipMapMsiXTable (
    PPCI_MSI_CONTEXT MsiContext
    )

/*++

Routine Description:

    This routine synchronously maps the MSI-X table.

Arguments:

    MsiContext - Supplies a pointer to the MSI context whose MSI-X table is to
        be mapped.

Return Value:

    Status code.

--*/

{

    PVOID OriginalTable;
    PVOID Table;
    ULONG TableSize;

    ASSERT(MsiContext->MsiXOffset != 0);

    //
    // Exit immediately if the table is already mapped.
    //

    if (MsiContext->MsiXTable != NULL) {
        return STATUS_SUCCESS;
    }

    //
    // Fail if there is no physical address to map. This indicates that the
    // MSI-X interface is being invoked a bit early.
    //

    if (MsiContext->MsiXTablePhysicalAddress == INVALID_PHYSICAL_ADDRESS) {
        return STATUS_TOO_EARLY;
    }

    //
    // Map the vector table. The vector count was cached when the context was
    // initialized.
    //

    TableSize = MsiContext->MsiXMaxVectorCount * sizeof(PCI_MSI_X_TABLE_ENTRY);
    Table = MmMapPhysicalAddress(MsiContext->MsiXTablePhysicalAddress,
                                 TableSize,
                                 TRUE,
                                 FALSE,
                                 TRUE);

    if (Table == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Synchronously try to set this as the virtual address of the table.
    //

    OriginalTable = (PVOID)RtlAtomicCompareExchange(
                                    (volatile UINTN *)&(MsiContext->MsiXTable),
                                    (UINTN)Table,
                                    (UINTN)NULL);

    if (OriginalTable != NULL) {
        MmUnmapAddress(Table, TableSize);
    }

    ASSERT(MsiContext->MsiXTable != NULL);

    return STATUS_SUCCESS;
}

KSTATUS
PcipMapMsiXPendingArray (
    PPCI_MSI_CONTEXT MsiContext
    )

/*++

Routine Description:

    This routine synchronously maps the MSI-X pending bit array.

Arguments:

    MsiContext - Supplies a pointer to the MSI context whose MSI-X pending bit
        array is to be mapped.

Return Value:

    Status code.

--*/

{

    PVOID Array;
    ULONG ArraySize;
    PVOID OriginalArray;

    ASSERT(MsiContext->MsiXOffset != 0);

    //
    // Exit immediately if the pending array is already mapped.
    //

    if (MsiContext->MsiXPendingArray != NULL) {
        return STATUS_SUCCESS;
    }

    //
    // Fail if there is no physical address to map. This indicates that the
    // MSI-X interface is being invoked a bit early.
    //

    if (MsiContext->MsiXPendingArrayPhysicalAddress ==
        INVALID_PHYSICAL_ADDRESS) {

        return STATUS_TOO_EARLY;
    }

    //
    // Determine the size of the array in bytes based on the cached vector
    // count.
    //

    ArraySize = MsiContext->MsiXMaxVectorCount / 64;
    if ((MsiContext->MsiXMaxVectorCount % 64) != 0) {
        ArraySize += 1;
    }

    ArraySize *= sizeof(ULONGLONG);
    Array = MmMapPhysicalAddress(MsiContext->MsiXPendingArrayPhysicalAddress,
                                 ArraySize,
                                 TRUE,
                                 FALSE,
                                 TRUE);

    if (Array == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Synchronously try to set this as the virtual address of the table.
    //

    OriginalArray = (PVOID)RtlAtomicCompareExchange(
                             (volatile UINTN *)&(MsiContext->MsiXPendingArray),
                             (UINTN)Array,
                             (UINTN)NULL);

    if (OriginalArray != NULL) {
        MmUnmapAddress(Array, ArraySize);
    }

    ASSERT(MsiContext->MsiXPendingArray != NULL);

    return STATUS_SUCCESS;
}

