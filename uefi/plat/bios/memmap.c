/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    memmap.c

Abstract:

    This module implements support for returning the initial memory map from
    the PC/AT BIOS.

Author:

    Evan Green 27-Feb-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "biosfw.h"

//
// ---------------------------------------------------------------- Definitions
//

#define BIOS_MEMORY_DESCRIPTOR_COUNT 128

#define E820_MAGIC 0x534D4150    // 'SMAP'

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _E820_MEMORY_TYPE {
    E820TypeInvalid,
    E820TypeUsableMemory,
    E820TypeReserved,
    E820TypeAcpiReclaimable,
    E820TypeAcpiReserved,
    E820TypeBadMemory
} E820_MEMORY_TYPE, *PE820_MEMORY_TYPE;

typedef struct _E820_DESCRIPTOR {
    UINT32 BaseAddressLow;
    UINT32 BaseAddressHigh;
    UINT32 LengthLow;
    UINT32 LengthHigh;
    UINT32 Type;
} E820_DESCRIPTOR, *PE820_DESCRIPTOR;

//
// ----------------------------------------------- Internal Function Prototypes
//

EFI_STATUS
EfipGetE820MemoryMap (
    EFI_MEMORY_DESCRIPTOR *Map,
    UINTN *MapSize
    );

EFI_STATUS
EfipAddBiosMemoryDescriptor (
    EFI_MEMORY_DESCRIPTOR *Map,
    EFI_MEMORY_DESCRIPTOR *Descriptor,
    UINTN *MapSize,
    UINTN MapCapacity,
    BOOLEAN ForceAdd
    );

EFI_STATUS
EfipInsertDescriptorAtIndex (
    EFI_MEMORY_DESCRIPTOR *Map,
    EFI_MEMORY_DESCRIPTOR *Descriptor,
    UINTN Index,
    UINTN *MapSize,
    UINTN MapCapacity
    );

//
// -------------------------------------------------------------------- Globals
//

EFI_MEMORY_DESCRIPTOR EfiBiosMemoryMap[BIOS_MEMORY_DESCRIPTOR_COUNT];

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfiPlatformGetInitialMemoryMap (
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

    UINTN Size;
    EFI_STATUS Status;

    Size = BIOS_MEMORY_DESCRIPTOR_COUNT;
    Status = EfipGetE820MemoryMap(EfiBiosMemoryMap, &Size);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    *Map = EfiBiosMemoryMap;
    *MapSize = Size;
    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

EFI_STATUS
EfipGetE820MemoryMap (
    EFI_MEMORY_DESCRIPTOR *Map,
    UINTN *MapSize
    )

/*++

Routine Description:

    This routine gets the firmware memory map from the BIOS using int 15 E820
    calls.

Arguments:

    Map - Supplies a pointer where the map will be returned on success.

    MapSize - Supplies a pointer that on input contains the maximum number of
        descriptors in the given array. On output, the number of descriptors
        reported will be returned.

Return Value:

    EFI status code.

--*/

{

    EFI_PHYSICAL_ADDRESS BaseAddress;
    EFI_MEMORY_DESCRIPTOR Descriptor;
    UINTN DescriptorCount;
    EFI_MEMORY_TYPE DescriptorType;
    PE820_DESCRIPTOR E820Descriptor;
    E820_MEMORY_TYPE E820Type;
    BOOLEAN FirstCall;
    UINT64 Length;
    UINTN MaxDescriptors;
    UINTN MoveIndex;
    BIOS_CALL_CONTEXT RealModeContext;
    EFI_MEMORY_DESCRIPTOR *Search;
    UINTN SearchIndex;
    EFI_STATUS Status;

    DescriptorCount = 0;
    FirstCall = TRUE;
    MaxDescriptors = *MapSize;
    Status = EfipCreateBiosCallContext(&RealModeContext, 0x15);
    if (EFI_ERROR(Status)) {
        goto GetMemoryMapEnd;
    }

    E820Descriptor = (PE820_DESCRIPTOR)RealModeContext.DataPage;
    RealModeContext.Ebx = 0;
    do {

        //
        // Watch for overflow conditions or buggy firmware.
        //

        if (DescriptorCount >= MaxDescriptors) {
            break;
        }

        //
        // Set up the firmware call.
        //

        E820Descriptor->Type = E820TypeInvalid;
        RealModeContext.Es =
                           ADDRESS_TO_SEGMENT((UINTN)RealModeContext.DataPage);

        RealModeContext.Edi = (UINTN)RealModeContext.DataPage & 0xF;
        RealModeContext.Edx = E820_MAGIC;
        RealModeContext.Eax = 0xE820;
        RealModeContext.Ecx = 24;

        //
        // Execute the firmware call.
        //

        EfipExecuteBiosCall(&RealModeContext);

        //
        // If eax is not set to the magic number (on the first call only), or
        // the carry clag is clear, then the call failed.
        //

        if ((FirstCall != FALSE) && (RealModeContext.Eax != E820_MAGIC)) {
            break;
        }

        FirstCall = FALSE;
        if ((RealModeContext.Eflags & IA32_EFLAG_CF) != 0) {
            break;
        }

        //
        // Get the descriptor information.
        //

        BaseAddress =
                ((EFI_PHYSICAL_ADDRESS)E820Descriptor->BaseAddressHigh << 32) |
                 (E820Descriptor->BaseAddressLow);

        Length = ((EFI_PHYSICAL_ADDRESS)E820Descriptor->LengthHigh << 32) |
                 (E820Descriptor->LengthLow);

        E820Type = E820Descriptor->Type;
        switch (E820Type) {
        case E820TypeUsableMemory:
            DescriptorType = EfiConventionalMemory;
            break;

        case E820TypeReserved:
            DescriptorType = EfiRuntimeServicesData;
            break;

        case E820TypeAcpiReclaimable:
            DescriptorType = EfiACPIReclaimMemory;
            break;

        case E820TypeAcpiReserved:
            DescriptorType = EfiACPIMemoryNVS;
            break;

        case E820TypeBadMemory:
            DescriptorType = EfiUnusableMemory;
            break;

        //
        // Unknown memory type. Skip this descriptor.
        //

        default:
            continue;
        }

        Descriptor.Type = DescriptorType;
        Descriptor.Padding = 0;
        Descriptor.PhysicalStart = BaseAddress;
        Descriptor.VirtualStart = 0;
        Descriptor.NumberOfPages = EFI_SIZE_TO_PAGES(Length);
        Descriptor.Attribute = 0;

        //
        // Add the descriptor to the memory map.
        //

        Status = EfipAddBiosMemoryDescriptor(Map,
                                             &Descriptor,
                                             &DescriptorCount,
                                             MaxDescriptors,
                                             FALSE);

        if (EFI_ERROR(Status)) {
            goto GetMemoryMapEnd;
        }

    } while ((RealModeContext.Ebx != 0) &&
            ((RealModeContext.Eflags & IA32_EFLAG_CF) == 0));

    //
    // Simply reserve the entire first megabyte.
    //

    Descriptor.Type = EfiUnusableMemory;
    Descriptor.Padding = 0;
    Descriptor.PhysicalStart = 0;
    Descriptor.NumberOfPages = (1024 * 1024) >> EFI_PAGE_SHIFT;
    Status = EfipAddBiosMemoryDescriptor(Map,
                                         &Descriptor,
                                         &DescriptorCount,
                                         MaxDescriptors,
                                         TRUE);

    if (EFI_ERROR(Status)) {
        goto GetMemoryMapEnd;
    }

    //
    // Mark a portion of that first megabyte usable between 0x1000 and 0x9F000.
    //

    Descriptor.Type = EfiConventionalMemory;
    Descriptor.Padding = 0;
    Descriptor.PhysicalStart = 0x1000;
    Descriptor.NumberOfPages = 0x9E;
    Status = EfipAddBiosMemoryDescriptor(Map,
                                         &Descriptor,
                                         &DescriptorCount,
                                         MaxDescriptors,
                                         TRUE);

    if (EFI_ERROR(Status)) {
        goto GetMemoryMapEnd;
    }

    //
    // Remove any empty regions.
    //

    SearchIndex = 0;
    while (SearchIndex < DescriptorCount) {
        Search = &(Map[SearchIndex]);
        if (Search->NumberOfPages == 0) {
            for (MoveIndex = SearchIndex;
                 MoveIndex < DescriptorCount - 1;
                 MoveIndex += 1) {

                Map[MoveIndex] = Map[MoveIndex + 1];
            }

            DescriptorCount -= 1;
            continue;
        }

        SearchIndex += 1;
    }

    Status = EFI_SUCCESS;

GetMemoryMapEnd:
    *MapSize = DescriptorCount;
    EfipDestroyBiosCallContext(&RealModeContext);
    return Status;
}

EFI_STATUS
EfipAddBiosMemoryDescriptor (
    EFI_MEMORY_DESCRIPTOR *Map,
    EFI_MEMORY_DESCRIPTOR *Descriptor,
    UINTN *MapSize,
    UINTN MapCapacity,
    BOOLEAN ForceAdd
    )

/*++

Routine Description:

    This routine adds a BIOS memory descriptor to the EFI memory map.

Arguments:

    Map - Supplies a pointer to the current memory map.

    Descriptor - Supplies a pointer to the descriptor to add.

    MapSize - Supplies a pointer that on input contains the current size of the
        memory map. On output, this will be updated to reflect the added
        descriptor (or multiple).

    MapCapacity - Supplies the total capacity of the map.

    ForceAdd - Supplies a boolean indicating if this descriptor should trump
        existing descriptors for the same region. If FALSE, then automatic
        rules are used to determine which descriptor wins a conflict.

Return Value:

    EFI status code.

--*/

{

    EFI_PHYSICAL_ADDRESS Base;
    EFI_PHYSICAL_ADDRESS End;
    EFI_MEMORY_DESCRIPTOR *Existing;
    EFI_PHYSICAL_ADDRESS ExistingEnd;
    BOOLEAN NewWins;
    EFI_MEMORY_DESCRIPTOR Remainder;
    UINTN SearchIndex;
    EFI_STATUS Status;
    EFI_MEMORY_TYPE Type;

    Base = Descriptor->PhysicalStart;
    End = Descriptor->PhysicalStart +
          (Descriptor->NumberOfPages << EFI_PAGE_SHIFT);

    Type = Descriptor->Type;

    //
    // Skip zero-length descriptors.
    //

    if (Descriptor->NumberOfPages == 0) {
        return EFI_SUCCESS;
    }

    //
    // Loop looking for the right place to put this descriptor in.
    //

    for (SearchIndex = 0; SearchIndex < *MapSize; SearchIndex += 1) {
        Existing = &(Map[SearchIndex]);
        ExistingEnd = Existing->PhysicalStart +
                      (Existing->NumberOfPages << EFI_PAGE_SHIFT);

        //
        // Skip empty descriptors.
        //

        if (Existing->NumberOfPages == 0) {
            continue;
        }

        //
        // If this descriptor is entirely before the new one, keep looking.
        //

        if (ExistingEnd <= Base) {
            continue;
        }

        //
        // If the start of this descriptor is after the end of the new one,
        // then just insert the new one before this one.
        //

        if (Existing->PhysicalStart >= End) {
            Status = EfipInsertDescriptorAtIndex(Map,
                                                 Descriptor,
                                                 SearchIndex,
                                                 MapSize,
                                                 MapCapacity);

            goto AddBiosMemoryDescriptorEnd;
        }

        //
        // The existing descriptor overlaps in some way. Who wins depends on
        // the type. Take the new descriptor if the existing one is "free", or
        // if the new one is a "firmware permanent" type of memory.
        //

        NewWins = FALSE;
        if (ForceAdd != FALSE) {
            NewWins = TRUE;

        } else if (Existing->Type == EfiConventionalMemory) {
            NewWins = TRUE;

        } else if ((Type == EfiUnusableMemory) ||
                   (Type == EfiRuntimeServicesCode) ||
                   (Type == EfiRuntimeServicesData) ||
                   (Type == EfiACPIMemoryNVS) ||
                   (Type == EfiMemoryMappedIO) ||
                   (Type == EfiMemoryMappedIOPortSpace) ||
                   (Type == EfiPalCode)) {

            NewWins = TRUE;
        }

        //
        // Shrink the existing descriptor if the new one should win.
        //

        if (NewWins != FALSE) {

            //
            // If the new descriptor splits the existing one, add a remainder
            // descriptor.
            //

            if ((Existing->PhysicalStart < Base) &&
                (ExistingEnd > End)) {

                Remainder = *Existing;
                Remainder.NumberOfPages = (ExistingEnd - End) >> EFI_PAGE_SHIFT;
                Remainder.PhysicalStart = End;
                Status = EfipInsertDescriptorAtIndex(Map,
                                                     &Remainder,
                                                     SearchIndex + 1,
                                                     MapSize,
                                                     MapCapacity);

                if (EFI_ERROR(Status)) {
                    goto AddBiosMemoryDescriptorEnd;
                }
            }

            //
            // Bump up the start if that's what overlaps with the new one.
            //

            if (Existing->PhysicalStart > Base) {
                if (ExistingEnd <= End) {
                    Existing->NumberOfPages = 0;

                } else {
                    Existing->NumberOfPages =
                                         (ExistingEnd - End) >> EFI_PAGE_SHIFT;

                    Existing->PhysicalStart = End;
                }
            }

            //
            // Clip down the end if that's what overlaps with the new one.
            //

            if (ExistingEnd > Base) {
                if (Existing->PhysicalStart >= Base) {
                    Existing->NumberOfPages = 0;

                } else {
                    Existing->NumberOfPages =
                            (Base - Existing->PhysicalStart) >> EFI_PAGE_SHIFT;
                }
            }

        //
        // The existing descriptor wins. Shrink the new descriptor.
        //

        } else {

            //
            // If the existing descriptor is completely contained within the
            // new descriptor, then it cuts it in two. Add the bottom portion
            // before this descriptor.
            //

            if ((Base < Existing->PhysicalStart) &&
                (End > Existing->PhysicalStart)) {

                Remainder = *Descriptor;
                Remainder.NumberOfPages =
                            (Existing->PhysicalStart - Base) >> EFI_PAGE_SHIFT;

                Status = EfipInsertDescriptorAtIndex(Map,
                                                     &Remainder,
                                                     SearchIndex,
                                                     MapSize,
                                                     MapCapacity);

                if (EFI_ERROR(Status)) {
                    goto AddBiosMemoryDescriptorEnd;
                }

                SearchIndex += 1;
                Existing = &(Map[SearchIndex]);

            } else {

                //
                // Bump up the start if that's what overlaps with the existing
                // one.
                //

                if (Base > Existing->PhysicalStart) {
                    if (End <= ExistingEnd) {
                        Status = EFI_SUCCESS;
                        goto AddBiosMemoryDescriptorEnd;
                    }

                    Descriptor->NumberOfPages =
                                         (End - ExistingEnd) >> EFI_PAGE_SHIFT;

                    Base = ExistingEnd;
                    Descriptor->PhysicalStart = Base;
                }

                //
                // Clip down the end if that's what overlaps with the existing
                // one.
                //

                if (End > Existing->PhysicalStart) {
                    if (Base >= Existing->PhysicalStart) {
                        Status = EFI_SUCCESS;
                        goto AddBiosMemoryDescriptorEnd;
                    }

                    Descriptor->NumberOfPages =
                            (Existing->PhysicalStart - Base) >> EFI_PAGE_SHIFT;

                    End = Descriptor->PhysicalStart +
                          (Descriptor->NumberOfPages << EFI_PAGE_SHIFT);
                }
            }
        }

        //
        // If the existing descriptor is still there and is greater than the
        // new descriptor, insert the new descriptor here.
        //

        if ((Existing->NumberOfPages != 0) &&
            (Existing->PhysicalStart > Base)) {

            Status = EfipInsertDescriptorAtIndex(Map,
                                                 Descriptor,
                                                 SearchIndex,
                                                 MapSize,
                                                 MapCapacity);

            goto AddBiosMemoryDescriptorEnd;
        }
    }

    //
    // After going through the loop the descriptor still hasn't been added. So
    // add it here on the end.
    //

    Status = EfipInsertDescriptorAtIndex(Map,
                                         Descriptor,
                                         SearchIndex,
                                         MapSize,
                                         MapCapacity);

AddBiosMemoryDescriptorEnd:
    return Status;
}

EFI_STATUS
EfipInsertDescriptorAtIndex (
    EFI_MEMORY_DESCRIPTOR *Map,
    EFI_MEMORY_DESCRIPTOR *Descriptor,
    UINTN Index,
    UINTN *MapSize,
    UINTN MapCapacity
    )

/*++

Routine Description:

    This routine inserts a descriptor into the given memory map at a specific
    index.

Arguments:

    Map - Supplies a pointer to the current memory map.

    Descriptor - Supplies a pointer to the descriptor to add.

    Index - Supplies the index to add the descriptor at.

    MapSize - Supplies a pointer that on input contains the current size of the
        memory map. On output, this will be updated to reflect the added
        descriptor (or multiple).

    MapCapacity - Supplies the total capacity of the map.

Return Value:

    EFI status code.

--*/

{

    UINTN MoveIndex;

    if (*MapSize == MapCapacity) {
        return EFI_BUFFER_TOO_SMALL;
    }

    //
    // Scoot everything over by one.
    //

    for (MoveIndex = *MapSize; MoveIndex > Index; MoveIndex -= 1) {
        Map[MoveIndex] = Map[MoveIndex - 1];
    }

    Map[Index] = *Descriptor;
    *MapSize += 1;
    return EFI_SUCCESS;
}

