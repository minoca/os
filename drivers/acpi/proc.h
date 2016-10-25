/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    proc.h

Abstract:

    This header contains definitions for ACPI processor devices.

Author:

    Evan Green 28-Sep-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define ACPI_MAX_CSTATES 8

#define ACPI_CSTATE_HALT 0x00000001
#define ACPI_CSTATE_IO_HALT 0x00000002
#define ACPI_CSTATE_MWAIT 0x00000004
#define ACPI_CSTATE_BUS_MASTER_AVOIDANCE 0x00000008

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _ACPI_CSTATE_TYPE {
    AcpiC1 = 1,
    AcpiC2 = 2,
    AcpiC3 = 3
} ACPI_CSTATE_TYPE, *PACPI_CSTATE_TYPE;

/*++

Structure Description:

    This structure defines ACPI C-state information.

Members:

    Register - Stores the register location needed to enter this C-state.

    Type - Stores the C-state type semantics. C-states higher than threee use
        type three.

    Latency - Stores the worst case latency to enter and exit this C-state, in
        microseconds.

    Power - Stores the average power consumption of the processor when in this
        C-state, in milliwatts.

    Flags - Stores a bitfield of flags about this C-state. See ACPI_CSTATE_*
        definitions.

--*/

typedef struct _ACPI_CSTATE {
    GENERIC_ADDRESS Register;
    ACPI_CSTATE_TYPE Type;
    ULONG Latency;
    ULONG Power;
    ULONG Flags;
} ACPI_CSTATE, *PACPI_CSTATE;

/*++

Structure Description:

    This structure defines system-wide context for ACPI processor management.

Members:

    CStateInterface - Stores the C-State interface with the OS.

    Processors - Stores the array of processor context structures, indexed by
        OS processor index (not ACPI processor numbers).

    ProcessorCount - Stores the number of processors in the array.

    StartedProcessorCount - Stores the number of processor devices that have
        been successfully started.

--*/

typedef struct _ACPI_PROCESSOR_GLOBAL_CONTEXT {
    PM_IDLE_STATE_INTERFACE CStateInterface;
    PACPI_PROCESSOR_CONTEXT Processors;
    ULONG ProcessorCount;
    ULONG StartedProcessorCount;
} ACPI_PROCESSOR_GLOBAL_CONTEXT, *PACPI_PROCESSOR_GLOBAL_CONTEXT;

/*++

Structure Description:

    This structure stores information about an ACPI processor device.

Members:

    AcpiId - Stores the ACPI processor ID. This should match up with the MADT
        entries.

    OsId - Stores the OS logical processor index.

    BlockAddress - Stores the P_BLK control address for this processor.

    BlockSize - Stores the size of the P_BLK region in bytes.

    Flags - Stores a bitfield of flags. See ACPI_PROCESSOR_* definitions.

    OsCStates - Stores the OS enumeration information for each C-state.

    CStateCount - Stores the number of C-states enumerated.

    HighestNonC3 - Stores the index of the highest C state that is not C3 type.

--*/

struct _ACPI_PROCESSOR_CONTEXT {
    ULONG AcpiId;
    ULONG OsId;
    ULONG BlockAddress;
    ULONG BlockSize;
    ULONG Flags;
    ACPI_CSTATE AcpiCStates[ACPI_MAX_CSTATES];
    PM_IDLE_STATE OsCStates[ACPI_MAX_CSTATES];
    ULONG CStateCount;
    ULONG HighestNonC3;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
AcpipProcessorStart (
    PACPI_DEVICE_CONTEXT Device
    );

/*++

Routine Description:

    This routine starts an ACPI processor object.

Arguments:

    Device - Supplies a pointer to the ACPI information associated with
        the processor device.

Return Value:

    Status code.

--*/

KSTATUS
AcpipArchInitializeProcessorManagement (
    PACPI_OBJECT NamespaceObject
    );

/*++

Routine Description:

    This routine is called to perform architecture-specific initialization for
    ACPI-based processor power management.

Arguments:

    NamespaceObject - Supplies the namespace object of this processor.

Return Value:

    Status code.

--*/

VOID
AcpipEnterCState (
    PPM_IDLE_PROCESSOR_STATE Processor,
    ULONG State
    );

/*++

Routine Description:

    This routine prototype represents a function that is called to go into a
    given idle state on the current processor. This routine is called with
    interrupts disabled, and should return with interrupts disabled.

Arguments:

    Processor - Supplies a pointer to the information for the current processor.

    State - Supplies the new state index to change to.

Return Value:

    None. It is assumed when this function returns that the idle state was
    entered and then exited.

--*/
