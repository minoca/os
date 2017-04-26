/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    am3soc.c

Abstract:

    This module implements the TI AM335x SoC driver.

Author:

    Evan Green 9-Sep-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/fw/acpitabs.h>
#include <minoca/soc/am335x.h>
#include <minoca/intrface/tps65217.h>
#include "mailbox.h"

//
// --------------------------------------------------------------------- Macros
//

#define AM3_READ_CM_WAKEUP(_Controller, _Register) \
    HlReadRegister32((_Controller)->Prcm + AM335_CM_WAKEUP_OFFSET + (_Register))

#define AM3_WRITE_CM_WAKEUP(_Controller, _Register, _Value)                    \
    HlWriteRegister32(                                                         \
                   (_Controller)->Prcm + AM335_CM_WAKEUP_OFFSET + (_Register), \
                   (_Value))

#define AM3_READ_PRM_WAKEUP(_Controller, _Register) \
    HlReadRegister32(                               \
                   (_Controller)->Prcm + AM335_PRM_WAKEUP_OFFSET + (_Register))

#define AM3_WRITE_PRM_WAKEUP(_Controller, _Register, _Value)                   \
    HlWriteRegister32(                                                         \
                  (_Controller)->Prcm + AM335_PRM_WAKEUP_OFFSET + (_Register), \
                  (_Value))

#define AM3_READ_PRCM(_Controller, _Register) \
    HlReadRegister32((_Controller)->Prcm + (_Register))

#define AM3_WRITE_PRCM(_Controller, _Register, _Value) \
    HlWriteRegister32((_Controller)->Prcm + (_Register), (_Value))

#define AM3_READ_CONTROL(_Controller, _Register) \
    HlReadRegister32((_Controller)->SocControl + (_Register))

#define AM3_WRITE_CONTROL(_Controller, _Register, _Value) \
    HlWriteRegister32((_Controller)->SocControl + (_Register), (_Value))

#define AM3_READ_EMIF(_Controller, _Register) \
    HlReadRegister32((_Controller)->Emif + (_Register))

#define AM3_WRITE_EMIF(_Controller, _Register, _Value) \
    HlWriteRegister32((_Controller)->Emif + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

#define AM3_SOC_ALLOCATION_TAG 0x53336D41

#define AM3_PERFORMANCE_STATE_COUNT 6

//
// Define the amount of time it takes for a performance state change to take
// effect, in microseconds.
//

#define AM3_SOC_PERFORMANCE_STATE_CHANGE_TIME 300000

//
// Define indices for certain performance states.
//

#define AM335_PERFORMANCE_STATE_600 2
#define AM335_PERFORMANCE_STATE_720 3
#define AM335_PERFORMANCE_STATE_800 4
#define AM335_PERFORMANCE_STATE_1000 5

#define AM3_SOC_IDLE_STATE_COUNT 2

#define AM335_M3_IPC_PARAMETER_DEFAULT 0xFFFFFFFF

#define AM335_IPC_MAX_SPIN_COUNT 50000

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _AM3_M3_STATE {
    Am3M3StateInvalid,
    Am3M3StateReset,
    Am3M3StateInitialized,
    Am3M3StatePowerMessage,
    Am3M3StateResetMessage
} AM3_M3_STATE, *PAM3_M3_STATE;

typedef enum _AM335_CONTROL_REGISTER {
    Am3ControlDeviceId = 0x600,
    Am3ControlEfuseSma = 0x7FC,
    Am3ControlM3TxEventEoi = 0x1324,
    Am3ControlIpc0 = 0x1328,
    Am3ControlIpc1 = 0x132C,
    Am3ControlIpc2 = 0x1330,
    Am3ControlIpc3 = 0x1334,
    Am3ControlIpc4 = 0x1338,
    Am3ControlIpc5 = 0x133C,
    Am3ControlIpc6 = 0x1340,
    Am3ControlIpc7 = 0x1344,
} AM335_CONTROL_REGISTER, *PAM335_CONTROL_REGISTER;

typedef enum _AM3_CM_WAKEUP_REGISTER {
    Am3CmWakeupClockStateControl = 0x00,
    Am3CmWakeupControlClockControl = 0x04,
    Am3CmWakeupGpio0ClockControl = 0x08,
    Am3CmWakeupL4WakeupClockControl = 0x0C,
    Am3CmWakeupTimer0ClockControl = 0x10,
    Am3CmWakeupDebugSsClockControl = 0x14,
    Am3CmWakeupL3AlwaysOnClockControl = 0x18,
    Am3CmWakeupAutoIdleDpllMpu = 0x1C,
    Am3CmWakeupIdleStateDpllMpu = 0x20,
    Am3CmWakeupSscDeltaMStepDpllMpu = 0x24,
    Am3CmWakeupSscModFrequencyDivDpllMpu = 0x28,
    Am3CmWakeupClockSelectDpllMpu = 0x2C,
    Am3CmWakeupAutoIdleDpllDdr = 0x30,
    Am3CmWakeupIdleStateDpllDdr = 0x34,
    Am3CmWakeupSscDeltaMStepDpllDdr = 0x38,
    Am3CmWakeupModFrequencyDivDpllDdr = 0x3C,
    Am3CmWakeupClockSelectDpllDdr = 0x40,
    Am3CmWakeupAutoIdleDpllDisp = 0x44,
    Am3CmWakeupIdleStateDpllDisp = 0x48,
    Am3CmWakeupSscDeltaMStepDpllDisp = 0x4C,
    Am3CmWakeupSscModFrequencyDivDpllDisp = 0x50,
    Am3CmWakeupClockSelectDpllDisp = 0x54,
    Am3CmWakeupAutoIdleDpllCore = 0x58,
    Am3CmWakeupIdleStateDpllCore = 0x5C,
    Am3CmWakeupSscDeltaMStepDpllCore = 0x60,
    Am3CmWakeupSscModFrequencyDivDpllCore = 0x64,
    Am3CmWakeupClockSelectDpllCore = 0x68,
    Am3CmWakeupAutoIdleDpllPer = 0x6C,
    Am3CmWakeupIdleStateDpllPer = 0x70,
    Am3CmWakeupSscDeltaMStepDpllPer = 0x74,
    Am3CmWakeupSscModFrequencyDivDpllPer = 0x78,
    Am3CmWakeupClkDcoLdoDpllPer = 0x7C,
    Am3CmWakeupDivM4DpllCore = 0x80,
    Am3CmWakeupDivM5DpllCore = 0x84,
    Am3CmWakeupClockModeDpllMpu = 0x88,
    Am3CmWakeupClockModeDpllPer = 0x8C,
    Am3CmWakeupClockModeDpllCore = 0x90,
    Am3CmWakeupClockModeDpllDdr = 0x94,
    Am3CmWakeupClockModeDpllDisp = 0x98,
    Am3CmWakeupClockSelectDpllPeriph = 0x9C,
    Am3CmWakeupDivM2DpllDdr = 0xA0,
    Am3CmWakeupDivM2DpllDisp = 0xA4,
    Am3CmWakeupDivM2DpllMpu = 0xA8,
    Am3CmWakeupDivM2DpllPer = 0xAC,
    Am3CmWakeupWakeupM3ClockControl = 0xB0,
    Am3CmWakeupUart0ClockControl = 0xB4,
    Am3CmWakeupI2c0ClockControl = 0xB8,
    Am3CmWakeupAdcTscClockControl = 0xBC,
    Am3CmWakeupSmartReflex0ClockControl = 0xC0,
    Am3CmWakeupTimer1ClockControl = 0xC4,
    Am3CmWakeupSmartReflex1ClockControl = 0xC8,
    Am3CmWakeupL4WakeupAlwaysOnClockStateControl = 0xCC,
    Am3CmWakeupWdt1ClockControl = 0xD4,
    Am3CmWakeupDivM6DpllCore = 0xD8
} AM3_CM_WAKEUP_REGISTER, *PAM3_CM_WAKEUP_REGISTER;

typedef enum _AM3_PRM_WAKEUP_REGISTER {
    Am3RmWakeupResetControl = 0x00,
    Am3PmWakeupPowerStateControl = 0x04,
    Am3PmWakeupPowerStatus = 0x08,
    Am3RmWakeupResetStatus = 0x0C
} AM3_PRM_WAKEUP_REGISTER, *PAM3_PRM_WAKEUP_REGISTER;

typedef enum _AM3_EMIF_REGISTER {
    Am3EmifRevision = 0x00,
    Am3EmifStatus = 0x04,
    Am3EmifSdramConfig = 0x08,
    Am3EmifSdramConfig2 = 0x0C,
    Am3EmifSdramRefControl = 0x10,
    Am3EmifSdramRefControlShadow = 0x14,
    Am3EmifSdramTiming1 = 0x18,
    Am3EmifSdramTiming1Shadow = 0x1C,
    Am3EmifSdramTiming2 = 0x20,
    Am3EmifSdramTiming2Shadow = 0x24,
    Am3EmifSdramTiming3 = 0x28,
    Am3EmifSdramTiming3Shadow = 0x2C,
    Am3EmifPowerManagementControl = 0x38,
    Am3EmifPowerManagementControlShadow = 0x3C,
    Am3EmifIntConfiguration = 0x54,
    Am3EmifIntConfigurationValue1 = 0x58,
    Am3EmifIntConfigurationValue2 = 0x5C,
    Am3EmifPerformanceCounter1 = 0x80,
    Am3EmifPerformanceCounter2 = 0x84,
    Am3EmifPerformanceConfig = 0x88,
    Am3EmifPerformanceSelect = 0x8C,
    Am3EmifPerformanceTiming = 0x90,
    Am3EmifReadIdleControl = 0x98,
    Am3EmifReadIdleControlShadow = 0x9C,
    Am3EmifInterruptStatusRawSys = 0xA4,
    Am3EmifInterruptStatusSys = 0xAC,
    Am3EmifInterruptEnableSetSys = 0xB4,
    Am3EmifInterruptEnableClearSys = 0xBC,
    Am3EmifZqConfiguration = 0xC8,
    Am3EmifRwLevelRampWindow = 0xD4,
    Am3EmifRwLevelRampContol = 0xD8,
    Am3EmifRwLevelControl = 0xDC,
    Am3EmifDdrPhyControl1 = 0xE4,
    Am3EmifDdrPhyControl1Shadow = 0xE8,
    Am3EmifPriCosMap = 0x100,
    Am3EmifConnidCos1Map = 0x104,
    Am3EmifConnidCos2Map = 0x108,
    Am3EmifRwExecThreshold = 0x120
} AM3_EMIF_REGISTER, *PAM3_EMIF_REGISTER;

typedef enum _AM3_IDLE_STATE {
    Am3IdleSelfRefreshWfi = 0,
    Am3IdleStandby = 1,
} AM3_IDLE_STATE, *PAM3_IDLE_STATE;

typedef enum _AM3_CM3_COMMAND {
    Am3Cm3CommandRtc = 0x1,
    Am3Cm3CommandRtcFast = 0x2,
    Am3Cm3CommandDs0 = 0x3,
    Am3Cm3CommandDs1 = 0x5,
    Am3Cm3CommandDs2 = 0x7,
    Am3Cm3CommandStandaloneApp = 0x9,
    Am3Cm3CommandStandby = 0xB,
    Am3Cm3CommandResetStateMachine = 0xE,
    Am3Cm3CommandVersion = 0xF
} AM3_CM3_COMMAND, *PAM3_CM3_COMMAND;

typedef enum _AM3_CM3_RESPONSE {
    Am3Cm3ResponsePass = 0x0,
    Am3Cm3ResponseFail = 0x1,
    Am3Cm3ResponseWait4Ok = 0x2
} AM3_CM3_RESPONSE, *PAM3_CM3_RESPONSE;

/*++

Structure Description:

    This structure defines the actual configuration values for a particular
    performance state in the AM335x SoC.

Members:

    ResumeAddress - Stores the address that ROM code should jump to for
        resume.

    Command - Stores the desired command to send to the Cortex M3.

    Data - Stores additional arguments.

--*/

typedef struct _AM3_WAKE_M3_IPC_DATA {
    ULONG ResumeAddress;
    ULONG Command;
    ULONG Data[4];
} AM3_WAKE_M3_IPC_DATA, *PAM3_WAKE_M3_IPC_DATA;

/*++

Structure Description:

    This structure defines the context for an AM335x generic SoC controller.

Members:

    OsDevice - Stores a pointer to the OS device object.

    Prcm - Stores the virtual address of the memory mapping to the
        PRCM region.

    SocControl - Stores the virtual address of the memory mapping to the SOC
        Control region.

    CortexM3Code - Stores the virtual address of the Cortex M3 code region.

    CortexM3Data - Stores the virtual address of the Cortex M3 data region.

    Emif - Stores the virtual address of the EMIF interface.

    Ocmc - Stores a pointer to the OCMC L3 RAM.

    OcmcPhysical - Stores the physical address of the OCMC RAM region.

    Lock - Stores a pointer to a lock serializing access to the controller.

    Tps65217 - Stores a pointer to the TPS65217 interface.

    Tps65217SignedUp - Stores a boolean indicating whether or not TPS65217
        interface notifications have been registered.

    CurrentPerformanceState - Stores the current performance state index.

    DesiredPerformanceState = Stores the desired performance state index.

    SocRevision - Stores the SOC revision number.

    WakeM3InterruptLine - Stores the interrupt line that the Cortex M3
        interrupt comes in on.

    WakeM3InterruptVector - Stores the interrupt vector that the Cortex M3
        interrupt comes in on.

    WakeM3InterruptHandle - Stores a pointer to the handle received when the
        Cortex M3 interrupt was connected.

    M3State - Stores the state of the Cortex M3.

    M3Ipc - Stores the IPC data for sleep transitions using the M3.

    Mailbox - Stores the mailbox device context.

    IdleInterface - Stores the idle state interface.

    HlSuspendInterface - Stores the low level suspend interface.

    IdleState - Stores the current idle state undergoing transition.

--*/

typedef struct _AM3_SOC {
    PDEVICE OsDevice;
    PVOID Prcm;
    PVOID SocControl;
    PVOID CortexM3Code;
    PVOID CortexM3Data;
    PVOID Emif;
    PVOID Ocmc;
    PHYSICAL_ADDRESS OcmcPhysical;
    PQUEUED_LOCK Lock;
    PINTERFACE_TPS65217 Tps65217;
    BOOL Tps65217SignedUp;
    ULONG CurrentPerformanceState;
    ULONG DesiredPerformanceState;
    ULONG SocRevision;
    ULONGLONG WakeM3InterruptLine;
    ULONGLONG WakeM3InterruptVector;
    HANDLE WakeM3InterruptHandle;
    AM3_M3_STATE M3State;
    AM3_WAKE_M3_IPC_DATA M3Ipc;
    AM3_MAILBOX Mailbox;
    PM_IDLE_STATE_INTERFACE IdleInterface;
    HL_SUSPEND_INTERFACE HlSuspendInterface;
    AM3_IDLE_STATE IdleState;
} AM3_SOC, *PAM3_SOC;

/*++

Structure Description:

    This structure defines the actual configuration values for a particular
    performance state in the AM335x SoC.

Members:

    PllMultiplier - Stores the MPU PLL multiplier.

    Millivolts - Stores the millivolts value to set the PMIC to.

--*/

typedef struct _AM335_PERFORMANCE_CONFIGURATION {
    ULONG PllMultiplier;
    ULONG Millivolts;
} AM335_PERFORMANCE_CONFIGURATION, *PAM335_PERFORMANCE_CONFIGURATION;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
Am3SocAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
Am3SocDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Am3SocDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Am3SocDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Am3SocDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Am3SocDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

KSTATUS
Am3SocProcessResourceRequirements (
    PIRP Irp
    );

KSTATUS
Am3SocStartDevice (
    PIRP Irp,
    PAM3_SOC Device
    );

VOID
Am3SocTps65217InterfaceNotificationCallback (
    PVOID Context,
    PDEVICE Device,
    PVOID InterfaceBuffer,
    ULONG InterfaceBufferSize,
    BOOL Arrival
    );

KSTATUS
Am3SocRegisterPerformanceStates (
    PAM3_SOC Controller
    );

ULONG
Am3SocGetMaxPerformanceState (
    PAM3_SOC Controller
    );

KSTATUS
Am3SocSetPerformanceState (
    PPM_PERFORMANCE_STATE_INTERFACE Interface,
    ULONG State
    );

VOID
Am3SocSetPerformanceStateThread (
    PVOID Parameter
    );

VOID
Am3SocProgramMpuPll (
    PAM3_SOC Controller,
    ULONG Multiplier
    );

KSTATUS
Am3SocStartCortexM3 (
    PAM3_SOC Device
    );

KSTATUS
Am3SocRegisterIdleInterface (
    PAM3_SOC Device
    );

KSTATUS
Am3SocInitializeIdleStates (
    PPM_IDLE_STATE_INTERFACE Interface,
    PPM_IDLE_PROCESSOR_STATE Processor
    );

VOID
Am3SocEnterIdleState (
    PPM_IDLE_PROCESSOR_STATE Processor,
    ULONG State
    );

VOID
Am3SocEnterSelfRefreshWfi (
    PAM3_SOC Device
    );

VOID
Am3SocEnterStandby (
    PAM3_SOC Device
    );

KSTATUS
Am3SocSuspendCallback (
    PVOID Context,
    HL_SUSPEND_PHASE Phase
    );

KSTATUS
Am3SocSuspendBegin (
    PAM3_SOC Device
    );

KSTATUS
Am3SocSuspendEnd (
    PAM3_SOC Device
    );

KSTATUS
Am3SocResetM3 (
    PAM3_SOC Device
    );

VOID
Am3SocSetupIpc (
    PAM3_SOC Device
    );

KSTATUS
Am3SocWaitForIpcResult (
    PAM3_SOC Device
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER Am3SocDriver = NULL;
UUID Am3SocTps65217InterfaceUuid = UUID_TPS65217_INTERFACE;

//
// Set this debug boolean to print out all performance state changes.
//

BOOL Am3SocPrintPerformanceStateChanges;

//
// Define the performance states. This may be platform specific, in which case
// the data will need to come from somewhere.
//

PM_PERFORMANCE_STATE Am3SocPerformanceStates[AM3_PERFORMANCE_STATE_COUNT] = {
    {275000, 170},
    {500000, 170},
    {600000, 170},
    {720000, 170},
    {800000, 170},
    {1000000, 174},
};

//
// Define the TPS65217 voltage settings that go along with each performance
// state, in millivolts
//

AM335_PERFORMANCE_CONFIGURATION
    Am3PerformanceConfigurations[AM3_PERFORMANCE_STATE_COUNT] = {

    {275, 1100},
    {500, 1100},
    {600, 1200},
    {720, 1200},
    {800, 1275},
    {1000, 1325}
};

PM_PERFORMANCE_STATE_INTERFACE Am3SocPerformanceStateInterface = {
    0,
    0,
    Am3SocPerformanceStates,
    AM3_PERFORMANCE_STATE_COUNT,
    Am3SocSetPerformanceState,
    NULL
};

//
// An estimate for standby (C3) was taken by snapping the time counter
// between SuspendBegin and SuspendEnd, and modifying the sleep code to never
// WFI (so it's all overhead, no sleep). Averaging over about 100 iterations,
// the overall latency was about 2440 microseconds.
//

PM_IDLE_STATE Am3SocIdleStates[AM3_SOC_IDLE_STATE_COUNT] = {
    {
        "C2",
         0,
         NULL,
         100,
         1000
    },
    {
        "C3",
         0,
         NULL,
         2500,
         5000
    }
};

ULONG Am3SocIdleStateCount = AM3_SOC_IDLE_STATE_COUNT;

//
// The Cortex M3 firmware is built in to the driver.
//

extern PVOID _binary_am3cm3fw_bin_start;
extern PVOID _binary_am3cm3fw_bin_end;

extern PVOID Am3SocOcmcCode;
extern PVOID Am3SocRefreshWfi;
extern PVOID Am3SocStandby;
extern PVOID Am3SocResumeStandby;
extern PVOID Am3SocSleep;
extern PVOID Am3SocResume;
extern PVOID Am3SocOcmcCodeEnd;

//
// ------------------------------------------------------------------ Functions
//

__USED
KSTATUS
DriverEntry (
    PDRIVER Driver
    )

/*++

Routine Description:

    This routine is the entry point for the AM335x SoC driver. It registers its
    other dispatch functions, and performs driver-wide initialization.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    STATUS_SUCCESS on success.

    Failure code on error.

--*/

{

    DRIVER_FUNCTION_TABLE FunctionTable;
    KSTATUS Status;

    Am3SocDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = Am3SocAddDevice;
    FunctionTable.DispatchStateChange = Am3SocDispatchStateChange;
    FunctionTable.DispatchOpen = Am3SocDispatchOpen;
    FunctionTable.DispatchClose = Am3SocDispatchClose;
    FunctionTable.DispatchIo = Am3SocDispatchIo;
    FunctionTable.DispatchSystemControl = Am3SocDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

KSTATUS
Am3SocAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a device is detected for which this driver
    acts as the function driver. The driver will attach itself to the stack.

Arguments:

    Driver - Supplies a pointer to the driver being called.

    DeviceId - Supplies a pointer to a string with the device ID.

    ClassId - Supplies a pointer to a string containing the device's class ID.

    CompatibleIds - Supplies a pointer to a string containing device IDs
        that would be compatible with this device.

    DeviceToken - Supplies an opaque token that the driver can use to identify
        the device in the system. This token should be used when attaching to
        the stack.

Return Value:

    STATUS_SUCCESS on success.

    Failure code if the driver was unsuccessful in attaching itself.

--*/

{

    PAM3_SOC Soc;
    KSTATUS Status;

    Soc = MmAllocateNonPagedPool(sizeof(AM3_SOC), AM3_SOC_ALLOCATION_TAG);
    if (Soc == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddDeviceEnd;
    }

    RtlZeroMemory(Soc, sizeof(AM3_SOC));
    Soc->WakeM3InterruptLine = INVALID_INTERRUPT_LINE;
    Soc->WakeM3InterruptHandle = INVALID_HANDLE;
    Soc->Lock = KeCreateQueuedLock();
    if (Soc->Lock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddDeviceEnd;
    }

    Status = IoAttachDriverToDevice(Driver, DeviceToken, Soc);

AddDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Soc != NULL) {
            if (Soc->Lock != NULL) {
                KeDestroyQueuedLock(Soc->Lock);
            }

            MmFreeNonPagedPool(Soc);
        }
    }

    return Status;
}

VOID
Am3SocDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles State Change IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    if (Irp->Direction == IrpUp) {
        switch (Irp->MinorCode) {
        case IrpMinorQueryResources:
            Status = Am3SocProcessResourceRequirements(Irp);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(Am3SocDriver, Irp, Status);
            }

            break;

        case IrpMinorStartDevice:
            Status = Am3SocStartDevice(Irp, DeviceContext);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(Am3SocDriver, Irp, Status);
            }

            break;

        default:
            break;
        }
    }

    return;
}

VOID
Am3SocDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles Open IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    return;
}

VOID
Am3SocDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles Close IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    return;
}

VOID
Am3SocDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles I/O IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    return;
}

VOID
Am3SocDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles System Control IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    ASSERT(Irp->MajorCode == IrpMajorSystemControl);

    //
    // Do no processing on any IRPs. Let them flow.
    //

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
Am3SocProcessResourceRequirements (
    PIRP Irp
    )

/*++

Routine Description:

    This routine filters through the resource requirements presented by the
    bus for an e100 LAN controller. It adds an interrupt vector requirement for
    any interrupt line requested.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

Return Value:

    Status code.

--*/

{

    PRESOURCE_CONFIGURATION_LIST Requirements;
    KSTATUS Status;
    RESOURCE_REQUIREMENT VectorRequirement;

    ASSERT((Irp->MajorCode == IrpMajorStateChange) &&
           (Irp->MinorCode == IrpMinorQueryResources));

    //
    // Initialize a nice interrupt vector requirement in preparation.
    //

    RtlZeroMemory(&VectorRequirement, sizeof(RESOURCE_REQUIREMENT));
    VectorRequirement.Type = ResourceTypeInterruptVector;
    VectorRequirement.Minimum = 0;
    VectorRequirement.Maximum = -1;
    VectorRequirement.Length = 1;

    //
    // Loop through all configuration lists, creating a vector for each line.
    //

    Requirements = Irp->U.QueryResources.ResourceRequirements;
    Status = IoCreateAndAddInterruptVectorsForLines(Requirements,
                                                    &VectorRequirement);

    if (!KSUCCESS(Status)) {
        goto ProcessResourceRequirementsEnd;
    }

ProcessResourceRequirementsEnd:
    return Status;
}

KSTATUS
Am3SocStartDevice (
    PIRP Irp,
    PAM3_SOC Device
    )

/*++

Routine Description:

    This routine starts the AM335x SoC "device".

Arguments:

    Irp - Supplies a pointer to the start IRP.

    Device - Supplies a pointer to the device information.

Return Value:

    Status code.

--*/

{

    ULONG AlignmentOffset;
    PRESOURCE_ALLOCATION Allocation;
    PRESOURCE_ALLOCATION_LIST AllocationList;
    PRESOURCE_ALLOCATION CortexM3Code;
    PRESOURCE_ALLOCATION CortexM3Data;
    PRESOURCE_ALLOCATION Emif;
    PHYSICAL_ADDRESS EndAddress;
    PRESOURCE_ALLOCATION LineAllocation;
    PRESOURCE_ALLOCATION Mailbox;
    ULONGLONG MailboxInterruptLine;
    ULONGLONG MailboxInterruptVector;
    PRESOURCE_ALLOCATION OcmcRam;
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    PRESOURCE_ALLOCATION Prcm;
    ULONG Size;
    PRESOURCE_ALLOCATION SocControl;
    KSTATUS Status;

    CortexM3Code = NULL;
    CortexM3Data = NULL;
    Emif = NULL;
    Mailbox = NULL;
    OcmcRam = NULL;
    Prcm = NULL;
    SocControl = NULL;
    MailboxInterruptLine = INVALID_INTERRUPT_LINE;
    MailboxInterruptVector = INVALID_INTERRUPT_VECTOR;

    //
    // Loop through the allocated resources to get the controller base.
    //

    AllocationList = Irp->U.StartDevice.ProcessorLocalResources;
    Allocation = IoGetNextResourceAllocation(AllocationList, NULL);
    while (Allocation != NULL) {

        //
        // If the resource is an interrupt vector, then it should have an
        // owning interrupt line allocation.
        //

        if (Allocation->Type == ResourceTypeInterruptVector) {

            ASSERT(Allocation->OwningAllocation != NULL);

            //
            // Save the line and vector number.
            //

            LineAllocation = Allocation->OwningAllocation;
            if (Device->WakeM3InterruptLine == INVALID_INTERRUPT_LINE) {
                Device->WakeM3InterruptLine = LineAllocation->Allocation;
                Device->WakeM3InterruptVector = Allocation->Allocation;

            } else if (MailboxInterruptLine == INVALID_INTERRUPT_LINE) {
                MailboxInterruptLine = LineAllocation->Allocation;
                MailboxInterruptVector = Allocation->Allocation;
            }

        //
        // There should be two physical address allocations: one for the PRCM,
        // and another for the SoC control region.
        //

        } else if (Allocation->Type == ResourceTypePhysicalAddressSpace) {
            if (Prcm == NULL) {
                Prcm = Allocation;

            } else if (SocControl == NULL) {
                SocControl = Allocation;

            } else if (CortexM3Code == NULL) {
                CortexM3Code = Allocation;

            } else if (CortexM3Data == NULL) {
                CortexM3Data = Allocation;

            } else if (Mailbox == NULL) {
                Mailbox = Allocation;

            } else if (OcmcRam == NULL) {
                OcmcRam = Allocation;

            } else if (Emif == NULL) {
                Emif = Allocation;
            }
        }

        Allocation = IoGetNextResourceAllocation(AllocationList, Allocation);
    }

    //
    // Fail to start if the controller base was not found.
    //

    if ((Prcm == NULL) ||
        (Prcm->Length < AM335_PRCM_SIZE) ||
        (SocControl == NULL) ||
        (SocControl->Length < AM335_SOC_CONTROL_SIZE) ||
        (CortexM3Code == NULL) ||
        (CortexM3Code->Length < AM335_CORTEX_M3_CODE_SIZE) ||
        (CortexM3Data == NULL) ||
        (Mailbox == NULL) ||
        (OcmcRam == NULL)) {

        Status = STATUS_INVALID_CONFIGURATION;
        goto StartDeviceEnd;
    }

    //
    // Map the PRCM.
    //

    if (Device->Prcm == NULL) {

        //
        // Page align the mapping request.
        //

        PageSize = MmPageSize();
        PhysicalAddress = Prcm->Allocation;
        EndAddress = PhysicalAddress + Prcm->Length;
        PhysicalAddress = ALIGN_RANGE_DOWN(PhysicalAddress, PageSize);
        AlignmentOffset = Prcm->Allocation - PhysicalAddress;
        EndAddress = ALIGN_RANGE_UP(EndAddress, PageSize);
        Size = (ULONG)(EndAddress - PhysicalAddress);

        //
        // If the size is not a the constant, then the failure code at the
        // bottom needs to be fancier.
        //

        ASSERT(Size == AM335_PRCM_SIZE);

        Device->Prcm = MmMapPhysicalAddress(PhysicalAddress,
                                            Size,
                                            TRUE,
                                            FALSE,
                                            TRUE);

        if (Device->Prcm == NULL) {
            Status = STATUS_NO_MEMORY;
            goto StartDeviceEnd;
        }

        Device->Prcm += AlignmentOffset;
    }

    ASSERT(Device->Prcm != NULL);

    //
    // Map the SoC control region.
    //

    if (Device->SocControl == NULL) {

        //
        // Page align the mapping request.
        //

        PageSize = MmPageSize();
        PhysicalAddress = SocControl->Allocation;
        EndAddress = PhysicalAddress + SocControl->Length;
        PhysicalAddress = ALIGN_RANGE_DOWN(PhysicalAddress, PageSize);
        AlignmentOffset = SocControl->Allocation - PhysicalAddress;
        EndAddress = ALIGN_RANGE_UP(EndAddress, PageSize);
        Size = (ULONG)(EndAddress - PhysicalAddress);

        //
        // If the size is not a the constant, then the failure code at the
        // bottom needs to be fancier.
        //

        ASSERT(Size == AM335_SOC_CONTROL_SIZE);

        Device->SocControl = MmMapPhysicalAddress(PhysicalAddress,
                                                  Size,
                                                  TRUE,
                                                  FALSE,
                                                  TRUE);

        if (Device->SocControl == NULL) {
            Status = STATUS_NO_MEMORY;
            goto StartDeviceEnd;
        }

        Device->SocControl += AlignmentOffset;
    }

    ASSERT(Device->SocControl != NULL);

    //
    // Map the OCMC RAM region.
    //

    if (Device->Ocmc == NULL) {

        //
        // Page align the mapping request.
        //

        PageSize = MmPageSize();
        PhysicalAddress = OcmcRam->Allocation;
        EndAddress = PhysicalAddress + OcmcRam->Length;
        PhysicalAddress = ALIGN_RANGE_DOWN(PhysicalAddress, PageSize);
        AlignmentOffset = OcmcRam->Allocation - PhysicalAddress;
        EndAddress = ALIGN_RANGE_UP(EndAddress, PageSize);
        Size = (ULONG)(EndAddress - PhysicalAddress);

        //
        // If the size is not a the constant, then the failure code at the
        // bottom needs to be fancier.
        //

        ASSERT(Size == AM335_OCMC_SIZE);

        Device->Ocmc = MmMapPhysicalAddress(PhysicalAddress,
                                            Size,
                                            TRUE,
                                            FALSE,
                                            TRUE);

        if (Device->Ocmc == NULL) {
            Status = STATUS_NO_MEMORY;
            goto StartDeviceEnd;
        }

        Device->Ocmc += AlignmentOffset;
        Device->OcmcPhysical = OcmcRam->Allocation;
    }

    //
    // Map the Cortex M3 region.
    //

    if (Device->CortexM3Code == NULL) {

        //
        // Page align the mapping request.
        //

        PageSize = MmPageSize();
        PhysicalAddress = CortexM3Code->Allocation;
        EndAddress = PhysicalAddress + CortexM3Code->Length;
        PhysicalAddress = ALIGN_RANGE_DOWN(PhysicalAddress, PageSize);
        AlignmentOffset = CortexM3Code->Allocation - PhysicalAddress;
        EndAddress = ALIGN_RANGE_UP(EndAddress, PageSize);
        Size = (ULONG)(EndAddress - PhysicalAddress);

        //
        // If the size is not a the constant, then the failure code at the
        // bottom needs to be fancier.
        //

        ASSERT(Size == AM335_CORTEX_M3_CODE_SIZE);

        Device->CortexM3Code = MmMapPhysicalAddress(PhysicalAddress,
                                                    Size,
                                                    TRUE,
                                                    FALSE,
                                                    TRUE);

        if (Device->CortexM3Code == NULL) {
            Status = STATUS_NO_MEMORY;
            goto StartDeviceEnd;
        }

        Device->CortexM3Code += AlignmentOffset;
    }

    ASSERT(Device->CortexM3Code != NULL);

    if (Device->CortexM3Data == NULL) {

        //
        // Page align the mapping request.
        //

        PageSize = MmPageSize();
        PhysicalAddress = CortexM3Data->Allocation;
        EndAddress = PhysicalAddress + CortexM3Data->Length;
        PhysicalAddress = ALIGN_RANGE_DOWN(PhysicalAddress, PageSize);
        AlignmentOffset = CortexM3Data->Allocation - PhysicalAddress;
        EndAddress = ALIGN_RANGE_UP(EndAddress, PageSize);
        Size = (ULONG)(EndAddress - PhysicalAddress);

        //
        // If the size is not a the constant, then the failure code at the
        // bottom needs to be fancier.
        //

        ASSERT(Size == AM335_CORTEX_M3_DATA_SIZE);

        Device->CortexM3Data = MmMapPhysicalAddress(PhysicalAddress,
                                                    Size,
                                                    TRUE,
                                                    FALSE,
                                                    TRUE);

        if (Device->CortexM3Data == NULL) {
            Status = STATUS_NO_MEMORY;
            goto StartDeviceEnd;
        }

        Device->CortexM3Data += AlignmentOffset;
    }

    ASSERT(Device->CortexM3Data != NULL);

    //
    // Map the EMIF region.
    //

    if (Device->Emif == NULL) {
        PageSize = MmPageSize();
        PhysicalAddress = Emif->Allocation;
        EndAddress = PhysicalAddress + Emif->Length;
        PhysicalAddress = ALIGN_RANGE_DOWN(PhysicalAddress, PageSize);
        AlignmentOffset = Emif->Allocation - PhysicalAddress;
        EndAddress = ALIGN_RANGE_UP(EndAddress, PageSize);
        Size = (ULONG)(EndAddress - PhysicalAddress);

        //
        // If the size is not a the constant, then the failure code at the
        // bottom needs to be fancier.
        //

        ASSERT(Size == AM335_EMIF_SIZE);

        Device->Emif = MmMapPhysicalAddress(PhysicalAddress,
                                            Size,
                                            TRUE,
                                            FALSE,
                                            TRUE);

        if (Device->Emif == NULL) {
            Status = STATUS_NO_MEMORY;
            goto StartDeviceEnd;
        }

        Device->Emif += AlignmentOffset;
    }

    ASSERT(Device->Emif != NULL);

    //
    // Fire up the mailbox support.
    //

    Status = Am3MailboxInitialize(&(Device->Mailbox),
                                  Irp,
                                  Mailbox,
                                  MailboxInterruptLine,
                                  MailboxInterruptVector);

    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    //
    // Determine the SoC revision.
    //

    Device->SocRevision = AM3_READ_CONTROL(Device, Am3ControlDeviceId) >>
                          AM335_SOC_CONTROL_DEVICE_ID_REVISION_SHIFT;

    //
    // Sign up for PMIC notifications.
    //

    if (Device->Tps65217SignedUp == FALSE) {
        Status = IoRegisterForInterfaceNotifications(
                                   &Am3SocTps65217InterfaceUuid,
                                   Am3SocTps65217InterfaceNotificationCallback,
                                   NULL,
                                   Device,
                                   TRUE);

        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }

        Device->Tps65217SignedUp = TRUE;
    }

    Status = Am3SocStartCortexM3(Device);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    Status = Am3SocRegisterIdleInterface(Device);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    Status = STATUS_SUCCESS;

StartDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Device->Tps65217SignedUp != FALSE) {
            Status = IoUnregisterForInterfaceNotifications(
                                   &Am3SocTps65217InterfaceUuid,
                                   Am3SocTps65217InterfaceNotificationCallback,
                                   NULL,
                                   Device);

            ASSERT(KSUCCESS(Status));
        }

        if (Device->Prcm != NULL) {
            MmUnmapAddress(Device->Prcm, AM335_PRCM_SIZE);
            Device->Prcm = NULL;
        }

        if (Device->SocControl != NULL) {
            MmUnmapAddress(Device->SocControl, AM335_SOC_CONTROL_SIZE);
            Device->SocControl = NULL;
        }

        if (Device->CortexM3Code != NULL) {
            MmUnmapAddress(Device->CortexM3Code, AM335_CORTEX_M3_CODE_SIZE);
            Device->CortexM3Code = NULL;
        }

        if (Device->CortexM3Data != NULL) {
            MmUnmapAddress(Device->CortexM3Data, AM335_CORTEX_M3_DATA_SIZE);
            Device->CortexM3Data = NULL;
        }

        if (Device->Emif != NULL) {
            MmUnmapAddress(Device->Emif, AM335_EMIF_SIZE);
            Device->Emif = NULL;
        }

        if (Device->Ocmc != NULL) {
            MmUnmapAddress(Device->Ocmc, AM335_OCMC_SIZE);
            Device->Ocmc = NULL;
        }

        if (Device->Mailbox.ControllerBase != NULL) {
            Am3MailboxDestroy(&(Device->Mailbox));
        }
    }

    return Status;
}

VOID
Am3SocTps65217InterfaceNotificationCallback (
    PVOID Context,
    PDEVICE Device,
    PVOID InterfaceBuffer,
    ULONG InterfaceBufferSize,
    BOOL Arrival
    )

/*++

Routine Description:

    This routine is called to notify listeners that an interface has arrived
    or departed.

Arguments:

    Context - Supplies the caller's context pointer, supplied when the caller
        requested interface notifications.

    Device - Supplies a pointer to the device exposing or deleting the
        interface.

    InterfaceBuffer - Supplies a pointer to the interface buffer of the
        interface.

    InterfaceBufferSize - Supplies the buffer size.

    Arrival - Supplies TRUE if a new interface is arriving, or FALSE if an
        interface is departing.

Return Value:

    None.

--*/

{

    PAM3_SOC Controller;
    PINTERFACE_TPS65217 Interface;

    Controller = Context;
    KeAcquireQueuedLock(Controller->Lock);

    //
    // If the interface is arriving, store a pointer to it.
    //

    if (Arrival != FALSE) {
        Interface = InterfaceBuffer;
        if (InterfaceBufferSize < sizeof(INTERFACE_TPS65217)) {

            ASSERT(FALSE);

            return;
        }

        ASSERT(Controller->Tps65217 == NULL);

        Controller->Tps65217 = Interface;
        Am3SocRegisterPerformanceStates(Controller);

    //
    // The interface is disappearing.
    //

    } else {
        Controller->Tps65217 = NULL;
    }

    KeReleaseQueuedLock(Controller->Lock);
    return;
}

KSTATUS
Am3SocRegisterPerformanceStates (
    PAM3_SOC Controller
    )

/*++

Routine Description:

    This routine registers the performance state interface with the system.

Arguments:

    Controller - Supplies a pointer to the device structure.

Return Value:

    Status code.

--*/

{

    UINTN DataSize;
    ULONG StateCount;
    KSTATUS Status;

    ASSERT(Am3SocPerformanceStateInterface.Context == NULL);

    StateCount = Am3SocGetMaxPerformanceState(Controller);
    if (StateCount == 0) {
        return STATUS_NOT_SUPPORTED;
    }

    Controller->CurrentPerformanceState = StateCount - 1;
    Am3SocPerformanceStateInterface.Context = Controller;
    Am3SocPerformanceStateInterface.MinimumPeriod =
        KeConvertMicrosecondsToTimeTicks(AM3_SOC_PERFORMANCE_STATE_CHANGE_TIME);

    Am3SocPerformanceStateInterface.StateCount = StateCount;
    DataSize = sizeof(Am3SocPerformanceStateInterface);
    Status = KeGetSetSystemInformation(SystemInformationPm,
                                       PmInformationPerformanceStateHandlers,
                                       &Am3SocPerformanceStateInterface,
                                       &DataSize,
                                       TRUE);

    if (!KSUCCESS(Status)) {
        Am3SocPerformanceStateInterface.Context = NULL;
    }

    return Status;
}

ULONG
Am3SocGetMaxPerformanceState (
    PAM3_SOC Controller
    )

/*++

Routine Description:

    This routine determines the maximum frequency at which the SoC can run,
    and therefore the maximum performance state.

Arguments:

    Controller - Supplies a pointer to the device structure.

Return Value:

    Returns the number of performance states available.

    0 if no performance states should be exposed.

--*/

{

    ULONG Efuse;
    ULONG MaxState;

    MaxState = 0;
    if (Controller->SocRevision == AM335_SOC_DEVICE_VERSION_1_0) {
        MaxState = AM335_PERFORMANCE_STATE_720;

    } else if (Controller->SocRevision == AM335_SOC_DEVICE_VERSION_2_0) {
        MaxState = AM335_PERFORMANCE_STATE_800;

    } else if (Controller->SocRevision == AM335_SOC_DEVICE_VERSION_2_1) {
        Efuse = AM3_READ_CONTROL(Controller, Am3ControlEfuseSma);
        Efuse &= AM335_SOC_CONTROL_EFUSE_OPP_MASK;
        if ((Efuse & AM335_EFUSE_OPPNT_1000_MASK) == 0) {
            MaxState = AM335_PERFORMANCE_STATE_1000;

        } else if ((Efuse & AM335_EFUSE_OPPTB_800_MASK) == 0) {
            MaxState = AM335_PERFORMANCE_STATE_800;

        } else if ((Efuse & AM335_EFUSE_OPP120_720_MASK) == 0) {
            MaxState = AM335_PERFORMANCE_STATE_720;

        } else if ((Efuse & AM335_EFUSE_OPP100_600_MASK) == 0) {
            MaxState = AM335_PERFORMANCE_STATE_600;
        }
    }

    if (MaxState == 0) {
        return 0;
    }

    return MaxState + 1;
}

KSTATUS
Am3SocSetPerformanceState (
    PPM_PERFORMANCE_STATE_INTERFACE Interface,
    ULONG State
    )

/*++

Routine Description:

    This routine changes the current performance state.

Arguments:

    Interface - Supplies a pointer to the interface.

    State - Supplies the new state index to change to.

Return Value:

    Status code.

--*/

{

    PAM3_SOC Controller;
    THREAD_CREATION_PARAMETERS Parameters;
    KSTATUS Status;

    Controller = Interface->Context;
    Controller->DesiredPerformanceState = State;

    //
    // TODO: Once I/O is allowed on work items, do the change directly rather
    // than spawning a thread to do it.
    //

    RtlZeroMemory(&Parameters, sizeof(THREAD_CREATION_PARAMETERS));
    Parameters.ThreadRoutine = Am3SocSetPerformanceStateThread;
    Parameters.Parameter = Controller;
    Status = PsCreateThread(&Parameters);
    return Status;
}

VOID
Am3SocSetPerformanceStateThread (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine changes the current performance state (actually).

Arguments:

    Parameter - Supplies a pointer to the AM3 SOC controller.

Return Value:

    None.

--*/

{

    PAM335_PERFORMANCE_CONFIGURATION Configuration;
    PAM3_SOC Controller;
    ULONG CurrentState;
    ULONG DesiredState;
    KSTATUS Status;

    Controller = Parameter;
    DesiredState = -1;
    KeAcquireQueuedLock(Controller->Lock);
    if (Controller->Tps65217 == NULL) {
        Status = STATUS_NOT_READY;
        goto SetPerformanceStateThreadEnd;
    }

    CurrentState = Controller->CurrentPerformanceState;
    DesiredState = Controller->DesiredPerformanceState;
    if (DesiredState == CurrentState) {
        Status = STATUS_SUCCESS;
        goto SetPerformanceStateThreadEnd;
    }

    Configuration = &(Am3PerformanceConfigurations[DesiredState]);
    if (Am3SocPrintPerformanceStateChanges != FALSE) {
        RtlDebugPrint("SetState %d MHz\n",
                      Am3SocPerformanceStates[DesiredState].Frequency / 1000);
    }

    //
    // If the performance is increasing, set the voltage first.
    //

    if (DesiredState > CurrentState) {
        Status = Controller->Tps65217->SetDcDcRegulator(
                                                    Controller->Tps65217,
                                                    Tps65217DcDc2,
                                                    Configuration->Millivolts);

        if (!KSUCCESS(Status)) {
            goto SetPerformanceStateThreadEnd;
        }
    }

    //
    // Set the MPU PLL to the new frequency.
    //

    Am3SocProgramMpuPll(Controller, Configuration->PllMultiplier);

    //
    // If the performance is decreasing, set the voltage now that the clock has
    // gone down.
    //

    if (DesiredState < CurrentState) {
        Status = Controller->Tps65217->SetDcDcRegulator(
                                                    Controller->Tps65217,
                                                    Tps65217DcDc2,
                                                    Configuration->Millivolts);

        if (!KSUCCESS(Status)) {

            //
            // Whoops, the voltage could not be set. Scale the frequency back
            // to what it was.
            //

            Configuration = &(Am3PerformanceConfigurations[CurrentState]);
            Am3SocProgramMpuPll(Controller, Configuration->PllMultiplier);
            goto SetPerformanceStateThreadEnd;
        }
    }

    Controller->CurrentPerformanceState = DesiredState;
    Status = STATUS_SUCCESS;

SetPerformanceStateThreadEnd:
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("AM3SOC: Could not set p-state %d: %d\n",
                      DesiredState,
                      Status);
    }

    KeReleaseQueuedLock(Controller->Lock);
    return;
}

VOID
Am3SocProgramMpuPll (
    PAM3_SOC Controller,
    ULONG Multiplier
    )

/*++

Routine Description:

    This routine initializes the MPU PLL.

Arguments:

    Controller - Supplies a pointer to the device.

    Multiplier - Supplies the multiplier value for the PLL.

Return Value:

    None.

--*/

{

    RUNLEVEL OldRunLevel;
    ULONG Value;

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);

    //
    // Put the PLL in bypass mode.
    //

    Value = AM3_READ_CM_WAKEUP(Controller, Am3CmWakeupClockModeDpllMpu) &
            ~AM335_CM_WAKEUP_CLOCK_MODE_DPLL_MPU_ENABLE;

    Value |= AM335_CM_WAKEUP_CLOCK_MODE_DPLL_MPU_ENABLE_MN_BYPASS;
    AM3_WRITE_CM_WAKEUP(Controller, Am3CmWakeupClockModeDpllMpu, Value);

    //
    // Wait for the PLL to go into bypass mode.
    //

    do {
        Value = AM3_READ_CM_WAKEUP(Controller, Am3CmWakeupIdleStateDpllMpu);

    } while ((Value & AM335_CM_WAKEUP_IDLE_STATUS_DPLL_MPU_MN_BYPASS) == 0);

    //
    // Clear the multiplier and divisor fields.
    //

    Value = AM3_READ_CM_WAKEUP(Controller, Am3CmWakeupClockSelectDpllMpu);
    Value &= ~(AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_MPU_MULT_MASK |
               AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_MPU_DIV_MASK);

    AM3_WRITE_CM_WAKEUP(Controller, Am3CmWakeupClockSelectDpllMpu, Value);

    //
    // Set the new multiplier and divisor.
    //

    Value |= (Multiplier << AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_MPU_MULT_SHIFT) |
             (AM335_MPU_PLL_N <<
              AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_MPU_DIV_SHIFT);

    AM3_WRITE_CM_WAKEUP(Controller, Am3CmWakeupClockSelectDpllMpu, Value);
    Value = AM3_READ_CM_WAKEUP(Controller, Am3CmWakeupDivM2DpllMpu);
    Value &= ~(AM335_CM_WAKEUP_DIV_M2_DPLL_MPU_CLOCK_OUT_MASK);
    Value |= AM335_MPU_PLL_M2;
    AM3_WRITE_CM_WAKEUP(Controller, Am3CmWakeupDivM2DpllMpu, Value);

    //
    // Enable and lock the PLL.
    //

    Value = AM3_READ_CM_WAKEUP(Controller, Am3CmWakeupClockModeDpllMpu);
    Value |= AM335_CM_WAKEUP_CLOCK_MODE_DPLL_MPU_ENABLE;
    AM3_WRITE_CM_WAKEUP(Controller, Am3CmWakeupClockModeDpllMpu, Value);
    do {
        Value = AM3_READ_CM_WAKEUP(Controller, Am3CmWakeupIdleStateDpllMpu);

    } while ((Value & AM335_CM_WAKEUP_IDLE_STATUS_DPLL_MPU_CLOCK) == 0);

    KeLowerRunLevel(OldRunLevel);
    return;
}

KSTATUS
Am3SocStartCortexM3 (
    PAM3_SOC Device
    )

/*++

Routine Description:

    This routine fires up the Cortex M3 processor that assists with power state
    transitions.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    PAM3_WAKE_M3_IPC_DATA IpcData;
    UINTN Size;
    ULONGLONG Timeout;
    ULONG Value;
    ULONG Version;

    ASSERT(Device->CortexM3Code != NULL);

    //
    // Copy the Cortex M3 firmware code into place, and take the Cortex M3 out
    // of reset.
    //

    Size = (UINTN)&_binary_am3cm3fw_bin_end -
           (UINTN)&_binary_am3cm3fw_bin_start;

    RtlCopyMemory(Device->CortexM3Code, &_binary_am3cm3fw_bin_start, Size);
    Device->M3State = Am3M3StateReset;
    Value = AM3_READ_PRM_WAKEUP(Device, Am3RmWakeupResetControl);
    Value &= ~AM335_RM_WAKEUP_RESET_CONTROL_RESET_CORTEX_M3;
    AM3_WRITE_PRM_WAKEUP(Device, Am3RmWakeupResetControl, Value);

    //
    // Get the firmware version to make sure the M3 is alive.
    //

    IpcData = &(Device->M3Ipc);
    IpcData->Command = Am3Cm3CommandVersion;
    IpcData->Data[0] = AM335_M3_IPC_PARAMETER_DEFAULT;
    IpcData->Data[1] = AM335_M3_IPC_PARAMETER_DEFAULT;
    IpcData->Data[2] = AM335_M3_IPC_PARAMETER_DEFAULT;
    IpcData->Data[3] = AM335_M3_IPC_PARAMETER_DEFAULT;
    Am3SocSetupIpc(Device);
    Am3MailboxSend(&(Device->Mailbox), AM335_WAKEM3_MAILBOX, -1);
    Am3MailboxFlush(&(Device->Mailbox), AM335_WAKEM3_MAILBOX);
    Timeout = HlQueryTimeCounter() + (HlQueryTimeCounterFrequency() * 5);
    do {
        Version = AM3_READ_CONTROL(Device, Am3ControlIpc2) & 0x0000FFFF;

    } while ((Version == 0xFFFF) && (HlQueryTimeCounter() <= Timeout));

    AM3_WRITE_CONTROL(Device, Am3ControlIpc1, Am3Cm3ResponseFail << 16);
    if (Version == 0xFFFF) {
        RtlDebugPrint("Am3: Failed to bring up CM3 firmware.\n");
        return STATUS_TIMEOUT;
    }

    RtlDebugPrint("Am3: CM3 Firmware version 0x%x\n", Version);
    return STATUS_SUCCESS;
}

KSTATUS
Am3SocRegisterIdleInterface (
    PAM3_SOC Device
    )

/*++

Routine Description:

    This routine registers the idle state handlers for the AM33xx SoC.

Arguments:

    Device - Supplies a pointer to the device context.

Return Value:

    Status code.

--*/

{

    UINTN CopySize;
    PPM_IDLE_STATE IdleState;
    ULONG Index;
    PPM_IDLE_STATE_INTERFACE Interface;
    UINTN Size;
    KSTATUS Status;

    Interface = &(Device->IdleInterface);
    if (Interface->Context != NULL) {
        return STATUS_SUCCESS;
    }

    //
    // Set up the OCMC RAM region.
    //

    CopySize = (UINTN)&Am3SocOcmcCodeEnd - (UINTN)&Am3SocOcmcCode;
    RtlCopyMemory(Device->Ocmc, &Am3SocOcmcCode, CopySize);

    //
    // Set up the low level suspend interface.
    //

    Device->HlSuspendInterface.Context = Device;
    Device->HlSuspendInterface.Callback = Am3SocSuspendCallback;

    //
    // Convert the microsecond values to time counter ticks.
    //

    for (Index = 0; Index < AM3_SOC_IDLE_STATE_COUNT; Index += 1) {
        IdleState = &(Am3SocIdleStates[Index]);
        IdleState->ExitLatency =
                      KeConvertMicrosecondsToTimeTicks(IdleState->ExitLatency);

        IdleState->TargetResidency =
                  KeConvertMicrosecondsToTimeTicks(IdleState->TargetResidency);
    }

    if (Am3SocIdleStateCount == 0) {
        return STATUS_SUCCESS;
    }

    Interface->Context = Device;
    Interface->Flags = 0;
    Interface->InitializeIdleStates = Am3SocInitializeIdleStates;
    Interface->EnterIdleState = Am3SocEnterIdleState;
    Size = sizeof(PM_IDLE_STATE_INTERFACE);
    Status = KeGetSetSystemInformation(SystemInformationPm,
                                       PmInformationIdleStateHandlers,
                                       Interface,
                                       &Size,
                                       TRUE);

    if (!KSUCCESS(Status)) {

        ASSERT(FALSE);

        Interface->Context = NULL;
    }

    return Status;
}

KSTATUS
Am3SocInitializeIdleStates (
    PPM_IDLE_STATE_INTERFACE Interface,
    PPM_IDLE_PROCESSOR_STATE Processor
    )

/*++

Routine Description:

    This routine sets up idle states on the current processor.

Arguments:

    Interface - Supplies a pointer to the interface.

    Processor - Supplies a pointer to the context for this processor.

Return Value:

    Status code.

--*/

{

    Processor->Context = Interface->Context;
    Processor->States = Am3SocIdleStates;
    Processor->StateCount = Am3SocIdleStateCount;
    return STATUS_SUCCESS;
}

VOID
Am3SocEnterIdleState (
    PPM_IDLE_PROCESSOR_STATE Processor,
    ULONG State
    )

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

{

    switch (State) {

    //
    // In C2, set the memory to self-refresh and then WFI.
    //

    case Am3IdleSelfRefreshWfi:
        Am3SocEnterSelfRefreshWfi(Processor->Context);
        break;

    //
    // In C3, take the core down to standby, which destroys the processor
    // state. It's basically like a suspend without any effect on peripherals.
    //

    case Am3IdleStandby:
        Am3SocEnterStandby(Processor->Context);
        break;

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

VOID
Am3SocEnterSelfRefreshWfi (
    PAM3_SOC Device
    )

/*++

Routine Description:

    This routine puts the DDR RAM into self refresh, executes a WFI, and then
    returns RAM to normal mode.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None. The net change upon returning from this function is zero.

--*/

{

    ULONG Address;

    //
    // This is done in physical mode because the EMIF controller says that a
    // DDR access is required for the self-refresh changes to take effect.
    //

    Address = Device->OcmcPhysical;
    Address += (UINTN)&Am3SocRefreshWfi - (UINTN)&Am3SocOcmcCode;
    HlDisableMmu((PHL_PHYSICAL_CALLBACK)Address, 0);
    return;
}

VOID
Am3SocEnterStandby (
    PAM3_SOC Device
    )

/*++

Routine Description:

    This routine takes the processor core down into standby.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None. The net change upon returning from this function is zero.

--*/

{

    Device->IdleState = Am3IdleStandby;
    HlSuspend(&(Device->HlSuspendInterface));
    return;
}

KSTATUS
Am3SocSuspendCallback (
    PVOID Context,
    HL_SUSPEND_PHASE Phase
    )

/*++

Routine Description:

    This routine represents a callback during low level suspend or resume.

Arguments:

    Context - Supplies the context supplied in the interface.

    Phase - Supplies the phase of suspend or resume the callback represents.

Return Value:

    Status code. On suspend, failure causes the suspend to abort. On resume,
    failure causes a crash.

--*/

{

    UINTN Address;
    PAM3_SOC Device;
    KSTATUS Status;

    Device = Context;
    switch (Phase) {
    case HlSuspendPhaseSuspendBegin:
        Status = Am3SocSuspendBegin(Device);
        break;

    case HlSuspendPhaseSuspend:
        Address = Device->OcmcPhysical;
        if (Device->IdleState == Am3IdleStandby) {
            Address += (UINTN)&Am3SocStandby - (UINTN)&Am3SocOcmcCode;

        } else {
            Address += (UINTN)&Am3SocSleep - (UINTN)&Am3SocOcmcCode;
        }

        HlDisableMmu((PHL_PHYSICAL_CALLBACK)Address,
                     Device->HlSuspendInterface.ResumeAddress);

        //
        // If execution came back, then the processor came out of WFI before
        // the Cortex M3 could take it down.
        //

        Status = STATUS_INTERRUPTED;
        break;

    case HlSuspendPhaseResume:
        Status = STATUS_SUCCESS;
        break;

    case HlSuspendPhaseResumeEnd:
        Status = Am3SocSuspendEnd(Device);
        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_SUCCESS;
        break;
    }

    return Status;
}

KSTATUS
Am3SocSuspendBegin (
    PAM3_SOC Device
    )

/*++

Routine Description:

    This routine begins the transition to a deeper idle state by requesting it
    from the Cortex M3.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    PAM3_WAKE_M3_IPC_DATA IpcData;
    AM3_IDLE_STATE State;
    KSTATUS Status;

    State = Device->IdleState;

    //
    // This routine is currently expected to be called with interrupts disabled.
    // If they're enabled, then this routine can use interrupts rather than
    // spinning.
    //

    IpcData = &(Device->M3Ipc);
    switch (State) {
    case Am3IdleSelfRefreshWfi:

        ASSERT(FALSE);

        return STATUS_NOT_SUPPORTED;

    case Am3IdleStandby:
        IpcData->Command = Am3Cm3CommandStandby;
        IpcData->ResumeAddress = (UINTN)&Am3SocResumeStandby -
                                 (UINTN)&Am3SocOcmcCode + Device->OcmcPhysical;

        break;

    default:

        ASSERT(FALSE);

        return STATUS_NOT_SUPPORTED;
    }

    //
    // Send the request IPC to the Cortex M3.
    //

    Device->M3State = Am3M3StatePowerMessage;
    Am3SocSetupIpc(Device);
    Am3MailboxSend(&(Device->Mailbox), AM335_WAKEM3_MAILBOX, -1);
    Am3MailboxFlush(&(Device->Mailbox), AM335_WAKEM3_MAILBOX);
    Status = Am3SocWaitForIpcResult(Device);
    if (Status != STATUS_MORE_PROCESSING_REQUIRED) {
        RtlDebugPrint("Am3: Failed to request power transition: %d\n", Status);

        ASSERT(IpcData->Command == Am3Cm3CommandStandby);

        Am3SocResetM3(Device);
        Status = STATUS_NOT_READY;

    } else {
        Status = STATUS_SUCCESS;
    }

    return Status;
}

KSTATUS
Am3SocSuspendEnd (
    PAM3_SOC Device
    )

/*++

Routine Description:

    This routine ends a transition from a deep sleep state.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    ULONG Result;
    KSTATUS Status;

    //
    // See if a reset is needed.
    //

    Result = AM3_READ_CONTROL(Device, Am3ControlIpc1) >> 16;
    if (Result == Am3Cm3ResponsePass) {
        Status = STATUS_SUCCESS;

    } else {
        Status = Am3SocResetM3(Device);
    }

    //
    // Write a failure result into the register.
    //

    Result = Am3Cm3ResponseFail << 16;
    AM3_WRITE_CONTROL(Device, Am3ControlIpc1, Result);
    return Status;
}

KSTATUS
Am3SocResetM3 (
    PAM3_SOC Device
    )

/*++

Routine Description:

    This routine sends a reset command to the Cortex M3.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    PAM3_WAKE_M3_IPC_DATA IpcData;
    KSTATUS Status;

    IpcData = &(Device->M3Ipc);
    IpcData->Command = Am3Cm3CommandResetStateMachine;
    Device->M3State = Am3M3StateResetMessage;

    //
    // Normally the message needs to be sent and immediately revoked because
    // wait for IPC result will change it to an invalid message, and if more
    // interrupts come in the M3 will suck that into its current message
    // variable. For a power transition, this would be deadly, since the M3
    // looks at that message ID again after the A8 WFIs. Here however the A8
    // might be racing with the M3 if an interrupt came in after the A8 WFI but
    // before the M3 could take it down. If that's the case the M3's mailbox
    // interrupt might be disabled, so pulsing the interrupt might cause it
    // to be missed. Keep it interrupting until the M3 sees it.
    //

    Am3SocSetupIpc(Device);
    Am3MailboxSend(&(Device->Mailbox), AM335_WAKEM3_MAILBOX, -1);
    Status = Am3SocWaitForIpcResult(Device);
    Am3MailboxFlush(&(Device->Mailbox), AM335_WAKEM3_MAILBOX);
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Cortex M3 reset failure: %d\n", Status);
    }

    Device->M3State = Am3M3StateReset;
    return Status;
}

VOID
Am3SocSetupIpc (
    PAM3_SOC Device
    )

/*++

Routine Description:

    This routine writes the IPC registers in preparation for sending a command
    to the Cortex M3.

Arguments:

    Device - Supplies a pointer to the device. The IPC command will be read
        from here.

Return Value:

    None.

--*/

{

    ULONG Command;

    Command = Device->M3Ipc.Command | (0xFFFF << 16);
    AM3_WRITE_CONTROL(Device, Am3ControlIpc0, Device->M3Ipc.ResumeAddress);
    AM3_WRITE_CONTROL(Device, Am3ControlIpc1, Command);
    AM3_WRITE_CONTROL(Device, Am3ControlIpc2, Device->M3Ipc.Data[0]);
    AM3_WRITE_CONTROL(Device, Am3ControlIpc3, Device->M3Ipc.Data[1]);
    AM3_WRITE_CONTROL(Device, Am3ControlIpc4, Device->M3Ipc.Data[2]);
    AM3_WRITE_CONTROL(Device, Am3ControlIpc5, Device->M3Ipc.Data[3]);
    return;
}

KSTATUS
Am3SocWaitForIpcResult (
    PAM3_SOC Device
    )

/*++

Routine Description:

    This routine spins waiting for the Cortex M3 IPC result to come back.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    STATUS_SUCCESS if the command completed successfully.

    STATUS_MORE_PROCESSING_REQUIRED if the command completed with the WAIT4OK
    status (successful).

    STATUS_UNSUCCESSFUL if the command failed.

--*/

{

    ULONG Result;
    ULONG SpinCount;
    ULONG Value;

    //
    // Wait for the command to clear. Normally spin counts are a terrible way
    // to timeout, but in this case this is it really shouldn't take long
    // for the M3 to respond, and this is really just a failsafe to keep the
    // machine from hanging entirely.
    //

    SpinCount = 0;
    do {
        Value = AM3_READ_CONTROL(Device, Am3ControlIpc1);
        SpinCount += 1;

    } while (((Value & 0xFFFF0000) == 0xFFFF0000) &&
             (SpinCount < AM335_IPC_MAX_SPIN_COUNT));

    if ((Value & 0x0000FFFF) != Device->M3Ipc.Command) {
        RtlDebugPrint("Am3: Got response 0x%x for other command 0x%x\n",
                      Value,
                      Device->M3Ipc.Command);
    }

    if (SpinCount >= AM335_IPC_MAX_SPIN_COUNT) {
        RtlDebugPrint("Am3: CM3 hung.\n");

        ASSERT(FALSE);
    }

    //
    // Write a bogus value into the command to prevent bugs involving rerunning
    // a previous or invalid command.
    //

    AM3_WRITE_CONTROL(Device, Am3ControlIpc1, Am3Cm3ResponseFail << 16);
    Result = Value >> 16;
    switch (Result) {
    case Am3Cm3ResponsePass:
        return STATUS_SUCCESS;

    case Am3Cm3ResponseFail:
        break;

    case Am3Cm3ResponseWait4Ok:
        return STATUS_MORE_PROCESSING_REQUIRED;

    default:

        ASSERT(FALSE);

        break;
    }

    return STATUS_UNSUCCESSFUL;
}

