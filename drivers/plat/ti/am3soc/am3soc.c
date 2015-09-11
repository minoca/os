/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

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

#include <minoca/driver.h>
#include <minoca/acpitabs.h>
#include <minoca/dev/am335x.h>
#include <minoca/intrface/tps65217.h>

//
// --------------------------------------------------------------------- Macros
//

#define AM3_READ_CM_WAKEUP(_Controller, _Register) \
    HlReadRegister32((_Controller)->Prcm + AM335_CM_WAKEUP_OFFSET + (_Register))

#define AM3_WRITE_CM_WAKEUP(_Controller, _Register, _Value)                    \
    HlWriteRegister32(                                                         \
                   (_Controller)->Prcm + AM335_CM_WAKEUP_OFFSET + (_Register), \
                   (_Value))

#define AM3_READ_PRCM(_Controller, _Register) \
    HlReadRegister32((_Controller)->Prcm + (_Register))

#define AM3_WRITE_PRCM(_Controller, _Register, _Value) \
    HlWriteRegister32((_Controller)->Prcm + (_Register), (_Value))

#define AM3_READ_CONTROL(_Controller, _Register) \
    HlReadRegister32((_Controller)->SocControl + (_Register))

#define AM3_WRITE_CONTROL(_Controller, _Register, _Value) \
    HlWriteRegister32((_Controller)->SocControl + (_Register), (_Value))

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

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _AM335_CONTROL_REGISTER {
    Am3ControlDeviceId = 0x600,
    Am3ControlEfuseSma = 0x7FC,
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

/*++

Structure Description:

    This structure defines the context for an AM335x generic SoC controller.

Members:

    OsDevice - Stores a pointer to the OS device object.

    Prcm - Stores the virtual address of the memory mapping to the
        PRCM region.

    SocControl - Stores the virtual address of the memory mapping to the SOC
        Control region.

    Lock - Stores a pointer to a lock serializing access to the controller.

    Tps65217 - Stores a pointer to the TPS65217 interface.

    Tps65217SignedUp - Stores a boolean indicating whether or not TPS65217
        interface notifications have been registered.

    CurrentPerformanceState - Stores the current performance state index.

    DesiredPerformanceState = Stores the desired performance state index.

    SocRevision - Stores the SOC revision number.

--*/

typedef struct _AM3_SOC {
    PDEVICE OsDevice;
    PVOID Prcm;
    PVOID SocControl;
    PQUEUED_LOCK Lock;
    PINTERFACE_TPS65217 Tps65217;
    BOOL Tps65217SignedUp;
    ULONG CurrentPerformanceState;
    ULONG DesiredPerformanceState;
    ULONG SocRevision;
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
    PSTR DeviceId,
    PSTR ClassId,
    PSTR CompatibleIds,
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
// ------------------------------------------------------------------ Functions
//

KSTATUS
DriverEntry (
    PDRIVER Driver
    )

/*++

Routine Description:

    This routine is the entry point for the null driver. It registers its other
    dispatch functions, and performs driver-wide initialization.

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
    PSTR DeviceId,
    PSTR ClassId,
    PSTR CompatibleIds,
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
    PHYSICAL_ADDRESS EndAddress;
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    PRESOURCE_ALLOCATION Prcm;
    ULONG Size;
    PRESOURCE_ALLOCATION SocControl;
    KSTATUS Status;

    Prcm = NULL;
    SocControl = NULL;

    //
    // Loop through the allocated resources to get the controller base.
    //

    AllocationList = Irp->U.StartDevice.ProcessorLocalResources;
    Allocation = IoGetNextResourceAllocation(AllocationList, NULL);
    while (Allocation != NULL) {

        //
        // There should be two physical address allocations: one for the PRCM,
        // and another for the SoC control region.
        //

        if (Allocation->Type == ResourceTypePhysicalAddressSpace) {
            if (Prcm == NULL) {
                Prcm = Allocation;

            } else if (SocControl == NULL) {
                SocControl = Allocation;
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
        (SocControl->Length < AM335_SOC_CONTROL_SIZE)) {

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
        RtlDebugPrint("AM3SOC: Could not set p-state %d: %x\n",
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

    ULONG Value;

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

    return;
}

