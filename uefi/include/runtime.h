/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    runtime.h

Abstract:

    This header contains definitions for the EFI runtime architectural protocol.

Author:

    Evan Green 4-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_RUNTIME_ARCH_PROTOCOL_GUID                      \
    {                                                       \
        0xB7DFB4E1, 0x052F, 0x449F,                         \
        {0x87, 0xBE, 0x98, 0x18, 0xFC, 0x91, 0xB7, 0x33}    \
    }

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores runtime data about a loaded image.

Members:

    ListEntry - Stores pointers to the next and previous runtime image entries.

    ImageBase - Stores a pointer to the start of the image loaded in memory. It
        is a pointer to either the DOS header or PE header of the image.

    ImageSize - Supplies the size in bytes of the image.

    RelocationData - Stores a pointer to the relocation information.

    Handle - Stores the image handle corresponding with this image.

--*/

typedef struct _EFI_RUNTIME_IMAGE_ENTRY {
    LIST_ENTRY ListEntry;
    VOID *ImageBase;
    UINT64 ImageSize;
    VOID *RelocationData;
    EFI_HANDLE Handle;
} EFI_RUNTIME_IMAGE_ENTRY, *PEFI_RUNTIME_IMAGE_ENTRY;

/*++

Structure Description:

    This structure stores runtime data about an event.

Members:

    ListEntry - Stores pointers to the next and previous runtime event entries.

    Type - Stores the type of event.

    NotifyTpl - Stores the task priority level of the event.

    NotifyFunction - Stores a pointer to a function called when the event fires.

    NotifyContext - Stores a pointer's worth of data passed to the notify
        function.

    Event - Stores a pointer to the parent event structure.

--*/

typedef struct _EFI_RUNTIME_EVENT_ENTRY {
    LIST_ENTRY ListEntry;
    UINT32 Type;
    EFI_TPL NotifyTpl;
    EFI_EVENT_NOTIFY NotifyFunction;
    VOID *NotifyContext;
    EFI_EVENT *Event;
} EFI_RUNTIME_EVENT_ENTRY, *PEFI_RUNTIME_EVENT_ENTRY;

/*++

Structure Description:

    This structure stores the EFI runtime architectural protocol, providing the
    handoff between the core and runtime environments.

Members:

    ImageListHead - Stores the head of the list of runtime image entries.

    EventListHead - Stores the head of the list of runtime events.

    MemoryDescriptorSize - Stores the size in bytes of a memory descriptor.

    MemoryDescriptorVersion - Stores the memory descriptor version number.

    MemoryMapSize - Stores the total size of the memory map in bytes.

    MemoryMapPhysical - Stores a physical pointer to the memory map.

    MemoryMapVirtual - Stores a virtual pointer to the memory map if the core
        has been virtualized.

    VirtualMode - Stores a boolean indicating if SetVirtualAddressMap has been
        called.

    AtRuntime - Stores a boolean indicating if ExitBootServices has been called.

--*/

typedef struct _EFI_RUNTIME_ARCH_PROTOCOL {
    LIST_ENTRY ImageListHead;
    LIST_ENTRY EventListHead;
    UINTN MemoryDescriptorSize;
    UINT32 MemoryDescriptorVersion;
    UINTN MemoryMapSize;
    EFI_MEMORY_DESCRIPTOR *MemoryMapPhysical;
    EFI_MEMORY_DESCRIPTOR *MemoryMapVirtual;
    BOOLEAN VirtualMode;
    BOOLEAN AtRuntime;
} EFI_RUNTIME_ARCH_PROTOCOL, *PEFI_RUNTIME_ARCH_PROTOCOL;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

EFIAPI
EFI_STATUS
EfiCoreCalculateCrc32 (
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

