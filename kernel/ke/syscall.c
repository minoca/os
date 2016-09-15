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

#include <minoca/kernel/kernel.h>

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

    CopyInSize - Supplies the size, in bytes, of the parameter structure for
        this system call that needs to be copied in from user mode to kernel
        mode. This should be set to a sizeof(something), rather than a
        hard-coded number.

    CopyOutSize - Supplies the size, in bytes, of the parameter structure for
        this system call that needs to be copied out to user mode from kernel
        mode after the handler routine is executed. This should be set to
        sizeof(something), rather than a hard-coded number.

--*/

typedef struct _SYSTEM_CALL_TABLE_ENTRY {
    PSYSTEM_CALL_ROUTINE HandlerRoutine;
    ULONG CopyInSize;
    ULONG CopyOutSize;
} SYSTEM_CALL_TABLE_ENTRY, *PSYSTEM_CALL_TABLE_ENTRY;

//
// ----------------------------------------------- Internal Function Prototypes
//

INTN
KepTestSystemCall (
    PVOID SystemCallParameter
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the global system call table.
//

SYSTEM_CALL_TABLE_ENTRY KeSystemCallTable[SystemCallCount] = {
    {KepTestSystemCall, 0, 0},
    {PsSysRestoreContext, sizeof(SYSTEM_CALL_RESTORE_CONTEXT), 0},
    {PsSysExitThread, sizeof(SYSTEM_CALL_EXIT_THREAD), 0},
    {IoSysOpen, sizeof(SYSTEM_CALL_OPEN), sizeof(SYSTEM_CALL_OPEN)},
    {IoSysClose, sizeof(SYSTEM_CALL_CLOSE), sizeof(SYSTEM_CALL_CLOSE)},
    {IoSysPerformIo,
        sizeof(SYSTEM_CALL_PERFORM_IO),
        sizeof(SYSTEM_CALL_PERFORM_IO)},
    {IoSysCreatePipe,
        sizeof(SYSTEM_CALL_CREATE_PIPE),
        sizeof(SYSTEM_CALL_CREATE_PIPE)},
    {PsSysCreateThread,
        sizeof(SYSTEM_CALL_CREATE_THREAD),
        sizeof(SYSTEM_CALL_CREATE_THREAD)},
    {PsSysForkProcess,
        sizeof(SYSTEM_CALL_FORK_PROCESS),
        sizeof(SYSTEM_CALL_FORK_PROCESS)},
    {PsSysExecuteImage, sizeof(SYSTEM_CALL_EXECUTE_IMAGE), 0},
    {IoSysChangeDirectory,
        sizeof(SYSTEM_CALL_CHANGE_DIRECTORY),
        sizeof(SYSTEM_CALL_CHANGE_DIRECTORY)},
    {PsSysSetSignalHandler,
        sizeof(SYSTEM_CALL_SET_SIGNAL_HANDLER),
        sizeof(SYSTEM_CALL_SET_SIGNAL_HANDLER)},
    {PsSysSendSignal,
        sizeof(SYSTEM_CALL_SEND_SIGNAL),
        sizeof(SYSTEM_CALL_SEND_SIGNAL)},
    {PsSysGetSetProcessId,
        sizeof(SYSTEM_CALL_GET_SET_PROCESS_ID),
        sizeof(SYSTEM_CALL_GET_SET_PROCESS_ID)},
    {PsSysSetSignalBehavior,
        sizeof(SYSTEM_CALL_SET_SIGNAL_BEHAVIOR),
        sizeof(SYSTEM_CALL_SET_SIGNAL_BEHAVIOR)},
    {PsSysWaitForChildProcess, 0, 0},
    {PsSysSuspendExecution,
        sizeof(SYSTEM_CALL_SUSPEND_EXECUTION),
        sizeof(SYSTEM_CALL_SUSPEND_EXECUTION)},
    {PsSysExitProcess,
        sizeof(SYSTEM_CALL_EXIT_PROCESS),
        sizeof(SYSTEM_CALL_EXIT_PROCESS)},
    {IoSysPoll, sizeof(SYSTEM_CALL_POLL), sizeof(SYSTEM_CALL_POLL)},
    {IoSysSocketCreate,
        sizeof(SYSTEM_CALL_SOCKET_CREATE),
        sizeof(SYSTEM_CALL_SOCKET_CREATE)},
    {IoSysSocketBind,
        sizeof(SYSTEM_CALL_SOCKET_BIND),
        sizeof(SYSTEM_CALL_SOCKET_BIND)},
    {IoSysSocketListen,
        sizeof(SYSTEM_CALL_SOCKET_LISTEN),
        sizeof(SYSTEM_CALL_SOCKET_LISTEN)},
    {IoSysSocketAccept,
        sizeof(SYSTEM_CALL_SOCKET_ACCEPT),
        sizeof(SYSTEM_CALL_SOCKET_ACCEPT)},
    {IoSysSocketConnect,
        sizeof(SYSTEM_CALL_SOCKET_CONNECT),
        sizeof(SYSTEM_CALL_SOCKET_CONNECT)},
    {IoSysSocketPerformIo,
        sizeof(SYSTEM_CALL_SOCKET_PERFORM_IO),
        sizeof(SYSTEM_CALL_SOCKET_PERFORM_IO)},
    {IoSysFileControl,
        sizeof(SYSTEM_CALL_FILE_CONTROL),
        sizeof(SYSTEM_CALL_FILE_CONTROL)},
    {IoSysGetSetFileInformation,
        sizeof(SYSTEM_CALL_GET_SET_FILE_INFORMATION),
        sizeof(SYSTEM_CALL_GET_SET_FILE_INFORMATION)},
    {PsSysDebug, sizeof(SYSTEM_CALL_DEBUG), sizeof(SYSTEM_CALL_DEBUG)},
    {IoSysSeek, sizeof(SYSTEM_CALL_SEEK), sizeof(SYSTEM_CALL_SEEK)},
    {IoSysCreateSymbolicLink,
        sizeof(SYSTEM_CALL_CREATE_SYMBOLIC_LINK),
        sizeof(SYSTEM_CALL_CREATE_SYMBOLIC_LINK)},
    {IoSysReadSymbolicLink,
        sizeof(SYSTEM_CALL_READ_SYMBOLIC_LINK),
        sizeof(SYSTEM_CALL_READ_SYMBOLIC_LINK)},
    {IoSysDelete, sizeof(SYSTEM_CALL_DELETE), sizeof(SYSTEM_CALL_DELETE)},
    {IoSysRename, sizeof(SYSTEM_CALL_RENAME), sizeof(SYSTEM_CALL_RENAME)},
    {KeSysTimeZoneControl,
        sizeof(SYSTEM_CALL_TIME_ZONE_CONTROL),
        sizeof(SYSTEM_CALL_TIME_ZONE_CONTROL)},
    {IoSysMountOrUnmount,
        sizeof(SYSTEM_CALL_MOUNT_UNMOUNT),
        sizeof(SYSTEM_CALL_MOUNT_UNMOUNT)},
    {PsSysQueryTimeCounter,
        sizeof(SYSTEM_CALL_QUERY_TIME_COUNTER),
        sizeof(SYSTEM_CALL_QUERY_TIME_COUNTER)},
    {PsSysTimerControl,
        sizeof(SYSTEM_CALL_TIMER_CONTROL),
        sizeof(SYSTEM_CALL_TIMER_CONTROL)},
    {IoSysGetEffectiveAccess,
        sizeof(SYSTEM_CALL_GET_EFFECTIVE_ACCESS),
        sizeof(SYSTEM_CALL_GET_EFFECTIVE_ACCESS)},
    {KeSysDelayExecution,
        sizeof(SYSTEM_CALL_DELAY_EXECUTION),
        sizeof(SYSTEM_CALL_DELAY_EXECUTION)},
    {IoSysUserControl,
        sizeof(SYSTEM_CALL_USER_CONTROL),
        sizeof(SYSTEM_CALL_USER_CONTROL)},
    {IoSysFlush, sizeof(SYSTEM_CALL_FLUSH), sizeof(SYSTEM_CALL_FLUSH)},
    {PsSysGetResourceUsage,
        sizeof(SYSTEM_CALL_GET_RESOURCE_USAGE),
        sizeof(SYSTEM_CALL_GET_RESOURCE_USAGE)},
    {IoSysLoadDriver,
        sizeof(SYSTEM_CALL_LOAD_DRIVER),
        sizeof(SYSTEM_CALL_LOAD_DRIVER)},
    {MmSysFlushCache,
        sizeof(SYSTEM_CALL_FLUSH_CACHE),
        sizeof(SYSTEM_CALL_FLUSH_CACHE)},
    {IoSysGetCurrentDirectory,
        sizeof(SYSTEM_CALL_GET_CURRENT_DIRECTORY),
        sizeof(SYSTEM_CALL_GET_CURRENT_DIRECTORY)},
    {IoSysSocketGetSetInformation,
        sizeof(SYSTEM_CALL_SOCKET_GET_SET_INFORMATION),
        sizeof(SYSTEM_CALL_SOCKET_GET_SET_INFORMATION)},
    {IoSysSocketShutdown,
        sizeof(SYSTEM_CALL_SOCKET_SHUTDOWN),
        sizeof(SYSTEM_CALL_SOCKET_SHUTDOWN)},
    {IoSysCreateHardLink,
        sizeof(SYSTEM_CALL_CREATE_HARD_LINK),
        sizeof(SYSTEM_CALL_CREATE_HARD_LINK)},
    {MmSysMapOrUnmapMemory,
        sizeof(SYSTEM_CALL_MAP_UNMAP_MEMORY),
        sizeof(SYSTEM_CALL_MAP_UNMAP_MEMORY)},
    {MmSysFlushMemory,
        sizeof(SYSTEM_CALL_FLUSH_MEMORY),
        sizeof(SYSTEM_CALL_FLUSH_MEMORY)},
    {IoSysLocateDeviceInformation,
        sizeof(SYSTEM_CALL_LOCATE_DEVICE_INFORMATION),
        sizeof(SYSTEM_CALL_LOCATE_DEVICE_INFORMATION)},
    {IoSysGetSetDeviceInformation,
        sizeof(SYSTEM_CALL_GET_SET_DEVICE_INFORMATION),
        sizeof(SYSTEM_CALL_GET_SET_DEVICE_INFORMATION)},
    {IoSysOpenDevice,
        sizeof(SYSTEM_CALL_OPEN_DEVICE),
        sizeof(SYSTEM_CALL_OPEN_DEVICE)},
    {KeSysGetSetSystemInformation,
        sizeof(SYSTEM_CALL_GET_SET_SYSTEM_INFORMATION),
        sizeof(SYSTEM_CALL_GET_SET_SYSTEM_INFORMATION)},
    {KeSysResetSystem,
        sizeof(SYSTEM_CALL_RESET_SYSTEM),
        sizeof(SYSTEM_CALL_RESET_SYSTEM)},
    {KeSysSetSystemTime,
        sizeof(SYSTEM_CALL_SET_SYSTEM_TIME),
        sizeof(SYSTEM_CALL_SET_SYSTEM_TIME)},
    {MmSysSetMemoryProtection,
        sizeof(SYSTEM_CALL_SET_MEMORY_PROTECTION),
        sizeof(SYSTEM_CALL_SET_MEMORY_PROTECTION)},
    {PsSysSetThreadIdentity,
        sizeof(SYSTEM_CALL_SET_THREAD_IDENTITY),
        sizeof(SYSTEM_CALL_SET_THREAD_IDENTITY)},
    {PsSysSetThreadPermissions,
        sizeof(SYSTEM_CALL_SET_THREAD_PERMISSIONS),
        sizeof(SYSTEM_CALL_SET_THREAD_PERMISSIONS)},
    {PsSysSetSupplementaryGroups,
        sizeof(SYSTEM_CALL_SET_SUPPLEMENTARY_GROUPS),
        sizeof(SYSTEM_CALL_SET_SUPPLEMENTARY_GROUPS)},
    {IoSysSocketCreatePair,
        sizeof(SYSTEM_CALL_SOCKET_CREATE_PAIR),
        sizeof(SYSTEM_CALL_SOCKET_CREATE_PAIR)},
    {IoSysCreateTerminal,
        sizeof(SYSTEM_CALL_CREATE_TERMINAL),
        sizeof(SYSTEM_CALL_CREATE_TERMINAL)},
    {IoSysSocketPerformVectoredIo,
        sizeof(SYSTEM_CALL_SOCKET_PERFORM_VECTORED_IO),
        sizeof(SYSTEM_CALL_SOCKET_PERFORM_VECTORED_IO)},
    {PsSysSetThreadPointer,
        sizeof(SYSTEM_CALL_SET_THREAD_POINTER),
        sizeof(SYSTEM_CALL_SET_THREAD_POINTER)},
    {PsSysUserLock,
        sizeof(SYSTEM_CALL_USER_LOCK),
        sizeof(SYSTEM_CALL_USER_LOCK)},
    {PsSysSetThreadIdPointer,
        sizeof(SYSTEM_CALL_SET_THREAD_ID_POINTER),
        sizeof(SYSTEM_CALL_SET_THREAD_ID_POINTER)},
    {PsSysSetUmask,
        sizeof(SYSTEM_CALL_SET_UMASK),
        sizeof(SYSTEM_CALL_SET_UMASK)},
    {IoSysDuplicateHandle,
        sizeof(SYSTEM_CALL_DUPLICATE_HANDLE),
        sizeof(SYSTEM_CALL_DUPLICATE_HANDLE)},
    {IoSysPerformVectoredIo,
        sizeof(SYSTEM_CALL_PERFORM_VECTORED_IO),
        sizeof(SYSTEM_CALL_PERFORM_VECTORED_IO)},
    {PsSysSetITimer,
        sizeof(SYSTEM_CALL_SET_ITIMER),
        sizeof(SYSTEM_CALL_SET_ITIMER)},
    {PsSysSetResourceLimit,
        sizeof(SYSTEM_CALL_SET_RESOURCE_LIMIT),
        sizeof(SYSTEM_CALL_SET_RESOURCE_LIMIT)},
    {MmSysSetBreak,
        sizeof(SYSTEM_CALL_SET_BREAK),
        sizeof(SYSTEM_CALL_SET_BREAK)},
};

//
// ------------------------------------------------------------------ Functions
//

INTN
KeSystemCallHandler (
    ULONG SystemCallNumber,
    PVOID SystemCallParameter,
    PTRAP_FRAME TrapFrame
    )

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

{

    PSYSTEM_CALL_TABLE_ENTRY Handler;
    SYSTEM_CALL_PARAMETER_UNION LocalParameters;
    USHORT OriginalThreadFlags;
    CYCLE_ACCOUNT PreviousCycleAccount;
    INTN Result;
    KSTATUS Status;
    PKTHREAD Thread;

    //
    // Begin charging kernel mode for cycles.
    //

    Status = STATUS_SUCCESS;
    PreviousCycleAccount = KeBeginCycleAccounting(CycleAccountKernel);
    Thread = KeGetCurrentThread();
    OriginalThreadFlags = Thread->Flags;
    Thread->Flags |= THREAD_FLAG_IN_SYSTEM_CALL;
    Thread->TrapFrame = TrapFrame;

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

    if (Handler->CopyInSize != 0) {

        ASSERT(Handler->CopyInSize <= sizeof(SYSTEM_CALL_PARAMETER_UNION));

        Status = MmCopyFromUserMode(&LocalParameters,
                                    SystemCallParameter,
                                    Handler->CopyInSize);

        if (!KSUCCESS(Status)) {
            PsSignalThread(Thread, SIGNAL_ACCESS_VIOLATION, NULL, TRUE);
            goto SystemCallHandlerEnd;
        }

        //
        // Call the handler.
        //

        Result = Handler->HandlerRoutine(&LocalParameters);

        //
        // Copy the local parameters back into user mode.
        //

        if (Handler->CopyOutSize != 0) {

            ASSERT(Handler->CopyOutSize <= Handler->CopyInSize);

            Status = MmCopyToUserMode(SystemCallParameter,
                                      &LocalParameters,
                                      Handler->CopyOutSize);

            if (!KSUCCESS(Status)) {
                PsSignalThread(Thread, SIGNAL_ACCESS_VIOLATION, NULL, TRUE);
                goto SystemCallHandlerEnd;
            }
        }

    //
    // Even if there is no data to copy in, still pass the system call
    // parameters along. The handler may be doing something special with them.
    //

    } else {
        Result = Handler->HandlerRoutine(SystemCallParameter);
    }

SystemCallHandlerEnd:
    if (!KSUCCESS(Status)) {
        Result = Status;
    }

    PsCheckRuntimeTimers(Thread);

    //
    // Return to the previous thread state and cycle account. A restarted
    // system call can come in on top of the context restore system call after
    // a signal is handled.
    //

    if ((OriginalThreadFlags & THREAD_FLAG_IN_SYSTEM_CALL) == 0) {
        Thread->Flags &= ~THREAD_FLAG_IN_SYSTEM_CALL;
    }

    KeBeginCycleAccounting(PreviousCycleAccount);
    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

INTN
KepTestSystemCall (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine implements a test system call.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    RtlDebugPrint("Test system call!\n");
    return STATUS_SUCCESS;
}

