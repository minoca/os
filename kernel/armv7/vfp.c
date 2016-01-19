/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    vfp.c

Abstract:

    This module implements kernel support routines for the Vector Floating
    Point Unit and Advanced SIMD hardware on ARM.

Author:

    Evan Green 10-Nov-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include <minoca/arm.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define opcode masks for VFP/NEON instructions.
//

#define ARM_VFP_INSTRUCTION_MASK1 0xFF100000
#define ARM_VFP_INSTRUCTION_VALUE1 0xF4000000
#define ARM_VFP_INSTRUCTION_MASK2 0xFE000000
#define ARM_VFP_INSTRUCTION_VALUE2 0xF2000000
#define THUMB_VFP_INSTRUCTION_MASK1 0xEF000000
#define THUMB_VFP_INSTRUCTION_VALUE1 0xEF000000
#define THUMB_VFP_INSTRUCTION_MASK2 0xFF100000
#define THUMB_VFP_INSTRUCTION_VALUE2 0xFA000000

#define ARM_COPROCESSOR_INSTRUCTION_MASK 0x0C000000
#define ARM_COPROCESSOR_INSTRUCTION_VALUE 0x0C000000
#define ARM_COPROCESSOR_INSTRUCTION_COPROCESSOR_MASK 0x00000F00
#define ARM_COPROCESSOR_INSTRUCTION_COPROCESSOR_SHIFT 8

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
ArpHandleVfpException (
    PTRAP_FRAME TrapFrame
    );

BOOL
ArpDummyVfpExceptionHandler (
    PTRAP_FRAME TrapFrame
    );

VOID
ArpEnableFpu (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

PARM_HANDLE_EXCEPTION ArHandleVfpException;

//
// Remember if there are 32 or 16 VFP registers.
//

BOOL ArVfpRegisters32;

//
// ------------------------------------------------------------------ Functions
//

VOID
ArInitializeVfpSupport (
    VOID
    )

/*++

Routine Description:

    This routine initializes processor support for the VFP unit, and sets the
    related feature bits in the user shared data.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG CoprocessorAccess;
    ULONG Extensions;
    ULONG FpsId;
    PARM_HANDLE_EXCEPTION OldHandler;
    ULONG Subarchitecture;
    PUSER_SHARED_DATA UserSharedData;

    UserSharedData = MmGetUserSharedData();

    //
    // Enable access to coprocessors 10 and 11.
    //

    CoprocessorAccess = ArGetCoprocessorAccessRegister();
    CoprocessorAccess &= ~(ARM_COPROCESSOR_ACCESS_MASK(10) |
                           ARM_COPROCESSOR_ACCESS_MASK(11));

    CoprocessorAccess |=
            ARM_COPROCESSOR_ACCESS(10, ARM_COPROCESSOR_ACCESS_FULL) |
            ARM_COPROCESSOR_ACCESS(11, ARM_COPROCESSOR_ACCESS_FULL);

    ArSetCoprocessorAccessRegister(CoprocessorAccess);

    //
    // Get the floating point ID register. This register might not exist, so a
    // dummy handler is set up to just return 0 in that case. This kind of
    // thing is obviously only safe during early kernel init, as while the
    // global is set any real FPU exceptions would be completely mishandled.
    //

    OldHandler = ArHandleVfpException;
    ArHandleVfpException = ArpDummyVfpExceptionHandler;
    FpsId = ArGetFloatingPointIdRegister();
    ArHandleVfpException = OldHandler;
    if (FpsId == 0) {
        return;
    }

    if (((FpsId & ARM_FPSID_IMPLEMENTER_MASK) >> ARM_FPSID_IMPLEMENTER_SHIFT) !=
        ARM_FPSID_IMPLEMENTER_ARM) {

        return;
    }

    Subarchitecture = (FpsId & ARM_FPSID_SUBARCHITECTURE_MASK) >>
                      ARM_FPSID_SUBARCHITECTURE_SHIFT;

    if (Subarchitecture < ARM_FPSID_SUBARCHITECTURE_VFPV2) {
        return;
    }

    UserSharedData->ProcessorFeatures |= ARM_FEATURE_VFP2;
    ArHandleVfpException = ArpHandleVfpException;
    if (Subarchitecture >= ARM_FPSID_SUBARCHITECTURE_VFPV3_COMMON_V2) {
        UserSharedData->ProcessorFeatures |= ARM_FEATURE_VFP3;
        Extensions = ArGetMvfr0Register();
        if ((Extensions & ARM_MVFR0_SIMD_REGISTERS_MASK) ==
            ARM_MVFR0_SIMD_REGISTERS_32) {

            UserSharedData->ProcessorFeatures |= ARM_FEATURE_NEON32;
            ArVfpRegisters32 = TRUE;
        }
    }

    return;
}

VOID
ArSaveFpuState (
    PFPU_CONTEXT Buffer
    )

/*++

Routine Description:

    This routine saves the current FPU context into the given buffer.

Arguments:

    Buffer - Supplies a pointer to the buffer where the information will be
        saved to.

Return Value:

    None.

--*/

{

    ArSaveVfp(Buffer, ArVfpRegisters32);
    return;
}

BOOL
ArCheckForVfpException (
    PTRAP_FRAME TrapFrame,
    ULONG Instruction
    )

/*++

Routine Description:

    This routine checks for VFP or NEON undefined instruction faults, and
    potentially handles them if found.

Arguments:

    TrapFrame - Supplies a pointer to the state immediately before the
        exception.

    Instruction - Supplies the instruction that caused the abort.

Return Value:

    None.

--*/

{

    ULONG Coprocessor;
    BOOL IsVfp;

    IsVfp = FALSE;

    //
    // Check for coprocessor access to CP10 or 11.
    //

    if ((Instruction & ARM_COPROCESSOR_INSTRUCTION_MASK) ==
        ARM_COPROCESSOR_INSTRUCTION_VALUE) {

        Coprocessor = (Instruction &
                       ARM_COPROCESSOR_INSTRUCTION_COPROCESSOR_MASK) >>
                      ARM_COPROCESSOR_INSTRUCTION_COPROCESSOR_SHIFT;

        if ((Coprocessor == 10) || (Coprocessor == 11)) {
            IsVfp = TRUE;
        }

    } else if ((TrapFrame->Cpsr & PSR_FLAG_THUMB) != 0) {
        if (((Instruction & THUMB_VFP_INSTRUCTION_MASK1) ==
             THUMB_VFP_INSTRUCTION_VALUE1) ||
            ((Instruction & THUMB_VFP_INSTRUCTION_MASK2) ==
             THUMB_VFP_INSTRUCTION_VALUE2)) {

            IsVfp = TRUE;
        }

    } else {
        if (((Instruction & ARM_VFP_INSTRUCTION_MASK1) ==
             ARM_VFP_INSTRUCTION_VALUE1) ||
            ((Instruction & ARM_VFP_INSTRUCTION_MASK2) ==
             ARM_VFP_INSTRUCTION_VALUE2)) {

            IsVfp = TRUE;
        }
    }

    //
    // If it's not a VFP instruction or there is no handler, bail.
    //

    if ((IsVfp == FALSE) || (ArHandleVfpException == NULL)) {
        return FALSE;
    }

    return ArHandleVfpException(TrapFrame);
}

VOID
ArDisableFpu (
    VOID
    )

/*++

Routine Description:

    This routine disallows access to the FPU on the current processor, causing
    all future accesses to generate exceptions.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG Control;

    Control = ArGetVfpExceptionRegister();
    Control &= ~ARM_FPEXC_ENABLE;
    ArSetVfpExceptionRegister(Control);
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
    PUSER_SHARED_DATA UserSharedData;

    AllocationSize = sizeof(FPU_CONTEXT);
    Context = MmAllocateNonPagedPool(AllocationSize, AllocationTag);
    if (Context == NULL) {
        return NULL;
    }

    RtlZeroMemory(Context, sizeof(FPU_CONTEXT));

    //
    // Currently the software assist support needed for VFPv2 and older is not
    // implemented. The bounce code usually covers denormalized numbers, so
    // set the flush to zero bit to cover the gap. This is not completely
    // IEEE754 compliant, but is good enough to limp along on these older
    // cores.
    //

    UserSharedData = MmGetUserSharedData();
    if ((UserSharedData->ProcessorFeatures & ARM_FEATURE_VFP3) == 0) {
        Context->Fpscr |= ARM_FPSCR_FLUSH_TO_ZERO;
    }

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

//
// --------------------------------------------------------- Internal Functions
//

BOOL
ArpHandleVfpException (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine handles a floating point access exception.

Arguments:

    TrapFrame - Supplies a pointer to the exception trap frame.

Return Value:

    TRUE if the exception was handled.

    FALSE if the exception was not handled.

--*/

{

    RUNLEVEL OldRunLevel;
    PKTHREAD Thread;

    ASSERT(ArAreInterruptsEnabled() != FALSE);

    Thread = KeGetCurrentThread();

    //
    // Kernel mode should not be tripping into FPU code, as it would destroy
    // user FPU context without the proper care.
    //

    ASSERT(ArIsTrapFrameFromPrivilegedMode(TrapFrame) == FALSE);

    //
    // If the thread has never used the FPU before, allocate FPU context while
    // still at low level.
    //

    if (Thread->FpuContext == NULL) {

        ASSERT((Thread->Flags & THREAD_FLAG_USING_FPU) == 0);

        Thread->FpuContext =
                           ArAllocateFpuContext(PS_FPU_CONTEXT_ALLOCATION_TAG);

        if (Thread->FpuContext == NULL) {
            PsSignalThread(Thread, SIGNAL_BUS_ERROR, NULL);
            goto HandleVfpExceptionEnd;
        }
    }

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);

    //
    // If the thread already owns the FPU, just enable it. Otherwise, do a full
    // restore (which also serves as init for a new FPU user).
    //

    ArpEnableFpu();
    if ((Thread->Flags & THREAD_FLAG_FPU_OWNER) == 0) {
        ArRestoreVfp(Thread->FpuContext, ArVfpRegisters32);
    }

    Thread->Flags |= THREAD_FLAG_FPU_OWNER | THREAD_FLAG_USING_FPU;
    KeLowerRunLevel(OldRunLevel);

HandleVfpExceptionEnd:
    return TRUE;
}

BOOL
ArpDummyVfpExceptionHandler (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine contains a no-op VFP exception handler, which simply always
    sets R0 to zero. It is used only during early kernel VFP detection.

Arguments:

    TrapFrame - Supplies a pointer to the exception trap frame.

Return Value:

    TRUE if the exception was handled.

    FALSE if the exception was not handled.

--*/

{

    TrapFrame->R0 = 0;
    return TRUE;
}

VOID
ArpEnableFpu (
    VOID
    )

/*++

Routine Description:

    This routine enables access to the floating point unit hardware on the
    current processor.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG Control;

    Control = ArGetVfpExceptionRegister();

    ASSERT((Control & ARM_FPEXC_ENABLE) == 0);

    Control |= ARM_FPEXC_ENABLE;
    ArSetVfpExceptionRegister(Control);
    return;
}

