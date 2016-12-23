/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    apic.h

Abstract:

    This header contains definitions for the Advanced Programmable Interrupt
    Controller.

Author:

    Evan Green 5-Aug-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// This macro reads from the local APIC. The parameter should be an
// APIC_REGISTER value.
//

#define READ_LOCAL_APIC(_Register) \
    HlReadRegister32((PUCHAR)HlLocalApic + ((_Register) << 4))

//
// This macro writes to the local APIC. Register should be an APIC_REGISTER,
// and Value should be a ULONG.
//

#define WRITE_LOCAL_APIC(_Register, _Value) \
    HlWriteRegister32((PUCHAR)HlLocalApic + ((_Register) << 4), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

#define APIC_PRIORITY_COUNT 16
#define APIC_SPURIOUS_VECTOR_MASK 0xFF
#define APIC_ENABLE 0x100
#define APIC_TIMER_ONE_SHOT 0x0
#define APIC_TIMER_PERIODIC 0x20000
#define APIC_TIMER_DIVIDE_BY_1 0xB
#define APIC_LVT_DISABLED 0x10000
#define APIC_LVT_ENABLED 0x0
#define APIC_ID_SHIFT 24
#define APIC_STARTUP_CODE_MASK 0x000FF000
#define APIC_STARTUP_CODE_SHIFT 12
#define APIC_DESTINATION_SHIFT 24
#define APIC_CLUSTER_SHIFT 4

//
// APIC shorthand notation.
//

#define APIC_SHORTHAND_NONE               0x00000000
#define APIC_SHORTHAND_SELF               0x00040000
#define APIC_SHORTHAND_ALL_INCLUDING_SELF 0x00080000
#define APIC_SHORTHAND_ALL_EXCLUDING_SELF 0x000C0000

//
// APIC delivery modes.
//

#define APIC_DELIVERY_MASK     0x00000700
#define APIC_DELIVER_FIXED     0x00000000
#define APIC_DELIVER_LOWEST    0x00000100
#define APIC_DELIVER_SMI       0x00000200
#define APIC_DELIVER_NMI       0x00000400
#define APIC_DELIVER_INIT      0x00000500
#define APIC_DELIVER_STARTUP   0x00000600
#define APIC_DELIVER_EXTINT    0x00000700
#define APIC_PHYSICAL_DELIVERY 0x00000000
#define APIC_LOGICAL_DELIVERY  0x00000800
#define APIC_DELIVERY_PENDING  0x00001000
#define APIC_LEVEL_ASSERT      0x00004000
#define APIC_LEVEL_DEASSERT    0x00000000
#define APIC_LEVEL_TRIGGERED   0x00008000
#define APIC_EDGE_TRIGGERED    0x00000000

//
// Logical destination and destination format register values.
//

#define APIC_LOGICAL_CLUSTERED 0x0FFFFFFF
#define APIC_LOGICAL_FLAT      0xFFFFFFFF
#define APIC_MAX_CLUSTER_SIZE  4
#define APIC_MAX_CLUSTERS      0xF

//
// IO APIC RTE bits.
//

#define APIC_ACTIVE_LOW        0x00002000
#define APIC_RTE_MASKED        0x00010000

typedef enum _LOCAL_APIC_REGISTER {
    ApicId                       = 0x2,
    ApicVersion                  = 0x3,
    ApicTaskPriority             = 0x8,
    ApicArbitrationPriority      = 0x9,
    ApicProcessorPriority        = 0xA,
    ApicEndOfInterrupt           = 0xB,
    ApicLogicalDestination       = 0xD,
    ApicDestinationFormat        = 0xE,
    ApicSpuriousVector           = 0xF,
    ApicInService                = 0x10,
    ApicTriggerMode              = 0x18,
    ApicInterruptRequest         = 0x20,
    ApicErrorStatus              = 0x28,
    ApicLvtCmci                  = 0x2F,
    ApicCommandLow               = 0x30,
    ApicCommandHigh              = 0x31,
    ApicTimerVector              = 0x32,
    ApicThermalSensorVector      = 0x33,
    ApicPerformanceMonitorVector = 0x34,
    ApicLInt0Vector              = 0x35,
    ApicLInt1Vector              = 0x36,
    ApicErrorVector              = 0x37,
    ApicTimerInitialCount        = 0x38,
    ApicTimerCurrentCount        = 0x39,
    ApicTimerDivideConfiguration = 0x3E
} LOCAL_APIC_REGISTER, *PLOCAL_APIC_REGISTER;

//
// Define the possible LVT entries on the APIC.
//

typedef enum _APIC_LVT_LINE {
    ApicLineTimer = 0,
    ApicLineThermal = 1,
    ApicLinePerformance = 2,
    ApicLineLInt0 = 3,
    ApicLineLInt1 = 4,
    ApicLineError = 5,
    ApicLineCmci = 6,
    ApicLineCount = 7
} APIC_LVT_LINE, *PAPIC_LVT_LINE;

//
// I/O APIC register offsets.
//

#define IO_APIC_SELECT_OFFSET 0x0
#define IO_APIC_DATA_OFFSET 0x10

#define IO_APIC_RTE_SIZE 2

//
// Define the default value used to mask an RTE.
//

#define IO_APIC_MASKED_RTE_VALUE (APIC_RTE_MASKED | 0xFF)

typedef enum _IO_APIC_REGISTER {
    IoApicRegisterIdentifier                 = 0x0,
    IoApicRegisterVersion                    = 0x1,
    IoApicRegisterArbitrationIdentifier      = 0x2,
    IoApicRegisterFirstRedirectionTableEntry = 0x10,
} IO_APIC_REGISTER, *PIO_APIC_REGISTER;

//
// Define I/O APIC version register bit definitions.
//

#define IO_APIC_VERSION_MAX_ENTRY_MASK 0x00FF0000
#define IO_APIC_VERSION_MAX_ENTRY_SHIFT 16

//
// --------------------------------------------------------------------- Macros
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Remember a pointer to the MADT.
//

extern PMADT HlApicMadt;

//
// Store a pointer to the local APIC. It is assumed that all local APICs are at
// the same physical address.
//

extern PVOID HlLocalApic;

//
// Store the identifier of the first I/O APIC.
//

extern ULONG HlFirstIoApicId;

//
// -------------------------------------------------------- Function Prototypes
//

