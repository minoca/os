/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    inttable.s

Abstract:

    This module implements the interrupt jump vector table for ARM processors.

Author:

    Evan Green 11-Aug-2012

Environment:

    Kernel

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

.global ArArmInterruptTable

//
// This address must be aligned to a 32 byte address so that it can be set in
// the VBAR register.
//

.balign 32

ArArmInterruptTable:
    ldr     %pc, ArArmResetVector
    ldr     %pc, ArArmUndefinedInstructionVector
    ldr     %pc, ArArmSoftwareInterruptVector
    ldr     %pc, ArArmPrefetchAbortVector
    ldr     %pc, ArArmDataAbortVector
.word 0
    ldr     %pc, ArArmIrqInterruptVector
    ldr     %pc, ArArmFiqInterruptVector

ArArmUndefinedInstructionVector:
    .word ArpUndefinedInstructionEntry

ArArmSoftwareInterruptVector:
    .word ArpSoftwareInterruptEntry

ArArmPrefetchAbortVector:
    .word ArpPrefetchAbortEntry

ArArmDataAbortVector:
    .word ArpDataAbortEntry

ArArmIrqInterruptVector:
    .word ArpIrqEntry

ArArmFiqInterruptVector:
    .word ArpFiqEntry

ArArmResetVector:
    .word ArpFiqEntry

