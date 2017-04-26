/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    init.c

Abstract:

    This module implements the kernel system startup.

Author:

    Evan Green 2-Jul-2012

Environment:

    Kernel Initialization

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/lib/bconf.h>
#include <minoca/kernel/bootload.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// The first row has at max 11 * 4 = 44 characters of value.
// The second row has at max 13 + (4 * 4) + 11 + 11 = 51 characters of value.
//

#define KE_BANNER_FULL_WIDTH 116
#define KE_BANNER_FULL_MEMORY_FORMAT \
    "Memory Used/Total: %s   Paged Pool: %s   Non-Paged Pool: %s   Cache: %s"

#define KE_BANNER_FULL_TIME_FORMAT \
    "Uptime: %s  CPU User: %s  Kernel: %s  Interrupt: %s  Idle: %s   IO: %s%s"

#define KE_BANNER_FULL_PAGING_FORMAT "   Pg: %s"

#define KE_BANNER_SHORT_WIDTH 80
#define KE_BANNER_SHORT_MEMORY_FORMAT \
    "Memory: %s Paged: %s Non-paged: %s Cache: %s"

#define KE_BANNER_SHORT_TIME_FORMAT \
    "%s U: %s K: %s In: %s Id: %s IO: %s%s"

#define KE_BANNER_SHORT_PAGING_FORMAT " Pg: %s"

#define KE_BANNER_TINY_WIDTH 40
#define KE_BANNER_TINY_MEMORY_FORMAT "Memory: %s Cache: %s"
#define KE_BANNER_TINY_TIME_FORMAT \
    "%s U%s K%s IO:%s"

#define KE_BANNER_TINY_PAGING_FORMAT ""

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _KERNEL_SUBSYSTEM {
    KernelSubsystemInvalid,
    KernelSubsystemKernelDebugger,
    KernelSubsystemKernelExecutive,
    KernelSubsystemMemoryManager,
    KernelSubsystemObjectManager,
    KernelSubsystemAcpi,
    KernelSubsystemHardwareLayer,
    KernelSubsystemProcess,
    KernelSubsystemInputOutput,
    KernelSubsystemProfiler
} KERNEL_SUBSYSTEM, *PKERNEL_SUBSYSTEM;

typedef struct _SYSTEM_USAGE_CONTEXT {
    ULONGLONG TimeCounter;
    ULONGLONG TimeCounterFrequency;
    ULONGLONG CycleCounterFrequency;
    ULONGLONG UserCycles;
    ULONGLONG KernelCycles;
    ULONGLONG InterruptCycles;
    ULONGLONG IdleCycles;
    ULONGLONG TotalCycles;
    ULONG UserPercent;
    ULONG KernelPercent;
    ULONG InterruptPercent;
    ULONG IdlePercent;
} SYSTEM_USAGE_CONTEXT, *PSYSTEM_USAGE_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
KepApplicationProcessorStartup (
    PPROCESSOR_START_BLOCK StartBlock
    );

VOID
KepCompleteSystemInitialization (
    PVOID Parameter
    );

VOID
KepAcquireProcessorStartLock (
    VOID
    );

VOID
KepReleaseProcessorStartLock (
    VOID
    );

VOID
KepBannerThread (
    PVOID Context
    );

VOID
KepUpdateSystemUsage (
    PSYSTEM_USAGE_CONTEXT Context
    );

VOID
KepPrintFormattedMemoryUsage (
    PCHAR String,
    ULONG StringSize,
    ULONGLONG UsedValue,
    ULONGLONG TotalValue
    );

ULONG
KepPrintFormattedSize (
    PCHAR String,
    ULONG StringSize,
    ULONGLONG Value
    );

ULONG
KepPrintFormattedPercent (
    PCHAR String,
    ULONG StringSize,
    ULONG PercentTimesTen
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define a lock used to serializes parts of the AP startup execution.
//

volatile ULONG KeProcessorStartLock = 0;
volatile ULONG KeProcessorsReady = 0;
volatile BOOL KeAllProcessorsInitialize = FALSE;
volatile BOOL KeAllProcessorsGo = FALSE;

//
// Odd values are off, even values are on.
//

volatile ULONG KeBannerThreadEnabled = 1;

//
// ------------------------------------------------------------------ Functions
//

__USED
VOID
KepStartSystem (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine implements the first function called in the kernel from the
    boot loader.

Arguments:

    Parameters - Supplies information about the system and memory layout as
        set up by the loader.

Return Value:

    This function does not return.

--*/

{

    PBOOT_ENTRY BootEntry;
    PDEBUG_DEVICE_DESCRIPTION DebugDevice;
    KERNEL_SUBSYSTEM FailingSubsystem;
    ULONG ProcessorCount;
    KSTATUS Status;

    DebugDevice = NULL;
    FailingSubsystem = KernelSubsystemInvalid;

    //
    // Perform very basic processor initialization, preparing it to take
    // exceptions and use the serial port.
    //

    ArInitializeProcessor(FALSE, NULL);
    AcpiInitializePreDebugger(Parameters);
    Status = MmInitialize(Parameters, NULL, 0);
    if (!KSUCCESS(Status)) {
        FailingSubsystem = KernelSubsystemMemoryManager;
        goto StartSystemEnd;
    }

    HlInitializePreDebugger(Parameters, 0, &DebugDevice);

    //
    // Initialize the debugging subsystem.
    //

    BootEntry = Parameters->BootEntry;
    if ((BootEntry != NULL) &&
        ((BootEntry->Flags & BOOT_ENTRY_FLAG_DEBUG) != 0)) {

        Status = KdInitialize(DebugDevice, Parameters->KernelModule);
        if (!KSUCCESS(Status)) {
            FailingSubsystem = KernelSubsystemKernelDebugger;
            goto StartSystemEnd;
        }
    }

    //
    // Initialize the kernel executive.
    //

    Status = KeInitialize(0, Parameters);
    if (!KSUCCESS(Status)) {
        FailingSubsystem = KernelSubsystemKernelExecutive;
        goto StartSystemEnd;
    }

    //
    // Perform phase 1 MM Initialization.
    //

    Status = MmInitialize(Parameters, NULL, 1);
    if (!KSUCCESS(Status)) {
        FailingSubsystem = KernelSubsystemMemoryManager;
        goto StartSystemEnd;
    }

    //
    // Initialize the Object Manager.
    //

    Status = ObInitialize();
    if (!KSUCCESS(Status)) {
        FailingSubsystem = KernelSubsystemObjectManager;
        goto StartSystemEnd;
    }

    //
    // Perform phase 1 executive initialization, which sets up primitives like
    // queued locks and events.
    //

    Status = KeInitialize(1, Parameters);
    if (!KSUCCESS(Status)) {
        FailingSubsystem = KernelSubsystemKernelExecutive;
        goto StartSystemEnd;
    }

    //
    // Initialize ACPI.
    //

    Status = AcpiInitialize(Parameters);
    if (!KSUCCESS(Status)) {
        FailingSubsystem = KernelSubsystemAcpi;
        goto StartSystemEnd;
    }

    //
    // Initialize the hardware layer.
    //

    Status = HlInitialize(Parameters, 0);
    if (!KSUCCESS(Status)) {
        FailingSubsystem = KernelSubsystemHardwareLayer;
        goto StartSystemEnd;
    }

    //
    // Initialize the process and thread subsystem.
    //

    Status = PsInitialize(0,
                          Parameters,
                          Parameters->KernelStack.Buffer,
                          Parameters->KernelStack.Size);

    if (!KSUCCESS(Status)) {
        FailingSubsystem = KernelSubsystemProcess;
        goto StartSystemEnd;
    }

    //
    // Perform phase 1 hardware layer initialization. The scheduler becomes
    // active at this point.
    //

    Status = HlInitialize(Parameters, 1);
    if (!KSUCCESS(Status)) {
        FailingSubsystem = KernelSubsystemHardwareLayer;
        goto StartSystemEnd;
    }

    //
    // Now that the system is multithreaded, lock down MM.
    //

    Status = MmInitialize(Parameters, NULL, 2);
    if (!KSUCCESS(Status)) {
        FailingSubsystem = KernelSubsystemMemoryManager;
        goto StartSystemEnd;
    }

    //
    // Perform additional process initialization now that MM is fully up.
    //

    Status = PsInitialize(1, Parameters, NULL, 0);
    if (!KSUCCESS(Status)) {
        FailingSubsystem = KernelSubsystemProcess;
        goto StartSystemEnd;
    }

    //
    // Start all processors. Wait for all processors to initialize before
    // allowing the debugger to start broadcasting NMIs.
    //

    Status = HlStartAllProcessors(KepApplicationProcessorStartup,
                                  &ProcessorCount);

    if (!KSUCCESS(Status)) {
        FailingSubsystem = KernelSubsystemHardwareLayer;
        goto StartSystemEnd;
    }

    KeAllProcessorsInitialize = TRUE;
    RtlAtomicAdd32(&KeProcessorsReady, 1);
    while (KeProcessorsReady != ProcessorCount) {
        ArProcessorYield();
    }

    KdEnableNmiBroadcast(TRUE);

    //
    // Perform phase 2 executive initialization, which creates things like the
    // worker threads.
    //

    Status = KeInitialize(2, Parameters);
    if (!KSUCCESS(Status)) {
        FailingSubsystem = KernelSubsystemKernelExecutive;
        goto StartSystemEnd;
    }

    //
    // Initialize the system profiler subsystem, which will start profiling
    // only if early profiling is enabled.
    //

    Status = SpInitializeProfiler();
    if (!KSUCCESS(Status)) {
        FailingSubsystem = KernelSubsystemProfiler;
        goto StartSystemEnd;
    }

    //
    // Create a thread to continue system initialization that may involve
    // blocking, letting this thread become the idle thread. After this point,
    // the idle thread really is the idle thread.
    //

    Status = PsCreateKernelThread(KepCompleteSystemInitialization,
                                  Parameters,
                                  "Init");

    if (!KSUCCESS(Status)) {
        goto StartSystemEnd;
    }

    //
    // Boot mappings will be freed by the thread just kicked off, so the
    // parameters are now untouchable.
    //

    Parameters = NULL;

StartSystemEnd:
    if (!KSUCCESS(Status)) {
        KeVideoPrintString(0, 14, "Kernel Failure: 0x");
        KeVideoPrintHexInteger(18, 14, Status);
        KeVideoPrintString(0, 15, "Subsystem: ");
        KeVideoPrintInteger(11, 15, FailingSubsystem);
        KeCrashSystem(CRASH_SYSTEM_INITIALIZATION_FAILURE,
                      FailingSubsystem,
                      Status,
                      0,
                      0);
    }

    //
    // Drop into the idle loop.
    //

    KeIdleLoop();
    return;
}

VOID
KepApplicationProcessorStartup (
    PPROCESSOR_START_BLOCK StartBlock
    )

/*++

Routine Description:

    This routine implements the main initialization routine for processors
    other than P0.

Arguments:

    StartBlock - Supplies a pointer to the processor start block that contains
        this processor's initialization information.

Return Value:

    This function does not return, this thread eventually becomes the idle
    thread.

--*/

{

    KSTATUS Status;

    //
    // Mark the core as started.
    //

    StartBlock->Started = TRUE;
    RtlMemoryBarrier();

    //
    // Wait here until P0 says it's okay to initialize. This barrier allows
    // all processors to get out of the stub code as quickly as possible and
    // not have to worry about contending for non-paged pool locks while
    // allocating an idle stack.
    //

    while (KeAllProcessorsInitialize == FALSE) {
        ArProcessorYield();
    }

    KepAcquireProcessorStartLock();
    ArInitializeProcessor(FALSE, StartBlock->ProcessorStructures);

    //
    // Initialize the kernel executive.
    //

    Status = KeInitialize(0, NULL);
    if (!KSUCCESS(Status)) {
        goto ApplicationProcessorStartupEnd;
    }

    //
    // Perform phase 1 MM Initialization.
    //

    Status = MmInitialize(NULL, StartBlock, 1);
    if (!KSUCCESS(Status)) {
        goto ApplicationProcessorStartupEnd;
    }

    //
    // Perform phase 1 executive initialization.
    //

    Status = KeInitialize(1, NULL);
    if (!KSUCCESS(Status)) {
        goto ApplicationProcessorStartupEnd;
    }

    //
    // Initialize the hardware layer. The clock interrupt becomes active at
    // this point. As a result, this routine raises the run level from low to
    // dispatch to prevent the scheduler from becoming active before the
    // process and thread subsystem is initialized.
    //

    Status = HlInitialize(NULL, 0);
    if (!KSUCCESS(Status)) {
        goto ApplicationProcessorStartupEnd;
    }

    //
    // Initialize the process and thread subsystem.
    //

    Status = PsInitialize(0,
                          NULL,
                          StartBlock->StackBase,
                          StartBlock->StackSize);

    if (!KSUCCESS(Status)) {
        goto ApplicationProcessorStartupEnd;
    }

    //
    // Perform phase 1 hardware layer initialization.
    //

    Status = HlInitialize(NULL, 1);
    if (!KSUCCESS(Status)) {
        goto ApplicationProcessorStartupEnd;
    }

ApplicationProcessorStartupEnd:
    KeFreeProcessorStartBlock(StartBlock, FALSE);
    KepReleaseProcessorStartLock();
    StartBlock = NULL;

    //
    // On failure, take the system down.
    //

    if (!KSUCCESS(Status)) {
        KeCrashSystem(CRASH_SYSTEM_INITIALIZATION_FAILURE,
                      KeGetCurrentProcessorNumber(),
                      Status,
                      0,
                      0);
    }

    //
    // Wait until all processors are ready, and drop down to low level.
    //

    RtlAtomicAdd32(&KeProcessorsReady, 1);
    while (KeAllProcessorsGo == FALSE) {
        ArProcessorYield();
    }

    KeLowerRunLevel(RunLevelLow);
    KeIdleLoop();
    return;
}

KSTATUS
KepSetBannerThread (
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine enables or disables the banner thread.

Arguments:

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    Status code.

--*/

{

    ULONG PreviousValue;
    KSTATUS Status;
    PULONG Value;

    if (*DataSize < sizeof(ULONG)) {
        *DataSize = sizeof(ULONG);
        return STATUS_BUFFER_TOO_SMALL;
    }

    Value = Data;
    *DataSize = sizeof(ULONG);
    if (Set == FALSE) {
        *Value = (KeBannerThreadEnabled & 0x1) != FALSE;
        return STATUS_SUCCESS;
    }

    //
    // This is privileged because there's no reason random users should be
    // doing it. Also since the threads linger hammering on this could lead to
    // resource exhaustion.
    //

    Status = PsCheckPermission(PERMISSION_SYSTEM_ADMINISTRATOR);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Loop increasing the generation number until the correct edge is
    // performed.
    //

    while (TRUE) {
        PreviousValue = KeBannerThreadEnabled;

        //
        // If the current value agrees with what the user wants, then break out.
        //

        if ((PreviousValue & 0x1) == (*Value != FALSE)) {
            break;
        }

        //
        // Bump the generation, which will hopefully make the desired
        // transition, but might end up doing the opposite if multiple threads
        // are in here.
        //

        PreviousValue = RtlAtomicAdd32(&KeBannerThreadEnabled, 1);

        //
        // Handle the thread previously being on (ie this turned it off). If the
        // user wanted it off, then great. Otherwise, loop again to try and
        // turn it back on.
        //

        if ((PreviousValue & 0x1) != 0) {
            if (*Value == FALSE) {
                break;
            }

        //
        // This action just turned it on. If the user wanted it on, then great,
        // create the thread. Otherwise, loop again to try and turn it off.
        //

        } else {
            if (*Value != FALSE) {
                Status = PsCreateKernelThread(KepBannerThread,
                                              (PVOID)(UINTN)(PreviousValue + 1),
                                              "KepBannerThread");

                break;
            }
        }
    }

    *Value = (PreviousValue & 0x1) != FALSE;
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
KepCompleteSystemInitialization (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine completes initial kernel startup. It is performed on a
    separate thread to allow the startup thread to mature into the idle thread
    before blocking work starts. There is no guarantee that this routine will
    be executed exclusively on any one processor, the scheduler and all
    processors are active at this point.

Arguments:

    Parameter - Supplies information about the system and memory layout as
        set up by the loader, the kernel initialization block.

Return Value:

    None.

--*/

{

    KERNEL_SUBSYSTEM FailingSubsystem;
    PKERNEL_INITIALIZATION_BLOCK Parameters;
    KSTATUS Status;

    FailingSubsystem = KernelSubsystemInvalid;
    Parameters = (PKERNEL_INITIALIZATION_BLOCK)Parameter;

    //
    // Let all processors idle.
    //

    KeAllProcessorsGo = TRUE;

    //
    // Perform phase 0 initialization of the I/O subsystem, which will
    // initialize boot start drivers.
    //

    Status = IoInitialize(0, Parameters);
    if (!KSUCCESS(Status)) {
        FailingSubsystem = KernelSubsystemInputOutput;
        goto CompleteSystemInitializationEnd;
    }

    //
    // Perform phase 3 executive initialization, which signs up for entropy
    // interface notifications.
    //

    Status = KeInitialize(3, NULL);
    if (!KSUCCESS(Status)) {
        FailingSubsystem = KernelSubsystemKernelExecutive;
        goto CompleteSystemInitializationEnd;
    }

    //
    // Perform phase 3 initialization of the memory manager, which completes
    // initialization by freeing all boot allocations. From here on out, the
    // parameters pointer is inaccessible.
    //

    Status = MmInitialize(Parameters, NULL, 3);
    if (!KSUCCESS(Status)) {
        FailingSubsystem = KernelSubsystemMemoryManager;
        goto CompleteSystemInitializationEnd;
    }

    //
    // Fire up the banner thread.
    //

    if ((KeBannerThreadEnabled & 0x1) != 0) {
        PsCreateKernelThread(KepBannerThread,
                             (PVOID)(UINTN)KeBannerThreadEnabled,
                             "KepBannerThread");
    }

CompleteSystemInitializationEnd:
    if (!KSUCCESS(Status)) {
        KeVideoPrintString(0, 24, "Failure: 0x");
        KeVideoPrintHexInteger(11, 24, Status);
        KeCrashSystem(CRASH_SYSTEM_INITIALIZATION_FAILURE,
                      FailingSubsystem,
                      Status,
                      0,
                      0);
    }

    return;
}

VOID
KepAcquireProcessorStartLock (
    VOID
    )

/*++

Routine Description:

    This routine acquires the processor start lock.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG LockValue;

    while (TRUE) {
        LockValue = RtlAtomicCompareExchange32(&KeProcessorStartLock, 1, 0);
        if (LockValue == 0) {
            break;
        }

        ArProcessorYield();
    }

    return;
}

VOID
KepReleaseProcessorStartLock (
    VOID
    )

/*++

Routine Description:

    This routine releases the processor start lock.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG LockValue;

    LockValue = RtlAtomicExchange32(&KeProcessorStartLock, 0);

    //
    // Assert if the lock was not held.
    //

    ASSERT(LockValue != 0);

    return;
}

VOID
KepBannerThread (
    PVOID Context
    )

/*++

Routine Description:

    This routine prints an updated banner at the top of the screen.

Arguments:

    Context - Supplies a context pointer, which in this case is a generation
        number. If the generation number changes from this, the thread exits.

Return Value:

    None.

--*/

{

    CHAR BannerString[120];
    IO_CACHE_STATISTICS Cache;
    CHAR CacheString[16];
    ULONG CellHeight;
    ULONG Columns;
    CHAR CpuIdleString[16];
    CHAR CpuInterruptString[16];
    CHAR CpuKernelString[16];
    CHAR CpuUserString[16];
    ULONGLONG Days;
    ULONGLONG Frequency;
    ULONGLONG Hours;
    IO_GLOBAL_STATISTICS IoStatistics;
    CHAR IoString[16];
    MM_STATISTICS Memory;
    PSTR MemoryFormat;
    ULONGLONG Minutes;
    CHAR NonPagedPoolString[16];
    CHAR PagedPoolString[16];
    ULONG PageShift;
    PSTR PagingFormat;
    CHAR PagingString[24];
    CHAR PagingValueString[16];
    IO_GLOBAL_STATISTICS PreviousIoStatistics;
    ULONGLONG ReadDifference;
    ULONG Rows;
    ULONGLONG Seconds;
    ULONG Size;
    KSTATUS Status;
    ULONGLONG TimeCounter;
    PSTR TimeFormat;
    PKTIMER Timer;
    TIMER_QUEUE_TYPE TimerQueueType;
    CHAR TotalMemoryString[16];
    CHAR UptimeString[16];
    SYSTEM_USAGE_CONTEXT UsageContext;
    UINTN UsedSize;
    ULONG Width;
    ULONGLONG WriteDifference;

    Frequency = HlQueryTimeCounterFrequency();
    PageShift = MmPageShift();
    RtlZeroMemory(&Memory, sizeof(MM_STATISTICS));
    RtlZeroMemory(&Cache, sizeof(IO_CACHE_STATISTICS));
    RtlZeroMemory(&UsageContext, sizeof(SYSTEM_USAGE_CONTEXT));
    RtlZeroMemory(&PreviousIoStatistics, sizeof(IO_GLOBAL_STATISTICS));
    IoStatistics.Version = IO_GLOBAL_STATISTICS_VERSION;
    Memory.Version = MM_STATISTICS_VERSION;
    Cache.Version = IO_CACHE_STATISTICS_VERSION;
    Status = KeVideoGetDimensions(&Width,
                                  NULL,
                                  NULL,
                                  &CellHeight,
                                  &Columns,
                                  &Rows);

    if ((!KSUCCESS(Status)) || (Rows < 3)) {
        return;
    }

    if (Columns > sizeof(BannerString) - 1) {
        Columns = sizeof(BannerString) - 1;
    }

    //
    // Determine the right format given the width of the console.
    //

    if (Columns >= KE_BANNER_FULL_WIDTH) {
        MemoryFormat = KE_BANNER_FULL_MEMORY_FORMAT;
        TimeFormat = KE_BANNER_FULL_TIME_FORMAT;
        PagingFormat = KE_BANNER_FULL_PAGING_FORMAT;

    } else if (Columns >= KE_BANNER_SHORT_WIDTH) {
        MemoryFormat = KE_BANNER_SHORT_MEMORY_FORMAT;
        TimeFormat = KE_BANNER_SHORT_TIME_FORMAT;
        PagingFormat = KE_BANNER_SHORT_PAGING_FORMAT;

    } else if (Columns >= KE_BANNER_TINY_WIDTH) {
        MemoryFormat = KE_BANNER_TINY_MEMORY_FORMAT;
        TimeFormat = KE_BANNER_TINY_TIME_FORMAT;
        PagingFormat = KE_BANNER_TINY_PAGING_FORMAT;

    } else {
        return;
    }

    Timer = KeCreateTimer(KE_ALLOCATION_TAG);
    if (Timer == NULL) {
        return;
    }

    KeVideoClearScreen(0, 0, Width, CellHeight * 3);
    while (TRUE) {
        if (KeBannerThreadEnabled != (ULONG)(UINTN)Context) {
            break;
        }

        Status = MmGetMemoryStatistics(&Memory);
        if (!KSUCCESS(Status)) {
            RtlDebugPrint("Failed to get MM statistics.\n");
            break;
        }

        Status = IoGetCacheStatistics(&Cache);
        if (!KSUCCESS(Status)) {
            RtlDebugPrint("Failed to get IO cache statistics.\n");
        }

        IoGetGlobalStatistics(&IoStatistics);
        TimeCounter = KeGetRecentTimeCounter();
        Seconds = TimeCounter / Frequency;
        Minutes = Seconds / SECONDS_PER_MINUTE;
        Seconds %= SECONDS_PER_MINUTE;
        Hours = Minutes / MINUTES_PER_HOUR;
        Minutes %= MINUTES_PER_HOUR;
        Days = Hours / HOURS_PER_DAY;
        Hours %= HOURS_PER_DAY;
        KepPrintFormattedMemoryUsage(TotalMemoryString,
                                     sizeof(TotalMemoryString),
                                     Memory.AllocatedPhysicalPages << PageShift,
                                     Memory.PhysicalPages << PageShift);

        UsedSize = Memory.PagedPool.TotalHeapSize -
                   Memory.PagedPool.FreeListSize;

        KepPrintFormattedMemoryUsage(PagedPoolString,
                                     sizeof(PagedPoolString),
                                     UsedSize,
                                     Memory.PagedPool.TotalHeapSize);

        UsedSize = Memory.NonPagedPool.TotalHeapSize -
                   Memory.NonPagedPool.FreeListSize;

        KepPrintFormattedMemoryUsage(NonPagedPoolString,
                                     sizeof(NonPagedPoolString),
                                     UsedSize,
                                     Memory.NonPagedPool.TotalHeapSize);

        KepPrintFormattedMemoryUsage(CacheString,
                                     sizeof(CacheString),
                                     Cache.DirtyPageCount << PageShift,
                                     Cache.PhysicalPageCount << PageShift);

        if (Columns >= KE_BANNER_SHORT_WIDTH) {
            Size = RtlPrintToString(BannerString,
                                    Columns + 1,
                                    CharacterEncodingDefault,
                                    MemoryFormat,
                                    TotalMemoryString,
                                    PagedPoolString,
                                    NonPagedPoolString,
                                    CacheString);

        } else {
            Size = RtlPrintToString(BannerString,
                                    Columns + 1,
                                    CharacterEncodingDefault,
                                    MemoryFormat,
                                    TotalMemoryString,
                                    CacheString);
        }

        Size -= 1;
        while (Size < Columns) {
            BannerString[Size] = ' ';
            Size += 1;
        }

        BannerString[Size] = '\0';
        KeVideoPrintString(0, 0, BannerString);

        //
        // Also update the second line, which contains the system usage.
        //

        KepUpdateSystemUsage(&UsageContext);
        if (Days == 0) {
            RtlPrintToString(UptimeString,
                             sizeof(UptimeString),
                             CharacterEncodingAscii,
                             "%02lld:%02lld:%02lld",
                             Hours,
                             Minutes,
                             Seconds);

        } else {
            RtlPrintToString(UptimeString,
                             sizeof(UptimeString),
                             CharacterEncodingAscii,
                             "%02lld:%02lld:%02lld:%02lld",
                             Days,
                             Hours,
                             Minutes,
                             Seconds);
        }

        KepPrintFormattedPercent(CpuUserString,
                                 sizeof(CpuUserString),
                                 UsageContext.UserPercent);

        KepPrintFormattedPercent(CpuKernelString,
                                 sizeof(CpuKernelString),
                                 UsageContext.KernelPercent);

        KepPrintFormattedPercent(CpuInterruptString,
                                 sizeof(CpuInterruptString),
                                 UsageContext.InterruptPercent);

        KepPrintFormattedPercent(CpuIdleString,
                                 sizeof(CpuIdleString),
                                 UsageContext.IdlePercent);

        ReadDifference = IoStatistics.BytesRead -
                         PreviousIoStatistics.BytesRead;

        WriteDifference = IoStatistics.BytesWritten -
                          PreviousIoStatistics.BytesWritten;

        KepPrintFormattedMemoryUsage(IoString,
                                     sizeof(IoString),
                                     ReadDifference,
                                     WriteDifference);

        ReadDifference = IoStatistics.PagingBytesRead -
                         PreviousIoStatistics.PagingBytesRead;

        WriteDifference = IoStatistics.PagingBytesWritten -
                          PreviousIoStatistics.PagingBytesWritten;

        PagingString[0] = '\0';
        if ((ReadDifference != 0) || (WriteDifference != 0)) {
            KepPrintFormattedMemoryUsage(PagingValueString,
                                         sizeof(PagingValueString),
                                         ReadDifference,
                                         WriteDifference);

            RtlPrintToString(PagingString,
                             sizeof(PagingString),
                             CharacterEncodingAscii,
                             PagingFormat,
                             PagingValueString);
        }

        RtlCopyMemory(&PreviousIoStatistics,
                      &IoStatistics,
                      sizeof(IO_GLOBAL_STATISTICS));

        if (Columns >= KE_BANNER_SHORT_WIDTH) {
            Size = RtlPrintToString(BannerString,
                                    Columns + 1,
                                    CharacterEncodingDefault,
                                    TimeFormat,
                                    UptimeString,
                                    CpuUserString,
                                    CpuKernelString,
                                    CpuInterruptString,
                                    CpuIdleString,
                                    IoString,
                                    PagingString);

        } else {
            Size = RtlPrintToString(BannerString,
                                    Columns + 1,
                                    CharacterEncodingDefault,
                                    TimeFormat,
                                    UptimeString,
                                    CpuUserString,
                                    CpuKernelString,
                                    IoString);
        }

        Size -= 1;
        while (Size < Columns) {
            BannerString[Size] = ' ';
            Size += 1;
        }

        BannerString[Size] = '\0';
        KeVideoPrintString(0, 1, BannerString);
        TimerQueueType = TimerQueueSoftWake;
        if ((Seconds % 5) == 0) {
            TimerQueueType = TimerQueueSoft;
        }

        KeQueueTimer(Timer,
                     TimerQueueType,
                     TimeCounter + Frequency,
                     0,
                     0,
                     NULL);

        ObWaitOnObject(Timer, 0, WAIT_TIME_INDEFINITE);
    }

    KeDestroyTimer(Timer);
    return;
}

VOID
KepUpdateSystemUsage (
    PSYSTEM_USAGE_CONTEXT Context
    )

/*++

Routine Description:

    This routine updates the system usage information.

Arguments:

    Context - Supplies a pointer to the context information.

Return Value:

    None.

--*/

{

    PROCESSOR_CYCLE_ACCOUNTING Cycles;
    ULONGLONG DeltaTotal;
    ULONGLONG ExpectedTotalDelta;
    ULONGLONG IdleDelta;
    ULONGLONG InterruptDelta;
    ULONGLONG KernelDelta;
    ULONGLONG StoppedCycles;
    ULONGLONG TimeCounter;
    ULONGLONG TimeCounterDelta;
    ULONGLONG TotalCycles;
    ULONGLONG TotalDelta;
    ULONGLONG UserDelta;

    if (Context->TimeCounterFrequency == 0) {
        Context->TimeCounterFrequency = HlQueryTimeCounterFrequency();
    }

    if (Context->CycleCounterFrequency == 0) {
        Context->CycleCounterFrequency = HlQueryProcessorCounterFrequency();
    }

    //
    // Snap the time counter and cycle counters.
    //

    TimeCounter = HlQueryTimeCounter();
    KeGetTotalProcessorCycleAccounting(&Cycles);

    //
    // The cycle counter may not count while the processor is idle. Use the
    // time counter to figure out how many cycles there should have been, and
    // compare to how many there actually are. Any difference gets added to the
    // idle cycles.
    //

    TimeCounterDelta = TimeCounter - Context->TimeCounter;
    if (TimeCounterDelta == 0) {
        return;
    }

    //
    // TcTicks * CcTicks/ * s/       = CcTicks.
    //           s          TcTicks
    //

    ExpectedTotalDelta = TimeCounterDelta * Context->CycleCounterFrequency *
                         KeGetActiveProcessorCount() /
                         Context->TimeCounterFrequency;

    TotalCycles = Cycles.UserCycles + Cycles.KernelCycles +
                  Cycles.InterruptCycles + Cycles.IdleCycles;

    TotalDelta = TotalCycles - Context->TotalCycles;
    StoppedCycles = 0;
    if (ExpectedTotalDelta > TotalDelta) {
        StoppedCycles = ExpectedTotalDelta - TotalDelta;
    }

    //
    // Compute the differences between this time and last time.
    //

    UserDelta = Cycles.UserCycles - Context->UserCycles;
    KernelDelta = Cycles.KernelCycles - Context->KernelCycles;
    InterruptDelta = Cycles.InterruptCycles - Context->InterruptCycles;
    IdleDelta = Cycles.IdleCycles - Context->IdleCycles + StoppedCycles;
    DeltaTotal = UserDelta + KernelDelta + InterruptDelta + IdleDelta;

    //
    // Save this snapshot into the context as the new previous snapshot.
    //

    Context->TimeCounter = TimeCounter;
    Context->UserCycles = Cycles.UserCycles;
    Context->KernelCycles = Cycles.KernelCycles;
    Context->InterruptCycles = Cycles.InterruptCycles;
    Context->IdleCycles = Cycles.IdleCycles;
    Context->TotalCycles = TotalCycles;

    //
    // Finally, update the percent (times ten) values.
    //

    Context->UserPercent = UserDelta * 1000 / DeltaTotal;
    Context->KernelPercent = KernelDelta * 1000 / DeltaTotal;
    Context->InterruptPercent = InterruptDelta * 1000 / DeltaTotal;
    Context->IdlePercent = IdleDelta * 1000 / DeltaTotal;
    return;
}

VOID
KepPrintFormattedMemoryUsage (
    PCHAR String,
    ULONG StringSize,
    ULONGLONG UsedValue,
    ULONGLONG TotalValue
    )

/*++

Routine Description:

    This routine prints two formatted sizes a la 5.8M/64M.

Arguments:

    String - Supplies a pointer to the string buffer to print to.

    StringSize - Supplies the total size of the string buffer in bytes.

    UsedValue - Supplies the first value to print.

    TotalValue - Supplies the second value to print.

Return Value:

    None.

--*/

{

    ULONG Size;

    Size = KepPrintFormattedSize(String, StringSize, UsedValue);
    if (Size != 0) {
        Size -= 1;
    }

    String += Size;
    StringSize -= Size;
    if (StringSize > 1) {
        *String = '/';
        String += 1;
        StringSize -= 1;
    }

    if (StringSize > 1) {
        KepPrintFormattedSize(String, StringSize, TotalValue);
    }

    return;
}

ULONG
KepPrintFormattedSize (
    PCHAR String,
    ULONG StringSize,
    ULONGLONG Value
    )

/*++

Routine Description:

    This routine prints a formatted size a la 5.8M (M for megabytes).

Arguments:

    String - Supplies a pointer to the string buffer to print to.

    StringSize - Supplies the total size of the string buffer in bytes.

    Value - Supplies the value in bytes to print.

Return Value:

    Returns the length of the final string after all formatting has been
    completed.

--*/

{

    ULONG Size;
    CHAR Suffix;

    Suffix = 'B';
    if (Value > 1024) {
        Suffix = 'K';
        Value = (Value * 10) / 1024;
        if (Value / 10 >= 1024) {
            Suffix = 'M';
            Value /= 1024;
            if (Value / 10 >= 1024) {
                Suffix = 'G';
                Value /= 1024;
            }
        }
    }

    ASSERT(Value < 1024 * 10);

    if (Suffix == 'B') {
        Size = RtlPrintToString(String,
                                StringSize,
                                CharacterEncodingAscii,
                                "%d",
                                (ULONG)Value);

    } else {
        if (Value < 100) {
            Size = RtlPrintToString(String,
                                    StringSize,
                                    CharacterEncodingAscii,
                                    "%d.%d%c",
                                    (ULONG)Value / 10,
                                    (ULONG)Value % 10,
                                    Suffix);

        } else {
            Size = RtlPrintToString(String,
                                    StringSize,
                                    CharacterEncodingAscii,
                                    "%d%c",
                                    (ULONG)Value / 10,
                                    Suffix);
        }
    }

    return Size;
}

ULONG
KepPrintFormattedPercent (
    PCHAR String,
    ULONG StringSize,
    ULONG PercentTimesTen
    )

/*++

Routine Description:

    This routine prints a formatted percentage a la 5.8% or 99%. The field
    width is always 4.

Arguments:

    String - Supplies a pointer to the string buffer to print to.

    StringSize - Supplies the total size of the string buffer in bytes.

    PercentTimesTen - Supplies ten times the percentage value. So 54.8% would
        have a value of 548. This value will be rounded to the precision that
        is printed.

    Offset - Supplies a pointer that on input supplies the offset within the
        string to print. This value will be updated to the new end of the
        string.

Return Value:

    Returns the length of the final string after all formatting has been
    completed.

--*/

{

    ULONG Size;

    //
    // For values less than 10%, print the single digit and first decimal
    // point.
    //

    if (PercentTimesTen < 100) {
        Size = RtlPrintToString(String,
                                StringSize,
                                CharacterEncodingAscii,
                                "%d.%d%%",
                                PercentTimesTen / 10,
                                PercentTimesTen % 10);

    } else {
        PercentTimesTen += 5;
        Size = RtlPrintToString(String,
                                StringSize,
                                CharacterEncodingAscii,
                                "%3d%%",
                                PercentTimesTen / 10);
    }

    return Size;
}

