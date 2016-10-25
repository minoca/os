/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sp.h

Abstract:

    This header contains definitions for the System Profiler.

Author:

    Chris Stevens 1-Jul-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/debug/spproto.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro collects thread statistics. It simply calls the function pointer
// if it is enabled.
//

#define SpCollectThreadStatistic(_Thread, _Processor, _ScheduleOutReason)   \
    if (SpCollectThreadStatisticRoutine != NULL) {                          \
        SpCollectThreadStatisticRoutine((_Thread),                          \
                                        (_Processor),                       \
                                        (_ScheduleOutReason));              \
    }

#define SpProcessNewProcess(_ProcessId)         \
    if (SpProcessNewProcessRoutine != NULL) {   \
        SpProcessNewProcessRoutine(_ProcessId); \
    }

#define SpProcessNewThread(_ProcessId, _ThreadId)          \
    if (SpProcessNewThreadRoutine != NULL) {               \
        SpProcessNewThreadRoutine(_ProcessId, _ThreadId);  \
    }

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _SP_INFORMATION_TYPE {
    SpInformationInvalid,
    SpInformationGetSetState,
} SP_INFORMATION_TYPE, *PSP_INFORMATION_TYPE;

/*++

Enumeration Descriptoin:

    This enumeration describes the various operations for getting or setting
    profiler state information.

Values:

    SpGetSetStateOperationNone - Indicates to take no action.

    SpGetSetStateOperationOverwrite - Indicates that the supplied profiler
        state should overwrite the current state. An profilers that are
        currently running, but not set in the supplied state will be disabled.

    SpGetSetStateOperationEnable - Indicates that the supplied profiler types
        should be enabled. Other currently enabled profiler types' state will
        not be changed.

    SpGetSetStateOperationDisable - Indicates that the supplied profiler types
        should be disabled. Other currently enabled profiler types' state will
        not be changed.

--*/

typedef enum _SP_GET_SET_STATE_OPERATION {
    SpGetSetStateOperationNone,
    SpGetSetStateOperationOverwrite,
    SpGetSetStateOperationEnable,
    SpGetSetStateOperationDisable,
} SP_GET_SET_STATE_OPERATION, *PSP_GET_SET_STATE_OPERATION;

/*++

Structure Description:

    This structure defines the system profiler state to get or set.

Members:

    Operation - Stores the get/set state operation to perform. This is ignored
        on a request to get system information.

    ProfilerTypeFlags - Stores a bitmask of flags indicating which system
        profilers are enabled on a get call or which system profilers to
        enable, disable, or overwite on a set call. See PROFILER_TYPE_FLAG_*
        for definitions.

--*/

typedef struct _SP_GET_SET_STATE_INFORMATION {
    SP_GET_SET_STATE_OPERATION Operation;
    ULONG ProfilerTypeFlags;
} SP_GET_SET_STATE_INFORMATION, *PSP_GET_SET_STATE_INFORMATION;

typedef
VOID
(*PSP_COLLECT_THREAD_STATISTIC) (
    PKTHREAD Thread,
    PPROCESSOR_BLOCK Processor,
    SCHEDULER_REASON ScheduleOutReason
    );

/*++

Routine Description:

    This routine collects statistics on a thread that is being scheduled out.
    This routine must be called at dispatch level inside the scheduler.

Arguments:

    Thread - Supplies a pointer to the thread being scheduled out.

    Process - Supplies a pointer to the executing processor block.

    ScheduleOutReason - Supplies the reason the thread is being scheduled out.

Return Value:

    None.

--*/

typedef
VOID
(*PSP_PROCESS_NEW_PROCESS) (
    PROCESS_ID ProcessId
    );

/*++

Routine Description:

    This routine collects statistics on a created process.

Arguments:

    ProcessId - Supplies the ID of the process being created.

Return Value:

    None.

--*/

typedef
VOID
(*PSP_PROCESS_NEW_THREAD) (
    PROCESS_ID ProcessId,
    THREAD_ID ThreadId
    );

/*++

Routine Description:

    This routine collects statistics on a created thread.

Arguments:

    ProcessId - Supplies the ID of the process creating the new thread.

    ThreadId - Supplies the ID of the new thread being created.

Return Value:

    None.

--*/

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to a function to call to collect thread statistics. This
// function pointer is only set when profiling is active. Callers should not
// access it directly, but use the macro to test and call it.
//

extern PSP_COLLECT_THREAD_STATISTIC SpCollectThreadStatisticRoutine;
extern PSP_PROCESS_NEW_PROCESS SpProcessNewProcessRoutine;
extern PSP_PROCESS_NEW_THREAD SpProcessNewThreadRoutine;

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
SpGetSetSystemInformation (
    BOOL FromKernelMode,
    SP_INFORMATION_TYPE InformationType,
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

VOID
SpProfilerInterrupt (
    PTRAP_FRAME TrapFrame
    );

/*++

Routine Description:

    This routine handles periodic profiler interrupts, collecting information
    about the system for analysis.

Arguments:

    TrapFrame - Supplies a pointer to the current trap frame.

Return Value:

    None.

--*/

VOID
SpSendProfilingData (
    VOID
    );

/*++

Routine Description:

    This routine sends profiling data to any listening consumer. It is called
    periodically on each processor during the clock interrupt.

Arguments:

    None.

Return Value:

    None.

--*/

KSTATUS
SpGetProfilerData (
    PPROFILER_NOTIFICATION ProfilerNotification,
    PULONG Flags
    );

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

ULONG
SpGetProfilerDataStatus (
    VOID
    );

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

KSTATUS
SpInitializeProfiler (
    VOID
    );

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

