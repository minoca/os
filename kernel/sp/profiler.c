/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    profiler.c

Abstract:

    This module implements system profiling support.

Author:

    Chris Stevens 1-Jul-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/kdebug.h>
#include "spp.h"

//
// ---------------------------------------------------------------- Definitions
//

#define SP_ALLOCATION_TAG 0x21217053 // '!!pS'

//
// Define the length of the scratch buffer within the profiler buffer.
//

#define SCRATCH_BUFFER_LENGTH 200

//
// Define the size of the profiler buffer.
//

#define PROFILER_BUFFER_LENGTH (128 * 1024)

//
// Define the period between memory statistics updates, in microseoncds.
//

#define MEMORY_STATISTICS_TIMER_PERIOD (1000 * MICROSECONDS_PER_MILLISECOND)

//
// Define the number of buffers required to track memory profiling data.
//

#define MEMORY_BUFFER_COUNT 3

//
// Define the buffer size for a new process or thread.
//

#define PROFILER_PROCESS_INFORMATION_SIZE 1024
#define PROFILER_THREAD_INFORMATION_SIZE 1024

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structures defines the system profiler's collection buffer.

Members:

    Buffer - Stores a byte array of profiler data ready to be consumed. This is
        a ring buffer.

    ProducerIndex - Stores the index into the buffer that the data producer
        will write to next.

    ConsumerIndex - Stores the index into the buffer that the consumer will
        read from next.

    ConsumerStopIndex - Stores the index into the buffer that the consumer will
        read up to before completing a round of consuming.

    Scratch - Supplies a byte array used as a temporary holding place for data,
        allowing data collection to be performed with sequential writes before
        copying the data to the ring buffer.

--*/

typedef struct _PROFILER_BUFFER {
    BYTE Buffer[PROFILER_BUFFER_LENGTH];
    ULONG ProducerIndex;
    ULONG ConsumerIndex;
    ULONG ConsumerStopIndex;
    BYTE Scratch[SCRATCH_BUFFER_LENGTH];
} PROFILER_BUFFER, *PPROFILER_BUFFER;

/*++

Structure Description:

    This structure defines a memory statistics collection buffer for the
    system profiler.

Members:

    Buffer - Stores a byte array of memory statistics data.

    BufferSize - Stores the size of the buffer, in bytes.

--*/

typedef struct _MEMORY_BUFFER {
    BYTE *Buffer;
    ULONG BufferSize;
    ULONG ConsumerIndex;
} MEMORY_BUFFER, *PMEMORY_BUFFER;

/*++

Structure Description:

    This structure defines the system's memory profiling state.

Members:

    MemoryBuffers - Stores an array of points to memory buffers.

    ConsumerActive - Stores a boolean indicating whether or not the consumer
        is active.

    ConsumerIndex - Stores the index of the memory buffer that was last
        consumed or is actively being consumed.

    ReadyIndex - Stores the index of the next buffer from which the consumer
        should read.

    ProducerIndex - Stores the index of the next buffer to which the producer
        should write.

    Timer - Stores a pointer to the timer that controls production.

    ThreadAlive - Stores a boolean indicating whether the thread is alive or
        not.

--*/

typedef struct _MEMORY_PROFILER {
    MEMORY_BUFFER MemoryBuffers[MEMORY_BUFFER_COUNT];
    BOOL ConsumerActive;
    ULONG ConsumerIndex;
    ULONG ReadyIndex;
    ULONG ProducerIndex;
    PKTIMER Timer;
    volatile BOOL ThreadAlive;
} MEMORY_PROFILER, *PMEMORY_PROFILER;

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
SppWriteProfilerBuffer (
    PPROFILER_BUFFER ProfilerBuffer,
    BYTE *Data,
    ULONG Length
    );

BOOL
SppReadProfilerBuffer (
    PPROFILER_BUFFER ProfilerBuffer,
    BYTE *Data,
    PULONG DataLength
    );

ULONG
SppReadProfilerData (
    BYTE *Destination,
    BYTE *Source,
    ULONG ByteCount,
    PULONG BytesRemaining
    );

KSTATUS
SppInitializeStackSampling (
    VOID
    );

VOID
SppDestroyStackSampling (
    ULONG Phase
    );

KSTATUS
SppInitializeMemoryStatistics (
    VOID
    );

VOID
SppDestroyMemoryStatistics (
    ULONG Phase
    );

VOID
SppMemoryStatisticsThread (
    PVOID Parameter
    );

KSTATUS
SppInitializeThreadStatistics (
    VOID
    );

VOID
SppSendInitialProcesses (
    VOID
    );

KSTATUS
SppSendInitialThreads (
    PROCESS_ID ProcessId
    );

VOID
SppProcessNewProcess (
    PROCESS_ID ProcessId
    );

VOID
SppProcessNewThread (
    PROCESS_ID ProcessId,
    THREAD_ID ThreadId
    );

VOID
SppDestroyThreadStatistics (
    ULONG Phase
    );

VOID
SppCollectThreadStatistic (
    PKTHREAD Thread,
    PPROCESSOR_BLOCK Processor,
    SCHEDULER_REASON ScheduleOutReason
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Stores a value indicating whether or not profiling is enabled for system
// initialization. Can be set with PROFILER_TYPE_FLAG_* values.
//

ULONG SpEarlyEnabledFlags = 0x0;

//
// Stores a value indicating which types of profiling are enabled.
//

ULONG SpEnabledFlags;

//
// Stores a pointer to a queued lock protecting access to the profiling status
// variables.
//

PQUEUED_LOCK SpProfilingQueuedLock;

//
// Structures that store stack sampling data.
//

PPROFILER_BUFFER *SpStackSamplingArray;
ULONG SpStackSamplingArraySize;

//
// Stores a pointer to a structure that tracks memory statistics profiling.
//

PMEMORY_PROFILER SpMemory;

//
// Structures that store thread statistics.
//

PPROFILER_BUFFER *SpThreadStatisticsArray;
ULONG SpThreadStatisticsArraySize;
PSP_COLLECT_THREAD_STATISTIC SpCollectThreadStatisticRoutine;
PSP_PROCESS_NEW_PROCESS SpProcessNewProcessRoutine;
PSP_PROCESS_NEW_THREAD SpProcessNewThreadRoutine;

//
// ------------------------------------------------------------------ Functions
//

VOID
SpProfilerInterrupt (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine handles periodic profiler interrupts, collecting information
    about the system for analysis.

Arguments:

    TrapFrame - Supplies a pointer to the current trap frame.

Return Value:

    None.

--*/

{

    PVOID *CallStack;
    ULONG CallStackSize;
    ULONG Processor;
    PVOID Scratch;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelHigh);

    //
    // Immediately return if stack sampling is not enabled. It may have been
    // turned off while this interrupt was pending.
    //

    if ((SpEnabledFlags & PROFILER_TYPE_FLAG_STACK_SAMPLING) == 0) {
        return;
    }

    //
    // Do nothing on interrupt replay if the trap frame is NULL.
    //

    if (TrapFrame == NULL) {
        return;
    }

    //
    // Do not collect data on processors that have not been initialized for
    // profiling.
    //

    Processor = KeGetCurrentProcessorNumber();
    if (Processor >= SpStackSamplingArraySize) {
        return;
    }

    //
    // Collect the stack data from the trap frame.
    //

    Scratch = SpStackSamplingArray[Processor]->Scratch;
    CallStackSize = SCRATCH_BUFFER_LENGTH - sizeof(UINTN);
    CallStack = (PVOID *)(Scratch + sizeof(UINTN));
    Status = SppArchGetKernelStackData(TrapFrame, CallStack, &CallStackSize);
    if (!KSUCCESS(Status)) {
        return;
    }

    ASSERT(CallStackSize != 0);

    CallStackSize += sizeof(UINTN);
    *((PUINTN)Scratch) = PROFILER_DATA_SENTINEL | CallStackSize;

    //
    // Write the data to the sampling buffer.
    //

    SppWriteProfilerBuffer(SpStackSamplingArray[Processor],
                           (BYTE *)Scratch,
                           CallStackSize);

    return;
}

VOID
SpSendProfilingData (
    VOID
    )

/*++

Routine Description:

    This routine sends profiling data to any listening consumer. It is called
    periodically on each processor during the clock interrupt.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ASSERT(KeGetRunLevel() >= RunLevelClock);

    //
    // Call out to the current profiling consumer to have that component ask
    // for the data.
    //

    KdSendProfilingData();
    return;
}

KSTATUS
SpGetProfilerData (
    PPROFILER_NOTIFICATION ProfilerNotification,
    PULONG Flags
    )

/*++

Routine Description:

    This routine fills the provided profiler notification with profiling data.
    A profiler consumer should call this routine to obtain data to send over
    the wire. It is assumed here that consumers will serialize consumption.

Arguments:

    ProfilerNotification - Supplies a pointer to the profiler notification that
        is to be filled in with profiling data.

    Flags - Supplies a pointer to the types of profiling data the caller wants
        to collect. Upon return, the flags for the returned data will be
        returned.

Return Value:

    Status code.

--*/

{

    ULONG DataSize;
    PMEMORY_BUFFER MemoryBuffer;
    ULONG Processor;
    BOOL ReadMore;
    ULONG RemainingLength;

    ASSERT(Flags != NULL);
    ASSERT(*Flags != 0);

    //
    // Process the requested profiling data in a set order, removing each type
    // from the set of flags as it is processed.
    //

    if ((*Flags & PROFILER_TYPE_FLAG_STACK_SAMPLING) != 0) {
        Processor = KeGetCurrentProcessorNumber();

        ASSERT(Processor < SpStackSamplingArraySize);

        //
        // Fill the buffer with data from the current processor's stack
        // sampling data.
        //

        ReadMore = SppReadProfilerBuffer(
                                       SpStackSamplingArray[Processor],
                                       ProfilerNotification->Data,
                                       &ProfilerNotification->Header.DataSize);

        ProfilerNotification->Header.Type = ProfilerDataTypeStack;
        ProfilerNotification->Header.Processor = Processor;

        //
        // If no more data is available, it means that the consumer has read up
        // to the producer, or up to its stop point (the point the producer was
        // at when the consumer started).
        //

        if (ReadMore == FALSE) {
            *Flags &= ~PROFILER_TYPE_FLAG_STACK_SAMPLING;
        }

    } else if ((*Flags & PROFILER_TYPE_FLAG_MEMORY_STATISTICS) != 0) {

        //
        // If the consumer is not currently active, then get the next buffer to
        // consume, which is indicated by the ready index.
        //

        if (SpMemory->ConsumerActive == FALSE) {
            SpMemory->ConsumerIndex = SpMemory->ReadyIndex;
            SpMemory->ConsumerActive = TRUE;
        }

        //
        // Copy as much data as possible from the consumer buffer to the
        // profiler notification data buffer.
        //

        MemoryBuffer = &(SpMemory->MemoryBuffers[SpMemory->ConsumerIndex]);
        RemainingLength = MemoryBuffer->BufferSize -
                          MemoryBuffer->ConsumerIndex;

        if (RemainingLength < ProfilerNotification->Header.DataSize) {
            DataSize = RemainingLength;

        } else {
            DataSize = ProfilerNotification->Header.DataSize;
        }

        if (DataSize != 0) {
            RtlCopyMemory(ProfilerNotification->Data,
                          &(MemoryBuffer->Buffer[MemoryBuffer->ConsumerIndex]),
                          DataSize);
        }

        MemoryBuffer->ConsumerIndex += DataSize;
        ProfilerNotification->Header.Type = ProfilerDataTypeMemory;
        ProfilerNotification->Header.Processor = KeGetCurrentProcessorNumber();
        ProfilerNotification->Header.DataSize = DataSize;

        //
        // Mark the consumer inactive if all the data was consumed.
        //

        if (MemoryBuffer->ConsumerIndex == MemoryBuffer->BufferSize) {
            SpMemory->ConsumerActive = FALSE;
            *Flags &= ~PROFILER_TYPE_FLAG_MEMORY_STATISTICS;
        }

    } else if ((*Flags & PROFILER_TYPE_FLAG_THREAD_STATISTICS) != 0) {
        Processor = KeGetCurrentProcessorNumber();

        ASSERT(Processor < SpThreadStatisticsArraySize);

        //
        // Fill the buffer with data from the current processor's stack
        // sampling data.
        //

        ReadMore = SppReadProfilerBuffer(
                                     SpThreadStatisticsArray[Processor],
                                     ProfilerNotification->Data,
                                     &(ProfilerNotification->Header.DataSize));

        ProfilerNotification->Header.Type = ProfilerDataTypeThread;
        ProfilerNotification->Header.Processor = Processor;

        //
        // If no more data is available, it means that the consumer has read up
        // to the producer, or up to its stop point (the point the producer was
        // at when the consumer started).
        //

        if (ReadMore == FALSE) {
            *Flags &= ~PROFILER_TYPE_FLAG_THREAD_STATISTICS;
        }
    }

    return STATUS_SUCCESS;
}

ULONG
SpGetProfilerDataStatus (
    VOID
    )

/*++

Routine Description:

    This routine determines if there is profiling data for the current
    processor that needs to be sent to a consumer.

Arguments:

    None.

Return Value:

    Returns a set of flags representing which types of profiling data are
    available. Returns zero if nothing is available.

--*/

{

    PPROFILER_BUFFER Buffer;
    ULONG Flags;
    ULONG Processor;

    ASSERT(KeGetRunLevel() >= RunLevelClock);

    if (SpEnabledFlags == 0) {
        return 0;
    }

    Flags = SpEnabledFlags;

    //
    // Determine if there is stack sampling data to send.
    //

    if ((Flags & PROFILER_TYPE_FLAG_STACK_SAMPLING) != 0) {

        //
        // If stack sampling is not yet initialized on this processor remove it
        // from the flags.
        //

        Processor = KeGetCurrentProcessorNumber();
        if (Processor >= SpStackSamplingArraySize) {
            Flags &= ~PROFILER_TYPE_FLAG_STACK_SAMPLING;

        //
        // Otherwise if the stack sampling buffer is empty, then remove it from
        // the flags.
        //
        // N.B. This access if safe because the stack sampling destruction code
        //      waits for least one clock interrupt after disabling stack
        //      sampling before destroying the global array.
        //

        } else {
            Buffer = SpStackSamplingArray[Processor];
            if (Buffer->ProducerIndex == Buffer->ConsumerIndex) {
                Flags &= ~PROFILER_TYPE_FLAG_STACK_SAMPLING;
            }
        }
    }

    //
    // Determine if there are memory statistics to send.
    //

    if ((Flags & PROFILER_TYPE_FLAG_MEMORY_STATISTICS) != 0) {

        //
        // There is no new data if the consumer index still equals the ready
        // index or the producer index.
        //

        if ((SpMemory->ConsumerIndex == SpMemory->ReadyIndex) ||
            (SpMemory->ConsumerIndex == SpMemory->ProducerIndex)) {

            Flags &= ~PROFILER_TYPE_FLAG_MEMORY_STATISTICS;
        }
    }

    if ((Flags & PROFILER_TYPE_FLAG_THREAD_STATISTICS) != 0) {

        //
        // If thread statistics are not yet initialized on this processor
        // remove them from the flags.
        //

        Processor = KeGetCurrentProcessorNumber();
        if (Processor >= SpThreadStatisticsArraySize) {
            Flags &= ~PROFILER_TYPE_FLAG_THREAD_STATISTICS;

        //
        // Otherwise if the thread statistics buffer is empty, then remove it
        // from the flags.
        //
        // N.B. This access if safe because the thread statistics destruction
        //      code waits for least one clock interrupt after disabling
        //      profiling before destroying the global array.
        //

        } else {
            Buffer = SpThreadStatisticsArray[Processor];
            if (Buffer->ProducerIndex == Buffer->ConsumerIndex) {
                Flags &= ~PROFILER_TYPE_FLAG_THREAD_STATISTICS;
            }
        }
    }

    return Flags;
}

KSTATUS
SpInitializeProfiler (
    VOID
    )

/*++

Routine Description:

    This routine initializes system profiling at processor start-up. It
    extends the profiling infrastructure as each processor comes online. If
    early profiling is not enabled, this routine just exits.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    ASSERT(KeGetRunLevel() <= RunLevelDispatch);
    ASSERT(KeGetCurrentProcessorNumber() == 0);

    //
    // Always initialize the profiling lock in case profiling gets enabled
    // later on.
    //

    SpProfilingQueuedLock = KeCreateQueuedLock();
    if (SpProfilingQueuedLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeProfilerEnd;
    }

    //
    // Do nothing more if early profiling is not enabled for any profiling
    // types.
    //

    if (SpEarlyEnabledFlags != 0) {
        KeAcquireQueuedLock(SpProfilingQueuedLock);
        Status = SppStartSystemProfiler(SpEarlyEnabledFlags);
        KeReleaseQueuedLock(SpProfilingQueuedLock);
        if (!KSUCCESS(Status)) {
            goto InitializeProfilerEnd;
        }
    }

    Status = STATUS_SUCCESS;

InitializeProfilerEnd:
    return Status;
}

KSTATUS
SppStartSystemProfiler (
    ULONG Flags
    )

/*++

Routine Description:

    This routine starts the system profiler. This routine must be called at low
    level. It assumes the profiler queued lock is held.

Arguments:

    Flags - Supplies a set of flags representing the types of profiling that
        should be started.

Return Value:

    Status code.

--*/

{

    ULONG InitializedFlags;
    ULONG NewFlags;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(KeIsQueuedLockHeld(SpProfilingQueuedLock) != FALSE);

    //
    // The caller must specify flags.
    //

    if (Flags == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    InitializedFlags = 0;

    //
    // Determine what new profiling types need to be started.
    //

    NewFlags = Flags & ~SpEnabledFlags;

    //
    // If all the desired profiling types are already active, then just exit.
    //

    if (NewFlags == 0) {
        Status = STATUS_SUCCESS;
        goto StartSystemProfilerEnd;
    }

    //
    // Initialize the system profiler for each of the new types.
    //

    if ((NewFlags & PROFILER_TYPE_FLAG_STACK_SAMPLING) != 0) {
        Status = SppInitializeStackSampling();
        if (!KSUCCESS(Status)) {
            goto StartSystemProfilerEnd;
        }

        InitializedFlags |= PROFILER_TYPE_FLAG_STACK_SAMPLING;
    }

    if ((NewFlags & PROFILER_TYPE_FLAG_MEMORY_STATISTICS) != 0) {
        Status = SppInitializeMemoryStatistics();
        if (!KSUCCESS(Status)) {
            goto StartSystemProfilerEnd;
        }

        InitializedFlags |= PROFILER_TYPE_FLAG_MEMORY_STATISTICS;
    }

    if ((NewFlags & PROFILER_TYPE_FLAG_THREAD_STATISTICS) != 0) {
        Status = SppInitializeThreadStatistics();
        if (!KSUCCESS(Status)) {
            goto StartSystemProfilerEnd;
        }

        InitializedFlags |= PROFILER_TYPE_FLAG_THREAD_STATISTICS;
    }

    KeUpdateClockForProfiling(TRUE);
    Status = STATUS_SUCCESS;

StartSystemProfilerEnd:
    if (!KSUCCESS(Status)) {
        if (InitializedFlags != 0) {
            SppStopSystemProfiler(InitializedFlags);
        }
    }

    return Status;
}

KSTATUS
SppStopSystemProfiler (
    ULONG Flags
    )

/*++

Routine Description:

    This routine stops the system profiler and destroys the profiling data
    structures. This routine must be called at low level. It assumes the
    profiler queued lock is held.

Arguments:

    Flags - Supplies a set of flags representing the types of profiling that
        should be stopped.

Return Value:

    Status code.

--*/

{

    BOOL DelayRequired;
    ULONG DisableFlags;
    ULONG Index;
    ULONG *InterruptCounts;
    RUNLEVEL OldRunLevel;
    ULONG ProcessorCount;
    PROCESSOR_SET Processors;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(KeIsQueuedLockHeld(SpProfilingQueuedLock) != FALSE);

    //
    // The caller must specify flags.
    //

    if (Flags == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Determine what profiling types need to be stopped.
    //

    DisableFlags = Flags & SpEnabledFlags;

    //
    // If profiling is already disabled for the requested profiling types, then
    // just exit.
    //

    if (DisableFlags == 0) {
        Status = STATUS_SUCCESS;
        goto StopSystemProfilerEnd;
    }

    //
    // Phase 0 destroy stops the system profiler for each type that needs to be
    // stopped.
    //

    if ((DisableFlags & PROFILER_TYPE_FLAG_STACK_SAMPLING) != 0) {
        SppDestroyStackSampling(0);
    }

    if ((DisableFlags & PROFILER_TYPE_FLAG_MEMORY_STATISTICS) != 0) {
        SppDestroyMemoryStatistics(0);
    }

    if ((DisableFlags & PROFILER_TYPE_FLAG_THREAD_STATISTICS) != 0) {
        SppDestroyThreadStatistics(0);
    }

    //
    // Once phase zero destruction is complete, each profiler has stopped
    // producing data immediately, but another core may be in the middle of
    // consuming profiling data during its clock interrupt. Wait until each
    // processor has received the notice that profiling is now disabled and
    // then destroy each profiler's data structures. This is guaranteed after
    // the clock interrupt has incremented once. If an array cannot be
    // allocated for the processor counts, then just yield for a bit. It is not
    // good enough to just send an IPI-level interrupt to each core. This may
    // land on top of a clock interrupt in the middle of checking to see if
    // there is pending profiling data, which is not done with interrupts
    // disabled (i.e. the IPI-level interrupt running doesn't indicate the
    // other core is done with the data). As a result, this routine could run
    // through and release the buffers being observed by the other core.
    //

    ProcessorCount = KeGetActiveProcessorCount();
    if (ProcessorCount > 1) {
        DelayRequired = TRUE;
        InterruptCounts = MmAllocateNonPagedPool(ProcessorCount * sizeof(ULONG),
                                                 SP_ALLOCATION_TAG);

        if (InterruptCounts != NULL) {
            for (Index = 0; Index < ProcessorCount; Index += 1) {
                InterruptCounts[Index] = KeGetClockInterruptCount(Index);
            }

            //
            // As some cores may have gone idle, send a clock IPI out to all of
            // them to make sure the interrupt count gets incremented.
            //

            Processors.Target = ProcessorTargetAll;
            OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
            Status = HlSendIpi(IpiTypeClock, &Processors);
            KeLowerRunLevel(OldRunLevel);
            if (KSUCCESS(Status)) {
                for (Index = 0; Index < ProcessorCount; Index += 1) {
                    while (KeGetClockInterruptCount(Index) <=
                           InterruptCounts[Index]) {

                        KeYield();
                    }
                }

                DelayRequired = FALSE;
            }

            MmFreeNonPagedPool(InterruptCounts);
        }

        //
        // If the allocation or IPI failed above, wait a conservative second to
        // make sure all the cores are done consuming the profiler data.
        //

        if (DelayRequired != FALSE) {
            KeDelayExecution(FALSE, FALSE, MICROSECONDS_PER_SECOND);
        }
    }

    //
    // Phase 1 destroy releases any resources for each type of profiling that
    // was stopped in phase 0.
    //

    if ((DisableFlags & PROFILER_TYPE_FLAG_STACK_SAMPLING) != 0) {
        SppDestroyStackSampling(1);
    }

    if ((DisableFlags & PROFILER_TYPE_FLAG_MEMORY_STATISTICS) != 0) {
        SppDestroyMemoryStatistics(1);
    }

    if ((DisableFlags & PROFILER_TYPE_FLAG_THREAD_STATISTICS) != 0) {
        SppDestroyThreadStatistics(1);
    }

    if (SpEnabledFlags == 0) {
        KeUpdateClockForProfiling(FALSE);
    }

    Status = STATUS_SUCCESS;

StopSystemProfilerEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
SppWriteProfilerBuffer (
    PPROFILER_BUFFER ProfilerBuffer,
    BYTE *Data,
    ULONG DataLength
    )

/*++

Routine Description:

    This routine writes the supplied data to the profiler data buffer. If there
    is not enough room in the buffer, it just exits.

Arguments:

    ProfilerBuffer - Supplies a pointer to a profiler data buffer to which the
        given data should be written.

    Data - Supplies an array of bytes to write to the profiler data buffer.

    DataLength - Supplies the length of the data array, in bytes.

Return Value:

    TRUE if the data was successfully added to the buffer.

    FALSE if the data was dropped.

--*/

{

    ULONG AvailableLength;
    ULONG BufferIndex;
    ULONG ConsumerIndex;
    ULONG DataIndex;
    ULONG FirstWriteLength;
    ULONG ProducerIndex;
    BOOL Result;
    ULONG SecondWriteLength;
    ULONG WriteLength;

    ConsumerIndex = ProfilerBuffer->ConsumerIndex;
    ProducerIndex = ProfilerBuffer->ProducerIndex;

    //
    // If the producer's and consumer's indices are equal, then the buffer is
    // empty. Allow the producer to write up until the end of the buffer, being
    // careful to never completely fill the buffer to differentiate between an
    // empty buffer and a full buffer.
    //

    if (ProducerIndex == ConsumerIndex) {
        AvailableLength = PROFILER_BUFFER_LENGTH - 1;

    //
    // If the producer's index is greater than the consumer's, then two writes
    // may be necessary to fill the buffer. Account for wrapping when
    // calculating the available length.
    //

    } else if (ProducerIndex > ConsumerIndex) {
        AvailableLength = PROFILER_BUFFER_LENGTH - ProducerIndex;
        AvailableLength += (ConsumerIndex - 1);

    //
    // If the producer's index is less than the consumer's, then allow the
    // producer to write up until 1 less than the consumer's index.
    //

    } else {

        ASSERT(ProducerIndex < ConsumerIndex);

        AvailableLength = (ConsumerIndex - 1) - ProducerIndex;
    }

    //
    // If the available length is not enough for the data, exit.
    //

    if (AvailableLength < DataLength) {
        Result = FALSE;
        goto WriteProfilerBufferEnd;
    }

    //
    // Determine if the write needs to be broken into two operations.
    //

    if ((ProducerIndex + DataLength) > PROFILER_BUFFER_LENGTH) {
        FirstWriteLength = PROFILER_BUFFER_LENGTH - ProducerIndex;

        ASSERT(FirstWriteLength <= DataLength);

        SecondWriteLength = DataLength - FirstWriteLength;

    } else {
        FirstWriteLength = DataLength;
        SecondWriteLength = 0;
    }

    //
    // Write the data to the buffer.
    //

    DataIndex = 0;
    BufferIndex = ProducerIndex;
    WriteLength = FirstWriteLength;
    RtlCopyMemory(&(ProfilerBuffer->Buffer[BufferIndex]),
                  &(Data[DataIndex]),
                  WriteLength);

    if (SecondWriteLength != 0) {
        DataIndex = WriteLength;
        BufferIndex = 0;
        WriteLength = SecondWriteLength;
        RtlCopyMemory(&(ProfilerBuffer->Buffer[BufferIndex]),
                      &(Data[DataIndex]),
                      WriteLength);
    }

    //
    // Update the producer index.
    //

    ProducerIndex = BufferIndex + WriteLength;
    if (ProducerIndex == PROFILER_BUFFER_LENGTH) {
        ProfilerBuffer->ProducerIndex = 0;

    } else {
        ProfilerBuffer->ProducerIndex = ProducerIndex;
    }

    Result = TRUE;

WriteProfilerBufferEnd:
    return Result;
}

BOOL
SppReadProfilerBuffer (
    PPROFILER_BUFFER ProfilerBuffer,
    BYTE *Data,
    PULONG DataLength
    )

/*++

Routine Description:

    This routine reads up to the provided data length of bytes from the given
    profiler buffer. Upon return, the data length is modified to reflect the
    total number of bytes read. If there are no new bytes to read from the
    buffer, then a data length of zero is returned.

Arguments:

    ProfilerBuffer - Supplies a pointer to a profiler data buffer from which
        up to data length bytes will be read.

    Data - Supplies an array of bytes that is to receive data from the profiler
        buffer.

    DataLength - Supplies the maximum number of bytes that can be read into the
        data buffer. Receives the total bytes read upon return.

Return Value:

    Returns TRUE if there is more data to be read, or FALSE otherwise..

--*/

{

    ULONG AvailableLength;
    ULONG BytesRead;
    ULONG ConsumerIndex;
    ULONG ConsumerStopIndex;
    ULONG FirstReadLength;
    BOOL MoreDataAvailable;
    ULONG ProducerIndex;
    ULONG RemainingLength;
    ULONG SecondReadLength;
    ULONG TotalReadLength;

    ASSERT(ProfilerBuffer != NULL);
    ASSERT(Data != NULL);
    ASSERT(DataLength != NULL);

    ConsumerIndex = ProfilerBuffer->ConsumerIndex;
    ProducerIndex = ProfilerBuffer->ProducerIndex;
    ConsumerStopIndex = ProfilerBuffer->ConsumerStopIndex;
    SecondReadLength = 0;
    AvailableLength = *DataLength;
    *DataLength = 0;
    MoreDataAvailable = FALSE;

    //
    // If the stop index equals the consumer index, then advance it to
    // the producer index in order to gather all of the currently available
    // data. Do this so that the consumer will eventually complete when faced
    // with a speedy producer.
    //

    if (ConsumerIndex == ConsumerStopIndex) {
        ProfilerBuffer->ConsumerStopIndex = ProducerIndex;
    }

    //
    // If the producer's and consumer's indices are equal, then there are no
    // bytes to consume. The buffer is empty.
    //

    if (ProducerIndex == ConsumerIndex) {
        goto EmptyProfilerBufferEnd;

    //
    // If the producer is ahead of the consumer, then consume the buffer all
    // the way up to the producer's index or up to the provided buffer size.
    //

    } else if (ProducerIndex > ConsumerIndex) {
        FirstReadLength = ProducerIndex - ConsumerIndex;
        if (FirstReadLength > AvailableLength) {
            FirstReadLength = AvailableLength;
        }

    //
    // If the producer is behind the consumer, then two reads are required to
    // wrap around the circular buffer. Truncate based on the provided data
    // length.
    //

    } else {

        ASSERT(ProducerIndex < ConsumerIndex);

        FirstReadLength = PROFILER_BUFFER_LENGTH - ConsumerIndex;
        if (FirstReadLength > AvailableLength) {
            FirstReadLength = AvailableLength;

        } else {
            SecondReadLength = ProducerIndex;
            if ((FirstReadLength + SecondReadLength) > AvailableLength) {
                SecondReadLength = AvailableLength - FirstReadLength;
            }
        }
    }

    TotalReadLength = FirstReadLength + SecondReadLength;

    //
    // The provided data buffer should be large enough to fit the determined
    // reads.
    //

    ASSERT(AvailableLength >= TotalReadLength);

    //
    // Read the data out into the supplied buffer, making sure to read on the
    // profiler unit boundary, as marked by the sentinel.
    //

    RemainingLength = TotalReadLength;
    BytesRead = SppReadProfilerData(&(Data[0]),
                                    &(ProfilerBuffer->Buffer[ConsumerIndex]),
                                    FirstReadLength,
                                    &RemainingLength);

    if ((SecondReadLength != 0) && (BytesRead == FirstReadLength)) {

        ASSERT(RemainingLength == SecondReadLength);

        BytesRead = SppReadProfilerData(&(Data[FirstReadLength]),
                                         &(ProfilerBuffer->Buffer[0]),
                                         SecondReadLength,
                                         &RemainingLength);

        ASSERT(SecondReadLength == (BytesRead + RemainingLength));

        ConsumerIndex = BytesRead;

    } else {
        ConsumerIndex += BytesRead;
    }

    //
    // Update the data length based on how much data was read.
    //

    *DataLength = TotalReadLength - RemainingLength;

    //
    // Update the consumer index.
    //

    if (ConsumerIndex == PROFILER_BUFFER_LENGTH) {
        ProfilerBuffer->ConsumerIndex = 0;

    } else {
        ProfilerBuffer->ConsumerIndex = ConsumerIndex;
    }

    //
    // If the stop index has been reached with this read, let the caller know
    // that there is no more data to collect at this time.
    //

    if (ProfilerBuffer->ConsumerIndex != ProfilerBuffer->ConsumerStopIndex) {
        MoreDataAvailable = TRUE;
    }

EmptyProfilerBufferEnd:
    return MoreDataAvailable;
}

ULONG
SppReadProfilerData (
    BYTE *Destination,
    BYTE *Source,
    ULONG ByteCount,
    PULONG BytesRemaining
    )

/*++

Routine Description:

    This routine reads as many profiler data units as it can, up to the
    supplied byte count, making sure to never exceed the remaining available
    bytes.

Arguments:

    Destination - Supplies a pointer to the destination data buffer.

    Source - Supplies a pointer to the source data buffer.

    ByteCount - Supplies the maximum number of bytes that should be read out of
        the source buffer.

    BytesRemaining - Supplies a pointer to the maximum number of bytes that
        can be read to the destination buffer. It is updated upon return.

Return Value:

    Returns the number of bytes read by this routine.

--*/

{

    ULONG BytesRead;
    ULONG DestinationIndex;
    ULONG SourceIndex;
    ULONG Value;

    BytesRead = 0;
    DestinationIndex = 0;
    for (SourceIndex = 0; SourceIndex < ByteCount; SourceIndex += 1) {

        //
        // If the current byte is the start of the sentinel, check the length
        // of the next data packet and do not continue if it will not fit in
        // the destination buffer.
        //

        Value = *((PULONG)&(Source[SourceIndex]));
        if (IS_PROFILER_DATA_SENTINEL(Value) != FALSE) {
            if (GET_PROFILER_DATA_SIZE(Value) > *BytesRemaining) {
                break;
            }
        }

        Destination[DestinationIndex] = Source[SourceIndex];
        DestinationIndex += 1;
        *BytesRemaining -= 1;
        BytesRead += 1;
    }

    return BytesRead;
}

KSTATUS
SppInitializeStackSampling (
    VOID
    )

/*++

Routine Description:

    This routine initializes the system's profiling data structures.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    ULONG Index;
    ULONG ProcessorCount;
    PPROFILER_BUFFER ProfilerBuffer;
    PPROFILER_BUFFER *StackSamplingArray;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT((SpEnabledFlags & PROFILER_TYPE_FLAG_STACK_SAMPLING) == 0);
    ASSERT(KeIsQueuedLockHeld(SpProfilingQueuedLock) != FALSE);
    ASSERT(SpStackSamplingArray == NULL);
    ASSERT(SpStackSamplingArraySize == 0);

    ProcessorCount = KeGetActiveProcessorCount();
    AllocationSize = ProcessorCount * sizeof(PPROFILER_BUFFER);
    StackSamplingArray = MmAllocateNonPagedPool(AllocationSize,
                                                SP_ALLOCATION_TAG);

    if (StackSamplingArray == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeProfilerEnd;
    }

    //
    // Now fill in the array with profiler buffers.
    //

    RtlZeroMemory(StackSamplingArray, AllocationSize);
    for (Index = 0; Index < ProcessorCount; Index += 1) {
        ProfilerBuffer = MmAllocateNonPagedPool(sizeof(PROFILER_BUFFER),
                                                SP_ALLOCATION_TAG);

        if (ProfilerBuffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializeProfilerEnd;
        }

        RtlZeroMemory(ProfilerBuffer, sizeof(PROFILER_BUFFER));
        StackSamplingArray[Index] = ProfilerBuffer;
    }

    //
    // Start the timer and then mark the profiler as enabled and update the
    // stack sampling globals. This might cause some initial interrupts to skip
    // data collection, but that's OK.
    //

    Status = HlStartProfilerTimer();
    if (!KSUCCESS(Status)) {
        goto InitializeProfilerEnd;
    }

    SpStackSamplingArray = StackSamplingArray;
    SpStackSamplingArraySize = ProcessorCount;
    RtlMemoryBarrier();
    SpEnabledFlags |= PROFILER_TYPE_FLAG_STACK_SAMPLING;

InitializeProfilerEnd:
    if (!KSUCCESS(Status)) {
        for (Index = 0; Index < ProcessorCount; Index += 1) {
            if (StackSamplingArray[Index] != NULL) {
                MmFreeNonPagedPool(StackSamplingArray[Index]);
            }
        }

        MmFreeNonPagedPool(StackSamplingArray);
    }

    return Status;
}

VOID
SppDestroyStackSampling (
    ULONG Phase
    )

/*++

Routine Description:

    This routine tears down stack sampling by disabling the profiler timer and
    destroy the stack sampling data structures. Phase 0 stops the stack
    sampling profiler producers and consumers. Phase 1 cleans up resources.

Arguments:

    Phase - Supplies the current phase of the destruction process.

Return Value:

    None.

--*/

{

    ULONG Index;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(KeIsQueuedLockHeld(SpProfilingQueuedLock) != FALSE);
    ASSERT(SpStackSamplingArray != NULL);
    ASSERT(SpStackSamplingArraySize != 0);

    if (Phase == 0) {

        ASSERT((SpEnabledFlags & PROFILER_TYPE_FLAG_STACK_SAMPLING) != 0);

        //
        // Disable stack sampling before disabling the profiler timer to
        // prevent any pending producer interrupts from touching the buffers
        // after they are released
        //

        SpEnabledFlags &= ~PROFILER_TYPE_FLAG_STACK_SAMPLING;

        //
        // Stop the profiler timer. Since the caller will wait for at least
        // one more clock interrupt, it is safe to proceed even though stopping
        // the timer doesn't guarantee the profiler interrupt will not run
        // again. It could be pending on another processor. The wait for the
        // clock interrupt will guarantee that the all high level and IPI
        // interrupts have completed.
        //

        HlStopProfilerTimer();

    } else {

        ASSERT(Phase == 1);
        ASSERT((SpEnabledFlags & PROFILER_TYPE_FLAG_STACK_SAMPLING) == 0);

        //
        // Destroy the stack sampling array.
        //

        for (Index = 0; Index < SpStackSamplingArraySize; Index += 1) {
            if (SpStackSamplingArray[Index] != NULL) {
                MmFreeNonPagedPool(SpStackSamplingArray[Index]);
            }
        }

        MmFreeNonPagedPool(SpStackSamplingArray);
        SpStackSamplingArray = NULL;
        SpStackSamplingArraySize = 0;
    }

    return;
}

KSTATUS
SppInitializeMemoryStatistics (
    VOID
    )

/*++

Routine Description:

    This routine initializes the structures and timers necessary for profiling
    system memory statistics.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    ULONGLONG Period;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(KeIsQueuedLockHeld(SpProfilingQueuedLock) != FALSE);
    ASSERT(SpMemory == NULL);

    //
    // Allocate the memory profiler structure.
    //

    SpMemory = MmAllocateNonPagedPool(sizeof(MEMORY_PROFILER),
                                      SP_ALLOCATION_TAG);

    if (SpMemory == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeMemoryStatisticsEnd;
    }

    RtlZeroMemory(SpMemory, sizeof(MEMORY_PROFILER));

    ASSERT(SpMemory->ConsumerActive == FALSE);
    ASSERT(SpMemory->ReadyIndex == 0);
    ASSERT(SpMemory->ProducerIndex == 0);

    SpMemory->ConsumerIndex = MEMORY_BUFFER_COUNT - 1;

    //
    // Create the timer that will periodically trigger memory statistics.
    //

    SpMemory->Timer = KeCreateTimer(SP_ALLOCATION_TAG);
    if (SpMemory->Timer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeMemoryStatisticsEnd;
    }

    //
    // Queue the timer.
    //

    Period = KeConvertMicrosecondsToTimeTicks(MEMORY_STATISTICS_TIMER_PERIOD);
    Status = KeQueueTimer(SpMemory->Timer,
                          TimerQueueSoft,
                          0,
                          Period,
                          0,
                          NULL);

    if (!KSUCCESS(Status)) {
        goto InitializeMemoryStatisticsEnd;
    }

    //
    // Create the worker thread, which will wait on the timer. Add an extra
    // reference because the destruction routine waits until this thread exits.
    //

    SpMemory->ThreadAlive = TRUE;
    Status = PsCreateKernelThread(SppMemoryStatisticsThread,
                                  NULL,
                                  "SppMemoryStatisticsThread");

    if (!KSUCCESS(Status)) {
        SpMemory->ThreadAlive = FALSE;
        goto InitializeMemoryStatisticsEnd;
    }

    //
    // Make sure everything above is complete before turning this on.
    //

    RtlMemoryBarrier();
    SpEnabledFlags |= PROFILER_TYPE_FLAG_MEMORY_STATISTICS;

InitializeMemoryStatisticsEnd:
    if (!KSUCCESS(Status)) {
        if (SpMemory != NULL) {
            if (SpMemory->Timer != NULL) {
                KeDestroyTimer(SpMemory->Timer);
            }

            //
            // Thread creation should be the last point of failure.
            //

            ASSERT(SpMemory->ThreadAlive == FALSE);

            MmFreeNonPagedPool(SpMemory);
            SpMemory = NULL;
        }
    }

    return Status;
}

VOID
SppDestroyMemoryStatistics (
    ULONG Phase
    )

/*++

Routine Description:

    This routine destroys the structures and timers used to profile system
    memory statistics. Phase 0 stops the memory profiler producers and
    consumers. Phase 1 cleans up resources.

Arguments:

    Phase - Supplies the current phase of the destruction process.

Return Value:

    None.

--*/

{

    ULONG Index;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(KeIsQueuedLockHeld(SpProfilingQueuedLock) != FALSE);
    ASSERT(SpMemory != NULL);
    ASSERT(SpMemory->Timer != NULL);

    if (Phase == 0) {

        ASSERT(SpMemory->ThreadAlive != FALSE);
        ASSERT((SpEnabledFlags & PROFILER_TYPE_FLAG_MEMORY_STATISTICS) != 0);

        //
        // Disable the memory statistics profiler.
        //

        SpEnabledFlags &= ~PROFILER_TYPE_FLAG_MEMORY_STATISTICS;

        //
        // Cancel the timer. This is a periodic timer, so cancel should always
        // succeed.
        //

        Status = KeCancelTimer(SpMemory->Timer);

        ASSERT(KSUCCESS(Status));

        //
        // Queue the timer one more time in case the worker thread was in the
        // act of waiting when the timer was cancelled or was processing data.
        //

        Status = KeQueueTimer(SpMemory->Timer,
                              TimerQueueSoftWake,
                              0,
                              0,
                              0,
                              NULL);

        ASSERT(KSUCCESS(Status));

        //
        // Wait until the thread exits in order to be sure that it has
        // registered that profiling has been cancelled.
        //

        while (SpMemory->ThreadAlive != FALSE) {
            KeYield();
        }

    } else {

        ASSERT(Phase == 1);
        ASSERT((SpEnabledFlags & PROFILER_TYPE_FLAG_MEMORY_STATISTICS) == 0);
        ASSERT(SpMemory->ThreadAlive == FALSE);

        //
        // Destroy the timer.
        //

        KeDestroyTimer(SpMemory->Timer);

        //
        // Release any buffers that are holding pool statistics.
        //

        for (Index = 0; Index < MEMORY_BUFFER_COUNT; Index += 1) {
            if (SpMemory->MemoryBuffers[Index].Buffer != NULL) {
                MmFreeNonPagedPool(SpMemory->MemoryBuffers[Index].Buffer);
            }
        }

        MmFreeNonPagedPool(SpMemory);
        SpMemory = NULL;
    }

    return;
}

VOID
SppMemoryStatisticsThread (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine is the workhorse for gathering memory statistics and writing
    them to a buffer than can then be consumed on the clock interrupt. It waits
    on the memory statistics timer before periodically collecting the
    statistics.

Arguments:

    Parameter - Supplies a pointer supplied by the creator of the thread. This
        pointer is not used.

Return Value:

    None.

--*/

{

    PVOID Buffer;
    ULONG BufferSize;
    ULONG Index;
    PMEMORY_BUFFER MemoryBuffer;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(SpMemory->ThreadAlive != FALSE);

    while (TRUE) {

        //
        // Wait for the memory statistics timer to expire.
        //

        ObWaitOnObject(SpMemory->Timer, 0, WAIT_TIME_INDEFINITE);

        //
        // Check to make sure memory statistics profiling is still enabled.
        //

        if ((SpEnabledFlags & PROFILER_TYPE_FLAG_MEMORY_STATISTICS) == 0) {
            break;
        }

        //
        // Call the memory manager to get the latest pool statistics. It will
        // pass back an appropriately sized buffer with all the statistics.
        //

        Status = MmGetPoolProfilerStatistics(&Buffer,
                                             &BufferSize,
                                             SP_ALLOCATION_TAG);

        if (!KSUCCESS(Status)) {
            continue;
        }

        //
        // Get the producer's memory buffer.
        //

        ASSERT(SpMemory->ProducerIndex < MEMORY_BUFFER_COUNT);

        MemoryBuffer = &(SpMemory->MemoryBuffers[SpMemory->ProducerIndex]);

        //
        // Destroy what is currently in the memory buffer.
        //

        if (MemoryBuffer->Buffer != NULL) {
            MmFreeNonPagedPool(MemoryBuffer->Buffer);
        }

        //
        // Reinitialize the buffer.
        //

        MemoryBuffer->Buffer = Buffer;
        MemoryBuffer->BufferSize = BufferSize;
        MemoryBuffer->ConsumerIndex = 0;

        //
        // Now that this is the latest and greatest memory information, point
        // the ready index at it. It doesn't matter that the ready index and
        // the producer index will temporarily be the same. There is a
        // guarantee that the producer will not produce again until it points
        // at a new buffer. This makes it safe for the consumer to just grab
        // the ready index.
        //

        SpMemory->ReadyIndex = SpMemory->ProducerIndex;

        //
        // Now search for the free buffer and make it the producer index. There
        // always has to be one free.
        //

        for (Index = 0; Index < MEMORY_BUFFER_COUNT; Index += 1) {
            if ((Index != SpMemory->ReadyIndex) &&
                (Index != SpMemory->ConsumerIndex)) {

                SpMemory->ProducerIndex = Index;
                break;
            }
        }

        ASSERT(SpMemory->ReadyIndex != SpMemory->ProducerIndex);
    }

    SpMemory->ThreadAlive = FALSE;
    return;
}

KSTATUS
SppInitializeThreadStatistics (
    VOID
    )

/*++

Routine Description:

    This routine initializes the system's thread profiling data structures.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    ULONG Index;
    RUNLEVEL OldRunLevel;
    ULONG ProcessorCount;
    ULONG ProcessorNumber;
    PPROFILER_BUFFER ProfilerBuffer;
    KSTATUS Status;
    SYSTEM_TIME SystemTime;
    PPROFILER_BUFFER *ThreadStatisticsArray;
    PROFILER_THREAD_TIME_COUNTER TimeCounterEvent;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT((SpEnabledFlags & PROFILER_TYPE_FLAG_THREAD_STATISTICS) == 0);
    ASSERT(KeIsQueuedLockHeld(SpProfilingQueuedLock) != FALSE);
    ASSERT(SpThreadStatisticsArray == NULL);
    ASSERT(SpThreadStatisticsArraySize == 0);

    ProcessorCount = KeGetActiveProcessorCount();
    AllocationSize = ProcessorCount * sizeof(PPROFILER_BUFFER);
    ThreadStatisticsArray = MmAllocateNonPagedPool(AllocationSize,
                                                   SP_ALLOCATION_TAG);

    if (ThreadStatisticsArray == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeProfilerEnd;
    }

    //
    // Now fill in the array with profiler buffers.
    //

    RtlZeroMemory(ThreadStatisticsArray, AllocationSize);
    for (Index = 0; Index < ProcessorCount; Index += 1) {
        ProfilerBuffer = MmAllocateNonPagedPool(sizeof(PROFILER_BUFFER),
                                                SP_ALLOCATION_TAG);

        if (ProfilerBuffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializeProfilerEnd;
        }

        RtlZeroMemory(ProfilerBuffer, sizeof(PROFILER_BUFFER));
        ThreadStatisticsArray[Index] = ProfilerBuffer;
    }

    SpThreadStatisticsArray = ThreadStatisticsArray;
    SpThreadStatisticsArraySize = ProcessorCount;

    //
    // Enable profiling by filling in the function pointer.
    //

    SpCollectThreadStatisticRoutine = SppCollectThreadStatistic;
    SpProcessNewProcessRoutine = SppProcessNewProcess;
    SpProcessNewThreadRoutine = SppProcessNewThread;
    RtlMemoryBarrier();
    SpEnabledFlags |= PROFILER_TYPE_FLAG_THREAD_STATISTICS;

    //
    // Raise to dispatch (so that no thread events are added on this processor)
    // and add the first event, a time counter synchronization event.
    //

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    ProcessorNumber = KeGetCurrentProcessorNumber();
    TimeCounterEvent.EventType = ProfilerThreadEventTimeCounter;
    TimeCounterEvent.TimeCounter = HlQueryTimeCounter();
    KeGetSystemTime(&SystemTime);
    TimeCounterEvent.SystemTimeSeconds = SystemTime.Seconds;
    TimeCounterEvent.SystemTimeNanoseconds = SystemTime.Nanoseconds;
    TimeCounterEvent.TimeCounterFrequency = HlQueryTimeCounterFrequency();
    SppWriteProfilerBuffer(SpThreadStatisticsArray[ProcessorNumber],
                           (BYTE *)&TimeCounterEvent,
                           sizeof(PROFILER_THREAD_TIME_COUNTER));

    KeLowerRunLevel(OldRunLevel);
    SppSendInitialProcesses();
    Status = STATUS_SUCCESS;

InitializeProfilerEnd:
    if (!KSUCCESS(Status)) {
        for (Index = 0; Index < ProcessorCount; Index += 1) {
            if (ThreadStatisticsArray[Index] != NULL) {
                MmFreeNonPagedPool(ThreadStatisticsArray[Index]);
            }
        }

        MmFreeNonPagedPool(ThreadStatisticsArray);
    }

    return Status;
}

VOID
SppSendInitialProcesses (
    VOID
    )

/*++

Routine Description:

    This routine sends the initial set of process and threads active on the
    system. This routine must be called at low level.

Arguments:

    None.

Return Value:

    None.

--*/

{

    BOOL Added;
    ULONG ConsumedSize;
    PPROFILER_THREAD_NEW_PROCESS Event;
    ULONG MaxNameSize;
    PSTR Name;
    ULONG NameSize;
    RUNLEVEL OldRunLevel;
    PPROCESS_INFORMATION Process;
    PPROCESS_INFORMATION ProcessList;
    UINTN ProcessListSize;
    ULONG ProcessorNumber;
    KSTATUS Status;
    KSTATUS ThreadStatus;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    ProcessList = NULL;
    Status = PsGetAllProcessInformation(SP_ALLOCATION_TAG,
                                        (PVOID)&ProcessList,
                                        &ProcessListSize);

    if (!KSUCCESS(Status)) {
        goto SendInitialProcessesEnd;
    }

    ConsumedSize = 0;
    MaxNameSize = SCRATCH_BUFFER_LENGTH -
                  FIELD_OFFSET(PROFILER_THREAD_NEW_PROCESS, Name);

    Process = ProcessList;
    while (ConsumedSize < ProcessListSize) {
        OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
        ProcessorNumber = KeGetCurrentProcessorNumber();
        Event = (PVOID)(SpThreadStatisticsArray[ProcessorNumber]->Scratch);
        Event->EventType = ProfilerThreadEventNewProcess;
        NameSize = Process->NameLength * sizeof(CHAR);
        if (NameSize > MaxNameSize) {
            NameSize = MaxNameSize;
        }

        Event->StructureSize = sizeof(PROFILER_THREAD_NEW_PROCESS);
        if (NameSize != 0) {
            Event->StructureSize -= ANYSIZE_ARRAY * sizeof(CHAR);
            Event->StructureSize += NameSize;
            Name = (PSTR)((PVOID)Process + Process->NameOffset);
            RtlStringCopy(Event->Name, Name, NameSize);

        } else {
            Event->Name[0] = STRING_TERMINATOR;
        }

        Event->ProcessId = Process->ProcessId;
        Event->TimeCounter = 0;
        Added = SppWriteProfilerBuffer(SpThreadStatisticsArray[ProcessorNumber],
                                       (BYTE *)Event,
                                       Event->StructureSize);

        if (Added == FALSE) {
            Status = STATUS_BUFFER_TOO_SMALL;
        }

        KeLowerRunLevel(OldRunLevel);
        ThreadStatus = SppSendInitialThreads(Process->ProcessId);
        if (!KSUCCESS(ThreadStatus)) {
            Status = ThreadStatus;
        }

        ConsumedSize += Process->StructureSize;

        ASSERT(ConsumedSize <= ProcessListSize);

        Process = (PPROCESS_INFORMATION)((PUCHAR)Process +
                                         Process->StructureSize);
    }

    Status = STATUS_SUCCESS;

SendInitialProcessesEnd:
    if (ProcessList != NULL) {
        MmFreeNonPagedPool(ProcessList);
    }

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Profiler: Failed to send initial processes: %d.\n",
                      Status);
    }

    return;
}

KSTATUS
SppSendInitialThreads (
    PROCESS_ID ProcessId
    )

/*++

Routine Description:

    This routine sends the initial set of threads for the given process.
    This routine must be called at dispatch level.

Arguments:

    ProcessId - Supplies the process ID of the threads to send.

Return Value:

    None.

--*/

{

    BOOL Added;
    ULONG ConsumedSize;
    PPROFILER_THREAD_NEW_THREAD Event;
    ULONG MaxNameSize;
    ULONG NameSize;
    RUNLEVEL OldRunLevel;
    ULONG ProcessorNumber;
    KSTATUS Status;
    PTHREAD_INFORMATION Thread;
    PTHREAD_INFORMATION ThreadList;
    ULONG ThreadListSize;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    if ((SpEnabledFlags & PROFILER_TYPE_FLAG_THREAD_STATISTICS) == 0) {
        return STATUS_SUCCESS;
    }

    ThreadList = NULL;
    Status = PsGetThreadList(ProcessId,
                             SP_ALLOCATION_TAG,
                             (PVOID)&ThreadList,
                             &ThreadListSize);

    if (!KSUCCESS(Status)) {
        goto SendInitialThreadsEnd;
    }

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    ProcessorNumber = KeGetCurrentProcessorNumber();
    Event = (PVOID)(SpThreadStatisticsArray[ProcessorNumber]->Scratch);
    ConsumedSize = 0;
    MaxNameSize = SCRATCH_BUFFER_LENGTH -
                  FIELD_OFFSET(PROFILER_THREAD_NEW_THREAD, Name);

    Event->EventType = ProfilerThreadEventNewThread;
    Thread = ThreadList;
    while (ConsumedSize < ThreadListSize) {

        ASSERT(Thread->StructureSize >= sizeof(THREAD_INFORMATION));

        NameSize = Thread->StructureSize -
                   FIELD_OFFSET(THREAD_INFORMATION, Name);

        if (NameSize > MaxNameSize) {
            NameSize = MaxNameSize;
        }

        Event->StructureSize = sizeof(PROFILER_THREAD_NEW_THREAD) -
                               (ANYSIZE_ARRAY * sizeof(CHAR)) + NameSize;

        Event->ProcessId = ProcessId;
        Event->ThreadId = Thread->ThreadId;
        Event->TimeCounter = 0;
        RtlStringCopy(Event->Name, Thread->Name, NameSize);
        Added = SppWriteProfilerBuffer(SpThreadStatisticsArray[ProcessorNumber],
                                       (BYTE *)Event,
                                       Event->StructureSize);

        if (Added == FALSE) {
            Status = STATUS_BUFFER_TOO_SMALL;
        }

        ConsumedSize += Thread->StructureSize;

        ASSERT(ConsumedSize <= ThreadListSize);

        Thread = (PTHREAD_INFORMATION)((PUCHAR)Thread + Thread->StructureSize);
    }

    KeLowerRunLevel(OldRunLevel);
    Status = STATUS_SUCCESS;

SendInitialThreadsEnd:
    if (ThreadList != NULL) {
        MmFreeNonPagedPool(ThreadList);
    }

    return Status;
}

VOID
SppProcessNewProcess (
    PROCESS_ID ProcessId
    )

/*++

Routine Description:

    This routine collects statistics on a created process.

Arguments:

    ProcessId - Supplies the ID of the process being created.

Return Value:

    None.

--*/

{

    BOOL Added;
    PPROFILER_THREAD_NEW_PROCESS Event;
    ULONG MaxNameSize;
    PSTR Name;
    ULONG NameSize;
    RUNLEVEL OldRunLevel;
    PPROCESS_INFORMATION Process;
    ULONG ProcessorNumber;
    UINTN ProcessSize;
    KSTATUS Status;

    if ((SpEnabledFlags & PROFILER_TYPE_FLAG_THREAD_STATISTICS) == 0) {
        return;
    }

    ProcessSize = PROFILER_PROCESS_INFORMATION_SIZE;
    Process = MmAllocateNonPagedPool(ProcessSize, SP_ALLOCATION_TAG);
    if (Process == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ProcessNewProcessEnd;
    }

    Process->Version = PROCESS_INFORMATION_VERSION;
    Status = PsGetProcessInformation(ProcessId, Process, &ProcessSize);
    if (!KSUCCESS(Status)) {
        goto ProcessNewProcessEnd;
    }

    MaxNameSize = SCRATCH_BUFFER_LENGTH -
                  FIELD_OFFSET(PROFILER_THREAD_NEW_PROCESS, Name);

    NameSize = Process->NameLength * sizeof(CHAR);
    if (NameSize > MaxNameSize) {
        NameSize = MaxNameSize;
    }

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    ProcessorNumber = KeGetCurrentProcessorNumber();
    Event = (PVOID)(SpThreadStatisticsArray[ProcessorNumber]->Scratch);
    Event->EventType = ProfilerThreadEventNewProcess;
    Event->StructureSize = sizeof(PROFILER_THREAD_NEW_PROCESS);
    if (NameSize != 0) {
        Event->StructureSize -= ANYSIZE_ARRAY * sizeof(CHAR);
        Event->StructureSize += NameSize;
        Name = (PSTR)((PVOID)Process + Process->NameOffset);
        RtlStringCopy(Event->Name, Name, NameSize);

    } else {
        Event->Name[0] = STRING_TERMINATOR;
    }

    Event->ProcessId = Process->ProcessId;
    Event->TimeCounter = HlQueryTimeCounter();
    Added = SppWriteProfilerBuffer(SpThreadStatisticsArray[ProcessorNumber],
                                   (BYTE *)Event,
                                   Event->StructureSize);

    Status = STATUS_SUCCESS;
    if (Added == FALSE) {
        Status = STATUS_BUFFER_TOO_SMALL;
    }

    KeLowerRunLevel(OldRunLevel);

ProcessNewProcessEnd:
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Warning: Unable to add profiling event for new "
                      "process %d.\n",
                      ProcessId);
    }

    if (Process != NULL) {
        MmFreeNonPagedPool(Process);
    }

    return;
}

VOID
SppProcessNewThread (
    PROCESS_ID ProcessId,
    THREAD_ID ThreadId
    )

/*++

Routine Description:

    This routine collects statistics on a created thread.

Arguments:

    ProcessId - Supplies the ID of the process creating the new thread.

    ThreadId - Supplies the ID of the new thread being created.

Return Value:

    None.

--*/

{

    BOOL Added;
    PPROFILER_THREAD_NEW_THREAD Event;
    ULONG NameSize;
    RUNLEVEL OldRunLevel;
    ULONG ProcessorNumber;
    KSTATUS Status;
    PTHREAD_INFORMATION Thread;
    ULONG ThreadSize;

    if ((SpEnabledFlags & PROFILER_TYPE_FLAG_THREAD_STATISTICS) == 0) {
        return;
    }

    ThreadSize = PROFILER_THREAD_INFORMATION_SIZE;
    Thread = MmAllocateNonPagedPool(ThreadSize,  SP_ALLOCATION_TAG);
    if (Thread == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ProcessNewThreadEnd;
    }

    Status = PsGetThreadInformation(ProcessId, ThreadId, Thread, &ThreadSize);
    if (!KSUCCESS(Status)) {
        goto ProcessNewThreadEnd;
    }

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    ProcessorNumber = KeGetCurrentProcessorNumber();
    Event = (PVOID)(SpThreadStatisticsArray[ProcessorNumber]->Scratch);
    Event->EventType = ProfilerThreadEventNewThread;

    ASSERT(Thread->StructureSize >= sizeof(THREAD_INFORMATION));

    NameSize = Thread->StructureSize -
               FIELD_OFFSET(THREAD_INFORMATION, Name);

    ASSERT(NameSize < ThreadSize);

    Event->StructureSize = sizeof(PROFILER_THREAD_NEW_THREAD) -
                           (ANYSIZE_ARRAY * sizeof(CHAR)) + NameSize;

    Event->ProcessId = ProcessId;
    Event->ThreadId = Thread->ThreadId;
    Event->TimeCounter = HlQueryTimeCounter();
    RtlStringCopy(Event->Name, Thread->Name, NameSize);
    Added = SppWriteProfilerBuffer(SpThreadStatisticsArray[ProcessorNumber],
                                   (BYTE *)Event,
                                   Event->StructureSize);

    Status = STATUS_SUCCESS;
    if (Added == FALSE) {
        Status = STATUS_BUFFER_TOO_SMALL;
    }

    KeLowerRunLevel(OldRunLevel);

ProcessNewThreadEnd:
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Warning: Unable to add profiling event for new "
                      "thread %d (Process %d).\n",
                      ThreadId,
                      ProcessId);
    }

    if (Thread != NULL) {
        MmFreeNonPagedPool(Thread);
    }

    return;
}
VOID
SppDestroyThreadStatistics (
    ULONG Phase
    )

/*++

Routine Description:

    This routine tears down thread profiling. Phase 0 stops the thread
    statistics producers and consumers. Phase 1 cleans up resources.

Arguments:

    Phase - Supplies the current phase of the destruction process.

Return Value:

    None.

--*/

{

    ULONG Index;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(KeIsQueuedLockHeld(SpProfilingQueuedLock) != FALSE);
    ASSERT(SpThreadStatisticsArray != NULL);
    ASSERT(SpThreadStatisticsArraySize != 0);

    if (Phase == 0) {

        ASSERT((SpEnabledFlags & PROFILER_TYPE_FLAG_THREAD_STATISTICS) != 0);

        //
        // Disable thread statistics before disabling the profiler function to
        // prevent any pending producers from touching the buffers after they
        // are released
        //

        SpEnabledFlags &= ~PROFILER_TYPE_FLAG_THREAD_STATISTICS;

        //
        // Clear the function pointer to officially take the profiling down.
        //

        SpCollectThreadStatisticRoutine = NULL;
        RtlMemoryBarrier();

    } else {

        ASSERT(Phase == 1);
        ASSERT((SpEnabledFlags & PROFILER_TYPE_FLAG_THREAD_STATISTICS) == 0);

        //
        // Destroy the stack sampling array.
        //

        for (Index = 0; Index < SpThreadStatisticsArraySize; Index += 1) {
            if (SpThreadStatisticsArray[Index] != NULL) {
                MmFreeNonPagedPool(SpThreadStatisticsArray[Index]);
            }
        }

        MmFreeNonPagedPool(SpThreadStatisticsArray);
        SpThreadStatisticsArray = NULL;
        SpThreadStatisticsArraySize = 0;
    }

    return;
}

VOID
SppCollectThreadStatistic (
    PKTHREAD Thread,
    PPROCESSOR_BLOCK Processor,
    SCHEDULER_REASON ScheduleOutReason
    )

/*++

Routine Description:

    This routine collects statistics on a thread that is being scheduled out.
    This routine must be called at dispatch level inside the scheduler.

Arguments:

    Thread - Supplies a pointer to the thread being scheduled out.

    Processor - Supplies a pointer to the executing processor block.

    ScheduleOutReason - Supplies the reason the thread is being scheduled out.

Return Value:

    None.

--*/

{

    PPROFILER_CONTEXT_SWAP ContextSwap;
    ULONG ProcessorNumber;

    if ((SpEnabledFlags & PROFILER_TYPE_FLAG_THREAD_STATISTICS) == 0) {
        return;
    }

    //
    // Do not collect data on processors that have not been initialized for
    // profiling.
    //

    if (Processor->ProcessorNumber >= SpThreadStatisticsArraySize) {
        return;
    }

    ProcessorNumber = Processor->ProcessorNumber;

    ASSERT(sizeof(PROFILER_CONTEXT_SWAP) < SCRATCH_BUFFER_LENGTH);

    ContextSwap = (PVOID)(SpThreadStatisticsArray[ProcessorNumber]->Scratch);
    ContextSwap->EventType = ScheduleOutReason;
    ContextSwap->TimeCount = HlQueryTimeCounter();
    ContextSwap->BlockingQueue = (UINTN)NULL;
    if (ScheduleOutReason == SchedulerReasonThreadBlocking) {
        ContextSwap->BlockingQueue = (UINTN)(ObGetBlockingQueue(Thread));
    }

    ContextSwap->ThreadId = Thread->ThreadId;
    ContextSwap->ProcessId = Thread->OwningProcess->Identifiers.ProcessId;

    //
    // Write the data to the sampling buffer.
    //

    SppWriteProfilerBuffer(SpThreadStatisticsArray[ProcessorNumber],
                           (BYTE *)ContextSwap,
                           sizeof(PROFILER_CONTEXT_SWAP));

    return;
}

