/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mapping.c

Abstract:

    This module implements memory mapping and unmapping functionality.

Author:

    Evan Green 12-Jun-2017

Environment:

    Kernel (AMD64)

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/bootload.h>
#include <minoca/kernel/x64.h>
#include "../mmp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// This macro uses the self map to get at the PML4T.
//

#define X64_PML4T \
    ((PPTE)((X64_SELF_MAP_INDEX << X64_PML4E_SHIFT) + \
            (X64_SELF_MAP_INDEX << X64_PDPE_SHIFT) + \
            (X64_SELF_MAP_INDEX << X64_PDE_SHIFT) + \
            (X64_SELF_MAP_INDEX << X64_PTE_SHIFT)))

//
// This macro gets a page directory pointer.
//

#define X64_PDPT(_VirtualAddress) \
    ((PPTE)((X64_SELF_MAP_INDEX << X64_PML4E_SHIFT) + \
            (X64_SELF_MAP_INDEX << X64_PDPE_SHIFT) + \
            (X64_SELF_MAP_INDEX << X64_PDE_SHIFT) + \
            (((UINTN)(_VirtualAddress) & X64_PDPE_MASK) >> (2 * X64_PTE_BITS))))

//
// This macro gets a page directory.
//

#define X64_PDT(_VirtualAddress) \
    ((PPTE)((X64_SELF_MAP_INDEX << X64_PML4E_SHIFT) + \
            (X64_SELF_MAP_INDEX << X64_PDPE_SHIFT) + \
            (((UINTN)(_VirtualAddress) & (X64_PDPE_MASK | X64_PDE_MASK)) >> \
             X64_PTE_BITS)))
//
// This macro gets a bottom level page table.
//

#define X64_PT(_VirtualAddress) \
    ((PPTE)((X64_SELF_MAP_INDEX << X64_PML4E_SHIFT) + \
            ((UINTN)(_VirtualAddress) & \
             (X64_PDPE_MASK | X64_PDE_MASK | X64_PTE_MASK))))

//
// This macro gets a page table at any level.
//

#define X64_SELF_MAP(_PdpIndex, _PdIndex, _PtIndex)     \
    ((PPTE)((X64_SELF_MAP_INDEX << X64_PML4E_SHIFT) +   \
            ((_PdpIndex) << X64_PDPE_SHIFT) +           \
            ((_PdIndex) << X64_PDE_SHIFT) +             \
            ((_PtIndex) << X64_PTE_SHIFT)))

//
// These macros evaluate to pointers to exact table entries for a particular
// address.
//

#define X64_PML4E(_VirtualAddress) \
    ((PPTE)X64_PML4T + X64_PML4_INDEX(_VirtualAddress))

#define X64_PDPE(_VirtualAddress) \
    ((PPTE)X64_PDPT(_VirtualAddress) + X64_PDP_INDEX(_VirtualAddress))

#define X64_PDE(_VirtualAddress) \
    ((PPTE)X64_PDT(_VirtualAddress) + X64_PD_INDEX(_VirtualAddress))

#define X64_PTE(_VirtualAddress) \
    ((PPTE)X64_PT(_VirtualAddress) + X64_PT_INDEX(_VirtualAddress))

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
MmpCreatePageDirectory (
    PADDRESS_SPACE_X64 AddressSpace
    );

VOID
MmpDestroyPageDirectory (
    PADDRESS_SPACE_X64 AddressSpace
    );

PPTE
MmpGetOtherProcessPte (
    PADDRESS_SPACE_X64 AddressSpace,
    PVOID VirtualAddress,
    BOOL Create
    );

KSTATUS
MmpEnsurePageTables (
    PADDRESS_SPACE_X64 AddressSpace,
    PVOID VirtualAddress
    );

KSTATUS
MmpCreatePageTable (
    PADDRESS_SPACE_X64 AddressSpace,
    PPTE Pte,
    PHYSICAL_ADDRESS Physical
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Stores a pointer to the kernel's top level page table structure.
//

volatile PTE *MmKernelPml4;

//
// Synchronizes access to creating or destroying page tables.
//

KSPIN_LOCK MmPageTableLock;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
ULONG
MmPageSize (
    VOID
    )

/*++

Routine Description:

    This routine returns the size of a page of memory.

Arguments:

    None.

Return Value:

    Returns the size of one page of memory (ie the minimum mapping granularity).

--*/

{

    return PAGE_SIZE;
}

KERNEL_API
ULONG
MmPageShift (
    VOID
    )

/*++

Routine Description:

    This routine returns the amount to shift by to truncate an address to a
    page number.

Arguments:

    None.

Return Value:

    Returns the amount to shift to reach page granularity.

--*/

{

    return PAGE_SHIFT;
}

VOID
MmIdentityMapStartupStub (
    ULONG PageCount,
    PVOID *Allocation,
    PVOID *PageDirectory
    )

/*++

Routine Description:

    This routine allocates and identity maps pages in the first 1MB of physical
    memory for use by application processor startup code.

Arguments:

    PageCount - Supplies the number of pages to allocate and map.

    Allocation - Supplies a pointer where the virtual/physical address will
        be returned on success.

    PageDirectory - Supplies a pointer where the current page directory will be
        returned.

Return Value:

    None.

--*/

{

    PVOID CurrentAddress;
    ULONG CurrentPage;
    ULONG MapFlags;

    //
    // Allocate pages starting at address 0x1000.
    //

    *Allocation = (PVOID)(UINTN)IDENTITY_STUB_ADDRESS;
    CurrentAddress = *Allocation;
    MapFlags = MAP_FLAG_PRESENT | MAP_FLAG_EXECUTE;
    if (CurrentAddress >= KERNEL_VA_START) {
        MapFlags |= MAP_FLAG_GLOBAL;
    }

    for (CurrentPage = 0; CurrentPage < PageCount; CurrentPage += 1) {
        MmpMapPage((UINTN)CurrentAddress, CurrentAddress, MapFlags);
        CurrentAddress += PAGE_SIZE;
    }

    *PageDirectory = (PVOID)ArGetCurrentPageDirectory();
    return;
}

VOID
MmUnmapStartupStub (
    PVOID Allocation,
    ULONG PageCount
    )

/*++

Routine Description:

    This routine unmaps memory allocated and identity mapped for use by the
    AP startup stub.

Arguments:

    Allocation - Supplies the allocation.

    PageCount - Supplies the number of pages in the allocation.

Return Value:

    None.

--*/

{

    ASSERT((UINTN)Allocation == IDENTITY_STUB_ADDRESS);

    //
    // Unmap the pages. Don't "free" the physical pages because they were
    // never recognized as memory.
    //

    MmpUnmapPages(Allocation, PageCount, UNMAP_FLAG_SEND_INVALIDATE_IPI, NULL);
    return;
}

VOID
MmUpdatePageDirectory (
    PADDRESS_SPACE AddressSpace,
    PVOID VirtualAddress,
    UINTN Size
    )

/*++

Routine Description:

    This routine updates the kernel mode entries in the given page directory
    for the given virtual address range so that they're current.

Arguments:

    AddressSpace - Supplies a pointer to the address space.

    VirtualAddress - Supplies the base virtual address of the range to be
        synchronized.

    Size - Supplies the size of the virtual address range to synchronize.

Return Value:

    None.

--*/

{

    ULONG EndIndex;
    ULONG Index;

    Index = X64_PML4_INDEX(VirtualAddress);
    EndIndex = X64_PML4_INDEX(VirtualAddress + (Size - 1));
    while (Index <= EndIndex) {

        //
        // The supplied VA range should never include the self map directory
        // entries.
        //

        ASSERT(Index != X64_SELF_MAP_INDEX);

        X64_PML4T[Index] = MmKernelPml4[Index];
        Index += 1;
    }

    return;
}

ULONG
MmValidateMemoryAccessForDebugger (
    PVOID Address,
    ULONG Length,
    PBOOL Writable
    )

/*++

Routine Description:

    This routine validates that access to a specified location in memory will
    not cause a page fault. It is intended to be called only by the debugger.

Arguments:

    Address - Supplies the virtual address of the memory that will be read or
        written.

    Length - Supplies how many bytes at that location the caller would like to
        read or write.

    Writable - Supplies an optional pointer that receives a boolean indicating
        whether or not the memory range is mapped writable.

Return Value:

    Returns the number of bytes from the beginning of the address that are
    accessible. If the memory is completely available, the return value will be
    equal to the Length parameter. If the memory is completely paged out, 0
    will be returned.

--*/

{

    PVOID Current;
    PVOID End;
    PPTE Table;

    //
    // Assume that all pages are writable until proven otherwise.
    //

    if (Writable != NULL) {
        *Writable = TRUE;
    }

    End = Address + Length;
    Current = Address;
    while (Current < End) {
        if ((*X64_PML4E(Current) & X86_PTE_PRESENT) == 0) {
            break;
        }

        if ((*X64_PDPE(Current) & X86_PTE_PRESENT) == 0) {
            break;
        }

        if ((*X64_PDE(Current) & X86_PTE_PRESENT) == 0) {
            break;
        }

        Table = X64_PTE(Current);
        if ((*Table & X86_PTE_PRESENT) == 0) {
            break;
        }

        if ((*Table & X86_PTE_WRITABLE) == 0) {
            *Writable = FALSE;
        }

        Current += PAGE_SIZE;
    }

    if (Current >= End) {
        return Length;
    }

    return Current - Address;
}

VOID
MmModifyAddressMappingForDebugger (
    PVOID Address,
    BOOL Writable,
    PBOOL WasWritable
    )

/*++

Routine Description:

    This routine modifies the mapping properties for the page that contains the
    given address.

Arguments:

    Address - Supplies the virtual address of the memory whose mapping
        properties are to be changed.

    Writable - Supplies a boolean indicating whether or not to make the page
        containing the address writable (TRUE) or read-only (FALSE).

    WasWritable - Supplies a pointer that receives a boolean indicating whether
        or not the page was writable (TRUE) or read-only (FALSE) before any
        modifications.

Return Value:

    None.

--*/

{

    PPTE Pte;

    *WasWritable = TRUE;

    ASSERT(((*X64_PML4E(Address) & X86_PTE_PRESENT) != 0) &&
           ((*X64_PDPE(Address) & X86_PTE_PRESENT) != 0) &&
           ((*X64_PDE(Address) & X86_PTE_PRESENT) != 0));

    Pte = X64_PTE(Address);
    if ((*Pte & X86_PTE_WRITABLE) == 0) {
        *WasWritable = FALSE;
        if (Writable != FALSE) {
            *Pte |= X86_PTE_WRITABLE;
        }

    } else {
        if (Writable == FALSE) {
            *Pte &= ~X86_PTE_WRITABLE;
        }
    }

    ArInvalidateTlbEntry(Address);
    return;
}

VOID
MmSwitchAddressSpace (
    PVOID Processor,
    PVOID CurrentStack,
    PADDRESS_SPACE AddressSpace
    )

/*++

Routine Description:

    This routine switches to the given address space.

Arguments:

    Processor - Supplies a pointer to the current processor block.

    CurrentStack - Supplies the address of the current thread's kernel stack.
        This routine will ensure this address is visible in the address space
        being switched to. Stacks must not cross page directory boundaries.

    AddressSpace - Supplies a pointer to the address space to switch to.

Return Value:

    None.

--*/

{

    ULONG Index;
    PADDRESS_SPACE_X64 Space;

    Space = (PADDRESS_SPACE_X64)AddressSpace;

    //
    // Make sure the current stack is visible. It might not be if this current
    // thread is new and its stack pushed out into a new level 4 table not in
    // the destination context.
    //

    Index = X64_PML4_INDEX(CurrentStack);
    X64_PML4T[Index] = MmKernelPml4[Index];
    ArSetCurrentPageDirectory(Space->Pml4Physical);
    return;
}

KSTATUS
MmpArchInitialize (
    PKERNEL_INITIALIZATION_BLOCK Parameters,
    ULONG Phase
    )

/*++

Routine Description:

    This routine performs architecture-specific initialization.

Arguments:

    Parameters - Supplies a pointer to the kernel initialization parameters
        from the loader.

    Phase - Supplies the initialization phase.

Return Value:

    Status code.

--*/

{

    PMEMORY_DESCRIPTOR Descriptor;
    MEMORY_DESCRIPTOR NewDescriptor;
    PPTE Pd;
    BOOL PdHasEntries;
    ULONG PdIndex;
    PPTE Pdp;
    BOOL PdpHasEntries;
    ULONG PdpIndex;
    PPTE Pml4;
    ULONG Pml4Index;
    PPROCESSOR_BLOCK ProcessorBlock;
    PPTE Pt;
    ULONG PtIndex;
    KSTATUS Status;

    //
    // Phase 0 runs on the boot processor before the debugger is online.
    //

    if (Phase == 0) {
        if ((Parameters->PageDirectory == NULL) ||
            (Parameters->PageTableStage == NULL)) {

            Status = STATUS_NOT_INITIALIZED;
            goto ArchInitializeEnd;
        }

        MmKernelPml4 = Parameters->PageDirectory;
        ProcessorBlock = KeGetCurrentProcessorBlock();
        ProcessorBlock->SwapPage = Parameters->PageTableStage;
        KeInitializeSpinLock(&MmPageTableLock);
        Status = STATUS_SUCCESS;

    //
    // Phase 1 initialization runs on all processors.
    //

    } else if (Phase == 1) {

        //
        // Initialize basic globals if this is the boot processor.
        //

        if (KeGetCurrentProcessorNumber() == 0) {

            //
            // Take over the second page of physical memory.
            //

            Descriptor = MmMdLookupDescriptor(
                                            Parameters->MemoryMap,
                                            IDENTITY_STUB_ADDRESS,
                                            IDENTITY_STUB_ADDRESS + PAGE_SIZE);

            ASSERT((Descriptor == NULL) ||
                   (Descriptor->Type == MemoryTypeFree));

            MmMdInitDescriptor(&NewDescriptor,
                               IDENTITY_STUB_ADDRESS,
                               IDENTITY_STUB_ADDRESS + PAGE_SIZE,
                               MemoryTypeReserved);

            MmMdAddDescriptorToList(Parameters->MemoryMap, &NewDescriptor);
        }

        Status = STATUS_SUCCESS;

    //
    // Phase 2 initialization only runs on the boot processor in order to
    // prepare for multi-threaded execution.
    //

    } else if (Phase == 2) {
        Status = STATUS_SUCCESS;

    //
    // Phase 3 runs once after the scheduler is active.
    //

    } else if (Phase == 3) {

        //
        // By now, all boot mappings should have been unmapped. Loop over the
        // kernel page table's user mode space looking for entries. If there
        // are non-zero entries on a page table, keep the page tables. If the
        // lower or mid level page tables are entirely clean, free them.
        //

        Pml4 = (PPTE)MmKernelPml4;
        for (Pml4Index = 0;
             Pml4Index < X64_PML4_INDEX(KERNEL_VA_START);
             Pml4Index += 1) {

            if (X86_PTE_ENTRY(Pml4[Pml4Index]) == 0) {

                ASSERT((Pml4[Pml4Index] & X86_PTE_PRESENT) == 0);

                continue;
            }

            //
            // Scan the PDP looking for valid entries.
            //

            PdpHasEntries = FALSE;
            Pdp = X64_PDPT((UINTN)Pml4Index << X64_PDPE_SHIFT);
            for (PdpIndex = 0; PdpIndex < X64_PTE_COUNT; PdpIndex += 1) {
                if (X86_PTE_ENTRY(Pdp[PdpIndex]) == 0) {

                    ASSERT((Pdp[PdpIndex] & X86_PTE_PRESENT) == 0);

                    continue;
                }

                //
                // Scan the PD looking for valid entries.
                //

                Pd = X64_PDT(((UINTN)Pml4Index << X64_PDPE_SHIFT) |
                             ((UINTN)PdpIndex << X64_PDE_SHIFT));

                PdHasEntries = FALSE;
                for (PdIndex = 0; PdIndex < X64_PTE_COUNT; PdIndex += 1) {
                    if (X86_PTE_ENTRY(Pd[PdIndex] == 0)) {

                        ASSERT((Pd[PdIndex] & X86_PTE_PRESENT) == 0);

                        continue;
                    }

                    //
                    // Scan the page table looking for entries.
                    //

                    Pt = X64_PT(((UINTN)Pml4Index << X64_PDPE_SHIFT) |
                                ((UINTN)PdpIndex << X64_PDE_SHIFT) |
                                ((UINTN)PdIndex << X64_PTE_SHIFT));

                    for (PtIndex = 0; PtIndex < X64_PTE_COUNT; PtIndex += 1) {
                        if (X86_PTE_ENTRY(Pt[PtIndex] != 0)) {
                            break;
                        }
                    }

                    //
                    // If there was a page mapped somewhere in the page table,
                    // then the PD and PDP have to stick around.
                    //

                    if (PtIndex != X64_PTE_COUNT) {
                        PdHasEntries = TRUE;
                        PdpHasEntries = TRUE;

                    //
                    // Free up this page table.
                    //

                    } else {
                        MmFreePhysicalPages(X86_PTE_ENTRY(Pd[PdIndex]), 1);
                        Pd[PdIndex] = 0;
                    }
                }

                //
                // If there were no page tables with mappings, then this page
                // directory can be freed.
                //

                if (PdHasEntries == FALSE) {
                    MmFreePhysicalPages(X86_PTE_ENTRY(Pdp[PdpIndex]), 1);
                    Pdp[PdpIndex] = 0;
                }
            }

            //
            // If there were no page tables in the entire PDP with mappings,
            // then free up the PDP.
            //

            if (PdpHasEntries == FALSE) {
                MmFreePhysicalPages(X86_PTE_ENTRY(Pml4[Pml4Index]), 1);
                Pml4[Pml4Index] = 0;
            }
        }

        Status = STATUS_SUCCESS;

    } else {

        ASSERT(FALSE);

        Status = STATUS_INVALID_PARAMETER;
        goto ArchInitializeEnd;
    }

ArchInitializeEnd:
    return Status;
}

PADDRESS_SPACE
MmpArchCreateAddressSpace (
    VOID
    )

/*++

Routine Description:

    This routine creates a new address space context. This routine allocates
    the structure, zeros at least the common portion, and initializes any
    architecture specific members after the common potion.

Arguments:

    None.

Return Value:

    Returns a pointer to the new address space on success.

    NULL on allocation failure.

--*/

{

    PADDRESS_SPACE_X64 Space;
    KSTATUS Status;

    Space = MmAllocateNonPagedPool(sizeof(ADDRESS_SPACE_X64),
                                   MM_ADDRESS_SPACE_ALLOCATION_TAG);

    if (Space == NULL) {
        return NULL;
    }

    RtlZeroMemory(Space, sizeof(ADDRESS_SPACE_X64));
    Status = MmpCreatePageDirectory(Space);
    if (!KSUCCESS(Status)) {
        goto ArchCreateAddressSpaceEnd;
    }

ArchCreateAddressSpaceEnd:
    if (!KSUCCESS(Status)) {
        if (Space != NULL) {
            MmpDestroyPageDirectory(Space);
            MmFreeNonPagedPool(Space);
            Space = NULL;
        }
    }

    return (PADDRESS_SPACE)Space;
}

VOID
MmpArchDestroyAddressSpace (
    PADDRESS_SPACE AddressSpace
    )

/*++

Routine Description:

    This routine destroys an address space, freeing this structure and all
    architecture-specific content. The common portion of the structure will
    already have been taken care of.

Arguments:

    AddressSpace - Supplies a pointer to the address space to destroy.

Return Value:

    None.

--*/

{

    PADDRESS_SPACE_X64 Space;

    Space = (PADDRESS_SPACE_X64)AddressSpace;
    MmpDestroyPageDirectory(Space);
    MmFreeNonPagedPool(Space);
    return;
}

BOOL
MmpCheckDirectoryUpdates (
    PVOID FaultingAddress
    )

/*++

Routine Description:

    This routine determines if a page fault occurred because a process' page
    directory is out of date. If so, it updates the directory entry.

Arguments:

    FaultingAddress - Supplies the address to verify for up-to-date PDE entries.

Return Value:

    Returns TRUE if the update resolved the page fault, or FALSE if the fault
    requires further attention.

--*/

{

    PPTE Pml4;
    ULONG Pml4Index;

    //
    // This check only applies to kernel-mode addresses.
    //

    if (FaultingAddress < KERNEL_VA_START) {
        return FALSE;
    }

    Pml4Index = X64_PML4_INDEX(FaultingAddress);
    Pml4 = X64_PML4T;
    if (Pml4[Pml4Index] != MmKernelPml4[Pml4Index]) {
        Pml4[Pml4Index] = MmKernelPml4[Pml4Index];

        //
        // See if the fault is resolved by this entry.
        //

        if (((Pml4[Pml4Index] & X86_PTE_PRESENT) != 0) &&
            ((*X64_PDPE(FaultingAddress) & X86_PTE_PRESENT) != 0) &&
            ((*X64_PDE(FaultingAddress) & X86_PTE_PRESENT) != 0) &&
            ((*X64_PTE(FaultingAddress) & X86_PTE_PRESENT) != 0)) {

            return TRUE;
        }
    }

    return FALSE;
}

VOID
MmpMapPage (
    PHYSICAL_ADDRESS PhysicalAddress,
    PVOID VirtualAddress,
    ULONG Flags
    )

/*++

Routine Description:

    This routine maps a physical page of memory into virtual address space.

Arguments:

    PhysicalAddress - Supplies the physical address to back the mapping with.

    VirtualAddress - Supplies the virtual address to map the physical page to.

    Flags - Supplies a bitfield of flags governing the options of the mapping.
        See MAP_FLAG_* definitions.

Return Value:

    None.

--*/

{

    PADDRESS_SPACE_X64 AddressSpace;
    PKTHREAD CurrentThread;
    PKPROCESS Process;
    PPTE Pte;

    CurrentThread = KeGetCurrentThread();
    if (CurrentThread == NULL) {

        ASSERT(VirtualAddress >= KERNEL_VA_START);

        AddressSpace = NULL;

    } else {
        Process = CurrentThread->OwningProcess;
        AddressSpace = (PADDRESS_SPACE_X64)(Process->AddressSpace);
    }

    //
    // Assert that the addresses are page aligned.
    //

    ASSERT((PhysicalAddress & PAGE_MASK) == 0);
    ASSERT(((UINTN)VirtualAddress & PAGE_MASK) == 0);

    //
    // If no page table exists for this entry, allocate and initialize one.
    //

    if (((*X64_PML4E(VirtualAddress) & X86_PTE_PRESENT) == 0) ||
        ((*X64_PDPE(VirtualAddress) & X86_PTE_PRESENT) == 0) ||
        ((*X64_PDE(VirtualAddress) & X86_PTE_PRESENT) == 0)) {

        MmpEnsurePageTables(AddressSpace, VirtualAddress);
    }

    Pte = X64_PTE(VirtualAddress);

    ASSERT(((*Pte & X86_PTE_PRESENT) == 0) && (X86_PTE_ENTRY(*Pte) == 0));

    *Pte = PhysicalAddress;
    if ((Flags & MAP_FLAG_READ_ONLY) == 0) {
        *Pte |= X86_PTE_WRITABLE;
    }

    if ((Flags & MAP_FLAG_CACHE_DISABLE) != 0) {

        ASSERT((Flags & MAP_FLAG_WRITE_THROUGH) == 0);

        *Pte |= X86_PTE_CACHE_DISABLED;

    } else if ((Flags & MAP_FLAG_WRITE_THROUGH) != 0) {
        *Pte |= X86_PTE_WRITE_THROUGH;
    }

    ASSERT((Flags & MAP_FLAG_LARGE_PAGE) == 0);

    if ((Flags & MAP_FLAG_USER_MODE) != 0) {

        ASSERT(VirtualAddress < KERNEL_VA_START);

        *Pte |= X86_PTE_USER_MODE;

    } else if ((Flags & MAP_FLAG_GLOBAL) != 0) {
        *Pte |= X86_PTE_USER_MODE;
    }

    if ((Flags & MAP_FLAG_DIRTY) != 0) {
        *Pte |= X86_PTE_DIRTY;
    }

    if ((Flags & MAP_FLAG_EXECUTE) == 0) {
        *Pte |= X86_PTE_NX;
    }

    //
    // TLB entry invalidation is not required when transitioning a PTE's
    // present bit from 0 to 1 as long as it was invalidated the last time it
    // went from 1 to 0. The invalidation on a 1 to 0 transition is, however,
    // required as the physical page may be immediately re-used.
    //

    if ((Flags & MAP_FLAG_PRESENT) != 0) {
        *Pte |= X86_PTE_PRESENT;
    }

    if (VirtualAddress < KERNEL_VA_START) {
        MmpUpdateResidentSetCounter(&(AddressSpace->Common), 1);
    }

    return;
}

VOID
MmpUnmapPages (
    PVOID VirtualAddress,
    ULONG PageCount,
    ULONG UnmapFlags,
    PBOOL PageWasDirty
    )

/*++

Routine Description:

    This routine unmaps a portion of virtual address space.

Arguments:

    VirtualAddress - Supplies the address to unmap.

    PageCount - Supplies the number of pages to unmap.

    UnmapFlags - Supplies a bitmask of flags for the unmap operation. See
        UNMAP_FLAG_* for definitions. In the default case, this should contain
        UNMAP_FLAG_SEND_INVALIDATE_IPI. There are specific situations where
        it's known that this memory could not exist in another processor's TLB.

    PageWasDirty - Supplies an optional pointer where a boolean will be
        returned indicating if any of the pages were dirty.

Return Value:

    None.

--*/

{

    PADDRESS_SPACE_X64 AddressSpace;
    BOOL ChangedSomething;
    PVOID CurrentVirtual;
    BOOL InvalidateTlb;
    INTN MappedCount;
    ULONG PageNumber;
    BOOL PageWasPresent;
    PHYSICAL_ADDRESS PhysicalPage;
    PPTE Pml4;
    ULONG Pml4Index;
    PKPROCESS Process;
    PPTE Pte;
    PHYSICAL_ADDRESS RunPhysicalPage;
    UINTN RunSize;
    PKTHREAD Thread;

    ChangedSomething = FALSE;
    InvalidateTlb = TRUE;
    Thread = KeGetCurrentThread();
    if (Thread == NULL) {

        ASSERT(VirtualAddress >= KERNEL_VA_START);
        ASSERT(((UINTN)VirtualAddress + (PageCount << MmPageShift())) - 1 >
               (UINTN)VirtualAddress);

        Process = NULL;
        AddressSpace = NULL;

    } else {
        Process = Thread->OwningProcess;
        AddressSpace = (PADDRESS_SPACE_X64)(Process->AddressSpace);

        //
        // If there's only one thread in the process and this is not a kernel
        // mode address, then there's no need to send a TLB invalidate IPI.
        //

        if ((Process->ThreadCount <= 1) && (VirtualAddress < KERNEL_VA_START)) {
            UnmapFlags &= ~UNMAP_FLAG_SEND_INVALIDATE_IPI;
            if (Process->ThreadCount == 0) {
                InvalidateTlb = FALSE;
            }
        }
    }

    ASSERT(((UINTN)VirtualAddress & PAGE_MASK) == 0);

    //
    // Loop through once to turn them all off. Other processors may still have
    // TLB mappings to them, so the page is technically still in use.
    //

    MappedCount = 0;
    CurrentVirtual = VirtualAddress;
    for (PageNumber = 0; PageNumber < PageCount; PageNumber += 1) {
        Pml4 = X64_PML4T;
        Pml4Index = X64_PML4_INDEX(CurrentVirtual);
        if ((Pml4[Pml4Index] & X86_PTE_PRESENT) == 0) {
            if ((CurrentVirtual >= KERNEL_VA_START) &&
                ((MmKernelPml4[Pml4Index] & X86_PTE_PRESENT) != 0)) {

                Pml4[Pml4Index] = MmKernelPml4[Pml4Index];
            }

            if ((Pml4[Pml4Index] & X86_PTE_PRESENT) == 0) {
                CurrentVirtual += PAGE_SIZE;
                continue;
            }
        }

        if (((*X64_PDPE(CurrentVirtual) & X86_PTE_PRESENT) == 0) ||
            ((*X64_PDE(CurrentVirtual) & X86_PTE_PRESENT) == 0)) {

            CurrentVirtual += PAGE_SIZE;
            continue;
        }

        Pte = X64_PTE(CurrentVirtual);

        //
        // If the page was not present or physical pages aren't being freed,
        // just wipe the whole PTE out.
        //

        if (X86_PTE_ENTRY(*Pte) != 0) {
            PageWasPresent = FALSE;
            if ((*Pte & X86_PTE_PRESENT) != 0) {
                ChangedSomething = TRUE;
                PageWasPresent = TRUE;
            }

            MappedCount += 1;
            if (((UnmapFlags & UNMAP_FLAG_FREE_PHYSICAL_PAGES) == 0) &&
                (PageWasDirty == NULL)) {

                *Pte = 0;

            //
            // Otherwise, preserve the entry so the physical page can be freed
            // below.
            //

            } else {
                *Pte &= ~X86_PTE_PRESENT;
            }

            //
            // If an IPI is not going to be sent, clear the TLB entries on this
            // processor as they're unmapped, unless this is a user mode
            // address for a dying process (i.e. a process with no threads) or
            // the page was not actually mapped.
            //

            if ((PageWasPresent != FALSE) &&
                (InvalidateTlb != FALSE) &&
                ((UnmapFlags & UNMAP_FLAG_SEND_INVALIDATE_IPI) == 0)) {

                ArInvalidateTlbEntry(CurrentVirtual);
            }

        } else {

            ASSERT((*Pte & X86_PTE_PRESENT) == 0);
        }

        CurrentVirtual += PAGE_SIZE;
    }

    //
    // Send the invalidate IPI to get everyone faulting. After this the pages
    // can be taken offline.
    //

    if ((ChangedSomething != FALSE) &&
        ((UnmapFlags & UNMAP_FLAG_SEND_INVALIDATE_IPI) != 0)) {

        MmpSendTlbInvalidateIpi(&(AddressSpace->Common),
                                VirtualAddress,
                                PageCount);
    }

    //
    // Loop through again to free the physical pages or check if things were
    // dirty or writable.
    //

    if ((PageWasDirty != NULL) ||
        ((UnmapFlags & UNMAP_FLAG_FREE_PHYSICAL_PAGES) != 0)) {

        if (PageWasDirty != NULL) {
            *PageWasDirty = FALSE;
        }

        RunSize = 0;
        RunPhysicalPage = INVALID_PHYSICAL_ADDRESS;
        CurrentVirtual = VirtualAddress;
        for (PageNumber = 0; PageNumber < PageCount; PageNumber += 1) {
            if (((*X64_PML4E(CurrentVirtual) & X86_PTE_PRESENT) == 0) ||
                ((*X64_PDPE(CurrentVirtual) & X86_PTE_PRESENT) == 0) ||
                ((*X64_PDE(CurrentVirtual) & X86_PTE_PRESENT) == 0)) {

                CurrentVirtual += PAGE_SIZE;
                continue;
            }

            Pte = X64_PTE(CurrentVirtual);
            PhysicalPage = X86_PTE_ENTRY(*Pte);
            if (PhysicalPage == 0) {
                CurrentVirtual += PAGE_SIZE;
                continue;
            }

            if ((UnmapFlags & UNMAP_FLAG_FREE_PHYSICAL_PAGES) != 0) {
                if (RunSize != 0) {
                    if ((RunPhysicalPage + RunSize) == PhysicalPage) {
                        RunSize += PAGE_SIZE;

                    } else {
                        MmFreePhysicalPages(RunPhysicalPage,
                                            RunSize >> PAGE_SHIFT);

                        RunPhysicalPage = PhysicalPage;
                        RunSize = PAGE_SIZE;
                    }

                } else {
                    RunPhysicalPage = PhysicalPage;
                    RunSize = PAGE_SIZE;
                }
            }

            if ((PageWasDirty != NULL) && ((*Pte & X86_PTE_DIRTY) != 0)) {
                *PageWasDirty = TRUE;
            }

            *Pte = 0;
            CurrentVirtual += PAGE_SIZE;
        }

        if (RunSize != 0) {
            MmFreePhysicalPages(RunPhysicalPage, RunSize >> PAGE_SHIFT);
        }
    }

    if (VirtualAddress < KERNEL_VA_START) {
        MmpUpdateResidentSetCounter(&(AddressSpace->Common), -MappedCount);
    }

    return;
}

PHYSICAL_ADDRESS
MmpVirtualToPhysical (
    PVOID VirtualAddress,
    PULONG Attributes
    )

/*++

Routine Description:

    This routine returns the physical address corresponding to the given
    virtual address.

Arguments:

    VirtualAddress - Supplies the address to translate to a physical address.

    Attributes - Supplies an optional pointer where a bitfield of attributes
        will be returned. See MAP_FLAG_* definitions.

Return Value:

    Returns the physical address corresponding to the virtual address, or NULL
    if no mapping could be found.

--*/

{

    PHYSICAL_ADDRESS PhysicalAddress;
    PPTE Pml4;
    ULONG Pml4Index;
    PPTE Pte;

    if (Attributes != NULL) {
        *Attributes = 0;
    }

    Pml4 = X64_PML4T;
    Pml4Index = X64_PML4_INDEX(VirtualAddress);
    if ((Pml4[Pml4Index] & X86_PTE_PRESENT) == 0) {
        if (VirtualAddress >= KERNEL_VA_START) {
            Pml4[Pml4Index] = MmKernelPml4[Pml4Index];
        }

        if ((Pml4[Pml4Index] & X86_PTE_PRESENT) == 0) {
            return INVALID_PHYSICAL_ADDRESS;
        }
    }

    if (((*X64_PDPE(VirtualAddress) & X86_PTE_PRESENT) == 0) ||
        ((*X64_PDE(VirtualAddress) & X86_PTE_PRESENT) == 0)) {

        return INVALID_PHYSICAL_ADDRESS;
    }

    Pte = X64_PTE(VirtualAddress);
    PhysicalAddress = X86_PTE_ENTRY(*Pte);
    if (PhysicalAddress == 0) {

        ASSERT((*Pte & X86_PTE_PRESENT) == 0);

        return INVALID_PHYSICAL_ADDRESS;
    }

    PhysicalAddress += (UINTN)VirtualAddress & PAGE_MASK;
    if (Attributes != NULL) {
        if ((*Pte & X86_PTE_PRESENT) != 0) {
            *Attributes |= MAP_FLAG_PRESENT;
        }

        if ((*Pte & X86_PTE_WRITABLE) == 0) {
            *Attributes |= MAP_FLAG_READ_ONLY;
        }

        if ((*Pte & X86_PTE_DIRTY) != 0) {
            *Attributes |= MAP_FLAG_DIRTY;
        }

        if ((*Pte & X86_PTE_NX) == 0) {
            *Attributes |= MAP_FLAG_EXECUTE;
        }
    }

    return PhysicalAddress;
}

PHYSICAL_ADDRESS
MmpVirtualToPhysicalInOtherProcess (
    PADDRESS_SPACE AddressSpace,
    PVOID VirtualAddress
    )

/*++

Routine Description:

    This routine returns the physical address corresponding to the given
    virtual address that belongs to another process.

Arguments:

    AddressSpace - Supplies a pointer to the address space.

    VirtualAddress - Supplies the address to translate to a physical address.

Return Value:

    Returns the physical address corresponding to the virtual address, or NULL
    if no mapping could be found.

--*/

{

    RUNLEVEL OldRunLevel;
    PHYSICAL_ADDRESS Physical;
    PPROCESSOR_BLOCK Processor;
    PPTE Pte;

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    Pte = MmpGetOtherProcessPte((PADDRESS_SPACE_X64)AddressSpace,
                                VirtualAddress,
                                FALSE);

    if (Pte == NULL) {
        Physical = INVALID_PHYSICAL_ADDRESS;

    } else {
        Physical = X86_PTE_ENTRY(*Pte);
    }

    //
    // Unmap the swap page and return.
    //

    Processor = KeGetCurrentProcessorBlock();
    *(X64_PTE(Processor->SwapPage)) = 0;
    ArInvalidateTlbEntry(Processor->SwapPage);
    KeLowerRunLevel(OldRunLevel);
    return Physical;
}

VOID
MmpUnmapPageInOtherProcess (
    PADDRESS_SPACE AddressSpace,
    PVOID VirtualAddress,
    ULONG UnmapFlags,
    PBOOL PageWasDirty
    )

/*++

Routine Description:

    This routine unmaps a page of VA space from this process or another.

Arguments:

    AddressSpace - Supplies a pointer to the address space to unmap the page
        from.

    VirtualAddress - Supplies the virtual address of the page to unmap.

    UnmapFlags - Supplies a bitmask of flags for the unmap operation. See
        UNMAP_FLAG_* for definitions. This routine will always send an IPI
        after doing the unmap.

    PageWasDirty - Supplies an optional pointer where a boolean will be
        returned indicating if the pages were dirty.

Return Value:

    None.

--*/

{

    RUNLEVEL OldRunLevel;
    PPROCESSOR_BLOCK Processor;
    PPTE Pte;
    PTE PteValue;

    if (PageWasDirty != NULL) {
        *PageWasDirty = FALSE;
    }

    PteValue = 0;
    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    Pte = MmpGetOtherProcessPte((PADDRESS_SPACE_X64)AddressSpace,
                                VirtualAddress,
                                FALSE);

    if (Pte != NULL) {

        //
        // Take the page offline first explicitly since it could become dirty
        // anytime between reading it and taking it offline.
        //

        *Pte &= ~X86_PTE_PRESENT;
        RtlMemoryBarrier();
        PteValue = *Pte;
        *Pte = 0;
    }

    //
    // Unmap the swap page and return.
    //

    Processor = KeGetCurrentProcessorBlock();
    *(X64_PTE(Processor->SwapPage)) = 0;
    ArInvalidateTlbEntry(Processor->SwapPage);
    KeLowerRunLevel(OldRunLevel);

    //
    // Potentially free the physical page and send out TLB IPIs.
    //

    if (X86_PTE_ENTRY(PteValue) != 0) {
        if ((UnmapFlags & UNMAP_FLAG_FREE_PHYSICAL_PAGES) != 0) {
            MmFreePhysicalPage(X86_PTE_ENTRY(PteValue));
        }

        if ((PageWasDirty != NULL) && ((PteValue & X86_PTE_DIRTY) != 0)) {
            *PageWasDirty = TRUE;
        }

        MmpSendTlbInvalidateIpi(AddressSpace, VirtualAddress, 1);

        ASSERT(VirtualAddress < KERNEL_VA_START);

        MmpUpdateResidentSetCounter(AddressSpace, -1);
    }

    return;
}

VOID
MmpMapPageInOtherProcess (
    PADDRESS_SPACE AddressSpace,
    PHYSICAL_ADDRESS PhysicalAddress,
    PVOID VirtualAddress,
    ULONG MapFlags,
    BOOL SendTlbInvalidateIpi
    )

/*++

Routine Description:

    This routine maps a physical page of memory into the virtual address space
    of another process.

Arguments:

    AddressSpace - Supplies a pointer to the address space to map the page in.

    PhysicalAddress - Supplies the physical address to back the mapping with.

    VirtualAddress - Supplies the virtual address to map the physical page to.

    MapFlags - Supplies a bitfield of flags governing the mapping. See
        MAP_FLAG_* definitions.

    SendTlbInvalidateIpi - Supplies a boolean indicating whether a TLB
        invalidate IPI needs to be sent out for this mapping. If in doubt,
        specify TRUE.

Return Value:

    None.

--*/

{

    ULONG MappedCount;
    RUNLEVEL OldRunLevel;
    PPROCESSOR_BLOCK Processor;
    PPTE Pte;

    //
    // This routine should be called from low level because it may return down
    // to low level to allocate page tables.
    //

    ASSERT(KeGetRunLevel() == RunLevelLow);

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    Pte = MmpGetOtherProcessPte((PADDRESS_SPACE_X64)AddressSpace,
                                VirtualAddress,
                                TRUE);

    if (Pte == NULL) {

        //
        // This should really be handled more gracefully. Perhaps send a
        // signal to the process.
        //

        KeCrashSystem(CRASH_OUT_OF_MEMORY, 0, 0, 0, 0);
        return;
    }

    //
    // This VA better be unmapped unless the caller requested an TLB
    // invalidation.
    //

    if (X86_PTE_ENTRY(*Pte) != 0) {
        MappedCount = 0;

        ASSERT(SendTlbInvalidateIpi != FALSE);

        if ((*Pte & X86_PTE_PRESENT) == 0) {
            SendTlbInvalidateIpi = FALSE;
        }

    } else {
        MappedCount = 1;
        SendTlbInvalidateIpi = FALSE;

        ASSERT((*Pte & X86_PTE_PRESENT) == 0);
    }

    *Pte = PhysicalAddress;
    if ((MapFlags & MAP_FLAG_READ_ONLY) == 0) {
        *Pte |= X86_PTE_WRITABLE;
    }

    if ((MapFlags & MAP_FLAG_WRITE_THROUGH) != 0) {
        *Pte |= X86_PTE_WRITE_THROUGH;
    }

    if ((MapFlags & MAP_FLAG_CACHE_DISABLE) != 0) {
        *Pte |= X86_PTE_CACHE_DISABLED;
    }

    ASSERT((MapFlags & MAP_FLAG_LARGE_PAGE) == 0);
    ASSERT(((MapFlags & MAP_FLAG_USER_MODE) != 0) &&
           (VirtualAddress < (PVOID)X64_CANONICAL_LOW));

    if ((MapFlags & MAP_FLAG_USER_MODE) != 0) {
        *Pte |= X86_PTE_USER_MODE;

    } else if ((MapFlags & MAP_FLAG_GLOBAL) != 0) {
        *Pte |= X86_PTE_GLOBAL;
    }

    if ((MapFlags & MAP_FLAG_EXECUTE) == 0) {
        *Pte |= X86_PTE_NX;
    }

    if ((MapFlags & MAP_FLAG_PRESENT) != 0) {
        *Pte |= X86_PTE_PRESENT;
    }

    Processor = KeGetCurrentProcessorBlock();
    *(X64_PTE(Processor->SwapPage)) = 0;
    ArInvalidateTlbEntry(Processor->SwapPage);
    KeLowerRunLevel(OldRunLevel);

    //
    // If requested, send a TLB invalidate IPI. This routine can be used for
    // remap, in which case the virtual address never got invalidated.
    //

    if (SendTlbInvalidateIpi != FALSE) {
        MmpSendTlbInvalidateIpi(AddressSpace, VirtualAddress, 1);
    }

    ASSERT(VirtualAddress < (PVOID)X64_CANONICAL_LOW);

    if (MappedCount != 0) {
        MmpUpdateResidentSetCounter(AddressSpace, MappedCount);
    }

    return;
}

VOID
MmpChangeMemoryRegionAccess (
    PVOID VirtualAddress,
    ULONG PageCount,
    ULONG MapFlags,
    ULONG MapFlagsMask
    )

/*++

Routine Description:

    This routine changes whether or not writes are allowed in the given VA
    range. This routine skips any pages in the range that are not mapped.

Arguments:

    VirtualAddress - Supplies the page-aligned beginning of the virtual address
        range to change.

    PageCount - Supplies the number of pages in the range to change.

    MapFlags - Supplies the bitfield of MAP_FLAG_* values to set. Only
        present, read-only, and execute can be changed.

    MapFlagsMask - Supplies the bitfield of supplied MAP_FLAG_* values that are
        valid. If in doubt, use MAP_FLAG_ALL_MASK to make all values valid.

Return Value:

    None.

--*/

{

    PADDRESS_SPACE AddressSpace;
    BOOL ChangedSomething;
    PVOID CurrentVirtual;
    PVOID End;
    BOOL InvalidateTlb;
    PPTE Pml4;
    ULONG Pml4Index;
    PKPROCESS Process;
    PPTE Pte;
    PTE PteMask;
    PTE PteValue;
    BOOL SendInvalidateIpi;

    InvalidateTlb = TRUE;
    SendInvalidateIpi = TRUE;
    End = VirtualAddress + (PageCount << PAGE_SHIFT);
    Process = PsGetKernelProcess();
    AddressSpace = Process->AddressSpace;
    if (End < KERNEL_VA_START) {

        //
        // If there's only one thread in the process, then there's no need to
        // send a TLB invalidate IPI for this user mode address.
        //

        if (Process->ThreadCount <= 1) {
            SendInvalidateIpi = FALSE;
            if (Process->ThreadCount == 0) {
                InvalidateTlb = FALSE;
            }
        }
    }

    //
    // Figure out which PTE bits are important and what they should be.
    //

    PteMask = 0;
    PteValue = 0;
    if ((MapFlagsMask & MAP_FLAG_PRESENT) != 0) {
        PteMask |= X86_PTE_PRESENT;
        if ((MapFlags & MAP_FLAG_PRESENT) != 0) {
            PteValue |= X86_PTE_PRESENT;
        }
    }

    if ((MapFlagsMask & MAP_FLAG_READ_ONLY) != 0) {
        PteMask |= X86_PTE_WRITABLE;
        if ((MapFlags & MAP_FLAG_READ_ONLY) == 0) {
            PteValue |= X86_PTE_WRITABLE;
        }
    }

    if ((MapFlagsMask & MAP_FLAG_EXECUTE) != 0) {
        PteMask |= X86_PTE_NX;
        if ((MapFlags & MAP_FLAG_EXECUTE) == 0) {
            PteValue |= X86_PTE_NX;
        }
    }

    CurrentVirtual = VirtualAddress;
    while (CurrentVirtual < End) {
        Pml4Index = X64_PML4_INDEX(CurrentVirtual);
        Pml4 = X64_PML4T;
        if (CurrentVirtual > KERNEL_VA_START) {
            if (X86_PTE_ENTRY(Pml4[Pml4Index]) == 0) {
                Pml4[Pml4Index] = MmKernelPml4[Pml4Index];
            }
        }

        if (X86_PTE_ENTRY(Pml4[Pml4Index]) == 0) {
            CurrentVirtual = ALIGN_POINTER_UP(CurrentVirtual + PAGE_SIZE,
                                              1ULL << X64_PML4E_SHIFT);

            continue;
        }

        Pte = X64_PDPE(CurrentVirtual);
        if (X86_PTE_ENTRY(*Pte) == 0) {
            CurrentVirtual = ALIGN_POINTER_UP(CurrentVirtual + PAGE_SIZE,
                                              1ULL << X64_PDPE_SHIFT);

            continue;
        }

        Pte = X64_PDE(CurrentVirtual);
        if (X86_PTE_ENTRY(*Pte) == 0) {
            CurrentVirtual = ALIGN_POINTER_UP(CurrentVirtual + PAGE_SIZE,
                                              1ULL << X64_PDE_SHIFT);

            continue;
        }

        Pte = X64_PTE(CurrentVirtual);
        if (X86_PTE_ENTRY(*Pte) == 0) {

            ASSERT((*Pte & X86_PTE_PRESENT) == 0);

            CurrentVirtual += PAGE_SIZE;
            continue;
        }

        //
        // Set the new attributes.
        //

        if ((*Pte & PteMask) != PteValue) {
            *Pte = (*Pte & ~PteMask) | PteValue;
            if (SendInvalidateIpi == FALSE) {
                if (InvalidateTlb != FALSE) {
                    ArInvalidateTlbEntry(CurrentVirtual);
                }

            } else {
                if (ChangedSomething == FALSE) {
                    ChangedSomething = TRUE;
                    VirtualAddress = CurrentVirtual;
                    PageCount = (End - CurrentVirtual) >> PAGE_SHIFT;
                }

            }
        }

        CurrentVirtual += PAGE_SIZE;
    }

    //
    // Send an invalidate IPI if any mappings were changed.
    //

    if (ChangedSomething != FALSE) {

        ASSERT(SendInvalidateIpi != FALSE);

        MmpSendTlbInvalidateIpi(AddressSpace, VirtualAddress, PageCount);
    }

    return;
}

KSTATUS
MmpPreallocatePageTables (
    PADDRESS_SPACE SourceAddressSpace,
    PADDRESS_SPACE DestinationAddressSpace
    )

/*++

Routine Description:

    This routine allocates, but does not initialize nor fully map the page
    tables for a process that is being forked. It is needed because physical
    page allocations are not allowed while an image section lock is held.

Arguments:

    SourceAddressSpace - Supplies a pointer to the address space to prepare to
        copy. A page table is allocated but not initialized for every missing
        page table in the destination.

    DestinationAddressSpace - Supplies a pointer to the destination to create
        the page tables in.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES on failure.

--*/

{

    PADDRESS_SPACE_X64 DestinationSpace;
    PHYSICAL_ADDRESS LocalPages[16];
    RUNLEVEL OldRunLevel;
    UINTN PageCount;
    UINTN PageIndex;
    PPHYSICAL_ADDRESS Pages;
    PPTE Pd;
    ULONG PdIndex;
    PPTE Pdp;
    ULONG PdpIndex;
    ULONG Pml4Index;
    PPROCESSOR_BLOCK Processor;
    UINTN PtCount;
    PPTE Pte;
    PTE SavedPdp;
    PADDRESS_SPACE_X64 SourceSpace;
    KSTATUS Status;
    PPTE SwapPte;

    DestinationSpace = (PADDRESS_SPACE_X64)DestinationAddressSpace;
    SourceSpace = (PADDRESS_SPACE_X64)SourceAddressSpace;
    PageCount = SourceSpace->ActivePageTables;
    if (PageCount <= (sizeof(LocalPages) / sizeof(LocalPages[0]))) {
        Pages = LocalPages;

    } else {
        Pages = MmAllocateNonPagedPool(PageCount * sizeof(PHYSICAL_ADDRESS),
                                       MM_ADDRESS_SPACE_ALLOCATION_TAG);
    }

    Status = MmpAllocateScatteredPhysicalPages(0, MAX_UINTN, Pages, PageCount);
    if (!KSUCCESS(Status)) {
        goto PreallocatePageTablesEnd;
    }

    PageIndex = 0;
    PtCount = 0;
    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    Processor = KeGetCurrentProcessorBlock();
    Pte = Processor->SwapPage;
    SwapPte = X64_PTE(Pte);

    ASSERT(*SwapPte == 0);

    //
    // Start by mapping the PML4 table, and looking for valid entries. The
    // idea is that each inner loop "borrows" the swap PTE region for its
    // inner table, but also restores it before the outer loop resumes.
    //

    *SwapPte = DestinationSpace->Pml4Physical |
               X86_PTE_PRESENT | X86_PTE_WRITABLE;

    for (Pml4Index = 0;
         Pml4Index < X64_PML4_INDEX(X64_CANONICAL_LOW + 1);
         Pml4Index += 1) {

        if ((X64_PML4T[Pml4Index] & X86_PTE_PRESENT) == 0) {
            continue;
        }

        //
        // Allocate and initialize a new PDPT.
        //

        ASSERT(PageIndex < PageCount);

        Pte[Pml4Index] = Pages[PageIndex] | X86_PTE_PRESENT | X86_PTE_WRITABLE;
        PageIndex += 1;
        *SwapPte = Pte[Pml4Index];
        SavedPdp = *SwapPte;
        ArInvalidateTlbEntry(Pte);
        RtlZeroMemory(Pte, PAGE_SIZE);
        Pdp = X64_PDPT((UINTN)Pml4Index << X64_PDPE_SHIFT);
        for (PdpIndex = 0; PdpIndex < X64_PTE_COUNT; PdpIndex += 1) {
            if ((Pdp[Pml4Index] & X86_PTE_PRESENT) == 0) {
                continue;
            }

            //
            // Allocate and initialize a new PD.
            //

            ASSERT(PageIndex < PageCount);

            Pte[PdpIndex] = Pages[PageIndex] |
                            X86_PTE_PRESENT | X86_PTE_WRITABLE;

            PageIndex += 1;
            *SwapPte = Pte[PdpIndex];
            ArInvalidateTlbEntry(Pte);
            RtlZeroMemory(Pte, PAGE_SIZE);
            Pd = X64_PDT(((UINTN)Pml4Index << X64_PDPE_SHIFT) |
                         ((UINTN)PdpIndex << X64_PDE_SHIFT));

            for (PdIndex = 0; PdIndex < X64_PTE_COUNT; PdIndex += 1) {
                if ((Pd[PdIndex] & X86_PTE_PRESENT) == 0) {
                    continue;
                }

                //
                // Allocate but don't bother zeroing a new PT.
                //

                ASSERT(PageIndex < PageCount);

                Pte[PdIndex] = Pages[PageIndex];
                PageIndex += 1;
                PtCount += 1;
            }

            //
            // Restore the PDP mapping.
            //

            *SwapPte = SavedPdp;
            ArInvalidateTlbEntry(Pte);
        }

        //
        // Restore the PML4 mapping.
        //

        *SwapPte = DestinationSpace->Pml4Physical |
                   X86_PTE_PRESENT | X86_PTE_WRITABLE;

        ArInvalidateTlbEntry(Pte);
    }

    *SwapPte = 0;
    ArInvalidateTlbEntry(Pte);

    //
    // The page table accounting had better be correct, otherwise physical
    // pages will be leaked.
    //

    ASSERT(PageIndex == PageCount);

    //
    // Don't count the lowest level page tables, since they're not live yet.
    //

    DestinationSpace->AllocatedPageTables = PageIndex;
    DestinationSpace->ActivePageTables = PageIndex - PtCount;
    KeLowerRunLevel(OldRunLevel);

PreallocatePageTablesEnd:
    if ((Pages != NULL) && (Pages != LocalPages)) {
        MmFreeNonPagedPool(Pages);
    }

    return Status;
}

KSTATUS
MmpCopyAndChangeSectionMappings (
    PADDRESS_SPACE Destination,
    PADDRESS_SPACE Source,
    PVOID VirtualAddress,
    UINTN Size
    )

/*++

Routine Description:

    This routine converts all the mappings of the given virtual address region
    to read-only, and copies those read-only mappings to another process.

Arguments:

    Destination - Supplies a pointer to the destination address space.

    Source - Supplies a pointer to the source address space.

    VirtualAddress - Supplies the starting virtual address of the memory range.

    Size - Supplies the size of the virtual address region, in bytes.

Return Value:

    None.

--*/

{

    PADDRESS_SPACE_X64 DestinationSpace;
    INTN MappedCount;
    RUNLEVEL OldRunLevel;
    PPTE Pd;
    PVOID PdEnd;
    ULONG PdIndex;
    PPTE Pdp;
    PVOID PdpEnd;
    ULONG PdpIndex;
    PVOID PdpStart;
    PVOID PdStart;
    PVOID Pml4End;
    ULONG Pml4Index;
    PVOID Pml4Start;
    PPROCESSOR_BLOCK Processor;
    PPTE Pt;
    PPTE Pte;
    ULONG PtEnd;
    ULONG PtIndex;
    ULONG PtStart;
    PTE SavedPd;
    PTE SavedPdp;
    PPTE SwapPte;
    PVOID VirtualEnd;

    DestinationSpace = (PADDRESS_SPACE_X64)Destination;
    MappedCount = 0;
    VirtualEnd = VirtualAddress + Size - 1;

    ASSERT((VirtualEnd > VirtualAddress) &&
           (VirtualEnd < (PVOID)X64_CANONICAL_LOW));

    //
    // It is assumed that all image sections are page aligned in base address
    // and size.
    //

    ASSERT((IS_POINTER_ALIGNED(VirtualAddress, PAGE_SIZE)) &&
           (IS_POINTER_ALIGNED(VirtualEnd + 1, PAGE_SIZE)));

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    Processor = KeGetCurrentProcessorBlock();
    Pte = Processor->SwapPage;
    SwapPte = X64_PTE(Pte);

    ASSERT(*SwapPte == 0);

    *SwapPte = DestinationSpace->Pml4Physical | X86_PTE_PRESENT;
    for (Pml4Index = X64_PML4_INDEX(VirtualAddress);
         Pml4Index <= X64_PML4_INDEX(VirtualEnd);
         Pml4Index += 1) {

        if ((X64_PML4T[Pml4Index] & X86_PTE_PRESENT) == 0) {
            continue;
        }

        Pml4Start = (PVOID)((UINTN)Pml4Index << X64_PML4E_SHIFT);
        Pml4End = Pml4Start + (1ULL << X64_PML4E_SHIFT) - 1;
        if (Pml4Start < VirtualAddress) {
            Pml4Start = VirtualAddress;
        }

        if (Pml4End > VirtualEnd) {
            Pml4End = VirtualEnd;
        }

        //
        // Map in and drill into the PDP.
        //

        *SwapPte = Pte[Pml4Index];
        SavedPdp = *SwapPte;
        ArInvalidateTlbEntry(Pte);
        Pdp = X64_PDPT((UINTN)Pml4Index << X64_PDPE_SHIFT);
        for (PdpIndex = X64_PDP_INDEX(Pml4Start);
             PdpIndex <= X64_PDP_INDEX(Pml4End);
             PdpIndex += 1) {

            if ((Pdp[PdpIndex] & X86_PTE_PRESENT) == 0) {
                continue;
            }

            PdpStart = (PVOID)(((UINTN)Pml4Index << X64_PML4E_SHIFT) |
                               ((UINTN)PdpIndex << X64_PDPE_SHIFT));

            PdpEnd = PdpStart + (1ULL << X64_PDPE_SHIFT) - 1;
            if (PdpStart < VirtualAddress) {
                PdpStart = VirtualAddress;
            }

            if (PdpEnd > VirtualEnd) {
                PdpEnd = VirtualEnd;
            }

            //
            // Map in and drill into the PD.
            //

            *SwapPte = Pte[PdpIndex];
            SavedPd = *SwapPte;
            ArInvalidateTlbEntry(Pte);
            Pd = X64_PDT(((UINTN)Pml4Index << X64_PDPE_SHIFT) |
                         ((UINTN)PdpIndex << X64_PDE_SHIFT));

            for (PdIndex = X64_PD_INDEX(PdpStart);
                 PdIndex <= X64_PD_INDEX(PdpEnd);
                 PdIndex += 1) {

                if ((Pd[PdIndex] & X86_PTE_PRESENT) == 0) {
                    continue;
                }

                PdStart = (PVOID)(((UINTN)Pml4Index << X64_PML4E_SHIFT) |
                                  ((UINTN)PdpIndex << X64_PDPE_SHIFT) |
                                  ((UINTN)PdIndex << X64_PDE_SHIFT));

                PdEnd = PdStart + (1ULL << X64_PDE_SHIFT) - 1;
                if (PdStart < VirtualAddress) {
                    PdStart = VirtualAddress;
                }

                if (PdEnd > VirtualEnd) {
                    PdEnd = VirtualEnd;
                }

                //
                // Finally, map in and drill into the PT. If the PT has not yet
                // been mapped, zero out the parts that don't apply to this
                // region. This was deferred to avoid zeroing out a page table
                // only to fill it all up with mappings.
                //

                PtStart = X64_PT_INDEX(PdStart);
                PtEnd = X64_PT_INDEX(PdEnd);
                if ((Pte[PdIndex] & X86_PTE_PRESENT) == 0) {

                    //
                    // The preallocate page tables function should have
                    // allocated a page and left it here.
                    //

                    ASSERT(X86_PTE_ENTRY(Pte[PdIndex]) != 0);

                    Pte[PdIndex] |= X86_PTE_PRESENT | X86_PTE_WRITABLE;
                    *SwapPte = Pte[PdIndex];
                    ArInvalidateTlbEntry(Pte);
                    DestinationSpace->ActivePageTables += 1;
                    if (PtStart != 0) {
                        RtlZeroMemory(Pte, PtStart * sizeof(PTE));
                    }

                    //
                    // The end PT index is inclusive, so zero out everything
                    // after it.
                    //

                    if ((PtEnd + 1) < X64_PTE_COUNT) {
                        RtlZeroMemory(
                                  &(Pte[PtEnd + 1]),
                                  (X64_PTE_COUNT - (PtEnd + 1)) * sizeof(PTE));
                    }

                //
                // This page table has been around the block. No need to zero
                // anything out.
                //

                } else {
                    *SwapPte = Pte[PdIndex];
                    ArInvalidateTlbEntry(Pte);
                }

                //
                // As promised in the title, copy and change section mappings.
                //

                Pt = X64_PT(((UINTN)Pml4Index << X64_PDPE_SHIFT) |
                            ((UINTN)PdpIndex << X64_PDE_SHIFT) |
                            ((UINTN)PdIndex << X64_PTE_SHIFT));

                for (PtIndex = PtStart; PtIndex <= PtEnd; PtIndex += 1) {
                    if (X86_PTE_ENTRY(Pte[PtIndex]) != 0) {
                        Pt[PtIndex] &= ~X86_PTE_WRITABLE;
                        Pte[PtIndex] = Pt[PtIndex] & ~X86_PTE_DIRTY;
                    }
                }

                //
                // Restore the PD mapping.
                //

                *SwapPte = SavedPd;
                ArInvalidateTlbEntry(Pte);
            }

            //
            // Restore the PDP mapping.
            //

            *SwapPte = SavedPdp;
            ArInvalidateTlbEntry(Pte);
        }

        //
        // Restore the PML4 mapping.
        //

        *SwapPte = DestinationSpace->Pml4Physical | X86_PTE_PRESENT;
        ArInvalidateTlbEntry(Pte);
    }

    *SwapPte = 0;
    ArInvalidateTlbEntry(Pte);
    KeLowerRunLevel(OldRunLevel);
    MmpUpdateResidentSetCounter(&(DestinationSpace->Common), MappedCount);
    return STATUS_SUCCESS;
}

VOID
MmpCreatePageTables (
    PVOID VirtualAddress,
    UINTN Size
    )

/*++

Routine Description:

    This routine ensures that all page tables are present for the given virtual
    address range.

Arguments:

    VirtualAddress - Supplies the starting virtual address of the memory range.

    Size - Supplies the size of the virtual address region, in bytes.

Return Value:

    None.

--*/

{

    PADDRESS_SPACE_X64 AddressSpace;
    PVOID End;
    PPTE Pd;
    ULONG PdIndex;
    PPTE Pdp;
    PVOID PdpEnd;
    ULONG PdpIndex;
    PVOID PdpStart;
    PVOID Pml4End;
    ULONG Pml4Index;
    PVOID Pml4Start;
    PKPROCESS Process;
    KSTATUS Status;

    Process = PsGetCurrentProcess();
    AddressSpace = (PADDRESS_SPACE_X64)(Process->AddressSpace);
    End = VirtualAddress + Size - 1;

    ASSERT(End > VirtualAddress);

    for (Pml4Index = X64_PML4_INDEX(VirtualAddress);
         Pml4Index <= X64_PML4_INDEX(End);
         Pml4Index += 1) {

        //
        // Make sure the top level table is in sync with the kernel's for
        // kernel addresses.
        //

        if (Pml4Index >= X64_PML4_INDEX(KERNEL_VA_START)) {
            X64_PML4T[Pml4Index] = MmKernelPml4[Pml4Index];
        }

        if ((X64_PML4T[Pml4Index] & X86_PTE_PRESENT) == 0) {
            Status = MmpCreatePageTable(AddressSpace,
                                        &(X64_PML4T[Pml4Index]),
                                        INVALID_PHYSICAL_ADDRESS);

            if (!KSUCCESS(Status)) {
                goto CreatePageTablesEnd;
            }
        }

        Pml4Start = (PVOID)((UINTN)Pml4Index << X64_PML4E_SHIFT);
        Pml4End = Pml4Start + (1ULL << X64_PML4E_SHIFT) - 1;
        if (Pml4Start < VirtualAddress) {
            Pml4Start = VirtualAddress;
        }

        if (Pml4End > End) {
            Pml4End = End;
        }

        Pdp = X64_PDPT((UINTN)Pml4Index << X64_PDPE_SHIFT);
        for (PdpIndex = X64_PDP_INDEX(Pml4Start);
             PdpIndex <= X64_PDP_INDEX(Pml4End);
             PdpIndex += 1) {

            PdpStart = (PVOID)(((UINTN)Pml4Index << X64_PML4E_SHIFT) |
                               ((UINTN)PdpIndex << X64_PDPE_SHIFT));

            PdpEnd = PdpStart + (1ULL << X64_PDPE_SHIFT) - 1;
            if (PdpStart < VirtualAddress) {
                PdpStart = VirtualAddress;
            }

            if (PdpEnd > End) {
                PdpEnd = End;
            }

            if ((Pdp[PdpIndex] & X86_PTE_PRESENT) == 0) {
                Status = MmpCreatePageTable(AddressSpace,
                                            &(Pdp[PdpIndex]),
                                            INVALID_PHYSICAL_ADDRESS);

                if (!KSUCCESS(Status)) {
                    goto CreatePageTablesEnd;
                }
            }

            Pd = X64_PDT(((UINTN)Pml4Index << X64_PDPE_SHIFT) |
                         ((UINTN)PdpIndex << X64_PDE_SHIFT));

            for (PdIndex = X64_PD_INDEX(PdpStart);
                 PdIndex <= X64_PD_INDEX(PdpEnd);
                 PdIndex += 1) {

                if ((Pd[PdIndex] & X86_PTE_PRESENT) == 0) {
                    Status = MmpCreatePageTable(AddressSpace,
                                                &(Pd[PdIndex]),
                                                INVALID_PHYSICAL_ADDRESS);

                    if (!KSUCCESS(Status)) {
                        goto CreatePageTablesEnd;
                    }
                }
            }
        }
    }

    Status = STATUS_SUCCESS;

CreatePageTablesEnd:

    //
    // TODO: Handle failure to create page tables.
    //

    ASSERT(KSUCCESS(Status));

    return;
}

VOID
MmpTearDownPageTables (
    PADDRESS_SPACE_X64 AddressSpace
    )

/*++

Routine Description:

    This routine tears down all the page tables for the given address space
    in user mode while the process is still live (but exiting).

Arguments:

    AddressSpace - Supplies a pointer to the address space being torn down.

Return Value:

    None.

--*/

{

    PHYSICAL_ADDRESS Entry;
    INTN Inactive;
    PPTE Pd;
    ULONG PdIndex;
    PPTE Pdp;
    ULONG PdpIndex;
    ULONG Pml4Index;
    INTN Total;

    ASSERT(AddressSpace ==
           (PADDRESS_SPACE_X64)(PsGetCurrentProcess()->AddressSpace));

    Inactive = 0;
    Total = 0;
    for (Pml4Index = X64_PML4_INDEX(0);
         Pml4Index <= X64_PML4_INDEX(X64_CANONICAL_LOW);
         Pml4Index += 1) {

        if ((X64_PML4T[Pml4Index] & X86_PTE_PRESENT) == 0) {
            continue;
        }

        Pdp = X64_PDPT((UINTN)Pml4Index << X64_PDPE_SHIFT);
        for (PdpIndex = 0; PdpIndex <= X64_PTE_COUNT; PdpIndex += 1) {
            if ((Pdp[PdpIndex] & X86_PTE_PRESENT) == 0) {
                continue;
            }

            Pd = X64_PDT(((UINTN)Pml4Index << X64_PDPE_SHIFT) |
                         ((UINTN)PdpIndex << X64_PDE_SHIFT));

            for (PdIndex = 0; PdIndex <= X64_PTE_COUNT; PdIndex += 1) {
                Entry = X86_PTE_ENTRY(Pd[PdIndex]);
                if (Entry == 0) {
                    continue;
                }

                //
                // Free the page table, which might either be active or
                // inactive.
                //

                if ((Pd[PdIndex] & X86_PTE_PRESENT) == 0) {
                    Inactive += 1;
                }

                MmFreePhysicalPage(X86_PTE_ENTRY(Pd[PdIndex]));
                Pd[PdIndex] = 0;
                Total += 1;
            }

            //
            // Free the page directory, which should always be active.
            //

            MmFreePhysicalPage(X86_PTE_ENTRY(Pdp[PdpIndex]));
            Pdp[PdpIndex] = 0;
            Total += 1;
        }

        //
        // Free the page directory pointer, which should always be active.
        //

        MmFreePhysicalPage(X86_PTE_ENTRY(X64_PML4T[Pml4Index]));
        X64_PML4T[Pml4Index] = 0;
        Total += 1;
    }

    ASSERT(Total == AddressSpace->AllocatedPageTables);
    ASSERT((Total - Inactive) == AddressSpace->ActivePageTables);

    AddressSpace->AllocatedPageTables -= Total;
    AddressSpace->ActivePageTables -= Total - Inactive;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
MmpCreatePageDirectory (
    PADDRESS_SPACE_X64 AddressSpace
    )

/*++

Routine Description:

    This routine creates a new page directory for a new address space, and
    initializes it with kernel address space.

Arguments:

    AddressSpace - Supplies a pointer to the address space to create a page
        directory for.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_MEMORY if memory could not be allocated for the page table.

--*/

{

    RUNLEVEL OldRunLevel;
    PHYSICAL_ADDRESS Physical;
    PPROCESSOR_BLOCK Processor;
    PPTE Pte;
    ULONG SplitIndex;
    PPTE SwapPte;

    //
    // TODO: Handle the first call representing the kernel address space.
    //

    Physical = MmpAllocatePhysicalPages(1, 0);
    if (Physical == INVALID_PHYSICAL_ADDRESS) {
        return STATUS_NO_MEMORY;
    }

    SplitIndex = X64_PML4_INDEX(KERNEL_VA_START);

    //
    // Use the processor's swap space to map and initialize the PML4.
    //

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    Processor = KeGetCurrentProcessorBlock();
    Pte = Processor->SwapPage;
    SwapPte = X64_PTE(Pte);

    ASSERT(*SwapPte == 0);

    *SwapPte = Physical | X86_PTE_PRESENT | X86_PTE_WRITABLE;

    //
    // Zero out the user mode part, and copy the kernel mappings.
    //

    RtlZeroMemory(Pte, SplitIndex * sizeof(PTE));
    RtlCopyMemory(&(Pte[SplitIndex]),
                  (PVOID)&(MmKernelPml4[SplitIndex]),
                  (X64_PTE_COUNT - SplitIndex) * sizeof(PTE));

    //
    // Activate the self map.
    //

    Pte[X64_SELF_MAP_INDEX] =
                    Physical | X86_PTE_PRESENT | X86_PTE_WRITABLE | X86_PTE_NX;

    *SwapPte = 0;
    ArInvalidateTlbEntry(Pte);
    KeLowerRunLevel(OldRunLevel);
    AddressSpace->Pml4Physical = Physical;
    return STATUS_SUCCESS;
}

VOID
MmpDestroyPageDirectory (
    PADDRESS_SPACE_X64 AddressSpace
    )

/*++

Routine Description:

    This routine destroys a page directory upon address space destruction.

Arguments:

    AddressSpace - Supplies a pointer to the address space being torn down.

Return Value:

    None.

--*/

{

    //
    // TODO: Call MmpTearDownPageTables clean up the page tables while the
    // process is still hot to avoid awkward mappings of tables via the swap
    // space.
    //

    ASSERT(AddressSpace->AllocatedPageTables == 0);
    ASSERT(AddressSpace->ActivePageTables == 0);

    MmFreePhysicalPage(AddressSpace->Pml4Physical);
    AddressSpace->Pml4Physical = INVALID_PHYSICAL_ADDRESS;
    return;
}

PPTE
MmpGetOtherProcessPte (
    PADDRESS_SPACE_X64 AddressSpace,
    PVOID VirtualAddress,
    BOOL Create
    )

/*++

Routine Description:

    This routine fetches the PTE for an address in another process. It must be
    called at dispatch level, though it may temporarily lower to create page
    tables.

Arguments:

    AddressSpace - Supplies a pointer to the foreign address space.

    VirtualAddress - Supplies the virtual address to create page tables for.

    Create - Supplies a boolean indicating if page tables should be created if
        they do not exist.

Return Value:

    Returns a pointer to the PTE within the current processor's swap page on
    success.

--*/

{

    ULONG EntryShift;
    ULONG Index;
    ULONG Level;
    PHYSICAL_ADDRESS NextTable;
    PHYSICAL_ADDRESS Physical;
    PPROCESSOR_BLOCK Processor;
    PPTE Pte;
    PVOID SwapPage;
    PPTE SwapPte;

    ASSERT(KeGetRunLevel() == RunLevelDispatch);

    Processor = KeGetCurrentProcessorBlock();
    SwapPage = Processor->SwapPage;
    SwapPte = X64_PTE(SwapPage);

    ASSERT(*SwapPte == 0);

    EntryShift = X64_PML4E_SHIFT;
    Physical = AddressSpace->Pml4Physical;
    for (Level = 0; Level < X64_PAGE_LEVEL - 1; Level += 1) {
        *SwapPte = Physical | X86_PTE_PRESENT | X86_PTE_WRITABLE;
        Index = ((UINTN)VirtualAddress >> EntryShift) & X64_PT_MASK;
        EntryShift -= X64_PTE_BITS;
        Pte = (PPTE)SwapPage + Index;
        NextTable = X86_PTE_ENTRY(*Pte);
        if (NextTable == 0) {
            if (Create == FALSE) {
                *SwapPte = 0;
                ArInvalidateTlbEntry(SwapPage);
                return NULL;
            }

            //
            // Undo everything, lower back down, and allocate a page for the
            // new page table.
            //

            *SwapPte = 0;
            ArInvalidateTlbEntry(SwapPage);
            KeLowerRunLevel(RunLevelLow);
            NextTable = MmpAllocatePhysicalPages(1, 0);
            KeRaiseRunLevel(RunLevelDispatch);
            if (NextTable == INVALID_PHYSICAL_ADDRESS) {
                return NULL;
            }

            Processor = KeGetCurrentProcessorBlock();
            SwapPage = Processor->SwapPage;
            SwapPte = X64_PTE(SwapPage);

            ASSERT(*SwapPte == 0);

            *SwapPte = Physical | X86_PTE_PRESENT | X86_PTE_WRITABLE;
            Pte = (PPTE)SwapPage + Index;
            MmpCreatePageTable(AddressSpace, Pte, NextTable);

            //
            // If the page wasn't even used, go down again to free it. Sad.
            //

            Physical = X86_PTE_ENTRY(*Pte);
            if (Physical != NextTable) {
                *SwapPte = 0;
                ArInvalidateTlbEntry(SwapPage);
                KeLowerRunLevel(RunLevelLow);
                MmFreePhysicalPage(NextTable);
                KeRaiseRunLevel(RunLevelDispatch);
                Processor = KeGetCurrentProcessorBlock();
                SwapPage = Processor->SwapPage;
                SwapPte = X64_PTE(SwapPage);

                ASSERT(*SwapPte == 0);
            }

        } else {
            Physical = NextTable;
            *SwapPte = 0;
            ArInvalidateTlbEntry(SwapPage);
        }
    }

    //
    // Map the lowest level page table to the swap space and return the pointer
    // to the PTE.
    //

    *SwapPte = Physical | X86_PTE_PRESENT | X86_PTE_WRITABLE;
    Index = ((UINTN)VirtualAddress >> EntryShift) & X64_PT_MASK;
    Pte = (PPTE)SwapPage + Index;
    return Pte;
}

KSTATUS
MmpEnsurePageTables (
    PADDRESS_SPACE_X64 AddressSpace,
    PVOID VirtualAddress
    )

/*++

Routine Description:

    This routine creates any missing page tables for the given virtual address
    in the current process.

Arguments:

    AddressSpace - Supplies a pointer to the address space.

    VirtualAddress - Supplies the virtual address to create page tables for.

Return Value:

    Status code.

--*/

{

    PPTE Pte;
    KSTATUS Status;

    Pte = X64_PML4E(VirtualAddress);
    if ((*Pte & X86_PTE_PRESENT) == 0) {
        Status = MmpCreatePageTable(AddressSpace,
                                    Pte,
                                    INVALID_PHYSICAL_ADDRESS);

        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    Pte = X64_PDPE(VirtualAddress);
    if ((*Pte & X86_PTE_PRESENT) == 0) {
        Status = MmpCreatePageTable(AddressSpace,
                                    Pte,
                                    INVALID_PHYSICAL_ADDRESS);

        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    Pte = X64_PDE(VirtualAddress);
    if ((*Pte & X86_PTE_PRESENT) == 0) {
        Status = MmpCreatePageTable(AddressSpace,
                                    Pte,
                                    INVALID_PHYSICAL_ADDRESS);

        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    return STATUS_SUCCESS;
}

KSTATUS
MmpCreatePageTable (
    PADDRESS_SPACE_X64 AddressSpace,
    PPTE Pte,
    PHYSICAL_ADDRESS Physical
    )

/*++

Routine Description:

    This routine creates a page table and installs it at the given PTE. This
    routine must called at low level, unless a page table physical address is
    already supplied, in which case it must be called at or below dispatch
    level.

Arguments:

    AddressSpace - Supplies a pointer to the address space.

    Pte - Supplies a pointer to the PTE where the page table should be
        installed.

    VirtualAddress - Supplies the final virtual address corresponding to the
        mapping. Used for page table accounting.

    Physical - Supplies an optional physical address to use for the new page
        table.

Return Value:

    Status code.

--*/

{

    PHYSICAL_ADDRESS AllocatedPhysical;
    ULONG Index;
    RUNLEVEL OldRunLevel;
    PPROCESSOR_BLOCK Processor;
    PVOID SwapPage;
    PTE SwapPte;
    PPTE SwapPtePointer;

    //
    // See if someone beat this routine to the punch, or perhaps there's an
    // inactive page table here.
    //

    if (X86_PTE_ENTRY(*Pte) != 0) {

        ASSERT(Physical == INVALID_PHYSICAL_ADDRESS);

        if ((*Pte & X86_PTE_PRESENT) != 0) {
            return STATUS_SUCCESS;
        }

        Physical = X86_PTE_ENTRY(*Pte);
    }

    AllocatedPhysical = INVALID_PHYSICAL_ADDRESS;
    if (Physical == INVALID_PHYSICAL_ADDRESS) {
        Physical = MmpAllocatePhysicalPages(1, 0);
        if (Physical == INVALID_PHYSICAL_ADDRESS) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        AllocatedPhysical = Physical;
    }

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    Processor = KeGetCurrentProcessorBlock();
    SwapPage = Processor->SwapPage;
    SwapPtePointer = X64_PTE(SwapPage);
    SwapPte = *SwapPtePointer;
    *SwapPtePointer = Physical | X86_PTE_PRESENT | X86_PTE_WRITABLE;
    if (SwapPte != 0) {
        ArInvalidateTlbEntry(SwapPage);
    }

    RtlZeroMemory(SwapPage, PAGE_SIZE);

    //
    // Put the original swap page back now in case the PTE pointer is in the
    // swap page itself.
    //

    *SwapPtePointer = SwapPte;
    ArInvalidateTlbEntry(SwapPage);
    KeAcquireSpinLock(&MmPageTableLock);

    //
    // Sync the kernel top level table if this is one of those PTEs.
    //

    if ((Pte >= (X64_PML4T + X64_PML4_INDEX(KERNEL_VA_START))) &&
        (Pte < (X64_PML4T + X64_PTE_COUNT))) {

        Index = ((UINTN)Pte - (UINTN)X64_PML4T) / sizeof(PTE);
        *Pte = MmKernelPml4[Index];
    }

    //
    // Double check to make sure there's no page table installed, and then
    // install it.
    //

    if (X86_PTE_ENTRY(*Pte) != 0) {
        if ((*Pte & X86_PTE_PRESENT) == 0) {
            *Pte |= X86_PTE_PRESENT | X86_PTE_WRITABLE;

            ASSERT(AddressSpace->ActivePageTables <
                   AddressSpace->AllocatedPageTables);

            AddressSpace->ActivePageTables += 1;
        }

    } else {
        *Pte = Physical | X86_PTE_PRESENT | X86_PTE_WRITABLE;
        AddressSpace->AllocatedPageTables += 1;
        AddressSpace->ActivePageTables += 1;
    }

    KeReleaseSpinLock(&MmPageTableLock);
    KeLowerRunLevel(OldRunLevel);
    if ((AllocatedPhysical != INVALID_PHYSICAL_ADDRESS) &&
        (X86_PTE_ENTRY(*Pte) != AllocatedPhysical)) {

        MmFreePhysicalPage(Physical);
    }

    return STATUS_SUCCESS;
}

