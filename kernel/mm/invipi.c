/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    invipi.c

Abstract:

    This module implements the TLB invalidation IPI.

Author:

    Evan Green 30-Sep-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "mmp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// Store the global containing the address to invalidate and the number of
// processors that have yet to respond to the IPI.
//

KSPIN_LOCK MmInvalidateIpiLock;
volatile PADDRESS_SPACE MmInvalidateIpiAddressSpace = NULL;
volatile PVOID MmInvalidateIpiAddress = NULL;
volatile ULONG MmInvalidateIpiPageCount = 0;
volatile ULONG MmInvalidateIpiProcessorsRemaining = 0;

//
// ------------------------------------------------------------------ Functions
//

INTERRUPT_STATUS
MmTlbInvalidateIpiServiceRoutine (
    PVOID Context
    )

/*++

Routine Description:

    This routine handles TLB invalidation IPIs.

Arguments:

    Context - Supplies an unused context pointer.

Return Value:

    Returns Claimed always.

--*/

{

    PVOID Address;
    ULONG Index;
    RUNLEVEL OldRunLevel;
    ULONG PageSize;
    PKPROCESS Process;

    OldRunLevel = KeRaiseRunLevel(RunLevelIpi);
    PageSize = MmPageSize();
    Process = PsGetCurrentProcess();
    if ((MmInvalidateIpiAddress >= KERNEL_VA_START) ||
        (Process->AddressSpace == MmInvalidateIpiAddressSpace)) {

        Address = MmInvalidateIpiAddress;
        for (Index = 0; Index < MmInvalidateIpiPageCount; Index += 1) {
            ArInvalidateTlbEntry(Address);
            Address = (PVOID)((UINTN)Address + PageSize);
        }
    }

    RtlAtomicAdd32(&MmInvalidateIpiProcessorsRemaining, -1);
    KeLowerRunLevel(OldRunLevel);
    return InterruptStatusClaimed;
}

VOID
MmpSendTlbInvalidateIpi (
    PADDRESS_SPACE AddressSpace,
    PVOID VirtualAddress,
    ULONG PageCount
    )

/*++

Routine Description:

    This routine invalidates the given TLB entry on all active processors.

Arguments:

    AddressSpace - Supplies a pointer to the address space to invalidate for.

    VirtualAddress - Supplies the virtual address to invalidate.

    PageCount - Supplies the number of pages to invalidate.

Return Value:

    None.

--*/

{

    RUNLEVEL OldRunLevel;
    ULONG PageIndex;
    ULONG PageSize;
    PROCESSOR_SET ProcessorSet;
    KSTATUS Status;

    //
    // If there is only one processor in the system, do the invalidate
    // directly.
    //

    if (KeGetActiveProcessorCount() == 1) {
        PageSize = MmPageSize();
        for (PageIndex = 0; PageIndex < PageCount; PageIndex += 1) {
            ArInvalidateTlbEntry(VirtualAddress);
            VirtualAddress = (PVOID)((UINTN)VirtualAddress + PageSize);
        }

        return;
    }

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeAcquireSpinLock(&MmInvalidateIpiLock);
    MmInvalidateIpiAddressSpace = AddressSpace;
    MmInvalidateIpiAddress = VirtualAddress;
    MmInvalidateIpiPageCount = PageCount;
    MmInvalidateIpiProcessorsRemaining = KeGetActiveProcessorCount();
    RtlMemoryBarrier();

    //
    // Send out the IPI.
    //

    ProcessorSet.Target = ProcessorTargetAll;
    Status = HlSendIpi(IpiTypeTlbFlush, &ProcessorSet);
    if (!KSUCCESS(Status)) {
        KeCrashSystem(CRASH_IPI_FAILURE, Status, 0, 0, 0);
    }

    //
    // Spin waiting for the IPI to complete on all processors before returning.
    //

    while (MmInvalidateIpiProcessorsRemaining != 0) {
        ArProcessorYield();
    }

    KeReleaseSpinLock(&MmInvalidateIpiLock);
    KeLowerRunLevel(OldRunLevel);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

