/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ke.h

Abstract:

    This header contains definitions for the Kernel Executive.

Author:

    Evan Green 5-Aug-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/crash.h>

//
// --------------------------------------------------------------------- Macros
//

//
// Define macros for encoding and decoding system version information. These
// can be revised as needed since this encoded structure is not exposed to
// consumers. They use the SYSTEM_VERSION_INFORMATION structure.
//

#define ENCODE_VERSION_INFORMATION(_Major, _Minor, _Revision, _Release, _Debug)\
    (((_Major) << 24) |     \
     ((_Minor) << 16) |     \
     ((_Revision) << 8) |   \
     ((_Release) << 4) |    \
     (_Debug))

#define DECODE_MAJOR_VERSION(_EncodedVersion) (UCHAR)((_EncodedVersion) >> 24)
#define DECODE_MINOR_VERSION(_EncodedVersion) (UCHAR)((_EncodedVersion) >> 16)
#define DECODE_VERSION_REVISION(_EncodedVersion) (UCHAR)((_EncodedVersion) >> 8)
#define DECODE_VERSION_RELEASE(_EncodedVersion) \
    (UCHAR)(((_EncodedVersion) >> 4) & 0x0F)

#define DECODE_VERSION_DEBUG(_EncodedVersion) \
    (UCHAR)((_EncodedVersion) & 0x0F)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the generic catch-all Ke tag: Ke!!
//

#define KE_ALLOCATION_TAG 0x2121654B

//
// Define the scheduler allocation tag: KeSc
//

#define KE_SCHEDULER_ALLOCATION_TAG 0x6353654B

//
// Define the event allocation tag: KeEv
//

#define KE_EVENT_ALLOCATION_TAG 0x7645654B

//
// Define the work item allocation tag: KeWo
//

#define KE_WORK_ITEM_ALLOCATION_TAG 0x6F57654B

//
// Define the Ke system information allocation tag: KInf
//

#define KE_INFORMATION_ALLOCATION_TAG 0x666E494B

//
// Define the maximum number of comma-separated values in a kernel argument.
//

#define KERNEL_MAX_ARGUMENT_VALUES 10
#define KERNEL_MAX_COMMAND_LINE 4096

//
// Work queue flags.
//

//
// Set this bit if the work queue should support adding work items at
// dispatch level.
//

#define WORK_QUEUE_FLAG_SUPPORT_DISPATCH_LEVEL 0x00000001

//
// Define the mask of publicly accessible timer flags.
//

#define KTIMER_FLAG_PUBLIC_MASK 0x0

//
// Define user shared data processor feature flags.
//

//
// This bit is set if the processor supports the sysenter instruction.
//

#define X86_FEATURE_SYSENTER 0x00000001

//
// This bit is set if the processor supports the syscall instruction.
//

#define X86_FEATURE_SYSCALL  0x00000002

//
// This bit is set if the processor conforms to at least the Pentium Pro ISA
// (circa 1995).
//

#define X86_FEATURE_I686     0x00000004

//
// This bit is set if the processor supports fxsave/fxrstor instructions.
//

#define X86_FEATURE_FXSAVE   0x00000008

//
// This bit is set if the kernel is ARMv7.
//

#define ARM_FEATURE_V7       0x00000001

//
// This bit is set if the processor supports VFPv2 or beyond.
//

#define ARM_FEATURE_VFP2      0x00000002

//
// This bit is set if the processor supports VFPv3.
//

#define ARM_FEATURE_VFP3      0x00000004

//
// This bit is set if the processor supports NEON advanced SIMD with 32 64-bit
// registers.
//

#define ARM_FEATURE_NEON32     0x00000008

//
// Define the set of DCP flags.
//

#define DPC_FLAG_QUEUED_ON_PROCESSOR 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _RUNLEVEL {
    RunLevelLow       = 0,
    RunLevelDispatch  = 2,
    RunLevelMaxDevice = 11,
    RunLevelClock     = 13,
    RunLevelIpi       = 14,
    RunLevelHigh      = 15,
    RunLevelCount     = 16
} RUNLEVEL, *PRUNLEVEL;

typedef enum _WORK_PRIORITY {
    WorkPriorityInvalid,
    WorkPriorityNormal,
    WorkPriorityHigh,
} WORK_PRIORITY, *PWORK_PRIORITY;

typedef enum _PROCESSOR_SET_TARGET {
    ProcessorTargetInvalid,
    ProcessorTargetNone,
    ProcessorTargetAny,
    ProcessorTargetAll,
    ProcessorTargetAllExcludingSelf,
    ProcessorTargetSelf,
    ProcessorTargetSingleProcessor
} PROCESSOR_SET_TARGET, *PPROCESSOR_SET_TARGET;

typedef enum _WORK_QUEUE_STATE {
    WorkQueueStateInvalid,
    WorkQueueStateOpen,
    WorkQueueStatePaused,
    WorkQueueStateWakingForDestroying,
    WorkQueueStateDestroying,
    WorkQueueStateDestroyed
} WORK_QUEUE_STATE, *PWORK_QUEUE_STATE;

typedef enum _DPC_CRASH_REASON {
    DpcCrashReasonInvalid,
    DpcCrashDpcBlocked,
    DpcCrashReasonDoubleQueueDpc,
    DpcCrashReasonNullRoutine,
    DpcCrashReasonCorrupt,
} DPC_CRASH_REASON, *PDPC_CRASH_REASON;

typedef enum _SCHEDULER_REASON {
    SchedulerReasonInvalid,
    SchedulerReasonDispatchInterrupt,
    SchedulerReasonThreadBlocking,
    SchedulerReasonThreadYielding,
    SchedulerReasonThreadSuspending,
    SchedulerReasonThreadExiting
} SCHEDULER_REASON, *PSCHEDULER_REASON;

typedef enum _SYSTEM_INFORMATION_SUBSYSTEM {
    SystemInformationInvalid,
    SystemInformationKe,
    SystemInformationIo,
    SystemInformationMm,
    SystemInformationPs,
    SystemInformationHl,
    SystemInformationSp,
    SystemInformationPm
} SYSTEM_INFORMATION_SUBSYSTEM, *PSYSTEM_INFORMATION_SUBSYSTEM;

typedef enum _KE_INFORMATION_TYPE {
    KeInformationInvalid,
    KeInformationSystemVersion,
    KeInformationFirmwareTable,
    KeInformationFirmwareType,
    KeInformationProcessorUsage,
    KeInformationProcessorCount,
    KeInformationKernelCommandLine,
    KeInformationBannerThread,
} KE_INFORMATION_TYPE, *PKE_INFORMATION_TYPE;

typedef enum _SYSTEM_FIRMWARE_TYPE {
    SystemFirmwareInvalid,
    SystemFirmwareUnknown,
    SystemFirmwareEfi,
    SystemFirmwarePcat,
} SYSTEM_FIRMWARE_TYPE, *PSYSTEM_FIRMWARE_TYPE;

typedef enum _CYCLE_ACCOUNT {
    CycleAccountInvalid,
    CycleAccountUser,
    CycleAccountKernel,
    CycleAccountInterrupt,
    CycleAccountIdle,
} CYCLE_ACCOUNT, *PCYCLE_ACCOUNT;

typedef enum _TIMER_QUEUE_TYPE {
    TimerQueueSoft,
    TimerQueueSoftWake,
    TimerQueueHard,
    TimerQueueCount,
} TIMER_QUEUE_TYPE, *PTIMER_QUEUE_TYPE;

typedef enum _CLOCK_TIMER_MODE {
    ClockTimerInvalid,
    ClockTimerPeriodic,
    ClockTimerOneShot,
    ClockTimerOff
} CLOCK_TIMER_MODE, *PCLOCK_TIMER_MODE;

typedef struct _KTIMER KTIMER, *PKTIMER;
typedef struct _KTIMER_DATA KTIMER_DATA, *PKTIMER_DATA;
typedef struct _WORK_ITEM WORK_ITEM, *PWORK_ITEM;
typedef struct _WORK_QUEUE WORK_QUEUE, *PWORK_QUEUE;
typedef struct _SCHEDULER_DATA SCHEDULER_DATA, *PSCHEDULER_DATA;
typedef struct _SCHEDULER_GROUP_ENTRY
    SCHEDULER_GROUP_ENTRY, *PSCHEDULER_GROUP_ENTRY;

typedef struct _SCHEDULER_GROUP SCHEDULER_GROUP, *PSCHEDULER_GROUP;
typedef struct _DPC DPC, *PDPC;
typedef struct _PROCESSOR_START_BLOCK
    PROCESSOR_START_BLOCK, *PPROCESSOR_START_BLOCK;

typedef
VOID
(*PIPI_ROUTINE) (
    PVOID Context
    );

/*++

Routine Description:

    This routine executes as part of an Inter-Processor Interrupt request. It
    is run simultaneously on the set of processors requested.

Arguments:

    Context - Supplies the context pointer supplied when the IPI was requested.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure describes a set of zero or more processors.

Members:

    Target - Stores the processor target.

    Number - Stores the processor number if the target indicates a single
        processor.

--*/

typedef struct _PROCESSOR_SET {
    PROCESSOR_SET_TARGET Target;
    union {
        ULONG Number;
    } U;

} PROCESSOR_SET, *PPROCESSOR_SET;

/*++

Structure Description:

    This structure describes the cycle accounting information for a processor.

Members:

    UserCycles - Stores the accumulated number of cycles this processor has
        spent in user mode.

    KernelCycles - Stores the accumulated number of cycles this processor has
        spent in kernel mode (not including interrupt and idle time).

    InterruptCycles - Stores the accumulated number of cycles this processor
        has spent servicing interrupts and DPCs.

    IdleCycles - Stores the accumulated number of cycles this processor has
        spent idle.

--*/

typedef struct _PROCESSOR_CYCLE_ACCOUNTING {
    ULONGLONG UserCycles;
    ULONGLONG KernelCycles;
    ULONGLONG InterruptCycles;
    ULONGLONG IdleCycles;
} PROCESSOR_CYCLE_ACCOUNTING, *PPROCESSOR_CYCLE_ACCOUNTING;

/*++

Structure Description:

    This structure contains the context for a scheduling group.

Members:

    ListEntry - Stores pointers to the next and previous groups underneath the
        parent of this group.

    Parent - Stores a pointer to the parent scheduler group.

    Children - Stores the head of the list of child scheduler groups this
        group owns.

    Entries - Stores a pointer to the array of entries for this group.

    EntryCount - Stores the element count of the entries array.

    ThreadCount - Stores the number of threads, ready or not, that live in the
        group.

--*/

struct _SCHEDULER_GROUP {
    LIST_ENTRY ListEntry;
    PSCHEDULER_GROUP Parent;
    LIST_ENTRY Children;
    PSCHEDULER_GROUP_ENTRY Entries;
    UINTN EntryCount;
    UINTN ThreadCount;
};

/*++

Structure Description:

    This structure contains the context for a scheduling group on a particular
    scheduler (CPU).

Members:

    Entry - Stores the regular scheduling entry data.

    Children - Stores the head of the list of scheduling entries that are ready
        to be run within this group.

    ReadyThreadCount - Stores the number of threads inside this group and all
        its children (meaning this includes all ready threads inside child and
        grandchild groups).

    Scheduler - Stores a pointer to the root CPU this group belongs to.

    Group - Stores a pointer to the owning group structure.

--*/

struct _SCHEDULER_GROUP_ENTRY {
    SCHEDULER_ENTRY Entry;
    LIST_ENTRY Children;
    UINTN ReadyThreadCount;
    PSCHEDULER_DATA Scheduler;
    PSCHEDULER_GROUP Group;
};

/*++

Structure Description:

    This structure contains the scheduler context for a specific processor.

Members:

    Lock - Stores the spin lock serializing access to the scheduling data.

    Group - Stores the fixed head scheduling group for this processor.

--*/

struct _SCHEDULER_DATA {
    KSPIN_LOCK Lock;
    SCHEDULER_GROUP_ENTRY Group;
};

/*++

Structure Description:

    This structure contains the state for a pending interrupt on this processor.
    Because new interrupts cause the interrupt controller to block all
    interrupts at that priority and lower, there can only be at most one
    pending interrupt per run level.

Members:

    Vector - Stores the interrupt vector.

    MagicCandy - Stores the opaque value returned by the interrupt controller
        when the interrupt was acknowledged. This is saved because it needs
        to be returned to the interrupt controller in the end of interrupt
        routine.

    InterruptController - Stores a pointer to the interrupt controller that
        generated the interrupt.

--*/

typedef struct _PENDING_INTERRUPT {
    ULONG Vector;
    ULONG MagicCandy;
    PVOID InterruptController;
} PENDING_INTERRUPT, *PPENDING_INTERRUPT;

/*++

Structure Description:

    This structure contains the state for this processor's dynamic tick
    management.

Members:

    Mode - Stores the current clock mode.

    NextMode - Stores the next clock mode.

    DueTime - Stores the next deadline if the current clock mode is one-shot.

    CurrentTime - Stores a relatively recent time counter timestamp.

    Hard - Stores a boolean indicating if the given due time is hard (must be
        met exactly then) or soft (can be met by the next periodic interrupt).

    AnyHard - Stores a boolean indicating if there are any hard timers queued
        on this processor.

    InterruptCount - Stores the total accumulated number of clock interrupts.

    NextDebugEvent - Stores the next deadline after which the processor should
        perform debug maintenance, either sending out profiling data or
        polling the debugger.

--*/

typedef struct _CLOCK_TIMER_DATA {
    CLOCK_TIMER_MODE Mode;
    CLOCK_TIMER_MODE NextMode;
    ULONGLONG DueTime;
    ULONGLONG CurrentTime;
    BOOL Hard;
    BOOL AnyHard;
    UINTN InterruptCount;
    ULONGLONG NextDebugEvent;
} CLOCK_TIMER_DATA, *PCLOCK_TIMER_DATA;

/*++

Structure Description:

    This structure contains the processor identification information.

Members:

    Vendor - Stores the CPU vendor. For x86, this contains the EBX portion of
        CPUID, function 1. For ARM, this contains the implementor code.

    Family - Stores the CPU family ID.

    Model - Stores the CPU model ID.

    Stepping - Stores the CPU stepping ID.

--*/

typedef struct _PROCESSOR_IDENTIFICATION {
    ULONG Vendor;
    USHORT Family;
    USHORT Model;
    USHORT Stepping;
} PROCESSOR_IDENTIFICATION, *PPROCESSOR_IDENTIFICATION;

/*++

Structure Description:

    This structure stores the current running state of a processor.

Members:

    Self - Stores a pointer to this structure. This is used to get the actual
        memory address when the structure is retrieved through unconventional
        means (like a segment register).

    ProcessorNumber - Stores the zero-based logical processor number.

    RunLevel - Stores the current run level of the processor.

    Tss - Stores a pointer to the current Task Segment for this processor. This
        only applies to PC processors. This member is accessed directly by
        assembly code, so its offset must be manually maintained.

    Gdt - Stores a pointer to the GDT for this processor. This only applies to
        PC processors. This member is accessed directly by assembly code, so
        its offset should be manually maintained.

    RunningThread - Stores the current thread scheduled on this processor.

    PreviousThread - Stores a pointer to the thread that was just scheduled
        out, but has yet to be put back on the ready list.

    IdleThread - Stores a pointer to the idle thread for this processor.

    Scheduler - Stores the scheduler context for this processor.

    Idt - Stores a pointer to the Interrupt Descriptor Table.

    InterruptTable - Stores an array of pointers to interrupt objects. The
        array is indexed by vector number, where the first index is the
        minimum vector.

    IpiListHead - Stores the list head for IPI request packets.

    IpiListLock - Stores the lock protecting access to the IPI list.

    PendingInterruptCount - Stores the number of interrupts that are currently
        queued to be replayed.

    PendingInterrupts - Stores the queue of interrupts that need to be
        replayed. This array is the size of the number of hardware levels that
        exist, and will always be sorted. It requires that interrupt
        controllers never allow interrupts to get through that are less than
        or equal to the priority of the current interrupt in service.

    PendingDispatchInterrupt - Stores a boolean indicating whether or not a
        dispatch level software interrupt is pending.

    DpcInProgress - Stores a pointer to the currently executing DPC.

    DpcLock - Stores the spin lock protecting the DPC list.

    DpcList - Stores the list head of DPCs pending on this processor.

    DpcCount - Stores the total number of DPCS that have occurred on this
        processor.

    TimerData - Stores a pointer to the timer management context.

    Clock - Stores the dynamic tick state.

    ClockInterruptCount - Stores the number of clock interrupts for this
        processor.

    CyclePeriodStart - Stores the beginning of the current cycle accounting
        period.

    CyclePeriodAccount - Stores the attribution of the current cycle accounting
        period.

    UserCycles - Stores the accumulated number of cycles this processor has
        spent in user mode.

    KernelCycles - Stores the accumulated number of cycles this processor has
        spent in kernel mode (not including interrupt and idle time).

    InterruptCycles - Stores the accumulated number of cycles this processor
        has spent servicing interrupts and DPCs.

    IdleCycles - Stores the accumulated number of cycles this processor has
        spent idle.

    SwapPage - Stores a pointer to a virtual address that can be used for
        temporary mappings.

    NmiCount - Stores a count of nested NMIs this processor has taken.

    CpuVersion - Stores the processor identification information for this CPU.

--*/

typedef struct _PROCESSOR_BLOCK PROCESSOR_BLOCK, *PPROCESSOR_BLOCK;
struct _PROCESSOR_BLOCK {
    PPROCESSOR_BLOCK Self;
    ULONG ProcessorNumber;
    RUNLEVEL RunLevel;
    PVOID Tss;
    PVOID Gdt;
    PKTHREAD RunningThread;
    PKTHREAD PreviousThread;
    PKTHREAD IdleThread;
    SCHEDULER_DATA Scheduler;
    PVOID Idt;
    PVOID *InterruptTable;
    LIST_ENTRY IpiListHead;
    KSPIN_LOCK IpiListLock;
    ULONG PendingInterruptCount;
    PENDING_INTERRUPT PendingInterrupts[RunLevelCount];
    UCHAR PendingDispatchInterrupt;
    PDPC DpcInProgress;
    KSPIN_LOCK DpcLock;
    LIST_ENTRY DpcList;
    UINTN DpcCount;
    PKTIMER_DATA TimerData;
    CLOCK_TIMER_DATA Clock;
    ULONGLONG CyclePeriodStart;
    CYCLE_ACCOUNT CyclePeriodAccount;
    volatile ULONGLONG UserCycles;
    volatile ULONGLONG KernelCycles;
    volatile ULONGLONG InterruptCycles;
    volatile ULONGLONG IdleCycles;
    PVOID SwapPage;
    UINTN NmiCount;
    PROCESSOR_IDENTIFICATION CpuVersion;
};

/*++

Structure Description:

    This structure defines usage information for one or more processors.

Members:

    ProcessorNumber - Stores the processor number corresponding to the usage,
        or -1 if this data represents all processors.

    CycleCounterFrequency - Stores the frequency of the cycle counter. If all
        processors are included and processors run at different speeds, then
        this value may be zero.

    Usage - Stores the cycle counter usage information.

--*/

typedef struct _PROCESSOR_USAGE_INFORMATION {
    UINTN ProcessorNumber;
    ULONGLONG CycleCounterFrequency;
    PROCESSOR_CYCLE_ACCOUNTING Usage;
} PROCESSOR_USAGE_INFORMATION, *PPROCESSOR_USAGE_INFORMATION;

/*++

Structure Description:

    This structure provides information about the number of processors in the
    system.

Members:

    MaxProcessorCount - Stores the maximum number of processors that might
        be active in the machine.

    ActiveProcessorCount - Stores the number of processors online right now.

--*/

typedef struct _PROCESSOR_COUNT_INFORMATION {
    UINTN MaxProcessorCount;
    UINTN ActiveProcessorCount;
} PROCESSOR_COUNT_INFORMATION, *PPROCESSOR_COUNT_INFORMATION;

/*++

Structure Description:

    This structure defines a queued lock. These locks can be used at or below
    dispatch level, or only below if paged memory is used.

Members:

    Header - Stores the object header.

    OwningThread - Stores a pointer to the thread that is holding the lock.

--*/

typedef struct _QUEUED_LOCK {
    OBJECT_HEADER Header;
    PKTHREAD OwningThread;
} QUEUED_LOCK, *PQUEUED_LOCK;

/*++

Structure Description:

    This structure defines an event.

Members:

    Header - Stores the object header.

--*/

typedef struct _KEVENT {
    OBJECT_HEADER Header;
} KEVENT, *PKEVENT;

/*++

Structure Description:

    This structure defines a shared-exclusive lock.

Members:

    State - Stores the current state of the shared-exclusive lock. See
        SHARED_EXCLUSIVE_LOCK_* definitions.

    Event - Stores a pointer to the event that allows for blocking.

    ExclusiveWaiters - Stores the number of thread trying to acquire the lock
        exclusively.

    SharedWaiters - Stores the number of threads trying to acquire the lock
        shared.

--*/

typedef struct _SHARED_EXCLUSIVE_LOCK {
    volatile ULONG State;
    PKEVENT Event;
    volatile ULONG ExclusiveWaiters;
    volatile ULONG SharedWaiters;
} SHARED_EXCLUSIVE_LOCK, *PSHARED_EXCLUSIVE_LOCK;

/*++

Structure Description:

    This structure defines a single kernel argument. An argument takes the
    form component.name=value1,value2,...

Members:

    Component - Stores a pointer to the string describing the component
        receiving the argument.

    Name - Stores a pointer to a string containing the name of the argument.

    Values - Stores an array of arguments.

    ValueCount - Stores the number of valid elements in the array.

--*/

typedef struct _KERNEL_ARGUMENT {
    PSTR Component;
    PSTR Name;
    PSTR Values[KERNEL_MAX_ARGUMENT_VALUES];
    ULONG ValueCount;
} KERNEL_ARGUMENT, *PKERNEL_ARGUMENT;

/*++

Structure Description:

    This structure defines the kernel command line information.

Members:

    Line - Stores a pointer to the complete command line.

    LineSize - Stores the size of the command line, including the null
        terminator.

    Arguments - Stores the array of command arguments. This will be NULL for
        user-mode requests; user-mode is responsible for doing its own
        splitting.

    ArgumentCount - Stores the number of arguments.

--*/

typedef struct _KERNEL_COMMAND_LINE {
    PSTR Line;
    ULONG LineSize;
    PKERNEL_ARGUMENT Arguments;
    ULONG ArgumentCount;
} KERNEL_COMMAND_LINE, *PKERNEL_COMMAND_LINE;

typedef
VOID
(*PWORK_ITEM_ROUTINE) (
    PVOID Parameter
    );

/*++

Routine Description:

    This routine prototype represents a work item.

Arguments:

    Parameter - Supplies an optional parameter passed in by the creator of the
        work item.

Return Value:

    None.

--*/

typedef
VOID
(*PDPC_ROUTINE) (
    PDPC Dpc
    );

/*++

Routine Description:

    This routine prototype represents a function that gets called when a DPC
    (Deferred Procedure Call) is executed. When this routine is called, it
    is safe to requeue or destroy the DPC.

Arguments:

    Dpc - Supplies a pointer to the DPC that is running.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure defines a Deferred Procedure Call object.

Members:

    ListEntry - Stores pointers to the next and previous DPCs in the queue.

    DpcRoutine - Stores a pointer to the routine to call when the DPC fires.

    UserData - Stores an opaque pointer that the creator of the DPC can use to
        store context.

    Processor - Stores the processor number this DPC is queued to.

    UseCount - Stores the number of entities actively using this object.

    Flags - Stores a bitmask of flags for the DPC. See DPC_FLAG_* for
        definitions.

--*/

struct _DPC {
    LIST_ENTRY ListEntry;
    PDPC_ROUTINE DpcRoutine;
    PVOID UserData;
    ULONG Processor;
    volatile ULONG UseCount;
    ULONG Flags;
};

/*++

Structure Description:

    This structure defines the contents of the user shared data page, which is
    shared between kernel mode and user mode.

Members:

    EncodedSystemVersion - Stores the encoded system version information.

    SystemVersionSerial - Stores the serial system revision.

    BuildTime - Stores the system build time (the seconds portion of a system
        time structure).

    TimeCounterFrequency - Stores the frequency of the time counter. This value
        won't change once the system is booted.

    ProcessorCounterFrequency - Stores the frequency of the processor counter
        on the boot processor. This is roughly related to the processor speed,
        but not exactly. For example, on ARM, it may the the processor speed
        divided by 64.

    TimeOffset - Stores the system time when the time counter was zero.

    TimeCounter - Stores the number of ticks since the system was started. This
        value is periodically updated and serves only as a reasonable
        approximation of the current time counter.

    SystemTime - Stores the current system time.

    TickCount - Stores the number of clock interrupts that have occurred since
        the system started. Clock interrupts do not necessarily occur at the
        same interval and thus cannot be used to accurately measure time. This
        member is incremented each time the time counter and system time
        members are updated, so it can be used to detect torn reads.

    TickCount2 - Stores a copy of the tick count value, updated after all the
        other time members are updated (with a memory barrier in between the
        updates of all other time variables and this one).

    ProcessorFeatures - Stores a bitfield of architecture-specific feature
        flags.

--*/

typedef struct _USER_SHARED_DATA {
    ULONG EncodedSystemVersion;
    ULONG SystemVersionSerial;
    ULONGLONG BuildTime;
    ULONGLONG TimeCounterFrequency;
    ULONGLONG ProcessorCounterFrequency;
    volatile SYSTEM_TIME TimeOffset;
    volatile ULONGLONG TimeCounter;
    volatile SYSTEM_TIME SystemTime;
    volatile ULONGLONG TickCount;
    volatile ULONGLONG TickCount2;
    ULONG ProcessorFeatures;
} USER_SHARED_DATA, *PUSER_SHARED_DATA;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KERNEL_API
KSTATUS
KeGetSystemVersion (
    PSYSTEM_VERSION_INFORMATION VersionInformation,
    PVOID Buffer,
    PULONG BufferSize
    );

/*++

Routine Description:

    This routine gets the system version information.

Arguments:

    VersionInformation - Supplies a pointer where the system version
        information will be returned.

    Buffer - Supplies an optional pointer to the buffer to use for the
        product name and build string.

    BufferSize - Supplies an optional pointer that on input contains the size
        of the supplied string buffer in bytes. On output, returns the needed
        size of the build string buffer in bytes including the null terminator
        characters.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_TOO_SMALL if the supplied buffer was not big enough to hold
    both strings.

--*/

KERNEL_API
PQUEUED_LOCK
KeCreateQueuedLock (
    VOID
    );

/*++

Routine Description:

    This routine creates a new queued lock under the current thread. These locks
    can be used at up to dispatch level if non-paged memory is used.

Arguments:

    None.

Return Value:

    Returns a pointer to the new lock on success.

    NULL on failure.

--*/

KERNEL_API
VOID
KeDestroyQueuedLock (
    PQUEUED_LOCK Lock
    );

/*++

Routine Description:

    This routine destroys a queued lock by decrementing its reference count.

Arguments:

    Lock - Supplies a pointer to the queued lock to destroy.

Return Value:

    None. When the function returns, the lock must not be used again.

--*/

KERNEL_API
VOID
KeAcquireQueuedLock (
    PQUEUED_LOCK Lock
    );

/*++

Routine Description:

    This routine acquires the queued lock. If the lock is held, the thread
    blocks until it becomes available.

Arguments:

    Lock - Supplies a pointer to the queued lock to acquire.

Return Value:

    None. When the function returns, the lock will be held.

--*/

KERNEL_API
KSTATUS
KeAcquireQueuedLockTimed (
    PQUEUED_LOCK Lock,
    ULONG TimeoutInMilliseconds
    );

/*++

Routine Description:

    This routine acquires the queued lock. If the lock is held, the thread
    blocks until it becomes available or the specified timeout expires.

Arguments:

    Lock - Supplies a pointer to the queued lock to acquire.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the given
        object should be waited on before timing out. Use WAIT_TIME_INDEFINITE
        to wait forever on the object.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_TIMEOUT if the specified amount of time expired and the lock could
    not be acquired.

--*/

KERNEL_API
VOID
KeReleaseQueuedLock (
    PQUEUED_LOCK Lock
    );

/*++

Routine Description:

    This routine releases a queued lock that has been previously acquired.

Arguments:

    Lock - Supplies a pointer to the queued lock to release.

Return Value:

    None.

--*/

KERNEL_API
BOOL
KeTryToAcquireQueuedLock (
    PQUEUED_LOCK Lock
    );

/*++

Routine Description:

    This routine attempts to acquire the queued lock. If the lock is busy, it
    does not add this thread to the queue of waiters.

Arguments:

    Lock - Supplies a pointer to a queued lock.

Return Value:

    Returns TRUE if the lock was acquired, or FALSE otherwise.

--*/

KERNEL_API
BOOL
KeIsQueuedLockHeld (
    PQUEUED_LOCK Lock
    );

/*++

Routine Description:

    This routine determines whether a queued lock is acquired or free.

Arguments:

    Lock - Supplies a pointer to the queued lock.

Return Value:

    TRUE if the queued lock is held.

    FALSE if the queued lock is free.

--*/

KERNEL_API
VOID
KeInitializeSpinLock (
    PKSPIN_LOCK Lock
    );

/*++

Routine Description:

    This routine initializes a spinlock.

Arguments:

    Lock - Supplies a pointer to the lock to initialize.

Return Value:

    None.

--*/

KERNEL_API
VOID
KeAcquireSpinLock (
    PKSPIN_LOCK Lock
    );

/*++

Routine Description:

    This routine acquires a kernel spinlock. It must be acquired at or below
    dispatch level. This routine may yield the processor.

Arguments:

    Lock - Supplies a pointer to the lock to acquire.

Return Value:

    None.

--*/

KERNEL_API
VOID
KeReleaseSpinLock (
    PKSPIN_LOCK Lock
    );

/*++

Routine Description:

    This routine releases a kernel spinlock.

Arguments:

    Lock - Supplies a pointer to the lock to release.

Return Value:

    None.

--*/

KERNEL_API
BOOL
KeTryToAcquireSpinLock (
    PKSPIN_LOCK Lock
    );

/*++

Routine Description:

    This routine makes one attempt to acquire a spinlock.

Arguments:

    Lock - Supplies a pointer to the lock to attempt to acquire.

Return Value:

    TRUE if the lock was acquired.

    FALSE if the lock was not acquired.

--*/

KERNEL_API
BOOL
KeIsSpinLockHeld (
    PKSPIN_LOCK Lock
    );

/*++

Routine Description:

    This routine determines whether a spin lock is held or free.

Arguments:

    Lock - Supplies a pointer to the lock to check.

Return Value:

    TRUE if the lock has been acquired.

    FALSE if the lock is free.

--*/

KERNEL_API
PSHARED_EXCLUSIVE_LOCK
KeCreateSharedExclusiveLock (
    VOID
    );

/*++

Routine Description:

    This routine creates a shared-exclusive lock.

Arguments:

    None.

Return Value:

    Returns a pointer to a shared-exclusive lock on success, or NULL on failure.

--*/

KERNEL_API
VOID
KeDestroySharedExclusiveLock (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    );

/*++

Routine Description:

    This routine destroys a shared-exclusive lock.

Arguments:

    SharedExclusiveLock - Supplies a pointer to a shared-exclusive lock.

Return Value:

    None.

--*/

KERNEL_API
VOID
KeAcquireSharedExclusiveLockShared (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    );

/*++

Routine Description:

    This routine acquired the given shared-exclusive lock in shared mode.

Arguments:

    SharedExclusiveLock - Supplies a pointer to the shared-exclusive lock.

Return Value:

    None.

--*/

KERNEL_API
BOOL
KeTryToAcquireSharedExclusiveLockShared (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    );

/*++

Routine Description:

    This routine makes a single attempt to acquire the given shared-exclusive
    lock in shared mode.

Arguments:

    SharedExclusiveLock - Supplies a pointer to the shared-exclusive lock.

Return Value:

    TRUE if the lock was successfully acquired shared.

    FALSE if the lock was not successfully acquired shared.

--*/

KERNEL_API
VOID
KeReleaseSharedExclusiveLockShared (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    );

/*++

Routine Description:

    This routine releases the given shared-exclusive lock from shared mode.

Arguments:

    SharedExclusiveLock - Supplies a pointer to the shared-exclusive lock.

Return Value:

    None.

--*/

KERNEL_API
VOID
KeAcquireSharedExclusiveLockExclusive (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    );

/*++

Routine Description:

    This routine acquired the given shared-exclusive lock in exclusive mode.

Arguments:

    SharedExclusiveLock - Supplies a pointer to the shared-exclusive lock.

Return Value:

    None.

--*/

KERNEL_API
BOOL
KeTryToAcquireSharedExclusiveLockExclusive (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    );

/*++

Routine Description:

    This routine makes a single attempt to acquire the given shared-exclusive
    lock exclusively.

Arguments:

    SharedExclusiveLock - Supplies a pointer to the shared-exclusive lock.

Return Value:

    TRUE if the lock was successfully acquired exclusively.

    FALSE if the lock was not successfully acquired.

--*/

KERNEL_API
VOID
KeReleaseSharedExclusiveLockExclusive (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    );

/*++

Routine Description:

    This routine releases the given shared-exclusive lock from exclusive mode.

Arguments:

    SharedExclusiveLock - Supplies a pointer to the shared-exclusive lock.

Return Value:

    None.

--*/

KERNEL_API
VOID
KeSharedExclusiveLockConvertToExclusive (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    );

/*++

Routine Description:

    This routine converts a lock that the caller holds shared into one that
    the caller holds exclusive. This routine will most likely fully release
    and reacquire the lock.

Arguments:

    SharedExclusiveLock - Supplies a pointer to the shared-exclusive lock.

Return Value:

    None.

--*/

KERNEL_API
BOOL
KeIsSharedExclusiveLockHeld (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    );

/*++

Routine Description:

    This routine determines whether a shared-exclusive lock is held or free.

Arguments:

    SharedExclusiveLock - Supplies a pointer to a shared-exclusive lock.

Return Value:

    Returns TRUE if the shared-exclusive lock is held, or FALSE if not.

--*/

KERNEL_API
BOOL
KeIsSharedExclusiveLockHeldExclusive (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    );

/*++

Routine Description:

    This routine determines whether a shared-exclusive lock is held exclusively
    or not.

Arguments:

    SharedExclusiveLock - Supplies a pointer to a shared-exclusive lock.

Return Value:

    Returns TRUE if the shared-exclusive lock is held exclusively, or FALSE
    otherwise.

--*/

KERNEL_API
BOOL
KeIsSharedExclusiveLockHeldShared (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    );

/*++

Routine Description:

    This routine determines whether a shared-exclusive lock is held shared or
    not.

Arguments:

    SharedExclusiveLock - Supplies a pointer to a shared-exclusive lock.

Return Value:

    Returns TRUE if the shared-exclusive lock is held shared, or FALSE
    otherwise.

--*/

KERNEL_API
BOOL
KeIsSharedExclusiveLockContended (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    );

/*++

Routine Description:

    This routine determines whether a shared-exclusive lock is being waited on
    for shared or exclusive access.

Arguments:

    SharedExclusiveLock - Supplies a pointer to a shared-exclusive lock.

Return Value:

    Returns TRUE if other threads are waiting to acquire the lock, or FALSE
    if the lock is uncontented.

--*/

KERNEL_API
RUNLEVEL
KeGetRunLevel (
    VOID
    );

/*++

Routine Description:

    This routine gets the running level for the current processor.

Arguments:

    None.

Return Value:

    Returns the current run level.

--*/

KERNEL_API
PDPC
KeCreateDpc (
    PDPC_ROUTINE DpcRoutine,
    PVOID UserData
    );

/*++

Routine Description:

    This routine creates a new DPC with the given routine and context data.

Arguments:

    DpcRoutine - Supplies a pointer to the routine to call when the DPC fires.

    UserData - Supplies a context pointer that can be passed to the routine via
        the DPC when it is called.

Return Value:

    Returns a pointer to the allocated and initialized (but not queued) DPC.

--*/

KERNEL_API
VOID
KeDestroyDpc (
    PDPC Dpc
    );

/*++

Routine Description:

    This routine destroys a DPC. It will cancel the DPC if it is queued, and
    wait for it to finish if it is running. This routine must be called from
    low level.

Arguments:

    Dpc - Supplies a pointer to the DPC to destroy.

Return Value:

    None.

--*/

KERNEL_API
VOID
KeQueueDpc (
    PDPC Dpc
    );

/*++

Routine Description:

    This routine queues a DPC on the current processor.

Arguments:

    Dpc - Supplies a pointer to the DPC to queue.

Return Value:

    None.

--*/

KERNEL_API
VOID
KeQueueDpcOnProcessor (
    PDPC Dpc,
    ULONG ProcessorNumber
    );

/*++

Routine Description:

    This routine queues a DPC on the given processor.

Arguments:

    Dpc - Supplies a pointer to the DPC to queue.

    ProcessorNumber - Supplies the processor number of the processor to queue
        the DPC on.

Return Value:

    None.

--*/

KERNEL_API
KSTATUS
KeCancelDpc (
    PDPC Dpc
    );

/*++

Routine Description:

    This routine attempts to cancel a DPC that has been queued.

Arguments:

    Dpc - Supplies a pointer to the DPC to cancel.

Return Value:

    STATUS_SUCCESS if the DPC was successfully pulled out of a queue.

    STATUS_TOO_LATE if the DPC has already started running.

--*/

KERNEL_API
VOID
KeFlushDpc (
    PDPC Dpc
    );

/*++

Routine Description:

    This routine does not return until the given DPC is out of the system. This
    means that the DPC is neither queued nor running. It's worth noting that
    this routine busy spins at dispatch level, and should therefore be used
    only sparingly. This routine can only be called from low level.

Arguments:

    Dpc - Supplies a pointer to the DPC to wait for.

Return Value:

    None.

--*/

KERNEL_API
PKTIMER
KeCreateTimer (
    ULONG AllocationTag
    );

/*++

Routine Description:

    This routine creates a new timer object. Once created, this timer needs to
    be initialized before it can be queued. This routine must be called at or
    below dispatch level.

Arguments:

    AllocationTag - Supplies a pointer to an identifier to use for the
        allocation that uniquely identifies the driver or module allocating the
        timer.

Return Value:

    Returns a pointer to the timer on success.

    NULL on resource allocation failure.

--*/

KERNEL_API
VOID
KeDestroyTimer (
    PKTIMER Timer
    );

/*++

Routine Description:

    This routine destroys a timer object. If the timer is currently queued, this
    routine cancels the timer and then destroys it. This routine must be called
    at or below dispatch level.

Arguments:

    Timer - Supplies a pointer to the timer to destroy.

Return Value:

    None.

--*/

KERNEL_API
KSTATUS
KeQueueTimer (
    PKTIMER Timer,
    TIMER_QUEUE_TYPE QueueType,
    ULONGLONG DueTime,
    ULONGLONG Period,
    ULONG Flags,
    PDPC Dpc
    );

/*++

Routine Description:

    This routine configures and queues a timer object. The timer must not
    already be queued, otherwise the system will crash. This routine must be
    called at or below dispatch level.

Arguments:

    Timer - Supplies a pointer to the timer to configure and queue.

    QueueType - Supplies the queue the timer should reside on. Valid values are:

        TimerQueueSoft - The timer will be expired at the first applicable
            clock interrupt, but a clock interrupt will not be scheduled solely
            for this timer. This timer type has the best power management
            profile, but may cause the expiration of the timer to be fairly
            late, as the system will not come out of idle to service this timer.
            The DPC for this timer may run on any processor.

        TimerQueueSoftWake - The timer will be expired at the first applicable
            clock interrupt. If the system was otherwise idle, a clock
            interrupt will be scheduled for this timer. This is a balanced
            choice for timers that can have some slack in their expiration, but
            need to run approximately when scheduled, even if the system is
            idle. The DPC will run on the processor where the timer was queued.

        TimerQueueHard - A clock interrupt will be scheduled for exactly the
            specified deadline. This is the best choice for high performance
            timers that need to expire as close to their deadlines as possible.
            It is the most taxing on power management, as it pulls the system
            out of idle, schedules an extra clock interrupt, and requires
            programming hardware. The DPC will run on the processor where the
            timer was queued.

    DueTime - Supplies the value of the time tick counter when this timer
        should expire (an absolute value in time counter ticks). If this value
        is 0, then an automatic due time of the current time plus the given
        period will be computed.

    Period - Supplies an optional period, in time counter ticks, for periodic
        timers. If this value is non-zero, the period will be added to the
        original due time and the timer will be automatically rearmed.

    Flags - Supplies an optional bitfield of flags. See KTIMER_FLAG_*
        definitions.

    Dpc - Supplies an optional pointer to a DPC that will be queued when this
        timer expires.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
KeCancelTimer (
    PKTIMER Timer
    );

/*++

Routine Description:

    This routine attempts to cancel a queued timer. This routine must be called
    at or below dispatch level. This routine will ensure that the DPC
    associated with the timer will have either been fully queued or not queued
    by the time this function returns, even if the timer was too late to
    cancel.

Arguments:

    Timer - Supplies a pointer to the timer to cancel.

Return Value:

    STATUS_SUCCESS if the timer was successfully cancelled.

    STATUS_TOO_LATE if the timer expired before the timer queue could be
    accessed.

--*/

KERNEL_API
VOID
KeSignalTimer (
    PKTIMER Timer,
    SIGNAL_OPTION Option
    );

/*++

Routine Description:

    This routine sets a timer to the given signal state.

Arguments:

    Timer - Supplies a pointer to the timer to signal or unsignal.

    Option - Supplies the signaling behavior to apply.

Return Value:

    None.

--*/

KERNEL_API
SIGNAL_STATE
KeGetTimerState (
    PKTIMER Timer
    );

/*++

Routine Description:

    This routine returns the signal state of a timer.

Arguments:

    Timer - Supplies a pointer to the timer to get the state of.

Return Value:

    Returns the signal state of the timer.

--*/

KERNEL_API
ULONGLONG
KeGetTimerDueTime (
    PKTIMER Timer
    );

/*++

Routine Description:

    This routine returns the next due time of the given timer. This could be in
    the past. This routine must be called at or below dispatch level.

Arguments:

    Timer - Supplies a pointer to the timer to query.

Return Value:

    Returns the due time of the timer.

    0 if the timer is not currently queued.

--*/

KERNEL_API
ULONGLONG
KeConvertMicrosecondsToTimeTicks (
    ULONGLONG Microseconds
    );

/*++

Routine Description:

    This routine converts the given number of microseconds into time counter
    ticks.

Arguments:

    Microseconds - Supplies the microsecond count.

Return Value:

    Returns the number of time ticks that correspond to the given number of
    microseconds.

--*/

KERNEL_API
ULONGLONG
KeGetRecentTimeCounter (
    VOID
    );

/*++

Routine Description:

    This routine returns a relatively recent snap of the time counter.

Arguments:

    None.

Return Value:

    Returns the fairly recent snap of the time counter.

--*/

KERNEL_API
KSTATUS
KeGetSetSystemInformation (
    SYSTEM_INFORMATION_SUBSYSTEM Subsystem,
    UINTN InformationType,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

/*++

Routine Description:

    This routine gets or sets system information.

Arguments:

    Subsystem - Supplies the subsystem to query or set information of.

    InformationType - Supplies the information type, which is specific to
        the subsystem. The type of this value is generally
        <subsystem>_INFORMATION_TYPE (eg IO_INFORMATION_TYPE).

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    STATUS_SUCCESS if the information was successfully queried or set.

    STATUS_BUFFER_TOO_SMALL if the buffer size specified was too small. The
    required buffer size will be returned in the data size parameter.

    STATUS_DATA_LENGTH_MISMATCH if the buffer size was not correct. The
    correct buffer size will be returned in the data size parameter.

    STATUS_INVALID_PARAMETER if the given subsystem or information type is
    not known.

    Other status codes on other failures.

--*/

KERNEL_API
PKERNEL_ARGUMENT
KeGetKernelArgument (
    PKERNEL_ARGUMENT Start,
    PCSTR Component,
    PCSTR Name
    );

/*++

Routine Description:

    This routine looks up a kernel command line argument.

Arguments:

    Start - Supplies an optional pointer to the previous command line argument
        to start from. Supply NULL here initially.

    Component - Supplies a pointer to the component string to look up.

    Name - Supplies a pointer to the argument name to look up.

Return Value:

    Returns a pointer to a matching kernel argument on success.

    NULL if no argument could be found.

--*/

KERNEL_API
KSTATUS
KeResetSystem (
    SYSTEM_RESET_TYPE ResetType
    );

/*++

Routine Description:

    This routine attempts to reboot the system. This routine must be called
    from low level.

Arguments:

    ResetType - Supplies the desired system reset type. If the given type is
        not supported and a cold reset is, then a cold reset will be
        performed.

Return Value:

    Does not return on success, the system is reset.

    STATUS_INVALID_PARAMETER if an invalid reset type was supplied.

    STATUS_NOT_SUPPORTED if the system cannot be reset.

    STATUS_UNSUCCESSFUL if the system did not reset.

--*/

INTN
KeSysGetSetSystemInformation (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine implements the user mode system call for getting and setting
    system information.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
KeSysResetSystem (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine implements the system call for resetting the system.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This stores the system reset type. It is passed to the
        kernel in a register.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

PPROCESSOR_BLOCK
KeGetCurrentProcessorBlock (
    VOID
    );

/*++

Routine Description:

    This routine gets the processor state for the currently executing processor.

Arguments:

    None.

Return Value:

    Returns the current processor block.

--*/

PPROCESSOR_BLOCK
KeGetCurrentProcessorBlockForDebugger (
    VOID
    );

/*++

Routine Description:

    This routine gets the processor block for the currently executing
    processor. It is intended to be called only by the debugger.

Arguments:

    None.

Return Value:

    Returns the current processor block.

--*/

ULONG
KeGetCurrentProcessorNumber (
    VOID
    );

/*++

Routine Description:

    This routine gets the processor number for the currently executing
    processor.

Arguments:

    None.

Return Value:

    Returns the current zero-indexed processor number.

--*/

ULONG
KeGetActiveProcessorCount (
    VOID
    );

/*++

Routine Description:

    This routine gets the number of processors currently running in the
    system.

Arguments:

    None.

Return Value:

    Returns the number of active processors currently in the system.

--*/

PKTHREAD
KeGetCurrentThread (
    VOID
    );

/*++

Routine Description:

    This routine gets the current thread running on this processor.

Arguments:

    None.

Return Value:

    Returns the current run level.

--*/

KERNEL_API
RUNLEVEL
KeRaiseRunLevel (
    RUNLEVEL RunLevel
    );

/*++

Routine Description:

    This routine raises the running level of the current processor to the given
    level.

Arguments:

    RunLevel - Supplies the new running level of the current processor.

Return Value:

    Returns the old running level of the processor.

--*/

KERNEL_API
VOID
KeLowerRunLevel (
    RUNLEVEL RunLevel
    );

/*++

Routine Description:

    This routine lowers the running level of the current processor to the given
    level.

Arguments:

    RunLevel - Supplies the new running level of the current processor.

Return Value:

    None.

--*/

KERNEL_API
PKEVENT
KeCreateEvent (
    PVOID ParentObject
    );

/*++

Routine Description:

    This routine creates a kernel event. It comes initialized to Not Signaled.

Arguments:

    ParentObject - Supplies an optional parent object to create the event
        under.

Return Value:

    Returns a pointer to the event, or NULL if the event could not be created.

--*/

KERNEL_API
VOID
KeDestroyEvent (
    PKEVENT Event
    );

/*++

Routine Description:

    This routine destroys an event created with KeCreateEvent. The event is no
    longer valid after this call.

Arguments:

    Event - Supplies a pointer to the event to free.

Return Value:

    None.

--*/

KERNEL_API
KSTATUS
KeWaitForEvent (
    PKEVENT Event,
    BOOL Interruptible,
    ULONG TimeoutInMilliseconds
    );

/*++

Routine Description:

    This routine waits until an event enters a signaled state.

Arguments:

    Event - Supplies a pointer to the event to wait for.

    Interruptible - Supplies a boolean indicating whether or not the wait can
        be interrupted if a signal is sent to the process on which this thread
        runs. If TRUE is supplied, the caller must check the return status
        code to find out if the wait was really satisfied or just interrupted.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the given
        objects should be waited on before timing out. Use WAIT_TIME_INDEFINITE
        to wait forever on these objects.

Return Value:

    Status code.

--*/

KERNEL_API
VOID
KeSignalEvent (
    PKEVENT Event,
    SIGNAL_OPTION Option
    );

/*++

Routine Description:

    This routine sets an event to the given signal state.

Arguments:

    Event - Supplies a pointer to the event to signal or unsignal.

    Option - Supplies the signaling behavior to apply.

Return Value:

    None.

--*/

KERNEL_API
SIGNAL_STATE
KeGetEventState (
    PKEVENT Event
    );

/*++

Routine Description:

    This routine returns the signal state of an event.

Arguments:

    Event - Supplies a pointer to the event to get the state of.

Return Value:

    Returns the signal state of the event.

--*/

KERNEL_API
PWORK_QUEUE
KeCreateWorkQueue (
    ULONG Flags,
    PCSTR Name
    );

/*++

Routine Description:

    This routine creates a new work queue.

Arguments:

    Flags - Supplies a bitfield of flags governing the behavior of the work
        queue. See WORK_QUEUE_FLAG_* definitions.

    Name - Supplies an optional pointer to the name of the worker threads
        created. A copy of this memory will be made. This should only be used
        for debugging, as text may be added to the end of the name supplied
        here to the actual worker thread names.

Return Value:

    Returns a pointer to the new work queue on success.

    NULL on failure.

--*/

KERNEL_API
VOID
KeDestroyWorkQueue (
    PWORK_QUEUE WorkQueue
    );

/*++

Routine Description:

    This routine destroys a work queue. If there are items on the work queue,
    they will be completed.

Arguments:

    WorkQueue - Supplies a pointer to the work queue to destroy.

Return Value:

    None.

--*/

KERNEL_API
VOID
KeFlushWorkQueue (
    PWORK_QUEUE WorkQueue
    );

/*++

Routine Description:

    This routine flushes a work queue. If there are items on the work queue,
    they will be completed before this routine returns.

Arguments:

    WorkQueue - Supplies a pointer to the work queue to flush.

Return Value:

    None.

--*/

KERNEL_API
PWORK_ITEM
KeCreateWorkItem (
    PWORK_QUEUE WorkQueue,
    WORK_PRIORITY Priority,
    PWORK_ITEM_ROUTINE WorkRoutine,
    PVOID Parameter,
    ULONG AllocationTag
    );

/*++

Routine Description:

    This routine creates a new reusable work item.

Arguments:

    WorkQueue - Supplies a pointer to the queue this work item will
        eventually be queued to. Supply NULL to use the system work queue.

    Priority - Supplies the work priority.

    WorkRoutine - Supplies the routine to execute to does the work. This
        routine should be prepared to take one parameter.

    Parameter - Supplies an optional parameter to pass to the worker routine.

    AllocationTag - Supplies an allocation tag to associate with the work item.

Return Value:

    Returns a pointer to the new work item on success.

    NULL on failure.

--*/

KERNEL_API
VOID
KeDestroyWorkItem (
    PWORK_ITEM WorkItem
    );

/*++

Routine Description:

    This routine destroys a reusable work item. If this is a work item that
    can re-queue itself, then the caller needs to make sure that that can no
    longer happen before trying to destroy the work item.

Arguments:

    WorkItem - Supplies a pointer to the work item.

Return Value:

    None.

--*/

KERNEL_API
KSTATUS
KeCancelWorkItem (
    PWORK_ITEM WorkItem
    );

/*++

Routine Description:

    This routine attempts to cancel the work item. If the work item is still on
    its work queue them this routine will pull it off and return successfully.
    Otherwise the work item may have been selected to run and this routine will
    return that the cancel was too late. Keep in mind that "too late" may also
    mean "too early" if the work item was never queued.

Arguments:

    WorkItem - Supplies a pointer to the work item to cancel.

Return Value:

    Status code.

--*/

KERNEL_API
VOID
KeFlushWorkItem (
    PWORK_ITEM WorkItem
    );

/*++

Routine Description:

    This routine does not return until the given work item has completed.

Arguments:

    WorkItem - Supplies a pointer to the work item.

Return Value:

    None.

--*/

KERNEL_API
VOID
KeSetWorkItemParameters (
    PWORK_ITEM WorkItem,
    WORK_PRIORITY Priority,
    PWORK_ITEM_ROUTINE WorkRoutine,
    PVOID Parameter
    );

/*++

Routine Description:

    This routine resets the parameters of a work item to the given parameters.
    The work item must not be queued. This routine must be called at or below
    dispatch level.

Arguments:

    WorkItem - Supplies a pointer to the work item to modify.

    Priority - Supplies the new work priority.

    WorkRoutine - Supplies the routine to execute to does the work. This
        routine should be prepared to take one parameter.

    Parameter - Supplies an optional parameter to pass to the worker routine.

Return Value:

    None.

--*/

KERNEL_API
KSTATUS
KeQueueWorkItem (
    PWORK_ITEM WorkItem
    );

/*++

Routine Description:

    This routine queues a work item onto the work queue for execution as soon
    as possible. This routine must be called from dispatch level or below.

Arguments:

    WorkItem - Supplies a pointer to the work item to queue.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_RESOURCE_IN_USE if the work item is already queued.

--*/

KERNEL_API
KSTATUS
KeCreateAndQueueWorkItem (
    PWORK_QUEUE WorkQueue,
    WORK_PRIORITY Priority,
    PWORK_ITEM_ROUTINE WorkRoutine,
    PVOID Parameter
    );

/*++

Routine Description:

    This routine creates and queues a work item. This work item will get
    executed in a worker thread an arbitrary amount of time later. The work item
    will be automatically freed after the work routine is executed.

Arguments:

    WorkQueue - Supplies a pointer to the queue this work item will
        eventually be queued to. Supply NULL to use the system work queue.

    Priority - Supplies the work priority.

    WorkRoutine - Supplies the routine to execute to doe the work. This
        routine should be prepared to take one parameter.

    Parameter - Supplies an optional parameter to pass to the worker routine.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_UNSUCCESSFUL on failure.

--*/

KERNEL_API
KSTATUS
KeGetRandomBytes (
    PVOID Buffer,
    UINTN Size
    );

/*++

Routine Description:

    This routine returns pseudo-random bytes from the system's random source.

Arguments:

    Buffer - Supplies a pointer where the random bytes will be returned on
        success.

    Size - Supplies the number of bytes of random data to get.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_SUCH_DEVICE if no pseudo-random interface is present.

--*/

INTN
KeSysDelayExecution (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine implements the system call for delaying execution of the
    current thread by a specified amount of time.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
KeSysSetSystemTime (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine implements the system call for setting the system time.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

VOID
KeClockInterrupt (
    VOID
    );

/*++

Routine Description:

    This routine handles periodic clock interrupts, updating system time and
    providing pre-emptive scheduling.

Arguments:

    None.

Return Value:

    None.

--*/

ULONG
KeGetClockInterruptCount (
    ULONG ProcessorNumber
    );

/*++

Routine Description:

    This routine returns the clock interrupt count of the given processor.

Arguments:

    ProcessorNumber - Supplies the number of the processor whose clock
        interrupt count is to be returned.

Return Value:

    Returns the given processor's clock interrupt count.

--*/

VOID
KeUpdateClockForProfiling (
    BOOL ProfilingEnabled
    );

/*++

Routine Description:

    This routine configures the clock interrupt handler for profiling.

Arguments:

    ProfilingEnabled - Supplies a boolean indicating if profiling is being
        enabled (TRUE) or disabled (FALSE).

Return Value:

    None.

--*/

VOID
KeDispatchSoftwareInterrupt (
    RUNLEVEL RunLevel,
    PTRAP_FRAME TrapFrame
    );

/*++

Routine Description:

    This routine handles a software interrupt. Consider it the ISR for
    software interrupts. On entry, interrupts are disabled. This routine may
    enable interrupts, but must exit with the interrupts disabled.

Arguments:

    RunLevel - Supplies the run level that that interrupt occurred on.

    TrapFrame - Supplies an optional pointer to the trap frame if this interrupt
        is being dispatched off a hardware interrupt. Supplying this variable
        enables checking for any pending user-mode signals.

Return Value:

    None.

--*/

PPROCESSOR_BLOCK
KeGetProcessorBlock (
    ULONG ProcessorNumber
    );

/*++

Routine Description:

    This routine returns the processor block for the given processor number.

Arguments:

    ProcessorNumber - Supplies the number of the processor.

Return Value:

    Returns the processor block for the given processor.

    NULL if the input was not a valid processor number.

--*/

KSTATUS
KeSendIpi (
    PIPI_ROUTINE IpiRoutine,
    PVOID IpiContext,
    PPROCESSOR_SET Processors
    );

/*++

Routine Description:

    This routine runs the given routine at IPI level on the specified set of
    processors. This routine runs synchronously: the routine will have completed
    running on all processors by the time this routine returns. This routine
    must be called at or below dispatch level.

Arguments:

    IpiRoutine - Supplies a pointer to the routine to run at IPI level.

    IpiContext - Supplies the value to pass to the IPI routine as a parameter.

    Processors - Supplies the set of processors to run the IPI on.

Return Value:

    Status code.

--*/

KERNEL_API
VOID
KeYield (
    VOID
    );

/*++

Routine Description:

    This routine yields the current thread's execution. The thread remains in
    the ready state, and may not actually be scheduled out if no other threads
    are ready.

Arguments:

    None.

Return Value:

    None.

--*/

KERNEL_API
VOID
KeGetSystemTime (
    PSYSTEM_TIME Time
    );

/*++

Routine Description:

    This routine returns the current system time.

Arguments:

    Time - Supplies a pointer where the system time will be returned.

Return Value:

    None.

--*/

VOID
KeGetHighPrecisionSystemTime (
    PSYSTEM_TIME Time
    );

/*++

Routine Description:

    This routine returns a high precision snap of the current system time.

Arguments:

    Time - Supplies a pointer that receives the precise system time.

Return Value:

    None.

--*/

KSTATUS
KeSetSystemTime (
    PSYSTEM_TIME NewTime,
    ULONGLONG TimeCounter
    );

/*++

Routine Description:

    This routine sets the system time.

Arguments:

    NewTime - Supplies a pointer to the new system time to set.

    TimeCounter - Supplies the time counter value corresponding with the
        moment the new system time was meant to be set by the caller.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
KeDelayExecution (
    BOOL Interruptible,
    BOOL TimeTicks,
    ULONGLONG Interval
    );

/*++

Routine Description:

    This routine blocks the current thread for the specified amount of time.
    This routine can only be called at low level.

Arguments:

    Interruptible - Supplies a boolean indicating if the wait can be
        interrupted by a dispatched signal. If TRUE, the caller must check the
        return status code to see if the wait expired or was interrupted.

    TimeTicks - Supplies a boolean indicating if the interval parameter is
        represented in time counter ticks (TRUE) or microseconds (FALSE).

    Interval - Supplies the interval to wait. If the time ticks parameter is
        TRUE, this parameter represents an absolute time in time counter ticks.
        If the time ticks parameter is FALSE, this parameter represents a
        relative time from now in microseconds. If an interval of 0 is
        supplied, this routine is equivalent to KeYield.

Return Value:

    STATUS_SUCCESS if the wait completed.

    STATUS_INTERRUPTED if the wait was interrupted.

--*/

KERNEL_API
KSTATUS
KeGetProcessorCycleAccounting (
    ULONG ProcessorNumber,
    PPROCESSOR_CYCLE_ACCOUNTING Accounting
    );

/*++

Routine Description:

    This routine returns a snapshot of the given processor's cycle accounting
    information.

Arguments:

    ProcessorNumber - Supplies the processor number to query.

    Accounting - Supplies a pointer where the processor accounting information
        will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if an invalid processor number was supplied.

--*/

KERNEL_API
VOID
KeGetTotalProcessorCycleAccounting (
    PPROCESSOR_CYCLE_ACCOUNTING Accounting
    );

/*++

Routine Description:

    This routine returns a snapshot of the accumulation of all processors'
    cycle accounting information.

Arguments:

    Accounting - Supplies a pointer where the processor accounting information
        will be returned.

Return Value:

    None.

--*/

VOID
KeSchedulerEntry (
    SCHEDULER_REASON Reason
    );

/*++

Routine Description:

    This routine serves as the entry point to the thread scheduler. It may
    decide to schedule a new thread or simply return.

Arguments:

    Reason - Supplies the scheduler with the reason why it's being called (ie
        run-level lowering, the thread is waiting, exiting, etc).

Return Value:

    None.

--*/

VOID
KeSetThreadReady (
    PKTHREAD Thread
    );

/*++

Routine Description:

    This routine unblocks a previously blocked thread and adds it to the
    ready queue.

Arguments:

    Thread - Supplies a pointer to the blocked thread.

Return Value:

    None.

--*/

VOID
KeSuspendExecution (
    VOID
    );

/*++

Routine Description:

    This routine suspends execution of the current thread until such time as
    another thread wakes it (usually because of a user mode signal).

Arguments:

    None.

Return Value:

    None. The function returns when another thread has woken this thread.

--*/

VOID
KeUnlinkSchedulerEntry (
    PSCHEDULER_ENTRY Entry
    );

/*++

Routine Description:

    This routine unlinks a scheduler entry from its parent group.

Arguments:

    Entry - Supplies a pointer to the entry to be unlinked.

Return Value:

    None.

--*/

VOID
KeIdleLoop (
    VOID
    );

/*++

Routine Description:

    This routine executes the idle loop. It does not return. It can be
    executed only from the idle thread.

Arguments:

    None.

Return Value:

    None.

--*/

CYCLE_ACCOUNT
KeBeginCycleAccounting (
    CYCLE_ACCOUNT CycleAccount
    );

/*++

Routine Description:

    This routine begins a new period of cycle accounting for the current
    processor.

Arguments:

    CycleAccount - Supplies the type of time to attribute these cycles to.

Return Value:

    Returns the previous type that cycles were being attributed to.

--*/

VOID
KeRegisterCrashDumpFile (
    HANDLE Handle,
    BOOL Register
    );

/*++

Routine Description:

    This routine registers a file for use as a crash dump file.

Arguments:

    Handle - Supplies a handle to the page file to register.

    Register - Supplies a boolean indicating if the page file is registering
        (TRUE) or de-registering (FALSE).

Return Value:

    None.

--*/

//
// Video printing routines
//

VOID
KeVideoPrintString (
    ULONG XCoordinate,
    ULONG YCoordinate,
    PCSTR String
    );

/*++

Routine Description:

    This routine prints a null-terminated string to the screen at the
    specified location.

Arguments:

    Context - Supplies a pointer to the initialized base video context.

    XCoordinate - Supplies the X coordinate of the location on the screen
        to write to.

    YCoordinate - Supplies the Y cooordinate of the location on the screen
        to write to.

    String - Supplies the string to print.

Return Value:

    None.

--*/

VOID
KeVideoPrintHexInteger (
    ULONG XCoordinate,
    ULONG YCoordinate,
    ULONG Number
    );

/*++

Routine Description:

    This routine prints an integer to the screen in the specified location.

Arguments:

    XCoordinate - Supplies the X coordinate of the location on the screen
        to write to.

    YCoordinate - Supplies the Y cooordinate of the location on the screen
        to write to.

    Number - Supplies the signed integer to print.

Return Value:

    None.

--*/

VOID
KeVideoPrintInteger (
    ULONG XCoordinate,
    ULONG YCoordinate,
    LONG Number
    );

/*++

Routine Description:

    This routine prints an integer to the screen in the specified location.

Arguments:

    Context - Supplies a pointer to the initialized base video context.

    XCoordinate - Supplies the X coordinate of the location on the screen
        to write to.

    YCoordinate - Supplies the Y cooordinate of the location on the screen
        to write to.

    Number - Supplies the signed integer to print.

Return Value:

    None.

--*/

VOID
KeVideoClearScreen (
    LONG MinimumX,
    LONG MinimumY,
    LONG MaximumX,
    LONG MaximumY
    );

/*++

Routine Description:

    This routine clears a portion of the video screen.

Arguments:

    MinimumX - Supplies the minimum X coordinate of the rectangle to clear,
        inclusive.

    MinimumY - Supplies the minimum Y coordinate of the rectangle to clear,
        inclusive.

    MaximumX - Supplies the maximum X coordinate of the rectangle to clear,
        exclusive.

    MaximumY - Supplies the maximum Y coordinate of the rectangle to clear,
        exclusive.

Return Value:

    None.

--*/

KSTATUS
KeVideoGetDimensions (
    PULONG Width,
    PULONG Height,
    PULONG CellWidth,
    PULONG CellHeight,
    PULONG Columns,
    PULONG Rows
    );

/*++

Routine Description:

    This routine returns the dimensions of the kernel's video frame buffer.

Arguments:

    Width - Supplies an optional pointer where the width in pixels will be
        returned. For text-based frame buffers, this will be equal to the
        number of text columns.

    Height - Supplies an optional pointer where the height in pixels will be
        returned. For text-based frame buffers, this will be equal to the
        number of text rows.

    CellWidth - Supplies an optional pointer where the width in pixels of a
        text character will be returned on success. For text-based frame
        buffers, 1 will be returned.

    CellHeight - Supplies an optional pointer where the height in pixels of a
        text character will be returned on success. For text-based frame
        buffers, 1 will be returned.

    Columns - Supplies an optional pointer where the number of text columns
        will be returned.

    Rows - Supplies an optional pointer where the number of text rows will be
        returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_INITIALIZED if there is no frame buffer.

--*/

INTN
KeSystemCallHandler (
    ULONG SystemCallNumber,
    PVOID SystemCallParameter,
    PTRAP_FRAME TrapFrame
    );

/*++

Routine Description:

    This routine responds to requests from user mode entered via a system call.
    It may also be called by the restore system call in order to restart a
    system call. This should not be seen as a general way to invoke system call
    behavior from inside the kernel.

Arguments:

    SystemCallNumber - Supplies the system call number.

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call.

    TrapFrame - Supplies a pointer to the trap frame generated by this jump
        from user mode to kernel mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

