/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

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

#include <minoca/regacces.h>
#include <minoca/hmod.h>

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

#define NANOSECONDS_PER_SECOND 1000000000ULL
#define MICROSECONDS_PER_SECOND 1000000ULL
#define MILLISECONDS_PER_SECOND 1000ULL
#define MICROSECONDS_PER_MILLISECOND 1000ULL
#define NANOSECONDS_PER_MICROSECOND 1000ULL

#define HL_CACHE_FLAG_CLEAN 0x00000001
#define HL_CACHE_FLAG_INVALIDATE 0x00000002

//
// Define the default system clock rate at system boot, in 100ns units.
//

#define DEFAULT_CLOCK_RATE 156250

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
    InterruptStatusLowLevelProcessingRequired
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

/*++

Structure Description:

    This structure defines an interrupt.

Members:

    Vector - Stores the vector the interrupt is connected to.

    Line - Stores the interrupt line this interrupt is connected to.

    Mode - Stores the mode of the interrupt (edge or level).

    ServiceRoutine - Stores the Interrupt Service Routine associated with this
        interrupt.

    Context - Stores the context to be passed in when this ISR is executed.

    InterruptCount - Stores the number of interrupts received. This variable is
        not synchronized, so the count may not be exact.

    LastTimestamp - Stores the time counter value the last time this interrupt
        was sampled. This is used for interrupt storm detection.

--*/

typedef struct _KINTERRUPT KINTERRUPT, *PKINTERRUPT;
struct _KINTERRUPT {
    PKINTERRUPT NextInterrupt;
    INTERRUPT_LINE Line;
    INTERRUPT_MODE Mode;
    ULONG Vector;
    RUNLEVEL RunLevel;
    PINTERRUPT_SERVICE_ROUTINE ServiceRoutine;
    PVOID Context;
    UINTN InterruptCount;
    ULONGLONG LastTimestamp;
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

KSTATUS
HlStartAllProcessors (
    PVOID InitializationRoutine,
    PULONG ProcessorsStarted
    );

/*++

Routine Description:

    This routine is called on the BSP, and starts all APs.

Arguments:

    InitializationRoutine - Supplies the routine the processors should jump to.

    ProcessorsStarted - Supplies a pointer where the number of processors
        started will be returned (the total number of processors in the system).

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
    PINTERRUPT_SERVICE_ROUTINE ServiceRoutine,
    PVOID Context
    );

/*++

Routine Description:

    This routine creates and initialize a new KINTERRUPT structure.

Arguments:

    Vector - Supplies the vector that the interrupt will come in on.

    ServiceRoutine - Supplies a pointer to the function to call when this
        interrupt comes in.

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
    INTERRUPT_MODE TriggerMode,
    INTERRUPT_ACTIVE_LEVEL Polarity,
    ULONG LineStateFlags,
    PKINTERRUPT Interrupt
    );

/*++

Routine Description:

    This routine enables the given interrupt line.

Arguments:

    GlobalSystemInterruptNumber - Supplies the global system interrupt number
        to enable.

    TriggerMode - Supplies the trigger mode of the interrupt.

    Polarity - Supplies the polarity of the interrupt.

    LineStateFlags - Supplies additional line state flags to set. The flags
        INTERRUPT_LINE_STATE_FLAG_ENABLED will be ORed in automatically.

    Interrupt - Supplies a pointer to the interrupt structure this line will
        be connected to.

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
    SYSTEM_RESET_TYPE ResetType
    );

/*++

Routine Description:

    This routine resets the system.

Arguments:

    ResetType - Supplies the desired reset type. If the desired reset type is
        not supported, a cold reset will be attempted.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if an invalid reset type was supplied.

    STATUS_NOT_SUPPORTED if the system cannot be reset.

    STATUS_UNSUCCESSFUL if the system did not reset.

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

