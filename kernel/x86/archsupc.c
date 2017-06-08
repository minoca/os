/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    archsupc.c

Abstract:

    This module implements x86 processor architecture features.

Author:

    Evan Green 18-Jul-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/x86.h>

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
// ------------------------------------------------------------------ Functions
//

VOID
ArSetUpUserSharedDataFeatures (
    VOID
    )

/*++

Routine Description:

    This routine initialize the user shared data processor specific features.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PUSER_SHARED_DATA Data;
    ULONG Eax;
    ULONG Ebx;
    ULONG Ecx;
    ULONG Edx;
    PPROCESSOR_BLOCK ProcessorBlock;
    PTSS Tss;

    Data = MmGetUserSharedData();
    Eax = X86_CPUID_IDENTIFICATION;
    ArCpuid(&Eax, &Ebx, &Ecx, &Edx);
    if (Eax < X86_CPUID_BASIC_INFORMATION) {
        return;
    }

    Eax = X86_CPUID_BASIC_INFORMATION;
    ArCpuid(&Eax, &Ebx, &Ecx, &Edx);

    //
    // Check for CMOV instructions, which is an indication of Pentium Pro
    // (i686) vs Pentium (i586). One might imagine that a modern OS such as
    // this one might not need to trifle with processor architectures before
    // 1995. One might be wrong. The Intel Quark for instance uses the Pentium
    // instruction set.
    //

    if ((Edx & X86_CPUID_BASIC_EDX_CMOV) != 0) {
        Data->ProcessorFeatures |= X86_FEATURE_I686;
    }

    //
    // In 32-bit mode, shoot for sysenter, and then syscall. (Note that in
    // long mode, syscall is just assumed to be present architecturally).
    //

    if ((Edx & X86_CPUID_BASIC_EDX_SYSENTER) != 0) {

        //
        // Set up SYSENTER support. Sysenter shares the double fault stack,
        // which happens to be right below the main TSS.
        // Normally sysenter doesn't need a stack, as the first thing the
        // handler does with interrupts disabled is to load Tss->Esp0. The one
        // exception is if usermode sets the trap flag when calling sysenter,
        // in which case a single step exception occurs in kernel mode with
        // whatever stack is set in the MSR. Sharing with the double fault
        // stack means that if a double fault occurs in the single step
        // handler, the developer trying to debug what's going on will be
        // presented with a confused stack (though EIP and the registers will
        // still be correct). Double faults are fatal anyway, so the corruption
        // of its stack isn't really any more fatal.
        //

        ProcessorBlock = KeGetCurrentProcessorBlock();
        Tss = ProcessorBlock->Tss;
        Data->ProcessorFeatures |= X86_FEATURE_SYSENTER;
        ArWriteMsr(X86_MSR_SYSENTER_CS, KERNEL_CS);
        ArWriteMsr(X86_MSR_SYSENTER_EIP, (UINTN)ArSysenterHandlerAsm);
        ArWriteMsr(X86_MSR_SYSENTER_ESP, (UINTN)Tss);

    } else {

        ASSERT((Data->ProcessorFeatures & X86_FEATURE_SYSENTER) == 0);

        Eax = X86_CPUID_EXTENDED_IDENTIFICATION;
        ArCpuid(&Eax, &Ebx, &Ecx, &Edx);
        if (Eax < X86_CPUID_EXTENDED_INFORMATION) {
            return;
        }

        Eax = X86_CPUID_EXTENDED_INFORMATION;
        ArCpuid(&Eax, &Ebx, &Ecx, &Edx);
        if ((Edx & X86_CPUID_EXTENDED_INFORMATION_EDX_SYSCALL) != 0) {

            //
            // Set up SYSCALL support.
            //

            RtlDebugPrint("Syscall but no sysenter!\n");
            Data->ProcessorFeatures |= X86_FEATURE_SYSCALL;
        }
    }

    //
    // Remember if the processor supports the fxsave instruction.
    //

    if ((Edx & X86_CPUID_BASIC_EDX_FX_SAVE_RESTORE) != 0) {
        Data->ProcessorFeatures |= X86_FEATURE_FXSAVE;
    }

    return;
}

PFPU_CONTEXT
ArAllocateFpuContext (
    ULONG AllocationTag
    )

/*++

Routine Description:

    This routine allocates a buffer that can be used for FPU context.

Arguments:

    AllocationTag - Supplies the pool allocation tag to use for the allocation.

Return Value:

    Returns a pointer to the newly allocated FPU context on success.

    NULL on allocation failure.

--*/

{

    UINTN AllocationSize;
    PFPU_CONTEXT Context;

    AllocationSize = sizeof(FPU_CONTEXT) + FPU_CONTEXT_ALIGNMENT;
    Context = MmAllocateNonPagedPool(AllocationSize, AllocationTag);
    if (Context == NULL) {
        return NULL;
    }

    //
    // Zero out the buffer to avoid leaking kernel pool to user mode.
    //

    RtlZeroMemory(Context, AllocationSize);
    return Context;
}

VOID
ArDestroyFpuContext (
    PFPU_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys a previously allocated FPU context buffer.

Arguments:

    Context - Supplies a pointer to the context to destroy.

Return Value:

    None.

--*/

{

    MmFreeNonPagedPool(Context);
    return;
}

VOID
ArSetThreadPointer (
    PVOID Thread,
    PVOID NewThreadPointer
    )

/*++

Routine Description:

    This routine sets the new thread pointer value.

Arguments:

    Thread - Supplies a pointer to the thread to set the thread pointer for.

    NewThreadPointer - Supplies the new thread pointer value to set.

Return Value:

    None.

--*/

{

    PGDT_ENTRY Gdt;
    PGDT_ENTRY GdtEntry;
    RUNLEVEL OldRunLevel;
    PPROCESSOR_BLOCK Processor;
    PKTHREAD TypedThread;

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    TypedThread = Thread;
    GdtEntry = (PGDT_ENTRY)&(TypedThread->ThreadPointer);

    ASSERT(sizeof(GDT_ENTRY) <= sizeof(TypedThread->ThreadPointer));

    ArpCreateSegmentDescriptor(GdtEntry,
                               NewThreadPointer,
                               MAX_GDT_LIMIT,
                               GDT_GRANULARITY_KILOBYTE | GDT_GRANULARITY_32BIT,
                               GATE_ACCESS_USER | GDT_TYPE_DATA_WRITE);

    if (Thread == KeGetCurrentThread()) {
        Processor = KeGetCurrentProcessorBlock();
        Gdt = Processor->Gdt;
        RtlCopyMemory(&(Gdt[GDT_THREAD / sizeof(GDT_ENTRY)]),
                      GdtEntry,
                      sizeof(GDT_ENTRY));

        ArReloadThreadSegment();
    }

    KeLowerRunLevel(OldRunLevel);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

