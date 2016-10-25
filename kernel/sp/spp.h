/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    spp.h

Abstract:

    This header contains privates definitions for the System Profiler.

Author:

    Chris Stevens 1-Jul-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Stores a value indicating which types of profiling are enabled.
//

extern ULONG SpEnabledFlags;

//
// Stores a pointer to a queued lock protecting access to the profiling status
// variables.
//

extern PQUEUED_LOCK SpProfilingQueuedLock;

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
SppStartSystemProfiler (
    ULONG Flags
    );

/*++

Routine Description:

    This routine starts the system profiler. This routine must be called at low
    level. It assumes the profiler queued lock is held.

Arguments:

    Flags - Supplies a set of flags representing the types of profiling that
        should be started.

Return Value:

    Status code.

--*/

KSTATUS
SppStopSystemProfiler (
    ULONG Flags
    );

/*++

Routine Description:

    This routine stops the system profiler and destroys the profiling data
    structures. This routine must be called at low level. It assumes the
    profiler queued lock is held.

Arguments:

    Flags - Supplies a set of flags representing the types of profiling that
        should be stopped.

Return Value:

    Status code.

--*/

KSTATUS
SppArchGetKernelStackData (
    PTRAP_FRAME TrapFrame,
    PVOID *CallStack,
    PULONG CallStackSize
    );

/*++

Routine Description:

    This routine retrieves the kernel stack and its size in the given data
    fields.

Arguments:

    TrapFrame - Supplies a pointer to the trap frame.

    CallStack - Supplies a pointer that receives an array of return addresses
        in the call stack.

    CallStackSize - Supplies a pointer to the size of the given call stack
        array. On return, it contains the size of the produced call stack, in
        bytes.

Return Value:

    Status code.

--*/

