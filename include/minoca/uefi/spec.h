/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    spec.h

Abstract:

    This header contains the primary UEFI specification definitions. Protocol
    definitions are defined elsewhere.

Author:

    Evan Green 7-Feb-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/uefi/protocol/devpath.h>
#include <minoca/uefi/protocol/stextin.h>
#include <minoca/uefi/protocol/stextinx.h>
#include <minoca/uefi/protocol/stextout.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define EFI variable attributes.
//

#define EFI_VARIABLE_NON_VOLATILE       0x00000001
#define EFI_VARIABLE_BOOTSERVICE_ACCESS 0x00000002
#define EFI_VARIABLE_RUNTIME_ACCESS     0x00000004

//
// Define the attribute for a hardware error record. This attribute is
// identified by the mnemonic 'HR' elsewhere in the specification.
//

#define EFI_VARIABLE_HARDWARE_ERROR_RECORD 0x00000008

//
// Define attributes of the Authenticated Variable.
//

#define EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS            0x00000010
#define EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS 0x00000020
#define EFI_VARIABLE_APPEND_WRITE                          0x00000040

//
// Define flags for the daylight saving member of the EFI_TIME structure.
//

#define EFI_TIME_ADJUST_DAYLIGHT 0x01
#define EFI_TIME_IN_DAYLIGHT     0x02

//
// Define the unspecified timezone value used in the time zone member of the
// EFI_TIME structure.
//

#define EFI_UNSPECIFIED_TIMEZONE 0x07FF

//
// Define memory cacheability attributes.
//

#define EFI_MEMORY_UC  0x0000000000000001ULL
#define EFI_MEMORY_WC  0x0000000000000002ULL
#define EFI_MEMORY_WT  0x0000000000000004ULL
#define EFI_MEMORY_WB  0x0000000000000008ULL
#define EFI_MEMORY_UCE 0x0000000000000010ULL

//
// Define physical memory protection attributes.
//

#define EFI_MEMORY_WP   0x0000000000001000ULL
#define EFI_MEMORY_RP   0x0000000000002000ULL
#define EFI_MEMORY_XP   0x0000000000004000ULL

//
// Define runtime memory attributes.
//

#define EFI_MEMORY_RUNTIME 0x8000000000000000ULL

//
// Define memory descriptor version number.
//

#define EFI_MEMORY_DESCRIPTOR_VERSION 1

//
// Define the DebugDisposition values for the ConvertPointer service.
//

#define EFI_OPTIONAL_PTR 0x00000001

//
// Define flags for events.
//

#define EVT_TIMER                         0x80000000
#define EVT_RUNTIME                       0x40000000
#define EVT_NOTIFY_WAIT                   0x00000100
#define EVT_NOTIFY_SIGNAL                 0x00000200

#define EVT_SIGNAL_EXIT_BOOT_SERVICES     0x00000201
#define EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE 0x60000202

//
// This flag is set if the notify context pointer is a runtime memory address.
// This event is deprecated in UEFI2.0 and later specifications.
//

#define EVT_RUNTIME_CONTEXT               0x20000000

//
// Define task priority levels.
//

#define TPL_APPLICATION 4
#define TPL_CALLBACK    8
#define TPL_NOTIFY      16
#define TPL_HIGH_LEVEL  31

//
// Define flags to the open protocol function.
//

#define EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL  0x00000001
#define EFI_OPEN_PROTOCOL_GET_PROTOCOL        0x00000002
#define EFI_OPEN_PROTOCOL_TEST_PROTOCOL       0x00000004
#define EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER 0x00000008
#define EFI_OPEN_PROTOCOL_BY_DRIVER           0x00000010
#define EFI_OPEN_PROTOCOL_EXCLUSIVE           0x00000020

//
// Define update capsule flags.
//

#define CAPSULE_FLAGS_PERSIST_ACROSS_RESET  0x00010000
#define CAPSULE_FLAGS_POPULATE_SYSTEM_TABLE 0x00020000
#define CAPSULE_FLAGS_INITIATE_RESET        0x00040000

//
// Define OS indications for update capsules.
//

#define EFI_OS_INDICATIONS_BOOT_TO_FW_UI                   0x0000000000000001
#define EFI_OS_INDICATIONS_TIMESTAMP_REVOCATION            0x0000000000000002
#define EFI_OS_INDICATIONS_FILE_CAPSULE_DELIVERY_SUPPORTED 0x0000000000000004
#define EFI_OS_INDICATIONS_FMP_CAPSULE_SUPPORTED           0x0000000000000008
#define EFI_OS_INDICATIONS_CAPSULE_RESULT_VAR_SUPPORTED    0x0000000000000010

//
// Definitions used in the EFI Runtime Services Table
//

#define EFI_SYSTEM_TABLE_SIGNATURE     0x5453595320494249ULL
#define EFI_2_40_SYSTEM_TABLE_REVISION ((2 << 16) | (40))
#define EFI_2_31_SYSTEM_TABLE_REVISION ((2 << 16) | (31))
#define EFI_2_30_SYSTEM_TABLE_REVISION ((2 << 16) | (30))
#define EFI_2_20_SYSTEM_TABLE_REVISION ((2 << 16) | (20))
#define EFI_2_10_SYSTEM_TABLE_REVISION ((2 << 16) | (10))
#define EFI_2_00_SYSTEM_TABLE_REVISION ((2 << 16) | (00))
#define EFI_1_10_SYSTEM_TABLE_REVISION ((1 << 16) | (10))
#define EFI_1_02_SYSTEM_TABLE_REVISION ((1 << 16) | (02))
#define EFI_SYSTEM_TABLE_REVISION      EFI_2_40_SYSTEM_TABLE_REVISION
#define EFI_SPECIFICATION_VERSION      EFI_SYSTEM_TABLE_REVISION

#define EFI_RUNTIME_SERVICES_SIGNATURE 0x56524553544E5552ULL
#define EFI_RUNTIME_SERVICES_REVISION  EFI_SPECIFICATION_VERSION

//
// Definitions used in the EFI Boot Services Table
//

#define EFI_BOOT_SERVICES_SIGNATURE 0x56524553544F4F42ULL
#define EFI_BOOT_SERVICES_REVISION  EFI_SPECIFICATION_VERSION

//
// Define EFI Load Options Attributes.
//

#define LOAD_OPTION_ACTIVE            0x00000001
#define LOAD_OPTION_FORCE_RECONNECT   0x00000002
#define LOAD_OPTION_HIDDEN            0x00000008
#define LOAD_OPTION_CATEGORY          0x00001F00

#define LOAD_OPTION_CATEGORY_BOOT     0x00000000
#define LOAD_OPTION_CATEGORY_APP      0x00000100

#define EFI_BOOT_OPTION_SUPPORT_KEY   0x00000001
#define EFI_BOOT_OPTION_SUPPORT_APP   0x00000002
#define EFI_BOOT_OPTION_SUPPORT_COUNT 0x00000300

//
// Define EFI File location to boot from on removable media devices.
//

#define EFI_REMOVABLE_MEDIA_FILE_NAME_IA32    L"\\EFI\\BOOT\\BOOTIA32.EFI"
#define EFI_REMOVABLE_MEDIA_FILE_NAME_IA64    L"\\EFI\\BOOT\\BOOTIA64.EFI"
#define EFI_REMOVABLE_MEDIA_FILE_NAME_X64     L"\\EFI\\BOOT\\BOOTX64.EFI"
#define EFI_REMOVABLE_MEDIA_FILE_NAME_ARM     L"\\EFI\\BOOT\\BOOTARM.EFI"
#define EFI_REMOVABLE_MEDIA_FILE_NAME_AARCH64 L"\\EFI\\BOOT\\BOOTAA64.EFI"

#if defined (EFI_X86)

#define EFI_REMOVABLE_MEDIA_FILE_NAME EFI_REMOVABLE_MEDIA_FILE_NAME_IA32

#elif defined (EFI_X64)

#define EFI_REMOVABLE_MEDIA_FILE_NAME EFI_REMOVABLE_MEDIA_FILE_NAME_X64

#elif defined (EFI_ARM)

#define EFI_REMOVABLE_MEDIA_FILE_NAME EFI_REMOVABLE_MEDIA_FILE_NAME_ARM

#elif defined (EFI_AARCH64)

#define EFI_REMOVABLE_MEDIA_FILE_NAME EFI_REMOVABLE_MEDIA_FILE_NAME_AARCH64

#else

#error Unknown Processor Type

#endif

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Enumeration Description:

    This enumeration describes the memory types introduced in UEFI.

Values:

    EfiReservedMemoryType - Indicates a reserved type that is not used.

    EfiLoaderCode - Indicates code portions of a loaded application. OS Loaders
        are UEFI applications.

    EfiLoaderData - Indicates data portions of a loaded application and the
        default allocation type used by an application to allocate pool memory.

    EfiBootServicesCode - Indicates the code portions of a loaded Boot Services
        Driver.

    EfiBootServicesData - Indicates the data portions of a loaded Boot Services
        Driver, and the default data allocation type used by a Boot Services
        Driver to allocate pool memory.

    EfiRuntimeServicesCode - Indicates the code portions of a loaded Runtime
        Services Driver.

    EfiRuntimeServicesData - Indicates the data portions of a loaded Runtime
        Services Driver and the default data allocation type used by a Runtime
        Services Driver to allocate pool memory.

    EfiConventionalMemory - Indicates free unallocated memory.

    EfiUnusableMemory - Indicates memory in which errors have been detected.

    EfiACPIReclaimMemory - Indicates memory that holds ACPI tables.

    EfiACPIMemoryNVS - Indicates address space reserved for use by the
        firmware.

    EfiMemoryMappedIO - Indicates a region used by system firmware to request
        that a memory-mapped I/O region be mapped by the OS to a virtual
        address so it can be accessed by EFI runtime services.

    EfiMemoryMappedIOPortSpace - Indicates a system memory-mapped I/O region
        that is used to translate memory cycles to I/O cycles by the processor.

    EfiPalCode - Indicates address space reserved by the firmware for code that
        is part of the processor.

    EfiMaxMemoryType - Indicates a type used to indicate the boundary of valid
        values.

--*/

typedef enum {
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiUnusableMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMappedIO,
    EfiMemoryMappedIOPortSpace,
    EfiPalCode,
    EfiMaxMemoryType
} EFI_MEMORY_TYPE;

/*++

Enumeration Description:

    This enumeration describes the allocation strategies that can be employed
    when allocating pages from UEFI.

Values:

    AllocateAnyPages - Indicates to use any available pages that satisfy the
        request.

    AllocateMaxAddress - Indicates to allocate any range of pages whose
        uppermost address is less than or equal to the specified maximum
        address.

    AllocateAddress - Indicates to allocate pages at a specified address.

    MaxAllocateType - Indicates the maximum enumeration value, used only for
        bounds checking.

--*/

typedef enum {
    AllocateAnyPages,
    AllocateMaxAddress,
    AllocateAddress,
    MaxAllocateType
} EFI_ALLOCATE_TYPE;

/*++

Enumeration Description:

    This enumeration describes timer delay types.

Values:

    TimerCancel - Indicates an event should be cancelled.

    TimerPeriodic - Indicates the event is to be signaled periodically at a
        specified interval from the current time.

    TimerRelative - Indicates the event is to be singaled once at a specified
        interval from the current time.

--*/

typedef enum {
    TimerCancel,
    TimerPeriodic,
    TimerRelative
} EFI_TIMER_DELAY;

/*++

Enumeration Description:

    This enumeration describes system reset types.

Values:

    EfiResetCold - Indicates a system-wide reset. This sets all circuitry
        within the system to its initial state. This type of reset is
        asynchronous to system operation and operates without regard to cycle
        boundaries.

    EfiResetWarm - Indicates a system-wide initialization. The processors are
        set to their initial state, and pending cycles are not corrupted. If
        the system does not support this type, then a cold reset must be
        performed.

    EfiResetShutdown - Indicates entry into a power state equivalent to the
        ACPI G2/S5 or G3 state. If the system does not support this reset type,
        then when the system is rebooted, it should exhibit the cold reset
        attributes.

    EfiResetPlatformSpecific - Indicates a system-wide reset. The exact type of
        of reset is defined by the EFI_GUID that follows the null-terminated
        unicode string passed into the reset data. If a platform does not
        recognize the GUID in the reset data the platform must pick a supported
        reset type to perform. The platform may optionally log the parameters
        from any non-normal reset that occurs.

--*/

typedef enum {
    EfiResetCold,
    EfiResetWarm,
    EfiResetShutdown,
    EfiResetPlatformSpecific
} EFI_RESET_TYPE;

/*++

Enumeration Description:

    This enumeration describes EFI interface types.

Values:

    EFI_INTERFACE_NATIVE - Indicates the supplied protocol interface is
        supplied in native form.

--*/

typedef enum {
    EFI_NATIVE_INTERFACE
} EFI_INTERFACE_TYPE;

/*++

Enumeration Description:

    This enumeration describes the EFI locate search types.

Values:

    AllHandles - Indicates to retrieve all handles in the handle database.

    ByRegisterNotify - Indicates to retrieve the next handle from a
        RegisterProtocolNotify event.

    ByProtocol - Indicates to retrieve the set of handles that support a
        specified protocol.

--*/

typedef enum {
    AllHandles,
    ByRegisterNotify,
    ByProtocol
} EFI_LOCATE_SEARCH_TYPE;

/*++

Structure Description:

    This structure describes the standard header that precedes all EFI tables.

Members:

    Signature - Stores a 64-bit signature that identifies the type of table
        that follows.

    Revision - Stores the revision of the EFI specification to which this table
        conforms. The upper 16 bits contain the major revision, and the lower
        16 bits contain the minor revision value. The minor revision values
        are limited to the range of 0 to 99, inclusive.

    HeaderSize - Stores the size, in bytes, of the entire table, including this
        header structure.

    CRC32 - Stores the 32-bit CRC for the entire table (whose length is defined
        in the header size field). This field is set to 0 during computation.

    Reserved - Stores a reserved field that must be set to zero.

--*/

typedef struct {
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

/*++

Structure Description:

    This structure defines an EFI memory descriptor.

Members:

    Type - Stores the type of memory. See the EFI_MEMORY_TYPE enum.

    Padding - Stores some padding to make the next member 8-byte aligned.

    PhysicalStart - Stores the physical address of the first byte of the memory
        region. This must be aligned on an EFI_PAGE_SIZE boundary.

    VirtualStart - Stores the virtual address of the first byte of the memory
        region. This must be aligned on an EFI_PAGE_SIZE boundary.

    NumberOfPages - Stores the number of pages described by the descriptor.

    Attribute - Stores a bitfield of memory attributes for that memory region.
        This is not necessarily the current settings for that region.

--*/

typedef struct {
    UINT32 Type;
    UINT32 Padding;
    EFI_PHYSICAL_ADDRESS PhysicalStart;
    EFI_VIRTUAL_ADDRESS VirtualStart;
    UINT64 NumberOfPages;
    UINT64 Attribute;
} EFI_MEMORY_DESCRIPTOR;

/*++

Structure Description:

    This structure describes the capabilities of the real time clock device
    backing the get/set time services.

Members:

    Resolution - Stores the reporting resolution of the real-time clock device
        in ticks per second. For a PC-AT CMOS RTC device, this value would be
        1 Hz to indicate that the device reports timing to the nearest second.

    Accuracy - Stores the timekeeping accuracy of the real-time clock in an
        error rate of 1E-6 parts per million. For a clock with an accuracy of
        50 parts per million, the value in this field would be 50 million.

    SetsToZero - Stores a boolean indicating that a time set operation clears
        the device's time below the reported resolution. If FALSE, the state
        below the reported resolution is not cleared when the time is set.
        PC-AT CMOS RTC devices set this value to FALSE.

--*/

typedef struct {
    UINT32 Resolution;
    UINT32 Accuracy;
    BOOLEAN SetsToZero;
} EFI_TIME_CAPABILITIES;

/*++

Structure Description:

    This structure describes information about an open handle.

Members:

    AgentHandle - Stores the handle of the agent that opened the handle.

    ControllerHandle - Stores the controller handle associated with the
        protocol.

    Attributes - Stores the attributes the handle was opened with.

    OpenCount - Stores the count of outstanding opens on these attributes.

--*/

typedef struct {
    EFI_HANDLE AgentHandle;
    EFI_HANDLE ControllerHandle;
    UINT32 Attributes;
    UINT32 OpenCount;
} EFI_OPEN_PROTOCOL_INFORMATION_ENTRY;

/*++

Structure Description:

    This structure describes an EFI capsule block descriptor.

Members:

    Length - Stores the length in bytes of the data pointed to by the
        data block or continuation pointer.

    DataBlock - Stores the physical address of the data block. This is used if
        the length is not zero.

    ContinuationPointer - Stores the physical address of another capsule block
        descriptor. This is used if the length is equal to zero. If this
        pointer is zero then this represents the end of the list.

--*/

typedef struct {
    UINT64 Length;
    union {
        EFI_PHYSICAL_ADDRESS DataBlock;
        EFI_PHYSICAL_ADDRESS ContinuationPointer;
    } Union;

} EFI_CAPSULE_BLOCK_DESCRIPTOR;

/*++

Structure Description:

    This structure describes an EFI capsule header.

Members:

    CapsuleGuid - Stores the GUID that defines the contents of the capsule.

    HeaderSize - Stores the size of the header, which can be bigger than the
        size of this structure if there are extended header entries.

    Flags - Stores a bitfield of flags describing the capsule attributes. The
        flag values 0x0000 - 0xFFFF are defined by the capsule GUID. Flag
        values of 0x10000 - 0xFFFFFFFF are defined by this specification.

    CapsuleImageSize - Stores the size in bytes of the capsule.

--*/

typedef struct {
    EFI_GUID CapsuleGuid;
    UINT32 HeaderSize;
    UINT32 Flags;
    UINT32 CapsuleImageSize;
} EFI_CAPSULE_HEADER;

/*++

Structure Description:

    This structure an EFI capsule table. The EFI System Table entry must point
    to an array of capsules that contain the same capsule GUID value. The array
    must be prefixed by a UINT32 that represents the size of the array of
    capsules.

Members:

    CapsuleArrayNumber - Stores the size of the array of capsules.

    CapsulePtr - Stores an array of capsules that contain the same capsule
        GUID value.

--*/

typedef struct {
    UINT32   CapsuleArrayNumber;
    VOID*    CapsulePtr[1];
} EFI_CAPSULE_TABLE;

typedef
EFI_STATUS
(EFIAPI *EFI_ALLOCATE_PAGES) (
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

typedef
EFI_STATUS
(EFIAPI *EFI_FREE_PAGES) (
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

typedef
EFI_STATUS
(EFIAPI *EFI_GET_MEMORY_MAP) (
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

typedef
EFI_STATUS
(EFIAPI *EFI_ALLOCATE_POOL) (
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

typedef
EFI_STATUS
(EFIAPI *EFI_FREE_POOL) (
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

typedef
EFI_STATUS
(EFIAPI *EFI_SET_VIRTUAL_ADDRESS_MAP) (
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

    EFI_SUCCESS on success.

    EFI_UNSUPPORTED if the firmware is not at runtime, or the firmware is
    already in virtual address mapped mode.

    EFI_INVALID_PARAMETER if the descriptor size or version is invalid.

    EFI_NO_MAPPING if the virtual address was not supplied for a range in the
    memory map that requires a mapping.

    EFI_NOT_FOUND if a virtual address was supplied for an address that is not
    found in the memory map.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_CONNECT_CONTROLLER) (
    EFI_HANDLE ControllerHandle,
    EFI_HANDLE *DriverImageHandle,
    EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath,
    BOOLEAN Recursive
    );

/*++

Routine Description:

    This routine connects one or more drivers to a controller.

Arguments:

    ControllerHandle - Supplies the handle of the controller which driver(s)
        are connecting to.

    DriverImageHandle - Supplies a pointer to an ordered list of handles that
        support the EFI_DRIVER_BINDING_PROTOCOL.

    RemainingDevicePath - Supplies an optional pointer to the device path that
        specifies a child of the controller specified by the controller handle.

    Recursive - Supplies a boolean indicating if this routine should be called
        recursively until the entire tree of controllers below the specified
        controller has been connected. If FALSE, then the tree of controllers
        is only expanded one level.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the controller handle is NULL.

    EFI_NOT_FOUND if either there are no EFI_DRIVER_BINDING_PROTOCOL instances
    present in the system, or no drivers were connected to the controller
    handle.

    EFI_SECURITY_VIOLATION if the user has no permission to start UEFI device
    drivers on the device associated with the controller handle or specified
    by the remaining device path.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_DISCONNECT_CONTROLLER) (
    EFI_HANDLE ControllerHandle,
    EFI_HANDLE DriverImageHandle,
    EFI_HANDLE ChildHandle
    );

/*++

Routine Description:

    This routine disconnects one or more drivers to a controller.

Arguments:

    ControllerHandle - Supplies the handle of the controller which driver(s)
        are disconnecting from.

    DriverImageHandle - Supplies an optional pointer to the driver to
        disconnect from the controller. If NULL, all drivers are disconnected.

    ChildHandle - Supplies an optional pointer to the handle of the child to
        destroy.

Return Value:

    EFI_SUCCESS if one or more drivers were disconnected, no drivers are
    managing the handle, or a driver image handle was supplied and it is not
    controlling the given handle.

    EFI_INVALID_PARAMETER if the controller handle or driver handle is not a
    valid EFI handle, or the driver image handle doesn't support the
    EFI_DRIVER_BINDING_PROTOCOL.

    EFI_OUT_OF_RESOURCES if there are not enough resources are available to
    disconnect the controller(s).

    EFI_DEVICE_ERROR if the controller could not be disconnected because of a
    device error.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_CONVERT_POINTER) (
    UINTN DebugDisposition,
    VOID **Address
    );

/*++

Routine Description:

    This routine determines the new virtual address that is to be used on
    subsequent memory accesses.

Arguments:

    DebugDisposition - Supplies type information for the pointer being
        converted.

    Address - Supplies a pointer to a pointer that is to be fixed to be the
        value needed for the new virtual address mappings being applied.

Return Value:

    EFI_SUCCESS if the pointer was modified.

    EFI_INVALID_PARAMETER if the address is NULL or the value of Address is
    NULL and the debug disposition does not have the EFI_OPTIONAL_PTR bit set.

    EFI_NOT_FOUND if the pointer pointed to by the address parameter was not
    found to be part of the current memory map. This is normally fatal.

--*/

typedef
VOID
(EFIAPI *EFI_EVENT_NOTIFY) (
    EFI_EVENT Event,
    VOID *Context
    );

/*++

Routine Description:

    This routine invokes a notification event.

Arguments:

    Event - Supplies a pointer to the event to invoke.

    Context - Supplies a pointer to the notification function's context.

Return Value:

    None.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_CREATE_EVENT) (
    UINT32 Type,
    EFI_TPL NotifyTpl,
    EFI_EVENT_NOTIFY NotifyFunction,
    VOID *NotifyContext,
    EFI_EVENT *Event
    );

/*++

Routine Description:

    This routine creates an event.

Arguments:

    Type - Supplies the type of event to create, as well as its mode and
        attributes.

    NotifyTpl - Supplies an optional task priority level of event notifications.

    NotifyFunction - Supplies an optional pointer to the event's notification
        function.

    NotifyContext - Supplies an optional context pointer that will be passed
        to the notify function when the event is signaled.

    Event - Supplies a pointer where the new event will be returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if one or more parameters are not valid.

    EFI_OUT_OF_RESOURCES if memory could not be allocated.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_CREATE_EVENT_EX) (
    UINT32 Type,
    EFI_TPL NotifyTpl,
    EFI_EVENT_NOTIFY NotifyFunction,
    VOID *NotifyContext,
    EFI_GUID *EventGroup,
    EFI_EVENT *Event
    );

/*++

Routine Description:

    This routine creates an event.

Arguments:

    Type - Supplies the type of event to create, as well as its mode and
        attributes.

    NotifyTpl - Supplies an optional task priority level of event notifications.

    NotifyFunction - Supplies an optional pointer to the event's notification
        function.

    NotifyContext - Supplies an optional context pointer that will be passed
        to the notify function when the event is signaled.

    EventGroup - Supplies an optional pointer to the unique identifier of the
        group to which this event belongs. If this is NULL, the function
        behaves as if the parameters were passed to the original create event
        function.

    Event - Supplies a pointer where the new event will be returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if one or more parameters are not valid.

    EFI_OUT_OF_RESOURCES if memory could not be allocated.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_SET_TIMER) (
    EFI_EVENT Event,
    EFI_TIMER_DELAY Type,
    UINT64 TriggerTime
    );

/*++

Routine Description:

    This routine sets the type of timer and trigger time for a timer event.

Arguments:

    Event - Supplies the timer to set.

    Type - Supplies the type of trigger to set.

    TriggerTime - Supplies the number of 100ns units until the timer expires.
        Zero is legal, and means the timer will be signaled on the next timer
        tick.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the event or type is not valid.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_SIGNAL_EVENT) (
    EFI_EVENT Event
    );

/*++

Routine Description:

    This routine signals an event.

Arguments:

    Event - Supplies the event to signal.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the given event is not valid.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_WAIT_FOR_EVENT) (
    UINTN NumberOfEvents,
    EFI_EVENT *Event,
    UINTN *Index
    );

/*++

Routine Description:

    This routine stops execution until an event is signaled.

Arguments:

    NumberOfEvents - Supplies the number of events in the event array.

    Event - Supplies the array of EFI_EVENTs.

    Index - Supplies a pointer where the index of the event which satisfied the
        wait will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the number of events is zero, or the event
    indicated by the index return parameter is of type EVT_NOTIFY_SIGNAL.

    EFI_UNSUPPORTED if the current TPL is not TPL_APPLICATION.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_CLOSE_EVENT) (
    EFI_EVENT Event
    );

/*++

Routine Description:

    This routine closes an event.

Arguments:

    Event - Supplies the event to close.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the given event is invalid.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_CHECK_EVENT) (
    EFI_EVENT Event
    );

/*++

Routine Description:

    This routine checks whether or not an event is in the signaled state.

Arguments:

    Event - Supplies the event to check.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_READY if the event is not signaled.

    EFI_INVALID_PARAMETER if the event is of type EVT_NOTIFY_SIGNAL.

--*/

typedef
EFI_TPL
(EFIAPI *EFI_RAISE_TPL) (
    EFI_TPL NewTpl
    );

/*++

Routine Description:

    This routine raises the current Task Priority Level.

Arguments:

    NewTpl - Supplies the new TPL to set.

Return Value:

    Returns the previous TPL.

--*/

typedef
VOID
(EFIAPI *EFI_RESTORE_TPL) (
    EFI_TPL OldTpl
    );

/*++

Routine Description:

    This routine restores the Task Priority Level back to its original value
    before it was raised.

Arguments:

    OldTpl - Supplies the original TPL to restore back to.

Return Value:

    None.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_GET_VARIABLE) (
    CHAR16 *VariableName,
    EFI_GUID *VendorGuid,
    UINT32 *Attributes,
    UINTN *DataSize,
    VOID *Data
    );

/*++

Routine Description:

    This routine returns the value of a variable.

Arguments:

    VariableName - Supplies a pointer to a null-terminated string containing
        the name of the vendor's variable.

    VendorGuid - Supplies a pointer to the unique GUID for the vendor.

    Attributes - Supplies an optional pointer where the attribute mask for the
        variable will be returned.

    DataSize - Supplies a pointer that on input contains the size of the data
        buffer. On output, the actual size of the data will be returned.

    Data - Supplies a pointer where the variable value will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if the variable was not found.

    EFI_BUFFER_TOO_SMALL if the supplied buffer is not big enough.

    EFI_INVALID_PARAMETER if the variable name, vendor GUID, or data size is
    NULL.

    EFI_DEVICE_ERROR if a hardware error occurred trying to read the variable.

    EFI_SECURITY_VIOLATION if the variable could not be retrieved due to an
    authentication failure.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_GET_NEXT_VARIABLE_NAME) (
    UINTN *VariableNameSize,
    CHAR16 *VariableName,
    EFI_GUID *VendorGuid
    );

/*++

Routine Description:

    This routine enumerates the current variable names.

Arguments:

    VariableNameSize - Supplies a pointer that on input contains the size of
        the variable name buffer. On output, will contain the size of the
        variable name.

    VariableName - Supplies a pointer that on input contains the last variable
        name that was returned. On output, returns the null terminated string
        of the current variable.

    VendorGuid - Supplies a pointer that on input contains the last vendor GUID
        returned by this routine. On output, returns the vendor GUID of the
        current variable.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if the next variable was not found.

    EFI_BUFFER_TOO_SMALL if the supplied buffer is not big enough.

    EFI_INVALID_PARAMETER if the variable name, vendor GUID, or data size is
    NULL.

    EFI_DEVICE_ERROR if a hardware error occurred trying to read the variable.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_SET_VARIABLE) (
    CHAR16 *VariableName,
    EFI_GUID *VendorGuid,
    UINT32 Attributes,
    UINTN DataSize,
    VOID *Data
    );

/*++

Routine Description:

    This routine sets the value of a variable.

Arguments:

    VariableName - Supplies a pointer to a null-terminated string containing
        the name of the vendor's variable. Each variable name is unique for a
        particular vendor GUID. A variable name must be at least one character
        in length.

    VendorGuid - Supplies a pointer to the unique GUID for the vendor.

    Attributes - Supplies the attributes for this variable. See EFI_VARIABLE_*
        definitions.

    DataSize - Supplies the size of the data buffer. Unless the
        EFI_VARIABLE_APPEND_WRITE, EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS, or
        EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS attribute is set, a
        size of zero causes the variable to be deleted. When the
        EFI_VARIABLE_APPEND_WRITE attribute is set, then a set variable call
        with a data size of zero will not cause any change to the variable
        value (the timestamp associated with the variable may be updated
        however even if no new data value is provided,see the description of
        the EFI_VARIABLE_AUTHENTICATION_2 descriptor below. In this case the
        data size will not be zero since the EFI_VARIABLE_AUTHENTICATION_2
        descriptor will be populated).

    Data - Supplies the contents of the variable.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if the variable being updated or deleted was not found.

    EFI_INVALID_PARAMETER if an invalid combination of attribute bits, name,
    and GUID was suplied, data size exceeds the maximum, or the variable name
    is an empty string.

    EFI_DEVICE_ERROR if a hardware error occurred trying to access the variable.

    EFI_WRITE_PROTECTED if the variable is read-only or cannot be deleted.

    EFI_SECURITY_VIOLATION if variable could not be written due to
    EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS or
    EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACESS being set, but the
    authorization information does NOT pass the validation check carried out by
    the firmware.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_GET_TIME) (
    EFI_TIME *Time,
    EFI_TIME_CAPABILITIES *Capabilities
    );

/*++

Routine Description:

    This routine returns the current time and dat information, and
    timekeeping capabilities of the hardware platform.

Arguments:

    Time - Supplies a pointer where the current time will be returned.

    Capabilities - Supplies an optional pointer where the capabilities will be
        returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the time parameter was NULL.

    EFI_DEVICE_ERROR if there was a hardware error accessing the device.

    EFI_UNSUPPORTED if the wakeup timer is not supported on this platform.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_SET_TIME) (
    EFI_TIME *Time
    );

/*++

Routine Description:

    This routine sets the current local time and date information.

Arguments:

    Time - Supplies a pointer to the time to set.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if a time field is out of range.

    EFI_DEVICE_ERROR if there was a hardware error accessing the device.

    EFI_UNSUPPORTED if the wakeup timer is not supported on this platform.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_GET_WAKEUP_TIME) (
    BOOLEAN *Enabled,
    BOOLEAN *Pending,
    EFI_TIME *Time
    );

/*++

Routine Description:

    This routine gets the current wake alarm setting.

Arguments:

    Enabled - Supplies a pointer that receives a boolean indicating if the
        alarm is currently enabled or disabled.

    Pending - Supplies a pointer that receives a boolean indicating if the
        alarm signal is pending and requires acknowledgement.

    Time - Supplies a pointer that receives the current wake time.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if any parameter is NULL.

    EFI_DEVICE_ERROR if there was a hardware error accessing the device.

    EFI_UNSUPPORTED if the wakeup timer is not supported on this platform.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_SET_WAKEUP_TIME) (
    BOOLEAN Enable,
    EFI_TIME *Time
    );

/*++

Routine Description:

    This routine sets the current wake alarm setting.

Arguments:

    Enable - Supplies a boolean enabling or disabling the wakeup timer.

    Time - Supplies an optional pointer to the time to set. This parameter is
        only optional if the enable parameter is FALSE.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if a time field is out of range.

    EFI_DEVICE_ERROR if there was a hardware error accessing the device.

    EFI_UNSUPPORTED if the wakeup timer is not supported on this platform.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_IMAGE_LOAD) (
    BOOLEAN BootPolicy,
    EFI_HANDLE ParentImageHandle,
    EFI_DEVICE_PATH_PROTOCOL *DevicePath,
    VOID *SourceBuffer,
    UINTN SourceSize,
    EFI_HANDLE *ImageHandle
    );

/*++

Routine Description:

    This routine loads an EFI image into memory.

Arguments:

    BootPolicy - Supplies a boolean indicating that the request originates
        from the boot manager, and that the boot manager is attempting to load
        the given file path as a boot selection. This is ignored if the source
        buffer is NULL.

    ParentImageHandle - Supplies the caller's image handle.

    DevicePath - Supplies a pointer to the device path from which the image is
        loaded.

    SourceBuffer - Supplies an optional pointer to the memory location
        containing a copy of the image to be loaded.

    SourceSize - Supplies the size in bytes of the source buffer.

    ImageHandle - Supplies a pointer where the loaded image handle will be
        returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if both the source buffer and device path are NULL.

    EFI_INVALID_PARAMETER if one or more parameters are not valid.

    EFI_UNSUPPORTED if the image type is unsupported.

    EFI_OUT_OF_RESOURCES if an allocation failed.

    EFI_LOAD_ERROR if the image format was corrupt or not understood.

    EFI_DEVICE_ERROR if the underlying device returned a read error.

    EFI_ACCESS_DENIED if the platform policy prohibits the image from being
    loaded.

    EFI_SECURITY_VIOLATION if the image was successfully loaded, but the
    platform policy indicates the image should not be started.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_IMAGE_START) (
    EFI_HANDLE ImageHandle,
    UINTN *ExitDataSize,
    CHAR16 **ExitData
    );

/*++

Routine Description:

    This routine transfers control to a loaded image's entry point.

Arguments:

    ImageHandle - Supplies the handle of the image to run.

    ExitDataSize - Supplies a pointer to the size, in bytes, of the exit data.

    ExitData - Supplies an optional pointer where a pointer will be returned
        that includes a null-terminated string, optionally followed by
        additional binary data.

Return Value:

    EFI_INVALID_PARAMETER if the image handle is invalid or the image has
    already been started.

    EFI_SECURITY_VIOLATION if the platform policy specifies the image should
    not be started.

    Otherwise, returns the exit code from the image.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_EXIT) (
    EFI_HANDLE ImageHandle,
    EFI_STATUS ExitStatus,
    UINTN ExitDataSize,
    CHAR16 *ExitData
    );

/*++

Routine Description:

    This routine terminates an loaded EFI image and returns control to boot
    services.

Arguments:

    ImageHandle - Supplies the handle of the image passed upon entry.

    ExitStatus - Supplies the exit code.

    ExitDataSize - Supplies the size of the exit data. This is ignored if the
        exit status code is EFI_SUCCESS.

    ExitData - Supplies an optional pointer where a pointer will be returned
        that includes a null-terminated string describing the reason the
        application exited, optionally followed by additional binary data. This
        buffer must be allocated from AllocatePool.

Return Value:

    EFI_SUCCESS if the image was unloaded.

    EFI_INVALID_PARAMETER if the image has been loaded and started with
    LoadImage and StartImage, but the image is not currently executing.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_IMAGE_UNLOAD) (
    EFI_HANDLE ImageHandle
    );

/*++

Routine Description:

    This routine unloads an image.

Arguments:

    ImageHandle - Supplies the handle of the image to unload.

    ExitStatus - Supplies the exit code.

    ExitDataSize - Supplies the size of the exit data. This is ignored if the
        exit status code is EFI_SUCCESS.

    ExitData - Supplies an optional pointer where a pointer will be returned
        that includes a null-terminated string describing the reason the
        application exited, optionally followed by additional binary data. This
        buffer must be allocated from AllocatePool.

Return Value:

    EFI_SUCCESS if the image was unloaded.

    EFI_INVALID_PARAMETER if the image handle is not valid.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_EXIT_BOOT_SERVICES) (
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

typedef
EFI_STATUS
(EFIAPI *EFI_STALL) (
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

typedef
EFI_STATUS
(EFIAPI *EFI_SET_WATCHDOG_TIMER) (
    UINTN Timeout,
    UINT64 WatchdogCode,
    UINTN DataSize,
    CHAR16 *WatchdogData
    );

/*++

Routine Description:

    This routine sets the system's watchdog timer.

Arguments:

    Timeout - Supplies the number of seconds to set the timer for.

    WatchdogCode - Supplies a numeric code to log on a watchdog timeout event.

    DataSize - Supplies the size of the watchdog data.

    WatchdogData - Supplies an optional buffer that includes a null-terminated
        string, optionally followed by additional binary data.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the supplied watchdog code is invalid.

    EFI_UNSUPPORTED if there is no watchdog timer.

    EFI_DEVICE_ERROR if an error occurred accessing the device hardware.

--*/

typedef
VOID
(EFIAPI *EFI_RESET_SYSTEM) (
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

typedef
EFI_STATUS
(EFIAPI *EFI_GET_NEXT_MONOTONIC_COUNT) (
    UINT64 *Count
    );

/*++

Routine Description:

    This routine returns a monotonically increasing count for the platform.

Arguments:

    Count - Supplies a pointer where the next count is returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the count is NULL.

    EFI_DEVICE_ERROR if the device is not functioning properly.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_GET_NEXT_HIGH_MONO_COUNT) (
    UINT32 *HighCount
    );

/*++

Routine Description:

    This routine returns the next high 32 bits of the platform's monotonic
    counter.

Arguments:

    HighCount - Supplies a pointer where the value is returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the count is NULL.

    EFI_DEVICE_ERROR if the device is not functioning properly.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_CALCULATE_CRC32) (
    VOID *Data,
    UINTN DataSize,
    UINT32 *Crc32
    );

/*++

Routine Description:

    This routine computes the 32-bit CRC for a data buffer.

Arguments:

    Data - Supplies a pointer to the buffer to compute the CRC on.

    DataSize - Supplies the size of the data buffer in bytes.

    Crc32 - Supplies a pointer where the 32-bit CRC will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if any parameter is NULL, or the data size is zero.

--*/

typedef
VOID
(EFIAPI *EFI_COPY_MEM) (
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

typedef
VOID
(EFIAPI *EFI_SET_MEM) (
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

typedef
EFI_STATUS
(EFIAPI *EFI_INSTALL_PROTOCOL_INTERFACE) (
    EFI_HANDLE *Handle,
    EFI_GUID *Protocol,
    EFI_INTERFACE_TYPE InterfaceType,
    VOID *Interface
    );

/*++

Routine Description:

    This routine installs a protocol interface on a device handle. If the
    handle does not exist, it is created and added to the list of handles in
    the system. InstallMultipleProtocolInterfaces performs more error checking
    than this routine, so it is recommended to be used in place of this
    routine.

Arguments:

    Handle - Supplies a pointer to the EFI handle on which the interface is to
        be installed.

    Protocol - Supplies a pointer to the numeric ID of the protocol interface.

    InterfaceType - Supplies the interface type.

    Interface - Supplies a pointer to the protocol interface.

Return Value:

    EFI_SUCCESS on success.

    EFI_OUT_OF_RESOURCES if memory could not be allocated.

    EFI_INVALID_PARAMETER if the handle or protocol is NULL, the interface type
    is not native, or the protocol is already install on the given handle.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_INSTALL_MULTIPLE_PROTOCOL_INTERFACES) (
    EFI_HANDLE *Handle,
    ...
    );

/*++

Routine Description:

    This routine installs one or more protocol interface into the boot
    services environment.

Arguments:

    Handle - Supplies a pointer to the EFI handle on which the interface is to
        be installed, or a pointer to NULL if a new handle is to be allocated.

    ... - Supplies a variable argument list containing pairs of protocol GUIDs
        and protocol interfaces.

Return Value:

    EFI_SUCCESS on success.

    EFI_OUT_OF_RESOURCES if memory could not be allocated.

    EFI_ALREADY_STARTED if a device path protocol instance was passed in that
    is already present in the handle database.

    EFI_INVALID_PARAMETER if the handle is NULL or the protocol is already
    installed on the given handle.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_REINSTALL_PROTOCOL_INTERFACE) (
    EFI_HANDLE Handle,
    EFI_GUID *Protocol,
    VOID *OldInterface,
    VOID *NewInterface
    );

/*++

Routine Description:

    This routine reinstalls a protocol interface on a device handle.

Arguments:

    Handle - Supplies the device handle on which the interface is to be
        reinstalled.

    Protocol - Supplies a pointer to the numeric ID of the interface.

    OldInterface - Supplies a pointer to the old interface. NULL can be used if
        a structure is not associated with the protocol.

    NewInterface - Supplies a pointer to the new interface.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if the old interface was not found.

    EFI_ACCESS_DENIED if the protocl interface could not be reinstalled because
    the old interface is still being used by a driver that will not release it.

    EFI_INVALID_PARAMETER if the handle or protocol is NULL.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_UNINSTALL_PROTOCOL_INTERFACE) (
    EFI_HANDLE Handle,
    EFI_GUID *Protocol,
    VOID *Interface
    );

/*++

Routine Description:

    This routine removes a protocol interface from a device handle. It is
    recommended that UninstallMultipleProtocolInterfaces be used in place of
    this routine.

Arguments:

    Handle - Supplies the device handle on which the interface is to be
        removed.

    Protocol - Supplies a pointer to the numeric ID of the interface.

    Interface - Supplies a pointer to the interface.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if the old interface was not found.

    EFI_ACCESS_DENIED if the protocl interface could not be reinstalled because
    the old interface is still being used by a driver that will not release it.

    EFI_INVALID_PARAMETER if the handle or protocol is NULL.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES) (
    EFI_HANDLE Handle,
    ...
    );

/*++

Routine Description:

    This routine removes one or more protocol interfaces into the boot services
    environment.

Arguments:

    Handle - Supplies the device handle on which the interface is to be
        removed.

    ... - Supplies a variable argument list containing pairs of protocol GUIDs
        and protocol interfaces.

Return Value:

    EFI_SUCCESS if all of the requested protocol interfaces were removed.

    EFI_INVALID_PARAMETER if one of the protocol interfaces was not previously
        installed on the given.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_HANDLE_PROTOCOL) (
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

typedef
EFI_STATUS
(EFIAPI *EFI_OPEN_PROTOCOL) (
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

    EFI_SUCCESS if the interface information was returned.

    EFI_UNSUPPORTED if the handle not support the specified protocol.

    EFI_INVALID_PARAMETER if a parameter is invalid.

    EFI_ACCESS_DENIED if the required attributes can't be supported in the
    current environment.

    EFI_ALREADY_STARTED if the item on the open list already has required
    attributes whose agent handle is the same as the given one.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_CLOSE_PROTOCOL) (
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

    EFI_SUCCESS if the interface information was returned.

    EFI_INVALID_PARAMETER if the handle, agent, or protocol is NULL, or if the
    controller handle is not NULL and the controller handle is not valid.

    EFI_NOT_FOUND if the handle does not support the given protocol, or the
    protocol interface is not currently open by the agent and controller
    handles.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_OPEN_PROTOCOL_INFORMATION) (
    EFI_HANDLE Handle,
    EFI_GUID *Protocol,
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **EntryBuffer,
    UINTN *EntryCount
    );

/*++

Routine Description:

    This routine retrieves a list of agents that currently have a protocol
    interface opened.

Arguments:

    Handle - Supplies the handle for the protocol interface being queried.

    Protocol - Supplies the published unique identifier of the protocol.

    EntryBuffer - Supplies a pointer where a pointer to a buffer of open
        protocol information in the form of EFI_OPEN_PROTOCOL_INFORMATION_ENTRY
        structures will be returned.

    EntryCount - Supplies a pointer that receives the number of entries in the
        buffer.

Return Value:

    EFI_SUCCESS if the interface information was returned.

    EFI_OUT_OF_RESOURCES if an allocation failed.

    EFI_NOT_FOUND if the handle does not support the given protocol.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_PROTOCOLS_PER_HANDLE) (
    EFI_HANDLE Handle,
    EFI_GUID ***ProtocolBuffer,
    UINTN *ProtocolBufferCount
    );

/*++

Routine Description:

    This routine retrieves the list of protocol interface GUIDs that are
    installed on a handle in a buffer allocated from pool.

Arguments:

    Handle - Supplies the handle from which to retrieve the list of protocol
        interface GUIDs.

    ProtocolBuffer - Supplies a pointer to the list of protocol interface GUID
        pointers that are installed on the given handle.

    ProtocolBufferCount - Supplies a pointer to the number of GUID pointers
        present in the protocol buffer.

Return Value:

    EFI_SUCCESS if the interface information was returned.

    EFI_OUT_OF_RESOURCES if an allocation failed.

    EFI_INVALID_PARAMETER if the handle is NULL or invalid, or the protocol
    buffer or count is NULL.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_REGISTER_PROTOCOL_NOTIFY) (
    EFI_GUID *Protocol,
    EFI_EVENT Event,
    VOID **Registration
    );

/*++

Routine Description:

    This routine creates an event that is to be signaled whenever an interface
    is installed for a specified protocol.

Arguments:

    Protocol - Supplies the numeric ID of the protocol for which the event is
        to be registered.

    Event - Supplies the event that is to be signaled whenever a protocol
        interface is registered for the given protocol.

    Registration - Supplies a pointer to a memory location to receive the
        registration value.

Return Value:

    EFI_SUCCESS on success.

    EFI_OUT_OF_RESOURCES if an allocation failed.

    EFI_INVALID_PARAMETER if the protocol, event, or registration is NULL.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_LOCATE_HANDLE) (
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

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if no handles match the search.

    EFI_BUFFER_TOO_SMALL if the given buffer wasn't big enough to hold all the
    results.

    EFI_INVALID_PARAMETER if the serach type is invalid, one of the parameters
    required by the given search type was NULL, one or more matches are found
    and the buffer size is NULL, or the buffer size is large enough and the
    buffer is NULL.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_LOCATE_DEVICE_PATH) (
    EFI_GUID *Protocol,
    EFI_DEVICE_PATH_PROTOCOL **DevicePath,
    EFI_HANDLE *Device
    );

/*++

Routine Description:

    This routine attempts to locate the handle to a device on the device path
    that supports the specified protocol.

Arguments:

    Protocol - Supplies a pointer to the protocol to search for.

    DevicePath - Supplies a pointer that on input contains a pointer to the
        device path. On output, the path pointer is modified to point to the
        remaining part of the device path.

    Device - Supplies a pointer where the handle of the device will be
        returned.

Return Value:

    EFI_SUCCESS if a handle was returned.

    EFI_NOT_FOUND if no handles match the search.

    EFI_INVALID_PARAMETER if the protocol is NULL, device path is NULL, or a
    handle matched and the device is NULL.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_INSTALL_CONFIGURATION_TABLE) (
    EFI_GUID *Guid,
    VOID *Table
    );

/*++

Routine Description:

    This routine adds, updates, or removes a configuration table entry from the
    EFI System Table.

Arguments:

    Guid - Supplies a pointer to the GUID for the entry to add, update, or
        remove.

    Table - Supplies a pointer to the configuration table for the entry to add,
        update, or remove. This may be NULL.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if an attempt was made to delete a nonexistant entry.

    EFI_INVALID_PARAMETER if the GUID is NULL.

    EFI_OUT_OF_RESOURCES if an allocation failed.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_LOCATE_HANDLE_BUFFER) (
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

typedef
EFI_STATUS
(EFIAPI *EFI_LOCATE_PROTOCOL) (
    EFI_GUID *Protocol,
    VOID *Registration,
    VOID **Interface
    );

/*++

Routine Description:

    This routine returns the first protocol instance that matches the given
    protocol.

Arguments:

    Protocol - Supplies a pointer to the protocol to search by.

    Registration - Supplies a pointer to an optional registration key
        returned from RegisterProtocolNotify.

    Interface - Supplies a pointer where a pointer to the first interface that
        matches will be returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if no protocol instances matched the search.

    EFI_INVALID_PARAMETER if the interface is NULL.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_UPDATE_CAPSULE) (
    EFI_CAPSULE_HEADER **CapsuleHeaderArray,
    UINTN CapsuleCount,
    EFI_PHYSICAL_ADDRESS ScatterGatherList
    );

/*++

Routine Description:

    This routine passes capsules to the firmware with both virtual and physical
    mapping. Depending on the intended consumption, the firmware may process
    the capsule immediately. If the payload should persist across a system
    reset, the reset value returned from EFI_QueryCapsuleCapabilities must be
    passed into ResetSystem and will cause the capsule to be processed by the
    firmware as part of the reset process.

Arguments:

    CapsuleHeaderArray - Supplies a virtual pointer to an array of virtual
        pointers to the capsules being passed into update capsule.

    CapsuleCount - Supplies the number of pointers to EFI_CAPSULE_HEADERs in
        the capsule header array.

    ScatterGatherList - Supplies an optional physical pointer to a set of
        EFI_CAPSULE_BLOCK_DESCRIPTOR that describes the location in physical
        memory of a set of capsules.

Return Value:

    EFI_SUCCESS if a valid capsule was passed. If
    CAPSULE_FLAGS_PERSIT_ACROSS_RESET is not set, the capsule has been
    successfully processed by the firmware.

    EFI_INVALID_PARAMETER if the capsule size is NULL, the capsule count is
    zero, or an incompatible set of flags were set in the capsule header.

    EFI_DEVICE_ERROR if the capsule update was started, but failed due to a
    device error.

    EFI_UNSUPPORTED if the capsule type is not supported on this platform.

    EFI_OUT_OF_RESOURCES if resources could not be allocated. If this call
    originated during runtime, this error is returned if the caller must retry
    the call during boot services.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_QUERY_CAPSULE_CAPABILITIES) (
    EFI_CAPSULE_HEADER **CapsuleHeaderArray,
    UINTN CapsuleCount,
    UINT64 *MaximumCapsuleSize,
    EFI_RESET_TYPE *ResetType
    );

/*++

Routine Description:

    This routine returns whether or not the capsule is supported via the
    UpdateCapsule routine.

Arguments:

    CapsuleHeaderArray - Supplies a virtual pointer to an array of virtual
        pointers to the capsules being passed into update capsule.

    CapsuleCount - Supplies the number of pointers to EFI_CAPSULE_HEADERs in
        the capsule header array.

    MaximumCapsuleSize - Supplies a pointer that on output contains the maximum
        size that the update capsule routine can support as an argument to
        the update capsule routine.

    ResetType - Supplies a pointer where the reset type required to perform the
        capsule update is returned.

Return Value:

    EFI_SUCCESS if a valid answer was returned.

    EFI_UNSUPPORTED if the capsule type is not supported on this platform.

    EFI_DEVICE_ERROR if the capsule update was started, but failed due to a
    device error.

    EFI_INVALID_PARAMETER if the maximum capsule size is NULL.

    EFI_OUT_OF_RESOURCES if resources could not be allocated. If this call
    originated during runtime, this error is returned if the caller must retry
    the call during boot services.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_QUERY_VARIABLE_INFO) (
    UINT32 Attributes,
    UINT64 *MaximumVariableStorageSize,
    UINT64 *RemainingVariableStorageSize,
    UINT64 *MaximumVariableSize
    );

/*++

Routine Description:

    This routine returns information about EFI variables.

Arguments:

    Attributes - Supplies a bitmask of attributes specifying the type of
        variables on which to return information.

    MaximumVariableStorageSize - Supplies a pointer where the maximum size of
        storage space for EFI variables with the given attributes will be
        returned.

    RemainingVariableStorageSize - Supplies a pointer where the remaining size
        of the storage space available for EFI variables associated with the
        attributes specified will be returned.

    MaximumVariableSize - Supplies a pointer where the maximum size of an
        individual variable will be returned on success.

Return Value:

    EFI_SUCCESS if a valid answer was returned.

    EFI_UNSUPPORTED if the attribute is not supported on this platform.

    EFI_INVALID_PARAMETER if an invalid combination of attributes was supplied.

--*/

/*++

Structure Description:

    This structure defines the EFI Runtime Services Table.

Members:

    Hdr - Stores the standard header for an EFI table.

    GetTime - Stores a pointer to a function for getting the current time.

    SetTime - Stores a pointer to a function for setting the current time.

    GetWakeupTime - Stores a pointer to a function for getting the current
        wake alarm time.

    SetWakeupTime - Stores a pointer to a function for setting (or disabling)
        the wake alarm.

    SetVirtualAddressMap - Stores a pointer to a function used to enable
        running EFI runtime services with virtual-to-physical translation (the
        MMU) enabled.

    ConvertPointer - Stores a pointer to a function used to convert a pointer
        into a virtual runtime pointer.

    GetVariable - Stores a pointer to a function used to get the value of an
        EFI variable.

    GetNextVariableName - Stores a pointer to a function used to iterate
        through a set of variables.

    SetVariable - Stores a pointer to a function used to set or delete an
        EFI variable.

    GetNextHighMonotonicCount - Stores a pointer to a function used to get
        the high 32-bits of a monotonically increasing number.

    ResetSystem - Stores a pointer to a function used to shut down or reset
        the platform.

    UpdateCapsule - Stores a pointer to a function used to send an update
        capsule to firmware.

    QueryCapsuleCapabilities - Stores a pointer to a function used to
        interrogate the firmware about update capsule capabilities.

    QueryVariableInfo - Stores a pointer to a function used to get
        information about EFI variable storage.

--*/

typedef struct {
    EFI_TABLE_HEADER Hdr;
    EFI_GET_TIME GetTime;
    EFI_SET_TIME SetTime;
    EFI_GET_WAKEUP_TIME GetWakeupTime;
    EFI_SET_WAKEUP_TIME SetWakeupTime;
    EFI_SET_VIRTUAL_ADDRESS_MAP SetVirtualAddressMap;
    EFI_CONVERT_POINTER ConvertPointer;
    EFI_GET_VARIABLE GetVariable;
    EFI_GET_NEXT_VARIABLE_NAME GetNextVariableName;
    EFI_SET_VARIABLE SetVariable;
    EFI_GET_NEXT_HIGH_MONO_COUNT GetNextHighMonotonicCount;
    EFI_RESET_SYSTEM ResetSystem;
    EFI_UPDATE_CAPSULE UpdateCapsule;
    EFI_QUERY_CAPSULE_CAPABILITIES QueryCapsuleCapabilities;
    EFI_QUERY_VARIABLE_INFO QueryVariableInfo;
} EFI_RUNTIME_SERVICES;

/*++

Structure Description:

    This structure defines the EFI Boot Services Table.

Members:

    Hdr - Stores the standard header for an EFI table.

    RaiseTPL - Stores a pointer to a function for raising the current Task
        Priority Level.

    RestoreTPL - Stores a pointer to a function for restoring a previous Task
        Priority Level.

    AllocatePages - Stores a pointer to a function for allocating pages from
        EFI firmware.

    FreePages - Stores a pointer to a function for freeing pages previously
        allocated.

    GetMemoryMap - Stores a pointer to a function for returning the current
        memory map.

    AllocatePool - Stores a pointer to a function for allocating heap memory.

    FreePool - Stores a pointer to a function for freeing heap allocated
        memory.

    CreateEvent - Stores a pointer to a function for creating an event or
        timer.

    SetTimer - Stores a pointer to a function for setting the trigger on a
        timer.

    WaitForEvent - Stores a pointer to a function for waiting until an event
        is triggered.

    SignalEvent - Stores a pointer to a function for signaling an event.

    CloseEvent - Stores a pointer to a function for closing an event.

    CheckEvent - Stores a pointer to a function for determining if an event
        is signaled.

    InstallProtocolInterface - Stores a pointer to a function for adding
        a protocol to a handle (or creating a new handle).

    ReinstallProtocolInterface - Stores a pointer to a function for
        reinstalling a protocol on a handle.

    UninstallProtocolInterface - Stores a pointer to a function for removing
        a protocol from a handle.

    HandleProtocol - Stores a pointer to a function for determining if a
        handle supports a given protocol.

    Reserved - Stores a reserved pointer. Ignore this.

    RegisterProtocolNotify - Stores a pointer to a function for registering
        for notifications when a protocol is added to a handle.

    LocateHandle - Stores a pointer to a function for locating a handle
        associated with a protocol.

    LocateDevicePath - Stores a pointer to a function for locating a handle on
        a given device path that supports a given protocol.

    InstallConfigurationTable - Stores a pointer to a function for adding a
        configuration table to the firmware.

    LoadImage - Stores a pointer to a function for loading a new EFI image.

    StartImage - Stores a pointer to a function for starting a loaded EFI image.

    Exit - Stores a pointer to a function for exiting an application.

    UnloadImage - Stores a pointer to a function for unloading a loaded EFI
        image.

    ExitBootServices - Stores a pointer to a function for terminating boot
        services.

    GetNextMonotonicCount - Stores a pointer to a function for getting a
        monotonically increasing value.

    Stall - Stores a pointer to a function for performing fine-grained delays.

    SetWatchdogTimer - Stores a pointer to a function for setting a platform
        watchdog timer.

    ConnectController - Stores a pointer to a function for connecting a driver
        to a device controller handle.

    DisconnectController - Stores a pointer to a function for disconnecting a
        driver from a device controller handle.

    OpenProtocol - Stores a pointer to a function for opening a protocol
        interface on a handle.

    CloseProtocol - Stores a pointer to a function for closing a previously
        opened protocol interface on a handle.

    OpenProtocolInformation - Stores a pointer to a function for getting a list
        of agents that currently have a protocol interface opened.

    ProtocolsPerHandle - Stores a pointer to a function for returning a list of
        protocol interface GUIDs that are installed on a handle.

    LocateHandleBuffer - Stores a pointer to a function for getting the list of
        handles that support the requested protocol.

    LocateProtocol - Stores a pointer to a function that returns the first
        protocol instance that matches a given protocol.

    InstallMultipleProtocolInterfaces - Stores a pointer to a function for
        installing one or more protocol interfaces on a handle.

    UninstallMultipleProtocolInterfaces - Stores a pointer to a function for
        uninstalling one or more protocol interfaces from a handle.

    CalculateCrc32 - Stores a pointer to a function for calculating the CRC32
        of a given buffer.

    CopyMem - Stores a pointer to a function for copying memory buffers.

    SetMem - Stores a pointer to a function for initializing memory buffers.

    CreateEventEx - Stores a pointer to a function for creating an event
        optionally associated with a particular event group.

--*/

typedef struct {
    EFI_TABLE_HEADER Hdr;
    EFI_RAISE_TPL RaiseTPL;
    EFI_RESTORE_TPL RestoreTPL;
    EFI_ALLOCATE_PAGES AllocatePages;
    EFI_FREE_PAGES FreePages;
    EFI_GET_MEMORY_MAP GetMemoryMap;
    EFI_ALLOCATE_POOL AllocatePool;
    EFI_FREE_POOL FreePool;
    EFI_CREATE_EVENT CreateEvent;
    EFI_SET_TIMER SetTimer;
    EFI_WAIT_FOR_EVENT WaitForEvent;
    EFI_SIGNAL_EVENT SignalEvent;
    EFI_CLOSE_EVENT CloseEvent;
    EFI_CHECK_EVENT CheckEvent;
    EFI_INSTALL_PROTOCOL_INTERFACE InstallProtocolInterface;
    EFI_REINSTALL_PROTOCOL_INTERFACE ReinstallProtocolInterface;
    EFI_UNINSTALL_PROTOCOL_INTERFACE UninstallProtocolInterface;
    EFI_HANDLE_PROTOCOL HandleProtocol;
    VOID *Reserved;
    EFI_REGISTER_PROTOCOL_NOTIFY RegisterProtocolNotify;
    EFI_LOCATE_HANDLE LocateHandle;
    EFI_LOCATE_DEVICE_PATH LocateDevicePath;
    EFI_INSTALL_CONFIGURATION_TABLE InstallConfigurationTable;
    EFI_IMAGE_LOAD LoadImage;
    EFI_IMAGE_START StartImage;
    EFI_EXIT Exit;
    EFI_IMAGE_UNLOAD UnloadImage;
    EFI_EXIT_BOOT_SERVICES ExitBootServices;
    EFI_GET_NEXT_MONOTONIC_COUNT GetNextMonotonicCount;
    EFI_STALL Stall;
    EFI_SET_WATCHDOG_TIMER SetWatchdogTimer;
    EFI_CONNECT_CONTROLLER ConnectController;
    EFI_DISCONNECT_CONTROLLER DisconnectController;
    EFI_OPEN_PROTOCOL OpenProtocol;
    EFI_CLOSE_PROTOCOL CloseProtocol;
    EFI_OPEN_PROTOCOL_INFORMATION OpenProtocolInformation;
    EFI_PROTOCOLS_PER_HANDLE ProtocolsPerHandle;
    EFI_LOCATE_HANDLE_BUFFER LocateHandleBuffer;
    EFI_LOCATE_PROTOCOL LocateProtocol;
    EFI_INSTALL_MULTIPLE_PROTOCOL_INTERFACES InstallMultipleProtocolInterfaces;
    EFI_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES
                                           UninstallMultipleProtocolInterfaces;

    EFI_CALCULATE_CRC32 CalculateCrc32;
    EFI_COPY_MEM CopyMem;
    EFI_SET_MEM SetMem;
    EFI_CREATE_EVENT_EX CreateEventEx;
} EFI_BOOT_SERVICES;

/*++

Structure Description:

    This structure defines a set consisting of a GUID and a pointer defining
    a configuration table in the EFI System Table.

Members:

    VendorGuid - Stores the GUID identifying the configuration table.

    VendorTable - Stores a pointer to the configuration table.

--*/

typedef struct {
    EFI_GUID VendorGuid;
    VOID *VendorTable;
} EFI_CONFIGURATION_TABLE;

/*++

Structure Description:

    This structure defines the EFI System Table.

Members:

    Hdr - Stores the standard EFI table header.

    FirmwareVendor - Stores a pointer to a null terminated string identifying
        the vendor that produces the firmware for the platform.

    FirmwareRevision - Stores a firmware vendor specific value identifying the
        revision of the system firmware on this platform.

    ConsoleInHandle - Stores the handle for the active console input device.
        This handle must support EFI_SIMPLE_TEXT_INPUT_PROTOCOL and
        EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL.

    ConIn - Stores a pointer to the EFI_SIMPLE_TEXT_INPUT_PROTOCOL interface
        associated with the console in handle.

    ConsoleOutHandle - Stores the handle for the active console output device.

    ConOut - Stores a pointer to the EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL interface
        associated with the console out handle.

    StandardErrorHandle - Stores the handle for the active standard error
        console device. This device must support the
        EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL.

    StdErr - Stores a pointer to the EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL interface
        that is associated with StandardErrorHandle.

    RuntimeServices - Stores a pointer to the EFI Runtime Services Table.

    BootServices - Stores a pointer to the EFI Boot Services Table.

    NumberOfTableEntries - Stores the number of system configuration tables in
        the configuration table buffer.

    ConfigurationTable - Stores a pointer to the array of system configuration
        tables.

--*/

typedef struct {
    EFI_TABLE_HEADER Hdr;
    CHAR16 *FirmwareVendor;
    UINT32 FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
    EFI_RUNTIME_SERVICES *RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
    UINTN NumberOfTableEntries;
    EFI_CONFIGURATION_TABLE *ConfigurationTable;
} EFI_SYSTEM_TABLE;

typedef
EFI_STATUS
(EFIAPI *EFI_IMAGE_ENTRY_POINT) (
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable
    );

/*++

Routine Description:

    This routine is an entry point into an EFI image. The entry point is the
    same for UEFI Applications, UEFI OS Loaders, and UEFI Drivers (both
    device and bus drivers).

Arguments:

    ImageHandle - Supplies the firmware allocated handle associated with this
        (just entered) UEFI image.

    SystemTable - Supplies a pointer to the EFI System Table.

Return Value:

    EFI_SUCCESS if the operation completed successfully.

    Other error codes if an unexpected error occurred.

--*/

/*++

Union Description:

    This union defines EFI Boot Key Data.

Members:

    Options - Stores boot key options.

        Revision - Stores the revision of the EFI_KEY_OPTION structure.
            Currently 0.

        ShiftPressed - Stores a bit indicating either the left or right shift
            key is pressed.

        ControlPressed - Stores a bit indicating either the left or right
            control key is pressed.

        AltPressed - Stores a bit indicating either the left or right alt key
            is pressed.

        LogoPressed - Stores a bit indicating either the left or right Logo key
            is pressed.

        MenuPressed - Stores a bit indicating that the menu key is pressed.

        SysReqPressed - Stores a bit indicating the SysReq key is pressed.

        Reserved - Stores bits reserved for future use.

        InputKeyCount - Stores the number of entries in the Keys member of
            the EFI_KEY_OPTION structure. Valid values are between 0 and 3,
            inclusive. If zero, then only the shift state is considered. If
            more than one, the boot option will only be launched if all the
            specified keys are pressed with the same shift state.

    PackedValue - Stores the packed representation of the options.

--*/

typedef union {
    struct {
        UINT32 Revision:8;
        UINT32 ShiftPressed:1;
        UINT32 ControlPressed:1;
        UINT32 AltPressed:1;
        UINT32 LogoPressed:1;
        UINT32 MenuPressed:1;
        UINT32 SysReqPressed:1;
        UINT32 Reserved:16;
        UINT32 InputKeyCount:2;
    } Options;

    UINT32  PackedValue;
} EFI_BOOT_KEY_DATA;

/*++

Structure Description:

    This structure defines an EFI Boot Key Option.

Members:

    KeyData - Stores options about how the key will be processed.

    BootOptionCrc - Stores the CRC32 of the entire EFI_LOAD_OPTION to which
        this boot option refers. If the CRCs do mot match this value, then this
        key option is ignored.

    BootOption - Stores the Boot#### option which will be invoked if this key
        is pressed and the boot option is active (LOAD_OPTION_ACTIVE is set).

    Keys - Stores the key codes to compare against those returned by the
        EFI_SIMPLE_TEXT_INPUT and EFI_SIMPLE_TEXT_INPUT_EX protocols. The
        number of key codes (0-3) is specified by the EFI_KEY_CODE_COUNT
        field in the key options.

--*/

#pragma pack(push, 1)

typedef struct {
    EFI_BOOT_KEY_DATA KeyData;
    UINT32 BootOptionCrc;
    UINT16 BootOption;
    //EFI_INPUT_KEY Keys[];
} PACKED EFI_KEY_OPTION;

#pragma pack(pop)

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
