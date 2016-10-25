/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    memory.h

Abstract:

    This header contains definitions for UEFI core memory services.

Author:

    Evan Green 27-Feb-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the expansion size of pool and memory descriptor allocations.
//

#define EFI_MEMORY_EXPANSION_SIZE EFI_PAGE_SIZE

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

extern EFI_LOCK EfiMemoryLock;

//
// -------------------------------------------------------- Function Prototypes
//

EFIAPI
EFI_STATUS
EfiCoreAllocatePages (
    EFI_ALLOCATE_TYPE Type,
    EFI_MEMORY_TYPE MemoryType,
    UINTN Pages,
    EFI_PHYSICAL_ADDRESS *Memory
    );

/*++

Routine Description:

    This routine allocates memory pages from the system.

Arguments:

    Type - Supplies the allocation strategy to use.

    MemoryType - Supplies the memory type of the allocation.

    Pages - Supplies the number of contiguous EFI_PAGE_SIZE pages.

    Memory - Supplies a pointer that on input contains a physical address whose
        use depends on the allocation strategy. On output, the physical address
        of the allocation will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the Type or MemoryType are invalid, or Memory is
    NULL.

    EFI_OUT_OF_RESOURCES if the pages could not be allocated.

    EFI_NOT_FOUND if the requested pages could not be found.

--*/

EFIAPI
EFI_STATUS
EfiCoreFreePages (
    EFI_PHYSICAL_ADDRESS Memory,
    UINTN Pages
    );

/*++

Routine Description:

    This routine frees memory pages back to the system.

Arguments:

    Memory - Supplies the base physical address of the allocation to free.

    Pages - Supplies the number of pages to free.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the memory is not page aligned or is invalid.

    EFI_NOT_FOUND if the requested pages were not allocated.

--*/

EFIAPI
EFI_STATUS
EfiCoreGetMemoryMap (
    UINTN *MemoryMapSize,
    EFI_MEMORY_DESCRIPTOR *MemoryMap,
    UINTN *MapKey,
    UINTN *DescriptorSize,
    UINT32 *DescriptorVersion
    );

/*++

Routine Description:

    This routine returns the current memory map.

Arguments:

    MemoryMapSize - Supplies a pointer to the size, in bytes, of the memory
        map buffer. On input, this is the size of the buffer allocated by the
        caller. On output, this is the size of the buffer returned by the
        firmware if the buffer was large enough, or the size of the buffer
        needed if the buffer was too small.

    MemoryMap - Supplies a pointer to a caller-allocated buffer where the
        memory map will be written on success.

    MapKey - Supplies a pointer where the firmware returns the map key.

    DescriptorSize - Supplies a pointer where the firmware returns the size of
        the EFI_MEMORY_DESCRIPTOR structure.

    DescriptorVersion - Supplies a pointer where the firmware returns the
        version number associated with the EFI_MEMORY_DESCRIPTOR structure.

Return Value:

    EFI_SUCCESS on success.

    EFI_BUFFER_TOO_SMALL if the supplied buffer was too small. The size needed
    is returned in the size parameter.

    EFI_INVALID_PARAMETER if the supplied size or memory map pointers are NULL.

--*/

VOID *
EfiCoreAllocatePoolPages (
    EFI_MEMORY_TYPE PoolType,
    UINTN PageCount,
    UINTN Alignment
    );

/*++

Routine Description:

    This routine allocates pages to back pool allocations and memory map
    descriptors.

Arguments:

    PoolType - Supplies the memory type of the allocation.

    PageCount - Supplies the number of pages to allocate.

    Alignment - Supplies the required alignment.

Return Value:

    Returns a pointer to the allocated memory on success.

    NULL on allocation failure.

--*/

VOID
EfiCoreFreePoolPages (
    EFI_PHYSICAL_ADDRESS Memory,
    UINTN PageCount
    );

/*++

Routine Description:

    This routine frees pages allocated for pool or descriptor.

Arguments:

    Memory - Supplies the address of the allocation.

    PageCount - Supplies the number of pages to free.

Return Value:

    None.

--*/

EFI_STATUS
EfiCoreInitializeMemoryServices (
    VOID *FirmwareLowestAddress,
    UINTN FirmwareSize,
    VOID *StackBase,
    UINTN StackSize
    );

/*++

Routine Description:

    This routine initializes core UEFI memory services.

Arguments:

    FirmwareLowestAddress - Supplies the lowest address where the firmware was
        loaded into memory.

    FirmwareSize - Supplies the size of the firmware image in memory, in bytes.

    StackBase - Supplies the base (lowest) address of the stack.

    StackSize - Supplies the size in bytes of the stack. This should be at
        least 0x4000 bytes (16kB).

Return Value:

    EFI status code.

--*/

EFI_STATUS
EfiCoreTerminateMemoryServices (
    UINTN MapKey
    );

/*++

Routine Description:

    This routine terminates memory services.

Arguments:

    MapKey - Supplies the map key reported by the boot application. This is
        checked against the current map key to ensure the boot application has
        an up to date view of the world.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the map key is not valid or the memory map is
    not consistent.

--*/

EFIAPI
VOID
EfiCoreEmptyCallbackFunction (
    EFI_EVENT Event,
    VOID *Context
    );

/*++

Routine Description:

    This routine does nothing but return. It conforms to the event notification
    function prototype.

Arguments:

    Event - Supplies an unused event.

    Context - Supplies an unused context pointer.

Return Value:

    None.

--*/

EFIAPI
VOID
EfiCoreCopyMemory (
    VOID *Destination,
    VOID *Source,
    UINTN Length
    );

/*++

Routine Description:

    This routine copies the contents of one buffer to another.

Arguments:

    Destination - Supplies a pointer to the destination of the copy.

    Source - Supplies a pointer to the source of the copy.

    Length - Supplies the number of bytes to copy.

Return Value:

    None.

--*/

EFIAPI
VOID
EfiCoreSetMemory (
    VOID *Buffer,
    UINTN Size,
    UINT8 Value
    );

/*++

Routine Description:

    This routine fills a buffer with a specified value.

Arguments:

    Buffer - Supplies a pointer to the buffer to fill.

    Size - Supplies the size of the buffer in bytes.

    Value - Supplies the value to fill the buffer with.

Return Value:

    None.

--*/

INTN
EfiCoreCompareMemory (
    VOID *FirstBuffer,
    VOID *SecondBuffer,
    UINTN Length
    );

/*++

Routine Description:

    This routine compares the contents of two buffers for equality.

Arguments:

    FirstBuffer - Supplies a pointer to the first buffer to compare.

    SecondBuffer - Supplies a pointer to the second buffer to compare.

    Length - Supplies the number of bytes to compare.

Return Value:

    0 if the buffers are identical.

    Returns the first mismatched byte as
    First[MismatchIndex] - Second[MismatchIndex].

--*/

BOOLEAN
EfiCoreCompareGuids (
    EFI_GUID *FirstGuid,
    EFI_GUID *SecondGuid
    );

/*++

Routine Description:

    This routine compares two GUIDs.

Arguments:

    FirstGuid - Supplies a pointer to the first GUID.

    SecondGuid - Supplies a pointer to the second GUID.

Return Value:

    TRUE if the GUIDs are equal.

    FALSE if the GUIDs are different.

--*/

VOID *
EfiCoreAllocateBootPool (
    UINTN Size
    );

/*++

Routine Description:

    This routine allocates pool from boot services data.

Arguments:

    Size - Supplies the size of the allocation in bytes.

Return Value:

    Returns a pointer to the allocation on success.

    NULL on allocation failure.

--*/

VOID *
EfiCoreAllocateRuntimePool (
    UINTN Size
    );

/*++

Routine Description:

    This routine allocates pool from runtime services data.

Arguments:

    Size - Supplies the size of the allocation in bytes.

Return Value:

    Returns a pointer to the allocation on success.

    NULL on allocation failure.

--*/

INTN
EfiCoreFindHighBitSet64 (
    UINT64 Value
    );

/*++

Routine Description:

    This routine returns the bit position of the highest bit set in a 64-bit
    value.

Arguments:

    Value - Supplies the input value.

Return Value:

    Returns the index of the highest bit set, between 0 and 63. If the value is
    zero, then -1 is returned.

--*/

INTN
EfiCoreFindHighBitSet32 (
    UINT32 Value
    );

/*++

Routine Description:

    This routine returns the bit position of the highest bit set in a 32-bit
    value.

Arguments:

    Value - Supplies the input value.

Return Value:

    Returns the index of the highest bit set, between 0 and 31. If the value is
    zero, then -1 is returned.

--*/

VOID
EfiCoreCalculateTableCrc32 (
    EFI_TABLE_HEADER *Header
    );

/*++

Routine Description:

    This routine recalculates the CRC32 of a given EFI table.

Arguments:

    Header - Supplies a pointer to the header. The size member will be used to
        determine the size of the entire table.

Return Value:

    None. The CRC is set in the header.

--*/

EFIAPI
EFI_EVENT
EfiCoreCreateProtocolNotifyEvent (
    EFI_GUID *ProtocolGuid,
    EFI_TPL NotifyTpl,
    EFI_EVENT_NOTIFY NotifyFunction,
    VOID *NotifyContext,
    VOID **Registration
    );

/*++

Routine Description:

    This routine creates an event, then registers that event to be notified
    whenever the given protocol appears. Finally, it signals the event so that
    any pre-existing protocols will be found.

Arguments:

    ProtocolGuid - Supplies a pointer to the GUID of the protocol to watch.

    NotifyTpl - Supplies the Task Priority Level of the callback function.

    NotifyFunction - Supplies a pointer to the function to call when a new
        protocol with the given GUID crops up.

    NotifyContext - Supplies a pointer to pass into the notify function.

    Registration - Supplies a pointer where the registration token for the
        event will be returned.

Return Value:

    Returns the notification event that was created.

    NULL on failure.

--*/

UINTN
EfiCoreStringLength (
    CHAR16 *String
    );

/*++

Routine Description:

    This routine returns the length of the given string, in characters (not
    bytes).

Arguments:

    String - Supplies a pointer to the string.

Return Value:

    Returns the number of characters in the string.

--*/

VOID
EfiCoreCopyString (
    CHAR16 *Destination,
    CHAR16 *Source
    );

/*++

Routine Description:

    This routine copies one string over to another buffer.

Arguments:

    Destination - Supplies a pointer to the destination buffer where the
        string will be copied to.

    Source - Supplies a pointer to the string to copy.

Return Value:

    None.

--*/

EFI_TPL
EfiCoreGetCurrentTpl (
    VOID
    );

/*++

Routine Description:

    This routine returns the current TPL.

Arguments:

    None.

Return Value:

    Returns the current TPL.

--*/

EFI_STATUS
EfiCoreInitializePool (
    VOID
    );

/*++

Routine Description:

    This routine initializes EFI core pool services.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

EFIAPI
EFI_STATUS
EfiCoreAllocatePool (
    EFI_MEMORY_TYPE PoolType,
    UINTN Size,
    VOID **Buffer
    );

/*++

Routine Description:

    This routine allocates memory from the heap.

Arguments:

    PoolType - Supplies the type of pool to allocate.

    Size - Supplies the number of bytes to allocate from the pool.

    Buffer - Supplies a pointer where a pointer to the allocated buffer will
        be returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_OUT_OF_RESOURCES if memory could not be allocated.

    EFI_INVALID_PARAMETER if the pool type was invalid or the buffer is NULL.

--*/

EFIAPI
EFI_STATUS
EfiCoreFreePool (
    VOID *Buffer
    );

/*++

Routine Description:

    This routine frees heap allocated memory.

Arguments:

    Buffer - Supplies a pointer to the buffer to free.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the buffer was invalid.

--*/

