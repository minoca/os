/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    memmap.c

Abstract:

    This module implements support for acquiring the initial memory map on a
    BCM2709 SoC.

Author:

    Chris Stevens 21-Dec-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include <dev/bcm2709.h>

//
// ---------------------------------------------------------------- Definitions
//

#define BCM2709_MEMORY_MAP_SIZE \
    (sizeof(EfiBcm2709MemoryMap) / sizeof(EfiBcm2709MemoryMap[0]))

#define BCM2709_MEMORY_MAP_SCRATCH_BUFFER_SIZE \
    sizeof(BCM2709_MAILBOX_GET_MEMORY_REGIONS) + BCM2709_MAILBOX_DATA_ALIGNMENT

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the data necessary to get the system's memory
    regions.

Members:

    Header - Stores a header that defines the total size of the messages being
        sent to and received from the mailbox.

    ArmMemoryRegion - Stores a request to get the ARM core's memory region.

    VideoMemoryRegion - Store a request to get the video core's memory region.

    EndTag - Stores the tag to denote the end of the mailbox message.

--*/

typedef struct _BCM2709_MAILBOX_GET_MEMORY_REGIONS {
    BCM2709_MAILBOX_HEADER Header;
    BCM2709_MAILBOX_MEMORY_REGION ArmMemoryRegion;
    BCM2709_MAILBOX_MEMORY_REGION VideoMemoryRegion;
    UINT32 EndTag;
} BCM2709_MAILBOX_GET_MEMORY_REGIONS, *PBCM2709_MAILBOX_GET_MEMORY_REGIONS;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// Define the initial memory map.
//

EFI_MEMORY_DESCRIPTOR EfiBcm2709MemoryMap[] = {
    {
        EfiConventionalMemory,
        0,
        0,
        0,
        0,
        0
    },

    {
        EfiMemoryMappedIO,
        0,
        0,
        0,
        0,
        0
    },

    {
        EfiRuntimeServicesData,
        0,
        BCM2709_PRM_OFFSET,
        0,
        EFI_SIZE_TO_PAGES(BCM2709_PRM_SIZE),
        EFI_MEMORY_UC | EFI_MEMORY_RUNTIME
    },
};

//
// Define a template for the call to query the memory regions.
//

BCM2709_MAILBOX_GET_MEMORY_REGIONS EfiBcm2709GetMemoryRegionsTemplate = {

    {
        sizeof(BCM2709_MAILBOX_GET_MEMORY_REGIONS),
        0
    },

    {
        {
            BCM2709_MAILBOX_TAG_GET_ARM_CORE_MEMORY,
            sizeof(UINT32) + sizeof(UINT32),
            0
        },

        0,
        0
    },

    {
        {
            BCM2709_MAILBOX_TAG_GET_VIDEO_CORE_MEMORY,
            sizeof(UINT32) + sizeof(UINT32),
            0
        },

        0,
        0
    },

    0
};

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipBcm2709GetInitialMemoryMap (
    EFI_MEMORY_DESCRIPTOR **Map,
    UINTN *MapSize
    )

/*++

Routine Description:

    This routine returns the initial platform memory map to the EFI core. The
    core maintains this memory map. The memory map returned does not need to
    take into account the firmware image itself or stack, the EFI core will
    reserve those regions automatically.

Arguments:

    Map - Supplies a pointer where the array of memory descriptors constituting
        the initial memory map is returned on success. The EFI core will make
        a copy of these descriptors, so they can be in read-only or
        temporary memory.

    MapSize - Supplies a pointer where the number of elements in the initial
        memory map will be returned on success.

Return Value:

    EFI status code.

--*/

{

    UINT8 Buffer[BCM2709_MEMORY_MAP_SCRATCH_BUFFER_SIZE];
    UINT32 ExpectedLength;
    UINT32 Length;
    PBCM2709_MAILBOX_GET_MEMORY_REGIONS MemoryRegions;
    EFI_STATUS Status;

    //
    // The BCM2709 device library must be initialized in order to get the
    // memory map.
    //

    if (EfiBcm2709Initialized == FALSE) {
        return EFI_NOT_READY;
    }

    MemoryRegions = ALIGN_POINTER(Buffer, BCM2709_MAILBOX_DATA_ALIGNMENT);
    EfiCopyMem(MemoryRegions,
               &EfiBcm2709GetMemoryRegionsTemplate,
               sizeof(BCM2709_MAILBOX_GET_MEMORY_REGIONS));

    //
    // Request and validate the memory regions.
    //

    Status = EfipBcm2709MailboxSendCommand(
                                    BCM2709_MAILBOX_PROPERTIES_CHANNEL,
                                    MemoryRegions,
                                    sizeof(BCM2709_MAILBOX_GET_MEMORY_REGIONS),
                                    FALSE);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Length = MemoryRegions->ArmMemoryRegion.TagHeader.Length;
    ExpectedLength = sizeof(BCM2709_MAILBOX_MEMORY_REGION) -
                     sizeof(BCM2709_MAILBOX_TAG);

    if (BCM2709_MAILBOX_CHECK_TAG_LENGTH(Length, ExpectedLength) == FALSE) {
        Status = EFI_DEVICE_ERROR;
        return Status;
    }

    Length = MemoryRegions->VideoMemoryRegion.TagHeader.Length;
    ExpectedLength = sizeof(BCM2709_MAILBOX_MEMORY_REGION) -
                     sizeof(BCM2709_MAILBOX_TAG);

    if (BCM2709_MAILBOX_CHECK_TAG_LENGTH(Length, ExpectedLength) == FALSE) {
        Status = EFI_DEVICE_ERROR;
        return Status;
    }

    //
    // Fill out the memory map based on the two regions returned by the
    // firmware.
    //

    EfiBcm2709MemoryMap[0].PhysicalStart =
                                    MemoryRegions->ArmMemoryRegion.BaseAddress;

    EfiBcm2709MemoryMap[0].NumberOfPages =
                           MemoryRegions->ArmMemoryRegion.Size / EFI_PAGE_SIZE;

    EfiBcm2709MemoryMap[1].PhysicalStart =
                                  MemoryRegions->VideoMemoryRegion.BaseAddress;

    EfiBcm2709MemoryMap[1].NumberOfPages =
                         MemoryRegions->VideoMemoryRegion.Size / EFI_PAGE_SIZE;

    //
    // Patch up the PRM base as only the offset from the BCM2709 base address
    // was stored in the global array.
    //

    EfiBcm2709MemoryMap[2].PhysicalStart = (UINTN)BCM2709_PRM_BASE;
    *Map = EfiBcm2709MemoryMap;
    *MapSize = BCM2709_MEMORY_MAP_SIZE;
    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

