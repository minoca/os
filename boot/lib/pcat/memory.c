/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/x86.h>
#include "realmode.h"
#include "firmware.h"
#include "bootlib.h"
#include "bios.h"

//
// ---------------------------------------------------------------- Definitions
//

#define E820_MAGIC 0x534D4150    // 'SMAP'
#define MAX_E820_DESCRIPTORS 100

#if __SIZEOF_LONG__ == 8

//
// Keep things to the first 8GB since that's what's mapped by default on x64.
//

#define PCAT_MAX_ALLOCATION_ADDRESS ((8ULL * _1GB) - 1)

#else

#define PCAT_MAX_ALLOCATION_ADDRESS MAX_UINTN

#endif

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
FwpPcatReserveKnownRegions (
    PMEMORY_DESCRIPTOR_LIST MemoryMap
    );

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
    ULONG BaseAddressLow;
    ULONG BaseAddressHigh;
    ULONG LengthLow;
    ULONG LengthHigh;
    ULONG Type;
} E820_DESCRIPTOR, *PE820_DESCRIPTOR;

//
// -------------------------------------------------------------------- Globals
//

//
// Define the statically allocated memory descriptors used to represent the
// memory map.
//

MEMORY_DESCRIPTOR FwMemoryMapDescriptors[MAX_E820_DESCRIPTORS];

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
FwPcatGetMemoryMap (
    PMEMORY_DESCRIPTOR_LIST MdlOut
    )

/*++

Routine Description:

    This routine gets the firmware memory map from the BIOS using int 15 E820
    calls.

Arguments:

    MdlOut - Supplies a pointer where the memory map information will be
        stored. This buffer must be allocated by the caller.

Return Value:

    STATUS_SUCCESS if one or more descriptors could be retrieved from the
        firmware.

    STATUS_UNSUCCESSFUL if no descriptors could be obtained from the firmware.

--*/

{

    ULONGLONG BaseAddress;
    MEMORY_DESCRIPTOR Descriptor;
    ULONG DescriptorsFound;
    MEMORY_TYPE DescriptorType;
    PE820_DESCRIPTOR E820Descriptor;
    E820_MEMORY_TYPE E820Type;
    BOOL FirstCall;
    ULONGLONG Length;
    ULONGLONG MaxAddress;
    ULONG PageSize;
    REAL_MODE_CONTEXT RealModeContext;
    KSTATUS Status;

    DescriptorsFound = 0;
    FirstCall = TRUE;
    PageSize = MmPageSize();
    MmMdInitDescriptorList(MdlOut, MdlAllocationSourceNone);
    MmMdAddFreeDescriptorsToMdl(MdlOut,
                                FwMemoryMapDescriptors,
                                sizeof(FwMemoryMapDescriptors));

    //
    // Create a standard BIOS call context.
    //

    Status = FwpRealModeCreateBiosCallContext(&RealModeContext, 0x15);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    E820Descriptor = (PE820_DESCRIPTOR)RealModeContext.DataPage.Page;
    RealModeContext.Ebx = 0;
    do {

        //
        // Watch for overflow conditions or buggy firmware.
        //

        if (DescriptorsFound >= MAX_E820_DESCRIPTORS) {
            break;
        }

        //
        // Set up the firmware call.
        //

        E820Descriptor->Type = E820TypeInvalid;
        RealModeContext.Es =
                   ADDRESS_TO_SEGMENT(RealModeContext.DataPage.RealModeAddress);

        RealModeContext.Edi = RealModeContext.DataPage.RealModeAddress & 0xF;
        RealModeContext.Edx = E820_MAGIC;
        RealModeContext.Eax = 0xE820;
        RealModeContext.Ecx = 24;

        //
        // Execute the firmware call.
        //

        FwpRealModeExecute(&RealModeContext);

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

        BaseAddress = ((ULONGLONG)E820Descriptor->BaseAddressHigh << 32) |
                      (E820Descriptor->BaseAddressLow);

        Length = ((ULONGLONG)E820Descriptor->LengthHigh << 32) |
                 (E820Descriptor->LengthLow);

        if (Length == 0) {
            continue;
        }

        E820Type = E820Descriptor->Type;
        switch (E820Type) {
        case E820TypeUsableMemory:
            DescriptorType = MemoryTypeFree;
            Length = ALIGN_RANGE_DOWN(Length, PageSize);
            break;

        case E820TypeReserved:
            DescriptorType = MemoryTypeFirmwarePermanent;
            Length = ALIGN_RANGE_UP(Length, PageSize);
            break;

        case E820TypeAcpiReclaimable:
            DescriptorType = MemoryTypeAcpiTables;
            Length = ALIGN_RANGE_UP(Length, PageSize);
            break;

        case E820TypeAcpiReserved:
            DescriptorType = MemoryTypeAcpiNvStorage;
            Length = ALIGN_RANGE_UP(Length, PageSize);
            break;

        case E820TypeBadMemory:
            DescriptorType = MemoryTypeBad;
            Length = ALIGN_RANGE_UP(Length, PageSize);
            break;

        //
        // Unknown memory type. Skip this descriptor.
        //

        default:
            continue;
        }

        MaxAddress = (UINTN)-1;
        if (DescriptorType == MemoryTypeFree) {

            //
            // If the descriptor starts above the maximum allocable address,
            // mark it firmware temporary.
            //

            if (BaseAddress > MaxAddress) {
                DescriptorType = MemoryTypeFirmwareTemporary;

            //
            // If the descriptor ends above the maximum allocable address,
            // mark the portion that goes above firmware temporary.
            //

            } else if (BaseAddress + Length > MaxAddress) {
                MmMdInitDescriptor(&Descriptor,
                                   MaxAddress + 1,
                                   BaseAddress + Length - (MaxAddress + 1),
                                   MemoryTypeFirmwareTemporary);

                MmMdAddDescriptorToList(MdlOut, &Descriptor);
                Length = (MaxAddress + 1) - BaseAddress;
            }
        }

        //
        // Initialize a new descriptor and add it to the MDL. On failure, just
        // skip this descriptor.
        //

        MmMdInitDescriptor(&Descriptor,
                           BaseAddress,
                           BaseAddress + Length,
                           DescriptorType);

        Status = MmMdAddDescriptorToList(MdlOut, &Descriptor);
        if (!KSUCCESS(Status)) {
            continue;
        }

        DescriptorsFound += 1;

    } while ((RealModeContext.Ebx != 0) &&
            ((RealModeContext.Eflags & IA32_EFLAG_CF) == 0));

    Status = STATUS_SUCCESS;
    if (DescriptorsFound == 0) {
        Status = STATUS_UNSUCCESSFUL;

    } else {
        Status = FwpPcatReserveKnownRegions(MdlOut);
    }

    FwpRealModeDestroyBiosCallContext(&RealModeContext);
    return Status;
}

KSTATUS
FwPcatAllocatePages (
    PULONGLONG Address,
    ULONGLONG Size,
    ULONG Alignment,
    MEMORY_TYPE MemoryType
    )

/*++

Routine Description:

    This routine allocates physical pages for use.

Arguments:

    Address - Supplies a pointer to where the allocation will be returned.

    Size - Supplies the size of the required space.

    Alignment - Supplies the alignment requirement for the allocation, in bytes.
        Valid values are powers of 2. Set to 1 or 0 to specify no alignment
        requirement.

    MemoryType - Supplies the type of memory to mark the allocation as.

Return Value:

    STATUS_SUCCESS if the allocation was successful.

    STATUS_INVALID_PARAMETER if a page count of 0 was passed or the address
        parameter was not filled out.

    STATUS_NO_MEMORY if the allocation request could not be filled.

--*/

{

    KSTATUS Status;

    Status = MmMdAllocateFromMdl(&BoMemoryMap,
                                 Address,
                                 Size,
                                 Alignment,
                                 0,
                                 PCAT_MAX_ALLOCATION_ADDRESS,
                                 MemoryType,
                                 AllocationStrategyLowestAddress);

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
FwpPcatReserveKnownRegions (
    PMEMORY_DESCRIPTOR_LIST MemoryMap
    )

/*++

Routine Description:

    This routine removes regions from the firmware memory map known to be
    reserved on BIOS machines.

Arguments:

    MemoryMap - Supplies a pointer to the firmware memory map. The memory map
        is modified to avoid using regions known to be reserved but not
        reflected in the firmware memory map.

Return Value:

    None.

--*/

{

    MEMORY_DESCRIPTOR Descriptor;
    KSTATUS Status;

    //
    // Don't bother trying to use the first megabyte of memory.
    //

    MmMdInitDescriptor(&Descriptor, 0, _1MB, MemoryTypeFirmwarePermanent);
    Status = MmMdAddDescriptorToList(MemoryMap, &Descriptor);
    if (!KSUCCESS(Status)) {
        goto ReserveKnownRegionsEnd;
    }

ReserveKnownRegionsEnd:
    return Status;
}

