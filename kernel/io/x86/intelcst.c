/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    intelcst.c

Abstract:

    This module implements support for Intel processor C-States.

Author:

    Evan Green 25-Sep-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// This could be abstracted out into a driver. The only reason it's not is that
// there are so few x86 manufacturers, and there's a small problem of
// enumerating some device that would cause this driver to load. Anyway, try to
// avoid including kernel internal headers here or using non-exported
// functions, as the extraction of this code out into a driver may happen some
// day.
//

#include <minoca/kernel/kernel.h>

#if __SIZEOF_LONG__ == 8

#include <minoca/kernel/x64.h>

#else

#include <minoca/kernel/x86.h>

#endif

//
// ---------------------------------------------------------------- Definitions
//

#define PM_INTEL_CSTATE_ALLOCATION_TAG 0x436C7449

#define PM_INTEL_MAX_CSTATES 8

#define PM_INTEL_CSTATE_MASK 0xFF
#define PM_INTEL_CSTATE_SHIFT 4
#define PM_INTEL_CSTATE_SUBSTATE_MASK 0x0F

#define PM_INTEL_PENRYN_CSTATE_COUNT 4
#define PM_INTEL_NEHALEM_CSTATE_COUNT 4
#define PM_INTEL_SANDY_BRIDGE_CSTATE_COUNT 5
#define PM_INTEL_BAY_TRAIL_CSTATE_COUNT 5
#define PM_INTEL_IVY_BRIDGE_CSTATE_COUNT 5
#define PM_INTEL_IVY_TOWN_CSTATE_COUNT 4
#define PM_INTEL_HASWELL_CSTATE_COUNT 8
#define PM_INTEL_ATOM_CSTATE_COUNT 4
#define PM_INTEL_AVOTON_CSTATE_COUNT 2

//
// Define Intel C-state flags that go along with the states.
//

//
// This flag is only ever set on the first state, and it indicates that
// automatic C1E promotion should be disabled.
//

#define PM_INTEL_DISABLE_C1E_PROMOTION (1 << 8)

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a single CPU idle state that a processor can enter.

Members:

    Interface - Stores the interface itself.

    States - Stores the array of enumerated C-states.

    StateCount - Stores the count of enumerated C-states.

    MwaitSubstates - Stores the MWAIT substates for each C-state.

    Model - Stores the processor model.

--*/

typedef struct _PM_INTEL_CSTATE_CONTEXT {
    PM_IDLE_STATE_INTERFACE Interface;
    PM_IDLE_STATE States[PM_INTEL_MAX_CSTATES];
    ULONG StateCount;
    ULONG MwaitSubstates;
    ULONG Model;
} PM_INTEL_CSTATE_CONTEXT, *PPM_INTEL_CSTATE_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
PmpIntelInitializeCstates (
    PPM_IDLE_STATE_INTERFACE Interface,
    PPM_IDLE_PROCESSOR_STATE Processor
    );

VOID
PmpIntelEnterCstate (
    PPM_IDLE_PROCESSOR_STATE Processor,
    ULONG State
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the idle states for the various Intel processor generations. The
// times are initialized in microseconds, and need to be converted to time
// counter ticks before sending to the kernel.
//

PM_IDLE_STATE PmIntelPenrynCstates[PM_INTEL_PENRYN_CSTATE_COUNT] = {
    {
        "C1",
        0,
        (PVOID)0x00,
        1,
        4
    },
    {
        "C2",
        0,
        (PVOID)0x10,
        20,
        80
    },
    {
        "C4",
        0,
        (PVOID)0x20,
        100,
        400
    },
    {
        "C6",
        0,
        (PVOID)0x30,
        150,
        550
    },
};

PM_IDLE_STATE PmIntelNehalemCstates[PM_INTEL_NEHALEM_CSTATE_COUNT] = {
    {
        "C1",
        0,
        (PVOID)(0x00 | PM_INTEL_DISABLE_C1E_PROMOTION),
        3,
        6
    },
    {
        "C1E",
        0,
        (PVOID)0x01,
        10,
        20
    },
    {
        "C3",
        0,
        (PVOID)0x10,
        20,
        80
    },
    {
        "C6",
        0,
        (PVOID)0x20,
        200,
        800
    },
};

PM_IDLE_STATE PmIntelSandyBridgeCstates[PM_INTEL_SANDY_BRIDGE_CSTATE_COUNT] = {
    {
        "C1",
        0,
        (PVOID)(0x00 | PM_INTEL_DISABLE_C1E_PROMOTION),
        2,
        2
    },
    {
        "C1E",
        0,
        (PVOID)0x01,
        10,
        20
    },
    {
        "C3",
        0,
        (PVOID)0x10,
        80,
        200
    },
    {
        "C6",
        0,
        (PVOID)0x20,
        100,
        300
    },
    {
        "C7",
        0,
        (PVOID)0x30,
        110,
        350
    },
};

PM_IDLE_STATE PmIntelBayTrailCstates[PM_INTEL_BAY_TRAIL_CSTATE_COUNT] = {
    {
        "C1",
        0,
        (PVOID)(0x00 | PM_INTEL_DISABLE_C1E_PROMOTION),
        1,
        4
    },
    {
        "C6N",
        0,
        (PVOID)0x58,
        300,
        300
    },
    {
        "C6S",
        0,
        (PVOID)0x52,
        500,
        550
    },
    {
        "C7",
        0,
        (PVOID)0x60,
        1200,
        4000
    },
    {
        "C7S",
        0,
        (PVOID)0x64,
        10000,
        20000
    },
};

PM_IDLE_STATE PmIntelIvyBridgeCstates[PM_INTEL_IVY_BRIDGE_CSTATE_COUNT] = {
    {
        "C1",
        0,
        (PVOID)(0x00 | PM_INTEL_DISABLE_C1E_PROMOTION),
        1,
        1
    },
    {
        "C1E",
        0,
        (PVOID)0x01,
        10,
        20
    },
    {
        "C3",
        0,
        (PVOID)0x10,
        60,
        150
    },
    {
        "C6",
        0,
        (PVOID)0x20,
        80,
        300
    },
    {
        "C7",
        0,
        (PVOID)0x30,
        90,
        350
    },
};

PM_IDLE_STATE PmIntelIvyTownCstates[PM_INTEL_IVY_TOWN_CSTATE_COUNT] = {
    {
        "C1",
        0,
        (PVOID)(0x00 | PM_INTEL_DISABLE_C1E_PROMOTION),
        1,
        1
    },
    {
        "C1E",
        0,
        (PVOID)0x01,
        10,
        120
    },
    {
        "C3",
        0,
        (PVOID)0x10,
        60,
        150
    },
    {
        "C6",
        0,
        (PVOID)0x20,
        80,
        300
    },
};

PM_IDLE_STATE PmIntelHaswellCstates[PM_INTEL_HASWELL_CSTATE_COUNT] = {
    {
        "C1",
        0,
        (PVOID)(0x00 | PM_INTEL_DISABLE_C1E_PROMOTION),
        1,
        2
    },
    {
        "C1E",
        0,
        (PVOID)0x01,
        10,
        20
    },
    {
        "C3",
        0,
        (PVOID)0x10,
        40,
        100
    },
    {
        "C6",
        0,
        (PVOID)0x20,
        150,
        400
    },
    {
        "C7s",
        0,
        (PVOID)0x32,
        160,
        500
    },
    {
        "C8",
        0,
        (PVOID)0x40,
        300,
        900
    },
    {
        "C9",
        0,
        (PVOID)0x50,
        600,
        1800
    },
    {
        "C10",
        0,
        (PVOID)0x60,
        2600,
        7700
    },
};

PM_IDLE_STATE PmIntelAtomCstates[PM_INTEL_ATOM_CSTATE_COUNT] = {
    {
        "C1",
        0,
        (PVOID)0x00,
        1,
        4
    },
    {
        "C2",
        0,
        (PVOID)0x10,
        20,
        80
    },
    {
        "C4",
        0,
        (PVOID)0x30,
        100,
        400
    },
    {
        "C6",
        0,
        (PVOID)0x52,
        150,
        550
    },
};

PM_IDLE_STATE PmIntelAvotonCstates[PM_INTEL_AVOTON_CSTATE_COUNT] = {
    {
        "C1",
        0,
        (PVOID)0x00,
        2,
        2
    },
    {
        "C6",
        0,
        (PVOID)0x51,
        15,
        45
    },
};

//
// ------------------------------------------------------------------ Functions
//

VOID
PmpIntelCstateDriverEntry (
    VOID
    )

/*++

Routine Description:

    This routine initializes support for Intel C-states. It will register
    itself as a processor idle state manager if it supports this processor.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG CState;
    ULONG CStateMask;
    PPM_INTEL_CSTATE_CONTEXT Data;
    UINTN DataSize;
    PPM_IDLE_STATE Destination;
    ULONG Eax;
    ULONG Ebx;
    ULONG Ecx;
    ULONG Edx;
    ULONG MaxLevel;
    ULONG Model;
    ULONG MwaitSubstates;
    RUNLEVEL OldRunLevel;
    PPROCESSOR_BLOCK ProcessorBlock;
    ULONG Shift;
    PPM_IDLE_STATE Source;
    ULONG StateCount;
    ULONG StateIndex;
    PPM_IDLE_STATE States;
    KSTATUS Status;
    ULONG Substates;

    Data = NULL;

    //
    // Get the vendor/family/model/stepping information out of the processor
    // block since it's all there. If this is extracted out to the driver, it
    // would need to do its own CPUIDing.
    //

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    ProcessorBlock = KeGetCurrentProcessorBlock();
    if ((ProcessorBlock->CpuVersion.Vendor != X86_VENDOR_INTEL) ||
        (ProcessorBlock->CpuVersion.Family != 6)) {

        KeLowerRunLevel(OldRunLevel);
        goto IntelCstateDriverEntryEnd;
    }

    Model = ProcessorBlock->CpuVersion.Model;
    KeLowerRunLevel(OldRunLevel);

    //
    // Make sure the MWAIT leaf is available.
    //

    Ecx = 0;
    Eax = X86_CPUID_IDENTIFICATION;
    ArCpuid(&Eax, &Ebx, &Ecx, &Edx);
    MaxLevel = Eax;
    if (MaxLevel < X86_CPUID_MWAIT) {
        goto IntelCstateDriverEntryEnd;
    }

    //
    // Make sure the monitor/mwait instructions are present.
    //

    Eax = X86_CPUID_BASIC_INFORMATION;
    Ecx = 0;
    ArCpuid(&Eax, &Ebx, &Ecx, &Edx);
    if ((Ecx & X86_CPUID_BASIC_ECX_MONITOR) == 0) {
        goto IntelCstateDriverEntryEnd;
    }

    //
    // Check if mwait has extensions and can be broken out even with interrupts
    // disabled.
    //

    Eax = X86_CPUID_MWAIT;
    Ecx = 0;
    ArCpuid(&Eax, &Ebx, &Ecx, &Edx);
    if (((Ecx & X86_CPUID_MWAIT_ECX_EXTENSIONS_SUPPORTED) == 0) ||
        ((Ecx & X86_CPUID_MWAIT_ECX_INTERRUPT_BREAK) == 0)) {

        goto IntelCstateDriverEntryEnd;
    }

    MwaitSubstates = Edx;
    switch (Model) {
    case 0x17:
        States = PmIntelPenrynCstates;
        StateCount = PM_INTEL_PENRYN_CSTATE_COUNT;
        break;

    //
    // Handle Penryn, Nehalem, Westmere models under the same group.
    //

    case 0x1A:
    case 0x1E:
    case 0x1F:
    case 0x25:
    case 0x2C:
    case 0x2E:
    case 0x2F:
        States = PmIntelNehalemCstates;
        StateCount = PM_INTEL_NEHALEM_CSTATE_COUNT;
        break;

    case 0x1C:
    case 0x26:
    case 0x36:
        States = PmIntelAtomCstates;
        StateCount = PM_INTEL_ATOM_CSTATE_COUNT;
        break;

    case 0x2A:
    case 0x2D:
        States = PmIntelSandyBridgeCstates;
        StateCount = PM_INTEL_SANDY_BRIDGE_CSTATE_COUNT;
        break;

    case 0x37:
    case 0x4C:
        States = PmIntelBayTrailCstates;
        StateCount = PM_INTEL_BAY_TRAIL_CSTATE_COUNT;
        break;

    case 0x3A:
        States = PmIntelIvyBridgeCstates;
        StateCount = PM_INTEL_IVY_BRIDGE_CSTATE_COUNT;
        break;

    case 0x3E:
        States = PmIntelIvyTownCstates;
        StateCount = PM_INTEL_IVY_TOWN_CSTATE_COUNT;
        break;

    //
    // Handle Haswell and Broadwell under the same group.
    //

    case 0x3C:
    case 0x3D:
    case 0x3F:
    case 0x45:
    case 0x46:
    case 0x47:
    case 0x4F:
    case 0x56:
        States = PmIntelHaswellCstates;
        StateCount = PM_INTEL_HASWELL_CSTATE_COUNT;
        break;

    case 0x4D:
        States = PmIntelAvotonCstates;
        StateCount = PM_INTEL_AVOTON_CSTATE_COUNT;
        break;

    default:
        RtlDebugPrint("Unknown Intel processor model 0x%x. "
                      "Disabling C-states.\n",
                      Model);

        goto IntelCstateDriverEntryEnd;
    }

    Data = MmAllocateNonPagedPool(sizeof(PM_INTEL_CSTATE_CONTEXT),
                                  PM_INTEL_CSTATE_ALLOCATION_TAG);

    if (Data == NULL) {
        goto IntelCstateDriverEntryEnd;
    }

    RtlZeroMemory(Data, sizeof(PM_INTEL_CSTATE_CONTEXT));

    //
    // Assuming that all CPUs are the same, go through and validate that each
    // C-state listed in the array is present in the processor.
    //

    CStateMask = 0;
    for (StateIndex = 0; StateIndex < StateCount; StateIndex += 1) {
        Source = States + StateIndex;

        //
        // Determine if the given state actually exists on the processor. Skip
        // any that don't.
        //

        CState = (((UINTN)Source->Context) & PM_INTEL_CSTATE_MASK) >>
                 PM_INTEL_CSTATE_SHIFT;

        Shift = (CState + 1) * 4;
        Substates = (MwaitSubstates >> Shift) & PM_INTEL_CSTATE_SUBSTATE_MASK;
        CStateMask |= PM_INTEL_CSTATE_SUBSTATE_MASK << Shift;
        if (Substates == 0) {
            continue;
        }

        Destination = &(Data->States[Data->StateCount]);
        RtlCopyMemory(Destination, Source, sizeof(PM_IDLE_STATE));
        Destination->ExitLatency =
                    KeConvertMicrosecondsToTimeTicks(Destination->ExitLatency);

        Destination->TargetResidency =
                KeConvertMicrosecondsToTimeTicks(Destination->TargetResidency);

        Data->StateCount += 1;
    }

    Data->MwaitSubstates = MwaitSubstates;
    Data->Model = Model;

    //
    // Notice if the CPU enumerated C-states that aren't in the hardcoded
    // arrays.
    //

    if ((MwaitSubstates & ~CStateMask) != 0) {
        RtlDebugPrint("Intel Model 0x%x had extra C-States: 0x%08x.\n",
                      Model,
                      MwaitSubstates & ~CStateMask);
    }

    //
    // If it ended up not enumerating any C-states, don't register the driver.
    //

    if (Data->StateCount == 0) {
        RtlDebugPrint("Intel: No C-states\n");
        MmFreeNonPagedPool(Data);
        goto IntelCstateDriverEntryEnd;
    }

    Data->Interface.InitializeIdleStates = PmpIntelInitializeCstates;
    Data->Interface.EnterIdleState = PmpIntelEnterCstate;
    Data->Interface.Context = Data;
    DataSize = sizeof(PM_IDLE_STATE_INTERFACE);
    Status = KeGetSetSystemInformation(SystemInformationPm,
                                       PmInformationIdleStateHandlers,
                                       &(Data->Interface),
                                       &DataSize,
                                       TRUE);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Intel: C-state registration failed: %d\n", Status);
        MmFreeNonPagedPool(Data);
    }

IntelCstateDriverEntryEnd:
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
PmpIntelInitializeCstates (
    PPM_IDLE_STATE_INTERFACE Interface,
    PPM_IDLE_PROCESSOR_STATE Processor
    )

/*++

Routine Description:

    This routine prototype represents a function that is called to go set up
    idle state information on the current processor. It should set the states
    and state count in the given processor idle information structure. This
    routine is called once on every processor. It runs at dispatch level.

Arguments:

    Interface - Supplies a pointer to the interface.

    Processor - Supplies a pointer to the context for this processor.

Return Value:

    Status code.

--*/

{

    PPM_INTEL_CSTATE_CONTEXT Data;
    ULONG Eax;
    ULONG Ebx;
    ULONG Ecx;
    ULONG Edx;
    ULONG Flags;
    ULONGLONG PowerControl;
    PPROCESSOR_BLOCK ProcessorBlock;

    Data = Interface->Context;

    //
    // Fail if the processor type is not the same as the original one that
    // everything was initialized for.
    //

    ProcessorBlock = KeGetCurrentProcessorBlock();
    if ((ProcessorBlock->CpuVersion.Vendor != X86_VENDOR_INTEL) ||
        (ProcessorBlock->CpuVersion.Model != Data->Model)) {

        ASSERT(FALSE);

        return STATUS_UNEXPECTED_TYPE;
    }

    Eax = X86_CPUID_MWAIT;
    Ecx = 0;
    ArCpuid(&Eax, &Ebx, &Ecx, &Edx);
    if (Edx != Data->MwaitSubstates) {

        ASSERT(FALSE);

        return STATUS_UNEXPECTED_TYPE;
    }

    Processor->States = Data->States;
    Processor->StateCount = Data->StateCount;
    if (Processor->StateCount != 0) {
        Flags = (UINTN)(Processor->States[0].Context);

        //
        // Disable automatic promotion of C1 to C1E by hardware if desired.
        //

        if ((Flags & PM_INTEL_DISABLE_C1E_PROMOTION) != 0) {
            PowerControl = ArReadMsr(X86_MSR_POWER_CONTROL);
            PowerControl &= ~X86_MSR_POWER_CONTROL_C1E_PROMOTION;
            ArWriteMsr(X86_MSR_POWER_CONTROL, PowerControl);
        }
    }

    return STATUS_SUCCESS;
}

VOID
PmpIntelEnterCstate (
    PPM_IDLE_PROCESSOR_STATE Processor,
    ULONG State
    )

/*++

Routine Description:

    This routine goes to the given C-state on Intel processors.

Arguments:

    Processor - Supplies a pointer to the information for the current processor.

    State - Supplies the new state index to change to.

Return Value:

    None. It is assumed when this function returns that the idle state was
    entered and then exited.

--*/

{

    ULONG Eax;
    ULONG Ecx;

    Eax = (UINTN)(Processor->States[State].Context) & PM_INTEL_CSTATE_MASK;
    Ecx = 1;
    ArMonitor(&Eax, 0, 0);
    ArMwait(Eax, Ecx);
    return;
}

