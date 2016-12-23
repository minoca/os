/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    smp.inc

Abstract:

    This module contains definitions for SMP assembly on the RK3288 Veyron SoC.

Author:

    Chris Stevens 9-Jun-2016

Environment:

    Firmware

--*/

##
## --------------------------------------------------------------- Definitions
##

#define RK32_TIMER_FREQUENCY 0x016E3600

##
## -------------------------------------------------------------------- Macros
##

##
## This macro can be run immediately after an exception or interrupt. It
## switches back to SVC mode and creates a trap frame. This uses several
## instructions from ARMv6T2, including srs, cpsid #mode, and clrex.
##

.macro RK32_SMP_INIT

    ##
    ## Set the SMP bit in the auxiliary control register on this core.
    ##

    mrc     p15, 0, %r0, %cr1, %cr0, 1
    orr     %r0, %r0, #CORTEX_A17_AUX_SMP_ENABLE
    mcr     p15, 0, %r0, %cr1, %cr0, 1
    DSB
    ISB

    ##
    ## Set the ARM Generic Timer frequency. This must be done in secure mode
    ## on each core.
    ##

    ldr     %r0, =RK32_TIMER_FREQUENCY
    mcr     p15, 0, %r0, %c14, %c0, 0           @ Set the CNTFRQ

    ##
    ## Set the Monitor Mode Interrupt Vector Base Address register in
    ## preparation for the SMC instruction below.
    ##

    ldr     %r0, =EfipRk32MonitorInterruptTable
    mcr     p15, 0, %r0, %c12, %c0, 1

    ##
    ## The SMC exception must be taken in Thumb mode.
    ##

    mrc     p15, 0, %r0, %cr1, %cr0, 0          @ Get the SCTLR.
    orr     %r0, %r0, #MMU_THUMB_EXCEPTIONS
    mcr     p15, 0, %r0, %cr1, %cr0, 0          @ Set the SCTLR.

    ##
    ## Make the jump to Monitor Mode in order to zero the virtual count offset
    ## of the ARM Generic Timer.
    ##

    smc     #0

.endm

