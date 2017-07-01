/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    inttable.S

Abstract:

    This module implements the interrupt jump vector table for ARM processors.

Author:

    Evan Green 14-Aug-2013

Environment:

    Boot

--*/

//
// ---------------------------------------------------------------- Definitions
//

#include <minoca/kernel/arm.inc>

//
// ----------------------------------------------------------------------- Code
//

ASSEMBLY_FILE_HEADER

//
// .global allows this label to be visible to the linker.
//

.global BoArmInterruptTable

//
// This address must be aligned to a 32 byte address so that it can be set in
// the VBAR register.
//

.balign 32

BoArmInterruptTable:
    ldr     %pc, BoArmResetVector
    ldr     %pc, BoArmUndefinedInstructionVector
    ldr     %pc, BoArmSoftwareInterruptVector
    ldr     %pc, BoArmPrefetchAbortVector
    ldr     %pc, BoArmDataAbortVector
.word 0
    ldr     %pc, BoArmIrqInterruptVector
    ldr     %pc, BoArmFiqInterruptVector

BoArmUndefinedInstructionVector:
    .word 0

BoArmSoftwareInterruptVector:
    .word 0

BoArmPrefetchAbortVector:
    .word 0

BoArmDataAbortVector:
    .word 0

BoArmIrqInterruptVector:
    .word 0

BoArmFiqInterruptVector:
    .word 0

BoArmResetVector:
    .word 0

