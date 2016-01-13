/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    syscall.c

Abstract:

    This module implements the interface between user and kernel mode.

Author:

    Evan Green 6-Feb-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores information about a particular system call.

Members:

    HandlerRoutine - Supplies a pointer to the routine that handles this system
        call.

    ParameterSize - Supplies the size, in bytes, of the parameter structure for
        this system call. This should be set to a sizeof(something), rather than
        a hard-coded number.

--*/

typedef struct _SYSTEM_CALL_TABLE_ENTRY {
    PSYSTEM_CALL_ROUTINE HandlerRoutine;
    ULONG ParameterSize;
} SYSTEM_CALL_TABLE_ENTRY, *PSYSTEM_CALL_TABLE_ENTRY;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
KepTestSystemCall (
    ULONG SystemCallNumber,
    PVOID SystemCallParameter,
    PTRAP_FRAME TrapFrame,
    PULONG ResultSize
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the global system call table.
//

SYSTEM_CALL_TABLE_ENTRY KeSystemCallTable[SystemCallCount] = {
    {KepTestSystemCall, 0},
    {PsSysExitThread, sizeof(SYSTEM_CALL_EXIT_THREAD)},
    {IoSysOpen, sizeof(SYSTEM_CALL_OPEN)},
    {IoSysClose, sizeof(SYSTEM_CALL_CLOSE)},
    {IoSysPerformIo, sizeof(SYSTEM_CALL_PERFORM_IO)},
    {IoSysCreatePipe, sizeof(SYSTEM_CALL_CREATE_PIPE)},
    {PsSysCreateThread, sizeof(SYSTEM_CALL_CREATE_THREAD)},
    {PsSysForkProcess, sizeof(SYSTEM_CALL_FORK_PROCESS)},
    {PsSysExecuteImage, sizeof(SYSTEM_CALL_EXECUTE_IMAGE)},
    {IoSysChangeDirectory, sizeof(SYSTEM_CALL_CHANGE_DIRECTORY)},
    {PsSysSetSignalHandler, sizeof(SYSTEM_CALL_SET_SIGNAL_HANDLER)},
    {PsSysResumePreSignalExecution, 0},
    {PsSysSendSignal, sizeof(SYSTEM_CALL_SEND_SIGNAL)},
    {PsSysGetSetProcessId, sizeof(SYSTEM_CALL_GET_SET_PROCESS_ID)},
    {PsSysSetSignalBehavior, sizeof(SYSTEM_CALL_SET_SIGNAL_BEHAVIOR)},
    {PsSysWaitForChildProcess, sizeof(SYSTEM_CALL_WAIT_FOR_CHILD)},
    {PsSysSuspendExecution, sizeof(SYSTEM_CALL_SUSPEND_EXECUTION)},
    {PsSysExitProcess, sizeof(SYSTEM_CALL_EXIT_PROCESS)},
    {IoSysPoll, sizeof(SYSTEM_CALL_POLL)},
    {IoSysSocketCreate, sizeof(SYSTEM_CALL_SOCKET_CREATE)},
    {IoSysSocketBind, sizeof(SYSTEM_CALL_SOCKET_BIND)},
    {IoSysSocketListen, sizeof(SYSTEM_CALL_SOCKET_LISTEN)},
    {IoSysSocketAccept, sizeof(SYSTEM_CALL_SOCKET_ACCEPT)},
    {IoSysSocketConnect, sizeof(SYSTEM_CALL_SOCKET_CONNECT)},
    {IoSysSocketPerformIo, sizeof(SYSTEM_CALL_SOCKET_PERFORM_IO)},
    {IoSysFileControl, sizeof(SYSTEM_CALL_FILE_CONTROL)},
    {IoSysGetSetFileInformation, sizeof(SYSTEM_CALL_GET_SET_FILE_INFORMATION)},
    {PsSysDebug, sizeof(SYSTEM_CALL_DEBUG)},
    {IoSysSeek, sizeof(SYSTEM_CALL_SEEK)},
    {IoSysCreateSymbolicLink, sizeof(SYSTEM_CALL_CREATE_SYMBOLIC_LINK)},
    {IoSysReadSymbolicLink, sizeof(SYSTEM_CALL_READ_SYMBOLIC_LINK)},
    {IoSysDelete, sizeof(SYSTEM_CALL_DELETE)},
    {IoSysRename, sizeof(SYSTEM_CALL_RENAME)},
    {KeSysTimeZoneControl, sizeof(SYSTEM_CALL_TIME_ZONE_CONTROL)},
    {IoSysMountOrUnmount, sizeof(SYSTEM_CALL_MOUNT_UNMOUNT)},
    {PsSysQueryTimeCounter, sizeof(SYSTEM_CALL_QUERY_TIME_COUNTER)},
    {PsSysTimerControl, sizeof(SYSTEM_CALL_TIMER_CONTROL)},
    {IoSysGetEffectiveAccess, sizeof(SYSTEM_CALL_GET_EFFECTIVE_ACCESS)},
    {KeSysDelayExecution, sizeof(SYSTEM_CALL_DELAY_EXECUTION)},
    {IoSysUserControl, sizeof(SYSTEM_CALL_USER_CONTROL)},
    {IoSysFlush, sizeof(SYSTEM_CALL_FLUSH)},
    {PsSysGetResourceUsage, sizeof(SYSTEM_CALL_GET_RESOURCE_USAGE)},
    {IoSysLoadDriver, sizeof(SYSTEM_CALL_LOAD_DRIVER)},
    {MmSysFlushCache, sizeof(SYSTEM_CALL_FLUSH_CACHE)},
    {IoSysGetCurrentDirectory, sizeof(SYSTEM_CALL_GET_CURRENT_DIRECTORY)},
    {IoSysSocketGetSetInformation,
        sizeof(SYSTEM_CALL_SOCKET_GET_SET_INFORMATION)},
    {IoSysSocketShutdown, sizeof(SYSTEM_CALL_SOCKET_SHUTDOWN)},
    {IoSysCreateHardLink, sizeof(SYSTEM_CALL_CREATE_HARD_LINK)},
    {MmSysMapOrUnmapMemory, sizeof(SYSTEM_CALL_MAP_UNMAP_MEMORY)},
    {MmSysFlushMemory, sizeof(SYSTEM_CALL_FLUSH_MEMORY)},
    {IoSysLocateDeviceInformation,
        sizeof(SYSTEM_CALL_LOCATE_DEVICE_INFORMATION)},
    {IoSysGetSetDeviceInformation,
        sizeof(SYSTEM_CALL_GET_SET_DEVICE_INFORMATION)},
    {IoSysOpenDevice, sizeof(SYSTEM_CALL_OPEN_DEVICE)},
    {KeSysGetSetSystemInformation,
        sizeof(SYSTEM_CALL_GET_SET_SYSTEM_INFORMATION)},
    {KeSysResetSystem, sizeof(SYSTEM_CALL_RESET_SYSTEM)},
    {KeSysSetSystemTime, sizeof(SYSTEM_CALL_SET_SYSTEM_TIME)},
    {MmSysSetMemoryProtection, sizeof(SYSTEM_CALL_SET_MEMORY_PROTECTION)},
    {PsSysSetThreadIdentity, sizeof(SYSTEM_CALL_SET_THREAD_IDENTITY)},
    {PsSysSetThreadPermissions, sizeof(SYSTEM_CALL_SET_THREAD_PERMISSIONS)},
    {PsSysSetSupplementaryGroups, sizeof(SYSTEM_CALL_SET_SUPPLEMENTARY_GROUPS)},
    {IoSysSocketCreatePair, sizeof(SYSTEM_CALL_SOCKET_CREATE_PAIR)},
    {IoSysCreateTerminal, sizeof(SYSTEM_CALL_CREATE_TERMINAL)},
    {IoSysSocketPerformVectoredIo,
        sizeof(SYSTEM_CALL_SOCKET_PERFORM_VECTORED_IO)},
    {PsSysSetThreadPointer, sizeof(SYSTEM_CALL_SET_THREAD_POINTER)},
    {PsSysUserLock, sizeof(SYSTEM_CALL_USER_LOCK)},
    {PsSysSetThreadIdPointer, sizeof(SYSTEM_CALL_SET_THREAD_ID_POINTER)},
    {PsSysSetUmask, sizeof(SYSTEM_CALL_SET_UMASK)},
    {IoSysDuplicateHandle, sizeof(SYSTEM_CALL_DUPLICATE_HANDLE)},
    {IoSysPerformVectoredIo, sizeof(SYSTEM_CALL_PERFORM_VECTORED_IO)},
};

//
// ------------------------------------------------------------------ Functions
//

VOID
KeSystemCallHandler (
    ULONG SystemCallNumber,
    PVOID SystemCallParameter,
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine responds to requests from user mode entered via a system call.

Arguments:

    SystemCallNumber - Supplies the system call number.

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call.

    TrapFrame - Supplies a pointer to the trap frame generated by this jump
        from user mode to kernel mode.

Return Value:

    None.

--*/

{

    PSYSTEM_CALL_TABLE_ENTRY Handler;
    SYSTEM_CALL_PARAMETER_UNION LocalParameters;
    ULONG ResultSize;
    KSTATUS Status;
    PKTHREAD Thread;

    //
    // Begin charging kernel mode for cycles.
    //

    KeBeginCycleAccounting(CycleAccountKernel);
    Thread = KeGetCurrentThread();
    Thread->Flags |= THREAD_FLAG_IN_SYSTEM_CALL;

    //
    // Validate the system call number.
    //

    if (SystemCallNumber >= SystemCallCount) {
        Status = STATUS_OUT_OF_BOUNDS;
        goto SystemCallHandlerEnd;
    }

    Handler = &(KeSystemCallTable[SystemCallNumber]);

    //
    // Copy the parameters to the stack local copy, if there are any.
    //

    if (Handler->ParameterSize != 0) {

        ASSERT(Handler->ParameterSize <= sizeof(SYSTEM_CALL_PARAMETER_UNION));

        Status = MmCopyFromUserMode(&LocalParameters,
                                    SystemCallParameter,
                                    Handler->ParameterSize);

        if (!KSUCCESS(Status)) {
            goto SystemCallHandlerEnd;
        }

        //
        // Call the handler.
        //

        ResultSize = Handler->ParameterSize;
        Handler->HandlerRoutine(SystemCallNumber,
                                &LocalParameters,
                                TrapFrame,
                                &ResultSize);

        //
        // Copy the local parameters back into user mode.
        //

        if (ResultSize != 0) {

            ASSERT(ResultSize <= Handler->ParameterSize);

            Status = MmCopyToUserMode(SystemCallParameter,
                                      &LocalParameters,
                                      ResultSize);

            if (!KSUCCESS(Status)) {
                goto SystemCallHandlerEnd;
            }
        }

    } else {
        Handler->HandlerRoutine(SystemCallNumber, NULL, TrapFrame, NULL);
    }

SystemCallHandlerEnd:

    //
    // Set up a single step exception coming out of the system call if needed.
    // This is usually only needed if user mode steps into a sysenter
    // instruction.
    //

    if ((Thread->Flags & THREAD_FLAG_SINGLE_STEP) != 0) {
        Thread->Flags &= ~THREAD_FLAG_SINGLE_STEP;
        ArSetSingleStep(TrapFrame);
    }

    PsDispatchPendingSignals(Thread, TrapFrame);
    Thread->Flags &= ~THREAD_FLAG_IN_SYSTEM_CALL;

    //
    // Return to charging user mode for cycles.
    //

    KeBeginCycleAccounting(CycleAccountUser);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
KepTestSystemCall (
    ULONG SystemCallNumber,
    PVOID SystemCallParameter,
    PTRAP_FRAME TrapFrame,
    PULONG ResultSize
    )

/*++

Routine Description:

    This routine implements a test system call.

Arguments:

    SystemCallNumber - Supplies the system call number that was requested.

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

    TrapFrame - Supplies a pointer to the trap frame generated by this jump
        from user mode to kernel mode.

    ResultSize - Supplies a pointer where the system call routine returns the
        size of the parameter structure to be copied back to user mode. The
        value returned here must be no larger than the original parameter
        structure size. The default is the original size of the parameters.

Return Value:

    None.

--*/

{

    RtlDebugPrint("Test system call %X!\n", SystemCallNumber);
    return;
}

