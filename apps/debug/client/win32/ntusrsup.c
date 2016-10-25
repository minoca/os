/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ntusrsup.c

Abstract:

    This module implements support functionality for the user mode debugging
    support on Windows. It mostly does all the work, but needs to be refactored
    into a private interface because the windows headers don't get along with
    the OS headers.

Author:

    Evan Green 4-Jun-2013

Environment:

    Win32

--*/

//
// ------------------------------------------------------------------- Includes
//

#define _WIN32_WINNT 0x0501

#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <minoca/debug/dbgext.h>
#include "ntusrsup.h"

//
// ---------------------------------------------------------------- Definitions
//

#define X86_TRAP_FLAG 0x00000100

#define NT_MAX_MODULE_COUNT 1024

#define SYSTEM_TIME_TO_EPOCH_DELTA (978307200LL)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
DbgpNtInitializeDebuggingEvent (
    DWORD ProcessId,
    DWORD ThreadId,
    PNT_DEBUGGER_EVENT Event
    );

VOID
DbgpNtContextToRegisters (
    PCONTEXT Context,
    PNT_X86_REGISTERS Registers
    );

VOID
DbgpNtRegistersToContext (
    PNT_X86_REGISTERS Registers,
    PCONTEXT Context
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the ID and handles to the currently broken in process.
//

DWORD DbgTargetProcessId;
DWORD DbgTargetThreadId;

DWORD DbgTargetPrimaryProcessId;
PSTR DbgPrimaryImageName;
PVOID DbgPrimaryImageBase;
PVOID DbgPrimaryImageLowestAddress;
ULONG DbgPrimaryImageSize;

//
// Store the number of create process events seen so that they can be matched
// to the close process events.
//

ULONG DbgActiveProcesses;

//
// ------------------------------------------------------------------ Functions
//

BOOL
DbgpNtLaunchChildProcess (
    ULONG ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine launches a new child process to be debugged.

Arguments:

    ArgumentCount - Supplies the number of command line arguments for the
        executable.

    Arguments - Supplies the array of arguments to pass.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG ArgumentIndex;
    ULONG BufferSize;
    PSTR CommandLine;
    PROCESS_INFORMATION ProcessInformation;
    BOOL Result;
    STARTUPINFO StartupInfo;

    CommandLine = NULL;

    //
    // Save off the image name.
    //

    BufferSize = strlen(Arguments[0]) + 1;
    if (DbgPrimaryImageName != NULL) {
        free(DbgPrimaryImageName);
    }

    DbgPrimaryImageName = malloc(BufferSize);
    if (DbgPrimaryImageName == NULL) {
        Result = FALSE;
        goto NtLaunchChildProcessEnd;
    }

    strcpy(DbgPrimaryImageName, Arguments[0]);

    //
    // Create one long command line out of the arguments array.
    //

    BufferSize = 0;
    for (ArgumentIndex = 0; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        BufferSize += strlen(Arguments[ArgumentIndex]) + 1;
    }

    CommandLine = malloc(BufferSize);
    if (CommandLine == NULL) {
        DbgOut("Error: Failed to allocated %d bytes for argument buffer.\n",
               BufferSize);
    }

    strcpy(CommandLine, Arguments[0]);
    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        strcat(CommandLine, " ");
        strcat(CommandLine, Arguments[ArgumentIndex]);
    }

    StartupInfo.cb = sizeof(STARTUPINFO);
    GetStartupInfo(&StartupInfo);
    StartupInfo.lpReserved = 0;
    Result = CreateProcess(NULL,
                           CommandLine,
                           NULL,
                           NULL,
                           TRUE,
                           DEBUG_PROCESS,
                           NULL,
                           NULL,
                           &StartupInfo,
                           &ProcessInformation);

    if (Result == FALSE) {
        DbgOut("Error: Failed to create process %s.\n", CommandLine);
        goto NtLaunchChildProcessEnd;
    }

    DbgOut("Created process %x.\n", ProcessInformation.dwProcessId);
    DbgTargetProcessId = ProcessInformation.dwProcessId;
    DbgTargetPrimaryProcessId = DbgTargetProcessId;
    DbgTargetThreadId = ProcessInformation.dwThreadId;
    CloseHandle(ProcessInformation.hProcess);
    CloseHandle(ProcessInformation.hThread);

NtLaunchChildProcessEnd:
    if (CommandLine != NULL) {
        free(CommandLine);
    }

    if (Result == FALSE) {
        if (DbgPrimaryImageName != NULL) {
            free(DbgPrimaryImageName);
            DbgPrimaryImageName = NULL;
        }
    }

    return Result;
}

BOOL
DbgpNtUserContinue (
    ULONG SignalToDeliver
    )

/*++

Routine Description:

    This routine sends the "go" command to the target, signaling to continue
    execution.

Arguments:

    SignalToDeliver - Supplies the signal number to actually send to the
        application. For kernel debugging, this parameter is ignored.

Return Value:

    Returns TRUE if successful, or FALSE if there was an error.

--*/

{

    BOOL Result;

    Result = ContinueDebugEvent(DbgTargetProcessId,
                                DbgTargetThreadId,
                                DBG_CONTINUE);

    if (Result == FALSE) {
        DbgOut("Error: Failed to continue.\n");
    }

    return Result;
}

BOOL
DbgpNtUserSetRegisters (
    PNT_X86_REGISTERS Registers
    )

/*++

Routine Description:

    This routine sets the registers of the debugging target.

Arguments:

    Registers - Supplies a pointer to the registers to set. All register values
        will be written.

Return Value:

    Returns TRUE if successful, or FALSE if there was an error.

--*/

{

    CONTEXT Context;
    BOOL Result;
    HANDLE ThreadHandle;

    ThreadHandle = OpenThread(THREAD_ALL_ACCESS, TRUE, DbgTargetThreadId);
    if (ThreadHandle == NULL) {
        DbgOut("Error: Failed to open thread %x\n", DbgTargetThreadId);
        return FALSE;
    }

    Context.ContextFlags = CONTEXT_FULL;
    Result = GetThreadContext(ThreadHandle, &Context);
    if (Result == FALSE) {
        DbgOut("Error: Failed to get thread context.\n");
        goto NtUserSetRegistersEnd;
    }

    DbgpNtRegistersToContext(Registers, &Context);
    Result = SetThreadContext(ThreadHandle, &Context);
    if (Result == FALSE) {
        DbgOut("Error: Failed to set thread context.\n");
        goto NtUserSetRegistersEnd;
    }

NtUserSetRegistersEnd:
    if (ThreadHandle != NULL) {
        CloseHandle(ThreadHandle);
    }

    return Result;
}

BOOL
DbgpNtUserSingleStep (
    ULONG SignalToDeliver
    )

/*++

Routine Description:

    This routine steps the target by one instruction.

Arguments:

    SignalToDeliver - Supplies the signal number to actually send to the
        application. For kernel debugging, this parameter is ignored.

Return Value:

    Returns TRUE if successful, or FALSE if there was an error.

--*/

{

    CONTEXT Context;
    BOOL Result;
    HANDLE ThreadHandle;

    ThreadHandle = OpenThread(THREAD_ALL_ACCESS, TRUE, DbgTargetThreadId);
    if (ThreadHandle == NULL) {
        DbgOut("Error: Failed to open thread %x\n", DbgTargetThreadId);
        Result = FALSE;
        goto NtUserSingleStepEnd;
    }

    Context.ContextFlags = CONTEXT_FULL;
    Result = GetThreadContext(ThreadHandle, &Context);
    if (Result == FALSE) {
        DbgOut("Error: Failed to get thread context.\n");
        goto NtUserSingleStepEnd;
    }

    Context.EFlags |= X86_TRAP_FLAG;
    Result = SetThreadContext(ThreadHandle, &Context);
    if (Result == FALSE) {
        DbgOut("Error: Failed to set thread context.\n");
        goto NtUserSingleStepEnd;
    }

    Result = DbgpNtUserContinue(SignalToDeliver);

NtUserSingleStepEnd:
    if (ThreadHandle != NULL) {
        CloseHandle(ThreadHandle);
    }

    return Result;
}

BOOL
DbgpNtUserWaitForEvent (
    PNT_DEBUGGER_EVENT Event
    )

/*++

Routine Description:

    This routine gets an event from the target, such as a break event or other
    exception.

Arguments:

    Event - Supplies a pointer where the event details will be returned.

Return Value:

    Returns TRUE on success, or FALSE on failure.

--*/

{

    ULONG ContinueCount;
    DWORD ContinueStatus;
    DEBUG_EVENT DebugEvent;
    PVOID DllBase;
    BOOL FoundOne;
    BOOL Result;

    ContinueCount = 0;
    memset(Event, 0, sizeof(NT_DEBUGGER_EVENT));
    FoundOne = FALSE;
    while (TRUE) {
        ContinueStatus = DBG_EXCEPTION_NOT_HANDLED;
        Result = WaitForDebugEvent(&DebugEvent, INFINITE);
        if (Result == FALSE) {
            DbgOut("Error: Failed to wait for debug event.\n");
            return FALSE;
        }

        switch (DebugEvent.dwDebugEventCode) {
        case CREATE_PROCESS_DEBUG_EVENT:
            if (DbgActiveProcesses == 0) {
                DbgPrimaryImageBase =
                                  DebugEvent.u.CreateProcessInfo.lpBaseOfImage;

                DbgPrimaryImageLowestAddress = (PVOID)0;
                DbgPrimaryImageSize = 0x80000000;

            } else {
                ContinueCount += 1;
                DbgOut("Created additional process %x\n",
                       DebugEvent.dwProcessId);
            }

            CloseHandle(DebugEvent.u.CreateProcessInfo.hFile);
            CloseHandle(DebugEvent.u.CreateProcessInfo.hProcess);
            CloseHandle(DebugEvent.u.CreateProcessInfo.hThread);
            DbgActiveProcesses += 1;
            break;

        case EXCEPTION_DEBUG_EVENT:
            DbgTargetProcessId = DebugEvent.dwProcessId;
            DbgTargetThreadId = DebugEvent.dwThreadId;
            Event->Type = NtDebuggerEventBreak;
            FoundOne = TRUE;
            switch (DebugEvent.u.Exception.ExceptionRecord.ExceptionCode) {
            case EXCEPTION_BREAKPOINT:
                Event->Exception = NtExceptionDebugBreak;
                break;

            case EXCEPTION_SINGLE_STEP:
                Event->Exception = NtExceptionSingleStep;
                break;

            case EXCEPTION_ACCESS_VIOLATION:
                Event->Exception = NtExceptionAccessViolation;
                break;

            case EXCEPTION_INVALID_HANDLE:
                DbgOut("WARNING: Invalid handle exception\n");
                FoundOne = FALSE;
                break;

            default:
                DbgOut("Unknown Exception Code %x\n",
                       DebugEvent.u.Exception.ExceptionRecord.ExceptionCode);

                Event->Exception = NtExceptionAccessViolation;
                break;
            }

            ContinueStatus = DBG_CONTINUE;
            break;

        case EXIT_PROCESS_DEBUG_EVENT:
            DbgActiveProcesses -= 1;
            if (DbgActiveProcesses == 0) {
                DbgTargetProcessId = DebugEvent.dwProcessId;
                DbgTargetThreadId = DebugEvent.dwThreadId;
                Event->Type = NtDebuggerEventShutdown;
                Event->Process = DebugEvent.dwProcessId;
                Event->ExitCode = DebugEvent.u.ExitProcess.dwExitCode;
                FoundOne = TRUE;

            } else {
                DbgOut("Process %x exited with status %d, still %d processes "
                       "alive.\n",
                       DebugEvent.dwProcessId,
                       DebugEvent.u.ExitProcess.dwExitCode,
                       DbgActiveProcesses);
            }

            break;

        case LOAD_DLL_DEBUG_EVENT:

            //
            // Use the DLL base to trim the bounds of the process a bit.
            //

            DllBase = DebugEvent.u.LoadDll.lpBaseOfDll;
            if (DllBase > DbgPrimaryImageBase) {
                if (DbgPrimaryImageLowestAddress + DbgPrimaryImageSize >
                    DllBase) {

                    DbgPrimaryImageSize = (ULONG)DllBase -
                                          (ULONG)DbgPrimaryImageLowestAddress;
                }

            } else {
                if (DbgPrimaryImageLowestAddress < DllBase) {
                    DbgPrimaryImageLowestAddress = DllBase;
                }
            }

            CloseHandle(DebugEvent.u.LoadDll.hFile);
            break;

        case CREATE_THREAD_DEBUG_EVENT:
            CloseHandle(DebugEvent.u.CreateThread.hThread);
            break;

        case EXIT_THREAD_DEBUG_EVENT:
        case OUTPUT_DEBUG_STRING_EVENT:
        case RIP_EVENT:
        case UNLOAD_DLL_DEBUG_EVENT:
            break;

        default:
            DbgOut("Unknown Win32 debug event %d\n",
                   DebugEvent.dwDebugEventCode);

            break;
        }

        if ((FoundOne != FALSE) && (ContinueCount == 0)) {
            break;

        } else {
            Result = ContinueDebugEvent(DebugEvent.dwProcessId,
                                        DebugEvent.dwThreadId,
                                        ContinueStatus);

            if (Result == FALSE) {
                DbgOut("Error: Failed to continue through event.\n");
            }

            if ((FoundOne != FALSE) && (ContinueCount != 0)) {
                ContinueCount -= 1;
            }

            FoundOne = FALSE;
        }
    }

    if (Result != FALSE) {
        Result = DbgpNtInitializeDebuggingEvent(DbgTargetProcessId,
                                                DbgTargetThreadId,
                                                Event);
    }

    return Result;
}

BOOL
DbgpNtUserReadWriteMemory (
    BOOL WriteOperation,
    BOOL VirtualMemory,
    ULONGLONG Address,
    PVOID Buffer,
    ULONG BufferSize,
    PULONG BytesCompleted
    )

/*++

Routine Description:

    This routine retrieves or writes to the target's memory.

Arguments:

    WriteOperation - Supplies a flag indicating whether this is a read
        operation (FALSE) or a write operation (TRUE).

    VirtualMemory - Supplies a flag indicating whether the memory accessed
        should be virtual or physical.

    Address - Supplies the address to read from or write to in the target's
        memory.

    Buffer - Supplies a pointer to the buffer where the memory contents will be
        returned for read operations, or supplies a pointer to the values to
        write to memory on for write operations.

    BufferSize - Supplies the size of the supplied buffer, in bytes.

    BytesCompleted - Supplies a pointer that receive the number of bytes that
        were actually read from or written to the target.

Return Value:

    Returns TRUE if the operation was successful.

    FALSE if there was an error.

--*/

{

    SIZE_T BytesDone;
    HANDLE ProcessHandle;
    BOOL Result;

    if (VirtualMemory == FALSE) {
        DbgOut("Error: Physical memory operations not permitted in user "
               "mode.\n");

        return FALSE;
    }

    ProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, TRUE, DbgTargetProcessId);
    if (ProcessHandle == NULL) {
        DbgOut("Error: Failed to open process %x\n", DbgTargetProcessId);
        return FALSE;
    }

    if (WriteOperation != FALSE) {
        Result = WriteProcessMemory(ProcessHandle,
                                    (PVOID)(ULONG)Address,
                                    Buffer,
                                    BufferSize,
                                    &BytesDone);

    } else {
        Result = ReadProcessMemory(ProcessHandle,
                                   (PVOID)(ULONG)Address,
                                   Buffer,
                                   BufferSize,
                                   &BytesDone);
    }

    CloseHandle(ProcessHandle);
    *BytesCompleted = BytesDone;
    return Result;
}

BOOL
DbgpNtUserGetThreadList (
    PULONG ThreadCount,
    PULONG *ThreadIds
    )

/*++

Routine Description:

    This routine gets the list of active threads in the process (or active
    processors in the machine for kernel mode).

Arguments:

    ThreadCount - Supplies a pointer where the number of threads will be
        returned on success.

    ThreadIds - Supplies a pointer where an array of thread IDs (or processor
        numbers) will be returned on success. It is the caller's responsibility
        to free this memory.

Return Value:

    Returns TRUE if successful, FALSE on failure.

--*/

{

    ULONG Capacity;
    ULONG Count;
    PULONG NewBuffer;
    THREADENTRY32 ThreadEntry;
    PULONG Threads;
    HANDLE ThreadSnap;

    Capacity = 0;
    Count = 0;
    Threads = NULL;
    ThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (ThreadSnap == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    ThreadEntry.dwSize = sizeof(THREADENTRY32);
    if (!Thread32First(ThreadSnap, &ThreadEntry)) {
        CloseHandle(ThreadSnap);
        return FALSE;
    }

    do {
        if (ThreadEntry.th32OwnerProcessID == DbgTargetProcessId) {
            if (Count >= Capacity) {
                if (Capacity == 0) {
                    Capacity = 10;

                } else {
                    Capacity *= 2;
                }

                NewBuffer = realloc(Threads, Capacity * sizeof(ULONG));
                if (NewBuffer == NULL) {
                    CloseHandle(ThreadSnap);
                    if (Threads != NULL) {
                        free(Threads);
                    }

                    return FALSE;
                }

                Threads = NewBuffer;
            }

            Threads[Count] = ThreadEntry.th32ThreadID;
            Count += 1;
        }

    } while (Thread32Next(ThreadSnap, &ThreadEntry));

    CloseHandle(ThreadSnap);
    *ThreadIds = Threads;
    *ThreadCount = Count;
    return TRUE;
}

BOOL
DbgpNtUserSwitchThread (
    ULONG ThreadId,
    PNT_DEBUGGER_EVENT NewBreakInformation
    )

/*++

Routine Description:

    This routine switches the debugger to another thread.

Arguments:

    ThreadId - Supplies the ID of the thread to switch to.

    NewBreakInformation - Supplies a pointer where the updated break information
        will be returned.

Return Value:

    Returns TRUE if successful, or FALSE if there was no change.

--*/

{

    BOOL Result;

    NewBreakInformation->Type = NtDebuggerEventBreak;
    NewBreakInformation->Exception = NtExceptionDebugBreak;
    Result = DbgpNtInitializeDebuggingEvent(DbgTargetProcessId,
                                            ThreadId,
                                            NewBreakInformation);

    return Result;
}

BOOL
DbgpNtUserGetImageDetails (
    PSTR *ImageName,
    PVOID *Base,
    PVOID *LowestAddress,
    PULONGLONG Size
    )

/*++

Routine Description:

    This routine retrieves information about where the primary image of the
    process was loaded.

Arguments:

    ImageName - Supplies a pointer where a string will be returned containing
        the image name. The caller does not own this memory after it's returned,
        and should not modify or free it.

    Base - Supplies a pointer where the image base will be returned.

    LowestAddress - Supplies a pointer where the loaded lowest address of the
        image will be returned.

    Size - Supplies a pointer where the size of the image in bytes will be
        returned.

Return Value:

    Returns TRUE on success, or FALSE on failure.

--*/

{

    *ImageName = DbgPrimaryImageName;
    *Base = DbgPrimaryImageBase;
    *LowestAddress = DbgPrimaryImageLowestAddress;
    *Size = DbgPrimaryImageSize;
    return TRUE;
}

VOID
DbgpNtUserRequestBreakIn (
    VOID
    )

/*++

Routine Description:

    This routine attempts to stop the running target.

Arguments:

    None.

Return Value:

    None.

--*/

{

    HANDLE ProcessHandle;
    BOOL Result;

    ProcessHandle = OpenProcess(PROCESS_ALL_ACCESS,
                                TRUE,
                                DbgTargetPrimaryProcessId);

    if (ProcessHandle == NULL) {
        DbgOut("Error: Failed to open process %x\n", DbgTargetPrimaryProcessId);
        return;
    }

    Result = DebugBreakProcess(ProcessHandle);
    if (Result == FALSE) {
        DbgOut("DebugBreakProcess failed.\n");
    }

    CloseHandle(ProcessHandle);
    return;
}

BOOL
DbgpNtGetLoadedModuleList (
    PMODULE_LIST_HEADER *ModuleList
    )

/*++

Routine Description:

    This routine retrieves the list of loaded binaries from the kernel debugging
    target.

Arguments:

    ModuleList - Supplies a pointer where a pointer to the loaded module header
        and subsequent array of entries will be returned. It is the caller's
        responsibility to free this allocated memory when finished.

Return Value:

    Returns TRUE on success, or FALSE on failure.

--*/

{

    ULONG AllocationSize;
    PSTR BinaryName;
    PLOADED_MODULE_ENTRY Entry;
    PMODULE_LIST_HEADER List;
    PCHAR LocalName;
    ULONG ModuleCount;
    HMODULE *ModuleHandles;
    ULONG ModuleIndex;
    MODULEINFO ModuleInfo;
    ULONG NameSize;
    HANDLE ProcessHandle;
    ULONG RealModuleCount;
    BOOL Result;
    DWORD SizeNeeded;
    struct stat Stat;
    INT Status;

    ProcessHandle = NULL;
    List = NULL;
    LocalName = NULL;
    ModuleHandles = malloc(NT_MAX_MODULE_COUNT * sizeof(HMODULE));
    if (ModuleHandles == NULL) {
        Result = FALSE;
        goto NtGetLoadedModuleListEnd;
    }

    memset(ModuleHandles, 0, NT_MAX_MODULE_COUNT * sizeof(HMODULE));
    LocalName = malloc(MAX_PATH * sizeof(CHAR));
    if (LocalName == NULL) {
        Result = FALSE;
        goto NtGetLoadedModuleListEnd;
    }

    ProcessHandle = OpenProcess(PROCESS_ALL_ACCESS,
                                TRUE,
                                DbgTargetProcessId);

    if (ProcessHandle == NULL) {
        DbgOut("Error: Failed to open process %x\n", DbgTargetProcessId);
        Result = FALSE;
        goto NtGetLoadedModuleListEnd;
    }

    SizeNeeded = 0;
    Result = EnumProcessModules(ProcessHandle,
                                ModuleHandles,
                                sizeof(HMODULE) * NT_MAX_MODULE_COUNT,
                                &SizeNeeded);

    if (Result == FALSE) {
        DbgOut("Error: Failed to enumerate process modules.\n");
        goto NtGetLoadedModuleListEnd;
    }

    ModuleCount = SizeNeeded / sizeof(HMODULE);

    //
    // Loop through once to figure out how big all the names are.
    //

    NameSize = 0;
    RealModuleCount = 0;
    for (ModuleIndex = 0; ModuleIndex < ModuleCount; ModuleIndex += 1) {
        if (ModuleHandles[ModuleIndex] == NULL) {
            continue;
        }

        SizeNeeded = GetModuleFileNameEx(ProcessHandle,
                                         ModuleHandles[ModuleIndex],
                                         LocalName,
                                         MAX_PATH);

        if (SizeNeeded == 0) {
            continue;
        }

        NameSize += SizeNeeded + 1;
        RealModuleCount += 1;
    }

    //
    // Create the list.
    //

    AllocationSize = sizeof(MODULE_LIST_HEADER) +
                     (sizeof(LOADED_MODULE_ENTRY) * RealModuleCount) + NameSize;

    List = malloc(AllocationSize);
    if (List == NULL) {
        Result = FALSE;
        goto NtGetLoadedModuleListEnd;
    }

    memset(List, 0, AllocationSize);
    List->ModuleCount = RealModuleCount;
    List->Signature = 0;
    Entry = (PLOADED_MODULE_ENTRY)(List + 1);
    for (ModuleIndex = 0; ModuleIndex < ModuleCount; ModuleIndex += 1) {
        if (ModuleHandles[ModuleIndex] == NULL) {
            continue;
        }

        SizeNeeded = GetModuleFileNameEx(ProcessHandle,
                                         ModuleHandles[ModuleIndex],
                                         LocalName,
                                         MAX_PATH);

        if (SizeNeeded == 0) {
            continue;
        }

        Result = GetModuleInformation(ProcessHandle,
                                      ModuleHandles[ModuleIndex],
                                      &ModuleInfo,
                                      sizeof(ModuleInfo));

        if (Result == FALSE) {
            DbgOut("Error: Failed to get module information, index %d, "
                   "process %x, Handle %x. GetLastError %x\n",
                   ModuleIndex,
                   ProcessHandle,
                   ModuleHandles[ModuleIndex],
                   GetLastError());

            goto NtGetLoadedModuleListEnd;
        }

        Status = stat(LocalName, &Stat);
        if (Status == 0) {
            Entry->Timestamp = Stat.st_mtime - SYSTEM_TIME_TO_EPOCH_DELTA;
        }

        Entry->StructureSize = FIELD_OFFSET(LOADED_MODULE_ENTRY, BinaryName) +
                               SizeNeeded + 1;

        Entry->LowestAddress = (UINT_PTR)(ModuleInfo.lpBaseOfDll);
        Entry->Size = ModuleInfo.SizeOfImage;
        Entry->Process = DbgTargetProcessId;
        BinaryName = (PSTR)(Entry->BinaryName);
        memcpy(BinaryName, LocalName, SizeNeeded);
        BinaryName[SizeNeeded] = '\0';
        List->Signature += Entry->Timestamp + Entry->LowestAddress;
        Entry = (PLOADED_MODULE_ENTRY)((PUCHAR)Entry + Entry->StructureSize);
    }

    Result = TRUE;

NtGetLoadedModuleListEnd:
    if (ModuleHandles != NULL) {
        free(ModuleHandles);
    }

    if (LocalName != NULL) {
        free(LocalName);
    }

    if (Result == FALSE) {
        if (List != NULL) {
            free(List);
            List = NULL;
        }
    }

    if (ProcessHandle != NULL) {
        CloseHandle(ProcessHandle);
    }

    *ModuleList = List;
    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
DbgpNtInitializeDebuggingEvent (
    DWORD ProcessId,
    DWORD ThreadId,
    PNT_DEBUGGER_EVENT Event
    )

/*++

Routine Description:

    This routine initialized a debugger event with common information.

Arguments:

    ProcessId - Supplies the ID of the process being debugged.

    ThreadId - Supplies the ID of the thread being debugged.

    Event - Supplies a pointer where the initialized event information will be
        returned on success.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    SIZE_T BytesDone;
    CONTEXT Context;
    CHAR LocalName[10];
    ULONG ModuleCount;
    HMODULE *ModuleHandles;
    ULONG ModuleIndex;
    MODULEINFO ModuleInfo;
    HANDLE ProcessHandle;
    BOOL Result;
    DWORD SizeNeeded;
    struct stat Stat;
    INT Status;
    HANDLE ThreadHandle;

    ProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, TRUE, ProcessId);
    if (ProcessHandle == NULL) {
        DbgOut("Error: Failed to open process %x\n", ProcessId);
        return FALSE;
    }

    ThreadHandle = OpenThread(THREAD_ALL_ACCESS, TRUE, ThreadId);
    if (ThreadHandle == NULL) {
        DbgOut("Error: Failed to open thread %x\n", ThreadId);
        Result = FALSE;
        goto NtInitializeDebuggingEvent;
    }

    Context.ContextFlags = CONTEXT_FULL;
    Result = GetThreadContext(ThreadHandle, &Context);
    if (Result == FALSE) {
        goto NtInitializeDebuggingEvent;
    }

    DbgpNtContextToRegisters(&Context, &(Event->Registers));
    Event->InstructionPointer = (PVOID)Event->Registers.Eip;
    Result = ReadProcessMemory(ProcessHandle,
                               (PVOID)Event->Registers.Eip,
                               Event->InstructionStream,
                               sizeof(Event->InstructionStream),
                               &BytesDone);

    if ((Result == FALSE) || (BytesDone != sizeof(Event->InstructionStream))) {
        DbgOut("Warning: Only %d bytes of instruction stream at %x read.\n",
               BytesDone,
               Event->Registers.Eip);

        Result = TRUE;
    }

    Event->Process = ProcessId;
    Event->ThreadCount = 1;
    Event->ThreadNumber = ThreadId;
    Event->LoadedModuleCount = 0;
    Event->LoadedModuleSignature = 0;
    ModuleHandles = malloc(NT_MAX_MODULE_COUNT * sizeof(HMODULE));
    if (ModuleHandles == NULL) {
        Result = FALSE;
        goto NtInitializeDebuggingEvent;
    }

    SizeNeeded = 0;
    Result = EnumProcessModules(ProcessHandle,
                                ModuleHandles,
                                sizeof(HMODULE) * NT_MAX_MODULE_COUNT,
                                &SizeNeeded);

    if (Result == FALSE) {
        DbgOut("Error: Failed to enumerate process modules.\n");
        goto NtInitializeDebuggingEvent;
    }

    ModuleCount = SizeNeeded / sizeof(HMODULE);

    //
    // Loop through once to figure out how big all the names are.
    //

    for (ModuleIndex = 0; ModuleIndex < ModuleCount; ModuleIndex += 1) {
        if (ModuleHandles[ModuleIndex] == NULL) {
            continue;
        }

        SizeNeeded = GetModuleFileNameEx(ProcessHandle,
                                         ModuleHandles[ModuleIndex],
                                         LocalName,
                                         sizeof(LocalName));

        if (SizeNeeded == 0) {
            continue;
        }

        Result = GetModuleInformation(ProcessHandle,
                                      ModuleHandles[ModuleIndex],
                                      &ModuleInfo,
                                      sizeof(ModuleInfo));

        if (Result == FALSE) {
            DbgOut("Error: Failed to get module information, index %d, "
                   "process %x, Handle %x. GetLastError %x\n",
                   ModuleIndex,
                   ProcessHandle,
                   ModuleHandles[ModuleIndex],
                   GetLastError());

            goto NtInitializeDebuggingEvent;
        }

        Event->LoadedModuleCount += 1;
        Status = stat(LocalName, &Stat);
        if (Status == 0) {
            Event->LoadedModuleSignature +=
                                (Stat.st_mtime - SYSTEM_TIME_TO_EPOCH_DELTA) +
                                (ULONG)(ModuleInfo.lpBaseOfDll);
        }
    }

NtInitializeDebuggingEvent:
    if (ProcessHandle != NULL) {
        CloseHandle(ProcessHandle);
    }

    if (ThreadHandle != NULL) {
        CloseHandle(ThreadHandle);
    }

    return Result;
}

VOID
DbgpNtContextToRegisters (
    PCONTEXT Context,
    PNT_X86_REGISTERS Registers
    )

/*++

Routine Description:

    This routine converts a Win32 context structure into the standardized
    structure.

Arguments:

    Context - Supplies a pointer to the Windows context structure.

    Registers - Supplies a pointer where the registers will be returned.

Return Value:

    None.

--*/

{

    Registers->SegGs = Context->SegGs;
    Registers->SegFs = Context->SegFs;
    Registers->SegEs = Context->SegEs;
    Registers->SegDs = Context->SegDs;
    Registers->Edi = Context->Edi;
    Registers->Esi = Context->Esi;
    Registers->Ebx = Context->Ebx;
    Registers->Edx = Context->Edx;
    Registers->Ecx = Context->Ecx;
    Registers->Eax = Context->Eax;
    Registers->Ebp = Context->Ebp;
    Registers->Eip = Context->Eip;
    Registers->SegCs = Context->SegCs;
    Registers->EFlags = Context->EFlags;
    Registers->Esp = Context->Esp;
    Registers->SegSs = Context->SegSs;
    return;
}

VOID
DbgpNtRegistersToContext (
    PNT_X86_REGISTERS Registers,
    PCONTEXT Context
    )

/*++

Routine Description:

    This routine converts a Win32 context structure into the standardized
    structure.

Arguments:

    Registers - Supplies a pointer to the registers to convert.

    Context - Supplies a pointer to the Windows context structure that will be
        set to reflect the given registers.

Return Value:

    None.

--*/

{

    Context->SegGs = Registers->SegGs;
    Context->SegFs = Registers->SegFs;
    Context->SegEs = Registers->SegEs;
    Context->SegDs = Registers->SegDs;
    Context->Edi = Registers->Edi;
    Context->Esi = Registers->Esi;
    Context->Ebx = Registers->Ebx;
    Context->Edx = Registers->Edx;
    Context->Ecx = Registers->Ecx;
    Context->Eax = Registers->Eax;
    Context->Ebp = Registers->Ebp;
    Context->Eip = Registers->Eip;
    Context->SegCs = Registers->SegCs;
    Context->EFlags = Registers->EFlags;
    Context->Esp = Registers->Esp;
    Context->SegSs = Registers->SegSs;
    return;
}

