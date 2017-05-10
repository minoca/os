/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    hmod.h

Abstract:

    This header contains definitions for external hardware modules. These are
    not drivers, but rather pieces of hardware core to the basic operation of
    the kernel, including timers, interrupt controllers, DMA controllers, and
    debug devices.

Author:

    Evan Green 28-Oct-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// These macros read from and write to a field that might not be aligned.
//

#define READ_UNALIGNED16(_Pointer) \
    (USHORT)(((volatile UCHAR *)(_Pointer))[0] | \
             (((USHORT)((volatile UCHAR *)(_Pointer))[1]) << 8))

#define WRITE_UNALIGNED16(_Pointer, _Value)               \
    ((volatile UCHAR *)(_Pointer))[0] = (UCHAR)(_Value),            \
    ((volatile UCHAR *)(_Pointer))[1] = (UCHAR)((_Value) >> 8)

#define READ_UNALIGNED32(_Pointer)          \
    (ULONG)(READ_UNALIGNED16(_Pointer) |    \
           (READ_UNALIGNED16((PUSHORT)(_Pointer) + 1) << 16))

#define WRITE_UNALIGNED32(_Pointer, _Value)                     \
    WRITE_UNALIGNED16((_Pointer), (_Value) & 0xFFFF),           \
    WRITE_UNALIGNED16((PUSHORT)(_Pointer) + 1, (((_Value) >> 16) & 0xFFFF))

#define READ_UNALIGNED64(_Pointer)              \
    (ULONGLONG)(READ_UNALIGNED32(_Pointer) |    \
                (((ULONGLONG)READ_UNALIGNED32((PULONG)(_Pointer) + 1)) << 32))

#define WRITE_UNALIGNED64(_Pointer, _Value)                                 \
    WRITE_UNALIGNED32(_Pointer, (_Value) & 0xFFFFFFFF),                     \
    WRITE_UNALIGNED32((PULONG)(_Pointer) + 1, ((_Value) >> 32) & 0xFFFFFFFF)

//
// ---------------------------------------------------------------- Definitions
//

//
// Set the GSI field of an interrupt lines description to this value to
// indicate that this set of lines does not map to any corresponding GSI range.
//

#define INTERRUPT_LINES_GSI_NONE 0xFFFFFFFF

//
// Define the special controller identifier reserved for the CPU itself.
//

#define INTERRUPT_CPU_IDENTIFIER ((UINTN)-1L)

//
// Define the PC CPU interrupt pins.
//

#define INTERRUPT_CPU_LINE_NORMAL_INTERRUPT 0x00000000
#define INTERRUPT_CPU_LINE_NMI 0x00000001
#define INTERRUPT_CPU_LINE_SMI 0x00000002
#define INTERRUPT_CPU_LINE_EXTINT 0x00000003
#define INTERRUPT_PC_MIN_CPU_LINE INTERRUPT_CPU_LINE_NORMAL_INTERRUPT
#define INTERRUPT_PC_MAX_CPU_LINE INTERRUPT_CPU_LINE_EXTINT + 1

//
// Define ARM CPU interrupt pins. Notice how the "normal" interrupt pin is
// always at 0.
//

#define INTERRUPT_CPU_IRQ_PIN 0x00000000
#define INTERRUPT_CPU_FIQ_PIN 0x00000001
#define INTERRUPT_ARM_MIN_CPU_LINE INTERRUPT_CPU_IRQ_PIN
#define INTERRUPT_ARM_MAX_CPU_LINE INTERRUPT_CPU_FIQ_PIN + 1

//
// Define the fixed vectors in the system. The spurious vector must end in 0xF
// as some processors hardwire the lower four bits of the spurious vector
// register.
//

#define VECTOR_SPURIOUS_INTERRUPT 0xFF
#define VECTOR_LOCAL_ERROR 0xFC

//
// Processor description flags.
//

//
// Set this flag if the processor is currently present and available to start.
//

#define PROCESSOR_DESCRIPTION_FLAG_PRESENT 0x00000001

//
// Timer feature flags.
//

//
// Set this flag if the timer's hardware is duplicated across every processor:
// that is, there is an independent timer for each processor.
//

#define TIMER_FEATURE_PER_PROCESSOR 0x00000001

//
// Set this flag if the timer's counter can be read. A readable timer is
// expected to be accessible immediately after it's been initialized, and must
// not generate interrupts or need to generate interrupts to handle rollovers.
// If these conditions cannot be met, do not expose the timer as readable.
//

#define TIMER_FEATURE_READABLE 0x00000002

//
// Set this flag if the timer's counter can be written to. For per-processor
// timers, this is expected to only write to the current processor's counter.
//

#define TIMER_FEATURE_WRITABLE 0x00000004

//
// Set this flag if the timer is capable of generating periodic interrupts.
//

#define TIMER_FEATURE_PERIODIC 0x00000008

//
// Set this flag if the timer is capable of generating one-shot interrupts.
//

#define TIMER_FEATURE_ONE_SHOT 0x00000010

//
// Set this flag if the timer's frequency varies with processor performance
// changes, such as frequency scaling.
//

#define TIMER_FEATURE_P_STATE_VARIANT 0x00000020

//
// Set this flag if the timer stops when the processor is halted.
//

#define TIMER_FEATURE_C_STATE_VARIANT 0x00000040

#define TIMER_FEATURE_VARIANT \
    (TIMER_FEATURE_P_STATE_VARIANT | TIMER_FEATURE_C_STATE_VARIANT)

//
// Set this flag only if this timer represents the official processor counter.
// For PC platforms this would be the TSC, for ARM this would be the cycle
// counter.
//

#define TIMER_FEATURE_PROCESSOR_COUNTER 0x00000080

//
// Set this flag if the timer is capable of generating interrupts based on an
// absolute timer value.
//

#define TIMER_FEATURE_ABSOLUTE 0x00000100

//
// Define calendar timer features.
//

//
// Set this flag if calls to write the calendar timer should pass a calendar
// time representation rather than a system time representation.
//

#define CALENDAR_TIMER_FEATURE_WANT_CALENDAR_FORMAT 0x00000001

//
// Set this flag if the calendar timer must be written to at low runlevel. This
// is true for timers that exist over busses like I2C.
//

#define CALENDAR_TIMER_FEATURE_LOW_RUNLEVEL 0x00000002

//
// Interrupt controller feature flags.
//

#define INTERRUPT_FEATURE_LOW_RUN_LEVEL 0x00000001

//
// Interrupt line state flags.
//

//
// Set this flag if the interrupt line should be unmasked.
//

#define INTERRUPT_LINE_STATE_FLAG_ENABLED 0x00000001

//
// Set this flag if the interrupt should be delivered to the processor that
// has the lowest hardware priority level.
//

#define INTERRUPT_LINE_STATE_FLAG_LOWEST_PRIORITY 0x00000002

//
// Set this flag if the interrupt is configured as a wake source.
//

#define INTERRUPT_LINE_STATE_FLAG_WAKE 0x00000004

//
// Set this flag to enable debouncing in the interrupt.
//

#define INTERRUPT_LINE_STATE_FLAG_DEBOUNCE 0x00000008

//
// Define the description table version numbers.
//

#define PROCESSOR_DESCRIPTION_VERSION 1
#define INTERRUPT_LINES_DESCRIPTION_VERSION 1
#define INTERRUPT_CONTROLLER_DESCRIPTION_VERSION 2
#define TIMER_DESCRIPTION_VERSION 1
#define DEBUG_DEVICE_DESCRIPTION_VERSION 1
#define CALENDAR_TIMER_DESCRIPTION_VERSION 2
#define CACHE_CONTROLLER_DESCRIPTION_VERSION 1

//
// Define the cache controller properties version.
//

#define CACHE_CONTROLLER_PROPERTIES_VERSION 1

#define HL_CACHE_FLAG_CLEAN 0x00000001
#define HL_CACHE_FLAG_INVALIDATE 0x00000002

//
// Define the reboot module table version.
//

#define REBOOT_MODULE_DESCRIPTION_VERSION 1

//
// Set this flag if the reboot controller needs to be called at low run level.
// If clear, this routine will be called at or above dispatch.
//

#define REBOOT_MODULE_LOW_LEVEL 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _INTERRUPT_CONTROLLER
    INTERRUPT_CONTROLLER, *PINTERRUPT_CONTROLLER;

typedef enum _HARDWARE_MODULE_TYPE {
    HardwareModuleInvalid,
    HardwareModuleInterruptController,
    HardwareModuleInterruptLines,
    HardwareModuleTimer,
    HardwareModuleDebugDevice,
    HardwareModuleCalendarTimer,
    HardwareModuleCacheController,
    HardwareModuleDebugUsbHostController,
    HardwareModuleReboot,
    HardwareModuleMaxTypes,
} HARDWARE_MODULE_TYPE, *PHARDWARE_MODULE_TYPE;

typedef enum _TIMER_MODE {
    TimerModeInvalid,
    TimerModePeriodic,
    TimerModeOneShot,
    TimerModeAbsolute
} TIMER_MODE, *PTIMER_MODE;

typedef enum _INTERRUPT_MODE {
    InterruptModeUnknown,
    InterruptModeEdge,
    InterruptModeLevel,
} INTERRUPT_MODE, *PINTERRUPT_MODE;

typedef enum _INTERRUPT_ACTIVE_LEVEL {
    InterruptActiveLevelUnknown,
    InterruptActiveLow,
    InterruptActiveHigh,
    InterruptActiveBoth
} INTERRUPT_ACTIVE_LEVEL, *PINTERRUPT_ACTIVE_LEVEL;

typedef enum _INTERRUPT_LINE_TYPE {
    InterruptLineInvalid,
    InterruptLineGsi,
    InterruptLineControllerSpecified,
} INTERRUPT_LINE_TYPE, *PINTERRUPT_LINE_TYPE;

typedef enum _INTERRUPT_LINES_TYPE {
    InterruptLinesInvalid,
    InterruptLinesStandardPin,
    InterruptLinesProcessorLocal,
    InterruptLinesSoftwareOnly,
    InterruptLinesOutput,
} INTERRUPT_LINES_TYPE, *PINTERRUPT_LINES_TYPE;

typedef enum _INTERRUPT_ADDRESSING {
    InterruptAddressingInvalid,
    InterruptAddressingPhysical,
    InterruptAddressingLogicalFlat,
    InterruptAddressingLogicalClustered,
    InterruptAddressingAll,
    InterruptAddressingAllExcludingSelf,
    InterruptAddressingSelf
} INTERRUPT_ADDRESSING, *PINTERRUPT_ADDRESSING;

typedef enum _INTERRUPT_CAUSE {
    InterruptCauseNoInterruptHere,
    InterruptCauseLineFired,
    InterruptCauseSpuriousInterrupt
} INTERRUPT_CAUSE, *PINTERRUPT_CAUSE;

typedef enum _SYSTEM_RESET_TYPE {
    SystemResetInvalid,
    SystemResetShutdown,
    SystemResetWarm,
    SystemResetCold,
    SystemResetTypeCount
} SYSTEM_RESET_TYPE, *PSYSTEM_RESET_TYPE;

/*++

Structure Description:

    This structure describes a high level lock. Users should not access or
    modify members of this structure directly, as its contents is subject to
    change without notice.

Members:

    Value - Stores the value of the lock.

    WasEnabled - Stores an internal boolean indicating the previous interrupt
        state.

--*/

typedef struct _HARDWARE_MODULE_LOCK {
    ULONG Value;
    BOOL WasEnabled;
} HARDWARE_MODULE_LOCK, *PHARDWARE_MODULE_LOCK;

//
// Interrupt controller structures.
//

/*++

Structure Description:

    This structure is used to return information about an interrupt controller.

Members:

    Controller - Stores a pointer to the controller itself, a kind of handle.

    StartingGsi - Stores the starting global system interrupt number of the
        controller.

    LineCount - Stores the number of lines in the interrupt controller.

--*/

typedef struct _INTERRUPT_CONTROLLER_INFORMATION {
    PINTERRUPT_CONTROLLER Controller;
    ULONG StartingGsi;
    ULONG LineCount;
} INTERRUPT_CONTROLLER_INFORMATION, *PINTERRUPT_CONTROLLER_INFORMATION;

/*++

Structure Description:

    This structure defines an interrupt target as actually supported by the
    interrupt controller hardware.

Members:

    Addressing - Stores the addressing mode of the interrupt.

    PhysicalId - Stores the physical ID of the processor being targeted, if the
        addressig mode is physical.

    LogicalFlatId - Stores the mask of processors being targeted if the
        addressing mode is logical flat.

    ClusterId - Stores the cluster identifier if the addressing is logical
        clustered.

    ClusterMask - Stores the mask of processors within the cluster if the
        addressing mode is logical clustered.

--*/

typedef struct _INTERRUPT_HARDWARE_TARGET {
    INTERRUPT_ADDRESSING Addressing;
    union {
        ULONG PhysicalId;
        ULONG LogicalFlatId;
        struct {
            ULONG Id;
            ULONG Mask;
        } Cluster;
    } U;

} INTERRUPT_HARDWARE_TARGET, *PINTERRUPT_HARDWARE_TARGET;

/*++

Structure Description:

    This structure describes a processor. It is filled out by the hardware
    module to describe a processor to the system.

Members:

    Version - Stores the version number of this table as understood by the
        hardware module. Set this to PROCESSOR_DESCRIPTION_VERSION.

    PhysicalId - Stores the processor identifier number. This number will be
        referred to by the system when communicating with the hardware module
        about a processor.

    LogicalFlatId - Stores the logical flat processor ID to use as a processor
        target. Set to 0 if logical flat mode is not supported or not supported
        for this processor.

    FirmwareIdentifier - Stores the processor identifier number used by the
        firmware. This number may or may not be the same as the hardware
        identifier.

    Flags - Stores a set of flags relating to the processor. See
        PROCESSOR_DESCRIPTION_FLAG_* definitions for valid values here.

    ParkedPhysicalAddress - Stores the physical address where this core has been
        parked.

--*/

typedef struct _PROCESSOR_DESCRIPTION {
    ULONG Version;
    ULONG PhysicalId;
    ULONG LogicalFlatId;
    ULONG FirmwareIdentifier;
    ULONG Flags;
    PHYSICAL_ADDRESS ParkedPhysicalAddress;
} PROCESSOR_DESCRIPTION, *PPROCESSOR_DESCRIPTION;

/*++

Structure Description:

    This structure describes a set of one or more interrupt lines.

Members:

    Version - Stores the version number of this table as understood by the
        hardware module. Set this to INTERRUPT_LINES_DESCRIPTION_VERSION.

    Type - Stores the general classification for this set of interrupt lines.

    Controller - Stores the controller ID for the controller these lines belong
        to.

    LineStart - Stores the first line, inclusive, of the line segment being
        described.

    LineEnd - Stores one beyond the last line (aka exclusive) of the line
        segment being described.

    Gsi - Stores the GSI base for this range. The GSI number in this member
        corresponds to the interrupt line at LineStart. The GSI numbers go up
        consecutively through the rest of the segment. Specify
        INTERRUPT_LINES_GSI_NONE to indicate that the line segment has no GSI
        mapping.

    OutputControllerIdentifer - Supplies the identifier of the controller this
        line segment refers to. This field is only valid for output line
        segments, as the lines refer to the destination controller's source
        lines.

--*/

typedef struct _INTERRUPT_LINES_DESCRIPTION {
    ULONG Version;
    INTERRUPT_LINES_TYPE Type;
    UINTN Controller;
    LONG LineStart;
    LONG LineEnd;
    ULONG Gsi;
    UINTN OutputControllerIdentifier;
} INTERRUPT_LINES_DESCRIPTION, *PINTERRUPT_LINES_DESCRIPTION;

/*++

Structure Description:

    This structure describes an interrupt line.

Members:

    Type - Stores the classification method used to identify the interrupt line.

    Gsi - Stores the global system interrupt number of the interrupt line. Used
        when the classification type is GSI.

    Controller - Stores the identifier of the controller on which the line
        exists. Used when the classification type is Controller specified.

    Line - Stores the line number. Negative numbers may be valid here. Used when
        the classification type is Controller specified.

--*/

typedef struct _INTERRUPT_LINE {
    INTERRUPT_LINE_TYPE Type;
    union {
        ULONG Gsi;
        struct {
            UINTN Controller;
            LONG Line;
        } Local;
    } U;

} INTERRUPT_LINE, *PINTERRUPT_LINE;

/*++

Structure Description:

    This structure describes the state of an interrupt line.

Members:

    Mode - Stores the interrupt trigger mode of the line.

    Polarity - Stores the polarity of the interrupt line.

    Flags - Stores a bitfield of flags governing the state of the interrupt
        line. See INTERRUPT_LINE_STATE_FLAG_* definitions.

    Vector - Stores the vector that this interrupt operates on.

    Target - Stores the set of processors to target this interrupt line at.

    Output - Stores the output line that this interrupt should output to.

    HardwarePriority - Stores a pointer to the hardware priority level this
        interrupt should be enabled at.

--*/

typedef struct _INTERRUPT_LINE_STATE {
    INTERRUPT_MODE Mode;
    INTERRUPT_ACTIVE_LEVEL Polarity;
    ULONG Flags;
    ULONG Vector;
    INTERRUPT_HARDWARE_TARGET Target;
    INTERRUPT_LINE Output;
    ULONG HardwarePriority;
} INTERRUPT_LINE_STATE, *PINTERRUPT_LINE_STATE;

//
// Timer structures.
//

/*++

Structure Description:

    This structure describes a timer's interrupt information.

Members:

    Line - Stores which interrupt line the timer fires on.

    TriggerMode - Stores the trigger mode. Set to unknown to use the default
        mode for the interrupt line.

    ActiveLevel - Stores the active line level. Set to unknown to use the
        default line level for the interrupt controller.

--*/

typedef struct _TIMER_INTERRUPT {
    INTERRUPT_LINE Line;
    INTERRUPT_MODE TriggerMode;
    INTERRUPT_ACTIVE_LEVEL ActiveLevel;
} TIMER_INTERRUPT, *PTIMER_INTERRUPT;

//
// Calendar time provider functions.
//

/*++

Structure Description:

    This structure describes an absolute wall clock time as provided to or
    from a calendar time hardware module.

Members:

    IsCalendarTime - Stores a boolean indicating if the calendar time is
        valid for this structure (TRUE) or the system time (FALSE).

    CalendarTime - Stores the calendar time to get or set.

    SystemTime - Stores the system time to get or set.

--*/

typedef struct _HARDWARE_MODULE_TIME {
    BOOL IsCalendarTime;
    union {
        CALENDAR_TIME CalendarTime;
        SYSTEM_TIME SystemTime;
    } U;

} HARDWARE_MODULE_TIME, *PHARDWARE_MODULE_TIME;

/*++

Structure Description:

    This structure describes the information for a message signaled interrupt
    that is to be retrieved from the hardware layer.

Members:

    Address - Stores the physical address to which the MSI/MSI-X data is to be
        written when the interrupt is triggered.

    Data - Stores the data to write to the physical address when the MSI/MSI-X
        interrupt is triggered.

--*/

typedef struct _MSI_INFORMATION {
    PHYSICAL_ADDRESS Address;
    ULONGLONG Data;
} MSI_INFORMATION, *PMSI_INFORMATION;

//
// Interrupt controller functions.
//

typedef
KSTATUS
(*PINTERRUPT_ENUMERATE_PROCESSORS) (
    PVOID Context,
    PPROCESSOR_DESCRIPTION Descriptions,
    ULONG DescriptionsBufferSize
    );

/*++

Routine Description:

    This routine describes all processors under the jurisdiction of an interrupt
    controller.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    Descriptions - Supplies a pointer to a buffer of an array of processor
        descriptions that the hardware module is expected to fill out on
        success. The number of entries in the array is the number of processors
        reported during the interrupt controller registration.

    DescriptionBufferSize - Supplies the size of the buffer passed. The hardware
        module should fail the routine if the buffer size is smaller than
        expected, but should not fail if the buffer size is larger than
        expected.

Return Value:

    STATUS_SUCCESS on success. The Descriptions buffer will contain
        descriptions of all processors under the jurisdiction of the given
        interrupt controller.

    Other status codes on failure. The contents of the Descriptions buffer is
        undefined.

--*/

typedef
KSTATUS
(*PINTERRUPT_INITIALIZE_LOCAL_UNIT) (
    PVOID Context,
    PULONG Identifier
    );

/*++

Routine Description:

    This routine initializes the local unit of an interrupt controller. It is
    always called on the processor of the local unit to initialize.

Arguments:

    Context - Supplies the pointer to the context of the I/O unit that owns
        this processor, provided by the hardware module upon initialization.

    Identifier - Supplies a pointer where this function will return the
        identifier of the processor being initialized.

Return Value:

    STATUS_SUCCESS on success.

    Other status codes on failure.

--*/

typedef
KSTATUS
(*PINTERRUPT_INITIALIZE_IO_UNIT) (
    PVOID Context
    );

/*++

Routine Description:

    This routine initializes an interrupt controller. It's responsible for
    masking all interrupt lines on the controller and setting the current
    priority to the lowest (allow all interrupts). Once completed successfully,
    it is expected that interrupts can be enabled at the processor core with
    no interrupts occurring.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

Return Value:

    STATUS_SUCCESS on success.

    Other status codes on failure.

--*/

typedef
KSTATUS
(*PINTERRUPT_SET_LOCAL_UNIT_ADDRESSING) (
    PVOID Context,
    PINTERRUPT_HARDWARE_TARGET Target
    );

/*++

Routine Description:

    This routine attempts to set the current processor's addressing mode.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    Target - Supplies a pointer to the targeting configuration to set for this
        processor.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_UNSUCCESSFUL if the operation failed.

    STATUS_NOT_SUPPORTED if this configuration is never supported on this
        hardware.

--*/

typedef
INTERRUPT_CAUSE
(*PINTERRUPT_BEGIN) (
    PVOID Context,
    PINTERRUPT_LINE FiringLine,
    PULONG MagicCandy
    );

/*++

Routine Description:

    This routine is called when an interrupt fires. Its role is to determine
    if an interrupt has fired on the given controller, accept it, and determine
    which line fired if any. This routine will always be called with interrupts
    disabled at the processor core.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    FiringLine - Supplies a pointer where the interrupt hardware module will
        fill in which line fired, if applicable.

    MagicCandy - Supplies a pointer where the interrupt hardware module can
        store 32 bits of private information regarding this interrupt. This
        information will be returned to it when the End Of Interrupt routine
        is called.

Return Value:

    Returns an interrupt cause indicating whether or not an interrupt line,
    spurious interrupt, or no interrupt fired on this controller.

--*/

typedef
VOID
(*PINTERRUPT_FAST_END_OF_INTERRUPT) (
    VOID
    );

/*++

Routine Description:

    This routine signals to the interrupt controller hardware that servicing
    of the highest priority interrupt line has been completed. This routine
    will always be called with interrupts disabled at the processor core.

Arguments:

    None.

Return Value:

    None.

--*/

typedef
VOID
(*PINTERRUPT_END_OF_INTERRUPT) (
    PVOID Context,
    ULONG MagicCandy
    );

/*++

Routine Description:

    This routine is called after an interrupt has fired and been serviced. Its
    role is to tell the interrupt controller that processing has completed.
    This routine will always be called with interrupts disabled at the
    processor core.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    MagicCandy - Supplies the magic candy that that the interrupt hardware
        module stored when the interrupt began.

Return Value:

    None.

--*/

typedef
KSTATUS
(*PINTERRUPT_REQUEST_INTERRUPT) (
    PVOID Context,
    PINTERRUPT_LINE Line,
    ULONG Vector,
    PINTERRUPT_HARDWARE_TARGET Target
    );

/*++

Routine Description:

    This routine requests a hardware interrupt on the given line.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    Line - Supplies a pointer to the interrupt line to spark.

    Vector - Supplies the vector to generate the interrupt on (for vectored
        architectures only).

    Target - Supplies a pointer to the set of processors to target.

Return Value:

    STATUS_SUCCESS on success.

    Error code on failure.

--*/

typedef
KSTATUS
(*PINTERRUPT_START_PROCESSOR) (
    PVOID Context,
    ULONG Identifier,
    PHYSICAL_ADDRESS JumpAddressPhysical
    );

/*++

Routine Description:

    This routine sends a "start interrupt" to the given processor.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    Identifier - Supplies the identifier of the processor to start.

    JumpAddressPhysical - Supplies the physical address of the location that
        new processor should jump to.

Return Value:

    STATUS_SUCCESS if the start command was successfully sent.

    Error code on failure.

--*/

typedef
KSTATUS
(*PINTERRUPT_SET_LINE_STATE) (
    PVOID Context,
    PINTERRUPT_LINE Line,
    PINTERRUPT_LINE_STATE State,
    PVOID ResourceData,
    UINTN ResourceDataSize
    );

/*++

Routine Description:

    This routine enables or disables and configures an interrupt line.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    Line - Supplies a pointer to the line to set up. This will always be a
        controller specified line.

    State - Supplies a pointer to the new configuration of the line.

    ResourceData - Supplies an optional pointer to the device specific resource
        data for the interrupt line.

    ResourceDataSize - Supplies the size of the resource data, in bytes.

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PINTERRUPT_GET_MESSAGE_INFORMATION) (
    ULONGLONG Vector,
    ULONGLONG VectorCount,
    PINTERRUPT_HARDWARE_TARGET Target,
    PINTERRUPT_LINE OutputLine,
    ULONG Flags,
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

    Target - Supplies a pointer to the set of processors to target.

    OutputLine - Supplies the output line this interrupt line should interrupt
        to.

    Flags - Supplies a bitfield of flags about the operation. See
        INTERRUPT_LINE_STATE_FLAG_* definitions.

    Information - Supplies a pointer to an array of MSI/MSI-X information to
        be filled in by the routine.

Return Value:

    Status code.

--*/

typedef
VOID
(*PINTERRUPT_MASK_LINE) (
    PVOID Context,
    PINTERRUPT_LINE Line,
    BOOL Enable
    );

/*++

Routine Description:

    This routine masks or unmasks an interrupt line, leaving the rest of the
    line state intact.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    Line - Supplies a pointer to the line to maek or unmask. This will always
        be a controller specified line.

    Enable - Supplies a boolean indicating whether to mask the interrupt,
        preventing interrupts from coming through (FALSE), or enable the line
        and allow interrupts to come through (TRUE).

Return Value:

    None.

--*/

typedef
KSTATUS
(*PINTERRUPT_SAVE_STATE) (
    PVOID Context,
    PVOID Buffer
    );

/*++

Routine Description:

    This routine saves the current state of the interrupt controller, which
    may lost momentarily in the hardware due to a power transition. Multiple
    save functions may be called in a row. If a transition is abandoned, the
    restore function is not called.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    Buffer - Supplies a pointer to the save buffer for this processor, the
        size of which was reported during registration.

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PINTERRUPT_RESTORE_STATE) (
    PVOID Context,
    PVOID Buffer
    );

/*++

Routine Description:

    This routine restores the previous state of the interrupt controller.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    Buffer - Supplies a pointer to the save buffer for this processor, the
        size of which was reported during registration.

Return Value:

    Status code.

--*/

/*++

Structure Description:

    This structure describes the API of a hardware module. It is passed by the
    hardware module to the kernel during registration to supply pointers to the
    hardware module's functionality.

Members:

    InitializeIoUnit - Stores a pointer to a function that initializes
        an interrupt controller.

    SetLineState - Stores a pointer to a fucntion that configures an interrupt
        line.

    MaskLine - Stores a pointer to a function used to mask and unmask
        interrupt lines (without altering the remaining line state).

    BeginInterrupt - Stores a pointer to a function that is called when an
        interrupt fires.

    FastEndOfInterrupt - Stores a pointer to a function that sends an End Of
        Interrupt command to the interrupt controller, signaling the end of
        servicing the highest priority line in service. The only difference
        between this routine and the normal end of interrupt routine is that
        this one takes no parameters. If this routine is supplied, it will
        always be used instead of the normal end of interrupt routine.

    EndOfInterrupt - Stores a pointer to a function that sends an End Of
        Interrupt command to the interrupt controller, singaling the end of
        servicing the highest priority line in service.

    RequestInterrupt - Stores a pointer to a function that requests a hardware
        interrupt on the given line.

    EnumerateProcessors - Stores a pointer to a function that describes a set of
        processors to the system.

    InitializeLocalUnit - Stores a pointer to a function that initializes the
        processor-local portion of an interrupt controller. This routine is
        called once on each processor during boot and after descructive idle
        states.

    SetLocalUnitAddressing - Stores a pointer to a function that sets the
        destination addressing mode for the current processor.

    StartProcessor - Stores a pointer to a function that starts another
        processor.

    GetMessageInformation - Stores a pointer to a function used to get MSI
        message address and data pairs, for controllers that support Message
        Signaled Interrupts.

    SaveState - Stores a pointer to a function used to save the interrupt
        controller state in preparation for a context loss (power transition).

    RestoreState - Stores a pointer to a function used to restore previously
        saved interrupt controller state after a power transition.

--*/

typedef struct _INTERRUPT_FUNCTION_TABLE {
    PINTERRUPT_INITIALIZE_IO_UNIT InitializeIoUnit;
    PINTERRUPT_SET_LINE_STATE SetLineState;
    PINTERRUPT_MASK_LINE MaskLine;
    PINTERRUPT_BEGIN BeginInterrupt;
    PINTERRUPT_FAST_END_OF_INTERRUPT FastEndOfInterrupt;
    PINTERRUPT_END_OF_INTERRUPT EndOfInterrupt;
    PINTERRUPT_REQUEST_INTERRUPT RequestInterrupt;
    PINTERRUPT_ENUMERATE_PROCESSORS EnumerateProcessors;
    PINTERRUPT_INITIALIZE_LOCAL_UNIT InitializeLocalUnit;
    PINTERRUPT_SET_LOCAL_UNIT_ADDRESSING SetLocalUnitAddressing;
    PINTERRUPT_START_PROCESSOR StartProcessor;
    PINTERRUPT_GET_MESSAGE_INFORMATION GetMessageInformation;
    PINTERRUPT_SAVE_STATE SaveState;
    PINTERRUPT_RESTORE_STATE RestoreState;
} INTERRUPT_FUNCTION_TABLE, *PINTERRUPT_FUNCTION_TABLE;

//
// Timer functions.
//

typedef
KSTATUS
(*PTIMER_INITIALIZE) (
    PVOID Context
    );

/*++

Routine Description:

    This routine initializes a timer and puts it into a known state. Once
    initialized, the timer should not be generating interrupts. If it has a
    readable counter, the counter should be counting after the initialize call
    has returned. This routine will be called once on boot and after any idle
    state transition that is destructive to the timer.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

Return Value:

    STATUS_SUCCESS on success.

    Other status codes on failure. The timer will not be used if a failure
    status code is returned.

--*/

typedef
ULONGLONG
(*PTIMER_READ_COUNTER) (
    PVOID Context
    );

/*++

Routine Description:

    This routine returns the hardware counter's raw value. All unimplemented
    bits should be set to 0. This routine will only be called for timers that
    have set the readable counter feature bit. The system assumes that all
    timers count up. If the hardware actually counts down, the implementation
    of this routine should do a subtraction from the maximum value to make it
    appear as though the timer counts up.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

Return Value:

    Returns the timer's current count.

--*/

typedef
VOID
(*PTIMER_WRITE_COUNTER) (
    PVOID Context,
    ULONGLONG NewCount
    );

/*++

Routine Description:

    This routine writes to the timer's hardware counter. This routine will
    only be called for timers that have the writable counter feature bit set.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

    NewCount - Supplies the value to write into the counter. It is expected that
        the counter will not stop after the write.

Return Value:

    None.

--*/

typedef
KSTATUS
(*PTIMER_ARM) (
    PVOID Context,
    TIMER_MODE Mode,
    ULONGLONG TickCount
    );

/*++

Routine Description:

    This routine arms the timer to fire an interrupt after the specified number
    of ticks. It is expected that arming the timer may alter the timeline of
    the counter.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

    Mode - Supplies the mode to arm the timer in. The system will never request
        a mode not supported by the timer's feature bits. The mode dictates
        how the tick count argument is interpreted.

    TickCount - Supplies the interval, in ticks, from now for the timer to fire
        in.

Return Value:

    STATUS_SUCCESS on success.

    Other error codes on failure. The timer will be considered in a failed
    state and will no longer be used if a failure code is returned here.

--*/

typedef
VOID
(*PTIMER_DISARM) (
    PVOID Context
    );

/*++

Routine Description:

    This routine disarms the timer, stopping interrupts from firing.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

Return Value:

    None.

--*/

typedef
VOID
(*PTIMER_ACKNOWLEDGE_INTERRUPT) (
    PVOID Context
    );

/*++

Routine Description:

    This routine performs any actions necessary upon reciept of a timer's
    interrupt. This may involve writing to an acknowledge register to re-enable
    the timer to fire again, or other hardware specific actions.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure describes the API of a timer module. It is passed by the
    hardware module to the kernel during registration to supply pointers to the
    hardware module's functionality.

Members:

    Initialize - Stores a pointer to a function that initializes a timer,
        making it non-interrupting and getting the counter ticking.

    ReadCounter - Stores a pointer to a function that reads the current count
        from the timer.

    WriteCounter - Stores a pointer to a function that writes a new count to
        the timer.

    Arm - Stores a pointer to a function that arms the timer to fire an
        interrupt at a given number of ticks from now.

    AcknowledgeInterrupt - Stores an optional pointer to a function that
        performs hardware specific actions in response to an interrupt.

--*/

typedef struct _TIMER_FUNCTION_TABLE {
    PTIMER_INITIALIZE Initialize;
    PTIMER_READ_COUNTER ReadCounter;
    PTIMER_WRITE_COUNTER WriteCounter;
    PTIMER_ARM Arm;
    PTIMER_DISARM Disarm;
    PTIMER_ACKNOWLEDGE_INTERRUPT AcknowledgeInterrupt;
} TIMER_FUNCTION_TABLE, *PTIMER_FUNCTION_TABLE;

//
// Debug device functions.
//

typedef
KSTATUS
(*PDEBUG_DEVICE_RESET) (
    PVOID Context,
    ULONG BaudRate
    );

/*++

Routine Description:

    This routine initializes and resets a debug device, preparing it to send
    and receive data.

Arguments:

    Context - Supplies the pointer to the port's context, provided by the
        hardware module upon initialization.

    BaudRate - Supplies the baud rate to set.

Return Value:

    STATUS_SUCCESS on success.

    Other status codes on failure. The device will not be used if a failure
    status code is returned.

--*/

typedef
KSTATUS
(*PDEBUG_DEVICE_TRANSMIT) (
    PVOID Context,
    PVOID Data,
    ULONG Size
    );

/*++

Routine Description:

    This routine transmits data from the host out through the debug device.

Arguments:

    Context - Supplies the pointer to the port's context, provided by the
        hardware module upon initialization.

    Data - Supplies a pointer to the data to write.

    Size - Supplies the size to write, in bytes.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

--*/

typedef
KSTATUS
(*PDEBUG_DEVICE_RECEIVE) (
    PVOID Context,
    PVOID Data,
    PULONG Size
    );

/*++

Routine Description:

    This routine receives incoming data from the debug device. If no data is
    available, this routine should return immediately. If only some of the
    requested data is available, this routine should return the data that can
    be obtained now and return.

Arguments:

    Context - Supplies the pointer to the port's context, provided by the
        hardware module upon initialization.

    Data - Supplies a pointer where the read data will be returned on success.

    Size - Supplies a pointer that on input contains the size of the receive
        buffer. On output, returns the number of bytes read.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_DATA_AVAILABLE if there was no data to be read at the current
    time.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

--*/

typedef
KSTATUS
(*PDEBUG_DEVICE_GET_STATUS) (
    PVOID Context,
    PBOOL ReceiveDataAvailable
    );

/*++

Routine Description:

    This routine returns the current device status.

Arguments:

    Context - Supplies the pointer to the port's context, provided by the
        hardware module upon initialization.

    ReceiveDataAvailable - Supplies a pointer where a boolean will be returned
        indicating whether or not receive data is available.

Return Value:

    Status code.

--*/

typedef
VOID
(*PDEBUG_DEVICE_DISCONNECT) (
    PVOID Context
    );

/*++

Routine Description:

    This routine disconnects a device, taking it offline.

Arguments:

    Context - Supplies the pointer to the port's context, provided by the
        hardware module upon initialization.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure describes the API of a debug device.

Members:

    Reset - Stores a pointer to a function used to reset and initialize the
        device.

    Transmit - Stores a pointer to a function used to transmit data out from
        the debug device.

    Receive - Stores a pointer to a function used to receive data from the
        debug device.

    GetStatus - Stores a pointer to a function used to get the status of the
        device.

    Disconnect - Stores a pointer to a function called when the debug
        connection is being dropped. If it is re-established, Reset will be
        called.

--*/

typedef struct _DEBUG_DEVICE_FUNCTION_TABLE {
    PDEBUG_DEVICE_RESET Reset;
    PDEBUG_DEVICE_TRANSMIT Transmit;
    PDEBUG_DEVICE_RECEIVE Receive;
    PDEBUG_DEVICE_GET_STATUS GetStatus;
    PDEBUG_DEVICE_DISCONNECT Disconnect;
} DEBUG_DEVICE_FUNCTION_TABLE, *PDEBUG_DEVICE_FUNCTION_TABLE;

//
// Calendar time functions.
//

typedef
KSTATUS
(*PCALENDAR_TIMER_INITIALIZE) (
    PVOID Context
    );

/*++

Routine Description:

    This routine initializes a calendar timer so that it may be ready for
    read and write calls.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

Return Value:

    STATUS_SUCCESS on success.

    Other status codes on failure. The timer will not be used if a failure
    status code is returned.

--*/

typedef
KSTATUS
(*PCALENDAR_TIMER_READ) (
    PVOID Context,
    PHARDWARE_MODULE_TIME CurrentTime
    );

/*++

Routine Description:

    This routine returns the calendar timer's current value.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

    CurrentTime - Supplies a pointer where the read current time will be
        returned.

Return Value:

    STATUS_SUCCESS on success.

    Other status codes on failure.

--*/

typedef
KSTATUS
(*PCALENDAR_TIMER_WRITE) (
    PVOID Context,
    PHARDWARE_MODULE_TIME NewTime
    );

/*++

Routine Description:

    This routine writes to the calendar timer.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

    NewTime - Supplies a pointer to the new time to set. The hardware module
        should set this as quickly as possible. The system will supply either
        a calendar time or a system time in here based on which type the timer
        requested at registration.

Return Value:

    STATUS_SUCCESS on success.

    Other status codes on failure.

--*/

/*++

Structure Description:

    This structure describes the API of a calendar timer hardware module. It is
    passed by the hardware module to the kernel during registration to supply
    pointers to the hardware module's functionality.

Members:

    Initialize - Stores a pointer to a function used to initialize the unit.

    Read - Stores a pointer to a function that returns the current calendar
        time from the timer.

    Write - Stores a pointer to a function that sets the current calendar time
        in the timer.

--*/

typedef struct _CALENDAR_TIMER_FUNCTION_TABLE {
    PCALENDAR_TIMER_INITIALIZE Initialize;
    PCALENDAR_TIMER_READ Read;
    PCALENDAR_TIMER_WRITE Write;
} CALENDAR_TIMER_FUNCTION_TABLE, *PCALENDAR_TIMER_FUNCTION_TABLE;

//
// Cache controller structures.
//

/*++

Structure Description:

    This structure describes the properties of a cache controller.

Members:

    Version - Stores the version of the cache controller properties structure.
        The system will set this to the version number it is expecting when
        querying properties. The hardware module should fail if it does not
        support the requested version (e.g. a version greater than its version).

    DataCacheLineSize - Stores the size of a data cache line in bytes.

    InstructionCacheLineSize - Stores the size of an instruction cache line in
        bytes.

    CacheSize - Stores the size of the cache in bytes.

--*/

typedef struct _CACHE_CONTROLLER_PROPERTIES {
    ULONG Version;
    ULONG DataCacheLineSize;
    ULONG InstructionCacheLineSize;
    ULONG CacheSize;
} CACHE_CONTROLLER_PROPERTIES, *PCACHE_CONTROLLER_PROPERTIES;

//
// Cache controller functions.
//

typedef
KSTATUS
(*PCACHE_CONTROLLER_INITIALIZE) (
    PVOID Context
    );

/*++

Routine Description:

    This routine initializes a cache controller to enable the cache and prepare
    it for clean and invalidate calls.

Arguments:

    Context - Supplies the pointer to the cache controller's context, provided
        by the hardware module upon initialization.

Return Value:

    STATUS_SUCCESS on success.

    Other status codes on failure. The cache controller will not be used if a
    failure status code is returned.

--*/

typedef
VOID
(*PCACHE_CONTROLLER_FLUSH) (
    PVOID Context,
    ULONG Flags
    );

/*++

Routine Description:

    This routine cleans and/or invalidates the cache owned by the cache
    controller.

Arguments:

    Context - Supplies the pointer to the cache controller's context, provided
        by the hardware module upon initialization.

    Flags - Supplies a bitmask of flush flags. See CACHE_CONTROLLER_FLUSH_FLAG_*
        for definitions.

Return Value:

    None.

--*/

typedef
VOID
(*PCACHE_CONTROLLER_FLUSH_REGION) (
    PVOID Context,
    PHYSICAL_ADDRESS Address,
    UINTN SizeInBytes,
    ULONG Flags
    );

/*++

Routine Description:

    This routine cleans and/or invalidates a region of the cache owned by the
    cache controller.

Arguments:

    Context - Supplies the pointer to the cache controller's context, provided
        by the hardware module upon initialization.

    Address - Supplies the starting physical address of the region to flush. It
        must be aligned to the cache line size.

    SizeInBytes - Supplies the number of bytes to flush.

    Flags - Supplies a bitmask of flush flags. See CACHE_CONTROLLER_FLUSH_FLAG_*
        for definitions.

Return Value:

    None.

--*/

typedef
KSTATUS
(*PCACHE_CONTROLLER_GET_PROPERTIES) (
    PVOID Context,
    PCACHE_CONTROLLER_PROPERTIES Properties
    );

/*++

Routine Description:

    This routine cleans and invalidates the cache owned by the cache controller.

Arguments:

    Context - Supplies the pointer to the cache controller's context, provided
        by the hardware module upon initialization.

    Properties - Supplies a pointer that receives the properties of the given
        cache controller (e.g. cache line size).

Return Value:

    STATUS_SUCCESS on success.

    Other status codes on failure.

--*/

/*++

Structure Description:

    This structure describes the API of a cache controller hardware module. It
    is passed by the hardware module to the kernel during registration to
    supply pointers to the hardware module's functionality.

Members:

    Initialize - Stores a pointer to a function used to initialize the unit.

    Flush - Stores a pointer to a function used to flush a single cache line.

    FlushRegion - Stores a pointer to a function used to flush a region of
        the cache.

    GetProperties - Stores a pointer to a function used to query the cache
        controller.

--*/

typedef struct _CACHE_CONTROLLER_FUNCTION_TABLE {
    PCACHE_CONTROLLER_INITIALIZE Initialize;
    PCACHE_CONTROLLER_FLUSH Flush;
    PCACHE_CONTROLLER_FLUSH_REGION FlushRegion;
    PCACHE_CONTROLLER_GET_PROPERTIES GetProperties;
} CACHE_CONTROLLER_FUNCTION_TABLE, *PCACHE_CONTROLLER_FUNCTION_TABLE;

//
// System reset functions
//

typedef
KSTATUS
(*PREBOOT_PREPARE) (
    PVOID Context,
    SYSTEM_RESET_TYPE ResetType
    );

/*++

Routine Description:

    This routine prepares the system for a reboot or system power transition.
    This function is called at low level when possible. During emergency reboot
    situations, this function may not be called.

Arguments:

    Context - Supplies the pointer to the reboot controller's context, provided
        by the hardware module upon initialization.

    ResetType - Supplies the reset type that is going to occur.

Return Value:

    STATUS_SUCCESS on success.

    Other status codes on failure.

--*/

typedef
KSTATUS
(*PREBOOT_SYSTEM) (
    PVOID Context,
    SYSTEM_RESET_TYPE ResetType,
    PVOID Data,
    UINTN Size
    );

/*++

Routine Description:

    This routine shuts down or reboots the entire system.

Arguments:

    Context - Supplies the pointer to the reboot controller's context, provided
        by the hardware module upon initialization.

    ResetType - Supplies the reset type.

    Data - Supplies an optional pointer's worth of platform-specific data.

    Size - Supplies the size of the platform-specific data.

Return Value:

    STATUS_SUCCESS on success.

    Other status codes on failure.

--*/

/*++

Structure Description:

    This structure describes the API of a reboot hardware module.

Members:

    Prepare - Supplies a pointer to a function used to prepare the system for
        a reboot or system power transition when done gracefully. During an
        emergency reboot this function may not be called.

    Reboot - Supplies a pointer to a function used to reboot the system.

--*/

typedef struct _REBOOT_MODULE_FUNCTION_TABLE {
    PREBOOT_PREPARE Prepare;
    PREBOOT_SYSTEM Reboot;
} REBOOT_MODULE_FUNCTION_TABLE, *PREBOOT_MODULE_FUNCTION_TABLE;

//
// Registration structures.
//

/*++

Structure Description:

    This structure is used to describe an interrupt controller to the system.
    It is passed from the hardware module to the kernel.

Members:

    TableVersion - Stores the version of the interrupt controller description
        table as understood by the hardware module. Set this to
        INTERRUPT_CONTROLLER_DESCRIPTION_VERSION.

    FunctionTable - Stores the table of pointers to the hardware module's
        functions.

    Context - Stores a pointer's worth of data specific to this interrupt
        controller instance. This pointer will be passed back to the hardware
        module on each call.

    Flags - Stores a bitfield of flags regarding this interrupt controller.
        See INTERRUPT_FEATURE_* flags.

    Identifier - Stores the unique identifier of the interrupt controller.
        This is expected to be unique across all interrupt controllers in the
        system.

    ProcessorCount - Stores the number of processors under the jurisdiction of
        this interrupt controller.

    PriorityCount - Stores the number of hardware priority levels that
        interrupts can be configured at. This value may be 0 to indicate that
        the controller does not support a hardware priority scheme.

    SaveContextSize - Stores the number of bytes needed per processor to save
        the interrupt controller state.

--*/

typedef struct _INTERRUPT_CONTROLLER_DESCRIPTION {
    ULONG TableVersion;
    INTERRUPT_FUNCTION_TABLE FunctionTable;
    PVOID Context;
    ULONG Flags;
    UINTN Identifier;
    ULONG ProcessorCount;
    ULONG PriorityCount;
    ULONG SaveContextSize;
} INTERRUPT_CONTROLLER_DESCRIPTION, *PINTERRUPT_CONTROLLER_DESCRIPTION;

/*++

Structure Description:

    This structure is used to describe a timer to the system. It is passed from
    the hardware module to the kernel.

Members:

    TableVersion - Stores the version of the timer description table as
        understood by the hardware module. Set this to
        TIMER_DESCRIPTION_VERSION.

    FunctionTable - Stores the table of pointers to the hardware module's
        functions.

    Context - Stores a pointer's worth of data specific to this timer instance.
        This pointer will be passed back to the hardware module on each call.

    Identifier - Stores the unique identifier of the timer.

    Features - Stores a bitfield of the timer's features. See TIMER_FEATURE_*
        definitions.

    CounterFrequency - Stores the frequency of the counter, in Hertz. This is
        required even if the counter is not exposed as readable, as it is used
        in calculations for arming tick counts. If the counter's frequency is
        not known, supply 0, and the system will measure the counter's frequency
        using another timer.

    CounterBitWidth - Stores the number of bits in the counter.

    Interrupt - Stores information about how the timer's interrupt is routed
        and configured.

--*/

typedef struct _TIMER_DESCRIPTION {
    ULONG TableVersion;
    TIMER_FUNCTION_TABLE FunctionTable;
    PVOID Context;
    ULONG Identifier;
    ULONG Features;
    ULONGLONG CounterFrequency;
    ULONG CounterBitWidth;
    TIMER_INTERRUPT Interrupt;
} TIMER_DESCRIPTION, *PTIMER_DESCRIPTION;

/*++

Structure Description:

    This structure is used to describe a debug device to the system. It is
    passed from the hardware module to the kernel.

Members:

    TableVersion - Stores the version of the debug device description table as
        understood by the hardware module. Set this to
        DEBUG_DEVICE_DESCRIPTION_VERSION.

    FunctionTable - Stores the table of pointers to the hardware module's
        functions.

    Context - Stores a pointer's worth of data specific to this serial instance.
        This pointer will be passed back to the hardware module on each call.

    PortType - Stores the port type of the debug device as defined by the
        debug port table 2 specification.

    PortSubType - Stores the port subtype of the debug device as defined by the
        debug port table 2 specification.

    Identifier - Stores the unique identifier of the device, often its physical
        base address.

--*/

typedef struct _DEBUG_DEVICE_DESCRIPTION {
    ULONG TableVersion;
    DEBUG_DEVICE_FUNCTION_TABLE FunctionTable;
    PVOID Context;
    USHORT PortType;
    USHORT PortSubType;
    ULONGLONG Identifier;
} DEBUG_DEVICE_DESCRIPTION, *PDEBUG_DEVICE_DESCRIPTION;

/*++

Structure Description:

    This structure is used to describe a calendar timer to the system. It is
    passed from the hardware module to the kernel.

Members:

    TableVersion - Stores the version of the calendar timer description
        table as understood by the hardware module. Set this to
        CALENDAR_TIMER_DESCRIPTION_VERSION.

    FunctionTable - Stores the table of pointers to the hardware module's
        functions.

    Context - Stores a pointer's worth of data specific to this calendar timer
        instance. This pointer will be passed back to the hardware module on
        each call.

    Identifier - Stores the unique identifier of the calendar timer.

    Features - Stores a bitfield of features about the timer. See
        CALENDAR_TIMER_FEATURE_* definitions.

--*/

typedef struct _CALENDAR_TIMER_DESCRIPTION {
    ULONG TableVersion;
    CALENDAR_TIMER_FUNCTION_TABLE FunctionTable;
    PVOID Context;
    ULONG Identifier;
    ULONG Features;
} CALENDAR_TIMER_DESCRIPTION, *PCALENDAR_TIMER_DESCRIPTION;

/*++

Structure Description:

    This structure is used to describe a cache controller to the system.
    It is passed from the hardware module to the kernel.

Members:

    TableVersion - Stores the version of the cache controller description table
        as understood by the hardware module. Set this to
        CACHE_CONTROLLER_DESCRIPTION_VERSION.

    FunctionTable - Stores the table of pointers to the hardware module's
        functions.

    Context - Stores a pointer's worth of data specific to this cache
        controller instance. This pointer will be passed back to the hardware
        module on each call.

    Identifier - Stores the unique identifier of the cache controller.

    PropertiesVersion - Stores the version of the cache controller properties
        as understood by the hardware module. Set this to
        CACHE_CONTROLLER_PROPERTIES_VERSION.

--*/

typedef struct _CACHE_CONTROLLER_DESCRIPTION {
    ULONG TableVersion;
    CACHE_CONTROLLER_FUNCTION_TABLE FunctionTable;
    PVOID Context;
    ULONG Identifier;
    ULONG PropertiesVersion;
} CACHE_CONTROLLER_DESCRIPTION, *PCACHE_CONTROLLER_DESCRIPTION;

/*++

Structure Description:

    This structure is used to describe a reboot controller to the system.
    It is passed from the hardware module to the kernel.

Members:

    TableVersion - Stores the version of the cache controller description table
        as understood by the hardware module. Set this to
        REBOOT_MODULE_DESCRIPTION_VERSION.

    FunctionTable - Stores the table of pointers to the hardware module's
        functions.

    Context - Stores a pointer's worth of data specific to this cache
        controller instance. This pointer will be passed back to the hardware
        module on each call.

    Identifier - Stores the unique identifier of the reboot controller.

    Properties - Stores a bitfield of flags describing the reboot controller.
        See REBOOT_MODULE_* definitions.

--*/

typedef struct _REBOOT_MODULE_DESCRIPTION {
    ULONG TableVersion;
    REBOOT_MODULE_FUNCTION_TABLE FunctionTable;
    PVOID Context;
    ULONG Identifier;
    ULONG Properties;
} REBOOT_MODULE_DESCRIPTION, *PREBOOT_MODULE_DESCRIPTION;

//
// Hardware module prototypes.
//

typedef
PVOID
(*PHARDWARE_MODULE_GET_ACPI_TABLE) (
    ULONG Signature,
    PVOID PreviousTable
    );

/*++

Routine Description:

    This routine attempts to find an ACPI description table with the given
    signature.

Arguments:

    Signature - Supplies the signature of the desired table.

    PreviousTable - Supplies a pointer to the table to start the search from.

Return Value:

    Returns a pointer to the beginning of the header to the table if the table
    was found, or NULL if the table could not be located.

--*/

typedef
VOID
(*PHARDWARE_MODULE_ENTRY) (
    VOID
    );

/*++

Routine Description:

    This routine is the entry point for a hardware module. Its role is to
    detect the prescense of any of the hardware modules it contains
    implementations for and instantiate them with the kernel.

Arguments:

    None.

Return Value:

    None.

--*/

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

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

KERNEL_API
PVOID
HlGetAcpiTable (
    ULONG Signature,
    PVOID PreviousTable
    );

/*++

Routine Description:

    This routine attempts to find an ACPI description table with the given
    signature.

Arguments:

    Signature - Supplies the signature of the desired table.

    PreviousTable - Supplies a pointer to the table to start the search from.

Return Value:

    Returns a pointer to the beginning of the header to the table if the table
    was found, or NULL if the table could not be located.

--*/

KERNEL_API
PVOID
HlAllocateMemory (
    UINTN Size,
    ULONG Tag,
    BOOL Device,
    PPHYSICAL_ADDRESS PhysicalAddress
    );

/*++

Routine Description:

    This routine allocates memory from the non-paged pool. This memory will
    never be paged out and can be accessed at any level.

Arguments:

    Size - Supplies the size of the allocation, in bytes.

    Tag - Supplies an identifier to associate with the allocation, useful for
        debugging and leak detection.

    Device - Supplies a boolean indicating if this memory will be accessed by
        a device directly. If TRUE, the memory will be mapped uncached.

    PhysicalAddress - Supplies an optional pointer where the physical address
        of the allocation is returned.

Return Value:

    Returns the allocated memory if successful, or NULL on failure.

--*/

KERNEL_API
PVOID
HlMapPhysicalAddress (
    PHYSICAL_ADDRESS PhysicalAddress,
    ULONG SizeInBytes,
    BOOL CacheDisabled
    );

/*++

Routine Description:

    This routine maps a physical address into kernel VA space. It is meant so
    that system components can access memory mapped hardware.

Arguments:

    PhysicalAddress - Supplies a pointer to the physical address. This address
        must be page aligned.

    SizeInBytes - Supplies the size in bytes of the mapping. This will be
        rounded up to the nearest page size.

    CacheDisabled - Supplies a boolean indicating if the memory is to be mapped
        uncached.

Return Value:

    Returns a pointer to the virtual address of the mapping on success, or
    NULL on failure.

--*/

KERNEL_API
VOID
HlUnmapAddress (
    PVOID VirtualAddress,
    ULONG SizeInBytes
    );

/*++

Routine Description:

    This routine unmaps memory mapped with MmMapPhysicalMemory.

Arguments:

    VirtualAddress - Supplies the virtual address to unmap.

    SizeInBytes - Supplies the number of bytes to unmap.

Return Value:

    None.

--*/

KERNEL_API
VOID
HlReportPhysicalAddressUsage (
    PHYSICAL_ADDRESS PhysicalAddress,
    ULONGLONG Size
    );

/*++

Routine Description:

    This routine is called by a hardware module plugin to notify the system
    about a range of physical address space that is in use by that hardware
    plugin. This helps notify the system to avoid using this address space
    when configuring devices that can remap their memory windows. This function
    should be called during the discovery portion, as it is relevant to the
    system regardless of whether that hardware module is actually initialized
    and used.

Arguments:

    PhysicalAddress - Supplies the first physical address in use by the hardware
        module.

    Size - Supplies the size of the memory segment, in bytes.

Return Value:

    None.

--*/

KERNEL_API
VOID
HlInitializeLock (
    PHARDWARE_MODULE_LOCK Lock
    );

/*++

Routine Description:

    This routine initializes a hardware module lock structure. This must be
    called before the lock can be acquired or released.

Arguments:

    Lock - Supplies a pointer to the lock to initialize.

Return Value:

    None.

--*/

KERNEL_API
VOID
HlAcquireLock (
    PHARDWARE_MODULE_LOCK Lock
    );

/*++

Routine Description:

    This routine disables interrupts and acquires a high level spin lock.
    Callers should be very careful to avoid doing this in hot paths or for
    very long. This lock is not reentrant.

Arguments:

    Lock - Supplies a pointer to the lock to acquire.

Return Value:

    None.

--*/

KERNEL_API
VOID
HlReleaseLock (
    PHARDWARE_MODULE_LOCK Lock
    );

/*++

Routine Description:

    This routine releases a previously acquired high level lock and restores
    interrupts to their previous state.

Arguments:

    Lock - Supplies a pointer to the lock to release.

Return Value:

    None.

--*/

