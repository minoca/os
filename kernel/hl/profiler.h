/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    profiler.h

Abstract:

    This header contains definitions for the hardware layer's profiler interrupt
    support.

Author:

    Chris Stevens 1-Jul-2013

--*/

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------- Function Prototypes
//

INTERRUPT_STATUS
HlpProfilerInterruptHandler (
    PVOID Context
    );

/*++

Routine Description:

    This routine is the main profiler ISR.

Arguments:

    Context - Supplies a context pointer. Currently unused.

Return Value:

    Claimed always.

--*/

KSTATUS
HlpTimerInitializeProfiler (
    VOID
    );

/*++

Routine Description:

    This routine initializes the system profiler source and start it ticking.

Arguments:

    None.

Return Value:

    Status code.

--*/

