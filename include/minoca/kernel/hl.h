/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    hl.h

Abstract:

    This header contains definitions for the kernel's Hardware Layer.

Author:

    Evan Green 5-Aug-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/regacces.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro converts a Binary Coded Decimal value into a binary.
//

#define BCD_TO_BINARY(_BcdValue) \
    ((((_BcdValue) >> 4) * 10) + ((_BcdValue) & 0x0F))

#define BINARY_TO_BCD(_BinaryValue) \
    ((((_BinaryValue) / 10) << 4) | ((_BinaryValue) % 10))

//
// ---------------------------------------------------------------- Definitions
//

#define HL_POOL_TAG 0x64726148 // 'draH'

#define HL_CRASH_PROCESSOR_INDEXING_ERROR         0x00000001
#define HL_CRASH_SET_PROCESSOR_ADDRESSING_FAILURE 0x00000002
#define HL_CRASH_NO_IO_PORTS                      0x00000003
#define HL_CRASH_CLOCK_WONT_START                 0x00000004
#define HL_CRASH_PROCESSOR_WONT_START             0x00000005
#define HL_CRASH_INVALID_INTERRUPT_DISCONNECT     0x00000006
#define HL_CRASH_PROCESSOR_HUNG                   0x00000007
#define HL_CRASH_RESUME_FAILURE                   0x00000008

//
// Define the default system clock rate at system boot, in 100ns units.
//

#define DEFAULT_CLOCK_RATE 156250

//
// Define low level suspend flags.
//

//
// This bit is set when the interrupt controller state needs to be saved.
//

#define HL_SUSPEND_RESTORE_INTERRUPTS 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _IPI_TYPE {
    IpiTypeInvalid,
    IpiTypePacket,
    IpiTypeTlbFlush,
    IpiTypeNmi,
    IpiTypeProfiler,
    IpiTypeClock
} IPI_TYPE, *PIPI_TYPE;

typedef enum _INTERRUPT_STATUS {
    InterruptStatusNotClaimed,
    InterruptStatusClaimed,
    InterruptStatusDefer
} INTERRUPT_STATUS, *PINTERRUPT_STATUS;

typedef enum _INTERRUPT_MODEL {
    InterruptModelInvalid,
    InterruptModelPic,
    InterruptModelApic,
} INTERRUPT_MODEL, *PINTERRUPT_MODEL;

typedef enum _HL_INFORMATION_TYPE {
    HlInformationInvalid,
    HlInformationEfiVariable,
} HL_INFORMATION_TYPE, *PHL_INFORMATION_TYPE;

typedef
INTERRUPT_STATUS
(*PINTERRUPT_SERVICE_ROUTINE) (
    PVOID Context
    );

/*++

Routine Description:

    This routine represents an interrupt service routine.

Arguments:

    Context - Supplies the context supplied when this interrupt was initially
        connected.

Return Value:

    Returns an interrupt status indicating if this ISR is claiming the
    interrupt, not claiming the interrupt, or needs the interrupt to be
    masked temporarily.

--*/

typedef
VOID
(*PPROCESSOR_START_ROUTINE) (
    PPROCESSOR_START_BLOCK StartBlock
    );

/*++

Routine Description:

    This routine represents a prototype for a processor initialization routine.

Arguments:

    StartBlock - Supplies a pointer to the processor start block.

Return Value:

    None. This routine does not return.

--*/

/*++

Structure Description:

    This structure defines an interrupt.

Members:

    Vector - Stores the vector the interrupt is connected to.

    Line - Stores the interrupt line this interrupt is connected to.

    Mode - Stores the mode of the interrupt (edge or level).

    InterruptServiceRoutine - Stores a pointer to the service routine to be
        called at interrupt runlevel.

    Context - Stores the context to be passed in when this ISR is executed.

    InterruptCount - Stores the number of interrupts received. This variable is
        not synchronized, so the count may not be exact.

    LastTimestamp - Stores the time counter value the last time this interrupt
        was sampled. This is used for interrupt storm detection.

    DispatchServiceRoutine - Stores an optiontl pointer to the function to
        call at dispatch level to service the interrupt.

    LowLevelServiceRoutine - Stores an optional pointer to the function to
        call at low run level to service the interrupt.

    Dpc - Stores a pointer to the DPC that is queued for this interrupt.

    WorkItem - Stores a pointer to the work item that is queued for this
        interrupt.

    QueueStatus - Stores various queue flags. See INTERRUPT_QUEUE_* definitions.

    Controller - Stores a pointer to the interrupt controller for this
        interrupt.

--*/

typedef struct _KINTERRUPT KINTERRUPT, *PKINTERRUPT;
struct _KINTERRUPT {
    PKINTERRUPT NextInterrupt;
    INTERRUPT_LINE Line;
    INTERRUPT_MODE Mode;
    ULONG Vector;
    RUNLEVEL RunLevel;
    PINTERRUPT_SERVICE_ROUTINE InterruptServiceRoutine;
    PVOID Context;
    UINTN InterruptCount;
    ULONGLONG LastTimestamp;
    PINTERRUPT_SERVICE_ROUTINE DispatchServiceRoutine;
    PINTERRUPT_SERVICE_ROUTINE LowLevelServiceRoutine;
    PDPC Dpc;
    PWORK_ITEM WorkItem;
    volatile ULONG QueueFlags;
    PINTERRUPT_CONTROLLER Controller;
};

/*++

Structure Description:

    This structure defines a range of physical address space in use by the
    hardware layer.

Members:

    ListEntry - Stores pointers to the next and previous physical address usage
        structures in the list.

    PhysicalAddress - Stores the first physical address in the segment of
        physical memory space occupied by this allocation.

    Size - Stores the size of the range, in bytes.

--*/

typedef struct _HL_PHYSICAL_ADDRESS_USAGE {
    LIST_ENTRY ListEntry;
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONGLONG Size;
} HL_PHYSICAL_ADDRESS_USAGE, *PHL_PHYSICAL_ADDRESS_USAGE;

/*++

Structure Description:

    This structure defines EFI variable information.

Members:

    VariableNameSize - Supplies the size of the variable name buffer in bytes.
        The buffer immediately follows this structure. Remember that UEFI
        strings have characters that are two-bytes wide.

    VendorGuid - Supplies the vendor GUID (byte for byte coped to an EFI_GUID).

    Attributes - Supplies either the attributes to set or the attributes
        returned.

    DataSize - Supplies the size of the data buffer in bytes. The data
        immediately follows the variable name.

--*/

typedef struct _HL_EFI_VARIABLE_INFORMATION {
    UINTN VariableNameSize;
    UUID VendorGuid;
    ULONG Attributes;
    UINTN DataSize;
} HL_EFI_VARIABLE_INFORMATION, *PHL_EFI_VARIABLE_INFORMATION;

/*++

Structure Description:

    This structure defines information about the processor counter.

Members:

    Frequency - Stores the frequency of the processor counter in Hertz. This
        is usually the maximum sustainable frequency, which is also the
        frequency at which the system was booted.

    Multiplier - Stores the multiplier to translate between this timer's speed
        and the actual processor execution speed.

    Features - Stores a bitfield of timer features. See TIMER_FEATURE_*
        definitions.

--*/

typedef struct _HL_PROCESSOR_COUNTER_INFORMATION {
    ULONGLONG Frequency;
    ULONG Multiplier;
    ULONG Features;
} HL_PROCESSOR_COUNTER_INFORMATION, *PHL_PROCESSOR_COUNTER_INFORMATION;

//
// SuspendBegin is called after all devices have been suspended, but before
// internal hardware layer context has been saved.
//
// Suspend is called after all internal context has been saved, and should
// actually take the CPU or platform down.
//
// Resume is called immediately after the machine context is restored, but
// before all the internal hardware layer state has been restored.
//
// ResumeEnd is called after the internal hardware layer state has been
// restored, but before devices have been resumed.
//
// Complete is not a phase under which the callback is called, but is the
// ending state indicating the transition went through successfully.
//

typedef enum _HL_SUSPEND_PHASE {
    HlSuspendPhaseInvalid,
    HlSuspendPhaseSuspendBegin = 0x100,
    HlSuspendPhaseSuspend = 0x200,
    HlSuspendPhaseResume = 0x300,
    HlSuspendPhaseResumeEnd = 0x400,
    HlSuspendPhaseComplete = 0x1000
} HL_SUSPEND_PHASE, *PHL_SUSPEND_PHASE;

typedef
KSTATUS
(*PHL_SUSPEND_CALLBACK) (
    PVOID Context,
    HL_SUSPEND_PHASE Phase
    );

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

typedef
UINTN
(*PHL_PHYSICAL_CALLBACK) (
    UINTN Argument
    );

/*++

Routine Description:

    This routine represents a callback with the MMU disabled. No services
    except a small stack are available during this callback.

Arguments:

    Argument - Supplies an argument to pass to the function.

Return Value:

    Returns an unspecified value significant to the caller.

--*/

/*++

Structure Description:

    This structure defines the interface used when going down for a low level
    suspend where the processor context will be lost. This interface should
    only be used/called by low-level platform drivers that implement suspend.

Members:

    Flags - Stores a bitfield of flags governing how the processor is taken
        down and brought back up. See HL_SUSPEND_* definitions.

    Context - Stores a context pointer passed to the callback routines.

    Callback - Stores a pointer to a function that is called for each phase of
        suspend and resume.

    Phase - Stores the phase at which the suspend or resume operation failed,
        if it did.

    ResumeAddress - Stores the physical address this processor should resume to.
        This will be filled out by the system by the time the suspend phase
        is called.

--*/

typedef struct _HL_SUSPEND_INTERFACE {
    ULONG Flags;
    PVOID Context;
    PHL_SUSPEND_CALLBACK Callback;
    HL_SUSPEND_PHASE Phase;
    PHYSICAL_ADDRESS ResumeAddress;
} HL_SUSPEND_INTERFACE, *PHL_SUSPEND_INTERFACE;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KERNEL_API
ULONGLONG
HlQueryTimeCounter (
    VOID
    );

/*++

Routine Description:

    This routine queries the time counter hardware and returns a 64-bit
    monotonically non-decreasing value that represents the number of timer ticks
    since the system was started. This value will continue to count through all
    idle and sleep states.

    This routine can be called at any runlevel.

Arguments:

    None.

Return Value:

    Returns the number of timer ticks that have elapsed since the system was
    booted. The absolute time between successive ticks can be retrieved from the
    Query Time Counter Frequency function.

--*/

KERNEL_API
ULONGLONG
HlQueryProcessorCounter (
    VOID
    );

/*++

Routine Description:

    This routine queries the processor counter hardware and returns a 64-bit
    monotonically non-decreasing value that correlates to "processor" time.
    This does not necessarily correspond to wall-clock time, as the frequency
    of this counter may vary over time. This counter may also vary across
    processors, so this routine must be called at dispatch level or higher.
    Failing to call this routine at or above dispatch level may cause the
    counter's internal accounting to malfunction.

    This routine is intended primarily for the scheduler to track processor
    cycles. Users looking to measure units of time should query the time
    counter.

Arguments:

    None.

Return Value:

    Returns a 64-bit non-decreasing value corresponding to "processor" time.

--*/

KERNEL_API
ULONGLONG
HlQueryProcessorCounterFrequency (
    VOID
    );

/*++

Routine Description:

    This routine returns the frequency of the processor counter. This frequency
    will never change after it is set on boot.

    This routine can be called at any runlevel.

Arguments:

    None.

Return Value:

    Returns the frequency of the time counter, in Hertz.

--*/

KERNEL_API
ULONGLONG
HlQueryTimeCounterFrequency (
    VOID
    );

/*++

Routine Description:

    This routine returns the frequency of the time counter. This frequency will
    never change after it is set on boot.

    This routine can be called at any runlevel.

Arguments:

    None.

Return Value:

    Returns the frequency of the time counter, in Hertz.

--*/

KERNEL_API
VOID
HlBusySpin (
    ULONG Microseconds
    );

/*++

Routine Description:

    This routine spins for at least the given number of microseconds by
    repeatedly reading a hardware timer. This routine should be avoided if at
    all possible, as it simply burns CPU cycles.

    This routine can be called at any runlevel.

Arguments:

    Microseconds - Supplies the number of microseconds to spin for.

Return Value:

    Returns the frequency of the time counter, in Hertz.

--*/

KERNEL_API
KSTATUS
HlUpdateCalendarTime (
    VOID
    );

/*++

Routine Description:

    This routine updates the first available hardware calendar time with a snap
    of the current system time.

Arguments:

    None.

Return Value:

    Status code.

--*/

KERNEL_API
INTERRUPT_MODEL
HlGetInterruptModel (
    VOID
    );

/*++

Routine Description:

    This routine returns the general system interrupt model currently in use.
    This routine is only useful to firmware or interrupt configuration parties.

Arguments:

    None.

Return Value:

    Returns the interrupt model in use.

--*/

KERNEL_API
KSTATUS
HlCreateInterruptController (
    ULONG ParentGsi,
    ULONG ParentVector,
    ULONG LineCount,
    PINTERRUPT_CONTROLLER_DESCRIPTION Registration,
    PINTERRUPT_CONTROLLER_INFORMATION ResultingInformation
    );

/*++

Routine Description:

    This routine creates an interrupt controller outside of the normal hardware
    module context. It is used primarily by GPIO controllers that function as a
    kind of secondary interrupt controller.

Arguments:

    ParentGsi - Supplies the global system interrupt number of the interrupt
        controller line this controller wires up to.

    ParentVector - Supplies the vector of the interrupt that this interrupt
        controller wires up to.

    LineCount - Supplies the number of lines this interrupt controller contains.

    Registration - Supplies a pointer to the interrupt controller information,
        filled out correctly by the caller.

    ResultingInformation - Supplies a pointer where the interrupt controller
        handle and other information will be returned.

Return Value:

    Status code.

--*/

KERNEL_API
VOID
HlDestroyInterruptController (
    PINTERRUPT_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine destroys an interrupt controller, taking it offline and
    releasing all resources associated with it.

Arguments:

    Controller - Supplies a pointer to the controller to destroy.

Return Value:

    None.

--*/

KERNEL_API
KSTATUS
HlGetInterruptControllerInformation (
    UINTN Identifier,
    PINTERRUPT_CONTROLLER_INFORMATION Information
    );

/*++

Routine Description:

    This routine returns information about an interrupt controller with a
    specific ID.

Arguments:

    Identifier - Supplies the identifier of the interrupt controller.

    Information - Supplies a pointer where the interrupt controller information
        will be returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_FOUND if no interrupt controller matching the given identifier
    exists in the system.

--*/

KERNEL_API
VOID
HlContinueInterrupt (
    HANDLE InterruptHandle,
    INTERRUPT_STATUS Status
    );

/*++

Routine Description:

    This routine continues an interrupt that was previously deferred at low
    level.

Arguments:

    InterruptHandle - Supplies the connected interrupt handle.

    Status - Supplies the final interrupt status that would have been returned
        had the interrupt not been deferred. This must either be claimed or
        not claimed.

Return Value:

    None.

--*/

KERNEL_API
INTERRUPT_STATUS
HlSecondaryInterruptControllerService (
    PVOID Context
    );

/*++

Routine Description:

    This routine implements a standard interrupt service routine for an
    interrupt that is wired to another interrupt controller. It will call out
    to determine what fired, and begin a new secondary interrupt.

Arguments:

    Context - Supplies the context, which must be a pointer to the secondary
        interrupt controller that needs service.

Return Value:

    Returns an interrupt status indicating if this ISR is claiming the
    interrupt, not claiming the interrupt, or needs the interrupt to be
    masked temporarily.

--*/

KERNEL_API
KSTATUS
HlGetMsiInformation (
    ULONGLONG Vector,
    ULONGLONG VectorCount,
    PPROCESSOR_SET Processors,
    PMSI_INFORMATION Information
    );

/*++

Routine Description:

    This routine gathers the appropriate MSI/MSI-X address and data information
    for the given set of contiguous interrupt vectors.

Arguments:

    Vector - Supplies the first vector for which information is being requested.

    VectorCount - Supplies the number of contiguous vectors for which
        information is being requested.

    Processors - Supplies the set of processors that the MSIs should utilize.

    Information - Supplies a pointer to an array of MSI/MSI-X information to
        be filled in by the routine.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
HlGetProcessorIndexFromId (
    ULONGLONG PhysicalId,
    PULONG ProcessorIndex,
    PBOOL Active
    );

/*++

Routine Description:

    This routine attempts to find the logical processor index of the processor
    with the given physical identifier.

Arguments:

    PhysicalId - Supplies the processor physical identifier.

    ProcessorIndex - Supplies a pointer where the processor index will be
        returned on success.

    Active - Supplies a pointer where a boolean will be returned indicating if
        this processor is present and active within the system.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
HlSuspend (
    PHL_SUSPEND_INTERFACE Interface
    );

/*++

Routine Description:

    This routine implements the low level primitive to suspend the processor
    and/or platform. This routine does not deal with device states at all, it
    simply takes the CPU/platform down.

Arguments:

    Interface - Supplies a pointer to the suspend interface to use going down.

Return Value:

    Status code. A failing status code indicates that the suspend did not
    occur.

--*/

KERNEL_API
UINTN
HlDisableMmu (
    PHL_PHYSICAL_CALLBACK PhysicalFunction,
    UINTN Argument
    );

/*++

Routine Description:

    This routine temporarily disables the MMU and calls then given callback
    function.

Arguments:

    PhysicalFunction - Supplies the physical address of a function to call
        with the MMU disabled. Interrupts will also be disabled during this
        call.

    Argument - Supplies an argument to pass to the function.

Return Value:

    Returns the value returned by the callback function.

--*/

KERNEL_API
KSTATUS
HlRegisterHardware (
    HARDWARE_MODULE_TYPE Type,
    PVOID Description
    );

/*++

Routine Description:

    This routine registers a hardware module with the system.

Arguments:

    Type - Supplies the type of resource being registered.

    Description - Supplies a description of the resource being registered.

Return Value:

    Returns a pointer to the allocation of the requested size on success.

    NULL on failure.

--*/

KSTATUS
HlStartAllProcessors (
    PPROCESSOR_START_ROUTINE StartRoutine,
    PULONG ProcessorsStarted
    );

/*++

Routine Description:

    This routine is called on the BSP, and starts all APs.

Arguments:

    StartRoutine - Supplies the routine the processors should jump to.

    ProcessorsStarted - Supplies a pointer where the number of processors
        started will be returned (the total number of processors in the system,
        including the boot processor).

Return Value:

    Status code.

--*/

KSTATUS
HlSendIpi (
    IPI_TYPE IpiType,
    PPROCESSOR_SET Processors
    );

/*++

Routine Description:

    This routine sends an Inter-Processor Interrupt (IPI) to the given set of
    processors.

Arguments:

    IpiType - Supplies the type of IPI to deliver.

    Processors - Supplies the set of processors to deliver the IPI to.

Return Value:

    Status code.

--*/

ULONG
HlGetMaximumProcessorCount (
    VOID
    );

/*++

Routine Description:

    This routine returns the maximum number of logical processors that this
    machine supports.

Arguments:

    None.

Return Value:

    Returns the maximum number of logical processors that may exist in the
    system.

--*/

VOID
HlDispatchInterrupt (
    ULONG Vector,
    PTRAP_FRAME TrapFrame
    );

/*++

Routine Description:

    This routine determines the source of an interrupt and runs the ISR.

Arguments:

    Vector - Supplies the vector this interrupt came in on.

    TrapFrame - Supplies a pointer to the machine state immediately before the
        interrupt.

Return Value:

    None.

--*/

RUNLEVEL
HlRaiseRunLevel (
    RUNLEVEL RunLevel
    );

/*++

Routine Description:

    This routine raises the interrupt run level of the system.

Arguments:

    RunLevel - Supplies the run level to raise to. This must be greater than
        or equal to the current runlevel.

Return Value:

    Returns a pointer to the old run level.

--*/

VOID
HlLowerRunLevel (
    RUNLEVEL RunLevel
    );

/*++

Routine Description:

    This routine lowers the interrupt run level of the system.

Arguments:

    RunLevel - Supplies the run level to lower to. This must be less than
        or equal to the current runlevel.

Return Value:

    Returns a pointer to the old run level.

--*/

PKINTERRUPT
HlCreateInterrupt (
    ULONG Vector,
    PINTERRUPT_SERVICE_ROUTINE InterruptServiceRoutine,
    PINTERRUPT_SERVICE_ROUTINE DispatchServiceRoutine,
    PINTERRUPT_SERVICE_ROUTINE LowLevelServiceRoutine,
    PVOID Context
    );

/*++

Routine Description:

    This routine creates and initialize a new KINTERRUPT structure.

Arguments:

    Vector - Supplies the vector that the interrupt will come in on.

    InterruptServiceRoutine - Supplies a pointer to the function to call at
        interrupt runlevel when this interrupt comes in.

    DispatchServiceRoutine - Supplies a pointer to the function to call at
        dispatch level when this interrupt comes in.

    LowLevelServiceRoutine - Supplies a pointer to the function to call at
        low runlevel when this interrupt comes in.

    Context - Supplies a pointer's worth of data that will be passed in to the
        service routine when it is called.

Return Value:

    Returns a pointer to the newly created interrupt on success. The interrupt
    is not connected at this point.

    NULL on failure.

--*/

VOID
HlDestroyInterrupt (
    PKINTERRUPT Interrupt
    );

/*++

Routine Description:

    This routine destroys a KINTERRUPT structure.

Arguments:

    Interrupt - Supplies a pointer to the interrupt to destroy.

Return Value:

    None.

--*/

KSTATUS
HlConnectInterrupt (
    PKINTERRUPT Interrupt
    );

/*++

Routine Description:

    This routine commits an interrupt service routine to active duty. When this
    call is completed, it will be called for interrupts coming in on the
    specified vector.

Arguments:

    Interrupt - Supplies a pointer to the initialized interrupt.

Return Value:

    Status code.

--*/

VOID
HlDisconnectInterrupt (
    PKINTERRUPT Interrupt
    );

/*++

Routine Description:

    This routine removes an interrupt service routine from active duty. When
    this call is completed, no new interrupts will come in for this device and
    vector.

Arguments:

    Interrupt - Supplies a pointer to the initialized interrupt.

Return Value:

    None.

--*/

KSTATUS
HlEnableInterruptLine (
    ULONGLONG GlobalSystemInterruptNumber,
    PINTERRUPT_LINE_STATE LineState,
    PKINTERRUPT Interrupt,
    PVOID ResourceData,
    UINTN ResourceDataSize
    );

/*++

Routine Description:

    This routine enables the given interrupt line.

Arguments:

    GlobalSystemInterruptNumber - Supplies the global system interrupt number
        to enable.

    LineState - Supplies a pointer to the desired line state. Only the mode,
        polarity and flags are required by this routine.

    Interrupt - Supplies a pointer to the interrupt structure this line will
        be connected to.

    ResourceData - Supplies an optional pointer to the device specific resource
        data for the interrupt line.

    ResourceDataSize - Supplies the size of the resource data, in bytes.

Return Value:

    Status code.

--*/

VOID
HlDisableInterruptLine (
    PKINTERRUPT Interrupt
    );

/*++

Routine Description:

    This routine disables the given interrupt line. Note that if the line is
    being shared by multiple interrupts, it may stay open for the other
    devices connected to it.

Arguments:

    Interrupt - Supplies a pointer to the interrupt line to disconnect.

Return Value:

    None.

--*/

KSTATUS
HlStartProfilerTimer (
    VOID
    );

/*++

Routine Description:

    This routine activates the profiler by arming the profiler timer.

Arguments:

    None.

Return Value:

    Status code.

--*/

VOID
HlStopProfilerTimer (
    VOID
    );

/*++

Routine Description:

    This routine stops the profiler by disarming the profiler timer.

Arguments:

    None.

Return Value:

    None.

--*/

KSTATUS
HlQueryCalendarTime (
    PSYSTEM_TIME SystemTime,
    PULONGLONG TimeCounter
    );

/*++

Routine Description:

    This routine returns the current calendar time as reported by the hardware
    calendar time source.

Arguments:

    SystemTime - Supplies a pointer where the system time as read from the
        hardware will be returned.

    TimeCounter - Supplies a pointer where a time counter value corresponding
        with the approximate moment the calendar time was read will be
        returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_SUCH_DEVICE if there are no registered calendar timer modules.

    Other errors on calendar timer hardware failure.

--*/

VOID
HlSetClockTimer (
    CLOCK_TIMER_MODE Mode,
    ULONGLONG DueTime,
    BOOL Hard
    );

/*++

Routine Description:

    This routine arms or disarms the main clock timer. This routine must be
    called at or above clock level, or with interrupts disabled.

Arguments:

    Mode - Supplies the mode to arm the timer in.

    DueTime - Supplies the due time in time counter ticks (absolute) to arm the
        timer in. This is only used in one-shot mode.

    Hard - Supplies a boolean indicating if this is a hard or soft deadline.
        This is used only for one-shot mode.

Return Value:

    None.

--*/

KSTATUS
HlGetProcessorCounterInformation (
    PHL_PROCESSOR_COUNTER_INFORMATION Information
    );

/*++

Routine Description:

    This routine returns information about the cycle counter built into the
    processor.

Arguments:

    Information - Supplies a pointer where the processor counter information
        will be returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_SUPPORTED if the processor does not have a processor cycle
    counter.

--*/

VOID
HlFlushCache (
    ULONG Flags
    );

/*++

Routine Description:

    This routine flushes the cache for every registered cache controller.

Arguments:

    Flags - Supplies a bitmask of cache flush flags. See HL_CACHE_FLAG_* for
        definitions.

Return Value:

    None.

--*/

VOID
HlFlushCacheRegion (
    PHYSICAL_ADDRESS Address,
    UINTN SizeInBytes,
    ULONG Flags
    );

/*++

Routine Description:

    This routine flushes the given cache region for every registered cache
    controller.

Arguments:

    Address - Supplies the starting physical address of the region to flush. It
        must be aligned to the cache line size.

    SizeInBytes - Supplies the number of bytes to flush.

    Flags - Supplies a bitmask of cache flush flags. See HL_CACHE_FLAG_* for
        definitions.

Return Value:

    None.

--*/

ULONG
HlGetDataCacheLineSize (
    VOID
    );

/*++

Routine Description:

    This routine returns the maximum data cache line size out of all registered
    cache controllers.

Arguments:

    None.

Return Value:

    Returns the maximum data cache line size out of all registered cache
    controllers in bytes.

--*/

KSTATUS
HlResetSystem (
    SYSTEM_RESET_TYPE ResetType,
    PVOID Data,
    UINTN Size
    );

/*++

Routine Description:

    This routine resets the system.

Arguments:

    ResetType - Supplies the desired reset type. If the desired reset type is
        not supported, a cold reset will be attempted.

    Data - Supplies a pointer to platform-specific reboot data.

    Size - Supplies the size of the platform-specific data in bytes.

Return Value:

    Does not return on success, the system is reset.

    STATUS_INVALID_PARAMETER if an invalid reset type was supplied.

    STATUS_NO_INTERFACE if there are no appropriate reboot capababilities
    registered with the system.

    Other status codes on other failures.

--*/

KSTATUS
HlGetSetSystemInformation (
    BOOL FromKernelMode,
    HL_INFORMATION_TYPE InformationType,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

/*++

Routine Description:

    This routine gets or sets system information.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not this request
        (and the buffer associated with it) originates from user mode (FALSE)
        or kernel mode (TRUE).

    InformationType - Supplies the information type.

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    Status code.

--*/

