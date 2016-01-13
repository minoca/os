/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    archsupc.c

Abstract:

    This module implements ARMv6 processor architecture features.

Author:

    Chris Stevens 3-Feb-2014

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

    return;
}

VOID
ArpInitializeCaches (
    PULONG DataCacheLineSize,
    PULONG InstructionCacheLineSize
    )

/*++

Routine Description:

    This routine initializes the system's processor cache infrastructure.

Arguments:

    DataCacheLineSize - Supplies a pointer that receives the size of a data
        cache line.

    InstructionCacheLineSize - Supplies a pointer that receives the size of a
        data cache line.

Return Value:

    None.

--*/

{

    ULONG CacheTypeRegister;
    ULONG LengthField;

    //
    // The Cache Type Register stores an off-by-one shift of the number of
    // words in the smallest data and instruction cache lines. On ARM, a word
    // is fixed at 32-bits so multiply (1 << (x + 1)) by the size of a ULONG.
    //

    CacheTypeRegister = ArGetCacheTypeRegister();

    ASSERT((CacheTypeRegister & ARMV6_CACHE_TYPE_SEPARATE_MASK) != 0);

    LengthField = (CacheTypeRegister &
                   ARMV6_CACHE_TYPE_DATA_CACHE_LENGTH_MASK) >>
                  ARMV6_CACHE_TYPE_DATA_CACHE_LENGTH_SHIFT;

    *DataCacheLineSize = (1 << (LengthField + 1)) * sizeof(ULONG);
    LengthField = CacheTypeRegister &
                  ARMV6_CACHE_TYPE_INSTRUCTION_CACHE_LENGTH_MASK;

    *InstructionCacheLineSize = (1 << (LengthField + 1)) * sizeof(ULONG);
    return;
}

VOID
ArpInitializePerformanceMonitor (
    VOID
    )

/*++

Routine Description:

    This routine initializes the system's performance monitor.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG ControlRegister;

    //
    // Disable performance monitor interrupts, and access to the performance
    // monitors in user mode.
    //

    ControlRegister = ArGetPerformanceControlRegister();
    if (ControlRegister != 0) {
        ControlRegister &= ~ARMV6_PERF_MONITOR_INTERRUPT_MASK;
        ArSetPerformanceControlRegister(ControlRegister);
        ArSetPerformanceUserEnableRegister(0);
    }

    return;
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

    ASSERT(FALSE);

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

    PULONG LowPointer;
    RUNLEVEL OldRunLevel;
    PKTHREAD TypedThread;

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);

    //
    // Only set the low 32-bits, the upper 32-bits are used to hold the
    // read/write thread pointer.
    //

    TypedThread = Thread;
    LowPointer = (PULONG)&(TypedThread->ThreadPointer);
    *LowPointer = (ULONG)NewThreadPointer;
    if (Thread == KeGetCurrentThread()) {
        ArSetThreadPointerUserReadOnly(NewThreadPointer);
    }

    KeLowerRunLevel(OldRunLevel);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

