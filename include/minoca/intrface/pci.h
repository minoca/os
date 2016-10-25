/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pci.h

Abstract:

    This header contains PCI device interfaces.

Author:

    Evan Green 19-Sep-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Interface UUID for PCI Configuration space access.
//

#define UUID_PCI_CONFIG_ACCESS \
    {{0x20656854, 0x646F6F77, 0x72612073, 0x6F6C2065}}

//
// Interface UUID for PCI Configuration space access to a specific device.
//

#define UUID_PCI_CONFIG_ACCESS_SPECIFIC \
    {{0x796C6576, 0x72616420, 0x6E61206b, 0x65642064}}

//
// Interface UUID for PCI MSI and MSI-X access.
//

#define UUID_PCI_MESSAGE_SIGNALED_INTERRUPTS \
    {{0x5BAAFA00, 0x079911E4, 0x9EEA20C9, 0xD0BFFAF6}}

//
// Define the PCI MSI/MSI-X information version.
//

#define PCI_MSI_INTERFACE_INFORMATION_VERSION 1

//
// Define the PCI MSI/MSI-X flags.
//

#define PCI_MSI_INTERFACE_FLAG_ENABLED        0x00000001
#define PCI_MSI_INTERFACE_FLAG_64_BIT_CAPABLE 0x00000002
#define PCI_MSI_INTERFACE_FLAG_MASKABLE       0x00000004
#define PCI_MSI_INTERFACE_FLAG_GLOBAL_MASK    0x00000008

//
// ------------------------------------------------------ Data Type Definitions
//

//
// PCI Configuration Access interface.
//

typedef
KSTATUS
(*PREAD_PCI_CONFIG) (
    PVOID DeviceToken,
    ULONG Offset,
    ULONG AccessSize,
    PULONGLONG Value
    );

/*++

Routine Description:

    This routine reads from a device's PCI configuration space.

Arguments:

    DeviceToken - Supplies the device token supplied when the interface was
        acquired.

    Offset - Supplies the offset in bytes into the PCI configuration space to
        read.

    AccessSize - Supplies the size of the access to make. Valid values are 1,
        2, 4, and 8.

    Value - Supplies a pointer where the value read from PCI configuration
        space will be returned on success.

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PWRITE_PCI_CONFIG) (
    PVOID DeviceToken,
    ULONG Offset,
    ULONG AccessSize,
    ULONGLONG Value
    );

/*++

Routine Description:

    This routine writes to a device's PCI configuration space.

Arguments:

    DeviceToken - Supplies the device token supplied when the interface was
        acquired.

    Offset - Supplies the offset in bytes into the PCI configuration space to
        write.

    AccessSize - Supplies the size of the access to make. Valid values are 1,
        2, 4, and 8.

    Value - Supplies the value to write into PCI configuration space.

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PREAD_SPECIFIC_PCI_CONFIG) (
    PVOID DeviceToken,
    ULONG BusNumber,
    ULONG DeviceNumber,
    ULONG FunctionNumber,
    ULONG Offset,
    ULONG AccessSize,
    PULONGLONG Value
    );

/*++

Routine Description:

    This routine reads from a specific device's PCI configuration space.

Arguments:

    DeviceToken - Supplies the device token supplied when the interface was
        acquired.

    BusNumber - Supplies the bus number of the device whose PCI configuration
        space should be read from.

    DeviceNumber - Supplies the device number of the device whose PCI
        configuration space should be read from.

    FunctionNumber - Supplies the function number of the device whose PCI
        configuration space should be read from.

    Offset - Supplies the offset in bytes into the PCI configuration space to
        read.

    AccessSize - Supplies the size of the access to make. Valid values are 1,
        2, 4, and 8.

    Value - Supplies a pointer where the value read from PCI configuration
        space will be returned on success.

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PWRITE_SPECIFIC_PCI_CONFIG) (
    PVOID DeviceToken,
    ULONG BusNumber,
    ULONG DeviceNumber,
    ULONG FunctionNumber,
    ULONG Offset,
    ULONG AccessSize,
    ULONGLONG Value
    );

/*++

Routine Description:

    This routine writes to a specific device's PCI configuration space.

Arguments:

    DeviceToken - Supplies the device token supplied when the interface was
        acquired.

    BusNumber - Supplies the bus number of the device whose PCI configuration
        space should be written to.

    DeviceNumber - Supplies the device number of the device whose PCI
        configuration space should be written to.

    FunctionNumber - Supplies the function number of the device whose PCI
        configuration space should be written to.

    Offset - Supplies the offset in bytes into the PCI configuration space to
        write.

    AccessSize - Supplies the size of the access to make. Valid values are 1,
        2, 4, and 8.

    Value - Supplies the value to write into PCI configuration space.

Return Value:

    Status code.

--*/

typedef enum _PCI_MSI_TYPE {
    PciMsiTypeInvalid,
    PciMsiTypeBasic,
    PciMsiTypeExtended,
    PciMsiTypeMax
} PCI_MSI_TYPE, *PPCI_MSI_TYPE;

/*++

Structure Description:

    This structure defines the message signaled interrupt information that can
    be queried or set.

Members:

    Version - Stores the version of the PCI MSI information structure.

    MsiType - Stores the type of MSI data to be set or returned.

    Flags - Stores a bitmask of PCI MSI flags. See PCI_MSI_INTERFACE_FLAG_*.

    VectorCount - Stores the number of vectors to enable for the PCI device.
        On a query request, it returns the number of vectors currenty enabled.
        This value is read only for MSI-X.

    MaxVectorCount - Stores the maximum number of vectors that can be used on
        the PCI device. This value is read-only.

--*/

typedef struct _PCI_MSI_INFORMATION {
    ULONG Version;
    PCI_MSI_TYPE MsiType;
    ULONG Flags;
    ULONGLONG VectorCount;
    ULONGLONG MaxVectorCount;
} PCI_MSI_INFORMATION, *PPCI_MSI_INFORMATION;

typedef
KSTATUS
(*PMSI_GET_SET_INFORMATION) (
    PVOID DeviceToken,
    PPCI_MSI_INFORMATION Information,
    BOOL Set
    );

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

typedef
KSTATUS
(*PMSI_SET_VECTORS) (
    PVOID DeviceToken,
    PCI_MSI_TYPE MsiType,
    ULONGLONG Vector,
    ULONGLONG VectorIndex,
    ULONGLONG VectorCount,
    PPROCESSOR_SET Processors
    );

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

typedef
KSTATUS
(*PMSI_MASK_VECTORS) (
    PVOID DeviceToken,
    PCI_MSI_TYPE MsiType,
    ULONGLONG VectorIndex,
    ULONGLONG VectorCount,
    BOOL MaskVector
    );

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

typedef
KSTATUS
(*PMSI_IS_VECTOR_MASKED) (
    PVOID DeviceToken,
    PCI_MSI_TYPE MsiType,
    ULONGLONG VectorIndex,
    PBOOL Masked
    );

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

typedef
KSTATUS
(*PMSI_IS_VECTOR_PENDING) (
    PVOID DeviceToken,
    PCI_MSI_TYPE MsiType,
    ULONGLONG VectorIndex,
    PBOOL Pending
    );

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

/*++

Structure Description:

    This structure defines the interface for a device to access its PCI
    configuration space.

Members:

    ReadPciConfig - Stores a pointer to a function that can be used to read
        PCI configuration space.

    WritePciConfig - Stores a pointer to a function that can be used to write
        to PCI configuration space.

    DeviceToken - Stores an opaque token passed to the read and write
        functions that uniquely identifies the device.

--*/

typedef struct _INTERFACE_PCI_CONFIG_ACCESS {
    PREAD_PCI_CONFIG ReadPciConfig;
    PWRITE_PCI_CONFIG WritePciConfig;
    PVOID DeviceToken;
} INTERFACE_PCI_CONFIG_ACCESS, *PINTERFACE_PCI_CONFIG_ACCESS;

/*++

Structure Description:

    This structure defines the interface exposed by a PCI bus or bridge that
    allows access to a specific device's PCI configuration space.

Members:

    ReadPciConfig - Stores a pointer to a function that can be used to read
        PCI configuration space.

    WritePciConfig - Stores a pointer to a function that can be used to write
        to PCI configuration space.

    DeviceToken - Stores an opaque token passed to the read and write
        functions that uniquely identifies the device.

--*/

typedef struct _INTERFACE_SPECIFIC_PCI_CONFIG_ACCESS {
    PREAD_SPECIFIC_PCI_CONFIG ReadPciConfig;
    PWRITE_SPECIFIC_PCI_CONFIG WritePciConfig;
    PVOID DeviceToken;
} INTERFACE_SPECIFIC_PCI_CONFIG_ACCESS, *PINTERFACE_SPECIFIC_PCI_CONFIG_ACCESS;

/*++

Structure Description:

    This structure defines the interface for a PCI device to access its MSI and
    MSI-X configuration information, if supported.

Members:

    GetSetInformation - Stores a pointer to a function that can be used to
        get or set the MSI or MSI-X information. Includes the ability to enable
        MSI/MSI-X.

    SetVectors - Stores a pointer to a function that can be used to configure a
        contiguous set of MSI/MSI-X vectors.

    MaskVectors - Stores a pointer to a function that can be used to mask or
        unmask a contiguous set of MSI or MSI-X vectors.

    IsVectorMasked - Stores a pointer to a function that can be used to
        determine whether or not a given vector is masked.

    IsVectorPending - Stores a pointer to a function that can be used to
        determine whether or not a given vector is pending.

    DeviceToken - Stores an oqaque token passed to the query and set functions
        that uniquely identifies the device.

--*/

typedef struct _INTERFACE_PCI_MSI {
    PMSI_GET_SET_INFORMATION GetSetInformation;
    PMSI_SET_VECTORS SetVectors;
    PMSI_MASK_VECTORS MaskVectors;
    PMSI_IS_VECTOR_MASKED IsVectorMasked;
    PMSI_IS_VECTOR_PENDING IsVectorPending;
    PVOID DeviceToken;
} INTERFACE_PCI_MSI, *PINTERFACE_PCI_MSI;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

