/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    archsupc.c

Abstract:

    This module implements ARMv7 processor architecture features.

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
        cache line in bytes.

    InstructionCacheLineSize - Supplies a pointer that receives the size of a
        data cache line in bytes.

Return Value:

    None.

--*/

{

    ULONG CacheTypeRegister;
    ULONG Log2CacheLineSize;

    //
    // The Cache Type Register stores Log base 2 of the number of words in the
    // smallest data and instruction cache lines. On ARM, a word is fixed at
    // 32-bits so multiply 2^x by the size of a ULONG.
    //

    CacheTypeRegister = ArGetCacheTypeRegister();
    Log2CacheLineSize = (CacheTypeRegister &
                         ARMV7_CACHE_TYPE_DATA_CACHE_SIZE_MASK) >>
                        ARMV7_CACHE_TYPE_DATA_CACHE_SIZE_SHIFT;

    *DataCacheLineSize = (1 << Log2CacheLineSize) * sizeof(ULONG);
    Log2CacheLineSize = CacheTypeRegister &
                        ARMV7_CACHE_TYPE_INSTRUCTION_CACHE_SIZE_MASK;

    *InstructionCacheLineSize = (1 << Log2CacheLineSize) * sizeof(ULONG);
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

    //
    // Disable performance monitor interrupts, and access to the performance
    // monitors in user mode.
    //

    if (ArGetPerformanceControlRegister() != 0) {
        ArClearPerformanceInterruptRegister(PERF_MONITOR_COUNTER_MASK);
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

