/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    bcm2836.h

Abstract:

    This header contains definitions for Broadcom 2836 System on Chip.

Author:

    Chris Stevens 19-Mar-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the base address for global system configuration space.
//

#define BCM2836_BASE 0x3F000000

//
// Define the base address for processor local configuration space.
//

#define BCM2836_LOCAL_BASE 0x40000000

//
// Define the timer interrupt control registers for each CPU.
//

#define BCM2836_CPU_0_TIMER_INTERRUPT_CONTROL (BCM2836_LOCAL_BASE + 0x40)
#define BCM2836_CPU_1_TIMER_INTERRUPT_CONTROL (BCM2836_LOCAL_BASE + 0x44)
#define BCM2836_CPU_2_TIMER_INTERRUPT_CONTROL (BCM2836_LOCAL_BASE + 0x48)
#define BCM2836_CPU_3_TIMER_INTERRUPT_CONTROL (BCM2836_LOCAL_BASE + 0x4C)

//
// Define the mailbox interrupt control registers for each CPU.
//

#define BCM2836_CPU_0_MAILBOX_INTERRUPT_CONTROL (BCM2836_LOCAL_BASE + 0x50)
#define BCM2836_CPU_1_MAILBOX_INTERRUPT_CONTROL (BCM2836_LOCAL_BASE + 0x54)
#define BCM2836_CPU_2_MAILBOX_INTERRUPT_CONTROL (BCM2836_LOCAL_BASE + 0x58)
#define BCM2836_CPU_3_MAILBOX_INTERRUPT_CONTROL (BCM2836_LOCAL_BASE + 0x5C)

//
// Define the IRQ pending registers for the four CPUs.
//

#define BCM2836_CPU_0_IRQ_PENDING (BCM2836_LOCAL_BASE + 0x60)
#define BCM2836_CPU_1_IRQ_PENDING (BCM2836_LOCAL_BASE + 0x64)
#define BCM2836_CPU_2_IRQ_PENDING (BCM2836_LOCAL_BASE + 0x68)
#define BCM2836_CPU_3_IRQ_PENDING (BCM2836_LOCAL_BASE + 0x6C)

//
// Define the FIQ pending registers for the four CPUs.
//

#define BCM2836_CPU_0_FIQ_PENDING (BCM2836_LOCAL_BASE + 0x70)
#define BCM2836_CPU_1_FIQ_PENDING (BCM2836_LOCAL_BASE + 0x74)
#define BCM2836_CPU_2_FIQ_PENDING (BCM2836_LOCAL_BASE + 0x78)
#define BCM2836_CPU_3_FIQ_PENDING (BCM2836_LOCAL_BASE + 0x7C)

//
// Define the mailbox set registers for the four CPUs.
//

#define BCM2836_CPU_0_MAILBOX_0_SET (BCM2836_LOCAL_BASE + 0x80)
#define BCM2836_CPU_0_MAILBOX_1_SET (BCM2836_LOCAL_BASE + 0x84)
#define BCM2836_CPU_0_MAILBOX_2_SET (BCM2836_LOCAL_BASE + 0x88)
#define BCM2836_CPU_0_MAILBOX_3_SET (BCM2836_LOCAL_BASE + 0x8C)
#define BCM2836_CPU_1_MAILBOX_0_SET (BCM2836_LOCAL_BASE + 0x90)
#define BCM2836_CPU_1_MAILBOX_1_SET (BCM2836_LOCAL_BASE + 0x94)
#define BCM2836_CPU_1_MAILBOX_2_SET (BCM2836_LOCAL_BASE + 0x98)
#define BCM2836_CPU_1_MAILBOX_3_SET (BCM2836_LOCAL_BASE + 0x9C)
#define BCM2836_CPU_2_MAILBOX_0_SET (BCM2836_LOCAL_BASE + 0xA0)
#define BCM2836_CPU_2_MAILBOX_1_SET (BCM2836_LOCAL_BASE + 0xA4)
#define BCM2836_CPU_2_MAILBOX_2_SET (BCM2836_LOCAL_BASE + 0xA8)
#define BCM2836_CPU_2_MAILBOX_3_SET (BCM2836_LOCAL_BASE + 0xAC)
#define BCM2836_CPU_3_MAILBOX_0_SET (BCM2836_LOCAL_BASE + 0xB0)
#define BCM2836_CPU_3_MAILBOX_1_SET (BCM2836_LOCAL_BASE + 0xB4)
#define BCM2836_CPU_3_MAILBOX_2_SET (BCM2836_LOCAL_BASE + 0xB8)
#define BCM2836_CPU_3_MAILBOX_3_SET (BCM2836_LOCAL_BASE + 0xBC)

//
// Define the mailbox clear registers for the four CPUs.
//

#define BCM2836_CPU_0_MAILBOX_0_CLEAR (BCM2836_LOCAL_BASE + 0xC0)
#define BCM2836_CPU_0_MAILBOX_1_CLEAR (BCM2836_LOCAL_BASE + 0xC4)
#define BCM2836_CPU_0_MAILBOX_2_CLEAR (BCM2836_LOCAL_BASE + 0xC8)
#define BCM2836_CPU_0_MAILBOX_3_CLEAR (BCM2836_LOCAL_BASE + 0xCC)
#define BCM2836_CPU_1_MAILBOX_0_CLEAR (BCM2836_LOCAL_BASE + 0xD0)
#define BCM2836_CPU_1_MAILBOX_1_CLEAR (BCM2836_LOCAL_BASE + 0xD4)
#define BCM2836_CPU_1_MAILBOX_2_CLEAR (BCM2836_LOCAL_BASE + 0xD8)
#define BCM2836_CPU_1_MAILBOX_3_CLEAR (BCM2836_LOCAL_BASE + 0xDC)
#define BCM2836_CPU_2_MAILBOX_0_CLEAR (BCM2836_LOCAL_BASE + 0xE0)
#define BCM2836_CPU_2_MAILBOX_1_CLEAR (BCM2836_LOCAL_BASE + 0xE4)
#define BCM2836_CPU_2_MAILBOX_2_CLEAR (BCM2836_LOCAL_BASE + 0xE8)
#define BCM2836_CPU_2_MAILBOX_3_CLEAR (BCM2836_LOCAL_BASE + 0xEC)
#define BCM2836_CPU_3_MAILBOX_0_CLEAR (BCM2836_LOCAL_BASE + 0xF0)
#define BCM2836_CPU_3_MAILBOX_1_CLEAR (BCM2836_LOCAL_BASE + 0xF4)
#define BCM2836_CPU_3_MAILBOX_2_CLEAR (BCM2836_LOCAL_BASE + 0xF8)
#define BCM2836_CPU_3_MAILBOX_3_CLEAR (BCM2836_LOCAL_BASE + 0xFC)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

