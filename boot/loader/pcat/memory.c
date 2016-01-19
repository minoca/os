/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    memory.c

Abstract:

    This module implements the BIOS int 0x15 E820 function calls used to get
    the firmware memory map.

Author:

    Evan Green 27-Jul-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include <minoca/x86.h>
#include "firmware.h"
#include "bios.h"
#include "bootlib.h"
#include "paging.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
BoFwMapKnownRegions (
    ULONG Phase,
    PKERNEL_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine maps known ARM regions of memory.

Arguments:

    Phase - Supplies the phase number, as this routine is called twice: once
        before any other mappings have been established (0), and once near the
        end of the loader (1).

    Parameters - Supplies a pointer to the kernel's initialization parameters.

Return Value:

    Status code.

--*/

{

    PSYSTEM_RESOURCE_FRAME_BUFFER FrameBufferResource;
    KSTATUS Status;

    if (Phase == 0) {
        return STATUS_SUCCESS;
    }

    ASSERT(Phase == 1);

    FrameBufferResource =
                        BoAllocateMemory(sizeof(SYSTEM_RESOURCE_FRAME_BUFFER));

    if (FrameBufferResource == NULL) {
        Status = STATUS_NO_MEMORY;
        goto MapKnownRegionsEnd;
    }

    RtlZeroMemory(FrameBufferResource,
                  sizeof(SYSTEM_RESOURCE_FRAME_BUFFER));

    FrameBufferResource->Header.Type = SystemResourceFrameBuffer;
    FrameBufferResource->Header.PhysicalAddress = FwFrameBufferPhysical;
    FrameBufferResource->Header.Size = FwFrameBufferWidth *
                                       FwFrameBufferHeight *
                                       FwFrameBufferBitsPerPixel /
                                       BITS_PER_BYTE;

    FrameBufferResource->Header.VirtualAddress = (PVOID)-1;
    FrameBufferResource->Width = FwFrameBufferWidth;
    FrameBufferResource->Height = FwFrameBufferHeight;
    FrameBufferResource->BitsPerPixel = FwFrameBufferBitsPerPixel;
    FrameBufferResource->PixelsPerScanLine = FrameBufferResource->Width;
    FrameBufferResource->RedMask = 0x00FF0000;
    FrameBufferResource->GreenMask = 0x0000FF00;
    FrameBufferResource->BlueMask = 0x000000FF;
    Status = BoMapPhysicalAddress(&(FrameBufferResource->Header.VirtualAddress),
                                  FrameBufferResource->Header.PhysicalAddress,
                                  FrameBufferResource->Header.Size,
                                  MAP_FLAG_WRITE_THROUGH | MAP_FLAG_GLOBAL,
                                  MemoryTypeLoaderPermanent);

    if (!KSUCCESS(Status)) {
        goto MapKnownRegionsEnd;
    }

MapKnownRegionsEnd:
    if (!KSUCCESS(Status)) {
        if (FrameBufferResource != NULL) {
            BoFreeMemory(FrameBufferResource);
        }

    } else {
        if (FrameBufferResource != NULL) {
            INSERT_BEFORE(&(FrameBufferResource->Header.ListEntry),
                          &(Parameters->SystemResourceListHead));
        }
    }

    return Status;
}

KSTATUS
BoFwPrepareForKernelLaunch (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine coordinates with the firmware to end boot services and
    prepare for the operating system to take over. Translation is still
    disabled (or identity mapped) at this point.

Arguments:

    Parameters - Supplies a pointer to the kernel initialization block.

Return Value:

    Status code.

--*/

{

    MEMORY_DESCRIPTOR Descriptor;
    KSTATUS Status;

    Parameters->FirmwareType = SystemFirmwarePcat;

    //
    // Add a free page at 0x1000 so the kernel has a place in low memory to
    // identity map for MP startup.
    //

    MmMdInitDescriptor(&Descriptor,
                       IDENTITY_STUB_ADDRESS,
                       IDENTITY_STUB_ADDRESS + PAGE_SIZE,
                       MemoryTypeFree);

    Status = MmMdAddDescriptorToList(&BoMemoryMap, &Descriptor);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    return STATUS_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

