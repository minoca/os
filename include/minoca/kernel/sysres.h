/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sysres.h

Abstract:

    This header contains definitions for builtin system resources.

Author:

    Evan Green 17-Oct-2012

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

typedef enum _SYSTEM_RESOURCE_TYPE {
    SystemResourceInvalid,
    SystemResourceFrameBuffer,
    SystemResourceRamDisk,
    SystemResourceHardwareModule,
    SystemResourceDebugDevice,
    SystemResourceMemory,
} SYSTEM_RESOURCE_TYPE, *PSYSTEM_RESOURCE_TYPE;

typedef enum _SYSTEM_MEMORY_RESOURCE_TYPE {
    SystemMemoryResourceHardwareModule,
    SystemMemoryResourceHardwareModuleDevice,
} SYSTEM_MEMORY_RESOURCE_TYPE, *PSYSTEM_MEMORY_RESOURCE_TYPE;

/*++

Structure Description:

    This structure stores the common header for a builtin system resource.

Members:

    ListEntry - Stores pointers to the next and previous system resources.

    Type - Stores the type of the system resource being described.

    Acquired - Stores a boolean indicating whether this resource is already
        acquired.

    PhysicalAddress - Stores the physical address of the resource, if the
        resource requires memory address space.

    Size - Stores the size of the frame buffer, in bytes. This will be 0 if the
        device requires no memory mapped resources.

    VirtualAddress - Stores the mapped virtual address of the frame buffer. This
        will be NULL if the device requires no memory mapped resoures, or the
        resources have not yet been mapped.

--*/

typedef struct _SYSTEM_RESOURCE_HEADER {
    LIST_ENTRY ListEntry;
    SYSTEM_RESOURCE_TYPE Type;
    BOOL Acquired;
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONGLONG Size;
    PVOID VirtualAddress;
} SYSTEM_RESOURCE_HEADER, *PSYSTEM_RESOURCE_HEADER;

/*++

Structure Description:

    This structure stores information about a frame buffer resource.

Members:

    Header - Stores the common system resource header.

    Mode - Stores the base video mode. This is of type BASE_VIDEO_MODE.

    Width - Stores the width of the frame buffer, in pixels.

    Height - Stores the height of the frame buffer, in pixels.

    BitsPerPixel - Stores the number of bits that correspond to one pixel.

    PixelsPerScanLine - Stores the number of pixels in a scan line.

    RedMask - Stores the mask of bits in the pixel that correspond to the red
        channel. This is assumed to be a contiguous chunk of bits.

    GreenMask - Stores the mask of bits in the pixel that correspond to the
        green channel. This is assumed to be a contiguous chunk of bits.

    BlueMask - Stores the mask of bits in the pixel that correspond to the blue
        channel. This is assumed to be a contiguous chunk of bits.

--*/

typedef struct _SYSTEM_RESOURCE_FRAME_BUFFER {
    SYSTEM_RESOURCE_HEADER Header;
    ULONG Mode;
    ULONG Width;
    ULONG Height;
    ULONG BitsPerPixel;
    ULONG PixelsPerScanLine;
    ULONG RedMask;
    ULONG GreenMask;
    ULONG BlueMask;
} SYSTEM_RESOURCE_FRAME_BUFFER, *PSYSTEM_RESOURCE_FRAME_BUFFER;

/*++

Structure Description:

    This structure stores information about a hardware module device resource.

Members:

    Header - Stores the common system resource header.

--*/

typedef struct _SYSTEM_RESOURCE_HARDWARE_MODULE {
    SYSTEM_RESOURCE_HEADER Header;
} SYSTEM_RESOURCE_HARDWARE_MODULE, *PSYSTEM_RESOURCE_HARDWARE_MODULE;

/*++

Structure Description:

    This structure stores information about a RAM disk resource.

Members:

    Header - Stores the common system resource header.

--*/

typedef struct _SYSTEM_RESOURCE_RAM_DISK {
    SYSTEM_RESOURCE_HEADER Header;
} SYSTEM_RESOURCE_RAM_DISK, *PSYSTEM_RESOURCE_RAM_DISK;

/*++

Structure Description:

    This structure stores information about a debug device resource.

Members:

    Header - Stores the common system resource header.

--*/

typedef struct _SYSTEM_RESOURCE_DEBUG_DEVICE {
    SYSTEM_RESOURCE_HEADER Header;
} SYSTEM_RESOURCE_DEBUG_DEVICE, *PSYSTEM_RESOURCE_DEBUG_DEVICE;

/*++

Structure Description:

    This structure stores information about a system memory resource.

Members:

    Header - Stores the common system resource header.

    MemoryType - Stores the type of memory this resource represents.

--*/

typedef struct _SYSTEM_RESOURCE_MEMORY {
    SYSTEM_RESOURCE_HEADER Header;
    SYSTEM_MEMORY_RESOURCE_TYPE MemoryType;
} SYSTEM_RESOURCE_MEMORY, *PSYSTEM_RESOURCE_MEMORY;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KERNEL_API
PSYSTEM_RESOURCE_HEADER
KeAcquireSystemResource (
    SYSTEM_RESOURCE_TYPE ResourceType
    );

/*++

Routine Description:

    This routine attempts to find an unacquired system resource of the given
    type.

Arguments:

    ResourceType - Supplies a pointer to the type of builtin resource to
        acquire.

Return Value:

    Returns a pointer to a resource of the given type on success.

    NULL on failure.

--*/

KERNEL_API
VOID
KeReleaseSystemResource (
    PSYSTEM_RESOURCE_HEADER ResourceHeader
    );

/*++

Routine Description:

    This routine releases a system resource.

Arguments:

    ResourceHeader - Supplies a pointer to the resource header to release back
        to the system.

Return Value:

    None.

--*/

