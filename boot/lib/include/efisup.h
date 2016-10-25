/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    efisup.h

Abstract:

    This header contains definitions for EFI support in the boot loader.

Author:

    Evan Green 11-Feb-2014

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
// Store pointers to the main EFI structures.
//

extern EFI_HANDLE BoEfiImageHandle;
extern EFI_SYSTEM_TABLE *BoEfiSystemTable;
extern EFI_BOOT_SERVICES *BoEfiBootServices;
extern EFI_RUNTIME_SERVICES *BoEfiRuntimeServices;

//
// Define some needed protocol GUIDs.
//

extern EFI_GUID BoEfiLoadedImageProtocolGuid;
extern EFI_GUID BoEfiBlockIoProtocolGuid;
extern EFI_GUID BoEfiGraphicsOutputProtocolGuid;
extern EFI_GUID BoEfiDevicePathProtocolGuid;
extern EFI_GUID BoEfiRamDiskProtocolGuid;

extern EFI_GUID BoEfiAcpiTableGuid;
extern EFI_GUID BoEfiAcpi1TableGuid;
extern EFI_GUID BoEfiSmbiosTableGuid;

//
// Store the allocation containing the memory descriptors for the memory map.
// This is the first allocation to arrive and the last to go, as it contains
// the list of other allocations to clean up.
//

extern EFI_PHYSICAL_ADDRESS BoEfiDescriptorAllocation;
extern UINTN BoEfiDescriptorAllocationPageCount;

//
// -------------------------------------------------------- Function Prototypes
//

//
// Define functions implemented by the application, callable by the boot
// library.
//

EFI_STATUS
BoEfiApplicationMain (
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable,
    PVOID TopOfStack,
    ULONG StackSize
    );

/*++

Routine Description:

    This routine is the entry point for the EFI Boot Application.

Arguments:

    ImageHandle - Supplies a pointer to the image handle.

    SystemTable - Supplies a pointer to the EFI system table.

    TopOfStack - Supplies the top of the stack that has been set up for the
        loader.

    StackSize - Supplies the total size of the stack set up for the loader, in
        bytes.

Return Value:

    EFI status code.

--*/

VOID
BopEfiArchInitialize (
    PVOID *TopOfStack,
    PULONG StackSize
    );

/*++

Routine Description:

    This routine performs early architecture specific initialization of an EFI
    application.

Arguments:

    TopOfStack - Supplies a pointer where an approximation of the top of the
        stack will be returned.

    StackSize - Supplies a pointer where the stack size will be returned.

Return Value:

    None.

--*/

//
// Utility functions
//

UINTN
BopEfiGetStackPointer (
    VOID
    );

/*++

Routine Description:

    This routine gets the value of the stack register. Note that this can only
    be used as an approximate value, since as soon as this function returns
    the stack pointer changes.

Arguments:

    None.

Return Value:

    Returns the current stack pointer.

--*/

VOID
BopEfiSaveInitialState (
    VOID
    );

/*++

Routine Description:

    This routine saves the initial CPU state as passed to the application. This
    state is restored when making EFI calls.

Arguments:

    None.

Return Value:

    None. The original contents are saved in globals.

--*/

VOID
BopEfiRestoreFirmwareContext (
    VOID
    );

/*++

Routine Description:

    This routine restores the processor context set when the EFI application
    was started. This routine is called right before an EFI firmware call is
    made. It is not possible to debug through this function, as the IDT is
    swapped out.

Arguments:

    None.

Return Value:

    None. The OS loader context is saved in globals.

--*/

VOID
BopEfiRestoreApplicationContext (
    VOID
    );

/*++

Routine Description:

    This routine restores the boot application context. This routine is called
    after an EFI call to restore the processor state set up by the OS loader.

Arguments:

    None.

Return Value:

    None.

--*/

EFI_STATUS
BopEfiLocateHandle (
    EFI_LOCATE_SEARCH_TYPE SearchType,
    EFI_GUID *Protocol,
    VOID *SearchKey,
    UINTN *BufferSize,
    EFI_HANDLE *Buffer
    );

/*++

Routine Description:

    This routine returns an array of handles that support a specified protocol.

Arguments:

    SearchType - Supplies which handle(s) are to be returned.

    Protocol - Supplies an optional pointer to the protocols to search by.

    SearchKey - Supplies an optional pointer to the search key.

    BufferSize - Supplies a pointer that on input contains the size of the
        result buffer in bytes. On output, the size of the result array will be
        returned (even if the buffer was too small).

    Buffer - Supplies a pointer where the results will be returned.

Return Value:

    EFI status code.

--*/

EFI_STATUS
BopEfiLocateHandleBuffer (
    EFI_LOCATE_SEARCH_TYPE SearchType,
    EFI_GUID *Protocol,
    VOID *SearchKey,
    UINTN *HandleCount,
    EFI_HANDLE **Buffer
    );

/*++

Routine Description:

    This routine returns an array of handles that support the requested
    protocol in a buffer allocated from pool.

Arguments:

    SearchType - Supplies the search behavior.

    Protocol - Supplies a pointer to the protocol to search by.

    SearchKey - Supplies a pointer to the search key.

    HandleCount - Supplies a pointer where the number of handles will be
        returned.

    Buffer - Supplies a pointer where an array will be returned containing the
        requested handles.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if no handles match the search.

    EFI_INVALID_PARAMETER if the handle count or buffer is NULL.

    EFI_OUT_OF_RESOURCES if an allocation failed.

--*/

EFI_STATUS
BopEfiOpenProtocol (
    EFI_HANDLE Handle,
    EFI_GUID *Protocol,
    VOID **Interface,
    EFI_HANDLE AgentHandle,
    EFI_HANDLE ControllerHandle,
    UINT32 Attributes
    );

/*++

Routine Description:

    This routine queries a handle to determine if it supports a specified
    protocol. If the protocol is supported by the handle, it opens the protocol
    on behalf of the calling agent.

Arguments:

    Handle - Supplies the handle for the protocol interface that is being
        opened.

    Protocol - Supplies the published unique identifier of the protocol.

    Interface - Supplies the address where a pointer to the corresponding
        protocol interface is returned.

    AgentHandle - Supplies the handle of the agent that is opening the protocol
        interface specified by the protocol and interface.

    ControllerHandle - Supplies the controller handle that requires the
        protocl interface if the caller is a driver that follows the UEFI
        driver model. If the caller does not follow the UEFI Driver Model, then
        this parameter is optional.

    Attributes - Supplies the open mode of the protocol interface specified by
        the given handle and protocol.

Return Value:

    EFI status code.

--*/

EFI_STATUS
BopEfiCloseProtocol (
    EFI_HANDLE Handle,
    EFI_GUID *Protocol,
    EFI_HANDLE AgentHandle,
    EFI_HANDLE ControllerHandle
    );

/*++

Routine Description:

    This routine closes a protocol on a handle that was previously opened.

Arguments:

    Handle - Supplies the handle for the protocol interface was previously
        opened.

    Protocol - Supplies the published unique identifier of the protocol.

    AgentHandle - Supplies the handle of the agent that is closing the
        protocol interface.

    ControllerHandle - Supplies the controller handle that required the
        protocl interface if the caller is a driver that follows the UEFI
        driver model. If the caller does not follow the UEFI Driver Model, then
        this parameter is optional.

Return Value:

    EFI status code.

--*/

EFI_STATUS
BopEfiHandleProtocol (
    EFI_HANDLE Handle,
    EFI_GUID *Protocol,
    VOID **Interface
    );

/*++

Routine Description:

    This routine queries a handle to determine if it supports a specified
    protocol.

Arguments:

    Handle - Supplies the handle being queried.

    Protocol - Supplies the published unique identifier of the protocol.

    Interface - Supplies the address where a pointer to the corresponding
        protocol interface is returned.

Return Value:

    EFI_SUCCESS if the interface information was returned.

    EFI_UNSUPPORTED if the device not support the specified protocol.

    EFI_INVALID_PARAMETER if the handle, protocol, or interface is NULL.

--*/

EFI_STATUS
BopEfiFreePool (
    VOID *Buffer
    );

/*++

Routine Description:

    This routine frees memory allocated from the EFI firmware heap (not the
    boot environment heap).

Arguments:

    Buffer - Supplies a pointer to the buffer to free.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the buffer was invalid.

--*/

EFI_STATUS
BopEfiExitBootServices (
    EFI_HANDLE ImageHandle,
    UINTN MapKey
    );

/*++

Routine Description:

    This routine terminates all boot services.

Arguments:

    ImageHandle - Supplies the handle that identifies the exiting image.

    MapKey - Supplies the latest memory map key.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the map key is incorrect.

--*/

EFI_STATUS
BopEfiGetTime (
    EFI_TIME *Time,
    EFI_TIME_CAPABILITIES *Capabilities
    );

/*++

Routine Description:

    This routine returns the current time and date information, and
    timekeeping capabilities of the hardware platform.

Arguments:

    Time - Supplies a pointer where the current time will be returned.

    Capabilities - Supplies an optional pointer where the capabilities will be
        returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the time parameter was NULL.

    EFI_DEVICE_ERROR if there was a hardware error accessing the device.

--*/

EFI_STATUS
BopEfiStall (
    UINTN Microseconds
    );

/*++

Routine Description:

    This routine induces a fine-grained delay.

Arguments:

    Microseconds - Supplies the number of microseconds to stall execution for.

Return Value:

    EFI_SUCCESS on success.

--*/

VOID
BopEfiResetSystem (
    EFI_RESET_TYPE ResetType,
    EFI_STATUS ResetStatus,
    UINTN DataSize,
    VOID *ResetData
    );

/*++

Routine Description:

    This routine resets the entire platform.

Arguments:

    ResetType - Supplies the type of reset to perform.

    ResetStatus - Supplies the status code for this reset.

    DataSize - Supplies the size of the reset data.

    ResetData - Supplies an optional pointer for reset types of cold, warm, or
        shutdown to a null-terminated string, optionally followed by additional
        binary data.

Return Value:

    None. This routine does not return.

--*/

VOID
BopEfiPrintString (
    PWSTR WideString
    );

/*++

Routine Description:

    This routine prints a string to the EFI standard out console.

Arguments:

    WideString - Supplies a pointer to the wide string to print. A wide
        character is two bytes in EFI.

Return Value:

    None.

--*/

KSTATUS
BopEfiGetSystemConfigurationTable (
    EFI_GUID *Guid,
    VOID **Table
    );

/*++

Routine Description:

    This routine attempts to find a configuration table with the given GUID.

Arguments:

    Guid - Supplies a pointer to the GUID to search for.

    Table - Supplies a pointer where a pointer to the table will be returned on
        success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_FOUND if no table with the given GUID could be found.

    STATUS_NOT_INITIALIZED if the EFI subsystem has not yet started.

--*/

KSTATUS
BopEfiStatusToKStatus (
    EFI_STATUS Status
    );

/*++

Routine Description:

    This routine returns a kernel status code similar to the given EFI status
    code.

Arguments:

    Status - Supplies the EFI status code.

Return Value:

    Status code.

--*/

BOOL
BopEfiAreGuidsEqual (
    EFI_GUID *FirstGuid,
    EFI_GUID *SecondGuid
    );

/*++

Routine Description:

    This routine determines if two GUIDs are equal.

Arguments:

    FirstGuid - Supplies a pointer to the first GUID to compare.

    SecondGuid - Supplies a pointer to the second GUId to compare.

Return Value:

    TRUE if the two GUIDs have equal values.

    FALSE if the two GUIDs are not the same.

--*/

//
// Memory functions
//

KSTATUS
BopEfiInitializeMemory (
    VOID
    );

/*++

Routine Description:

    This routine initializes memory services for the boot loader.

Arguments:

    None.

Return Value:

    Status code.

--*/

VOID
BopEfiDestroyMemory (
    VOID
    );

/*++

Routine Description:

    This routine cleans up memory services upon failure.

Arguments:

    None.

Return Value:

    None.

--*/

KSTATUS
BopEfiLoaderAllocatePages (
    PULONGLONG Address,
    ULONGLONG Size,
    MEMORY_TYPE MemoryType
    );

/*++

Routine Description:

    This routine allocates physical pages for use.

Arguments:

    Address - Supplies a pointer to where the allocation will be returned.

    Size - Supplies the size of the required space, in bytes.

    MemoryType - Supplies the type of memory to mark the allocation as.

Return Value:

    STATUS_SUCCESS if the allocation was successful.

    STATUS_INVALID_PARAMETER if a page count of 0 was passed or the address
        parameter was not filled out.

    STATUS_NO_MEMORY if the allocation request could not be filled.

--*/

KSTATUS
BopEfiSynchronizeMemoryMap (
    PUINTN Key
    );

/*++

Routine Description:

    This routine synchronizes the EFI memory map with the boot memory map.

Arguments:

    Key - Supplies a pointer where the latest EFI memory map key will be
        returned.

Return Value:

    Status code.

--*/

KSTATUS
BopEfiVirtualizeFirmwareServices (
    UINTN MemoryMapSize,
    UINTN DescriptorSize,
    UINT32 DescriptorVersion,
    EFI_MEMORY_DESCRIPTOR *VirtualMap
    );

/*++

Routine Description:

    This routine changes the runtime addressing mode of EFI firmware from
    physical to virtual.

Arguments:

    MemoryMapSize - Supplies the size of the virtual map.

    DescriptorSize - Supplies the size of an entry in the virtual map.

    DescriptorVersion - Supplies the version of the structure entries in the
        virtual map.

    VirtualMap - Supplies the array of memory descriptors which contain the
        new virtual address mappings for all runtime ranges.

Return Value:

    Status code.

--*/

KSTATUS
BopEfiGetAllocatedMemoryMap (
    UINTN *MemoryMapSize,
    EFI_MEMORY_DESCRIPTOR **MemoryMap,
    UINTN *MapKey,
    UINTN *DescriptorSize,
    UINT32 *DescriptorVersion
    );

/*++

Routine Description:

    This routine returns the current memory map.

Arguments:

    MemoryMapSize - Supplies a pointer where the size in bytes of the map
        buffer will be returned.

    MemoryMap - Supplies a pointer where a pointer to the memory map will be
        returned on success. The caller is responsible for freeing this memory.

    MapKey - Supplies a pointer where the map key will be returned on success.

    DescriptorSize - Supplies a pointer where the firmware returns the size of
        the EFI_MEMORY_DESCRIPTOR structure.

    DescriptorVersion - Supplies a pointer where the firmware returns the
        version number associated with the EFI_MEMORY_DESCRIPTOR structure.

Return Value:

    Status code.

--*/

//
// Disk functions
//

KSTATUS
BopEfiOpenBootDisk (
    PHANDLE Handle
    );

/*++

Routine Description:

    This routine attempts to open the boot disk, the disk from which to load
    the OS.

Arguments:

    Handle - Supplies a pointer where a handle to the disk will be returned.

Return Value:

    Status code.

--*/

KSTATUS
BopEfiOpenPartition (
    UCHAR PartitionId[FIRMWARE_PARTITION_ID_SIZE],
    PHANDLE Handle
    );

/*++

Routine Description:

    This routine opens a handle to a disk and partition with the given IDs.

Arguments:

    PartitionId - Supplies the partition identifier to match against.

    Handle - Supplies a pointer where a handle to the opened disk will be
        returned upon success.

Return Value:

    Status code.

--*/

VOID
BopEfiCloseDisk (
    HANDLE DiskHandle
    );

/*++

Routine Description:

    This routine closes an open disk.

Arguments:

    DiskHandle - Supplies a pointer to the open disk handle.

Return Value:

    None.

--*/

KSTATUS
BopEfiLoaderBlockIoRead (
    HANDLE DiskHandle,
    ULONGLONG Sector,
    ULONG SectorCount,
    PVOID Buffer
    );

/*++

Routine Description:

    This routine uses firmware calls read sectors off of a disk.

Arguments:

    DiskHandle - Supplies a handle to the disk to read from.

    Sector - Supplies the zero-based sector number to read from.

    SectorCount - Supplies the number of sectors to read. The supplied buffer
        must be at least this large.

    Buffer - Supplies the buffer where the data read from the disk will be
        returned upon success.

Return Value:

    STATUS_SUCCESS if the operation completed successfully.

    STATUS_FIRMWARE_ERROR if the firmware returned an error.

    Other error codes.

--*/

KSTATUS
BopEfiLoaderBlockIoWrite (
    HANDLE DiskHandle,
    ULONGLONG Sector,
    ULONG SectorCount,
    PVOID Buffer
    );

/*++

Routine Description:

    This routine uses firmware calls to write sectors to a disk.

Arguments:

    DiskHandle - Supplies a handle to the disk to write to.

    Sector - Supplies the zero-based sector number to write to.

    SectorCount - Supplies the number of sectors to write. The supplied buffer
        must be at least this large.

    Buffer - Supplies the buffer containing the data to write to the disk.

Return Value:

    STATUS_SUCCESS if the operation completed successfully.

    STATUS_FIRMWARE_ERROR if the firmware returned an error.

    Other error codes.

--*/

ULONG
BopEfiGetDiskBlockSize (
    HANDLE DiskHandle
    );

/*++

Routine Description:

    This routine determines the number of bytes in a sector on the given disk.

Arguments:

    DiskHandle - Supplies a handle to the disk to query.

Return Value:

    Returns the number of bytes in a sector on success.

    0 on error.

--*/

ULONGLONG
BopEfiGetDiskBlockCount (
    HANDLE DiskHandle
    );

/*++

Routine Description:

    This routine determines the number of sectors on the disk.

Arguments:

    DiskHandle - Supplies a handle to the disk to query.

Return Value:

    Returns the number of sectors in the disk on success.

    0 on error.

--*/

KSTATUS
BopEfiGetRamDisks (
    PBOOT_RAM_DISK *RamDisks,
    PULONG RamDiskCount
    );

/*++

Routine Description:

    This routine returns an array of the RAM disks known to the firmware.

Arguments:

    RamDisks - Supplies a pointer where an array of RAM disk structures will
        be allocated and returned. It is the caller's responsibility to free
        this memory.

    RamDiskCount - Supplies a pointer where the count of RAM disks in the
        array will be returned.

Return Value:

    Status code.

--*/

//
// Video services
//

VOID
BopEfiInitializeVideo (
    VOID
    );

/*++

Routine Description:

    This routine initializes UEFI video services. Failure here is not fatal.

Arguments:

    None.

Return Value:

    None.

--*/

KSTATUS
BopEfiGetVideoInformation (
    PULONG ResolutionX,
    PULONG ResolutionY,
    PULONG PixelsPerScanLine,
    PULONG BitsPerPixel,
    PULONG RedMask,
    PULONG GreenMask,
    PULONG BlueMask,
    PPHYSICAL_ADDRESS FrameBufferBase,
    PULONGLONG FrameBufferSize
    );

/*++

Routine Description:

    This routine returns information about the video frame buffer.

Arguments:

    ResolutionX - Supplies a pointer where the horizontal resolution in pixels
        will be returned on success.

    ResolutionY - Supplies a pointer where the vertical resolution in pixels
        will be returned on success.

    PixelsPerScanLine - Supplies a pointer where the number of pixels per scan
        line will be returned on success.

    BitsPerPixel - Supplies a pointer where the number of bits per pixel will
        be returned on success.

    RedMask - Supplies a pointer where the mask of bits corresponding to the
        red channel will be returned on success. It is assumed this will be a
        contiguous chunk of bits.

    GreenMask - Supplies a pointer where the mask of bits corresponding to the
        green channel will be returned on success. It is assumed this will be a
        contiguous chunk of bits.

    BlueMask - Supplies a pointer where the mask of bits corresponding to the
        blue channel will be returned on success. It is assumed this will be a
        contiguous chunk of bits.

    FrameBufferBase - Supplies a pointer where the physical base address of the
        frame buffer will be returned on success.

    FrameBufferSize - Supplies a pointer where the size of the frame buffer in
        bytes will be returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_CONFIGURED if video services could not be initialized.

--*/

VOID
BopEfiGetDebugDevice (
    VOID
    );

/*++

Routine Description:

    This routine searches for the Serial I/O protocol and enumerates a debug
    devices with it if found.

Arguments:

    None.

Return Value:

    None. Failure is not fatal.

--*/

//
// Time services
//

KSTATUS
BopEfiGetCurrentTime (
    PSYSTEM_TIME Time
    );

/*++

Routine Description:

    This routine attempts to get the current system time.

Arguments:

    Time - Supplies a pointer where the current time will be returned.

Return Value:

    Status code.

--*/

