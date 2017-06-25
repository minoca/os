/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    archsup.S

Abstract:

    This module implements x86 processor architecture features not
    implementable in C. This file compiles in both x86 and x64 builds.

Author:

    Chris Stevens 26-Feb-2014

Environment:

    Kernel mode

--*/

##
## ------------------------------------------------------------------ Includes
##

#if __SIZEOF_LONG__ == 8

#include <minoca/kernel/x64.inc>

#else

#include <minoca/kernel/x86.inc>

#endif

##
## ---------------------------------------------------------------------- Code
##

##
## .text specifies that this code belongs in the executable section.
##

.text

##
## KERNEL_API
## ULONGLONG
## HlQueryProcessorCounter (
##     VOID
##     )
##

/*++

Routine Description:

    This routine reads the time stamp counter from the current processor. It
    is essential that callers of this function understand that this returns
    instruction cycles, which does not always translate directly into units
    of time. For example, some processors halt the timestamp counter during
    performance and CPU idle state transitions. In other cases, the timestamp
    counters of all processors are not in sync, so as execution of a thread
    bounces unpredictably from one core to another, different timelines may be
    observed. Additionally, one must understand that this intrinsic is not a
    serializing instruction to the hardware, so the processor may decide to
    execute any number of instructions after this one before actually snapping
    the timestamp counter. To all those who choose to continue to use this
    primitive to measure time, you have been warned.

Arguments:

    None.

Return Value:

    Returns the current instruction cycle count since the processor was started.

--*/

PROTECTED_FUNCTION(HlQueryProcessorCounter)
    rdtsc                       # Store the timestamp counter.
    ret                         # And return!

END_FUNCTION(HlQueryProcessorCounter)

##
## --------------------------------------------------------- Internal Functions
##
