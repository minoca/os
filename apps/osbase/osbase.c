/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    osbase.c

Abstract:

    This module implements the Operating System Base interface.

Author:

    Evan Green 25-Feb-2013

Environment:

    User mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "osbasep.h"

//
// ---------------------------------------------------------------- Definitions
//

#define OS_MOUNT_ALLOCATION_TAG 0x744D734F // 'tMsO'
#define OS_VERSION_ALLOCATION_TAG 0x73726556 // 'sreV'
#define OS_CURRENT_DIRECTORY_ALLOCATION_TAG 0x6443734F // 'dCsO'

//
// Defines the number of times the get mount points routine should try to
// collect mount points.
//

#define OS_GET_MOUNT_POINTS_TRY_COUNT 5

//
// Defines the initial size of the mount points buffer.
//

#define OS_GET_MOUNT_POINTS_BUFFER_SIZE_GUESS 4096

//
// Defines the number of times the get current directory routine should try
// to collect the current directory string.
//

#define OS_GET_CURRENT_DIRECTORY_TRY_COUNT 5

//
// Defines the initial size of the current directory buffer.
//

#define OS_GET_CURRENT_DIRECTORY_BUFFER_SIZE_GUESS 256

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PVOID
OspSetSignalHandler (
    PVOID SignalHandlerRoutine
    );

KSTATUS
OspGetSetFileInformation (
    HANDLE Directory,
    PSTR Path,
    ULONG PathSize,
    BOOL FollowLink,
    PSET_FILE_INFORMATION Request
    );

KSTATUS
OspMountOrUnmount (
    PSTR MountPointPath,
    ULONG MountPointPathSize,
    PSTR TargetPath,
    ULONG TargetPathSize,
    ULONG Flags
    );

NO_RETURN
VOID
OspExitThread (
    PVOID UnmapAddress,
    UINTN UnmapSize
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a boolean indicating whether this library is initialized or not to be
// moderately defensive against people accidentally calling this routine
// multiple times.
//

BOOL OsLibraryInitialized = FALSE;

//
// Store a pointer to the signal handling routine.
//

volatile PSIGNAL_HANDLER_ROUTINE OsSignalHandler = NULL;

//
// Store pointers to the system version information if it has been requested
// already.
//

PSTR OsProductName;
PSTR OsBuildString;
ULONG OsSystemVersionStringsSize;

//
// ------------------------------------------------------------------ Functions
//

OS_API
VOID
OsInitializeLibrary (
    PPROCESS_ENVIRONMENT Environment
    )

/*++

Routine Description:

    This routine initializes the base OS library. It needs to be called only
    once, when the library is first loaded.

Arguments:

    Environment - Supplies a pointer to the environment information.

Return Value:

    None.

--*/

{

    if (OsLibraryInitialized != FALSE) {
        return;
    }

    OsEnvironment = Environment;
    OsLibraryInitialized = TRUE;
    OspSetUpSystemCalls();
    OspInitializeMemory();
    OspInitializeImageSupport();
    OspInitializeThreadSupport();

    //
    // Register the signal handler to start receiving signals.
    //

    OspSetSignalHandler(OspSignalHandler);
    return;
}

OS_API
VOID
OsTestSystemCall (
    VOID
    )

/*++

Routine Description:

    This routine performs a meaningless system call.

Arguments:

    None.

Return Value:

    None.

--*/

{

    OsSystemCall(SystemCallInvalid, NULL);
    return;
}

OS_API
KSTATUS
OsOpen (
    HANDLE Directory,
    PCSTR Path,
    ULONG PathLength,
    ULONG Flags,
    FILE_PERMISSIONS CreatePermissions,
    PHANDLE Handle
    )

/*++

Routine Description:

    This routine opens a file or other I/O object.

Arguments:

    Directory - Supplies an optional handle to a directory to start the
        search from if the supplied path is relative. Supply INVALID_HANDLE
        here to to use the current directory for relative paths.

    Path - Supplies a pointer to a string containing the path of the object
        to open.

    PathLength - Supplies the length of the path buffer, in bytes, including
        the null terminator.

    Flags - Supplies flags associated with the open operation. See
        SYS_OPEN_FLAG_* definitions.

    CreatePermissions - Supplies the permissions for create operations.

    Handle - Supplies a pointer where a handle will be returned on success.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_OPEN Parameters;
    KSTATUS Status;

    Parameters.Directory = Directory;
    Parameters.Path = Path;
    Parameters.PathBufferLength = PathLength;
    Parameters.Flags = Flags;
    Parameters.CreatePermissions = CreatePermissions & FILE_PERMISSION_MASK;
    Status = OsSystemCall(SystemCallOpen, &Parameters);
    *Handle = Parameters.Handle;
    return Status;
}

OS_API
KSTATUS
OsOpenDevice (
    DEVICE_ID DeviceId,
    ULONG Flags,
    PHANDLE Handle
    )

/*++

Routine Description:

    This routine opens a device directly.

Arguments:

    DeviceId - Supplies the identifier of the device to open.

    Flags - Supplies flags associated with the open operation. See
        SYS_OPEN_FLAG_* definitions.

    Handle - Supplies a pointer where a handle will be returned on success.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_OPEN_DEVICE Parameters;
    KSTATUS Status;

    Parameters.DeviceId = DeviceId;
    Parameters.Flags = Flags;
    Status = OsSystemCall(SystemCallOpenDevice, &Parameters);
    *Handle = Parameters.Handle;
    return Status;
}

OS_API
KSTATUS
OsClose (
    HANDLE Handle
    )

/*++

Routine Description:

    This routine closes an I/O handle.

Arguments:

    Handle - Supplies a pointer to the open handle.

Return Value:

    Status code.

--*/

{

    return OsSystemCall(SystemCallClose, Handle);
}

OS_API
KSTATUS
OsPerformIo (
    HANDLE Handle,
    IO_OFFSET Offset,
    UINTN Size,
    ULONG Flags,
    ULONG TimeoutInMilliseconds,
    PVOID Buffer,
    PUINTN BytesCompleted
    )

/*++

Routine Description:

    This routine performs I/O on an open handle.

Arguments:

    Handle - Supplies a pointer to the opened I/O handle.

    Offset - Supplies the offset into the file to read from or write to. Set
        this to IO_OFFSET_NONE to do I/O at the current file position or for
        handles that are not seekable.

    Size - Supplies the number of bytes to transfer.

    Flags - Supplies a bitfield of flags. See SYS_IO_FLAG_* definitions.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        SYS_WAIT_TIME_INDEFINITE to wait forever on the I/O.

    Buffer - Supplies a pointer to the buffer containing the data to write or
        where the read data should be returned, depending on the operation.

    BytesCompleted - Supplies a pointer where the number of bytes completed
        will be returned.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_PERFORM_IO Parameters;
    INTN Result;

    //
    // Truncate the size so that the bytes completed can be returned via a
    // register. Callers of perform I/O should be aware enough that bytes
    // completed may not be the requested size and that large I/O needs to
    // happen in a loop.
    //

    if (Size > (UINTN)MAX_INTN) {
        Size = (UINTN)MAX_INTN;
    }

    Parameters.Handle = Handle;
    Parameters.Buffer = Buffer;
    Parameters.Flags = Flags;
    Parameters.TimeoutInMilliseconds = TimeoutInMilliseconds;
    Parameters.Offset = Offset;
    Parameters.Size = (INTN)Size;
    Result = OsSystemCall(SystemCallPerformIo, &Parameters);
    if (Result < 0) {
        *BytesCompleted = 0;
        return Result;
    }

    *BytesCompleted = (UINTN)Result;
    return STATUS_SUCCESS;
}

OS_API
KSTATUS
OsPerformVectoredIo (
    HANDLE Handle,
    IO_OFFSET Offset,
    UINTN Size,
    ULONG Flags,
    ULONG TimeoutInMilliseconds,
    PIO_VECTOR VectorArray,
    UINTN VectorCount,
    PUINTN BytesCompleted
    )

/*++

Routine Description:

    This routine performs I/O on an open handle.

Arguments:

    Handle - Supplies a pointer to the opened I/O handle.

    Offset - Supplies the offset into the file to read from or write to. Set
        this to IO_OFFSET_NONE to do I/O at the current file position or for
        handles that are not seekable.

    Size - Supplies the number of bytes to transfer.

    Flags - Supplies a bitfield of flags. See SYS_IO_FLAG_* definitions.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        SYS_WAIT_TIME_INDEFINITE to wait forever on the I/O.

    VectorArray - Supplies an array of I/O vector structures to do I/O to/from.

    VectorCount - Supplies the number of elements in the vector array.

    BytesCompleted - Supplies a pointer where the number of bytes completed
        will be returned.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_PERFORM_VECTORED_IO Parameters;
    INTN Result;

    //
    // Truncate the size so that the bytes completed can be returned via a
    // register. Callers of perform I/O should be aware enough that bytes
    // completed may not be the requested size and that large I/O needs to
    // happen in a loop.
    //

    if (Size > (UINTN)MAX_INTN) {
        Size = (UINTN)MAX_INTN;
    }

    Parameters.Handle = Handle;
    Parameters.Flags = Flags;
    Parameters.TimeoutInMilliseconds = TimeoutInMilliseconds;
    Parameters.Offset = Offset;
    Parameters.Size = (INTN)Size;
    Parameters.VectorArray = VectorArray;
    Parameters.VectorCount = VectorCount;
    Result = OsSystemCall(SystemCallPerformVectoredIo, &Parameters);
    if (Result < 0) {
        *BytesCompleted = 0;
        return Result;
    }

    *BytesCompleted = (UINTN)Result;
    return STATUS_SUCCESS;
}

OS_API
KSTATUS
OsFlush (
    HANDLE Handle,
    ULONG Flags
    )

/*++

Routine Description:

    This routine flushes handle data to its backing device. If the flags
    specify that all data is to be flushed, then a handle is not required.

Arguments:

    Handle - Supplies an open I/O handle. This parameter is not required if
        SYS_FLUSH_FLAG_ALL is set.

    Flags - Supplies a bitfield of flags. See SYS_FLUSH_FLAG_* definitions.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_FLUSH Parameters;

    Parameters.Handle = Handle;
    Parameters.Flags = Flags;
    return OsSystemCall(SystemCallFlush, &Parameters);
}

OS_API
KSTATUS
OsCreatePipe (
    HANDLE Directory,
    PSTR Path,
    ULONG PathLength,
    ULONG Flags,
    FILE_PERMISSIONS Permissions,
    PHANDLE ReadHandle,
    PHANDLE WriteHandle
    )

/*++

Routine Description:

    This routine creates a pipe.

Arguments:

    Directory - Supplies an optional handle to a directory to start the
        search from if the supplied path is relative. Supply INVALID_HANDLE
        here to to use the current directory for relative paths.

    Path - Supplies an optional pointer to the path to create the pipe at.

    PathLength - Supplies the length of the path buffer in bytes, including
        the null terminator character.

    Flags - Supplies a bitfield of flags governing the behavior of the new pipe
        descriptors. Only SYS_OPEN_FLAG_CLOSE_ON_EXECUTE and
        SYS_OPEN_FLAG_NON_BLOCKING are permitted.

    Permissions - Supplies the initial permissions to set on the pipe.

    ReadHandle - Supplies a pointer where the handle to the read end of the
        pipe will be returned on success. Handles are only returned if a
        NULL path was passed in.

    WriteHandle - Supplies a pointer where the handle to the write end of the
        pipe will be returned on success. Handles are only returned in a NULL
        path was passed in.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_CREATE_PIPE Parameters;
    KSTATUS Status;

    Parameters.Directory = Directory;
    Parameters.Path = Path;
    Parameters.PathLength = PathLength;
    Parameters.OpenFlags = Flags;
    Parameters.Permissions = Permissions;
    Status = OsSystemCall(SystemCallCreatePipe, &Parameters);
    if (Path == NULL) {
        *ReadHandle = Parameters.ReadHandle;
        *WriteHandle = Parameters.WriteHandle;
    }

    return Status;
}

OS_API
VOID
OsExitThread (
    PVOID UnmapAddress,
    UINTN UnmapSize
    )

/*++

Routine Description:

    This routine terminates the current thread, and optionally attempts to
    unmap a region of memory on its way out. Usually this is the stack of the
    thread that is exiting.

Arguments:

    UnmapAddress - Supplies an optional pointer to a region of memory to unmap
        as the thread exits. Supply NULL to skip unmapping.

    UnmapSize - Supplies the size of the region to unmap in bytes. This must be
        aligned to the page size. If it is not, the unmap simply won't happen.
        Supply 0 to skip the unmap and just exit the thread. If -1 is supplied,
        this routine returns. This value can be used to warm up the PLT entry,
        since lazy binding cannot take place after the thread's control block
        has been destroyed.

Return Value:

    This routine does not return, unless the magic size is passed in.

--*/

{

    if (UnmapSize == -1UL) {
        return;
    }

    OspExitThread(UnmapAddress, UnmapSize);
}

OS_API
KSTATUS
OsCreateThread (
    PSTR ThreadName,
    ULONG ThreadNameBufferLength,
    PTHREAD_ENTRY_ROUTINE ThreadRoutine,
    PVOID Parameter,
    PVOID StackBase,
    ULONG StackSize,
    PVOID ThreadPointer,
    PTHREAD_ID ThreadId
    )

/*++

Routine Description:

    This routine creates a new thread.

Arguments:

    ThreadName - Supplies an optional pointer to the thread name.

    ThreadNameBufferLength - Supplies the size of the thread name buffer,
        including the null terminator.

    ThreadRoutine - Supplies a pointer to the funtion that should be run on the
        new thread.

    Parameter - Supplies a pointer that will be passed directly to the thread
        routine.

    StackBase - Supplies an optional pointer to the stack base address. If
        supplied, the kernel will not add a guard region or automatically
        expand the stack.

    StackSize - Supplies the size of the new thread's stack. Supply 0 to use
        the system default.

    ThreadPointer - Supplies the thread pointer to set for the new thread.

    ThreadId - Supplies an optional pointer where the ID of the new thread will
        be returned on success.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_CREATE_THREAD Parameters;

    Parameters.Name = ThreadName;
    Parameters.NameBufferLength = ThreadNameBufferLength;
    Parameters.ThreadRoutine = ThreadRoutine;
    Parameters.Parameter = Parameter;
    Parameters.StackBase = StackBase;
    Parameters.StackSize = StackSize;
    Parameters.ThreadPointer = ThreadPointer;
    Parameters.ThreadId = ThreadId;
    return OsSystemCall(SystemCallCreateThread, &Parameters);
}

OS_API
KSTATUS
OsForkProcess (
    ULONG Flags,
    PPROCESS_ID NewProcessId
    )

/*++

Routine Description:

    This routine forks the current process into two separate processes. The
    child process begins executing in the middle of this function.

Arguments:

    Flags - Supplies a bitfield of flags governing the behavior of the newly
        forked process. See FORK_FLAG_* definitions.

    NewProcessId - Supplies a pointer that on success contains the process ID
        of the child process in the parent, and 0 in the child. This value
        contains -1 if the new process failed to spawn.

Return Value:

    STATUS_SUCCESS in both the parent and child on success.

    Other status codes are returned to the parent if the child failed to spawn.

--*/

{

    SYSTEM_CALL_FORK Parameters;
    INTN Result;

    //
    // Fork returns the process ID of the child to the parent and 0 to the
    // child. Or a negative status code to the parent if the fork failed.
    //

    Parameters.Flags = Flags;
    Result = OspSystemCallFull(SystemCallForkProcess, &Parameters);
    if (Result < 0) {
        *NewProcessId = -1;
        return (KSTATUS)Result;
    }

    *NewProcessId = Result;
    return STATUS_SUCCESS;
}

OS_API
KSTATUS
OsExecuteImage (
    PPROCESS_ENVIRONMENT Environment
    )

/*++

Routine Description:

    This routine replaces the currently running process with the given binary
    image.

Arguments:

    Environment - Supplies a pointer to the environment to execute, which
        includes the image name, parameters, and environment variables.

Return Value:

    If this routine succeeds, it will not return, as the process will be
    replaced with the new executable. If the process could not be started,
    a failing status code will be returned to the caller.

--*/

{

    PSYSTEM_CALL_EXECUTE_IMAGE Parameters;

    //
    // Avoid copying the process environment to the system call structure, only
    // for it to be copied again by the kernel. Just cast the environment into
    // the system call parameters. It's a bit sneaky, but saves a double copy.
    //

    ASSERT(FIELD_OFFSET(SYSTEM_CALL_EXECUTE_IMAGE, Environment) == 0);

    Parameters = (PSYSTEM_CALL_EXECUTE_IMAGE)Environment;
    return OspSystemCallFull(SystemCallExecuteImage, Parameters);
}

OS_API
KSTATUS
OsGetSystemVersion (
    PSYSTEM_VERSION_INFORMATION VersionInformation,
    BOOL WantStrings
    )

/*++

Routine Description:

    This routine gets the system version information.

Arguments:

    VersionInformation - Supplies a pointer where the system version
        information will be returned. The caller should not attempt to modify
        or free the strings pointed to by members of this structure.

    WantStrings - Supplies a boolean indicating if the build strings are
        desired or just the major/minor version information.

Return Value:

    Status code.

--*/

{

    UINTN BufferSize;
    ULONG EncodedVersion;
    PVOID HeapAllocation;
    PSYSTEM_VERSION_INFORMATION LocalInformation;
    KSTATUS Status;
    PUSER_SHARED_DATA UserSharedData;

    Status = STATUS_SUCCESS;

    //
    // Get the build strings from the kernel if needed.
    //

    if ((WantStrings != FALSE) && (OsProductName == NULL)) {
        BufferSize = 0;
        Status = OsGetSetSystemInformation(SystemInformationKe,
                                           KeInformationSystemVersion,
                                           NULL,
                                           &BufferSize,
                                           FALSE);

        ASSERT(Status == STATUS_BUFFER_TOO_SMALL);

        OsSystemVersionStringsSize = BufferSize;
        HeapAllocation = OsHeapAllocate(BufferSize, OS_VERSION_ALLOCATION_TAG);
        if (HeapAllocation == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto GetSystemVersionEnd;
        }

        Status = OsGetSetSystemInformation(SystemInformationKe,
                                           KeInformationSystemVersion,
                                           HeapAllocation,
                                           &BufferSize,
                                           FALSE);

        if (!KSUCCESS(Status)) {
            goto GetSystemVersionEnd;
        }

        LocalInformation = HeapAllocation;
        if (LocalInformation->BuildString != NULL) {
            LocalInformation->BuildString =
                (PVOID)((UINTN)LocalInformation +
                        (UINTN)(LocalInformation->BuildString));
        }

        if (LocalInformation->ProductName != NULL) {
            LocalInformation->ProductName =
                (PVOID)((UINTN)LocalInformation +
                        (UINTN)(LocalInformation->ProductName));
        }

        OsBuildString = LocalInformation->BuildString;
        OsProductName = LocalInformation->ProductName;
    }

    //
    // Fill in the caller's structure.
    //

    UserSharedData = OspGetUserSharedData();
    EncodedVersion = UserSharedData->EncodedSystemVersion;
    VersionInformation->MajorVersion = DECODE_MAJOR_VERSION(EncodedVersion);
    VersionInformation->MinorVersion = DECODE_MINOR_VERSION(EncodedVersion);
    VersionInformation->Revision = DECODE_VERSION_REVISION(EncodedVersion);
    VersionInformation->SerialVersion = UserSharedData->SystemVersionSerial;
    VersionInformation->ReleaseLevel = DECODE_VERSION_RELEASE(EncodedVersion);
    VersionInformation->DebugLevel = DECODE_VERSION_DEBUG(EncodedVersion);
    VersionInformation->BuildTime.Seconds = UserSharedData->BuildTime;
    VersionInformation->BuildTime.Nanoseconds = 0;
    VersionInformation->ProductName = NULL;
    VersionInformation->BuildString = NULL;

    //
    // Copy the strings as well if requested.
    //

    if (WantStrings != FALSE) {
        VersionInformation->ProductName = OsProductName;
        VersionInformation->BuildString = OsBuildString;
    }

    Status = STATUS_SUCCESS;

GetSystemVersionEnd:
    return Status;
}

OS_API
KSTATUS
OsGetCurrentDirectory (
    BOOL Root,
    PSTR *Buffer,
    PUINTN BufferSize
    )

/*++

Routine Description:

    This routine retrieves a pointer to a null terminated string containing the
    path to the current working directory or the current root directory.

Arguments:

    Root - Supplies a boolean indicating whether caller would like the current
        working directory (FALSE) or the path to the current root directory
        (TRUE). If the caller does not have permission to escape roots, or
        does not currently have an altered root directory, then / is returned.

    Buffer - Supplies a pointer that receives a pointer to a buffer that
        contains a null terminated string for the path to the current
        directory.

    BufferSize - Supplies a pointer that receives the size of the buffer, in
        bytes.

Return Value:

    Status code.

--*/

{

    PSTR CurrentDirectory;
    UINTN CurrentDirectorySize;
    ULONG Index;
    SYSTEM_CALL_GET_CURRENT_DIRECTORY Parameters;
    KSTATUS Status;

    CurrentDirectorySize = OS_GET_CURRENT_DIRECTORY_BUFFER_SIZE_GUESS;
    for (Index = 0; Index < OS_GET_CURRENT_DIRECTORY_TRY_COUNT; Index += 1) {
        CurrentDirectory = OsHeapAllocate(CurrentDirectorySize,
                                          OS_CURRENT_DIRECTORY_ALLOCATION_TAG);

        if (CurrentDirectory == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto GetCurrentDirectoryEnd;
        }

        Parameters.Root = Root;
        Parameters.Buffer = CurrentDirectory;
        Parameters.BufferSize = CurrentDirectorySize;
        Status = OsSystemCall(SystemCallGetCurrentDirectory, &Parameters);

        //
        // Exit on any status besides a buffer too small result.
        //

        if (Status != STATUS_BUFFER_TOO_SMALL) {
            break;
        }

        //
        // If the buffer is too small. Double the expected size just in case
        // another thread changes directories.
        //

        CurrentDirectorySize = Parameters.BufferSize * 2;
        OsHeapFree(CurrentDirectory);
        CurrentDirectory = NULL;
    }

GetCurrentDirectoryEnd:
    if (KSUCCESS(Status)) {
        *Buffer = Parameters.Buffer;
        *BufferSize = Parameters.BufferSize;

    } else {
        if (CurrentDirectory != NULL) {
            OsHeapFree(CurrentDirectory);
        }
    }

    return Status;
}

OS_API
KSTATUS
OsChangeDirectory (
    BOOL Root,
    PSTR Path,
    ULONG PathSize
    )

/*++

Routine Description:

    This routine sets the current working directory or current root directory.

Arguments:

    Root - Supplies a boolean indicating whether to change the current working
        directory (FALSE) or the current root directory (TRUE). If attempting
        to change the root, the caller must have permission to change the root,
        must be running a single thread, and must not have any other handles
        to directories open.

    Path - Supplies a pointer to the path of the new working directory. If
        trying to escape the root, supply NULL here. The caller must have
        sufficient privileges to escape a root.

    PathSize - Supplies the size of the path directory string in bytes
        including the null terminator.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_CHANGE_DIRECTORY Parameters;

    Parameters.Root = Root;
    Parameters.Buffer = Path;
    Parameters.BufferLength = PathSize;
    Parameters.Handle = INVALID_HANDLE;
    return OsSystemCall(SystemCallChangeDirectory, &Parameters);
}

OS_API
KSTATUS
OsChangeDirectoryHandle (
    BOOL Root,
    HANDLE Handle
    )

/*++

Routine Description:

    This routine sets the current working directory or root directory to the
    same directory opened with the given file handle.

Arguments:

    Root - Supplies a boolean indicating whether to change the current working
        directory (FALSE) or the current root directory (TRUE). If attempting
        to change the root, the caller must have permission to change the root,
        must be running a single thread, and must not have any other handles
        to directories open.

    Handle - Supplies an open handle to a directory to change the current
        working directroy to. Supply INVALID_HANDLE here to escape the root.
        The caller must have sufficient privileges to escape a root.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_CHANGE_DIRECTORY Parameters;

    Parameters.Root = Root;
    Parameters.Buffer = NULL;
    Parameters.BufferLength = 0;
    Parameters.Handle = Handle;
    return OsSystemCall(SystemCallChangeDirectory, &Parameters);
}

OS_API
KSTATUS
OsPoll (
    PSIGNAL_SET SignalMask,
    PPOLL_DESCRIPTOR Descriptors,
    ULONG DescriptorCount,
    ULONG TimeoutInMilliseconds,
    PULONG DescriptorsSelected
    )

/*++

Routine Description:

    This routine polls several I/O handles.

Arguments:

    SignalMask - Supplies an optional pointer to a mask to set for the
        duration of the wait.

    Descriptors - Supplies a pointer to an array of poll descriptor structures
        describing the descriptors and events to wait on.

    DescriptorCount - Supplies the number of descriptors in the array.

    TimeoutInMilliseconds - Supplies the number of milliseconds to wait before
        giving up.

    DescriptorsSelected - Supplies a pointer where the number of descriptors
        that had activity will be returned on success.

Return Value:

    STATUS_SUCCESS if one or more descriptors is ready for action.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated for the
        operation.

    STATUS_INTERRUPTED if a signal was caught during the wait.

    STATUS_TIMEOUT if no descriptors were ready in the given amount of time.

    STATUS_INVALID_PARAMETER if more than MAX_LONG descriptors are supplied.

--*/

{

    SYSTEM_CALL_POLL Poll;
    INTN Result;

    if (DescriptorCount > (ULONG)MAX_LONG) {
        return STATUS_INVALID_PARAMETER;
    }

    Poll.SignalMask = SignalMask;
    Poll.Descriptors = Descriptors;
    Poll.DescriptorCount = (LONG)DescriptorCount;
    Poll.TimeoutInMilliseconds = TimeoutInMilliseconds;
    Result = OsSystemCall(SystemCallPoll, &Poll);
    if (Result < 0) {
        *DescriptorsSelected = 0;
        return Result;
    }

    *DescriptorsSelected = (ULONG)Result;
    return STATUS_SUCCESS;
}

OS_API
PSIGNAL_HANDLER_ROUTINE
OsSetSignalHandler (
    PSIGNAL_HANDLER_ROUTINE NewHandler
    )

/*++

Routine Description:

    This routine sets the signal handler routine called whenever a signal is
    delivered by the kernel.

Arguments:

    NewHandler - Supplies a pointer to the new handler routine to use.

Return Value:

    Returns a pointer to the old handler, or NULL if no other signal handlers
    were registered.

--*/

{

    PSIGNAL_HANDLER_ROUTINE OldHandler;

    OldHandler = (PVOID)RtlAtomicExchange((PVOID)&OsSignalHandler,
                                          (UINTN)NewHandler);

    return OldHandler;
}

OS_API
KSTATUS
OsSendSignal (
    SIGNAL_TARGET_TYPE TargetType,
    ULONG TargetId,
    ULONG SignalNumber,
    SHORT SignalCode,
    UINTN SignalParameter
    )

/*++

Routine Description:

    This routine sends a signal to a process, process group or thread.

Arguments:

    TargetType - Supplies the target type to which the signal is being sent. It
        can be either a process, process group, or thread.

    TargetId - Supplies the ID for the signal's target process, process group,
        or thread.

    SignalNumber - Supplies the signal number to send.

    SignalCode - Supplies the signal code to send. See SIGNAL_CODE_*
        definitions.

    SignalParameter - Supplies a parameter to send with the signal if the
        signal is in the real time signal range.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_SEND_SIGNAL SendSignal;

    SendSignal.TargetType = TargetType;
    SendSignal.TargetId = TargetId;
    SendSignal.SignalNumber = SignalNumber;
    SendSignal.SignalCode = SignalCode;
    SendSignal.SignalParameter = SignalParameter;
    return OsSystemCall(SystemCallSendSignal, &SendSignal);
}

OS_API
KSTATUS
OsGetProcessId (
    PROCESS_ID_TYPE ProcessIdType,
    PPROCESS_ID ProcessId
    )

/*++

Routine Description:

    This routine gets an identifier associated with the process, such as the
    process ID, thread ID, parent process ID, process group ID, and session ID.

Arguments:

    ProcessIdType - Supplies the type of ID to get.

    ProcessId - Supplies a pointer that on input contains the process ID
        argument if the operation takes a parameter. On successful output,
        returns the desired ID. Supply zero to use the calling process ID.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_GET_SET_PROCESS_ID Parameters;
    INTN Result;

    //
    // TODO: These values should be read once and cached, which means changing
    // the system call interface and then invalidating on fork.
    //

    Parameters.ProcessIdType = ProcessIdType;
    Parameters.ProcessId = *ProcessId;
    Parameters.NewValue = 0;
    Parameters.Set = FALSE;
    Result = OsSystemCall(SystemCallGetSetProcessId, &Parameters);
    if (Result < 0) {
        return Result;
    }

    *ProcessId = Result;
    return STATUS_SUCCESS;
}

OS_API
KSTATUS
OsSetProcessId (
    PROCESS_ID_TYPE ProcessIdType,
    PROCESS_ID ProcessId,
    PROCESS_ID NewValue
    )

/*++

Routine Description:

    This routine sets an identifier associated with the process, such as the
    process group ID or session ID.

Arguments:

    ProcessIdType - Supplies the type of ID to set. Not all types can be set.

    ProcessId - Supplies the ID of the process to change. Supply 0 to use the
        current process.

    NewValue - Supplies the new value to set.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_GET_SET_PROCESS_ID Parameters;

    Parameters.ProcessIdType = ProcessIdType;
    Parameters.ProcessId = ProcessId;
    Parameters.NewValue = NewValue;
    Parameters.Set = TRUE;
    return OsSystemCall(SystemCallGetSetProcessId, &Parameters);
}

OS_API
SIGNAL_SET
OsSetSignalBehavior (
    SIGNAL_MASK_TYPE MaskType,
    SIGNAL_MASK_OPERATION Operation,
    PSIGNAL_SET NewMask
    )

/*++

Routine Description:

    This routine sets signal behavior, either for the current thread in the
    case of the blocked signals, or for the process for other signal masks.

Arguments:

    MaskType - Supplies the type of mask to change.

    Operation - Supplies the interaction between the new mask and the previous
        signal mask.

    NewMask - Supplies a pointer to the mask of signals that are affected by
        this operation.

Return Value:

    Returns the original signal mask before this function was called.

--*/

{

    SYSTEM_CALL_SET_SIGNAL_BEHAVIOR SetSignalBehavior;

    SetSignalBehavior.MaskType = MaskType;
    SetSignalBehavior.Operation = Operation;
    if (NewMask != NULL) {
        SetSignalBehavior.SignalSet = *NewMask;

    } else {
        INITIALIZE_SIGNAL_SET(SetSignalBehavior.SignalSet);
    }

    OsSystemCall(SystemCallSetSignalBehavior, &SetSignalBehavior);
    return SetSignalBehavior.SignalSet;
}

OS_API
KSTATUS
OsWaitForChildProcess (
    ULONG Flags,
    PPROCESS_ID ChildPid,
    PULONG Reason,
    PUINTN ChildExitValue,
    PRESOURCE_USAGE ChildResourceUsage
    )

/*++

Routine Description:

    This routine is called to suspend execution of the current thread until
    a child process completes.

Arguments:

    Flags - Supplies a bitfield of flags governing the behavior of the wait.
        See SYSTEM_CALL_WAIT_FLAG_* definitions.

    ChildPid - Supplies a pointer that on input supplies the child process ID
        parameter. This parameter can be one of the following:

        If -1 is supplied, any child signal will be pulled off and returned.

        If a number greater than 0 is supplied, only the specific process ID
        will be pulled off and returned.

        If 0 is supplied, any child process whose process group ID is equal to
        that of the calling process will be pulled.

        If a number less than zero (but not -1) is supplied, then any process
        whose process group ID is equal to the absolute value of this parameter
        will be dequeued and returned.

        On output, this parameter will contain the process ID of the child that
        generated the signal activity, and the child signal will be discarded.
        If the wait for child parameter is set to FALSE, then this parameter
        is ignored. If a non-child signal caused the routine to return, then
        the value at this parameter is undefined.

    Reason - Supplies a pointer where the reason for the child event will be
        returned. See CHILD_SIGNAL_REASON_* definitions.

    ChildExitValue - Supplies a pointer where the child exit value (or signal
        that caused the event) will be returned.

    ChildResourceUsage - Supplies an optional pointer where the resource usage
        of the child will be returned on success.

Return Value:

    STATUS_SUCCESS if the wait was successfully satisfied.

    STATUS_NO_DATA_AVAILABLE if the SYSTEM_CALL_WAIT_FLAG_RETURN_IMMEDIATELY
    flag is set and there are no children ready to be reaped. The child PID is
    returned as -1.

    STATUS_INTERRUPTED if the wait was interrupted by a signal.

    STATUS_NO_ELIGIBLE_CHILDREN if no eligible children could be reaped.

--*/

{

    SYSTEM_CALL_WAIT_FOR_CHILD Parameters;
    KSTATUS Status;

    Parameters.Flags = Flags;
    Parameters.ChildPid = -1;
    if (ChildPid != NULL) {
        Parameters.ChildPid = *ChildPid;
    }

    Parameters.ResourceUsage = ChildResourceUsage;
    Status = OsSystemCall(SystemCallWaitForChildProcess, &Parameters);
    if (ChildPid != NULL) {
        *ChildPid = Parameters.ChildPid;
    }

    if (Reason != NULL) {
        *Reason = Parameters.Reason;
    }

    if (ChildExitValue != NULL) {
        *ChildExitValue = Parameters.ChildExitValue;
    }

    return Status;
}

OS_API
KSTATUS
OsSuspendExecution (
    SIGNAL_MASK_OPERATION SignalOperation,
    PSIGNAL_SET SignalSet,
    PSIGNAL_PARAMETERS SignalParameters,
    ULONG TimeoutInMilliseconds
    )

/*++

Routine Description:

    This routine suspends execution of the current thread until a signal comes
    in. The current thread's blocked signal mask can be changed for the
    duration of the call by providing an operation and a signal set.

Arguments:

    SignalOperation - Supplies the operation to perform with the signal set for
        the duration of the call: set, clear, overwrite or none.

    SignalSet - Supplies a pointer to the signal set to apply for the duration
        of this system call as dictated by the signal operation.

    SignalParameters - Supplies an optional pointer where the signal
        information for the signal that occurred will be returned.

    TimeoutInMilliseconds - Supplies the timeout of the operation in
        milliseconds.

Return Value:

    STATUS_SUCCESS if a signal arrived.

    STATUS_INTERRUPTED on a clear signal operation if a signal that is not in
    the given set arrived.

    STATUS_TIMEOUT if no signal arrived before the given timeout expires.

    STATUS_INVALID_PARAMETER if no signal set is supplied for an operation
    other than SignalMaskOperationNone.

--*/

{

    SYSTEM_CALL_SUSPEND_EXECUTION Parameters;

    if (SignalSet == NULL) {
        if (SignalOperation != SignalMaskOperationNone) {
            return STATUS_INVALID_PARAMETER;
        }

    } else {
        Parameters.SignalSet = *SignalSet;
    }

    Parameters.SignalOperation = SignalOperation;
    Parameters.SignalParameters = SignalParameters;
    Parameters.TimeoutInMilliseconds = TimeoutInMilliseconds;
    return OsSystemCall(SystemCallSuspendExecution, &Parameters);
}

OS_API
NO_RETURN
VOID
OsExitProcess (
    UINTN Status
    )

/*++

Routine Description:

    This routine terminates the current process and any threads that may be
    running in it.

Arguments:

    Status - Supplies the exit status, returned to the parent in the wait
        calls. Conventionally 0 indicates success, and non-zero indicates
        failure. The C library only receives the first eight bits of the return
        status, portable applications should not set bits beyond that.

Return Value:

    This routine does not return.

--*/

{

    OsSystemCall(SystemCallExitProcess, (PVOID)Status);
    while (TRUE) {

        ASSERT(FALSE);

    }
}

OS_API
KSTATUS
OsFileControl (
    HANDLE Handle,
    FILE_CONTROL_COMMAND Command,
    PFILE_CONTROL_PARAMETERS_UNION Parameters
    )

/*++

Routine Description:

    This routine performs a file control operation on the given handle.

Arguments:

    Handle - Supplies the file handle to operate on.

    Command - Supplies the command to perform.

    Parameters - Supplies an optional pointer to any additional parameters
        needed by the command.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_FILE_CONTROL FileControl;

    FileControl.File = Handle;
    FileControl.Command = Command;
    FileControl.Parameters = Parameters;
    return OsSystemCall(SystemCallFileControl, &FileControl);
}

OS_API
KSTATUS
OsGetFileInformation (
    HANDLE Directory,
    PSTR Path,
    ULONG PathLength,
    BOOL FollowLink,
    PFILE_PROPERTIES Properties
    )

/*++

Routine Description:

    This routine gets the file properties for a given file.

Arguments:

    Directory - Supplies an optional handle to a directory to start the
        search from if the supplied path is relative. Supply INVALID_HANDLE
        here to to use the current directory for relative paths.

    Path - Supplies a pointer to the string containing the file path to get
        properties for.

    PathLength - Supplies the length of the path string in bytes including the
        null terminator.

    FollowLink - Supplies a boolean indicating what to do if the file path
        points to a symbolic link. If set to TRUE, the file information set or
        returned will be for the file the link points to. If FALSE, the call
        will set or get information for the link itself.

    Properties - Supplies a pointer where the file properties will be returned
        on success.

Return Value:

    Status code.

--*/

{

    SET_FILE_INFORMATION Request;
    KSTATUS Status;

    Request.FieldsToSet = 0;
    Request.FileProperties = Properties;
    Status = OspGetSetFileInformation(Directory,
                                      Path,
                                      PathLength,
                                      FollowLink,
                                      &Request);

    return Status;
}

OS_API
KSTATUS
OsSetFileInformation (
    HANDLE Directory,
    PSTR Path,
    ULONG PathLength,
    BOOL FollowLink,
    PSET_FILE_INFORMATION Request
    )

/*++

Routine Description:

    This routine sets the file properties for a given file.

Arguments:

    Directory - Supplies an optional handle to a directory to start the
        search from if the supplied path is relative. Supply INVALID_HANDLE
        here to to use the current directory for relative paths.

    Path - Supplies a pointer to the string containing the file path to set
        properties for.

    PathLength - Supplies the length of the path string in bytes including the
        null terminator.

    FollowLink - Supplies a boolean indicating what to do if the file path
        points to a symbolic link. If set to TRUE, the file information set or
        returned will be for the file the link points to. If FALSE, the call
        will set or get information for the link itself.

    Request - Supplies a pointer to the set file information request.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = OspGetSetFileInformation(Directory,
                                      Path,
                                      PathLength,
                                      FollowLink,
                                      Request);

    return Status;
}

OS_API
VOID
OsDebugPrint (
    PSTR String,
    ULONG StringSize
    )

/*++

Routine Description:

    This routine prints a message to the debug console. No formatting is
    provided.

Arguments:

    String - Supplies a pointer to the string to print.

    StringSize - Supplies the size of the string in bytes including the null
        terminator.

Return Value:

    None.

--*/

{

    OsDebug(DebugCommandPrint, 0, NULL, String, StringSize, 0);
    return;
}

OS_API
KSTATUS
OsDebug (
    DEBUG_COMMAND_TYPE Command,
    PROCESS_ID Process,
    PVOID Address,
    PVOID Data,
    ULONG Size,
    ULONG SignalToDeliver
    )

/*++

Routine Description:

    This routine sends a debug command to a process.

Arguments:

    Command - Supplies the command to send.

    Process - Supplies the process ID to send the command to.

    Address - Supplies the address parameter of the command, usually the
        address in the target to read from or write to.

    Data - Supplies the data parameter, usually the buffer containing the data
        to write or the buffer where the read data will be returned.

    Size - Supplies the size of the buffer, in bytes.

    SignalToDeliver - Supplies the signal number to deliver to the debugged
        process for step and continue commands. For other commands, this
        parameter is ignored.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_DEBUG Request;

    Request.Command.Command = Command;
    Request.Process = Process;
    Request.Command.U.Address = Address;
    Request.Command.Data = Data;
    Request.Command.Size = Size;
    Request.Command.SignalToDeliver = SignalToDeliver;
    OsSystemCall(SystemCallDebug, &Request);
    return Request.Command.Status;
}

OS_API
KSTATUS
OsSeek (
    HANDLE Handle,
    SEEK_COMMAND SeekCommand,
    IO_OFFSET Offset,
    PIO_OFFSET NewOffset
    )

/*++

Routine Description:

    This routine seeks to the given position in a file. This routine is only
    relevant for normal file or block based devices.

Arguments:

    Handle - Supplies the open file handle.

    SeekCommand - Supplies the reference point for the seek offset. Usual
        reference points are the beginning of the file, current file position,
        and the end of the file.

    Offset - Supplies the offset from the reference point to move in bytes.

    NewOffset - Supplies an optional pointer where the file position after the
        move will be returned on success.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_SEEK Request;
    KSTATUS Status;

    Request.Handle = Handle;
    Request.Command = SeekCommand;
    Request.Offset = Offset;
    Status = OsSystemCall(SystemCallSeek, &Request);
    if (NewOffset != NULL) {
        *NewOffset = Request.Offset;
    }

    return Status;
}

OS_API
KSTATUS
OsCreateSymbolicLink (
    HANDLE Directory,
    PSTR Path,
    ULONG PathSize,
    PSTR LinkDestinationBuffer,
    ULONG LinkDestinationBufferSize
    )

/*++

Routine Description:

    This routine creates a symbolic link.

Arguments:

    Directory - Supplies an optional handle to a directory to start the
        search from if the supplied path is relative. Supply INVALID_HANDLE
        here to to use the current directory for relative paths.

    Path - Supplies a pointer to the symbolic link path.

    PathSize - Supplies the size of the symbolic link pointer in bytes
        including the null terminator.

    LinkDestinationBuffer - Supplies a pointer to a string containing the link's
        target path, the location the link points to.

    LinkDestinationBufferSize - Supplies the size of the link destination
        buffer in bytes, NOT including the null terminator.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_CREATE_SYMBOLIC_LINK Parameters;

    Parameters.Directory = Directory;
    Parameters.Path = Path;
    Parameters.PathSize = PathSize;
    Parameters.LinkDestinationBuffer = LinkDestinationBuffer;
    Parameters.LinkDestinationBufferSize = LinkDestinationBufferSize;
    return OsSystemCall(SystemCallCreateSymbolicLink, &Parameters);
}

OS_API
KSTATUS
OsReadSymbolicLink (
    HANDLE Directory,
    PSTR Path,
    ULONG PathSize,
    PSTR LinkDestinationBuffer,
    ULONG LinkDestinationBufferSize,
    PULONG LinkDestinationSize
    )

/*++

Routine Description:

    This routine reads the destination path of a symbolic link.

Arguments:

    Directory - Supplies an optional handle to a directory to start the
        search from if the supplied path is relative. Supply INVALID_HANDLE
        here to to use the current directory for relative paths.

    Path - Supplies a pointer to the symbolic link path.

    PathSize - Supplies the size of the symbolic link pointer in bytes
        including the null terminator.

    LinkDestinationBuffer - Supplies a pointer to a buffer where the
        destination of the link will be returned. A null terminator byte is not
        written.

    LinkDestinationBufferSize - Supplies the size of the link destination
        buffer in bytes.

    LinkDestinationSize - Supplies a pointer where the actual size of the
        link destination (including the null terminator) will be returned on
        either success or a STATUS_BUFFER_TOO_SMALL case. On failure, 0 will be
        returned here.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_TOO_SMALL if the link destination buffer was not large
    enough to store the link destination.

    Other errors on other failures.

--*/

{

    SYSTEM_CALL_READ_SYMBOLIC_LINK Parameters;
    KSTATUS Status;

    Parameters.Directory = Directory;
    Parameters.Path = Path;
    Parameters.PathSize = PathSize;
    Parameters.LinkDestinationBuffer = LinkDestinationBuffer;
    Parameters.LinkDestinationBufferSize = LinkDestinationBufferSize;
    Status = OsSystemCall(SystemCallReadSymbolicLink, &Parameters);
    *LinkDestinationSize = Parameters.LinkDestinationSize;
    return Status;
}

OS_API
KSTATUS
OsCreateHardLink (
    HANDLE ExistingFileDirectory,
    PSTR ExistingFile,
    ULONG ExistingFileSize,
    HANDLE LinkDirectory,
    PSTR LinkPath,
    ULONG LinkPathSize,
    BOOL FollowExistingFileLinks
    )

/*++

Routine Description:

    This routine creates a hard link.

Arguments:

    ExistingFileDirectory - Supplies an optional handle to the directory to
        start path traversal from if the specified existing file path is
        relative. Supply INVALID_HANDLE here to use the current directory for
        relative paths.

    ExistingFile - Supplies a pointer to the path of the existing file to
        create the link from.

    ExistingFileSize - Supplies the size of the existing file path buffer in
        bytes, including the null terminator.

    LinkDirectory - Supplies an optional handle to the directory to start path
        traversal from if the specified new link path is relative. Supply
        INVALID_HANDLE here to use the current directory for relative paths.

    LinkPath - Supplies a pointer to a string containing the destination path
        of the new link.

    LinkPathSize - Supplies the size of the link path buffer in bytes.

    FollowExistingFileLinks - Supplies a boolean indicating that if the
        existing file path exists and is a symbolic link, the new link shall be
        for the target of that link (TRUE) rather than the link itself (FALSE).

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_CREATE_HARD_LINK Parameters;

    Parameters.ExistingFileDirectory = ExistingFileDirectory;
    Parameters.ExistingFilePath = ExistingFile;
    Parameters.ExistingFilePathSize = ExistingFileSize;
    Parameters.NewLinkDirectory = LinkDirectory;
    Parameters.NewLinkPath = LinkPath;
    Parameters.NewLinkPathSize = LinkPathSize;
    Parameters.FollowLinks = FollowExistingFileLinks;
    return OsSystemCall(SystemCallCreateHardLink, &Parameters);
}

OS_API
KSTATUS
OsDelete (
    HANDLE Directory,
    PSTR Path,
    ULONG PathSize,
    ULONG Flags
    )

/*++

Routine Description:

    This routine attempts to delete the object at the given path. If the path
    points to a directory, the directory must be empty. If the path points to
    a file, the hard link count on the file is decremented. If the hard link
    count reaches zero and no processes have the file open, the contents of the
    file are destroyed. If processes have open handles to the file, the
    destruction of the file contents are deferred until the last handle to the
    old file is closed. If the path points to a symbolic link, the link itself
    is removed and not the destination. The removal of the entry from the
    directory is immediate.

Arguments:

    Directory - Supplies an optional handle to a directory to start the
        search from if the supplied path is relative. Supply INVALID_HANDLE
        here to to use the current directory for relative paths.

    Path - Supplies a pointer to the path to delete.

    PathSize - Supplies the length of the path buffer in bytes, including the
        null terminator.

    Flags - Supplies flags associated with the delete operation. See
        SYS_DELETE_FLAG_* definitions.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_DELETE Parameters;

    Parameters.Directory = Directory;
    Parameters.Path = Path;
    Parameters.PathSize = PathSize;
    Parameters.Flags = Flags;
    return OsSystemCall(SystemCallDelete, &Parameters);
}

OS_API
KSTATUS
OsRename (
    HANDLE SourceDirectory,
    PSTR SourcePath,
    ULONG SourcePathSize,
    HANDLE DestinationDirectory,
    PSTR DestinationPath,
    ULONG DestinationPathSize
    )

/*++

Routine Description:

    This routine attempts to rename the object at the given path. This routine
    operates on symbolic links themselves, not the destinations of symbolic
    links. If the source and destination paths are equal, this routine will do
    nothing and return successfully. If the source path is not a directory, the
    destination path must not be a directory. If the destination file exists,
    it will be deleted. The caller must have write access in both the old and
    new directories. If the source path is a directory, the destination path
    must not exist or be an empty directory. The destination path must not have
    a path prefix of the source (ie it's illegal to move /my/path into
    /my/path/stuff).

Arguments:

    SourceDirectory - Supplies an optional handle to the directory to start
        source path searches from. If the source path is absolute, this value
        is ignored. If this is INVALID_HANDLE, then source path searches will
        start from the current working directory.

    SourcePath - Supplies a pointer to the path of the file to rename.

    SourcePathSize - Supplies the length of the source path buffer in bytes,
        including the null terminator.

    DestinationDirectory - Supplies an optional handle to the directory to
        start destination path searches from. If the destination path is
        absolute, this value is ignored. If this is INVALID_HANDLE, then
        destination path searches will start from the current working directory.

    DestinationPath - Supplies a pointer to the path to rename the file to.

    DestinationPathSize - Supplies the size of the destination path buffer in
        bytes, including the null terminator.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_RENAME Parameters;

    Parameters.SourceDirectory = SourceDirectory;
    Parameters.SourcePath = SourcePath;
    Parameters.SourcePathSize = SourcePathSize;
    Parameters.DestinationDirectory = DestinationDirectory;
    Parameters.DestinationPath = DestinationPath;
    Parameters.DestinationPathSize = DestinationPathSize;
    return OsSystemCall(SystemCallRename, &Parameters);
}

OS_API
KSTATUS
OsUserControl (
    HANDLE Handle,
    ULONG RequestCode,
    PVOID Context,
    UINTN ContextSize
    )

/*++

Routine Description:

    This routine sends a user I/O request to the given file/device/etc.

Arguments:

    Handle - Supplies the open file handle to send the request to.

    RequestCode - Supplies the request to send. For device handles, this is the
        minor code of the IRP.

    Context - Supplies a pointer to the context buffer allocated by the
        caller for the request.

    ContextSize - Supplies the size of the supplied context buffer.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_USER_CONTROL Parameters;

    Parameters.Handle = Handle;
    Parameters.RequestCode = RequestCode;
    Parameters.Context = Context;
    Parameters.ContextSize = ContextSize;
    return OsSystemCall(SystemCallUserControl, &Parameters);
}

OS_API
KSTATUS
OsMount (
    PSTR MountPointPath,
    ULONG MountPointPathSize,
    PSTR TargetPath,
    ULONG TargetPathSize,
    ULONG Flags
    )

/*++

Routine Description:

    This routine attempts to mount the given target at the given mount point.

Arguments:

    MountPointPath - Supplies a pointer to a string containing the path to the
        mount point where the target is to be mounted.

    MountPointPathSize - Supplies the size of the mount point path string in
        bytes, including the null terminator.

    TargetPath - Supplies a pointer to a string containing the path to the
        target file, directory, volume, or device that is to be mounted.

    TargetPathSize - Supplies the size of the target path string in bytes,
        including the null terminator.

    Flags - Supplies the flags associated with the mount operation. See
        SYS_MOUNT_FLAG_* for definitions.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Flags &= ~SYS_MOUNT_FLAG_UNMOUNT;
    Status = OspMountOrUnmount(MountPointPath,
                               MountPointPathSize,
                               TargetPath,
                               TargetPathSize,
                               Flags);

    return Status;
}

OS_API
KSTATUS
OsUnmount (
    PSTR MountPointPath,
    ULONG MountPointPathSize,
    ULONG Flags
    )

/*++

Routine Description:

    This routine attempts to unmount the given target. If the target is not a
    mount point, or the user does not have access to the mount point, then the
    routine will return the appropriate error. Otherwise, it will remove the
    mount point based on the supplied flags.

Arguments:

    MountPointPath - Supplies a pointer to a string containing the path to the
        mount point that is to be unmounted.

    MountPointPathSize - Supplies the size of the mount point path string in
        bytes, including the null terminator.

    Flags - Supplies the flags associated with the mount operation. See
        SYS_MOUNT_FLAG_* for definitions.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Flags |= SYS_MOUNT_FLAG_UNMOUNT;
    Status = OspMountOrUnmount(MountPointPath,
                               MountPointPathSize,
                               NULL,
                               0,
                               Flags);

    return Status;
}

OS_API
KSTATUS
OsGetMountPoints (
    PVOID *Buffer,
    PUINTN BufferSize
    )

/*++

Routine Description:

    This routine returns the list of mount points currently in the system. It
    only returns the mounts that are visible to the calling process. The caller
    is responsible for releasing the buffer.

Arguments:

    Buffer - Supplies a pointer that receives a buffer of mount point data.

    BufferSize - Supplies a pointer that receives the size of the mount point
        data.

Return Value:

    Status code.

--*/

{

    ULONG Index;
    PVOID MountPoints;
    UINTN MountPointsSize;
    KSTATUS Status;

    MountPointsSize = OS_GET_MOUNT_POINTS_BUFFER_SIZE_GUESS;
    for (Index = 0; Index < OS_GET_MOUNT_POINTS_TRY_COUNT; Index += 1) {
        MountPoints = OsHeapAllocate(MountPointsSize, OS_MOUNT_ALLOCATION_TAG);
        if (MountPoints == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto GetMountPointsEnd;
        }

        Status = OsGetSetSystemInformation(SystemInformationIo,
                                           IoInformationMountPoints,
                                           MountPoints,
                                           &MountPointsSize,
                                           FALSE);

        //
        // Exit on any status besides a buffer too small result.
        //

        if (Status != STATUS_BUFFER_TOO_SMALL) {
            break;
        }

        //
        // If the buffer is too small. Double the expected size just in case
        // something else sneaks in and try again.
        //

        MountPointsSize *= 2;
        OsHeapFree(MountPoints);
    }

GetMountPointsEnd:
    if (KSUCCESS(Status)) {
        *Buffer = MountPoints;
        *BufferSize = MountPointsSize;

    } else {
        OsHeapFree(MountPoints);
    }

    return Status;
}

OS_API
KSTATUS
OsGetEffectiveAccess (
    HANDLE Directory,
    PSTR Path,
    ULONG PathSize,
    ULONG DesiredFlags,
    BOOL UseRealIds,
    PULONG EffectiveAccess
    )

/*++

Routine Description:

    This routine determines the effective access for the given path.

Arguments:

    Directory - Supplies an optional handle to a directory to start the
        search from if the supplied path is relative. Supply INVALID_HANDLE
        here to to use the current directory for relative paths.

    Path - Supplies a pointer to a string containing the path to the object
        to query access of.

    PathSize - Supplies the size of the supplied path buffer in bytes including
        the null terminator.

    DesiredFlags - Supplies the bitfield of flags the user is interested in.
        See EFFECTIVE_ACCESS_* definitions.

    UseRealIds - Supplies a boolean indicating whether the access check should
        use the real user and group IDs (TRUE) or the effective user and group
        IDs (FALSE).

    EffectiveAccess - Supplies a pointer where the effective access permission
        bits are returned. Only bits set in the desired flags parameter will be
        potentially set. All other bits will be set to 0. See
        EFFECTIVE_ACCESS_* definitions.

Return Value:

    Status code indicating if the request completed successfully.

--*/

{

    SYSTEM_CALL_GET_EFFECTIVE_ACCESS Parameters;
    KSTATUS Status;

    Parameters.Directory = Directory;
    Parameters.FilePath = Path;
    Parameters.FilePathSize = PathSize;
    Parameters.UseRealIds = UseRealIds;
    Parameters.DesiredFlags = DesiredFlags;
    Parameters.EffectiveAccess = 0;
    Status = OsSystemCall(SystemCallGetEffectiveAccess, &Parameters);
    *EffectiveAccess = Parameters.EffectiveAccess;
    return Status;
}

OS_API
KSTATUS
OsLoadDriver (
    PSTR Path,
    ULONG PathSize
    )

/*++

Routine Description:

    This routine loads the given driver into kernel address space.

Arguments:

    Path - Supplies a pointer to a string containing the path to the driver.

    PathSize - Supplies the size of the supplied path buffer in bytes including
        the null terminator.

Return Value:

    Status code indicating if the request completed successfully.

--*/

{

    SYSTEM_CALL_LOAD_DRIVER Parameters;

    Parameters.DriverName = Path;
    Parameters.DriverNameSize = PathSize;
    return OsSystemCall(SystemCallLoadDriver, &Parameters);
}

OS_API
KSTATUS
OsLocateDeviceInformation (
    PUUID Uuid,
    PDEVICE_ID DeviceId,
    PDEVICE_INFORMATION_RESULT Results,
    PULONG ResultCount
    )

/*++

Routine Description:

    This routine returns instances of devices enumerating information. Callers
    can get all devices enumerating the given information type, or all
    information types enumerated by a given device. This routine must be called
    at low level.

Arguments:

    Uuid - Supplies an optional pointer to the information identifier to
        filter on. If NULL, any information type will match.

    DeviceId - Supplies an optional pointer to the device ID to match against.
        If NULL, then any device will match.

    Results - Supplies a pointer to a caller allocated buffer where the
        results will be returned.

    ResultCount - Supplies a pointer that upon input contains the size of the
        buffer in information result elements. On output, returns the number
        of elements in the query, even if the provided buffer was too small.
        Do note however that the number of results can change between two
        successive searches.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_TOO_SMALL if the provided buffer was not large enough to
    contain all the results. The result count will contain the required number
    of elements to contain the results.

--*/

{

    SYSTEM_CALL_LOCATE_DEVICE_INFORMATION Request;
    KSTATUS Status;

    RtlZeroMemory(&Request, sizeof(SYSTEM_CALL_LOCATE_DEVICE_INFORMATION));
    if (Uuid != NULL) {
        Request.ByUuid = TRUE;
        RtlCopyMemory(&(Request.Uuid), Uuid, sizeof(UUID));
    }

    if (DeviceId != NULL) {
        Request.ByDeviceId = TRUE;
        Request.DeviceId = *DeviceId;
    }

    Request.Results = Results;
    Request.ResultCount = *ResultCount;
    Status = OsSystemCall(SystemCallLocateDeviceInformation, &Request);
    *ResultCount = Request.ResultCount;
    return Status;
}

OS_API
KSTATUS
OsGetSetDeviceInformation (
    DEVICE_ID DeviceId,
    PUUID Uuid,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets device information.

Arguments:

    DeviceId - Supplies the device ID of the device to get or set information
        for.

    Uuid - Supplies a pointer to the identifier of the device information type
        to get or set.

    Data - Supplies a pointer to the data buffer that either contains the
        information to set or will contain the information to get on success.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output contains the actual size of the data.

    Set - Supplies a boolean indicating whether to get information (FALSE) or
        set information (TRUE).

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_GET_SET_DEVICE_INFORMATION Request;
    KSTATUS Status;

    RtlCopyMemory(&(Request.Uuid), Uuid, sizeof(UUID));
    Request.DeviceId = DeviceId;
    Request.Data = Data;
    Request.DataSize = *DataSize;
    Request.Set = Set;
    Status = OsSystemCall(SystemCallGetSetDeviceInformation, &Request);
    *DataSize = Request.DataSize;
    return Status;
}

OS_API
KSTATUS
OsGetSetSystemInformation (
    SYSTEM_INFORMATION_SUBSYSTEM Subsystem,
    UINTN InformationType,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

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

{

    SYSTEM_CALL_GET_SET_SYSTEM_INFORMATION Request;
    KSTATUS Status;

    Request.Subsystem = Subsystem;
    Request.InformationType = InformationType;
    Request.Data = Data;
    Request.DataSize = *DataSize;
    Request.Set = Set;
    Status = OsSystemCall(SystemCallGetSetSystemInformation, &Request);
    *DataSize = Request.DataSize;
    return Status;
}

OS_API
KSTATUS
OsResetSystem (
    SYSTEM_RESET_TYPE ResetType
    )

/*++

Routine Description:

    This routine attempts to reboot the system.

Arguments:

    ResetType - Supplies the desired system reset type. If the given type is
        not supported and a cold reset is, then a cold reset will be
        performed.

Return Value:

    STATUS_SUCCESS if the reset request was successfully queued. The process
    should expect to receive a termination signal shortly, followed by a
    kill signal shortly after that.

    STATUS_INVALID_PARAMETER if the given reset type is not valid.

    STATUS_INSUFFICIENT_RESOURCES if an allocation failure in the kernel
    prevented queuing of the reset system work item.

--*/

{

    return OsSystemCall(SystemCallResetSystem, (PVOID)ResetType);
}

OS_API
PVOID
OsSetProgramBreak (
    PVOID NewBreak
    )

/*++

Routine Description:

    This routine gets or sets the application program break for the process.

Arguments:

    NewBreak - Supplies an optional pointer to the new break to set. If this
        is less than the original break, then no change is made. Set to NULL
        to simply get the current program break.

Return Value:

    Returns the current program break, which is either the new value set or
    the previous value.

--*/

{

    SYSTEM_CALL_SET_BREAK Request;

    Request.Break = NewBreak;
    OsSystemCall(SystemCallSetBreak, &Request);
    return Request.Break;
}

OS_API
KSTATUS
OsMemoryMap (
    HANDLE Handle,
    IO_OFFSET Offset,
    UINTN Size,
    ULONG Flags,
    PVOID *Address
    )

/*++

Routine Description:

    This routine maps the specified object starting at the given offset for the
    requested size, in bytes. A suggested address can optionally be supplied.

Arguments:

    Handle - Supplies a pointer to an opened I/O handle.

    Offset - Supplies the offset into the I/O object where the mapping should
        begin.

    Size - Supplies the size of the mapping region, in bytes.

    Flags - Supplies a bitfield of flags. See SYS_MAP_FLAG_* for definitions.

    Address - Supplies a pointer that receives the address of the mapped
        region. The caller can optionally specify a suggested address using
        this paramter.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_MAP_UNMAP_MEMORY Parameters;
    KSTATUS Status;

    Parameters.Map = TRUE;
    Parameters.Flags = Flags;
    Parameters.Handle = Handle;
    Parameters.Address = *Address;
    Parameters.Offset = Offset;
    Parameters.Size = Size;
    Status = OsSystemCall(SystemCallMapOrUnmapMemory, &Parameters);
    if (KSUCCESS(Status)) {
        *Address = Parameters.Address;
    }

    return Status;
}

OS_API
KSTATUS
OsMemoryUnmap (
    PVOID Address,
    UINTN Size
    )

/*++

Routine Description:

    This routine unmaps the specified region from the current process' address
    space.

Arguments:

    Address - Supplies the starting address of the memory region to unmap.

    Size - Supplies the size of the region to unmap, in bytes.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_MAP_UNMAP_MEMORY Parameters;

    Parameters.Map = FALSE;
    Parameters.Address = Address;
    Parameters.Size = Size;
    return OsSystemCall(SystemCallMapOrUnmapMemory, &Parameters);
}

OS_API
KSTATUS
OsSetMemoryProtection (
    PVOID Address,
    UINTN Size,
    ULONG NewAttributes
    )

/*++

Routine Description:

    This routine set the memory protection attributes for the given region.

Arguments:

    Address - Supplies the starting address (inclusive) to change the memory
        protection for. This must be aligned to a page boundary.

    Size - Supplies the length, in bytes, of the region to change attributes
        for.

    NewAttributes - Supplies the new attributes to set. See SYS_MAP_FLAG_*
        definitions.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_SET_MEMORY_PROTECTION Parameters;

    Parameters.Address = Address;
    Parameters.Size = Size;
    Parameters.NewAttributes = NewAttributes;
    return OsSystemCall(SystemCallSetMemoryProtection, &Parameters);
}

OS_API
KSTATUS
OsMemoryFlush (
    PVOID Address,
    ULONGLONG Size,
    ULONG Flags
    )

/*++

Routine Description:

    This routine flushes a region of the current process' mapped memory to
    permament storage, if the region has a backing image.

Arguments:

    Address - Supplies the starting address of the memory region to synchronize.

    Size - Supplies the size of the region to synchronize, in bytes.

    Flags - Supplies a bitfield of flags. See SYS_MAP_SYNC_FLAG_* for
        definitions.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_FLUSH_MEMORY Parameters;

    Parameters.Address = Address;
    Parameters.Size = Size;
    Parameters.Flags = Flags;
    return OsSystemCall(SystemCallFlushMemory, &Parameters);
}

OS_API
KSTATUS
OsSetThreadIdentity (
    ULONG FieldsToSet,
    PTHREAD_IDENTITY Identity
    )

/*++

Routine Description:

    This routine gets or sets a thread's identity.

Arguments:

    FieldsToSet - Supplies a bitfield indicating which identity fields to set.
        Supply zero to simply get the current thread identity. See
        THREAD_IDENTITY_FIELD_* definitions.

    Identity - Supplies a pointer that on input contains the thread identity
        fields to set. On successful output, will contain the complete new
        thread identity.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_SET_THREAD_IDENTITY Parameters;
    KSTATUS Status;

    Parameters.Request.FieldsToSet = FieldsToSet;
    if (FieldsToSet != 0) {
        RtlCopyMemory(&(Parameters.Request.Identity),
                      Identity,
                      sizeof(THREAD_IDENTITY));
    }

    Status = OsSystemCall(SystemCallSetThreadIdentity, &Parameters);
    if (KSUCCESS(Status)) {
        RtlCopyMemory(Identity,
                      &(Parameters.Request.Identity),
                      sizeof(THREAD_IDENTITY));
    }

    return Status;
}

OS_API
KSTATUS
OsSetThreadPermissions (
    ULONG FieldsToSet,
    PTHREAD_PERMISSIONS Permissions
    )

/*++

Routine Description:

    This routine gets or sets a thread's permission masks.

Arguments:

    FieldsToSet - Supplies a bitfield indicating which permission sets or
        fields to modify. Supply zero to simply get the current thread
        permission sets. See THREAD_PERMISSION_FIELD_* definitions.

    Permissions - Supplies a pointer that on input contains the thread
        permission masks to set. On successful output, will contain the
        complete new set of permission masks.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_SET_THREAD_PERMISSIONS Parameters;
    KSTATUS Status;

    Parameters.Request.FieldsToSet = FieldsToSet;
    if (FieldsToSet != 0) {
        RtlCopyMemory(&(Parameters.Request.Permissions),
                      Permissions,
                      sizeof(THREAD_PERMISSIONS));
    }

    Status = OsSystemCall(SystemCallSetThreadPermissions, &Parameters);
    if (KSUCCESS(Status)) {
        RtlCopyMemory(Permissions,
                      &(Parameters.Request.Permissions),
                      sizeof(THREAD_PERMISSIONS));
    }

    return Status;
}

OS_API
KSTATUS
OsSetSupplementaryGroups (
    BOOL Set,
    PGROUP_ID Groups,
    PUINTN Count
    )

/*++

Routine Description:

    This routine gets or sets a thread's set of supplementary groups. To set
    the supplementary groups, the thread must have the set group ID permission.

Arguments:

    Set - Supplies a boolean indicating whether to set the new groups (TRUE) or
        just get the current list of supplementary groups.

    Groups - Supplies a pointer that receives the supplementary groups for a
        get operation or contains the new group IDs to set for a set operation.

    Count - Supplies a pointer that on input contains the number of elements
        in the given buffer. On output, contains the number of valid elements.
        If STATUS_BUFFER_TOO_SMALL is returned, the number of elements needed
        will be retunred.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_ACCESS_VIOLATION if the given groups buffer is invalid.

    STATUS_PERMISSION_DENIED if the caller does not have the set group ID
    permission.

    STATUS_INSUFFICIENT_RESOURCES if an internal kernel allocation failed.

    STATUS_INVALID_PARAMETER if the count was too big.

    STATUS_BUFFER_TOO_SMALL if the given buffer was not big enough to contain
    all the current supplementary groups. In this case, count contains the
    number of elements needed.

--*/

{

    SYSTEM_CALL_SET_SUPPLEMENTARY_GROUPS Parameters;
    KSTATUS Status;

    Parameters.Set = Set;
    Parameters.Groups = Groups;
    Parameters.Count = *Count;
    Status = OsSystemCall(SystemCallSetSupplementaryGroups, &Parameters);
    *Count = Parameters.Count;
    return Status;
}

OS_API
KSTATUS
OsSetResourceLimit (
    RESOURCE_LIMIT_TYPE Type,
    PRESOURCE_LIMIT NewValue,
    PRESOURCE_LIMIT OldValue
    )

/*++

Routine Description:

    This routine gets or sets the current resource limit value for a given type.

Arguments:

    Type - Supplies the resource limit type to get the limit for.

    NewValue - Supplies an optional pointer to the new limit to set. If this is
        NULL, then a new value is not set.

    OldValue - Supplies an optional pointer where the previous limit will be
        returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if the resource type is not valid or the current
    value was greater than the max.

    STATUS_PERMISSION_DENIED if the caller is trying to raise the max/hard
    limit and does not have the resources permission.

--*/

{

    SYSTEM_CALL_SET_RESOURCE_LIMIT Parameters;
    KSTATUS Status;

    Parameters.Type = Type;
    if (NewValue != NULL) {
        Parameters.Set = TRUE;
        Parameters.Value.Current = NewValue->Current;
        Parameters.Value.Max = NewValue->Max;

    } else {
        Parameters.Set = FALSE;
    }

    Status = OsSystemCall(SystemCallSetResourceLimit, &Parameters);
    if (OldValue != NULL) {
        OldValue->Current = Parameters.Value.Current;
        OldValue->Max = Parameters.Value.Max;
    }

    return Status;
}

OS_API
KSTATUS
OsCreateTerminal (
    HANDLE MasterDirectory,
    HANDLE SlaveDirectory,
    PSTR MasterPath,
    UINTN MasterPathLength,
    PSTR SlavePath,
    UINTN SlavePathLength,
    ULONG MasterOpenFlags,
    FILE_PERMISSIONS MasterCreatePermissions,
    FILE_PERMISSIONS SlaveCreatePermissions,
    PHANDLE MasterHandle
    )

/*++

Routine Description:

    This routine creates a new pseudo-terminal master and slave at the given
    paths.

Arguments:

    MasterDirectory - Supplies an optional handle to a directory for relative
        paths when creating the master. Supply INVALID_HANDLE to use the
        current working directory.

    SlaveDirectory - Supplies an optional handle to a directory for relative
        paths when creating the slave. Supply INVALID_HANDLE to use the
        current working directory.

    MasterPath - Supplies an optional pointer to the path to create for the
        master.

    MasterPathLength - Supplies the length of the master side path buffer in
        bytes, including the null terminator.

    SlavePath - Supplies an optional pointer to the path to create for the
        master.

    SlavePathLength - Supplies the length of the slave side path buffer in
        bytes, including the null terminator.

    MasterOpenFlags - Supplies the open flags to use when opening the master.
        Only read, write, and "no controlling terminal" flags are honored.

    MasterCreatePermissions - Supplies the permissions to apply to the created
        master side.

    SlaveCreatePermissions - Supplies the permission to apply to the created
        slave side.

    MasterHandle - Supplies a pointer where the opened handle to the master
        will be returned on success.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_CREATE_TERMINAL Parameters;
    KSTATUS Status;

    Parameters.MasterDirectory = MasterDirectory;
    Parameters.SlaveDirectory = SlaveDirectory;
    Parameters.MasterPath = MasterPath;
    Parameters.MasterPathLength = MasterPathLength;
    Parameters.SlavePath = SlavePath;
    Parameters.SlavePathLength = SlavePathLength;
    Parameters.MasterOpenFlags = MasterOpenFlags;
    Parameters.MasterCreatePermissions = MasterCreatePermissions;
    Parameters.SlaveCreatePermissions = SlaveCreatePermissions;
    Status = OsSystemCall(SystemCallCreateTerminal, &Parameters);
    *MasterHandle = INVALID_HANDLE;
    if (KSUCCESS(Status)) {
        *MasterHandle = Parameters.MasterHandle;
    }

    return Status;
}

OS_API
KSTATUS
OsGetFilePath (
    HANDLE Handle,
    PSTR Path,
    PUINTN PathSize
    )

/*++

Routine Description:

    This routine returns the file path for the given handle, if possible.

Arguments:

    Handle - Supplies the open handle to get the file path of.

    Path - Supplies a pointer where the path will be returned on success.

    PathSize - Supplies a pointer that on input contains the size of the path
        buffer. On output, returns the needed size of the path buffer, even
        if the supplied buffer is too small.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_HANDLE if the given handle is not valid.

    STATUS_PATH_NOT_FOUND if no path exists for the given handle.

    STATUS_BUFFER_TOO_SMALL if the supplied path buffer was not large enough to
    contain the complete path. In this case the path size returned is the size
    needed.

    STATUS_ACCESS_VIOLATION if the path buffer was invalid.

--*/

{

    FILE_CONTROL_PARAMETERS_UNION Parameters;
    KSTATUS Status;

    Parameters.FilePath.Path = Path;
    Parameters.FilePath.PathSize = *PathSize;
    Status = OsFileControl(Handle, FileControlCommandGetPath, &Parameters);
    *PathSize = Parameters.FilePath.PathSize;
    return Status;
}

OS_API
VOID
OsSetThreadIdPointer (
    PTHREAD_ID Pointer
    )

/*++

Routine Description:

    This routine sets the thread ID pointer in the kernel. If this value is
    non-null when the thread exits, then zero will be written to this address,
    and a UserLockWake operation will be performed to wake up one thread.

Arguments:

    Pointer - Supplies the new user mode pointer to the thread ID that will be
        cleared when the thread exits. Supply NULL to clear the thread ID
        pointer in the kernel. If this is non-null, the kernel will write the
        thread ID into this region.

Return Value:

    None.

--*/

{

    OsSystemCall(SystemCallSetThreadIdPointer, Pointer);
    return;
}

OS_API
FILE_PERMISSIONS
OsSetUmask (
    FILE_PERMISSIONS NewMask
    )

/*++

Routine Description:

    This routine sets file permission mask for the current process. Bits set
    in this mask will be automatically cleared out of the permissions of any
    file or directory created.

Arguments:

    NewMask - Supplies the new mask to set.

Return Value:

    Returns the previously set mask.

--*/

{

    SYSTEM_CALL_SET_UMASK Parameters;

    Parameters.Mask = NewMask;
    OsSystemCall(SystemCallSetUmask, &Parameters);
    return Parameters.Mask;
}

OS_API
KSTATUS
OsDuplicateHandle (
    HANDLE ExistingHandle,
    PHANDLE NewHandle,
    ULONG Flags
    )

/*++

Routine Description:

    This routine duplicates a given handle at a new handle.

Arguments:

    ExistingHandle - Supplies the handle to duplicate.

    NewHandle - Supplies a pointer that contains the destination handle value
        for the new handle. If this is INVALID_HANDLE, then the duplicated
        handle will be the lowest available handle value, and will be returned
        here. If this is not INVALID_HANDLE, then the previous handle at that
        location will be closed. If the new handle equals the existing handle,
        failure is returned.

    Flags - Supplies open flags to be set on the new handle. Only
        SYS_OPEN_FLAG_CLOSE_ON_EXECUTE is permitted. If not set, the new handle
        will have the close on execute flag cleared.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_DUPLICATE_HANDLE Parameters;
    KSTATUS Status;

    Parameters.OldHandle = ExistingHandle;
    Parameters.NewHandle = *NewHandle;
    Parameters.OpenFlags = Flags;
    Status = OsSystemCall(SystemCallDuplicateHandle, &Parameters);
    *NewHandle = Parameters.NewHandle;
    return Status;
}

VOID
OspProcessSignal (
    PSIGNAL_PARAMETERS Parameters,
    PSIGNAL_CONTEXT Context
    )

/*++

Routine Description:

    This routine processes a signal sent via the kernel.

Arguments:

    Parameters - Supplies a pointer to the signal parameters from the kernel.

    Context - Supplies a pointer to the signal context from the kernel.

Return Value:

    None.

--*/

{

    BOOL RestartAllowed;
    PSIGNAL_HANDLER_ROUTINE SignalHandler;

    RestartAllowed = FALSE;
    SignalHandler = OsSignalHandler;
    if (SignalHandler != NULL) {
        RestartAllowed = SignalHandler(Parameters, Context);
    }

    //
    // Clear the restart flag if it's set but the handler does not allow
    // restarts.
    //

    if (((Context->Flags & SIGNAL_CONTEXT_FLAG_RESTART) != 0) &&
        (RestartAllowed == FALSE)) {

        Context->Flags &= ~SIGNAL_CONTEXT_FLAG_RESTART;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

PVOID
OspSetSignalHandler (
    PVOID SignalHandlerRoutine
    )

/*++

Routine Description:

    This routine sets the signal handler routine for the given thread.

Arguments:

    SignalHandlerRoutine - Supplies a pointer to the signal handler routine to
        invoke when signals occur. The two parameters to the routine will be
        the signal number and the optional parameter, both of which will be
        passed via registers.

Return Value:

    Returns a pointer to the original signal handler registered, or NULL if no
    signal handler was previously registered.

--*/

{

    SYSTEM_CALL_SET_SIGNAL_HANDLER Parameters;

    Parameters.SignalHandler = SignalHandlerRoutine;
    OsSystemCall(SystemCallSetSignalHandler, &Parameters);
    return Parameters.SignalHandler;
}

KSTATUS
OspGetSetFileInformation (
    HANDLE Directory,
    PSTR Path,
    ULONG PathSize,
    BOOL FollowLink,
    PSET_FILE_INFORMATION Request
    )

/*++

Routine Description:

    This routine gets or sets the file properties for a given file.

Arguments:

    Directory - Supplies an optional handle to a directory to start the
        search from if the supplied path is relative. Supply INVALID_HANDLE
        here to to use the current directory for relative paths (usually the
        expected default).

    Path - Supplies a pointer to the string containing the file path to get or
        set properties for.

    PathSize - Supplies the size of the path string in bytes including the
        null terminator.

    FollowLink - Supplies a boolean indicating what to do if the file path
        points to a symbolic link. If set to TRUE, the file information set or
        returned will be for the file the link points to. If FALSE, the call
        will set or get information for the link itself.

    Request - Supplies a pointer to the get or set file properties request.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_GET_SET_FILE_INFORMATION Parameters;
    KSTATUS Status;

    Parameters.Request.FieldsToSet = Request->FieldsToSet;
    Parameters.Request.FileProperties = Request->FileProperties;
    Parameters.Directory = Directory;
    Parameters.FilePath = Path;
    Parameters.FilePathSize = PathSize;
    Parameters.FollowLink = FollowLink;
    Status = OsSystemCall(SystemCallGetSetFileInformation, &Parameters);
    if (Request->FieldsToSet == 0) {
        RtlCopyMemory(Request,
                      &(Parameters.Request),
                      sizeof(SET_FILE_INFORMATION));
    }

    return Status;
}

KSTATUS
OspMountOrUnmount (
    PSTR MountPointPath,
    ULONG MountPointPathSize,
    PSTR TargetPath,
    ULONG TargetPathSize,
    ULONG Flags
    )

/*++

Routine Description:

    This routine mounts the target at the given mount point or unmounts the
    mount point.

Arguments:

    MountPointPath - Supplies a pointer to a string containing the path to the
        mount point where the target is to be mounted.

    MountPointPathSize - Supplies the size of the mount point path string in
        bytes, including the null terminator.

    TargetPath - Supplies a pointer to a string containing the path to the
        target file, directory, volume, or device that is to be mounted.

    TargetPathSize - Supplies the size of the target path string in bytes,
        including the null terminator.

    Flags - Supplies the flags associated with the mount operation. See
        SYS_MOUNT_FLAG_* for definitions.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_MOUNT_UNMOUNT Parameters;

    //
    // Make the system call with the mount/unmount information.
    //

    Parameters.MountPointPath = MountPointPath;
    Parameters.MountPointPathSize = MountPointPathSize;
    Parameters.TargetPath = TargetPath;
    Parameters.TargetPathSize = TargetPathSize;
    Parameters.Flags = Flags;
    return OsSystemCall(SystemCallMountOrUnmount, &Parameters);
}

NO_RETURN
VOID
OspExitThread (
    PVOID UnmapAddress,
    UINTN UnmapSize
    )

/*++

Routine Description:

    This routine terminates the current thread, and optionally attempts to
    unmap a region of memory on its way out. Usually this is the stack of the
    thread that is exiting.

Arguments:

    UnmapAddress - Supplies an optional pointer to a region of memory to unmap
        as the thread exits. Supply NULL to skip unmapping.

    UnmapSize - Supplies the size of the region to unmap in bytes. This must be
        aligned to the page size. If it is not, the unmap simply won't happen.
        Supply 0 to skip the unmap and just exit the thread.

Return Value:

    This routine does not return.

--*/

{

    SYSTEM_CALL_EXIT_THREAD Parameters;

    Parameters.UnmapAddress = UnmapAddress;
    Parameters.UnmapSize = UnmapSize;
    OsSystemCall(SystemCallExitThread, &Parameters);
    while (TRUE) {

        ASSERT(FALSE);

    }
}

