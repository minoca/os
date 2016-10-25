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

##
## ---------------------------------------------------------------- Definitions
##

#include <minoca/kernel/arm.inc>

##
## ----------------------------------------------------------------------- Code
##

ASSEMBLY_FILE_HEADER

##
## .global allows this label to be visible to the linker.
##

.global EfiArmInterruptTable

##
## This address must be aligned to a 32 byte address so that it can be set in
## the VBAR register.
##

.balign 32

EfiArmInterruptTable:
    ldr     %pc, EfiArmResetVector
    ldr     %pc, EfiArmUndefinedInstructionVector
    ldr     %pc, EfiArmSoftwareInterruptVector
    ldr     %pc, EfiArmPrefetchAbortVector
    ldr     %pc, EfiArmDataAbortVector
.word 0
    ldr     %pc, EfiArmIrqInterruptVector
    ldr     %pc, EfiArmFiqInterruptVector

EfiArmUndefinedInstructionVector:
    .word 0

EfiArmSoftwareInterruptVector:
    .word 0

EfiArmPrefetchAbortVector:
    .word 0

EfiArmDataAbortVector:
    .word 0

EfiArmIrqInterruptVector:
    .word 0

EfiArmFiqInterruptVector:
    .word 0

EfiArmResetVector:
    .word 0

