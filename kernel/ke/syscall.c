/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
    {PsSysRestoreContext, 0, 0},
    {PsSysExitThread, sizeof(SYSTEM_CALL_EXIT_THREAD), 0},
    {IoSysOpen, sizeof(SYSTEM_CALL_OPEN), sizeof(SYSTEM_CALL_OPEN)},
    {IoSysClose, 0, 0},
    {IoSysPerformIo, sizeof(SYSTEM_CALL_PERFORM_IO), 0},
    {IoSysCreatePipe,
        sizeof(SYSTEM_CALL_CREATE_PIPE),
        sizeof(SYSTEM_CALL_CREATE_PIPE)},
    {PsSysCreateThread,
        sizeof(SYSTEM_CALL_CREATE_THREAD),
        sizeof(SYSTEM_CALL_CREATE_THREAD)},
    {PsSysForkProcess, sizeof(SYSTEM_CALL_FORK), 0},
    {PsSysExecuteImage, sizeof(SYSTEM_CALL_EXECUTE_IMAGE), 0},
    {IoSysChangeDirectory, sizeof(SYSTEM_CALL_CHANGE_DIRECTORY), 0},
    {PsSysSetSignalHandler,
        sizeof(SYSTEM_CALL_SET_SIGNAL_HANDLER),
        sizeof(SYSTEM_CALL_SET_SIGNAL_HANDLER)},
    {PsSysSendSignal, sizeof(SYSTEM_CALL_SEND_SIGNAL), 0},
    {PsSysGetSetProcessId, sizeof(SYSTEM_CALL_GET_SET_PROCESS_ID), 0},
    {PsSysSetSignalBehavior,
        sizeof(SYSTEM_CALL_SET_SIGNAL_BEHAVIOR),
        sizeof(SYSTEM_CALL_SET_SIGNAL_BEHAVIOR)},
    {PsSysWaitForChildProcess,
        sizeof(SYSTEM_CALL_WAIT_FOR_CHILD),
        sizeof(SYSTEM_CALL_WAIT_FOR_CHILD)},
    {PsSysSuspendExecution,
        sizeof(SYSTEM_CALL_SUSPEND_EXECUTION),
        sizeof(SYSTEM_CALL_SUSPEND_EXECUTION)},
    {PsSysExitProcess, 0, 0},
    {IoSysPoll, sizeof(SYSTEM_CALL_POLL), 0},
    {IoSysSocketCreate,
        sizeof(SYSTEM_CALL_SOCKET_CREATE),
        sizeof(SYSTEM_CALL_SOCKET_CREATE)},
    {IoSysSocketBind, sizeof(SYSTEM_CALL_SOCKET_BIND), 0},
    {IoSysSocketListen, sizeof(SYSTEM_CALL_SOCKET_LISTEN), 0},
    {IoSysSocketAccept,
        sizeof(SYSTEM_CALL_SOCKET_ACCEPT),
        sizeof(SYSTEM_CALL_SOCKET_ACCEPT)},
    {IoSysSocketConnect, sizeof(SYSTEM_CALL_SOCKET_CONNECT), 0},
    {IoSysSocketPerformIo, sizeof(SYSTEM_CALL_SOCKET_PERFORM_IO), 0},
    {IoSysFileControl, sizeof(SYSTEM_CALL_FILE_CONTROL), 0},
    {IoSysGetSetFileInformation,
        sizeof(SYSTEM_CALL_GET_SET_FILE_INFORMATION),
        sizeof(SYSTEM_CALL_GET_SET_FILE_INFORMATION)},
    {PsSysDebug, sizeof(SYSTEM_CALL_DEBUG), sizeof(SYSTEM_CALL_DEBUG)},
    {IoSysSeek, sizeof(SYSTEM_CALL_SEEK), sizeof(SYSTEM_CALL_SEEK)},
    {IoSysCreateSymbolicLink, sizeof(SYSTEM_CALL_CREATE_SYMBOLIC_LINK), 0},
    {IoSysReadSymbolicLink,
        sizeof(SYSTEM_CALL_READ_SYMBOLIC_LINK),
        sizeof(SYSTEM_CALL_READ_SYMBOLIC_LINK)},
    {IoSysDelete, sizeof(SYSTEM_CALL_DELETE), 0},
    {IoSysRename, sizeof(SYSTEM_CALL_RENAME), 0},
    {IoSysMountOrUnmount, sizeof(SYSTEM_CALL_MOUNT_UNMOUNT), 0},
    {PsSysQueryTimeCounter, 0, sizeof(SYSTEM_CALL_QUERY_TIME_COUNTER)},
    {PsSysTimerControl,
        sizeof(SYSTEM_CALL_TIMER_CONTROL),
        sizeof(SYSTEM_CALL_TIMER_CONTROL)},
    {IoSysGetEffectiveAccess,
        sizeof(SYSTEM_CALL_GET_EFFECTIVE_ACCESS),
        sizeof(SYSTEM_CALL_GET_EFFECTIVE_ACCESS)},
    {KeSysDelayExecution, sizeof(SYSTEM_CALL_DELAY_EXECUTION), 0},
    {IoSysUserControl, sizeof(SYSTEM_CALL_USER_CONTROL), 0},
    {IoSysFlush, sizeof(SYSTEM_CALL_FLUSH), 0},
    {PsSysGetResourceUsage,
        sizeof(SYSTEM_CALL_GET_RESOURCE_USAGE),
        sizeof(SYSTEM_CALL_GET_RESOURCE_USAGE)},
    {IoSysLoadDriver, sizeof(SYSTEM_CALL_LOAD_DRIVER), 0},
    {MmSysFlushCache, sizeof(SYSTEM_CALL_FLUSH_CACHE), 0},
    {IoSysGetCurrentDirectory,
        sizeof(SYSTEM_CALL_GET_CURRENT_DIRECTORY),
        sizeof(SYSTEM_CALL_GET_CURRENT_DIRECTORY)},
    {IoSysSocketGetSetInformation,
        sizeof(SYSTEM_CALL_SOCKET_GET_SET_INFORMATION),
        sizeof(SYSTEM_CALL_SOCKET_GET_SET_INFORMATION)},
    {IoSysSocketShutdown, sizeof(SYSTEM_CALL_SOCKET_SHUTDOWN), 0},
    {IoSysCreateHardLink, sizeof(SYSTEM_CALL_CREATE_HARD_LINK), 0},
    {MmSysMapOrUnmapMemory,
        sizeof(SYSTEM_CALL_MAP_UNMAP_MEMORY),
        sizeof(SYSTEM_CALL_MAP_UNMAP_MEMORY)},
    {MmSysFlushMemory, sizeof(SYSTEM_CALL_FLUSH_MEMORY), 0},
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
    {KeSysResetSystem, 0, 0},
    {KeSysSetSystemTime, sizeof(SYSTEM_CALL_SET_SYSTEM_TIME), 0},
    {MmSysSetMemoryProtection, sizeof(SYSTEM_CALL_SET_MEMORY_PROTECTION), 0},
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
        0},
    {PsSysSetThreadPointer, 0, 0},
    {PsSysUserLock,
        sizeof(SYSTEM_CALL_USER_LOCK),
        sizeof(SYSTEM_CALL_USER_LOCK)},
    {PsSysSetThreadIdPointer, 0, 0},
    {PsSysSetUmask,
        sizeof(SYSTEM_CALL_SET_UMASK),
        sizeof(SYSTEM_CALL_SET_UMASK)},
    {IoSysDuplicateHandle,
        sizeof(SYSTEM_CALL_DUPLICATE_HANDLE),
        sizeof(SYSTEM_CALL_DUPLICATE_HANDLE)},
    {IoSysPerformVectoredIo, sizeof(SYSTEM_CALL_PERFORM_VECTORED_IO), 0},
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
    INTN Result;
    KSTATUS Status;
    PKTHREAD Thread;

    //
    // Begin charging kernel mode for cycles.
    //

    Status = STATUS_SUCCESS;
    KeBeginCycleAccounting(CycleAccountKernel);
    Thread = KeGetCurrentThread();
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

    if ((Handler->CopyInSize != 0) || (Handler->CopyOutSize != 0)) {
        if (Handler->CopyInSize != 0) {

            ASSERT(Handler->CopyInSize <= sizeof(SYSTEM_CALL_PARAMETER_UNION));

            Status = MmCopyFromUserMode(&LocalParameters,
                                        SystemCallParameter,
                                        Handler->CopyInSize);

            if (!KSUCCESS(Status)) {
                PsSignalThread(Thread, SIGNAL_ACCESS_VIOLATION, NULL, TRUE);
                goto SystemCallHandlerEnd;
            }
        }

        //
        // Call the handler.
        //

        Result = Handler->HandlerRoutine(&LocalParameters);

        //
        // Copy the local parameters back into user mode.
        //

        if (Handler->CopyOutSize != 0) {

            ASSERT(Handler->CopyOutSize <= sizeof(SYSTEM_CALL_PARAMETER_UNION));

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
    // Return to the previous thread state and cycle account.
    //

    Thread->Flags &= ~THREAD_FLAG_IN_SYSTEM_CALL;
    KeBeginCycleAccounting(CycleAccountUser);
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

